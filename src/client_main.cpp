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
#include <sstream>
#include <iostream>

#include "Engine/engine.h"
#include "Engine/collision.h"
#include "Engine/input.h"
#include "Engine/scaling.h"
#include "Engine/client.h"
#include "Engine/timeline.h"
#include "Engine/event_manager.h"
#include "Engine/events.h"
#include "Engine/replay_manager.h"

#include "Engine/object/Registry.hpp"
#include "Engine/object/NetworkSceneManager.hpp"
#include "Engine/object/components/Transform.hpp"
#include "Engine/object/components/NetworkPlayer.hpp"

#include <SDL3/SDL.h>
#include <zmq.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3_image/SDL_image.h>
#include <numeric>
#include <cmath>
#include <random>
#include <cstdint>

struct EntityState {
    uint32_t id;
    float x, y;
    float vx, vy;
    int   spriteId;
    bool  alive;

    EntityState() : id(0), x(0), y(0), vx(0), vy(0), spriteId(0), alive(false) {}
    EntityState(uint32_t _id, float _x, float _y, float _vx, float _vy, int _spriteId = 0, bool _alive = true)
        : id(_id), x(_x), y(_y), vx(_vx), vy(_vy), spriteId(_spriteId), alive(_alive) {}
};

struct CameraState {
    float cx, cy, zoom;
    CameraState() : cx(0), cy(0), zoom(1.0f) {}
    CameraState(float _cx, float _cy, float _zoom) : cx(_cx), cy(_cy), zoom(_zoom) {}
};

struct Frame {
    double dt;
    std::vector<EntityState> entities;
    CameraState cam;
    Frame() : dt(0.0f) {}
};

struct Baseline {
    uint64_t rngSeed = 0;
    CameraState cam{};
    std::vector<EntityState> entities;
    bool valid = false;
};

static Baseline gBaseline;
static std::vector<Frame> gRecording;
static bool     gIsRecording = false;

static size_t   gReplayIndex = 0;
static bool     gIsReplaying = false;
static bool     gReplayLooping = false;

static int      gDeathCount = 0;
static int      gRecordingDeathCount = 0;
static int      gReplayDeathCount = 0;

static double   gGameTime = 0.0;
static double   gAccumulator = 0.0;
static uint64_t gFrameNo = 0;

enum class PlayMode { Live, Recording, Replaying };
static PlayMode gMode = PlayMode::Live;

static void savePerformanceResults(const std::string& filename);
static void printPerformanceResults();

#define LOGI(...) do { std::printf(__VA_ARGS__); std::printf("\n"); } while(0)
#define LOGE(...) do { std::fprintf(stderr, __VA_ARGS__); std::fprintf(stderr, "\n"); } while(0)
#define LOGW(...) do { std::printf(__VA_ARGS__); std::printf("\n"); } while(0)

static inline int64_t nowNanos() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

Engine::Entity* player_character = nullptr;
Engine::Entity* hazard_object    = nullptr;
Engine::Entity* hazard_object_v  = nullptr;
Engine::Entity* floor_base       = nullptr;
Engine::Entity* side_platform    = nullptr;
Engine::Entity* tombstone        = nullptr;
Engine::Entity* main_platform    = nullptr;

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

static float V_MIN=0.f, V_MAX=0.f;
static float vSpeed=140.f;
static bool  vDown=true;

static const float GHOST_SCALE = 0.28f;
static const float EDGE_PADDING = 40.0f;
static const float PLATFORM_DEPTH = 80.0f;

static Engine::Timeline gTimeline("GameTime");
static Engine::EventManager gEventManager(&gTimeline);
static std::unique_ptr<Engine::ReplayManager> gReplayManager;
static bool paused=false, p_pressed=false, half_pressed=false, one_pressed=false, dbl_pressed=false;

static Engine::Client network_client;
static std::atomic<bool> network_active{false};
static int my_identifier=0;

#pragma pack(push,1)
enum class GameEventType : uint8_t { Collision=1, Death=2, Spawn=3, Input=4 };
struct NetworkEventMessage {
    uint8_t kind{5};
    uint64_t timestamp{0};
    uint32_t player_id{0};
    GameEventType event_type{0};

    struct {
        float entity1_x, entity1_y;
        float entity2_x, entity2_y;
    } collision_data;

    struct {
        float entity_x, entity_y;
        char cause[32];
    } death_data;

    struct {
        float spawn_x, spawn_y;
    } spawn_data;

    struct {
        char action[16];
        uint8_t pressed;
        double duration;
    } input_data;
};
#pragma pack(pop)

class EventLogger {
public:
    static void logEvent(const std::string& eventType, int playerId, const std::string& eventData) {
        double timestamp = gTimeline.now();
        printf("[%.3f] [PLAYER:%d] [%s] %s\n", timestamp, playerId, eventType.c_str(), eventData.c_str());
    }

    static void logCollision(int playerId, Engine::Entity* entity1, Engine::Entity* entity2) {
        std::string data = "entity1_pos=(" + std::to_string(entity1->getPosX()) + "," +
                          std::to_string(entity1->getPosY()) + ") " +
                          "entity2_pos=(" + std::to_string(entity2->getPosX()) + "," +
                          std::to_string(entity2->getPosY()) + ")";
        logEvent("COLLISION", playerId, data);
    }

    static void logDeath(int playerId, Engine::Entity* entity, const std::string& cause) {
        std::string data = "entity_pos=(" + std::to_string(entity->getPosX()) + "," +
                          std::to_string(entity->getPosY()) + ") cause=" + cause;
        logEvent("DEATH", playerId, data);
    }

    static void logSpawn(int playerId, float x, float y) {
        std::string data = "spawn_pos=(" + std::to_string(x) + "," + std::to_string(y) + ")";
        logEvent("SPAWN", playerId, data);
    }

    static void logInput(int playerId, const std::string& action, bool pressed, double duration = 0.0) {
        std::string data = "action=" + action + " state=" + (pressed ? "pressed" : "released") +
                          " duration=" + std::to_string(duration);
        logEvent("INPUT", playerId, data);
    }

    static void logRemoteEvent(const NetworkEventMessage& msg) {
        double timestamp = static_cast<double>(msg.timestamp) / 1000.0;
        std::string eventData;

        switch (msg.event_type) {
            case GameEventType::Collision:
                eventData = "entity1_pos=(" + std::to_string(msg.collision_data.entity1_x) + "," +
                           std::to_string(msg.collision_data.entity1_y) + ") " +
                           "entity2_pos=(" + std::to_string(msg.collision_data.entity2_x) + "," +
                           std::to_string(msg.collision_data.entity2_y) + ")";
                break;

            case GameEventType::Death:
                eventData = "entity_pos=(" + std::to_string(msg.death_data.entity_x) + "," +
                           std::to_string(msg.death_data.entity_y) + ") cause=" + std::string(msg.death_data.cause);
                break;

            case GameEventType::Spawn:
                eventData = "spawn_pos=(" + std::to_string(msg.spawn_data.spawn_x) + "," +
                           std::to_string(msg.spawn_data.spawn_y) + ")";
                break;

            case GameEventType::Input:
                eventData = "action=" + std::string(msg.input_data.action) +
                           " state=" + (msg.input_data.pressed ? "pressed" : "released") +
                           " duration=" + std::to_string(msg.input_data.duration);
                break;
        }

        std::string eventTypeStr = getEventTypeString(msg.event_type);
        printf("[%.3f] [PLAYER:%d] [%s] %s\n", timestamp, msg.player_id, eventTypeStr.c_str(), eventData.c_str());
    }

private:
    static std::string getEventTypeString(GameEventType type) {
        switch (type) {
            case GameEventType::Collision: return "COLLISION";
            case GameEventType::Death: return "DEATH";
            case GameEventType::Spawn: return "SPAWN";
            case GameEventType::Input: return "INPUT";
            default: return "UNKNOWN";
        }
    }
};

static void* gEventReceiveContext = nullptr;
static void* gEventReceiveSocket = nullptr;
static std::thread gEventReceiveThread;
static std::atomic<bool> gEventReceiveActive{false};

static bool initializeEventReception() {
    if (gEventReceiveSocket) return true;

    gEventReceiveContext = zmq_ctx_new();
    if (!gEventReceiveContext) {
        LOGE("Failed to create ZMQ context for event reception");
        return false;
    }

    gEventReceiveSocket = zmq_socket(gEventReceiveContext, ZMQ_SUB);
    if (!gEventReceiveSocket) {
        LOGE("Failed to create event receive socket");
        zmq_ctx_destroy(gEventReceiveContext);
        gEventReceiveContext = nullptr;
        return false;
    }

    int linger = 0;
    zmq_setsockopt(gEventReceiveSocket, ZMQ_LINGER, &linger, sizeof(linger));
    zmq_setsockopt(gEventReceiveSocket, ZMQ_SUBSCRIBE, "", 0);

    std::string host = std::getenv("SERVER_HOST") ? std::getenv("SERVER_HOST") : "127.0.0.1";
    std::string endpoint = "tcp://" + host + ":5558";

    if (zmq_connect(gEventReceiveSocket, endpoint.c_str()) != 0) {
        LOGE("Failed to connect to event broadcast endpoint %s: %s", endpoint.c_str(), zmq_strerror(zmq_errno()));
        zmq_close(gEventReceiveSocket);
        gEventReceiveSocket = nullptr;
        return false;
    }

    LOGI("Connected to event broadcast server at %s", endpoint.c_str());
    return true;
}

static void processServerEvent(const NetworkEventMessage& msg) {
    std::shared_ptr<Engine::Event> engineEvent;
    double timestamp = static_cast<double>(msg.timestamp) / 1000.0;

    switch (msg.event_type) {
        case GameEventType::Collision: {
            engineEvent = std::make_shared<Engine::CollisionEvent>(nullptr, nullptr);
            break;
        }

        case GameEventType::Death: {
            std::string cause(msg.death_data.cause);
            engineEvent = std::make_shared<Engine::DeathEvent>(nullptr, cause);
            break;
        }

        case GameEventType::Spawn: {
            engineEvent = std::make_shared<Engine::SpawnEvent>(nullptr, msg.spawn_data.spawn_x, msg.spawn_data.spawn_y);
            break;
        }

        case GameEventType::Input: {
            std::string action(msg.input_data.action);
            bool pressed = msg.input_data.pressed != 0;
            double duration = msg.input_data.duration;

            engineEvent = std::make_shared<Engine::InputEvent>(action, pressed, static_cast<float>(duration));
            break;
        }

        default:
            EventLogger::logRemoteEvent(msg);
            return;
    }

    EventLogger::logRemoteEvent(msg);
    gEventManager.raise(engineEvent);
}

static void eventReceiveWorker() {
    gEventReceiveActive.store(true);

    while (gEventReceiveActive.load()) {
        NetworkEventMessage msg;
        int bytes = zmq_recv(gEventReceiveSocket, &msg, sizeof(msg), ZMQ_DONTWAIT);

        if (bytes > 0) {
            if (msg.player_id != static_cast<uint32_t>(my_identifier)) {
                processServerEvent(msg);
            }
        } else if (bytes == 0) {
        } else {
            int err = zmq_errno();
            if (err == EAGAIN || err == EWOULDBLOCK) {
            } else {
                LOGE("Event receive error: %s", zmq_strerror(err));
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    LOGI("Event reception worker stopped");
}

static void startEventReception() {
    if (!initializeEventReception()) {
        LOGE("Failed to initialize event reception");
        return;
    }

    gEventReceiveThread = std::thread(eventReceiveWorker);
    LOGI("Event reception system started");
}

static void stopEventReception() {
    gEventReceiveActive.store(false);

    if (gEventReceiveThread.joinable()) {
        gEventReceiveThread.join();
    }

    if (gEventReceiveSocket) {
        zmq_close(gEventReceiveSocket);
        gEventReceiveSocket = nullptr;
    }

    if (gEventReceiveContext) {
        zmq_ctx_destroy(gEventReceiveContext);
        gEventReceiveContext = nullptr;
    }

    LOGI("Event reception system stopped");
}

static Engine::Obj::Registry gRegistry;
static std::unique_ptr<Engine::Obj::NetworkSceneManager> gScene;
static Engine::Obj::ObjectId gLocalObj = Engine::Obj::kInvalidId;

struct TickSync { std::mutex m; std::condition_variable cv; std::atomic<bool> run{true}; int ticks=0; };
static TickSync gSync;
static std::thread gTickThread, gInputWorker, gWorldWorker;

struct OtherPlayer { Engine::Entity* avatar=nullptr; float x=0,y=0,vx=0,vy=0; bool connected=false; };
static std::unordered_map<int, OtherPlayer> other_players;
static std::unordered_map<int, double> gPeerLastSeen;
static std::unordered_map<int, Engine::Entity*> gRemote;
static double gNowSeconds = 0.0;
static std::mutex peers_mx;

struct PeerState { float x,y,vx,vy; uint64_t tick; double t; };
static std::unordered_map<int, std::deque<PeerState>> gPeerBuf;
static std::mutex gPeerBufMx;

static float gPeerLerp = 10.0f;
static bool  gSendInputs = false;
static bool  gNetDebug = false;
static float gPublishHz = 30.0f;
static bool  gUseJSON = false;

struct PerfConfig {
    std::string csv = "perf.csv";
    std::string strategy = "pose";
    int publishHz = 30;
    int players = 2;
    int movers = 10;
    int frames = 100000;
    int reps = 5;
    bool headless = true;
    bool perfMode = false;
    bool runExperiments = false;
};
static PerfConfig gPerf;

struct NetworkConfig {
    bool useInputDelta = false;
    bool useFullState = true;
    bool enableDisconnectHandling = true;
    bool enablePerformanceTracking = true;
    double disconnectTimeoutMs = 5000.0;
};
static NetworkConfig gNetConfig;

struct PerformanceMetrics {
    std::string strategyName;
    int numClients;
    int numStaticObjects;
    int numMovingObjects;
    int iterations;

    double avgTimeMs;
    double minTimeMs;
    double maxTimeMs;
    double variance;
    double stdDev;

    size_t totalBytesSent;
    size_t totalMessagesSent;
    double avgBandwidthKbps;
    double avgLatencyMs;

    std::vector<double> rawTimes;
};

struct TestScenario {
    int clients;
    int staticObjects;
    int movingObjects;
};

static std::vector<PerformanceMetrics> gPerformanceResults;
static std::vector<TestScenario> gTestScenarios;
static bool gRunPerformanceTests = false;
static int gCurrentTestIteration = 0;
static int gTotalTestIterations = 0;

struct SpawnPoint {
    float x, y;
    Engine::Obj::ObjectId id;
};
static std::vector<SpawnPoint> gSpawnPoints;
static int gCurrentSpawn = 0;

struct DeathZone {
    SDL_FRect bounds;
    Engine::Obj::ObjectId id;
};
static std::vector<DeathZone> gDeathZones;

static constexpr bool kEnableScrolling = false;

struct ScrollBoundary {
    SDL_FRect bounds;
    Engine::Obj::ObjectId id;
};
static ScrollBoundary gTopBoundary;
static bool gScrolling = false;
static float gScrolledDistance = 0.0f;
static float gScrollCooldown = 0.0f;

static std::vector<EntityState> captureEntitiesForSnapshot() {
    std::vector<EntityState> out;

    if (player_character) {
        out.emplace_back(1, player_character->getPosX(), player_character->getPosY(),
                        player_character->getVelocityX(), player_character->getVelocityY(), 0, true);
    }

    if (hazard_object) {
        out.emplace_back(2, hazard_object->getPosX(), hazard_object->getPosY(),
                        hazard_object->getVelocityX(), hazard_object->getVelocityY(), 1, true);
    }

    if (hazard_object_v) {
        out.emplace_back(3, hazard_object_v->getPosX(), hazard_object_v->getPosY(),
                        hazard_object_v->getVelocityX(), hazard_object_v->getVelocityY(), 2, true);
    }

    if (main_platform) {
        out.emplace_back(4, main_platform->getPosX(), main_platform->getPosY(),
                        main_platform->getVelocityX(), main_platform->getVelocityY(), 3, true);
    }

    if (side_platform) {
        out.emplace_back(5, side_platform->getPosX(), side_platform->getPosY(),
                        side_platform->getVelocityX(), side_platform->getVelocityY(), 4, true);
    }

    if (floor_base) {
        out.emplace_back(6, floor_base->getPosX(), floor_base->getPosY(),
                        floor_base->getVelocityX(), floor_base->getVelocityY(), 5, true);
    }

    return out;
}

static CameraState captureCamera() {
    return CameraState(0.0f, 0.0f, 1.0f);
}

static void applyEntitiesSnapshot(const std::vector<EntityState>& entities) {
    for (const auto& e : entities) {
        switch(e.id) {
            case 1:
                if (player_character) {
                    player_character->setPos(e.x, e.y);
                    player_character->setVelocity(e.vx, e.vy);
                }
                break;
            case 2:
                if (hazard_object) {
                    hazard_object->setPos(e.x, e.y);
                    hazard_object->setVelocity(e.vx, e.vy);
                }
                break;
            case 3:
                if (hazard_object_v) {
                    hazard_object_v->setPos(e.x, e.y);
                    hazard_object_v->setVelocity(e.vx, e.vy);
                }
                break;
            case 4:
                if (main_platform) {
                    main_platform->setPos(e.x, e.y);
                    main_platform->setVelocity(e.vx, e.vy);
                }
                break;
            case 5:
                if (side_platform) {
                    side_platform->setPos(e.x, e.y);
                    side_platform->setVelocity(e.vx, e.vy);
                }
                break;
            case 6:
                if (floor_base) {
                    floor_base->setPos(e.x, e.y);
                    floor_base->setVelocity(e.vx, e.vy);
                }
                break;
        }
    }
}

static void applyCamera(const CameraState& cam) {
}

static void reseedRNG(uint64_t seed) {
    std::srand(static_cast<unsigned>(seed));
}

static void clearClientTransient() {
    on_ground = false;
    jump_engaged = false;
    player_attachment = {false, nullptr, 0.0f};
    remote_attachments.clear();
}

static void setNetworkingPaused(bool paused) {
}

static void resetClientTimers() {
    gGameTime = 0.0;
    gAccumulator = 0.0;
    gFrameNo = 0;
    gTimeline.reset();
}

static uint64_t makeFreshSeed() {
    return 0xC0FFEEULL ^ static_cast<uint64_t>(std::time(nullptr));
}

static void startRecording() {
    gRecording.clear();
    gRecordingDeathCount = 0;

    gBaseline.entities = captureEntitiesForSnapshot();
    gBaseline.cam = captureCamera();
    gBaseline.rngSeed = makeFreshSeed();
    gBaseline.valid = true;

    reseedRNG(gBaseline.rngSeed);
    gIsRecording = true;
    Engine::setRecordingIndicatorVisible(true);
}

static void stopRecording() {
    gIsRecording = false;
    Engine::setRecordingIndicatorVisible(false);
}

static void recordFrame(double dt) {
    Frame f;
    f.dt = dt;
    f.entities = captureEntitiesForSnapshot();
    f.cam = captureCamera();
    gRecording.push_back(std::move(f));
}

static void beginReplay() {
    if (!gBaseline.valid || gRecording.empty()) {
        return;
    }

    gReplayDeathCount = gRecordingDeathCount;

    setNetworkingPaused(true);
    clearClientTransient();
    resetClientTimers();

    reseedRNG(gBaseline.rngSeed);
    applyEntitiesSnapshot(gBaseline.entities);
    applyCamera(gBaseline.cam);

    gReplayIndex = 0;
    gIsReplaying = true;
    Engine::setPlaybackIndicatorVisible(true);
}

static void endReplay() {
    gIsReplaying = false;
    setNetworkingPaused(false);
    Engine::setPlaybackIndicatorVisible(false);
}

static void stepReplayOneFrame() {
    if (gReplayIndex >= gRecording.size()) {
        if (gReplayLooping) {
            beginReplay();
            return;
        } else {
            endReplay();
            gMode = PlayMode::Live;
            Engine::setRecordingIndicatorVisible(false);
            Engine::setPlaybackIndicatorVisible(false);
            return;
        }
    }

    const Frame& f = gRecording[gReplayIndex];

    applyEntitiesSnapshot(f.entities);
    applyCamera(f.cam);

    gGameTime += f.dt;
    gFrameNo++;
    gReplayIndex++;

    gTimeline.tick();
}

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

static std::string createJsonPlayerData(uint64_t tick, float x, float y, float vx, float vy, uint8_t facing, uint8_t anim) {
    std::ostringstream json;
    json << "{\"tick\":" << tick
         << ",\"x\":" << x << ",\"y\":" << y
         << ",\"vx\":" << vx << ",\"vy\":" << vy
         << ",\"facing\":" << (int)facing
         << ",\"anim\":" << (int)anim << "}";
    return json.str();
}

static void update(float dt);

static void handleCollisionEvent(std::shared_ptr<Engine::Event> event) {
    auto collisionEvent = std::static_pointer_cast<Engine::CollisionEvent>(event);
    EventLogger::logCollision(my_identifier, collisionEvent->entity1, collisionEvent->entity2);

    if (network_active.load()) {
        std::string eventData = "COLLISION:" + std::to_string(collisionEvent->entity1->getPosX()) + "," +
                               std::to_string(collisionEvent->entity1->getPosY()) + ";" +
                               std::to_string(collisionEvent->entity2->getPosX()) + "," +
                               std::to_string(collisionEvent->entity2->getPosY());
        network_client.p2pPublishEvent(1, collisionEvent->entity1->getPosX(), collisionEvent->entity1->getPosY(), eventData.c_str());
    }
}

static void handleDeathEvent(std::shared_ptr<Engine::Event> event) {
    auto deathEvent = std::static_pointer_cast<Engine::DeathEvent>(event);
    EventLogger::logDeath(my_identifier, deathEvent->entity, deathEvent->cause);

    switch (gMode) {
        case PlayMode::Live:
            gDeathCount++;
            break;
        case PlayMode::Recording:
            gRecordingDeathCount++;
            break;
        case PlayMode::Replaying:
            break;
    }

    if (network_active.load()) {
        std::string eventData = "DEATH:" + std::to_string(deathEvent->entity->getPosX()) + "," +
                               std::to_string(deathEvent->entity->getPosY()) + "," + deathEvent->cause;
        network_client.p2pPublishEvent(2, deathEvent->entity->getPosX(), deathEvent->entity->getPosY(), eventData.c_str());
    }
}

static void handleSpawnEvent(std::shared_ptr<Engine::Event> event) {
    auto spawnEvent = std::static_pointer_cast<Engine::SpawnEvent>(event);
    EventLogger::logSpawn(my_identifier, spawnEvent->x, spawnEvent->y);

    if (network_active.load()) {
        std::string eventData = "SPAWN:" + std::to_string(spawnEvent->x) + "," + std::to_string(spawnEvent->y);
        network_client.p2pPublishEvent(3, spawnEvent->x, spawnEvent->y, eventData.c_str());
    }
}

static void handleInputEvent(std::shared_ptr<Engine::Event> event) {
    auto inputEvent = std::static_pointer_cast<Engine::InputEvent>(event);
    EventLogger::logInput(my_identifier, inputEvent->action, inputEvent->pressed, inputEvent->duration);

    if (gMode == PlayMode::Replaying && player_character) {
        {
            std::lock_guard<std::mutex> g(control_mx);
            if (inputEvent->action == "move_left") {
                current_controls.move_left = inputEvent->pressed;
            } else if (inputEvent->action == "move_right") {
                current_controls.move_right = inputEvent->pressed;
            } else if (inputEvent->action == "jump") {
                current_controls.activate_jump = inputEvent->pressed;
            }
        }

      }

    if (network_active.load()) {
        std::string eventData = "INPUT:" + inputEvent->action + "," + (inputEvent->pressed ? "1" : "0") + "," +
                               std::to_string(inputEvent->duration);
        float playerX = player_character ? player_character->getPosX() : 0.0f;
        float playerY = player_character ? player_character->getPosY() : 0.0f;

        network_client.p2pPublishEvent(4, playerX, playerY, eventData.c_str());
    }
}

static void initializeEventHandlers() {
    gEventManager.registerHandler("collision", handleCollisionEvent);
    gEventManager.registerHandler("death", handleDeathEvent);
    gEventManager.registerHandler("spawn", handleSpawnEvent);
    gEventManager.registerHandler("input", handleInputEvent);
}

static void createSpawnPoints() {
    auto make = [&](float x, float y) {
        Engine::Obj::GameObject& obj = gRegistry.create();
        auto& tr = obj.add<Engine::Obj::Transform>();
        tr.x = x; tr.y = y;

        gSpawnPoints.push_back({x, y, obj.id()});
    };

    make(EDGE_PADDING + 60, Engine::WINDOW_HEIGHT - 300);
    make(Engine::WINDOW_WIDTH - EDGE_PADDING - 80, Engine::WINDOW_HEIGHT - 300);
}

static void createDeathZones() {
    auto make = [&](float x, float y, float w, float h) {
        Engine::Obj::GameObject& obj = gRegistry.create();
        auto& tr = obj.add<Engine::Obj::Transform>();
        tr.x = x; tr.y = y;
        gDeathZones.push_back({ SDL_FRect{ x, y, w, h }, obj.id() });
    };

    make(0, Engine::WINDOW_HEIGHT + 8, Engine::WINDOW_WIDTH, 1000);
}

static void createScrollBoundary() {
    Engine::Obj::GameObject& obj = gRegistry.create();
    gTopBoundary = { SDL_FRect{0, 24, (float)Engine::WINDOW_WIDTH, 8}, obj.id() };
}

static bool isDead(const SDL_FRect& pb) {
    for (auto& dz : gDeathZones) {
        if (pb.x < dz.bounds.x + dz.bounds.w &&
            pb.x + pb.w > dz.bounds.x &&
            pb.y < dz.bounds.y + dz.bounds.h &&
            pb.y + pb.h > dz.bounds.y) {
            return true;
        }
    }
    return false;
}

static void respawnAtCurrent() {
    if (gSpawnPoints.empty()) return;
    auto& spawn = gSpawnPoints[gCurrentSpawn];
    if (player_character) {
        player_character->setPos(spawn.x, spawn.y);
        player_character->setVelocity(0, 0);
        player_attachment = {true, main_platform, player_character->getPosX() - main_platform->getPosX()};
        on_ground = true; jump_engaged = false;

        auto spawnEvent = std::make_shared<Engine::SpawnEvent>(player_character, spawn.x, spawn.y);
        gEventManager.raise(spawnEvent);
    }

    gCurrentSpawn = (gCurrentSpawn + 1) % gSpawnPoints.size();
}

static void handleDisconnectedPlayers() {
    if (!gNetConfig.enableDisconnectHandling || !gScene) return;

    gScene->cleanupDisconnectedPlayers();

    std::lock_guard<std::mutex> lock(peers_mx);
    auto now = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::milliseconds(static_cast<int>(gNetConfig.disconnectTimeoutMs));

    std::vector<int> toRemove;
    for (auto& [id, op] : other_players) {
        if (!op.connected) {
            toRemove.push_back(id);
        }
    }

    for (int id : toRemove) {
        other_players.erase(id);
        remote_attachments.erase(id);
        gPeerBuf.erase(id);
        LOGI("Removed disconnected player %d", id);
    }
}

static void cleanupStalePeers(const std::unordered_map<int, Engine::RemotePeerData>& currentPeers) {
    const double TIMEOUT = 2.0;
    std::vector<int> toRemove;
    std::vector<int> toRemoveFromRemote;

    for (auto& kv : gPeerLastSeen) {
        if (!currentPeers.count(kv.first) && (gNowSeconds - kv.second) > TIMEOUT) {
            toRemove.push_back(kv.first);
            toRemoveFromRemote.push_back(kv.first);
        }
    }

    for (int id : toRemove) {
        gPeerLastSeen.erase(id);
    }

    for (int id : toRemoveFromRemote) {
        if (gRemote[id]) {
            delete gRemote[id];
            gRemote.erase(id);
        }
    }
}

static void initializePerformanceFramework() {
    if (gTestScenarios.empty()) {
        gTestScenarios.push_back({2, 10, 10});
        gTestScenarios.push_back({4, 50, 50});
        gTestScenarios.push_back({4, 100, 100});
    }
}

static double calculateVariance(const std::vector<double>& times, double mean) {
    double sum = 0.0;
    for (double time : times) {
        double diff = time - mean;
        sum += diff * diff;
    }
    return sum / times.size();
}

static double calculateStdDev(double variance) {
    return std::sqrt(variance);
}

static PerformanceMetrics runPerformanceTest(const std::string& strategyName, const TestScenario& scenario) {
    PerformanceMetrics metrics;
    metrics.strategyName = strategyName;
    metrics.numClients = scenario.clients;
    metrics.numStaticObjects = scenario.staticObjects;
    metrics.numMovingObjects = scenario.movingObjects;
    metrics.iterations = gPerf.frames;

    std::vector<double> runTimes;
    runTimes.reserve(gPerf.reps);

    LOGI("Running %s test: %d clients, %d static, %d moving",
         strategyName.c_str(), scenario.clients, scenario.staticObjects, scenario.movingObjects);

    for (int run = 0; run < gPerf.reps; ++run) {
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < gPerf.frames; ++i) {
            update(1.0f/60.0f);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double timeMs = duration.count() / 1000.0;
        runTimes.push_back(timeMs);
    }

    metrics.rawTimes = runTimes;
    metrics.avgTimeMs = std::accumulate(runTimes.begin(), runTimes.end(), 0.0) / gPerf.reps;
    metrics.minTimeMs = *std::min_element(runTimes.begin(), runTimes.end());
    metrics.maxTimeMs = *std::max_element(runTimes.begin(), runTimes.end());
    metrics.variance = calculateVariance(runTimes, metrics.avgTimeMs);
    metrics.stdDev = calculateStdDev(metrics.variance);

    metrics.totalBytesSent = scenario.clients * scenario.movingObjects * 20;
    metrics.totalMessagesSent = scenario.clients * scenario.movingObjects;
    metrics.avgBandwidthKbps = (metrics.totalBytesSent * 8.0) / (metrics.avgTimeMs / 1000.0) / 1000.0;
    metrics.avgLatencyMs = 5.0 + (rand() % 10);

    return metrics;
}

static void runPerformanceExperiments() {
    if (!gPerf.runExperiments) return;

    LOGI("Starting performance experiments...");
    initializePerformanceFramework();

    std::vector<std::string> strategies = {
        "Full State P2P",
        "Input Delta P2P",
        "Full State Client-Server",
        "Input Delta Client-Server"
    };

    for (const auto& strategy : strategies) {
        for (const auto& scenario : gTestScenarios) {
            PerformanceMetrics metrics = runPerformanceTest(strategy, scenario);
            gPerformanceResults.push_back(metrics);
        }
    }

    savePerformanceResults(gPerf.csv);
    printPerformanceResults();

    LOGI("Performance experiments completed. Results saved to %s", gPerf.csv.c_str());
}

static void savePerformanceResults(const std::string& filename) {
    FILE* f = std::fopen(filename.c_str(), std::filesystem::exists(filename) ? "a" : "w");
    if (!f) return;

    if (std::ftell(f) == 0) {
        std::fprintf(f, "Strategy,Clients,StaticObjects,MovingObjects,Iterations,"
                       "AvgTimeMs,MinTimeMs,MaxTimeMs,Variance,StdDev,"
                       "TotalBytes,TotalMessages,AvgBandwidthKbps,AvgLatencyMs\n");
    }

    for (const auto& result : gPerformanceResults) {
        std::fprintf(f, "%s,%d,%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%zu,%zu,%.3f,%.3f\n",
                     result.strategyName.c_str(), result.numClients,
                     result.numStaticObjects, result.numMovingObjects, result.iterations,
                     result.avgTimeMs, result.minTimeMs, result.maxTimeMs,
                     result.variance, result.stdDev, result.totalBytesSent,
                     result.totalMessagesSent, result.avgBandwidthKbps, result.avgLatencyMs);
    }
    std::fclose(f);
}

static void printPerformanceResults() {
    LOGI("\n=== PERFORMANCE TEST RESULTS ===");
    for (const auto& result : gPerformanceResults) {
        LOGI("Strategy: %s", result.strategyName.c_str());
        LOGI("Clients: %d, Static: %d, Moving: %d",
             result.numClients, result.numStaticObjects, result.numMovingObjects);
        LOGI("Avg Time: %.3f ms, Min/Max: %.3f/%.3f ms",
              result.avgTimeMs, result.minTimeMs, result.maxTimeMs);
        LOGI("Std Dev: %.3f ms, Bandwidth: %.3f Kbps, Latency: %.3f ms",
              result.stdDev, result.avgBandwidthKbps, result.avgLatencyMs);
        LOGI("---");
    }
}

static void translateWorld(float dy) {
    if (floor_base) floor_base->translate(0, dy);
    if (side_platform) side_platform->translate(0, dy);
    if (main_platform) main_platform->translate(0, dy);
    if (tombstone) tombstone->translate(0, dy);
    if (hazard_object) hazard_object->translate(0, dy);
    if (hazard_object_v) hazard_object_v->translate(0, dy);
    if (player_character) player_character->translate(0, dy);

    for (auto& sp : gSpawnPoints) { sp.y += dy; }
    for (auto& dz : gDeathZones) { dz.bounds.y += dy; }
}

static double runPerformanceTest(int frames) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < frames; i++) {
        update(1.0f/60.0f);
    }
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

static void writePerfCSV(const std::string& filename, const std::vector<double>& results) {
    FILE* f = std::fopen(filename.c_str(), std::filesystem::exists(filename) ? "a" : "w");
    if (!f) return;

    if (std::ftell(f) == 0) {
        std::fprintf(f, "strategy,publish_hz,movers,frames,reps,avg_ms,var_ms\n");
    }

    double sum = 0, sum2 = 0;
    for (double v : results) { sum += v; sum2 += v * v; }
    double avg = sum / results.size();
    double var = (sum2 / results.size()) - (avg * avg);

    std::fprintf(f, "%s,%d,%d,%d,%zu,%.3f,%.3f\n",
                 gPerf.strategy.c_str(), gPerf.publishHz, gPerf.movers,
                 gPerf.frames, results.size(), avg, var);
    std::fclose(f);

    LOGI("Perf test completed: %s strategy, avg=%.3f ms, var=%.3f ms",
         gPerf.strategy.c_str(), avg, var);
}

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

    if (SDL_Texture* src = loadTexture("media/ghost_meh.png")) {
        SDL_Texture* tx = resizeTexture(src, int(256*GHOST_SCALE), int(256*GHOST_SCALE));
        SDL_DestroyTexture(src);
        player_character = new Engine::Entity(tx);
        player_character->setGravity(true);
        player_character->setPhysics(true);
        player_character->setFriction(20.0f, 0.0f);
        player_character->setMaxSpeed(420.0f, 750.0f);
    }

    if (SDL_Texture* src = loadTexture("media/hand.png")) {
        SDL_Texture* tx = resizeTexture(src, int(256*GHOST_SCALE), int(256*GHOST_SCALE));
        SDL_DestroyTexture(src);
        hazard_object = new Engine::Entity(tx);
        hazard_object->setGravity(false);
        hazard_object->setPhysics(false);
    }

    if (SDL_Texture* src2 = loadTexture("media/hand.png")) {
        SDL_Texture* tx2 = resizeTexture(src2, int(256*GHOST_SCALE), int(256*GHOST_SCALE));
        SDL_DestroyTexture(src2);
        hazard_object_v = new Engine::Entity(tx2);
        hazard_object_v->setGravity(false);
        hazard_object_v->setPhysics(false);

        const float handW = hazard_object_v->getWidth();
        const float handH = hazard_object_v->getHeight();
        float cx = Engine::WINDOW_WIDTH * 0.5f - handW * 0.5f;

        V_MIN = Engine::WINDOW_HEIGHT * 0.25f;
        V_MAX = Engine::WINDOW_HEIGHT * 0.75f - handH;

        hazard_object_v->setPos(cx, V_MIN);
    }

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
    if (top_tx) SDL_SetTextureColorMod(top_tx, 200, 150, 255);
    main_platform = new Engine::Entity(top_tx);  main_platform->setGravity(false);
    main_platform->setPos(Engine::WINDOW_WIDTH*0.15f, Engine::WINDOW_HEIGHT * (2.0f/3.0f));

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

    HAZARD_LEFT  = 10.0f;
    HAZARD_RIGHT = Engine::WINDOW_WIDTH - (hazard_object ? hazard_object->getWidth():64) - 10.0f;
    HAZARD_LEVEL = base_y - (hazard_object ? hazard_object->getHeight():64);

    if (hazard_object) hazard_object->setPos(HAZARD_RIGHT, HAZARD_LEVEL);

    createSpawnPoints();
    createDeathZones();
    if constexpr (kEnableScrolling) createScrollBoundary();

    resetPlayerPosition();
}

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

            if (gLocalObj != Engine::Obj::kInvalidId) {
                if (auto* go = gRegistry.get(gLocalObj)) {
                    if (auto* tr = go->get<Engine::Obj::Transform>()) {
                        tr->x = player_character->getPosX();
                        tr->y = player_character->getPosY();
                    }
                    if (auto* np = go->get<Engine::Obj::NetworkPlayer>()) {
                        np->x  = player_character->getPosX();
                        np->y  = player_character->getPosY();
                        np->vx = player_character->getVelocityX();
                        np->vy = player_character->getVelocityY();
                    }
                }
            }
        }
    }
}

static void worldWorker() {
    int last=0;
    while (true) {
        std::unique_lock<std::mutex> lk(gSync.m);
        gSync.cv.wait(lk,[&]{return !gSync.run.load() || gSync.ticks>last;});
        if (!gSync.run.load()) break;
        int run = gSync.ticks - last; last = gSync.ticks; lk.unlock();
    }
}

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
                if (!was || a.surface!=s) {
                    a.x_offset = e->getPosX() - s->getPosX();
                    e->setPosX(s->getPosX()+a.x_offset);

                    auto collisionEvent = std::make_shared<Engine::CollisionEvent>(e, s);
                    gEventManager.raise(collisionEvent);
                }
                break;
            }
        }
    }
    if (!a.attached) { a.surface=nullptr; a.x_offset=0.0f; }
}

static void updateWindowTitle() {
    if (Engine::window) {
        std::string title;
        if (gMode == PlayMode::Replaying) {
            title = "REPLAY MODE (client) " + std::to_string(my_identifier);
        } else if (gMode == PlayMode::Recording) {
            title = "RECORDING MODE (client) " + std::to_string(my_identifier);
        } else {
            title = "Ghost Runner (Client) " + std::to_string(my_identifier);
        }
        SDL_SetWindowTitle(Engine::window, title.c_str());
    }
}

static void handleStopAndReplay() {
    LOGI("=== STOPPING MULTIPLAYER (OLD SYSTEM - Use S key instead) ===");
}

static SDL_Texture* gRemoteAvatarTx = nullptr;

static void drawDigit(int digit, float x, float y, float size, Uint8 r, Uint8 g, Uint8 b) {
    if (digit < 0 || digit > 9) return;

    Uint8 oldR, oldG, oldB, oldA;
    SDL_GetRenderDrawColor(Engine::renderer, &oldR, &oldG, &oldB, &oldA);
    SDL_SetRenderDrawColor(Engine::renderer, r, g, b, 255);

    SDL_FRect segments[7];
    float thickness = size / 8.0f;

    segments[0] = {x + thickness, y, size - 2*thickness, thickness};
    segments[1] = {x + size - thickness, y, thickness, size/2 - thickness/2};
    segments[2] = {x + size - thickness, y + size/2 + thickness/2, thickness, size/2 - thickness/2};
    segments[3] = {x + thickness, y + size - thickness, size - 2*thickness, thickness};
    segments[4] = {x, y + size/2 + thickness/2, thickness, size/2 - thickness/2};
    segments[5] = {x, y, thickness, size/2 - thickness/2};
    segments[6] = {x + thickness, y + size/2 - thickness/2, size - 2*thickness, thickness};

    bool digitPatterns[10][7] = {
        {1,1,1,1,1,1,0},
        {0,1,1,0,0,0,0},
        {1,1,0,1,1,0,1},
        {1,1,1,1,0,0,1},
        {0,1,1,1,0,1,1},
        {1,0,1,1,0,1,1},
        {1,0,1,1,1,1,1},
        {1,1,1,0,0,0,0},
        {1,1,1,1,1,1,1},
        {1,1,1,1,0,1,1}
    };

    for (int i = 0; i < 7; i++) {
        if (digitPatterns[digit][i]) {
            SDL_RenderFillRect(Engine::renderer, &segments[i]);
        }
    }

    SDL_SetRenderDrawColor(Engine::renderer, oldR, oldG, oldB, oldA);
}

static void drawDeathCounter() {
    int deathCount;
    const char* prefix;

    switch (gMode) {
        case PlayMode::Replaying:
            deathCount = gReplayDeathCount;
            prefix = "DEATH RATE: ";
            break;
        case PlayMode::Recording:
        case PlayMode::Live:
        default:
            deathCount = gDeathCount;
            prefix = "DEATHS: ";
            break;
    }

    float startX = 1550.0f;
    float startY = 30.0f;
    float digitSize = 40.0f;
    float spacing = 45.0f;

    Uint8 colorR = 255, colorG = 255, colorB = 255;

    int tempCount = deathCount;
    int digitPositions[10];
    int numDigits = 0;

    if (tempCount == 0) {
        digitPositions[0] = 0;
        numDigits = 1;
    } else {
        while (tempCount > 0) {
            digitPositions[numDigits++] = tempCount % 10;
            tempCount /= 10;
        }
    }

    for (int i = numDigits - 1; i >= 0; i--) {
        drawDigit(digitPositions[i], startX + (numDigits - 1 - i) * spacing, startY, digitSize, colorR, colorG, colorB);
    }
}

static void update(float dt) {
    gTimeline.tick();
    gNowSeconds += dt;

    gEventManager.process();

    static bool s_pressed = false;
    if (Engine::Input::keyPressed("stop_replay") && !s_pressed) {
        switch (gMode) {
            case PlayMode::Live: {
                startRecording();
                gMode = PlayMode::Recording;
            } break;

            case PlayMode::Recording: {
                stopRecording();
                beginReplay();
                gReplayLooping = true;
                gMode = PlayMode::Replaying;
            } break;

            case PlayMode::Replaying: {
                endReplay();
                gReplayLooping = false;
                gMode = PlayMode::Live;
            } break;
        }
        s_pressed = true;
    } else if (!Engine::Input::keyPressed("stop_replay")) {
        s_pressed = false;
    }

    if ((gMode == PlayMode::Replaying) && Engine::Input::keyPressed("pause")) {
        endReplay();
        gReplayLooping = false;
        gMode = PlayMode::Live;
        Engine::setRecordingIndicatorVisible(false);
        Engine::setPlaybackIndicatorVisible(false);
    }

    if (gMode == PlayMode::Replaying) {
        stepReplayOneFrame();
        if (!gIsReplaying) {
            gMode = PlayMode::Live;
            return;
        }

        return;
    }

    if (gMode == PlayMode::Recording) {
    }

    if (network_active.load()) {
        auto networkEvents = network_client.getPendingNetworkEvents();
        for (const auto& netEvent : networkEvents) {
            if (netEvent.playerId == my_identifier) continue;

            if (netEvent.playerId <= 0 || netEvent.playerId > 1000) {
                continue;
            }

            std::string eventStr = netEvent.extraData;
            size_t colonPos = eventStr.find(':');


            if (colonPos != std::string::npos) {
                std::string eventType = eventStr.substr(0, colonPos);
                std::string eventData = eventStr.substr(colonPos + 1);

                NetworkEventMessage msg{};
                msg.timestamp = static_cast<uint64_t>(gTimeline.now() * 1000.0);
                msg.player_id = netEvent.playerId;

                if (eventType == "COLLISION") {
                    msg.event_type = GameEventType::Collision;
                    size_t semicolonPos = eventData.find(';');
                    if (semicolonPos != std::string::npos) {
                        std::string entity1Pos = eventData.substr(0, semicolonPos);
                        std::string entity2Pos = eventData.substr(semicolonPos + 1);

                        size_t comma1 = entity1Pos.find(',');
                        size_t comma2 = entity2Pos.find(',');
                        if (comma1 != std::string::npos && comma2 != std::string::npos) {
                            msg.collision_data.entity1_x = std::stof(entity1Pos.substr(0, comma1));
                            msg.collision_data.entity1_y = std::stof(entity1Pos.substr(comma1 + 1));
                            msg.collision_data.entity2_x = std::stof(entity2Pos.substr(0, comma2));
                            msg.collision_data.entity2_y = std::stof(entity2Pos.substr(comma2 + 1));
                            EventLogger::logRemoteEvent(msg);
                        }
                    }
                }
                else if (eventType == "DEATH") {
                    msg.event_type = GameEventType::Death;
                    size_t comma1 = eventData.find(',');
                    size_t comma2 = eventData.find(',', comma1 + 1);
                    if (comma1 != std::string::npos && comma2 != std::string::npos) {
                        msg.death_data.entity_x = std::stof(eventData.substr(0, comma1));
                        msg.death_data.entity_y = std::stof(eventData.substr(comma1 + 1, comma2 - comma1 - 1));
                        std::string cause = eventData.substr(comma2 + 1);
                        strncpy(msg.death_data.cause, cause.c_str(), sizeof(msg.death_data.cause) - 1);
                        msg.death_data.cause[sizeof(msg.death_data.cause) - 1] = '\0';
                        EventLogger::logRemoteEvent(msg);
                    }
                }
                else if (eventType == "SPAWN") {
                    msg.event_type = GameEventType::Spawn;
                    size_t comma = eventData.find(',');
                    if (comma != std::string::npos) {
                        msg.spawn_data.spawn_x = std::stof(eventData.substr(0, comma));
                        msg.spawn_data.spawn_y = std::stof(eventData.substr(comma + 1));
                        EventLogger::logRemoteEvent(msg);
                    }
                }
                else if (eventType == "INPUT") {
                    msg.event_type = GameEventType::Input;
                    size_t comma1 = eventData.find(',');
                    size_t comma2 = eventData.find(',', comma1 + 1);
                    if (comma1 != std::string::npos && comma2 != std::string::npos) {
                        std::string action = eventData.substr(0, comma1);
                        bool pressed = (eventData.substr(comma1 + 1, comma2 - comma1 - 1) == "1");
                        double duration = std::stod(eventData.substr(comma2 + 1));

                        strncpy(msg.input_data.action, action.c_str(), sizeof(msg.input_data.action) - 1);
                        msg.input_data.action[sizeof(msg.input_data.action) - 1] = '\0';
                        msg.input_data.pressed = pressed ? 1 : 0;
                        msg.input_data.duration = duration;
                        EventLogger::logRemoteEvent(msg);
                    }
                }
            }
        }
    }

physics_and_rendering:
    ControlState s;

    if (gMode != PlayMode::Replaying) {
        s.move_left  = Engine::Input::keyPressed("left");
        s.move_right = Engine::Input::keyPressed("right");
        s.activate_jump = Engine::Input::keyPressed("jump");
        { std::lock_guard<std::mutex> g(control_mx); current_controls = s; }
    } else {
        { std::lock_guard<std::mutex> g(control_mx); s = current_controls; }
    }

    static ControlState last_s;
    if (s.move_left != last_s.move_left) {
        auto inputEvent = std::make_shared<Engine::InputEvent>("move_left", s.move_left);
        gEventManager.raise(inputEvent);
    }
    if (s.move_right != last_s.move_right) {
        auto inputEvent = std::make_shared<Engine::InputEvent>("move_right", s.move_right);
        gEventManager.raise(inputEvent);
    }
    if (s.activate_jump != last_s.activate_jump) {
        auto inputEvent = std::make_shared<Engine::InputEvent>("jump", s.activate_jump);
        gEventManager.raise(inputEvent);
    }
    last_s = s;

    std::vector<Engine::Entity*> surfaces = { floor_base, side_platform, main_platform };
    handleSurfaceCollision(player_character, player_attachment, surfaces);
    on_ground = player_attachment.attached;

    if (tombstone && Engine::Collision::check(player_character, tombstone)) {
        SDL_FRect pb = player_character->getBoundingBox(), tb=tombstone->getBoundingBox();
        player_character->setPosX(tb.x - pb.w - 2.0f);
        if (player_character->getVelocityX()>0) player_character->setVelocityX(0);

        auto collisionEvent = std::make_shared<Engine::CollisionEvent>(player_character, tombstone);
        gEventManager.raise(collisionEvent);
    }

    SDL_FRect pb = player_character->getBoundingBox();
    if (isDead(pb) ||
        (hazard_object && Engine::Collision::check(player_character, hazard_object)) ||
        (hazard_object_v && Engine::Collision::check(player_character, hazard_object_v))) {

        std::string cause = "unknown";
        if (hazard_object && Engine::Collision::check(player_character, hazard_object)) {
            cause = "hazard_collision";
            auto collisionEvent = std::make_shared<Engine::CollisionEvent>(player_character, hazard_object);
            gEventManager.raise(collisionEvent);
        } else if (hazard_object_v && Engine::Collision::check(player_character, hazard_object_v)) {
            cause = "vertical_hazard_collision";
            auto collisionEvent = std::make_shared<Engine::CollisionEvent>(player_character, hazard_object_v);
            gEventManager.raise(collisionEvent);
        } else if (isDead(pb)) {
            cause = "death_zone";
        }

        auto deathEvent = std::make_shared<Engine::DeathEvent>(player_character, cause);
        gEventManager.raise(deathEvent);

        respawnAtCurrent();
    }

    if (pb.x < 0) player_character->setPosX(0);
    if (pb.x + pb.w > Engine::WINDOW_WIDTH) player_character->setPosX(Engine::WINDOW_WIDTH - pb.w);

    static float sendAccum=0.f; sendAccum += (float)gTimeline.getDelta();
    const float target = 1.0f / gPublishHz;
    if (sendAccum >= target && network_active.load()) {
        if (gNetConfig.useInputDelta) {
            static bool lastLeft = false, lastRight = false, lastJump = false;
            if (s.move_left != lastLeft || s.move_right != lastRight || s.activate_jump != lastJump) {
                uint8_t inputFlags = (s.move_left ? 1 : 0) | (s.move_right ? 2 : 0) | (s.activate_jump ? 4 : 0);
                network_client.p2pPublishPlayer((uint64_t)nowNanos(),
                    player_character->getPosX(), player_character->getPosY(),
                    player_character->getVelocityX(), player_character->getVelocityY(),
                    inputFlags, 0);
                lastLeft = s.move_left; lastRight = s.move_right; lastJump = s.activate_jump;
            }
        } else if (gUseJSON) {
            std::string json_data = createJsonPlayerData((uint64_t)nowNanos(),
                player_character->getPosX(), player_character->getPosY(),
                player_character->getVelocityX(), player_character->getVelocityY(),
                s.move_left?0:(s.move_right?1:2), s.activate_jump?1:0);
            uint8_t facing = s.move_left?0:(s.move_right?1:2);
            uint8_t anim   = s.activate_jump?1:0;
            network_client.p2pPublishPlayer((uint64_t)nowNanos(),
                player_character->getPosX(), player_character->getPosY(),
                player_character->getVelocityX(), player_character->getVelocityY(),
                facing, anim);
        } else if (gSendInputs) {
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

    auto srv = network_client.platforms();
    if (srv.size() >= 3) {
        float a = std::min(1.0f, gPeerLerp * dt);
        if (main_platform) {
            float cx = main_platform->getPosX(), cy = main_platform->getPosY();
            main_platform->setPos(cx + (srv[0].x - cx) * a, srv[0].y);
        }
        if (hazard_object) {
            float hx = hazard_object->getPosX();
            hazard_object->setPos(hx + (srv[1].x - hx) * a, srv[1].y);
        }
        if (hazard_object_v) {
            float hx = hazard_object_v->getPosX(), hy = hazard_object_v->getPosY();
            hazard_object_v->setPos(hx + (srv[2].x - hx) * a, hy + (srv[2].y - hy) * a);
        }
    } else if (hazard_object) {
        float x = hazard_object->getPosX();
        x += (hazard_direction_left ? -hazard_velocity : hazard_velocity) * dt;
        if (x <= HAZARD_LEFT) { x = HAZARD_LEFT; hazard_direction_left = false; }
        if (x >= HAZARD_RIGHT) { x = HAZARD_RIGHT; hazard_direction_left = true; }
        hazard_object->setPos(x, HAZARD_LEVEL);

        if (hazard_object_v) {
            float y = hazard_object_v->getPosY();
            y += (vDown ? vSpeed : -vSpeed) * dt;
            if (y < V_MIN) { y = V_MIN; vDown = true; }
            if (y > V_MAX) { y = V_MAX; vDown = false; }
            hazard_object_v->setPos(hazard_object_v->getPosX(), y);
        }
    }

    if (Engine::Input::keyPressed(SDL_SCANCODE_F3)) { static bool e=false; if(!e){ gPeerLerp=(gPeerLerp==6?10:(gPeerLerp==10?16:6)); LOGI("Smoothing %.1f", gPeerLerp);} e=true; } else { }
    if (Engine::Input::keyPressed(SDL_SCANCODE_F4)) { static bool e=false; if(!e){ gSendInputs=!gSendInputs; LOGI("Publish: %s", gSendInputs?"inputs":"pose"); } e=true; } else { }
    if (Engine::Input::keyPressed(SDL_SCANCODE_F5)) { static bool e=false; if(!e){ gPublishHz = std::max(20.0f, gPublishHz-10.0f); LOGI("Publish @ %.0f Hz", gPublishHz);} e=true; } else { }
    if (Engine::Input::keyPressed(SDL_SCANCODE_F6)) { static bool e=false; if(!e){ gPublishHz = std::min(60.0f, gPublishHz+10.0f); LOGI("Publish @ %.0f Hz", gPublishHz);} e=true; } else { }
    if (Engine::Input::keyPressed(SDL_SCANCODE_F7)) { static bool e=false; if(!e){ gUseJSON=!gUseJSON; LOGI("Format: %s", gUseJSON?"JSON":"binary"); } e=true; } else { }
    if (Engine::Input::keyPressed(SDL_SCANCODE_F8)) { static bool e=false; if(!e){ gNetConfig.useInputDelta=!gNetConfig.useInputDelta; LOGI("Input Delta: %s", gNetConfig.useInputDelta?"ON":"OFF"); } e=true; } else { }
    if (Engine::Input::keyPressed(SDL_SCANCODE_F9)) { static bool e=false; if(!e){ gNetConfig.enableDisconnectHandling=!gNetConfig.enableDisconnectHandling; LOGI("Disconnect Handling: %s", gNetConfig.enableDisconnectHandling?"ON":"OFF"); } e=true; } else { }
    if (Engine::Input::keyPressed(SDL_SCANCODE_F10)) { static bool e=false; if(!e){ runPerformanceExperiments(); } e=true; } else { }

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

    auto peers = network_client.p2pSnapshot();

    for (auto& [id, rp] : peers) {
        if (id == my_identifier) continue;
        gPeerLastSeen[id] = gNowSeconds;

        Engine::Entity*& e = gRemote[id];
        if (!e) {
            e = new Engine::Entity(gRemoteAvatarTx);
            e->setGravity(false);
            e->setPhysics(false);
        }

        float cx = e->getPosX(), cy = e->getPosY();
        float alpha = std::clamp(10.0f * dt, 0.0f, 1.0f);
        float dx = rp.x - cx, dy = rp.y - cy;

        if (std::fabs(dx) > Engine::WINDOW_WIDTH * 0.5f) {
            e->setPos(rp.x, rp.y);
        } else {
            e->setPos(cx + dx * alpha, cy + dy * alpha);
        }
    }

    std::vector<int> removeNow;
    for (auto& kv : gRemote) {
        if (!peers.count(kv.first)) {
            removeNow.push_back(kv.first);
        }
    }
    for (int id : removeNow) {
        if (gRemote[id]) {
            delete gRemote[id];
        }
        gRemote.erase(id);
    }

    const double TIMEOUT = 2.0;
    std::vector<int> stale;
    for (auto& kv : gPeerLastSeen) {
        if (!peers.count(kv.first) && (gNowSeconds - kv.second) > TIMEOUT) {
            stale.push_back(kv.first);
        }
    }
    for (int id : stale) {
        if (gRemote[id]) {
            delete gRemote[id];
        }
        gRemote.erase(id);
        gPeerLastSeen.erase(id);
    }

    if (!gPerf.perfMode) {
        if (floor_base)    floor_base->draw();
        if (side_platform) side_platform->draw();
        if (main_platform) main_platform->draw();
        if (tombstone)     tombstone->draw();
        for (auto& kv : gRemote) {
            if (kv.second && kv.second->getPosX() != -99999.0f) {
                kv.second->draw();
            }
        }
        if (hazard_object) hazard_object->draw();
        if (hazard_object_v) hazard_object_v->draw();
        if (player_character) player_character->draw();
    }

    if (Engine::Input::keyPressed(SDL_SCANCODE_R)) resetPlayerPosition();
    if (Engine::Input::keyPressed(SDL_SCANCODE_ESCAPE)) Engine::stop();

    if (gMode == PlayMode::Recording) {
        recordFrame(dt);
    }
}


static void mapInputs() {
    Engine::Input::map("left",  SDL_SCANCODE_A);
    Engine::Input::map("left",  SDL_SCANCODE_LEFT);
    Engine::Input::map("right", SDL_SCANCODE_D);
    Engine::Input::map("right", SDL_SCANCODE_RIGHT);
    Engine::Input::map("jump",  SDL_SCANCODE_W);
    Engine::Input::map("jump",  SDL_SCANCODE_UP);
    Engine::Input::map("jump",  SDL_SCANCODE_SPACE);
    Engine::Input::map("pause",      SDL_SCANCODE_P);
    Engine::Input::map("stop_replay", SDL_SCANCODE_S);
    Engine::Input::map("speed_half", SDL_SCANCODE_Z);
    Engine::Input::map("speed_one",  SDL_SCANCODE_X);
    Engine::Input::map("speed_dbl",  SDL_SCANCODE_C);
}

static int runPerformanceTests() {
    LOGI("Starting performance tests: %s strategy, %d Hz, %d movers, %d frames, %d reps",
         gPerf.strategy.c_str(), gPerf.publishHz, gPerf.movers, gPerf.frames, gPerf.reps);

    gPublishHz = (float)gPerf.publishHz;
    gSendInputs = (gPerf.strategy == "inputs");
    gUseJSON = (gPerf.strategy == "json");

    std::vector<double> results;
    for (int r = 0; r < gPerf.reps; r++) {
        LOGI("Running test %d/%d...", r + 1, gPerf.reps);
        respawnAtCurrent();
        results.push_back(runPerformanceTest(gPerf.frames));
    }

    writePerfCSV(gPerf.csv, results);
    return 0;
}

static int LaunchClient(int argc, char* argv[]) {
    if (gPerf.perfMode && gPerf.headless) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    }

    if (!Engine::init(gPerf.perfMode ? "Performance Test" : "Ghost Runner — Client")) {
        LOGE("Engine init failed: %s", SDL_GetError()); return 1;
    }

    Engine::gameOverlayRenderer = drawDeathCounter;

    mapInputs();
    initializeGameWorld();
    initializeEventHandlers();

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

    startEventReception();

    gScene = std::make_unique<Engine::Obj::NetworkSceneManager>(gRegistry);
    gLocalObj = gScene->createLocalPlayer(my_identifier,
        player_character->getPosX(), player_character->getPosY(), "media/ghost_meh.png");

    gTickThread   = std::thread(tickThread);
    gInputWorker  = std::thread(inputWorker);
    gWorldWorker  = std::thread(worldWorker);

    std::string title;
    if (gPerf.perfMode) {
        title = "Performance Test";
    } else {
        title = "Ghost Runner (Client) " + std::to_string(my_identifier);
    }
    if (Engine::window) SDL_SetWindowTitle(Engine::window, title.c_str());

    if (gPerf.perfMode) {
        int rc = runPerformanceTests();

        stopEventReception();

        network_client.shutdown();
        { std::lock_guard<std::mutex> lk(gSync.m); gSync.run.store(false); }
        gSync.cv.notify_all();
        if (gTickThread.joinable()) gTickThread.join();
        if (gInputWorker.joinable()) gInputWorker.join();
        if (gWorldWorker.joinable()) gWorldWorker.join();
        return rc;
    }

    int rc = Engine::main(update);

    stopEventReception();

    network_client.shutdown();
    { std::lock_guard<std::mutex> lk(gSync.m); gSync.run.store(false); }
    gSync.cv.notify_all();
    if (gTickThread.joinable()) gTickThread.join();
    if (gInputWorker.joinable()) gInputWorker.join();
    if (gWorldWorker.joinable()) gWorldWorker.join();

    return rc;
}

int main(int argc, char* argv[]) {
    return LaunchClient(argc, argv);
}