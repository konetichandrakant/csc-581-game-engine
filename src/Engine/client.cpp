#include "Engine/client.h"

#include <zmq.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <iostream>
#include <cctype>
#include <arpa/inet.h> // ntohs

namespace {


#pragma pack(push,1)


enum class MsgKind : uint8_t { Hello=1, HelloAck=2, UpdatePos=3, Snapshot=4 };

struct HelloMsg {
    MsgKind  kind{MsgKind::Hello};
    uint32_t name_len{0};
};

struct HelloAck {
    MsgKind kind{MsgKind::HelloAck};
    int32_t assigned_id{0};
    int32_t cmd_port{0};
    int32_t pub_port{0};
};

struct UpdatePosMsg {
    MsgKind kind{MsgKind::UpdatePos};
    int32_t id{0};
    float   x{0}, y{0};
};

struct XYRaw { float x; float y; };

struct SnapshotMsgHeader {
    MsgKind  kind{MsgKind::Snapshot};
    uint64_t world_tick{0};
    uint32_t player_count{0};
    uint32_t platform_count{0};
};

// P2P directory + player state + authority world
enum class P2PKind : uint8_t { World=1, Player=2, DirRegister=3, DirReply=4 };

struct P2PHeader { P2PKind kind; uint64_t tick; };

struct P2PPlayer {
    P2PHeader h{P2PKind::Player,0};
    int32_t   player_id{0};
    float     x{0}, y{0}, vx{0}, vy{0};
    uint8_t   facing{0}, anim{0};
};

struct P2DRegister {
    P2PHeader h{P2PKind::DirRegister,0};
    int32_t   want_list{1};
    int32_t   player_id{0};
    uint16_t  pub_port{0};
};

struct P2DPeerEndpoint {
    int32_t  player_id{0};
    uint32_t ipv4_be{0};
    uint16_t port_be{0};
};

struct P2DReply {
    P2PHeader h{P2PKind::DirReply,0};
    int32_t   my_player_id{0};
    uint32_t  peer_count{0};

};


struct P2PWorld {
    P2PHeader h{P2PKind::World,0};
    uint32_t  platform_count{0};

};

#pragma pack(pop)

// ===== Small ZMQ helpers =====
static void* mk_ctx() { return zmq_ctx_new(); }
static void  free_ctx(void* c){ if (c) zmq_ctx_term(c); }

static void* mk_sock(void* ctx, int type){ return zmq_socket(ctx, type); }
static void  close_sock(void* s){ if (s) zmq_close(s); }

static bool  connect_tcp(void* s, const std::string& host, int port){
    char ep[128]; std::snprintf(ep, sizeof(ep), "tcp://%s:%d", host.c_str(), port);
    return zmq_connect(s, ep) == 0;
}

static bool  subscribe_all(void* s){ return zmq_setsockopt(s, ZMQ_SUBSCRIBE, "", 0) == 0; }

static void  set_rcvtimeo(void* s, int ms){ zmq_setsockopt(s, ZMQ_RCVTIMEO, &ms, sizeof(ms)); }
static void  set_sndtimeo(void* s, int ms){ zmq_setsockopt(s, ZMQ_SNDTIMEO, &ms, sizeof(ms)); }
static void  set_linger(void* s, int ms){ zmq_setsockopt(s, ZMQ_LINGER,  &ms, sizeof(ms)); }
static void  set_conflate(void* s, int on){ zmq_setsockopt(s, ZMQ_CONFLATE,&on, sizeof(on)); }
static void  set_rcvhwm(void* s, int hwm){ zmq_setsockopt(s, ZMQ_RCVHWM,  &hwm, sizeof(hwm)); }
static void  set_sndhwm(void* s, int hwm){ zmq_setsockopt(s, ZMQ_SNDHWM,  &hwm, sizeof(hwm)); }

static uint16_t parse_port_from_endpoint(const char* ep) {

    int len = (int)std::strlen(ep);
    int i = len - 1;
    while (i >= 0 && std::isdigit((unsigned char)ep[i])) --i;
    ++i; // first digit
    if (i < 0 || i >= len) return 0;
    int port = std::atoi(ep + i);
    if (port <= 0 || port > 65535) return 0;
    return static_cast<uint16_t>(port);
}

static uint16_t bind_ephemeral(void* sock) {
    if (zmq_bind(sock, "tcp://*:0") != 0) return 0;
    char ep[256]; size_t elen = sizeof(ep);
    if (zmq_getsockopt(sock, ZMQ_LAST_ENDPOINT, ep, &elen) != 0) return 0; // e.g. "tcp://[::]:53217"
    uint16_t port = parse_port_from_endpoint(ep);
    std::cout << "[P2P] bound PUB at " << ep << " (port=" << port << ")\n";
    return port;
}

inline int64_t nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

}

namespace Engine {



Client::Client() {}

Client::~Client() {
    shutdown();
}

void Client::shutdown() {

    p2pRunning_.store(false, std::memory_order_relaxed);
    if (p2pRxThread_.joinable()) p2pRxThread_.join();


    running_.store(false, std::memory_order_relaxed);
    if (recv_thread_.joinable()) recv_thread_.join();


    close_sock(pubMine_);  pubMine_  = nullptr;
    close_sock(subPeers_); subPeers_ = nullptr;
    close_sock(req_);      req_      = nullptr;
    close_sock(sub_);      sub_      = nullptr;


    free_ctx(ctx_);        ctx_      = nullptr;


    {
        std::scoped_lock lk1(snap_mx_, plat_mx_, peersMtx_);
        snap_.clear();
        platforms_.clear();
        peers_.clear();
        connectedPeerIds_.clear();
    }
    isAuthority_.store(false);
}



bool Client::start(const std::string& host, const std::string& displayName) {
    constexpr int kCmdPort   = 5555; // Hello/IDs on directory/server
    constexpr int kWorldPort = 5556; // external world server (platforms-only)
    return hello(host, displayName, kCmdPort, kWorldPort);
}

bool Client::hello(const std::string& host, const std::string& displayName,
                   int cmdPort, int worldPubPort)
{
    if (recv_thread_.joinable()) { running_.store(false); recv_thread_.join(); }
    close_sock(req_);  req_ = nullptr;
    close_sock(sub_);  sub_ = nullptr;

    if (!ctx_) ctx_ = mk_ctx();
    if (!ctx_) return false;

    req_ = mk_sock(ctx_, ZMQ_REQ);
    if (!req_) return false;
    set_linger(req_, 0);
    set_sndhwm(req_, 1);
    set_rcvhwm(req_, 1);
    set_rcvtimeo(req_, 500);
    set_sndtimeo(req_, 500);
    if (!connect_tcp(req_, host, cmdPort)) { close_sock(req_); req_ = nullptr; return false; }


    bool ok = false;
    for (int attempt = 0; attempt < 40 && !ok; ++attempt) {
        HelloMsg hello{};
        hello.name_len = static_cast<uint32_t>(displayName.size());
        zmq_msg_t msg; zmq_msg_init_size(&msg, sizeof(HelloMsg) + hello.name_len);
        std::memcpy(zmq_msg_data(&msg), &hello, sizeof(HelloMsg));
        std::memcpy(static_cast<char*>(zmq_msg_data(&msg)) + sizeof(HelloMsg),
                    displayName.data(), hello.name_len);

        if (zmq_msg_send(&msg, req_, 0) < 0) { zmq_msg_close(&msg); std::this_thread::sleep_for(std::chrono::milliseconds(250)); continue; }
        zmq_msg_close(&msg);

        HelloAck ack{};
        int n = zmq_recv(req_, &ack, sizeof(ack), 0);
        if (n == static_cast<int>(sizeof(ack)) && ack.kind == MsgKind::HelloAck && ack.assigned_id > 0) {
            my_id_.store(ack.assigned_id);
            ok = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    if (!ok) { close_sock(req_); req_ = nullptr; std::cerr << "[Client] Hello failed.\n"; return false; }

    sub_ = mk_sock(ctx_, ZMQ_SUB);
    if (sub_) {
        set_linger(sub_, 0);
        set_conflate(sub_, 1);
        set_rcvhwm(sub_, 1);
        subscribe_all(sub_);
        connect_tcp(sub_, host, worldPubPort);
    }

    running_.store(true);
    recv_thread_ = std::thread(&Client::recvLoop, this);

    std::cout << "[Client] Hello OK. id=" << my_id_.load()
              << " (SUB world@" << host << ":" << worldPubPort << ")\n";
    return true;
}

void Client::sendPos(float x, float y) {
    if (!req_ || my_id_.load() <= 0) return;
    UpdatePosMsg up{};
    up.id = my_id_.load();
    up.x = x; up.y = y;
    zmq_send(req_, &up, sizeof(up), 0); // server will ack (compat only)
    char ack[8]; set_rcvtimeo(req_, 50); zmq_recv(req_, &ack, sizeof(ack), 0);
}

std::unordered_map<int, XY> Client::snapshot() const {
    std::scoped_lock lk(snap_mx_);
    return snap_;
}

std::vector<XY> Client::platforms() const {
    std::scoped_lock lk(plat_mx_);
    return platforms_;
}

void Client::recvLoop() {
    if (!sub_) return;
    set_rcvtimeo(sub_, 250);

    while (running_.load()) {
        zmq_msg_t msg; zmq_msg_init(&msg);
        int rc = zmq_msg_recv(&msg, sub_, 0);
        if (rc < 0) { zmq_msg_close(&msg); std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }

        const uint8_t* data = static_cast<const uint8_t*>(zmq_msg_data(&msg));
        size_t len = static_cast<size_t>(rc);

        if (len >= sizeof(SnapshotMsgHeader)) {
            const auto* h = reinterpret_cast<const SnapshotMsgHeader*>(data);
            if (h->kind == MsgKind::Snapshot) {
                size_t off = sizeof(SnapshotMsgHeader);

                std::unordered_map<int, XY> tmpPlayers;
                tmpPlayers.reserve(h->player_count);
                for (uint32_t i=0; i<h->player_count; ++i) {
                    if (off + sizeof(int32_t) + sizeof(XYRaw) > len) break;
                    int32_t id = *reinterpret_cast<const int32_t*>(data + off);
                    off += sizeof(int32_t);
                    const XYRaw* p = reinterpret_cast<const XYRaw*>(data + off);
                    off += sizeof(XYRaw);
                    tmpPlayers[id] = XY{p->x, p->y};
                }


                std::vector<XY> tmpPlatforms;
                tmpPlatforms.reserve(h->platform_count);
                for (uint32_t i=0; i<h->platform_count; ++i) {
                    if (off + sizeof(XYRaw) > len) break;
                    const XYRaw* p = reinterpret_cast<const XYRaw*>(data + off);
                    off += sizeof(XYRaw);
                    tmpPlatforms.push_back(XY{p->x, p->y});
                }

                { std::scoped_lock l1(snap_mx_); snap_.swap(tmpPlayers); }
                { std::scoped_lock l2(plat_mx_); platforms_.swap(tmpPlatforms); }

                lastWorldRecvNs_.store(nowNs(), std::memory_order_relaxed);
                hadServer_.store(true, std::memory_order_relaxed);
            }
        }
        zmq_msg_close(&msg);
    }
}


bool Client::startP2P(const std::string& dirHost, int /*worldPubPortUnused*/, int dirPort) {
    dirHost_  = dirHost;
    dirPort_  = dirPort;
    connectedPeerIds_.clear();

    if (!ctx_) ctx_ = mk_ctx();
    if (!ctx_) { std::cerr << "[P2P] no ctx\n"; return false; }

    subPeers_ = mk_sock(ctx_, ZMQ_SUB);
    if (!subPeers_) { std::cerr << "[P2P] SUB alloc failed\n"; return false; }
    set_linger(subPeers_, 0);
    set_rcvhwm(subPeers_, 1);
    set_conflate(subPeers_, 1);
    subscribe_all(subPeers_);
    pubMine_ = mk_sock(ctx_, ZMQ_PUB);
    if (!pubMine_) { std::cerr << "[P2P] PUB alloc failed\n"; return false; }
    set_linger(pubMine_, 0);
    set_sndhwm(pubMine_, 1000);
    myPubPort_ = bind_ephemeral(pubMine_);
    if (!myPubPort_) { std::cerr << "[P2P] bind ephemeral failed (LAST_ENDPOINT parse?)\n"; close_sock(pubMine_); pubMine_=nullptr; return false; }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));


    if (!p2pQueryDirectoryAndConnect_()) {
        std::cerr << "[P2P] initial directory query FAILED (host=" << dirHost_ << " port=" << dirPort_ << ")\n";

    }


    nextAuthSim_ = std::chrono::steady_clock::now();
    nextAuthPub_ = std::chrono::steady_clock::now();

    p2pRunning_.store(true);
    p2pRxThread_ = std::thread(&Client::p2pRxLoop_, this);

    // Trigger an early refresh after startup
    nextDirRefresh_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);

    std::cout << "[P2P] startP2P OK. myId=" << my_id_.load()
              << " pubPort=" << myPubPort_ << "\n";
    return true;
}

void Client::stopP2P() {
    p2pRunning_.store(false);
    if (p2pRxThread_.joinable()) p2pRxThread_.join();
    close_sock(pubMine_);  pubMine_ = nullptr;
    close_sock(subPeers_); subPeers_ = nullptr;
}

bool Client::p2pQueryDirectoryAndConnect_() {
    void* req = mk_sock(ctx_, ZMQ_REQ);
    if (!req) return false;
    set_linger(req, 0);
    set_rcvtimeo(req, 200);
    set_sndtimeo(req, 200);

    std::string url = "tcp://" + dirHost_ + ":" + std::to_string(dirPort_);
    if (zmq_connect(req, url.c_str()) != 0) { close_sock(req); return false; }

    P2DRegister reg{};
    reg.player_id = my_id_.load();
    reg.pub_port  = static_cast<uint16_t>(myPubPort_);
    if (zmq_send(req, &reg, sizeof(reg), 0) != static_cast<int>(sizeof(reg))) { close_sock(req); return false; }

    uint8_t buf[4096];
    int n = zmq_recv(req, buf, sizeof(buf), 0);
    close_sock(req);
    if (n < static_cast<int>(sizeof(P2DReply))) {

        return false;
    }

    const auto* r = reinterpret_cast<const P2DReply*>(buf);
    my_id_.store(r->my_player_id);
    std::cout << "[P2P] dir reply: myId=" << my_id_.load() << " peers=" << r->peer_count << "\n";

    const auto* list = reinterpret_cast<const P2DPeerEndpoint*>(buf + sizeof(P2DReply));
    size_t newConnects = 0;
    for (uint32_t i=0; i<r->peer_count; ++i) {
        const int peerId = list[i].player_id;
        if (peerId == my_id_.load()) continue;
        if (connectedPeerIds_.count(peerId)) continue;

        int port = ntohs(list[i].port_be);
        if (port <= 0) continue;

        std::string ep = "tcp://" + dirHost_ + ":" + std::to_string(port);
        if (zmq_connect(subPeers_, ep.c_str()) == 0) {
            connectedPeerIds_.insert(peerId);
            ++newConnects;
            std::cout << "[P2P] connect to peerId=" << peerId
                      << " at " << ep << "\n";
        } else {
            std::cerr << "[P2P] connect FAILED to " << ep << "\n";
        }
    }

    if (newConnects == 0) std::cout << "[P2P] no new peers to connect\n";
    return true;
}

void Client::p2pPublishPlayer(uint64_t tick,
                              float x,float y,float vx,float vy,
                              uint8_t facing,uint8_t anim) {
    if (!pubMine_) return;
    P2PPlayer ps{};
    ps.h.tick   = tick;
    ps.player_id= my_id_.load();
    ps.x=x; ps.y=y; ps.vx=vx; ps.vy=vy; ps.facing=facing; ps.anim=anim;
    zmq_send(pubMine_, &ps, sizeof(ps), ZMQ_DONTWAIT);
}

void Client::configureAuthorityLayout(int winW, int winH) {
    winW_ = winW; winH_ = winH;
}

void Client::becomeAuthority_() {
    if (isAuthority_.load()) return;

    const float minX = 120.0f;
    const float maxX = (float)winW_ - 320.0f;
    authPlats_.clear();
    authPlats_.push_back(AuthPlat{ 200.0f,            (float)winH_ - 320.0f, minX, maxX, +220.0f, 0.0f });
    authPlats_.push_back(AuthPlat{ (float)winW_-420.0f,(float)winH_ - 520.0f, minX, maxX, -260.0f, 0.0f });

    isAuthority_.store(true);
    nextAuthSim_ = std::chrono::steady_clock::now();
    nextAuthPub_ = std::chrono::steady_clock::now();
    std::cout << "[P2P] >>> I am AUTHORITY (id=" << my_id_.load() << ")\n";
}

void Client::resignAuthority_() {
    if (!isAuthority_.load()) return;
    isAuthority_.store(false);
    std::cout << "[P2P] <<< resign AUTHORITY (id=" << my_id_.load() << ")\n";
}

void Client::authorityMaybeStepAndBroadcast_() {
    if (!isAuthority_.load() || !pubMine_) return;

    using clock = std::chrono::steady_clock;
    auto now = clock::now();


    const double simHz = 120.0;
    const double pubHz = 60.0;
    auto dtSim = std::chrono::duration<double>(1.0 / simHz);
    auto dtPub = std::chrono::duration<double>(1.0 / pubHz);


    while (now >= nextAuthSim_) {
        double step = dtSim.count();
        for (auto& p : authPlats_) {
            p.x += p.vx * (float)step;
            if (p.x < p.minX) { p.x = p.minX; p.vx =  std::abs(p.vx); }
            if (p.x > p.maxX) { p.x = p.maxX; p.vx = -std::abs(p.vx); }
        }
        nextAuthSim_ += std::chrono::duration_cast<clock::duration>(dtSim);
    }

    {
        std::vector<XY> local;
        local.reserve(authPlats_.size());
        for (const auto& p : authPlats_) local.push_back(XY{p.x, p.y});
        std::scoped_lock l2(plat_mx_);
        platforms_.swap(local);
    }
    lastP2PWorldRecvNs_.store(nowNs(), std::memory_order_relaxed); // mark as fresh

    if (now >= nextAuthPub_) {
        const uint32_t N = (uint32_t)authPlats_.size();
        const size_t total = sizeof(P2PWorld) + N * sizeof(XYRaw);
        std::vector<uint8_t> out(total);
        auto* w = reinterpret_cast<P2PWorld*>(out.data());
        w->h.kind = P2PKind::World;
        w->h.tick = (uint64_t)nowNs();
        w->platform_count = N;
        size_t off = sizeof(P2PWorld);
        for (const auto& p : authPlats_) {
            XYRaw xy{ p.x, p.y };
            std::memcpy(out.data()+off, &xy, sizeof(xy)); off += sizeof(xy);
        }
        zmq_send(pubMine_, out.data(), (int)out.size(), ZMQ_DONTWAIT);
        nextAuthPub_ += std::chrono::duration_cast<clock::duration>(dtPub);
    }
}



void Client::p2pRxLoop_() {
    set_rcvtimeo(subPeers_, 0); // we'll use non-blocking drain
    uint8_t buf[4096];

    if (nextDirRefresh_.time_since_epoch().count() == 0) {
        nextDirRefresh_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    }

    static std::unordered_set<int> firstSeen;
    static auto nextPrune = std::chrono::steady_clock::now();

    while (p2pRunning_.load()) {

        int n = 0;
        do {
            n = zmq_recv(subPeers_, buf, sizeof(buf), ZMQ_DONTWAIT);
            if (n > 0 && static_cast<size_t>(n) >= sizeof(P2PHeader)) {
                const auto* h = reinterpret_cast<const P2PHeader*>(buf);

                if (h->kind == P2PKind::Player && static_cast<size_t>(n) >= sizeof(P2PPlayer)) {
                    const auto* ps = reinterpret_cast<const P2PPlayer*>(buf);
                    if (ps->player_id != my_id_.load()) {
                        std::lock_guard<std::mutex> lk(peersMtx_);
                        auto& rp = peers_[ps->player_id];
                        rp.id       = ps->player_id;
                        rp.x        = ps->x;
                        rp.y        = ps->y;
                        rp.vx       = ps->vx;
                        rp.vy       = ps->vy;
                        rp.facing   = ps->facing;
                        rp.anim     = ps->anim;
                        rp.lastTick = ps->h.tick;
                        rp.lastRecvNs = nowNs();

                        if (!firstSeen.count(ps->player_id)) {
                            firstSeen.insert(ps->player_id);
                            std::cout << "[P2P] first packet from peer " << ps->player_id << "\n";
                        }
                    }
                } else if (h->kind == P2PKind::World && static_cast<size_t>(n) >= sizeof(P2PWorld)) {
                    const auto* w = reinterpret_cast<const P2PWorld*>(buf);
                    const size_t need = sizeof(P2PWorld) + w->platform_count * sizeof(XYRaw);
                    if (static_cast<size_t>(n) >= need) {
                        std::vector<XY> plats;
                        plats.reserve(w->platform_count);
                        const auto* arr = reinterpret_cast<const XYRaw*>(buf + sizeof(P2PWorld));
                        for (uint32_t i=0; i<w->platform_count; ++i) {
                            plats.push_back(XY{arr[i].x, arr[i].y});
                        }
                        { std::scoped_lock l2(plat_mx_); platforms_.swap(plats); }
                        lastP2PWorldRecvNs_.store(nowNs(), std::memory_order_relaxed);
                    }
                }
            }
        } while (n > 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        auto now = std::chrono::steady_clock::now();
        if (now >= nextDirRefresh_) {
            p2pQueryDirectoryAndConnect_();
            nextDirRefresh_ = now + std::chrono::milliseconds(500);
        }

        if (now >= nextPrune) {
            nextPrune = now + std::chrono::seconds(1);
            const int64_t staleNs = 3'000'000'000LL;
            const int64_t cutoff = nowNs() - staleNs;

            std::lock_guard<std::mutex> lk(peersMtx_);
            for (auto it = peers_.begin(); it != peers_.end(); ) {
                if (it->second.lastRecvNs.load() < cutoff) it = peers_.erase(it);
                else ++it;
            }
        }
        const bool serverStale = hadServer_.load(std::memory_order_relaxed) &&
                                 (nowNs() - lastWorldRecvNs_.load(std::memory_order_relaxed) > 1'000'000'000LL);

        int minId = my_id_.load();
        {
            std::lock_guard<std::mutex> lk(peersMtx_);
            for (const auto& kv : peers_) { if (kv.first < minId) minId = kv.first; }
        }

        if (serverStale) {
            if (my_id_.load() == minId) becomeAuthority_();
            else resignAuthority_();
        } else {
            // Prefer real server when it's live
            resignAuthority_();
        }


        authorityMaybeStepAndBroadcast_();
    }
}

std::unordered_map<int, RemotePeerData> Client::p2pSnapshot() {
    std::lock_guard<std::mutex> lk(peersMtx_);

    std::unordered_map<int, RemotePeerData> out;
    out.reserve(peers_.size());

    for (const auto& kv : peers_) {
        const int id = kv.first;
        const RemotePeer& rp = kv.second;

        RemotePeerData d;
        d.id       = id;
        d.x        = rp.x.load(std::memory_order_relaxed);
        d.y        = rp.y.load(std::memory_order_relaxed);
        d.vx       = rp.vx.load(std::memory_order_relaxed);
        d.vy       = rp.vy.load(std::memory_order_relaxed);
        d.facing   = rp.facing.load(std::memory_order_relaxed);
        d.anim     = rp.anim.load(std::memory_order_relaxed);
        d.lastTick = rp.lastTick.load(std::memory_order_relaxed);

        out.emplace(id, d);
    }
    return out;
}

}
