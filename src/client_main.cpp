// Single-player Space Invaders built on the existing Engine.
#include "Engine/engine.h"
#include "Engine/collision.h"
#include "Engine/input.h"
#include "Engine/scaling.h"

#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>
#include <filesystem>
#include <string>

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
    return relative; // fallback
}

// Tuning constants
constexpr float kPlayerSpeed    = 520.0f;
constexpr float kBulletSpeed    = -900.0f;
constexpr float kFireCooldown   = 0.28f;
constexpr float kInvaderBaseSpd = 70.0f;
constexpr float kInvaderAccel   = 4.0f;  // speed up per invader destroyed
constexpr float kInvaderDrop    = 32.0f;
constexpr float kLeftMargin     = 32.0f;
constexpr float kRightMargin    = 32.0f;
constexpr float kTopMargin      = 96.0f;
constexpr float kPlayerYOffset  = 140.0f;
constexpr int   kRows           = 4;
constexpr int   kCols           = 10;

struct GameState;

class Bullet : public Engine::Entity {
public:
    Bullet(float x, float y) : Engine::Entity(ResolveAsset("media/bullet.png").c_str()) {
        setPhysics(false);
        setPos(x, y);
        setType("Bullet");
    }

    void update(float dt) override {
        translate(0.0f, kBulletSpeed * dt);
        if (getPosY() + getHeight() < -64.0f) {
            dead = true;
        }
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
        setPos(Engine::WINDOW_WIDTH * 0.5f - getWidth() * 0.5f,
               Engine::WINDOW_HEIGHT - kPlayerYOffset);
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
    bool movingRight{true};
    bool victory{false};
    bool gameOver{false};
    bool printedOutcome{false};
    int totalInvaders{0};
};

static GameState* G = nullptr;

template<typename T>
void pruneDead(std::vector<T*>& list) {
    list.erase(std::remove_if(list.begin(), list.end(), [](T* ptr) {
        if (ptr && ptr->dead) {
            delete ptr;
            return true;
        }
        return false;
    }), list.end());
}

void Player::update(float dt) {
    if (!G || G->gameOver || G->victory) return;

    float dx = 0.0f;
    if (Engine::Input::keyPressed("left"))  dx -= kPlayerSpeed * dt;
    if (Engine::Input::keyPressed("right")) dx += kPlayerSpeed * dt;
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
            auto* inv = new Invader(x, y, (r + c) % 2 == 0);
            state.invaders.push_back(inv);
        }
    }
}

void updateInvaders(GameState& state, float dt) {
    if (state.invaders.empty()) return;

    const float speed = kInvaderBaseSpd + (state.totalInvaders - static_cast<int>(state.invaders.size())) * kInvaderAccel;
    const float dir = state.movingRight ? 1.0f : -1.0f;
    const float dx  = dir * speed * dt;

    for (auto* inv : state.invaders) {
        inv->translate(dx, 0.0f);
    }

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
        for (auto* inv : state.invaders) {
            inv->translate(0.0f, kInvaderDrop);
        }
    }
}

void handleCollisions(GameState& state) {
    for (auto* b : state.bullets) {
        if (b->dead) continue;
        for (auto* inv : state.invaders) {
            if (inv->dead) continue;
            if (Engine::Collision::check(b, inv)) {
                b->dead = true;
                inv->dead = true;
                break;
            }
        }
    }

    if (state.player && !state.gameOver) {
        for (auto* inv : state.invaders) {
            if (inv->dead) continue;
            if (Engine::Collision::check(inv, state.player) ||
                inv->getPosY() + inv->getHeight() >= state.player->getPosY()) {
                state.gameOver = true;
                break;
            }
        }
    }
}

void cleanup(GameState& state) {
    pruneDead(state.invaders);
    pruneDead(state.bullets);

    if (state.invaders.empty() && !state.victory) {
        state.victory = true;
    }
}

void announceOutcome(GameState& state) {
    if (state.printedOutcome) return;
    if (state.victory) {
        std::puts("[Space Invaders] Victory! All invaders destroyed.");
        state.printedOutcome = true;
    } else if (state.gameOver) {
        std::puts("[Space Invaders] Game over. The invaders reached you.");
        state.printedOutcome = true;
    }
}

void gameUpdate(float dt) {
    if (!G) return;
    updateInvaders(*G, dt);
    handleCollisions(*G);
    cleanup(*G);
    announceOutcome(*G);
}

void buildScene() {
    // Quick verification to avoid null textures/crash when media isn't beside the executable.
    if (!std::filesystem::exists(ResolveAsset("media/bg_starfield.png"))) {
        std::fprintf(stderr, "[Space Invaders] Could not find media folder; tried relative to cwd and executable.\n");
        Engine::stop();
        return;
    }

    Engine::setBackgroundColor(6, 6, 12);
    Engine::Scaling::setMode(Engine::Scaling::PROPORTIONAL_MAINTAIN_ASPECT_X);

    static GameState state;
    state = GameState{};
    G = &state;

    configureInput();

    // background
    auto* bg = new Engine::Entity(ResolveAsset("media/bg_starfield.png").c_str());
    bg->setPhysics(false);
    bg->setCollisions(false);
    bg->setPos(0.0f, 0.0f);
    bg->setType("Background");

    state.player = new Player(state);
    spawnInvaderGrid(state);
}

} // namespace

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
