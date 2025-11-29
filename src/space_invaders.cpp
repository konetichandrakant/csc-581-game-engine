#include "Engine/engine.h"
#include "Engine/collision.h"
#include "Engine/input.h"
#include "Engine/scaling.h"
#include "Engine/memory/MemoryManager.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_stdinc.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace {

std::string ResolveAsset(const std::string& relative) {
    namespace fs = std::filesystem;
    static std::vector<fs::path> searchPaths;
    if (searchPaths.empty()) {
        searchPaths.push_back(fs::current_path());
        searchPaths.push_back(fs::current_path().parent_path());
        if (auto* base = SDL_GetBasePath()) {
            fs::path exe(base);
            SDL_free(const_cast<char*>(base));
            searchPaths.push_back(exe);
            searchPaths.push_back(exe.parent_path());
        }
    }
    for (const auto& base : searchPaths) {
        fs::path p = base / relative;
        if (fs::exists(p)) return p.string();
    }
    return relative;
}

constexpr float kPlayerSpeed      = 520.0f;
constexpr float kBulletSpeed      = -900.0f;
constexpr float kFireCooldown     = 0.28f;
constexpr float kInvaderBaseSpd   = 70.0f;
constexpr float kInvaderAccel     = 4.0f;
constexpr float kInvaderDrop      = 32.0f;
constexpr float kLeftMargin       = 32.0f;
constexpr float kRightMargin      = 32.0f;
constexpr float kTopMargin        = 96.0f;
constexpr float kPlayerYOffset    = 140.0f;
constexpr int   kRows             = 4;
constexpr int   kCols             = 10;
constexpr int   kStartingLives    = 3;
constexpr float kEnemyBulletSpeed = 380.0f;
constexpr float kEnemyFireMin     = 1.2f;
constexpr float kEnemyFireMax     = 2.5f;
constexpr float kExplosionLife    = 0.35f;
constexpr float kPromptAlpha      = 200.0f;
constexpr int   kPromptPixelSize  = 6;
constexpr float kDashBoostDur     = 1.0f;
constexpr float kDashBoostMult    = 2.5f;

struct GameState;

class Explosion : public Engine::Entity {
public:
    Explosion(float x, float y) : Engine::Entity(ResolveAsset("media/explosion.png").c_str()) {
        setPhysics(false);
        setCollisions(false);
        setPos(x, y);
        setType("Explosion");
    }
    void update(float dt) override {
        life -= dt;
        if (life <= 0.0f) dead = true;
    }
    float life{kExplosionLife};
    bool dead{false};
};

class Bullet : public Engine::Entity {
public:
    Bullet(float x, float y) : Engine::Entity(ResolveAsset("media/bullet.png").c_str()) {
        setPhysics(false);
        setPos(x, y);
        setType("Bullet");
    }
    void update(float dt) override {
        translate(0.0f, kBulletSpeed * dt);
        if (getPosY() + getHeight() < -64.0f) dead = true;
    }
    bool dead{false};
};

class EnemyBullet : public Engine::Entity {
public:
    EnemyBullet(float x, float y) : Engine::Entity(ResolveAsset("media/bullet.png").c_str()) {
        setPhysics(false);
        setPos(x, y);
        setType("EnemyBullet");
    }
    void update(float dt) override {
        translate(0.0f, kEnemyBulletSpeed * dt);
        if (getPosY() > Engine::WINDOW_HEIGHT + 80.0f) dead = true;
    }
    bool dead{false};
};

class Invader : public Engine::Entity {
public:
    Invader(float x, float y, bool alt) : Engine::Entity(ResolveAsset(alt ? "media/invader_b.png" : "media/invader_a.png").c_str()) {
        setPhysics(false);
        setPos(x, y);
        setType("Invader");
    }
    bool dead{false};
};

class Player : public Engine::Entity {
public:
    explicit Player(GameState& st) : Engine::Entity(ResolveAsset("media/player_ship.png").c_str()), state(st) {
        setPhysics(false);
        setType("Player");
    }
    void update(float dt) override;
    bool dead{false};
private:
    GameState& state;
    float cooldown{0.0f};
};

struct GameState {
    Player* player{nullptr};
    std::vector<Invader*> invaders;
    std::vector<Bullet*> bullets;
    std::vector<EnemyBullet*> enemyBullets;
    std::vector<Explosion*> explosions;

    bool movingRight{true};
    bool victory{false};
    bool gameOver{false};
    bool printedOutcome{false};
    int totalInvaders{0};

    int lives{kStartingLives};
    bool awaitingRestart{false};
    float enemyFireTimer{2.0f};
    float speedScale{1.0f};
    bool promptActive{false};
    bool promptShown{false};
    bool promptExit{false};
    float dashBoostTimer{0.0f};
};

static GameState* G = nullptr;
static std::mt19937 rng{12345u};

template <typename T, typename... Args>
T* Make(Args&&... args) {
    return Engine::Memory::MemoryManager::instance().create<T>(std::forward<Args>(args)...);
}

template <typename T>
void Destroy(T*& ptr) {
    Engine::Memory::MemoryManager::instance().destroy(ptr);
    ptr = nullptr;
}

template<typename T>
void pruneDead(std::vector<T*>& list) {
    list.erase(std::remove_if(list.begin(), list.end(), [](T* ptr) {
        if (ptr && ptr->dead) {
            Destroy(ptr);
            return true;
        }
        return false;
    }), list.end());
}

float randRange(float a, float b) {
    std::uniform_real_distribution<float> dist(a, b);
    return dist(rng);
}

bool anyKeyPressed() {
    int count = 0;
    auto* keys = SDL_GetKeyboardState(&count);
    if (!keys) return false;
    for (int i = 0; i < count; ++i) {
        if (keys[i]) return true;
    }
    return false;
}

void resetPlayer(GameState& state) {
    if (!state.player) return;
    state.player->setPos(Engine::WINDOW_WIDTH * 0.5f - state.player->getWidth() * 0.5f,
                         Engine::WINDOW_HEIGHT - kPlayerYOffset);
    state.player->setVelocity(0, 0);
    state.gameOver = false;
}

void Player::update(float dt) {
    if (!G || G->gameOver || G->victory || G->awaitingRestart) return;

    const float speed = (G->dashBoostTimer > 0.0f) ? kPlayerSpeed * kDashBoostMult : kPlayerSpeed;
    float dx = 0.0f;
    if (Engine::Input::keyPressed("left"))  dx -= speed * dt;
    if (Engine::Input::keyPressed("right")) dx += speed * dt;
    translate(dx, 0.0f);

    const SDL_FRect visible = Engine::Scaling::getVisibleArea();
    const float minX = visible.x + kLeftMargin;
    const float maxX = visible.x + visible.w - kRightMargin - getWidth();
    const float clampedX = std::clamp(getPosX(), minX, maxX);
    setPosX(clampedX);

    cooldown = std::max(0.0f, cooldown - dt);
    if (cooldown <= 0.0f && Engine::Input::keyPressed("fire")) {
        const float bx = getPosX() + getWidth() * 0.5f - 3.0f;
        const float by = getPosY() - 20.0f;
        auto* b = new Bullet(bx, by);
        G->bullets.push_back(b);
        cooldown = kFireCooldown;
    }
}

void configureInput() {
    using namespace Engine;
    Input::map("left", SDL_SCANCODE_LEFT);
    Input::map("left", SDL_SCANCODE_A);
    Input::map("right", SDL_SCANCODE_RIGHT);
    Input::map("right", SDL_SCANCODE_D);
    Input::map("fire", SDL_SCANCODE_SPACE);
    Input::map("pause", SDL_SCANCODE_P);
    Input::map("restart", SDL_SCANCODE_R);
    Input::map("speed_half", SDL_SCANCODE_Z);
    Input::map("speed_one", SDL_SCANCODE_X);
    Input::map("speed_dbl", SDL_SCANCODE_C);
    Input::map("confirm", SDL_SCANCODE_Y);
    Input::map("exit", SDL_SCANCODE_ESCAPE);
    Input::registerChord("dash_boost", {"left", "right"});
}

void spawnInvaderGrid(GameState& state) {
    const float spacingX = 96.0f;
    const float spacingY = 72.0f;
    const float startX   = kLeftMargin + 24.0f;
    const float startY   = kTopMargin;

    state.totalInvaders = kRows * kCols;
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            const float x = startX + spacingX * c;
            const float y = startY  + spacingY * r;
            auto* inv = Make<Invader>(x, y, (r + c) % 2 == 0);
            state.invaders.push_back(inv);
        }
    }
}

void resetGame(GameState& state) {
    for (auto* i : state.invaders) Destroy(i);
    for (auto* b : state.bullets) Destroy(b);
    for (auto* eb : state.enemyBullets) Destroy(eb);
    for (auto* ex : state.explosions) Destroy(ex);
    state.invaders.clear();
    state.bullets.clear();
    state.enemyBullets.clear();
    state.explosions.clear();

    state.movingRight = true;
    state.victory = false;
    state.gameOver = false;
    state.printedOutcome = false;
    state.awaitingRestart = false;
    state.enemyFireTimer = randRange(kEnemyFireMin, kEnemyFireMax);
    state.lives = kStartingLives;
    state.dashBoostTimer = 0.0f;
    spawnInvaderGrid(state);
    resetPlayer(state);
    state.promptActive = false;
    state.promptShown = false;
    state.promptExit = false;
}

void updateInvaders(GameState& state, float dt) {
    if (state.invaders.empty()) return;

    const float speed = kInvaderBaseSpd + (state.totalInvaders - static_cast<int>(state.invaders.size())) * kInvaderAccel;
    const float dir = state.movingRight ? 1.0f : -1.0f;
    const float dx  = dir * speed * dt;

    for (auto* inv : state.invaders) inv->translate(dx, 0.0f);

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    for (auto* inv : state.invaders) {
        minX = std::min(minX, inv->getPosX());
        maxX = std::max(maxX, inv->getPosX() + inv->getWidth());
    }

    const SDL_FRect visible = Engine::Scaling::getVisibleArea();
    const float leftBound  = visible.x + kLeftMargin;
    const float rightBound = visible.x + visible.w - kRightMargin;

    if (minX <= leftBound || maxX >= rightBound) {
        state.movingRight = !state.movingRight;
        for (auto* inv : state.invaders) inv->translate(0.0f, kInvaderDrop);
    }
}

void spawnEnemyBullet(GameState& state) {
    if (state.invaders.empty()) return;
    size_t idx = static_cast<size_t>(randRange(0.0f, float(state.invaders.size())));
    idx = std::min(idx, state.invaders.size() - 1);
    auto* inv = state.invaders[idx];
    if (!inv || inv->dead) return;
    float x = inv->getPosX() + inv->getWidth() * 0.5f;
    float y = inv->getPosY() + inv->getHeight();
    auto* eb = Make<EnemyBullet>(x, y);
    state.enemyBullets.push_back(eb);
}

void handleCollisions(GameState& state) {
    for (auto* b : state.bullets) {
        if (b->dead) continue;
        for (auto* inv : state.invaders) {
            if (inv->dead) continue;
            if (Engine::Collision::check(b, inv)) {
                b->dead = true;
                inv->dead = true;
                state.explosions.push_back(Make<Explosion>(inv->getPosX(), inv->getPosY()));
                break;
            }
        }
    }

    if (state.player && !state.awaitingRestart) {
        for (auto* eb : state.enemyBullets) {
            if (eb->dead) continue;
            if (Engine::Collision::check(eb, state.player)) {
                eb->dead = true;
                state.explosions.push_back(Make<Explosion>(state.player->getPosX(), state.player->getPosY()));
                state.gameOver = true;
                break;
            }
        }
    }

    if (state.player && !state.awaitingRestart) {
        for (auto* inv : state.invaders) {
            if (inv->dead) continue;
            if (Engine::Collision::check(inv, state.player) ||
                inv->getPosY() + inv->getHeight() >= state.player->getPosY()) {
                state.explosions.push_back(Make<Explosion>(state.player->getPosX(), state.player->getPosY()));
                state.gameOver = true;
                break;
            }
        }
    }
}

void applyGameOver(GameState& state) {
    if (!state.gameOver) return;
    state.lives = std::max(0, state.lives - 1);
    if (state.lives > 0) {
        state.gameOver = false;
        resetPlayer(state);
        for (auto* eb : state.enemyBullets) Destroy(eb);
        state.enemyBullets.clear();
    } else {
        state.awaitingRestart = true;
        state.promptActive = true;
        std::puts("[Space Invaders] Lives are over; press 'Y' to play again or ESC to exit.");
    }
}

void cleanup(GameState& state) {
    pruneDead(state.invaders);
    pruneDead(state.bullets);
    pruneDead(state.enemyBullets);
    pruneDead(state.explosions);
    if (state.invaders.empty() && !state.victory) state.victory = true;
    if (state.victory) {
        state.promptActive = true;
    }
}

void announceOutcome(GameState& state) {
    if (state.printedOutcome) return;
    if (state.victory) {
        std::puts("[Space Invaders] Victory! All invaders destroyed.");
        state.printedOutcome = true;
    } else if (state.gameOver) {
        std::puts("[Space Invaders] Hit! Life lost.");
        state.printedOutcome = true;
    }
}

void applySpeedShortcuts(GameState& state) {
    if (!Engine::timeline) return;
    if (Engine::Input::keyPressed("speed_half")) {
        state.speedScale = 0.5f; Engine::timeline->setScale(0.5); std::puts("[Speed] 0.5x");
    }
    if (Engine::Input::keyPressed("speed_one")) {
        state.speedScale = 1.0f; Engine::timeline->setScale(1.0); std::puts("[Speed] 1.0x");
    }
    if (Engine::Input::keyPressed("speed_dbl")) {
        state.speedScale = 2.0f; Engine::timeline->setScale(2.0); std::puts("[Speed] 2.0x");
    }
}

void gameUpdate(float dt) {
    if (!G) return;

    static bool pauseLatch = false;

    for (auto evt : Engine::Input::consumeChordEvents()) {
        if (evt.chordName == "dash_boost") {
            G->dashBoostTimer = kDashBoostDur;
        }
    }
    if (G->dashBoostTimer > 0.0f) {
        G->dashBoostTimer = std::max(0.0f, G->dashBoostTimer - dt);
    }

    if (G->promptActive) {
        if (!G->promptShown) {
            std::puts("[Space Invaders] Play new game? Press 'Y' to start again or ESC to quit.");
            G->promptShown = true;
        }
        if (Engine::Input::keyPressed("confirm")) {
            resetGame(*G);
        } else if (Engine::Input::keyPressed("exit")) {
            Engine::stop();
        }
        return;
    }

    if (Engine::Input::keyPressed("restart")) {
        resetGame(*G);
    }
    applySpeedShortcuts(*G);

    if (Engine::Input::keyPressed("pause") && Engine::timeline) {
        if (!pauseLatch) {
            Engine::timeline->togglePause();
            pauseLatch = true;
        }
    } else {
        pauseLatch = false;
    }

    if (G->awaitingRestart) {
        return;
    }

    updateInvaders(*G, dt);
    handleCollisions(*G);
    applyGameOver(*G);
    cleanup(*G);
    announceOutcome(*G);

    G->enemyFireTimer -= dt;
    if (G->enemyFireTimer <= 0.0f) {
        spawnEnemyBullet(*G);
        G->enemyFireTimer = randRange(kEnemyFireMin, kEnemyFireMax);
    }
}

void drawOverlay() {
    if (!Engine::renderer || !G) return;
    Uint8 oldR, oldG, oldB, oldA;
    SDL_GetRenderDrawColor(Engine::renderer, &oldR, &oldG, &oldB, &oldA);

    for (int i = 0; i < G->lives; ++i) {
        SDL_FRect rect{12.0f + i * 22.0f, 12.0f, 18.0f, 18.0f};
        SDL_SetRenderDrawColor(Engine::renderer, 220, 60, 60, 255);
        SDL_RenderFillRect(Engine::renderer, &rect);
    }

    float speedVal = G->speedScale;
    SDL_FRect bar{12.0f, 40.0f, 80.0f * speedVal, 8.0f};
    SDL_SetRenderDrawColor(Engine::renderer, 80, 200, 255, 255);
    SDL_RenderFillRect(Engine::renderer, &bar);

    if (G->awaitingRestart || G->promptActive) {
        SDL_SetRenderDrawColor(Engine::renderer, 0, 0, 0, static_cast<Uint8>(kPromptAlpha));
        SDL_FRect backdrop{Engine::WINDOW_WIDTH * 0.15f, Engine::WINDOW_HEIGHT * 0.4f,
                           Engine::WINDOW_WIDTH * 0.7f, 120.0f};
        SDL_RenderFillRect(Engine::renderer, &backdrop);
        SDL_SetRenderDrawColor(Engine::renderer, 255, 255, 255, 255);
        SDL_RenderRect(Engine::renderer, &backdrop);

        auto drawLines = [&](float x, float y, const std::vector<std::string>& lines) {
            for (size_t r = 0; r < lines.size(); ++r) {
                for (size_t c = 0; c < lines[r].size(); ++c) {
                    if (lines[r][c] != ' ') {
                        SDL_FRect px{ x + float(c * kPromptPixelSize),
                                      y + float(r * kPromptPixelSize),
                                      float(kPromptPixelSize), float(kPromptPixelSize) };
                        SDL_RenderFillRect(Engine::renderer, &px);
                    }
                }
            }
        };

        const std::vector<std::string> msg = {
            "#######################   ###############################",
            "# PRESS Y TO PLAY AGAIN #   # PRESS ESC TO QUIT GAME   #",
            "#######################   ###############################"
        };
        float textW = float(msg[0].size() * kPromptPixelSize);
        float textH = float(msg.size() * kPromptPixelSize);
        float tx = Engine::WINDOW_WIDTH * 0.5f - textW * 0.5f;
        float ty = Engine::WINDOW_HEIGHT * 0.45f - textH * 0.5f;
        drawLines(tx, ty, msg);
    }

    SDL_SetRenderDrawColor(Engine::renderer, oldR, oldG, oldB, oldA);
}

void buildScene() {
    if (!std::filesystem::exists(ResolveAsset("media/player_ship.png"))) {
        std::fprintf(stderr, "[Space Invaders] Could not find media folder.\n");
        Engine::stop();
        return;
    }

    Engine::setBackgroundColor(0, 0, 0);
    Engine::Scaling::setMode(Engine::Scaling::PROPORTIONAL_MAINTAIN_ASPECT_Y);
    Engine::setOverlayRenderer(drawOverlay);

    static GameState state;
    state = GameState{};
    state.speedScale = 1.0f;
    rng.seed(SDL_GetTicks());
    G = &state;

    Engine::Memory::MemoryManager::instance().configurePool<Invader>(kRows * kCols + 4);
    Engine::Memory::MemoryManager::instance().configurePool<Bullet>(64);
    Engine::Memory::MemoryManager::instance().configurePool<EnemyBullet>(64);
    Engine::Memory::MemoryManager::instance().configurePool<Explosion>(64);
    Engine::Memory::MemoryManager::instance().configurePool<Player>(1);

    configureInput();
    state.player = Make<Player>(state);
    resetGame(state);
}

}

int RunSpaceInvaders() {
    if (!Engine::init("Space Invaders")) {
        std::fprintf(stderr, "Engine init failed: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    buildScene();
    return Engine::main(&gameUpdate);
}

int main() {
    return RunSpaceInvaders();
}
