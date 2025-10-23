#include <cstdlib>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <thread>
#include <iostream>
#include <atomic>
#include <csignal>
#include <cstring>
#include <zmq.h>

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "Ws2_32.lib")
#else
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <sys/socket.h>
#endif

static constexpr const char* CMD_ENDPOINT   = "tcp://*:5555";
static constexpr const char* WORLD_ENDPOINT = "tcp://*:5556";
static constexpr const char* DIR_ENDPOINT   = "tcp://*:5557";
static constexpr double WORLD_HZ = 60.0;
static constexpr double SIM_HZ   = 120.0;
static constexpr int SCREEN_W = 1920;
static constexpr int SCREEN_H = 1080;

#pragma pack(push,1)
struct XY { float x,y; };
struct WorldHdr { uint8_t kind{4}; uint64_t tick{0}; uint32_t players{0}; uint32_t plats{0}; };
struct Hello   { uint8_t kind{1}; uint32_t len{0}; };
struct Welcome { uint8_t kind{2}; int32_t id{0}; int32_t cmd_port{5555}; int32_t pub_port{5556}; };

enum class P2PKind : uint8_t { PeerReg=3, PeerList=4 };
struct P2PHeader { P2PKind kind; uint64_t t; };
struct PeerReg { P2PHeader h{P2PKind::PeerReg,0}; int32_t want_list{1}; int32_t player_id{0}; uint16_t pub_port{0}; };
struct PeerInfo { int32_t id; uint32_t ipv4_be; uint16_t port_be; };
struct PeerList { P2PHeader h{P2PKind::PeerList,0}; int32_t my_id; uint32_t count; };
#pragma pack(pop)

struct DynPlatform {
    float x,y; float vx,vy;
    float minX,maxX, minY,maxY;
    float w,h;
    bool is_vertical;
};
struct ClientConn { uint16_t port_be{0}; std::chrono::steady_clock::time_point lastSeen; };

static int gNumMovers = 2;
static int gNumVertical = 1;
static bool gEnablePerformanceTracking = false;
static bool gEnableDisconnectHandling = true;
static double gDisconnectTimeoutSeconds = 5.0;

static std::atomic<bool> running{true};
static void on_sigint(int){ running.store(false); }

// Publish moving platform positions to all clients
static void world_pub(void* ctx) {
    void* pub = zmq_socket(ctx, ZMQ_PUB);
    int linger=0, one=1; zmq_setsockopt(pub, ZMQ_LINGER, &linger, sizeof(linger));
    zmq_setsockopt(pub, ZMQ_CONFLATE, &one, sizeof(one));
    if (zmq_bind(pub, WORLD_ENDPOINT)!=0) { std::cerr << "[world] bind failed: " << zmq_strerror(zmq_errno()) << "\n"; zmq_close(pub); return; }
    std::cout << "[world] PUB @ 5556\n";

    std::vector<DynPlatform> plats;
    float left=120.f, right=float(SCREEN_W-320);

    if (gNumMovers >= 1) {
        plats.push_back({ 200.f, float(SCREEN_H- 520), +220.f, 0.f, left, right, 0, 0, 300.f, 80.f, false});
    }
    if (gNumMovers >= 2) {
        plats.push_back({ right, float(SCREEN_H- 200 - 64), -260.f, 0.f, 10.f, float(SCREEN_W-90), 0, 0, 64.f,64.f, false});
    }

    for (int i = 2; i < gNumMovers; i++) {
        float speed = 150.f + (i * 30.f);
        float y = float(SCREEN_H - 300 - (i * 80));
        plats.push_back({ left + (i * 100), y, speed, 0.f, left, right - (i * 50), 0, 0, 250.f, 60.f, false });
    }

    for (int i = 0; i < gNumVertical; i++) {
        float speed = 180.f + (i * 40.f);
        float x = 800.f + (i * 200);
        float minY = 200.f + (i * 50);
        float maxY = float(SCREEN_H - 300 - (i * 30));
        plats.push_back({ x, minY, 0.f, speed, 0, 0, minY, maxY, 300.f, 80.f, true });
    }

    using clk=std::chrono::steady_clock;
    auto dtSim = std::chrono::duration<double>(1.0/SIM_HZ);
    auto dtPub = std::chrono::duration<double>(1.0/WORLD_HZ);
    auto nextSim=clk::now(), nextPub=clk::now(); uint64_t tick=0;

    while (running.load()) {
        auto now=clk::now();
        if (now>=nextSim) {
            double ds = dtSim.count();
            for (auto& p: plats) {
                if (p.is_vertical) {
                    p.y += p.vy*ds;
                    if (p.y < p.minY) { p.y=p.minY; p.vy= std::abs(p.vy); }
                    if (p.y + p.h > p.maxY) { p.y=p.maxY - p.h; p.vy= -std::abs(p.vy); }
                } else {
                    p.x += p.vx*ds;
                    if (p.x < p.minX) { p.x=p.minX; p.vx= std::abs(p.vx); }
                    if (p.x + p.w > p.maxX) { p.x=p.maxX - p.w; p.vx= -std::abs(p.vx); }
                }
            }
            nextSim += std::chrono::duration_cast<clk::duration>(dtSim);
            ++tick;
        }
        if (now>=nextPub) {
            WorldHdr hdr; hdr.tick=tick; hdr.plats=(uint32_t)plats.size();
            std::vector<uint8_t> buf(sizeof(hdr) + hdr.plats*sizeof(XY));
            std::memcpy(buf.data(), &hdr, sizeof(hdr));
            size_t off=sizeof(hdr);
            for (auto& p: plats) { XY xy{p.x, p.y}; std::memcpy(buf.data()+off, &xy, sizeof(xy)); off+=sizeof(xy); }

            zmq_msg_t m; zmq_msg_init_size(&m, buf.size());
            std::memcpy(zmq_msg_data(&m), buf.data(), buf.size());
            zmq_msg_send(&m, pub, 0); zmq_msg_close(&m);

            nextPub += std::chrono::duration_cast<clk::duration>(dtPub);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    zmq_close(pub);
}

// Handle client hello requests and assign unique player IDs
static void hello_rep(void* ctx) {
    void* rep = zmq_socket(ctx, ZMQ_REP);
    int linger=0; zmq_setsockopt(rep, ZMQ_LINGER, &linger, sizeof(linger));
    if (zmq_bind(rep, CMD_ENDPOINT)!=0) { std::cerr << "[hello] bind failed\n"; zmq_close(rep); return; }
    std::cout << "[hello] REP @ 5555\n";

    int nextId=1;
    while (running.load()) {
        uint8_t buf[512]; int n = zmq_recv(rep, buf, sizeof(buf), ZMQ_DONTWAIT);
        if (n>0) {
            if (n >= (int)sizeof(Hello) && buf[0]==1) {
                Welcome w; w.id = nextId++; zmq_send(rep, &w, sizeof(w), 0);
                std::cout << "[hello] new id="<<w.id<<"\n";
            } else {
                char ok=1; zmq_send(rep, &ok, 1, 0);
            }
        } else if (n==-1 && zmq_errno()!=EAGAIN) std::cerr << "[hello] recv error: " << zmq_strerror(zmq_errno()) << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    zmq_close(rep);
}

// Manage P2P peer directory and handle client discovery
static void directory_rep(void* ctx) {
    void* rep = zmq_socket(ctx, ZMQ_REP);
    int linger=0; zmq_setsockopt(rep, ZMQ_LINGER, &linger, sizeof(linger));
    if (zmq_bind(rep, DIR_ENDPOINT)!=0) { std::cerr << "[dir] bind failed\n"; zmq_close(rep); return; }
    std::cout << "[dir] REP @ 5557\n";

    std::unordered_map<int32_t, ClientConn> peers;
    int32_t nextId=1;

    std::thread janitor([&](){
        while (running.load()) {
            auto now=std::chrono::steady_clock::now();
            const auto TO=std::chrono::seconds(static_cast<int>(gDisconnectTimeoutSeconds));
            std::vector<int32_t> dead;
            for (auto& [id,cc]:peers) if (now-cc.lastSeen>TO) dead.push_back(id);
            for (auto id:dead) {
                peers.erase(id);
                std::cout << "[dir] pruned disconnected client " << id << "\n";
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

    while (running.load()) {
        uint8_t buf[1024]; int n=zmq_recv(rep, buf, sizeof(buf), ZMQ_DONTWAIT);
        if (n>0 && n>=(int)sizeof(PeerReg)) {
            auto* reg = reinterpret_cast<PeerReg*>(buf);
            int32_t id = reg->player_id; if (id<=0) id = nextId++;
            ClientConn cc; cc.port_be = htons(reg->pub_port); cc.lastSeen = std::chrono::steady_clock::now();
            peers[id] = cc;

            std::vector<PeerInfo> list;
            for (auto& [pid,ppi] : peers) if (pid!=id) {
                uint32_t loopback = htonl(0x7F000001);
                list.push_back({pid, loopback, ppi.port_be});
            }

            PeerList out; out.h.kind=P2PKind::PeerList; out.my_id=id; out.count=(uint32_t)list.size();
            std::vector<uint8_t> pkt(sizeof(out)+list.size()*sizeof(PeerInfo));
            std::memcpy(pkt.data(), &out, sizeof(out));
            if (!list.empty()) std::memcpy(pkt.data()+sizeof(out), list.data(), list.size()*sizeof(PeerInfo));
            zmq_send(rep, pkt.data(), (int)pkt.size(), 0);

            std::cout << "[dir] id="<<id<<" peers_out="<<list.size()<<" total="<<peers.size()<<"\n";
        } else if (n==-1 && zmq_errno()!=EAGAIN) std::cerr << "[dir] recv error: " << zmq_strerror(zmq_errno()) << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (janitor.joinable()) janitor.join();
    zmq_close(rep);
}

// Parse command line arguments for server configuration
static void parseArguments(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--movers") == 0 && i + 1 < argc) {
            gNumMovers = std::max(1, std::min(20, atoi(argv[++i])));
        } else if (strcmp(argv[i], "--vertical") == 0 && i + 1 < argc) {
            gNumVertical = std::max(0, std::min(10, atoi(argv[++i])));
        } else if (strcmp(argv[i], "--performance-tracking") == 0) {
            gEnablePerformanceTracking = true;
        } else if (strcmp(argv[i], "--disconnect-handling") == 0) {
            gEnableDisconnectHandling = true;
        } else if (strcmp(argv[i], "--disconnect-timeout") == 0 && i + 1 < argc) {
            gDisconnectTimeoutSeconds = std::max(1.0, std::min(60.0, atof(argv[++i])));
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --movers N        Number of horizontal moving platforms (1-20, default: 2)\n";
            std::cout << "  --vertical N      Number of vertical moving platforms (0-10, default: 1)\n";
            std::cout << "  --performance-tracking Enable performance tracking\n";
            std::cout << "  --disconnect-handling Enable disconnect handling\n";
            std::cout << "  --disconnect-timeout SEC Disconnect timeout in seconds (default: 5.0)\n";
            std::cout << "  --help, -h        Show this help\n";
            exit(0);
        }
    }
}

// Main server entry point - initialize networking and start service threads
int main(int argc, char* argv[]) {
    WSADATA w; if (WSAStartup(MAKEWORD(2,2), &w)!=0) { std::cerr << "WSAStartup failed\n"; return 1; }
    parseArguments(argc, argv);

    std::signal(SIGINT, on_sigint);
    std::cout << "Game Server starting… ports: 5555 (hello), 5556 (world), 5557 (dir)\n";
    std::cout << "Configuration: " << gNumMovers << " horizontal movers, " << gNumVertical << " vertical movers\n";
    std::cout << "Features: Performance tracking=" << (gEnablePerformanceTracking ? "ON" : "OFF")
              << ", Disconnect handling=" << (gEnableDisconnectHandling ? "ON" : "OFF")
              << ", Timeout=" << gDisconnectTimeoutSeconds << "s\n";

    void* ctx = zmq_ctx_new();
    if (!ctx) { std::cerr << "zmq_ctx_new failed\n"; return 1; }

    std::thread t1([&]{ world_pub(ctx); });
    std::thread t2([&]{ hello_rep(ctx); });
    std::thread t3([&]{ directory_rep(ctx); });

    while (running.load()) std::this_thread::sleep_for(std::chrono::milliseconds(200));

    if (t1.joinable()) t1.join();
    if (t2.joinable()) t2.join();
    if (t3.joinable()) t3.join();

    zmq_ctx_term(ctx);
    WSACleanup();
    std::cout << "Server stopped\n";
    return 0;
}