// client_main.cpp — your visuals + friend-style integration (no perf header)
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <mutex>
#include <unordered_map>
#include <deque>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <vector>
#include <filesystem>
#include <thread>
#include <condition_variable>

#include "Engine/engine.h"
#include "Engine/collision.h"
#include "Engine/input.h"
#include "Engine/scaling.h"
#include "Engine/client.h"
#include "Engine/timeline.h"

// 1A seam (component/registry style; we don’t change your visuals)
#include "Engine/object/Registry.hpp"
#include "Engine/object/NetworkSceneManager.hpp"
#include "Engine/object/components/Transform.hpp"
#include "Engine/object/components/NetworkPlayer.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3_image/SDL_image.h>

#define LOGI(...) do { std::printf(__VA_ARGS__); std::printf("\n"); } while(0)
#define LOGE(...) do { std::fprintf(stderr, __VA_ARGS__); std::fprintf(stderr, "\n"); } while(0)

static inline int64_t nowNanos() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

// ---------- your game objects / names ----------
Engine::Entity* player_character = nullptr;
Engine::Entity* hazard_object    = nullptr; // hand "arm"
Engine::Entity* floor_base       = nullptr;
Engine::Entity* side_platform    = nullptr;
Engine::Entity* tombstone        = nullptr;
Engine::Entity* main_platform    = nullptr; // purple mid

struct ControlState { bool move_left=false, move_right=false, activate_jump=false; };
static std::mutex control_mx;
static ControlState current_controls;
static bool on_ground=false, jump_engaged=false;

struct SurfaceAttachment { bool attached=false; Engine::Entity* surface=nullptr; float x_offset=0.0f; };
static SurfaceAttachment player_attachment;
static std::unordered_map<int, SurfaceAttachment> remote_attachments;

static float HAZARD_LEFT=0.f, HAZARD_RIGHT=0.f, HAZARD_LEVEL=0.f;
static float hazard_velocity=60.f;
static bool  hazard_direction_left=true;

static const float GHOST_SCALE = 0.28f;
static const float EDGE_PADDING = 40.0f;
static const float PLATFORM_DEPTH = 80.0f;

static Engine::Timeline gTimeline("GameTime");
static bool paused=false, p_pressed=false, half_pressed=false, one_pressed=false, dbl_pressed=false;

// networking
static Engine::Client network_client;
static std::atomic<bool> network_active{false};
static int my_identifier=0;

// 1A object model seam
static Engine::Obj::Registry gRegistry;
static std::unique_ptr<Engine::Obj::NetworkSceneManager> gScene;
static Engine::Obj::ObjectId gLocalObj = Engine::Obj::kInvalidId;

// 1B friend-style multithreading
struct TickSync { std::mutex m; std::condition_variable cv; std::atomic<bool> run{true}; int ticks=0; };
static TickSync gSync;
static std::thread gTickThread, gInputWorker, gWorldWorker;

// peer visuals & buffer
struct OtherPlayer { Engine::Entity* avatar=nullptr; float x=0,y=0,vx=0,vy=0; bool connected=false; };
static std::unordered_map<int, OtherPlayer> other_players;
static std::mutex peers_mx;

// small jitter buffer per peer
struct PeerState { float x,y,vx,vy; uint64_t tick; double t; };
static std::unordered_map<int, std::deque<PeerState>> gPeerBuf;
static std::mutex gPeerBufMx;

// smoothing & toggles
static float gPeerLerp = 10.0f;        // F3 cycles {6,10,16}
static bool  gSendInputs = false;      // F4: publish input intent instead of pose
static bool  gNetDebug = false;        // F2: print stats
static float gPublishHz = 30.0f;       // F5/F6: change publish rate (20/30/45/60)

static SDL_Texture* loadTexture(const char* path) {
    SDL_Texture* tx = IMG_LoadTexture(Engine::renderer, path);
    if (!tx) {
        std::error_code ec; auto cwd = std::filesystem::current_path(ec).string();
        LOGE("Failed to load %s (cwd=%s) : %s", path, cwd.c_str(), SDL_GetError());
    }
    return tx;
}
static SDL_Texture* resizeTexture(SDL_Texture* src, int w, int h) {
    if (!src) return nullptr;
    SDL_Texture* out = SDL_CreateTexture(Engine::renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
    SDL_SetTextureBlendMode(out, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(Engine::renderer, out);
    SDL_SetRenderDrawColor(Engine::renderer, 0,0,0,0);
    SDL_RenderClear(Engine::renderer);
    SDL_FRect dst{0,0,(float)w,(float)h};
    SDL_RenderTexture(Engine::renderer, src, nullptr, &dst);
    SDL_SetRenderTarget(Engine::renderer, nullptr);
    return out;
}

// ---------- world setup ----------
static void resetPlayerPosition() {
    if (player_character && main_platform) {
        player_character->setVelocity(0,0);
        float cx = main_platform->getPosX() + main_platform->getWidth()/2;
        player_character->setPos(cx - player_character->getWidth()/2,
                                 main_platform->getPosY() - player_character->getHeight());
        player_attachment = {true, main_platform, player_character->getPosX() - main_platform->getPosX()};
        on_ground = true; jump_engaged=false;
    }
}
static void initializeGameWorld() {
    Engine::Scaling::setMode(Engine::Scaling::PROPORTIONAL_MAINTAIN_ASPECT_Y);
    Engine::Physics::setGravity(800.0f);

    // player (ghost)
    if (SDL_Texture* src = loadTexture("media/ghost_meh.png")) {
        SDL_Texture* tx = resizeTexture(src, int(256*GHOST_SCALE), int(256*GHOST_SCALE));
        SDL_DestroyTexture(src);
        player_character = new Engine::Entity(tx);
        player_character->setGravity(true);
        player_character->setPhysics(true);
        player_character->setFriction(20.0f, 0.0f);
        player_character->setMaxSpeed(420.0f, 750.0f);
    }

    // hand hazard
    if (SDL_Texture* src = loadTexture("media/hand.png")) {
        SDL_Texture* tx = resizeTexture(src, int(256*GHOST_SCALE), int(256*GHOST_SCALE));
        SDL_DestroyTexture(src);
        hazard_object = new Engine::Entity(tx);
        hazard_object->setGravity(false);
        hazard_object->setPhysics(false);
    }

    // platforms
    float base_y = Engine::WINDOW_HEIGHT - 200.0f;
    float avail_w = Engine::WINDOW_WIDTH - 2*EDGE_PADDING;
    float plat_w = (avail_w - 0.20f*Engine::WINDOW_WIDTH) / 2.0f;
    int   plat_h = (int)PLATFORM_DEPTH;

    SDL_Texture* plat_src = loadTexture("media/platform_base.png");
    SDL_Texture* plat_tx  = plat_src ? resizeTexture(plat_src, (int)plat_w, plat_h) : nullptr;
    if (plat_src) SDL_DestroyTexture(plat_src);

    floor_base = new Engine::Entity(plat_tx);  floor_base->setGravity(false);  floor_base->setPos(EDGE_PADDING, base_y);
    side_platform = new Engine::Entity(plat_tx); side_platform->setGravity(false);
    side_platform->setPos(Engine::WINDOW_WIDTH - EDGE_PADDING - plat_w, base_y);

    SDL_Texture* top_tx = resizeTexture(plat_tx, int(plat_w*1.2f), plat_h);
    if (top_tx) SDL_SetTextureColorMod(top_tx, 200, 150, 255); // purple tint
    main_platform = new Engine::Entity(top_tx);  main_platform->setGravity(false);
    main_platform->setPos(Engine::WINDOW_WIDTH*0.15f, Engine::WINDOW_HEIGHT * (2.0f/3.0f));

    // tombstone
    if (SDL_Texture* tomb_src = loadTexture("media/rip.png")) {
        int tw=0, th=0; SDL_GetTextureSize(tomb_src, (float*)&tw, (float*)&th);
        float desired_w = plat_w * 0.25f;
        float s = (tw>0? desired_w/float(tw) : 1.0f);
        SDL_Texture* tomb_tx = resizeTexture(tomb_src, int(tw*s), int(th*s));
        SDL_DestroyTexture(tomb_src);
        tombstone = new Engine::Entity(tomb_tx);
        tombstone->setGravity(false);
        tombstone->setPos(side_platform->getPosX() + side_platform->getWidth() - tombstone->getWidth() - 10,
                          side_platform->getPosY() - tombstone->getHeight());
    }

    // hazard bounds
    HAZARD_LEFT  = 10.0f;
    HAZARD_RIGHT = Engine::WINDOW_WIDTH - (hazard_object ? hazard_object->getWidth():64) - 10.0f;
    HAZARD_LEVEL = base_y - (hazard_object ? hazard_object->getHeight():64);

    if (hazard_object) hazard_object->setPos(HAZARD_RIGHT, HAZARD_LEVEL);
    resetPlayerPosition();
}

// ---------- workers ----------
static void tickThread() {
    using clk=std::chrono::steady_clock;
    auto step = std::chrono::duration<double>(1.0/120.0);
    auto next = clk::now();
    while (gSync.run.load()) {
        next += std::chrono::duration_cast<clk::duration>(step);
        { std::lock_guard<std::mutex> lk(gSync.m); ++gSync.ticks; }
        gSync.cv.notify_all();
        std::this_thread::sleep_until(next);
    }
    gSync.cv.notify_all();
}
static void inputWorker() {
    int last=0; const float dt=1.0f/120.0f;
    while (true) {
        std::unique_lock<std::mutex> lk(gSync.m);
        gSync.cv.wait(lk,[&]{return !gSync.run.load() || gSync.ticks>last;});
        if (!gSync.run.load()) break;
        int run = gSync.ticks - last; last = gSync.ticks; lk.unlock();

        for (int i=0;i<run;i++) {
            ControlState s; { std::lock_guard<std::mutex> g(control_mx); s = current_controls; }

            if (!paused) {
                const float SPEED=250.f, JUMP=-600.f;
                float vx = (s.move_left?-SPEED:0.f) + (s.move_right?SPEED:0.f);
                player_character->setVelocityX(vx);
                if (s.activate_jump && !jump_engaged) {
                    player_character->setVelocityY(JUMP);
                    player_attachment.attached=false; player_attachment.surface=nullptr;
                }
            } else {
                player_character->setVelocityX(0);
            }
            jump_engaged = s.activate_jump;

            // 1A: update local player object
            if (gLocalObj != Engine::Obj::kInvalidId) {
                if (auto* o=gRegistry.get(gLocalObj)) {
                    if (auto* tr=o->get<Engine::Obj::Transform>()) {
                        tr->x = player_character->getPosX();
                        tr->y = player_character->getPosY();
                    }
                    if (auto* np=o->get<Engine::Obj::NetworkPlayer>()) {
                        np->x = player_character->getPosX();
                        np->y = player_character->getPosY();
                        np->vx = player_character->getVelocityX();
                        np->vy = player_character->getVelocityY();
                    }
                }
            }
        }
    }
}
static void worldWorker() {
    int last=0; const float dt=1.0f/120.0f; double t=0.0;
    while (true) {
        std::unique_lock<std::mutex> lk(gSync.m);
        gSync.cv.wait(lk,[&]{return !gSync.run.load() || gSync.ticks>last;});
        if (!gSync.run.load()) break;
        int run = gSync.ticks - last; last = gSync.ticks; lk.unlock();

        for (int i=0;i<run;i++) {
            t += dt;

            // server-authoritative: [0]=purple, [1]=hand
            auto srv = network_client.platforms();
            if (srv.size()>=2) {
                float a = std::min(1.0f, gPeerLerp*dt);
                if (main_platform) {
                    float cx=main_platform->getPosX(), cy=main_platform->getPosY();
                    main_platform->setPos(cx + (srv[0].x - cx)*a, srv[0].y);
                }
                if (hazard_object) {
                    float hx=hazard_object->getPosX();
                    hazard_object->setPos(hx + (srv[1].x - hx)*a, srv[1].y);
                }
            } else if (hazard_object) {
                // offline fallback
                float x = hazard_object->getPosX();
                x += (hazard_direction_left?-hazard_velocity:hazard_velocity)*dt;
                if (x<=HAZARD_LEFT){x=HAZARD_LEFT;hazard_direction_left=false;}
                if (x>=HAZARD_RIGHT){x=HAZARD_RIGHT;hazard_direction_left=true;}
                hazard_object->setPos(x, HAZARD_LEVEL);
            }

            // buffer peer snapshots
            auto peers = network_client.p2pSnapshot();
            std::lock_guard<std::mutex> gb(gPeerBufMx);
            for (auto& [id,rp] : peers) {
                PeerState ps{rp.x,rp.y,rp.vx,rp.vy,rp.lastTick,t};
                auto& q = gPeerBuf[id];
                q.push_back(ps);
                while (q.size()>6) q.pop_front();
            }
        }
    }
}

// ---------- collision helper ----------
static void handleSurfaceCollision(Engine::Entity* e, SurfaceAttachment& a,
                                   const std::vector<Engine::Entity*>& surfaces) {
    bool was = a.attached; a.attached=false;
    for (Engine::Entity* s : surfaces) if (s) {
        if (Engine::Collision::check(e, s)) {
            SDL_FRect eb=e->getBoundingBox(), sb=s->getBoundingBox();
            float overlap = eb.y + eb.h - sb.y;
            if (eb.y < sb.y && overlap>0 && overlap < 24.0f && e->getVelocityY()>=0) {
                e->setPosY(sb.y - eb.h); e->setVelocityY(0);
                a.attached=true; a.surface=s;
                if (!was || a.surface!=s) a.x_offset = e->getPosX() - s->getPosX();
                e->setPosX(s->getPosX()+a.x_offset);
                break;
            }
        }
    }
    if (!a.attached) { a.surface=nullptr; a.x_offset=0.0f; }
}

// ---------- frame (render) ----------
static SDL_Texture* gRemoteAvatarTx = nullptr;
static void updateAndRender(float dt) {
    gTimeline.tick();

    ControlState s;
    s.move_left  = Engine::Input::keyPressed("left");
    s.move_right = Engine::Input::keyPressed("right");
    s.activate_jump = Engine::Input::keyPressed("jump");
    { std::lock_guard<std::mutex> g(control_mx); current_controls = s; }

    std::vector<Engine::Entity*> surfaces = { floor_base, side_platform, main_platform };
    handleSurfaceCollision(player_character, player_attachment, surfaces);
    on_ground = player_attachment.attached;

    if (tombstone && Engine::Collision::check(player_character, tombstone)) {
        SDL_FRect pb = player_character->getBoundingBox(), tb=tombstone->getBoundingBox();
        player_character->setPosX(tb.x - pb.w - 2.0f);
        if (player_character->getVelocityX()>0) player_character->setVelocityX(0);
    }

    if (hazard_object && Engine::Collision::check(player_character, hazard_object)) {
        resetPlayerPosition();
    }

    SDL_FRect pb = player_character->getBoundingBox();
    if (pb.x < 0) player_character->setPosX(0);
    if (pb.x + pb.w > Engine::WINDOW_WIDTH) player_character->setPosX(Engine::WINDOW_WIDTH - pb.w);
    if (player_character->getPosY() > Engine::WINDOW_HEIGHT + 600) resetPlayerPosition();

    // publish to peers
    static float sendAccum=0.f; sendAccum += (float)gTimeline.getDelta();
    const float target = 1.0f / gPublishHz;
    if (sendAccum >= target && network_active.load()) {
        if (gSendInputs) {
            uint8_t facing = s.move_left?0:(s.move_right?1:2);
            uint8_t anim   = s.activate_jump?1:0;
            network_client.p2pPublishPlayer((uint64_t)nowNanos(),
                player_character->getPosX(), player_character->getPosY(),
                player_character->getVelocityX(), player_character->getVelocityY(),
                facing, anim);
        } else {
            network_client.p2pPublishPlayer((uint64_t)nowNanos(),
                player_character->getPosX(), player_character->getPosY(),
                player_character->getVelocityX(), player_character->getVelocityY(),
                1, 0);
        }
        network_client.sendPos(player_character->getPosX(), player_character->getPosY());
        sendAccum = 0.f;
    }

    // net & timeline toggles
    if (Engine::Input::keyPressed(SDL_SCANCODE_F3)) { static bool e=false; if(!e){ gPeerLerp=(gPeerLerp==6?10:(gPeerLerp==10?16:6)); LOGI("Smoothing %.1f", gPeerLerp);} e=true; } else { /*release*/ }
    if (Engine::Input::keyPressed(SDL_SCANCODE_F4)) { static bool e=false; if(!e){ gSendInputs=!gSendInputs; LOGI("Publish: %s", gSendInputs?"inputs":"pose"); } e=true; } else { /*release*/ }
    if (Engine::Input::keyPressed(SDL_SCANCODE_F5)) { static bool e=false; if(!e){ gPublishHz = std::max(20.0f, gPublishHz-10.0f); LOGI("Publish @ %.0f Hz", gPublishHz);} e=true; } else { /*release*/ }
    if (Engine::Input::keyPressed(SDL_SCANCODE_F6)) { static bool e=false; if(!e){ gPublishHz = std::min(60.0f, gPublishHz+10.0f); LOGI("Publish @ %.0f Hz", gPublishHz);} e=true; } else { /*release*/ }

    if (Engine::Input::keyPressed("pause"))      { if(!p_pressed){ paused=!paused; if(paused) gTimeline.pause(); else gTimeline.unpause(); } p_pressed=true; } else p_pressed=false;
    if (Engine::Input::keyPressed("speed_half")) { if(!half_pressed) gTimeline.setScale(0.5f); half_pressed=true; } else half_pressed=false;
    if (Engine::Input::keyPressed("speed_one"))  { if(!one_pressed)  gTimeline.setScale(1.0f); one_pressed=true; } else one_pressed=false;
    if (Engine::Input::keyPressed("speed_dbl"))  { if(!dbl_pressed)  gTimeline.setScale(2.0f); dbl_pressed=true; } else dbl_pressed=false;

    if (!gRemoteAvatarTx) {
        if (SDL_Texture* base = loadTexture("media/ghost_meh.png")) {
            gRemoteAvatarTx = resizeTexture(base, int(256*GHOST_SCALE), int(256*GHOST_SCALE));
            SDL_DestroyTexture(base);
            SDL_SetTextureColorMod(gRemoteAvatarTx, 255, 120, 120);
        }
    }

    // smooth remotes from buffer
    std::lock_guard<std::mutex> lock(peers_mx);
    {
        std::lock_guard<std::mutex> gb(gPeerBufMx);
        for (auto& kv : gPeerBuf) {
            int id = kv.first; if (id == my_identifier) continue;
            if (!other_players.count(id)) {
                OtherPlayer np; np.avatar = new Engine::Entity(gRemoteAvatarTx);
                np.avatar->setGravity(false); np.avatar->setPhysics(false);
                np.connected = true; other_players[id]=np; remote_attachments[id]={};
            }
            auto& q = kv.second;
            if (!q.empty()) {
                PeerState ps = q.back();
                auto& op = other_players[id];
                float cx=op.avatar->getPosX(), cy=op.avatar->getPosY();
                float a = std::min(1.0f, gPeerLerp * dt);
                op.avatar->setPos(cx + (ps.x - cx)*a, cy + (ps.y - cy)*a);

                bool was = remote_attachments[id].attached;
                remote_attachments[id].attached=false;
                for (Engine::Entity* srf : { floor_base, side_platform, main_platform }) if (srf) {
                    if (Engine::Collision::check(op.avatar, srf)) {
                        SDL_FRect ab=op.avatar->getBoundingBox(), sb=srf->getBoundingBox();
                        float ov = ab.y + ab.h - sb.y;
                        if (ab.y < sb.y && ov>0 && ov<24.0f && ps.vy>=0) {
                            op.avatar->setPosY(sb.y - ab.h);
                            remote_attachments[id].attached=true; remote_attachments[id].surface=srf;
                            if (!was || remote_attachments[id].surface!=srf)
                                remote_attachments[id].x_offset = op.avatar->getPosX() - srf->getPosX();
                            op.avatar->setPosX(srf->getPosX() + remote_attachments[id].x_offset);
                            break;
                        }
                    }
                }
                if (!remote_attachments[id].attached) { remote_attachments[id].surface=nullptr; remote_attachments[id].x_offset=0.0f; }
            }
        }
    }

    // draw
    if (floor_base)    floor_base->draw();
    if (side_platform) side_platform->draw();
    if (main_platform) main_platform->draw();
    if (tombstone)     tombstone->draw();
    for (auto& [id,op] : other_players) if (op.avatar && op.connected) op.avatar->draw();
    if (hazard_object) hazard_object->draw();
    if (player_character) player_character->draw();

    if (Engine::Input::keyPressed(SDL_SCANCODE_R)) resetPlayerPosition();
    if (Engine::Input::keyPressed(SDL_SCANCODE_ESCAPE)) Engine::stop();
}

// ---------- app bootstrap ----------
static void mapInputs() {
    Engine::Input::map("left",  SDL_SCANCODE_A);
    Engine::Input::map("left",  SDL_SCANCODE_LEFT);
    Engine::Input::map("right", SDL_SCANCODE_D);
    Engine::Input::map("right", SDL_SCANCODE_RIGHT);
    Engine::Input::map("jump",  SDL_SCANCODE_W);
    Engine::Input::map("jump",  SDL_SCANCODE_UP);
    Engine::Input::map("jump",  SDL_SCANCODE_SPACE);
    Engine::Input::map("pause",      SDL_SCANCODE_P);
    Engine::Input::map("speed_half", SDL_SCANCODE_Z);
    Engine::Input::map("speed_one",  SDL_SCANCODE_X);
    Engine::Input::map("speed_dbl",  SDL_SCANCODE_C);
}
static int LaunchClient() {
    if (!Engine::init("Ghost Runner — Client")) {
        LOGE("Engine init failed: %s", SDL_GetError()); return 1;
    }
    mapInputs();
    initializeGameWorld();

    // connect to server + P2P
    std::string host = std::getenv("SERVER_HOST") ? std::getenv("SERVER_HOST") : "127.0.0.1";
    if (!network_client.start(host.c_str(), "Player"))
        LOGE("Server connection failed (%s)", host.c_str());
    my_identifier = network_client.myId();

    if (network_client.startP2P(host.c_str(), 0, 5557)) {
        network_client.configureAuthorityLayout(Engine::WINDOW_WIDTH, Engine::WINDOW_HEIGHT);
        network_active.store(true);
    } else {
        LOGE("P2P start failed");
    }

    // 1A seam registry (does not change your visuals)
    gScene = std::make_unique<Engine::Obj::NetworkSceneManager>(gRegistry);
    gLocalObj = gScene->createLocalPlayer(my_identifier,
        player_character->getPosX(), player_character->getPosY(), "media/ghost_meh.png");

    // workers
    gTickThread   = std::thread(tickThread);
    gInputWorker  = std::thread(inputWorker);
    gWorldWorker  = std::thread(worldWorker);

    std::string title = "Ghost Runner (Client) " + std::to_string(my_identifier);
    if (Engine::window) SDL_SetWindowTitle(Engine::window, title.c_str());

    int rc = Engine::main(updateAndRender);

    // shutdown
    network_client.shutdown();
    { std::lock_guard<std::mutex> lk(gSync.m); gSync.run.store(false); }
    gSync.cv.notify_all();
    if (gTickThread.joinable()) gTickThread.join();
    if (gInputWorker.joinable()) gInputWorker.join();
    if (gWorldWorker.joinable()) gWorldWorker.join();

    return rc;
}
int main(int argc, char* argv[]) {
    if (argc>=2 && (std::string(argv[1])=="--help" || std::string(argv[1])=="-h")) {
        LOGI("Usage: %s", argv[0]); return 0;
    }
    return LaunchClient();
}
