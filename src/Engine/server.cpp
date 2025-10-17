// src/Engine/server.cpp — Windows-only build

#include "Engine/server.h"

#include <zmq.h>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

// Winsock: htons/htonl, etc.
#include <winsock2.h>

namespace {

#pragma pack(push,1)
enum class MsgKind : uint8_t { Hello=1, HelloAck=2, UpdatePos=3, Snapshot=4 };

struct HelloMsg { MsgKind kind{MsgKind::Hello}; uint32_t name_len{0}; };
struct HelloAck { MsgKind kind{MsgKind::HelloAck}; int32_t assigned_id{0}; int32_t cmd_port{0}; int32_t pub_port{0}; };
struct UpdatePosMsg { MsgKind kind{MsgKind::UpdatePos}; int32_t id{0}; float x{0}, y{0}; };

// P2P directory wire types
enum class P2PKind : uint8_t { World=1, Player=2, DirRegister=3, DirReply=4 };
struct P2PHeader { P2PKind kind; uint64_t tick; };
struct P2DRegister { P2PHeader h{P2PKind::DirRegister,0}; int32_t want_list{1}; int32_t player_id{0}; uint16_t pub_port{0}; };
struct P2DPeerEndpoint { int32_t player_id; uint32_t ipv4_be; uint16_t port_be; };
struct P2DReply { P2PHeader h{P2PKind::DirReply,0}; int32_t my_player_id; uint32_t peer_count; };
#pragma pack(pop)

// ZMQ helpers
static void set_linger0(void* s){ int v=0; zmq_setsockopt(s, ZMQ_LINGER, &v, sizeof(v)); }
static bool bind_tcp(void* s, const char* host, int port){
    char ep[128]; std::snprintf(ep, sizeof(ep), "tcp://%s:%d", host, port);
    return zmq_bind(s, ep) == 0;
}

// ===== Command socket: Hello / UpdatePos =====
void cmdLoop(std::atomic<bool>* running, const char* host, int cmdPort, void* ctx) {
    void* rep = zmq_socket(ctx, ZMQ_REP);
    if (!rep) { std::cerr << "[Server] REP alloc failed\n"; return; }
    set_linger0(rep);
    if (!bind_tcp(rep, host, cmdPort)) {
        std::cerr << "[Server] bind REP failed on " << host << ":" << cmdPort << "\n";
        zmq_close(rep); return;
    }

    int nextId = 1;

    while (running->load(std::memory_order_relaxed)) {
        uint8_t buf[512];
        int n = zmq_recv(rep, buf, sizeof(buf), 0);
        if (n <= 0) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue; }

        MsgKind kind = *reinterpret_cast<MsgKind*>(buf);
        if (kind == MsgKind::Hello) {
            HelloAck ack{}; ack.assigned_id = nextId++; ack.cmd_port = cmdPort; ack.pub_port = 0;
            zmq_send(rep, &ack, sizeof(ack), 0);
            std::cout << "[Server] Hello -> id " << ack.assigned_id << "\n";
        } else if (kind == MsgKind::UpdatePos) {
            char ok[1] = {1}; zmq_send(rep, ok, 1, 0);
        } else {
            char ok[1] = {0}; zmq_send(rep, ok, 1, 0);
        }
    }

    zmq_close(rep);
}

// ===== Directory: register clients / hand back peer list =====
struct PeerInfo { uint32_t ipv4_be{0}; uint16_t port_be{0}; uint64_t lastSeen{0}; };

void dirLoop(std::atomic<bool>* running, const char* host, int dirPort, void* ctx) {
    void* rep = zmq_socket(ctx, ZMQ_REP);
    if (!rep) { std::cerr << "[Server] DIR REP alloc failed\n"; return; }
    set_linger0(rep);
    if (!bind_tcp(rep, host, dirPort)) {
        std::cerr << "[Server] bind DIR REP failed on " << host << ":" << dirPort << "\n";
        zmq_close(rep); return;
    }

    std::unordered_map<int32_t, PeerInfo> peers;
    int32_t nextId = 1;

    while (running->load(std::memory_order_relaxed)) {
        uint8_t buf[1024];
        int n = zmq_recv(rep, buf, sizeof(buf), 0);
        if (n < static_cast<int>(sizeof(P2DRegister))) { zmq_send(rep, "", 0, 0); continue; }

        auto* reg = reinterpret_cast<P2DRegister*>(buf);

        int32_t id = reg->player_id;
        if (id <= 0) id = nextId++;

        PeerInfo pi{};
        pi.ipv4_be = htonl(0);             // unknown; ZMQ hides remote IP
        pi.port_be = htons(reg->pub_port);  // client’s local PUB port
        pi.lastSeen = static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()
        );
        peers[id] = pi;

        // Build peer list (exclude requester)
        std::vector<P2DPeerEndpoint> list; list.reserve(peers.size());
        for (const auto& kv : peers) {
            const int32_t pid = kv.first; if (pid == id) continue;
            const PeerInfo& info = kv.second;
            list.push_back(P2DPeerEndpoint{ pid, info.ipv4_be, info.port_be });
        }

        std::vector<uint8_t> out(sizeof(P2DReply) + list.size() * sizeof(P2DPeerEndpoint));
        auto* r = reinterpret_cast<P2DReply*>(out.data());
        r->h.kind = P2PKind::DirReply; r->h.tick = 0;
        r->my_player_id = id;
        r->peer_count   = static_cast<uint32_t>(list.size());

        if (!list.empty()) {
            std::memcpy(out.data() + sizeof(P2DReply), list.data(), list.size() * sizeof(P2DPeerEndpoint));
        }

        zmq_send(rep, out.data(), static_cast<int>(out.size()), 0);
    }

    zmq_close(rep);
}

} // namespace

namespace Engine {

int runServer(const char* host, int cmdPort, int /*pubPortUnused*/) {
    void* ctx = zmq_ctx_new();
    if (!ctx) { std::cerr << "[Server] zmq_ctx_new failed\n"; return 1; }

    std::atomic<bool> running{true};
    std::thread tCmd(cmdLoop, &running, host, cmdPort, ctx);
    std::thread tDir(dirLoop, &running, host, 5557,    ctx);

    std::cout << "[Server] Directory-only on " << host
              << "  cmd:" << cmdPort << "  dir:5557\n"
              << "Press ENTER to stop.\n";
    std::cin.get();

    running.store(false, std::memory_order_relaxed);
    if (tCmd.joinable()) tCmd.join();
    if (tDir.joinable()) tDir.join();

    zmq_ctx_term(ctx);
    std::cout << "[Server] stopped.\n";
    return 0;
}

} // namespace Engine
