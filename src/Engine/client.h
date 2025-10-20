#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>

namespace Engine {

// Simple 2D point used by snapshot/world messages
struct XY { float x{0}, y{0}; };

// Internal, thread-safe state for a remote peer (non-copyable due to atomics)
struct RemotePeer {
    int id{0};
    std::atomic<float>    x{0}, y{0}, vx{0}, vy{0};
    std::atomic<uint8_t>  facing{0}, anim{0};
    std::atomic<uint64_t> lastTick{0};
    std::atomic<int64_t>  lastRecvNs{0};   // steady_clock::now() in ns
};

// Copyable DTO for outside the client (safe to return in containers)
struct RemotePeerData {
    int      id = 0;
    float    x  = 0.f, y  = 0.f, vx = 0.f, vy = 0.f;
    uint8_t  facing = 0,  anim = 0;
    uint64_t lastTick = 0;
};

class Client {
public:
    Client();
    ~Client();

    // Start client (HELLO on 5555); will also SUB to world on 5556 (external world server).
    bool start(const std::string& host, const std::string& displayName);
    int  myId() const { return my_id_; }

    // Legacy compat (server acks; no effect on world)
    void sendPos(float x, float y);

    // World snapshot accessors (platforms from external world server on 5556, or P2P authority when server is down)
    std::unordered_map<int, XY> snapshot() const; // players (usually empty with platform-only server)
    std::vector<XY>             platforms() const;

    // --- Hybrid P2P (peers exchange player state directly) ---
    // dirHost: directory REP host (5557). worldPubPortUnused kept for API compat (pass 0).
    bool startP2P(const std::string& dirHost, int /*worldPubPortUnused*/, int dirPort);
    void stopP2P();

    // Publish local player to peers (~30–60 Hz)
    void p2pPublishPlayer(uint64_t tick,
                          float x,float y,float vx,float vy,
                          uint8_t facing,uint8_t anim);

    // Copyable snapshot of all known peers
    std::unordered_map<int, RemotePeerData> p2pSnapshot();

    // Authority layout (optional, defaults to 1920x1080)
    void configureAuthorityLayout(int winW, int winH);

    // Graceful stop (safe to call multiple times)
    void shutdown();

private:
    // Base client internals
    bool hello(const std::string& host, const std::string& displayName, int cmdPort, int worldPubPort);
    void recvLoop(); // reads world snapshots (e.g., external world server)

    // P2P internals
    bool p2pQueryDirectoryAndConnect_(); // connects only to *new* peers
    void p2pRxLoop_();                   // receives P2P and periodically refreshes directory

    // Authority failover
    void authorityMaybeStepAndBroadcast_(); // if elected authority, step platforms and send P2P world
    void becomeAuthority_();
    void resignAuthority_();

private:
    // ZMQ context & sockets
    void* ctx_{nullptr};
    void* req_{nullptr};      // REQ  -> server HELLO / compat UpdatePos
    void* sub_{nullptr};      // SUB  -> world snapshots (external world server on :5556)
    void* subPeers_{nullptr}; // SUB  -> all peer PUBs (multi-connect)
    void* pubMine_{nullptr};  // PUB  -> this player's state + (if authority) world

    // Threads & state
    std::atomic<bool> running_{false};
    std::thread       recv_thread_;
    std::atomic<int>  my_id_{0};

    // World snapshot buffers
    mutable std::mutex snap_mx_;
    std::unordered_map<int, XY> snap_; // (players if used; often 0 here)
    mutable std::mutex plat_mx_;
    std::vector<XY> platforms_;        // from external world server, or P2P authority
    std::atomic<int64_t> lastWorldRecvNs_{0};   // when we last received server world
    std::atomic<int64_t> lastP2PWorldRecvNs_{0}; // when we last received P2P world

    // P2P
    std::atomic<bool> p2pRunning_{false};
    std::thread       p2pRxThread_;
    std::string       dirHost_;
    int               dirPort_{0};
    int               myPubPort_{0};

    std::mutex peersMtx_;
    std::unordered_map<int, RemotePeer> peers_;
    std::unordered_set<int> connectedPeerIds_;
    std::chrono::steady_clock::time_point nextDirRefresh_{};

    // Authority election & simulation
    std::atomic<bool> isAuthority_{false};
    std::atomic<bool> hadServer_{false}; // true if we have seen server world at least once
    int winW_{1920}, winH_{1080};

    struct AuthPlat { float x,y,minX,maxX,vx,vy; };
    std::vector<AuthPlat> authPlats_;
    std::chrono::steady_clock::time_point nextAuthSim_{};
    std::chrono::steady_clock::time_point nextAuthPub_{};
};

}
