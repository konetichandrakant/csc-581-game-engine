#include "Engine/engine.h"
#include "Engine/input.h"
#include "Engine/scaling.h"
#include "Engine/memory/MemoryManager.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

constexpr int   kGridCols          = 26;
constexpr int   kGridRows          = 18;
constexpr float kCellSize          = 32.0f;
constexpr float kBaseStepInterval  = 0.38f;
constexpr float kMinStepInterval   = 0.12f;
constexpr float kGridBorder        = 8.0f;
constexpr float kPromptAlpha       = 200.0f;
constexpr int   kPromptPixelSize   = 6;

struct Segment {
    int x;
    int y;
};

enum class Direction { Up, Down, Left, Right };

struct GameState {
    std::vector<Segment*> snake;
    Direction dir{Direction::Right};
    Direction queued{Direction::Right};
    SDL_Point food{0, 0};
    bool foodActive{false};
    bool gameOver{false};
    bool paused{false};
    bool printedOutcome{false};
    float tickAccumulator{0.0f};
    float interpAlpha{0.0f};
    int score{0};
    std::vector<Segment> lastSnake;
    bool promptActive{false};
    bool promptShown{false};
    float speedScale{1.0f};
};

static GameState* G = nullptr;
static std::mt19937 rng{1234u};

template <typename T, typename... Args>
T* Make(Args&&... args) {
    return Engine::Memory::MemoryManager::instance().create<T>(std::forward<Args>(args)...);
}

template <typename T>
void Destroy(T*& ptr) {
    Engine::Memory::MemoryManager::instance().destroy(ptr);
    ptr = nullptr;
}

void snapshotSnake(GameState& st) {
    st.lastSnake.clear();
    st.lastSnake.reserve(st.snake.size());
    for (auto* seg : st.snake) {
        if (seg) st.lastSnake.push_back(*seg);
    }
}

bool occupiesCell(const GameState& st, int cx, int cy) {
    for (auto* s : st.snake) {
        if (s && s->x == cx && s->y == cy) return true;
    }
    return false;
}

SDL_Point randomEmptyCell(const GameState& st) {
    std::uniform_int_distribution<int> dx(0, kGridCols - 1);
    std::uniform_int_distribution<int> dy(0, kGridRows - 1);
    for (int attempts = 0; attempts < 512; ++attempts) {
        int x = dx(rng);
        int y = dy(rng);
        if (!occupiesCell(st, x, y)) return {x, y};
    }
    return {0, 0};
}

void spawnFood(GameState& st) {
    st.food = randomEmptyCell(st);
    st.foodActive = true;
}

void resetSnake(GameState& st) {
    for (auto* seg : st.snake) Destroy(seg);
    st.snake.clear();

    const int startX = 0;
    const int startY = 1;
    for (int i = 3; i >= 0; --i) {
        st.snake.push_back(Make<Segment>(Segment{startX, startY + i}));
    }
    st.dir = Direction::Down;
    st.queued = Direction::Down;
    st.gameOver = false;
    st.paused = false;
    st.tickAccumulator = 0.0f;
    st.interpAlpha = 0.0f;
    st.score = 0;
    st.printedOutcome = false;
    st.promptActive = false;
    st.promptShown = false;
    st.speedScale = 1.0f;
    snapshotSnake(st);
    spawnFood(st);
    std::puts("[Snake] Ready. Use LEFT/RIGHT to turn. ESC to quit, Y to restart when prompted.");
}

Direction turnLeft(Direction dir) {
    switch (dir) {
        case Direction::Up: return Direction::Left;
        case Direction::Down: return Direction::Right;
        case Direction::Left: return Direction::Down;
        case Direction::Right: return Direction::Up;
    }
    return dir;
}

Direction turnRight(Direction dir) {
    switch (dir) {
        case Direction::Up: return Direction::Right;
        case Direction::Down: return Direction::Left;
        case Direction::Left: return Direction::Up;
        case Direction::Right: return Direction::Down;
    }
    return dir;
}

float stepInterval(const GameState& st) {
    float shrink = std::min(0.07f, 0.0015f * std::max(0, static_cast<int>(st.snake.size()) - 4));
    float interval = kBaseStepInterval - shrink;
    return std::max(interval, kMinStepInterval);
}

bool advance(GameState& st) {
    if (st.snake.empty()) return false;
    snapshotSnake(st);
    st.dir = st.queued;
    Segment next = *st.snake.front();
    switch (st.dir) {
        case Direction::Up:    --next.y; break;
        case Direction::Down:  ++next.y; break;
        case Direction::Left:  --next.x; break;
        case Direction::Right: ++next.x; break;
    }

    if (next.x < 0 || next.y < 0 || next.x >= kGridCols || next.y >= kGridRows) {
        st.gameOver = true;
        st.printedOutcome = false;
        return false;
    }
    if (occupiesCell(st, next.x, next.y)) {
        st.gameOver = true;
        st.printedOutcome = false;
        return false;
    }

    bool ateFood = st.foodActive && next.x == st.food.x && next.y == st.food.y;
    st.snake.insert(st.snake.begin(), Make<Segment>(Segment{next.x, next.y}));
    if (!ateFood && !st.snake.empty()) {
        auto* tail = st.snake.back();
        st.snake.pop_back();
        Destroy(tail);
    } else if (ateFood) {
        st.score += 10;
        st.foodActive = false;
        spawnFood(st);
    }
    if (st.lastSnake.size() < st.snake.size()) {
        Segment tailCopy = st.lastSnake.empty() ? Segment{next.x, next.y} : st.lastSnake.back();
        st.lastSnake.push_back(tailCopy);
    }
    return true;
}

void configureInput() {
    using namespace Engine;
    Input::map("left", SDL_SCANCODE_LEFT);
    Input::map("left", SDL_SCANCODE_A);
    Input::map("right", SDL_SCANCODE_RIGHT);
    Input::map("right", SDL_SCANCODE_D);
    Input::map("pause", SDL_SCANCODE_P);
    Input::map("restart", SDL_SCANCODE_R);
    Input::map("speed_half", SDL_SCANCODE_Z);
    Input::map("speed_one", SDL_SCANCODE_X);
    Input::map("speed_dbl", SDL_SCANCODE_C);
    Input::map("exit", SDL_SCANCODE_ESCAPE);
    Input::map("confirm", SDL_SCANCODE_Y);
}

void handleInput(GameState& st) {
    static bool leftHeld = false;
    static bool rightHeld = false;

    bool leftNow = Engine::Input::keyPressed("left");
    bool rightNow = Engine::Input::keyPressed("right");

    if (leftNow && !leftHeld) st.queued = turnLeft(st.dir);
    if (rightNow && !rightHeld) st.queued = turnRight(st.dir);

    leftHeld = leftNow;
    rightHeld = rightNow;

    if (Engine::Input::keyPressed("restart")) resetSnake(st);
    if (Engine::Input::keyPressed("exit")) Engine::stop();
}

void announceOutcome(GameState& st) {
    if (st.printedOutcome) return;
    if (st.gameOver) {
        std::printf("[Snake] Game over. Final length %zu. Press R to try again.\n", st.snake.size());
        st.printedOutcome = true;
    }
}

void gameUpdate(float dt) {
    if (!G) return;
    handleInput(*G);
    static bool pauseLatch = false;

    if (Engine::Input::keyPressed("pause")) {
        if (!pauseLatch) {
            G->paused = !G->paused;
            pauseLatch = true;
        }
    } else {
        pauseLatch = false;
    }

    if (Engine::Input::keyPressed("speed_half") && Engine::timeline) {
        G->speedScale = 0.5f; Engine::timeline->setScale(0.5); std::puts("[Snake Speed] 0.5x");
    }
    if (Engine::Input::keyPressed("speed_one") && Engine::timeline) {
        G->speedScale = 1.0f; Engine::timeline->setScale(1.0); std::puts("[Snake Speed] 1.0x");
    }
    if (Engine::Input::keyPressed("speed_dbl") && Engine::timeline) {
        G->speedScale = 2.0f; Engine::timeline->setScale(2.0); std::puts("[Snake Speed] 2.0x");
    }

    if (G->promptActive) {
        if (!G->promptShown) {
            std::puts("[Snake] Play again? Press 'Y' to restart or ESC to quit.");
            G->promptShown = true;
        }
        if (Engine::Input::keyPressed("confirm")) {
            resetSnake(*G);
        } else if (Engine::Input::keyPressed("exit")) {
            Engine::stop();
        }
        return;
    }

    if (G->paused) {
        G->interpAlpha = 0.0f;
        return;
    }

    if (G->gameOver) {
        G->interpAlpha = 0.0f;
        announceOutcome(*G);
        G->promptActive = true;
        return;
    }

    G->tickAccumulator += dt;

    float interval = stepInterval(*G);
    while (G->tickAccumulator >= interval && !G->gameOver) {
        if (!advance(*G)) break;
        G->tickAccumulator -= interval;
        interval = stepInterval(*G);
    }
    G->interpAlpha = std::clamp((interval > 0.0f) ? (G->tickAccumulator / interval) : 0.0f, 0.0f, 1.0f);
    announceOutcome(*G);
}

SDL_FRect cellRect(float cx, float cy, float originX, float originY) {
    return {originX + cx * kCellSize, originY + cy * kCellSize, kCellSize, kCellSize};
}

void drawFilledCircle(const SDL_FPoint& center, float radius, SDL_Color color) {
    if (!Engine::renderer) return;
    SDL_SetRenderDrawColor(Engine::renderer, color.r, color.g, color.b, color.a);
    const int ir = static_cast<int>(std::ceil(radius));
    for (int y = -ir; y <= ir; ++y) {
        float dx = radius * radius - float(y * y);
        if (dx < 0.0f) continue;
        float span = std::sqrt(dx);
        SDL_FRect spanRect{center.x - span, center.y + float(y), span * 2.0f, 1.0f};
        SDL_FRect scaled = Engine::Scaling::apply(spanRect);
        SDL_RenderFillRect(Engine::renderer, &scaled);
    }
}

void drawOverlay() {
    if (!Engine::renderer || !G) return;

    SDL_FRect visible = Engine::Scaling::getVisibleArea();
    const float gridW = kGridCols * kCellSize;
    const float gridH = kGridRows * kCellSize;
    const float originX = visible.x + (visible.w - gridW) * 0.5f;
    const float originY = visible.y + (visible.h - gridH) * 0.5f;

    SDL_FRect gridArea{originX - kGridBorder, originY - kGridBorder,
                       gridW + kGridBorder * 2.0f, gridH + kGridBorder * 2.0f};
    SDL_FRect scaledGrid = Engine::Scaling::apply(gridArea);
    SDL_SetRenderDrawColor(Engine::renderer, 12, 16, 26, 255);
    SDL_RenderFillRect(Engine::renderer, &scaledGrid);

    for (int c = 0; c <= kGridCols; ++c) {
        SDL_FRect line{originX + c * kCellSize, originY, 1.0f, gridH};
        SDL_FRect scaled = Engine::Scaling::apply(line);
        SDL_SetRenderDrawColor(Engine::renderer, 32, 46, 68, 255);
        SDL_RenderFillRect(Engine::renderer, &scaled);
    }
    for (int r = 0; r <= kGridRows; ++r) {
        SDL_FRect line{originX, originY + r * kCellSize, gridW, 1.0f};
        SDL_FRect scaled = Engine::Scaling::apply(line);
        SDL_SetRenderDrawColor(Engine::renderer, 32, 46, 68, 255);
        SDL_RenderFillRect(Engine::renderer, &scaled);
    }

    for (size_t i = 0; i < G->snake.size(); ++i) {
        auto* seg = G->snake[i];
        if (!seg) continue;
        Segment prev = (i < G->lastSnake.size()) ? G->lastSnake[i] : *seg;
        float alpha = G->interpAlpha;
        float cx = prev.x + (seg->x - prev.x) * alpha;
        float cy = prev.y + (seg->y - prev.y) * alpha;
        SDL_Color color = (i == 0) ? SDL_Color{30, 150, 90, 255} : SDL_Color{90, 210, 140, 255};
        SDL_FRect r = cellRect(cx, cy, originX, originY);
        SDL_FRect scaled = Engine::Scaling::apply(r);
        SDL_SetRenderDrawColor(Engine::renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(Engine::renderer, &scaled);
    }

    if (G->foodActive) {
        SDL_FPoint center{originX + (G->food.x + 0.5f) * kCellSize,
                          originY + (G->food.y + 0.5f) * kCellSize};
        drawFilledCircle(center, kCellSize * 0.35f, SDL_Color{220, 60, 60, 255});
    }

    SDL_FRect hud{originX, originY - 28.0f, gridW, 20.0f};
    SDL_FRect hudScaled = Engine::Scaling::apply(hud);
    SDL_SetRenderDrawColor(Engine::renderer, 18, 26, 38, 220);
    SDL_RenderFillRect(Engine::renderer, &hudScaled);

    if (G->promptActive) {
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
}

void buildScene() {
    Engine::setBackgroundColor(6, 8, 14);
    Engine::Scaling::setMode(Engine::Scaling::PROPORTIONAL);
    Engine::setOverlayRenderer(drawOverlay);

    static GameState state;
    state = GameState{};
    rng.seed(SDL_GetTicks());
    G = &state;

    Engine::Memory::MemoryManager::instance().configurePool<Segment>(kGridCols * kGridRows + 8);

    configureInput();
    resetSnake(state);
}

int RunSnake() {
    if (!Engine::init("Snake")) {
        std::fprintf(stderr, "Engine init failed: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    buildScene();
    return Engine::main(&gameUpdate);
}

int main() {
    return RunSnake();
}
