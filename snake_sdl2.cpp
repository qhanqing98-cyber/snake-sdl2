#include <SDL.h>
#include <iostream>
#include <vector>
#include <deque>
#include <random>
#include <chrono>
#include <cstdio>

// Simple SDL2 Snake (graphical) replacing the console ASCII version.
// Controls: Arrow keys. Esc to quit.

static constexpr int GRID_W = 22;
static constexpr int GRID_H = 22;
static constexpr int CELL = 20; // pixel size per cell

struct Point {
    int x;
    int y;
};

static bool in_bounds(int x, int y) {
    return x >= 1 && x <= GRID_W - 2 && y >= 1 && y <= GRID_H - 2;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Snake (SDL2)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        GRID_W * CELL, GRID_H * CELL,
        SDL_WINDOW_SHOWN);

    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Colors
    const SDL_Color c_bg   = { 0, 0, 0, 255 };
    const SDL_Color c_wall = { 70, 70, 70, 255 };
    const SDL_Color c_food = { 255, 170, 0, 255 };
    const SDL_Color c_head = { 0, 220, 0, 255 };
    const SDL_Color c_body = { 0, 150, 0, 255 };

    auto set_cell = [&](int gx, int gy, const SDL_Color& col) {
        SDL_Rect r{ gx * CELL, gy * CELL, CELL, CELL };
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
        SDL_RenderFillRect(renderer, &r);
    };

    std::mt19937 rng((unsigned)std::chrono::high_resolution_clock::now().time_since_epoch().count());

    std::deque<Point> snake;
    // start similar to original: tail at y=2, head at x=1,y=4 in grid terms; but we'll just start centered.
    // Coordinates in grid: 0..21, walls at borders.
    snake.push_back({1, 2});
    snake.push_back({2, 2});
    snake.push_back({3, 2});
    snake.push_back({4, 2});

    Point dir{1, 0}; // moving right
    bool alive = true;

    int grade = 1;
    int score = 0;
    int length = (int)snake.size();
    int gamespeed_ms = 500; // start

    Point food;
    {
        // place food not on snake
        while (true) {
            std::uniform_int_distribution<int> dx(1, GRID_W - 2);
            std::uniform_int_distribution<int> dy(1, GRID_H - 2);
            food = Point{dx(rng), dy(rng)};
            bool onSnake = false;
            for (auto& p : snake) if (p.x == food.x && p.y == food.y) { onSnake = true; break; }
            if (!onSnake) break;
        }
    }

    auto last_tick = std::chrono::steady_clock::now();

    while (alive) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                alive = false;
            }
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) alive = false;
                // prevent reversing direction
                if (e.key.keysym.sym == SDLK_UP) {
                    if (!(dir.x == 0 && dir.y == 1)) dir = {0, -1};
                } else if (e.key.keysym.sym == SDLK_DOWN) {
                    if (!(dir.x == 0 && dir.y == -1)) dir = {0, 1};
                } else if (e.key.keysym.sym == SDLK_LEFT) {
                    if (!(dir.x == 1 && dir.y == 0)) dir = {-1, 0};
                } else if (e.key.keysym.sym == SDLK_RIGHT) {
                    if (!(dir.x == -1 && dir.y == 0)) dir = {1, 0};
                }
            }
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tick).count();
        if (elapsed_ms >= gamespeed_ms) {
            last_tick = now;

            Point head = snake.back();
            Point next{ head.x + dir.x, head.y + dir.y };

            // collision with walls
            if (!in_bounds(next.x, next.y)) {
                alive = false;
            } else {
                // collision with body: allow moving into tail only if tail moves away (handled by food check)
                bool will_eat = (next.x == food.x && next.y == food.y);

                if (!will_eat) {
                    // tail will be removed
                    Point tail = snake.front();
                    bool hit = false;
                    for (size_t i = 0; i < snake.size(); i++) {
                        if (snake[i].x == next.x && snake[i].y == next.y) {
                            if (!(snake[i].x == tail.x && snake[i].y == tail.y)) hit = true;
                        }
                    }
                    if (hit) alive = false;
                } else {
                    // if eating, tail won't move away; any overlap is collision
                    for (auto& p : snake) {
                        if (p.x == next.x && p.y == next.y) { alive = false; break; }
                    }
                }

                if (alive) {
                    snake.push_back(next);
                    if (will_eat) {
                        length++;
                        score += 100;

                        if (length >= 8) {
                            length -= 8;
                            grade++;
                            if (gamespeed_ms >= 200) gamespeed_ms = 550 - grade * 50;
                        }

                        // spawn new food not on snake
                        while (true) {
                            std::uniform_int_distribution<int> dx(1, GRID_W - 2);
                            std::uniform_int_distribution<int> dy(1, GRID_H - 2);
                            food = Point{dx(rng), dy(rng)};
                            bool onSnake = false;
                            for (auto& p : snake) if (p.x == food.x && p.y == food.y) { onSnake = true; break; }
                            if (!onSnake) break;
                        }
                    } else {
                        // move forward: remove tail
                        snake.pop_front();
                    }
                }
            }
        }

        // render
        SDL_SetRenderDrawColor(renderer, c_bg.r, c_bg.g, c_bg.b, c_bg.a);
        SDL_RenderClear(renderer);

        // walls
        for (int x = 0; x < GRID_W; x++) {
            set_cell(x, 0, c_wall);
            set_cell(x, GRID_H - 1, c_wall);
        }
        for (int y = 0; y < GRID_H; y++) {
            set_cell(0, y, c_wall);
            set_cell(GRID_W - 1, y, c_wall);
        }

        // food
        set_cell(food.x, food.y, c_food);

        // snake
        for (size_t i = 0; i < snake.size(); i++) {
            const auto& p = snake[i];
            if (i + 1 == snake.size()) set_cell(p.x, p.y, c_head);
            else set_cell(p.x, p.y, c_body);
        }

        SDL_RenderPresent(renderer);
        // Update window title with score info
        char title_buf[64];
        snprintf(title_buf, sizeof(title_buf), "Snake - Score: %d  Grade: %d", score, grade);
        SDL_SetWindowTitle(window, title_buf);
    }

    // Game over screen: red head + dark overlay
    {
        if (!snake.empty()) {
            const auto& p = snake.back();
            SDL_Color dead = { 200, 0, 0, 255 };
            set_cell(p.x, p.y, dead);
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_Rect bar = { 0, (GRID_H/2 - 1) * CELL, GRID_W * CELL, 3 * CELL };
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
        SDL_RenderFillRect(renderer, &bar);
        SDL_RenderPresent(renderer);
        char buf[128];
        snprintf(buf, sizeof(buf), "Game Over!  Score: %d  Grade: %d", score, grade);
        SDL_SetWindowTitle(window, buf);
    }
    SDL_Delay(3000);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

