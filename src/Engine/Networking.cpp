#include "Networking.hpp"
#include <cassert>
#include <cstring>
#include <thread>


static void* make_ctx() { return zmq_ctx_new(); }
static void  free_ctx(void* c) { if(c) zmq_ctx_term(c); }
static void* make_socket(void* ctx, int type) { return zmq_socket(ctx, type); }
static void  close_socket(void* s){ if(s) zmq_close(s); }
static bool  bind_tcp(void* sock, int port){
    char ep[64]; std::snprintf(ep,sizeof(ep),"tcp://*:%d",port);
    return zmq_bind(sock, ep)==0;
}
static bool  connect_tcp(void* sock, const std::string& host, int port){
    char ep[128]; std::snprintf(ep,sizeof(ep),"tcp://%s:%d",host.c_str(),port);
    return zmq_connect(sock, ep)==0;
}
static bool  send_buf(void* sock, const void* data, size_t n){
    return zmq_send(sock, data, n, 0)==(int)n;
}
static int   recv_buf(void* sock, void* out, size_t n){

    return zmq_recv(sock, out, n, 0);
}

namespace Engine { namespace Net {



Server::Server(const ServerConfig& cfg)
: worldHz(cfg.world_hz)
{
    zmq_ctx = make_ctx();
    t0 = Clock::now();


    {
        std::lock_guard<std::mutex> lk(worldMx);
        for (auto s : cfg.platforms) {
            Platform p;
            p.id = s.id;
            p.pos = {s.x, s.y};
            p.minX = s.minX; p.maxX = s.maxX; p.speed = s.speed; p.dir = s.dir;
            platforms.push_back(p);
        }
    }

    for (auto [id, port] : cfg.clients) {
        clientThreads.push_back(ClientThread{id, port, std::thread()});

        std::lock_guard<std::mutex> lk(worldMx);
        players.emplace(id, Player{});
    }
}

Server::~Server() {
    stop();
    free_ctx(zmq_ctx);
}

void Server::start() {
    if (running.exchange(true)) return;


    worldThread = std::thread(&Server::worldLoop, this);


    for (auto& c : clientThreads) {
        c.th = std::thread(&Server::clientRepLoop, this, c.id, c.port);
    }
}

void Server::stop() {
    if (!running.exchange(false)) return;

    for (auto& c : clientThreads) if (c.th.joinable()) c.th.join();
    if (worldThread.joinable()) worldThread.join();
}

void Server::worldLoop() {
    using namespace std::chrono;
    const auto dt = duration<double>(1.0 / worldHz);
    auto next = Clock::now();
    while (running.load()) {
        auto dt = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(1.0 / worldHz)
);
        {
            std::lock_guard<std::mutex> lk(worldMx);
            stepPlatforms(dt.count());
            stepPlayers(dt.count());
            tick++;
        }
        std::this_thread::sleep_until(next);
    }
}

void Server::stepPlatforms(double dt){
    for (auto& p : platforms) {
        p.pos.x += p.dir * p.speed * (float)dt;
        if (p.pos.x < p.minX) { p.pos.x = p.minX; p.dir = +1; }
        if (p.pos.x > p.maxX) { p.pos.x = p.maxX; p.dir = -1; }
    }
}

void Server::stepPlayers(double dt){
    for (auto& kv : players) {
        auto& pl = kv.second;
        const float accel = 40.f;
        const float maxV  = 8.f;
        if (pl.left)  pl.vel.x = std::max(pl.vel.x - accel*(float)dt, -maxV);
        if (pl.right) pl.vel.x = std::min(pl.vel.x + accel*(float)dt,  maxV);
        pl.vel.x *= 0.90f;

        if (pl.jump)  pl.vel.y = -10.f;
        pl.vel.y += 20.f*(float)dt;

        pl.pos.x += pl.vel.x*(float)dt;
        pl.pos.y += pl.vel.y*(float)dt;


    }
}

void Server::snapshotFor(uint32_t client_id, StateMsg& out){
    out.clear();
    out.world_tick = tick;
    out.world_time = std::chrono::duration<double>(Clock::now() - t0).count();


    auto it = players.find(client_id);
    if (it != players.end()) {
        out.me.client_id = client_id;
        out.me.pos = it->second.pos;
        out.me.vel = it->second.vel;
    }


    out.others_count = 0;
    for (auto& kv : players) {
        if (kv.first == client_id) continue;
        if (out.others_count >= MAX_PLAYERS) break;
        auto& slot = out.others[out.others_count++];
        slot.client_id = kv.first;
        slot.pos = kv.second.pos;
        slot.vel = kv.second.vel;
    }


    out.platforms_count = 0;
    for (auto& p : platforms) {
        if (out.platforms_count >= MAX_PLATFORMS) break;
        auto& ps = out.platforms[out.platforms_count++];
        ps.id = p.id;
        ps.pos = p.pos;
    }
}

void Server::clientRepLoop(uint32_t client_id, int port){
    void* rep = make_socket(zmq_ctx, ZMQ_REP);
    if (!bind_tcp(rep, port)) {

        while (running.load()) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        close_socket(rep);
        return;
    }


    {
        std::lock_guard<std::mutex> lk(worldMx);
        players.emplace(client_id, Player{});
    }

    while (running.load()) {
        InputMsg in{};
        int n = recv_buf(rep, &in, sizeof(in));
        if (n <= 0) continue;

        if (in.kind == MsgKind::Input && in.proto_ver == PROTO_VER && in.client_id == client_id) {

            std::lock_guard<std::mutex> lk(worldMx);
            auto it = players.find(client_id);
            if (it != players.end()) {
                it->second.left  = (in.left  != 0);
                it->second.right = (in.right != 0);
                it->second.jump  = (in.jump  != 0);
            }
        }


        StateMsg out{};
        {
            std::lock_guard<std::mutex> lk(worldMx);
            snapshotFor(client_id, out);
        }
        send_buf(rep, &out, sizeof(out));
    }
    close_socket(rep);
}



Client::Client(const std::string& host, int port, uint32_t my_id)
: myId(my_id)
{
    zmq_ctx = make_ctx();
    req = make_socket(zmq_ctx, ZMQ_REQ);
    connect_tcp(req, host, port);
}

Client::~Client(){
    close_socket(req);
    free_ctx(zmq_ctx);
}

bool Client::exchange(bool left, bool right, bool jump, float dt_client, StateMsg& out){
    InputMsg in = InputMsg::make(myId, ++seq, dt_client, left, right, jump);
    if (!send_buf(req, &in, sizeof(in))) return false;
    int n = recv_buf(req, &out, sizeof(out));
    return n == (int)sizeof(out) && out.kind == MsgKind::State && out.proto_ver == PROTO_VER;
}

}}
