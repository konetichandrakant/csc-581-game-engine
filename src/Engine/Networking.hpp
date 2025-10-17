#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <zmq.h>
#include "SharedData.hpp"

namespace Engine {

using Clock = std::chrono::steady_clock;

struct ServerConfig {

    std::vector<std::pair<uint32_t,int>> clients;
    double world_hz = 60.0;

    struct PlatformSeed { uint32_t id; float x, y, minX, maxX, speed; int dir; };
    std::vector<PlatformSeed> platforms;
};

class Server {
public:
    explicit Server(const ServerConfig& cfg);
    ~Server();

    void start();
    void stop();

private:
    struct Player {
        Vec2 pos{0,0};
        Vec2 vel{0,0};
        bool left=false, right=false, jump=false;
    };
    struct Platform {
        uint32_t id;
        Vec2 pos{0,0};
        float minX, maxX, speed;
        int dir;
    };

    // world
    std::atomic<bool> running{false};
    std::thread worldThread;
    double worldHz;
    Clock::time_point t0;
    uint64_t tick{0};

    std::unordered_map<uint32_t, Player> players;
    std::vector<Platform> platforms;
    std::mutex worldMx;

    // per-client threads
    struct ClientThread {
        uint32_t id;
        int port;
        std::thread th;
    };
    std::vector<ClientThread> clientThreads;

    // zmq context (one per server process)
    void* zmq_ctx{nullptr};

    // internals
    void worldLoop();
    void clientRepLoop(uint32_t client_id, int port);

    void stepPlatforms(double dt);
    void stepPlayers(double dt);
    void snapshotFor(uint32_t client_id, StateMsg& out);
};

class Client {
public:
    Client(const std::string& host, int port, uint32_t my_id);
    ~Client();

    // Combined REQ/REP exchange (blocking); returns true if a reply was received.
    bool exchange(bool left, bool right, bool jump, float dt_client, StateMsg& out);

private:
    uint32_t myId;
    uint64_t seq{0};
    void* zmq_ctx{nullptr};
    void* req{nullptr};
};

}
