#include <SDL.h>
#include <iostream>
#include <deque>
#include <random>
#include <chrono>
#include <cstdio>

static constexpr int GRID_W = 22;
static constexpr int GRID_H = 22;
static constexpr int CELL  = 20;

static constexpr int TICK_INITIAL  = 500;
static constexpr int TICK_MIN      = 80;
static constexpr int TICK_PER_FOOD = 5;
static constexpr int GRADE_EVERY   = 8;
static constexpr int SCORE_PER_FOOD = 100;

struct Point { int x, y; };

static bool in_bounds(int x, int y) {
    return x >= 1 && x <= GRID_W - 2 && y >= 1 && y <= GRID_H - 2;
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Snake", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        GRID_W * CELL, GRID_H * CELL, SDL_WINDOW_SHOWN);
    if (!window) { std::cerr << "SDL_CreateWindow failed\n"; SDL_Quit(); return 1; }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed\n";
        SDL_DestroyWindow(window); SDL_Quit(); return 1;
    }

    const SDL_Color c_bg    = {  0,   0,   0, 255 };
    const SDL_Color c_wall  = { 70,  70,  70, 255 };
    const SDL_Color c_food  = {255, 170,   0, 255 };
    const SDL_Color c_head  = {  0, 220,   0, 255 };
    const SDL_Color c_body  = {  0, 150,   0, 255 };
    const SDL_Color c_pause = {255, 255, 255, 200 };

    auto set_cell = [&](int gx, int gy, const SDL_Color& col) {
        SDL_Rect r{ gx * CELL, gy * CELL, CELL, CELL };
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
        SDL_RenderFillRect(renderer, &r);
    };

    auto draw_rect = [&](int x, int y, int w, int h, const SDL_Color& col) {
        SDL_Rect r{ x, y, w, h };
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
        SDL_RenderFillRect(renderer, &r);
    };

    std::mt19937 rng((unsigned)std::chrono::high_resolution_clock::now().time_since_epoch().count());

    auto spawn_food = [&](const std::deque<Point>& snake) -> Point {
        int free_cells = (GRID_W - 2) * (GRID_H - 2) - (int)snake.size();
        if (free_cells <= 0) return { -1, -1 };
        while (true) {
            std::uniform_int_distribution<int> dx(1, GRID_W - 2);
            std::uniform_int_distribution<int> dy(1, GRID_H - 2);
            Point p{ dx(rng), dy(rng) };
            bool on_snake = false;
            for (auto& s : snake) if (s.x == p.x && s.y == p.y) { on_snake = true; break; }
            if (!on_snake) return p;
        }
    };

    bool quit = false;
    while (!quit) {
        std::deque<Point> snake = { {1,2}, {2,2}, {3,2}, {4,2} };
        Point dir{ 1, 0 };
        bool alive = true;
        bool paused = false;
        int grade = 1;
        int score = 0;
        int tick_ms = TICK_INITIAL;
        int total_eaten = 0;
        Point food = spawn_food(snake);
        int eaten_since_grade = 4;
        auto last_tick = std::chrono::steady_clock::now();

        while (alive) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) { alive = false; quit = true; }
                if (e.type == SDL_KEYDOWN) {
                    switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: alive = false; quit = true; break;
                    case SDLK_SPACE:  paused = !paused; break;
                    default: break;
                    }
                    if (!paused) {
                        switch (e.key.keysym.sym) {
                        case SDLK_UP:    if (!(dir.x == 0 && dir.y ==  1)) dir = { 0, -1 }; break;
                        case SDLK_DOWN:  if (!(dir.x == 0 && dir.y == -1)) dir = { 0,  1 }; break;
                        case SDLK_LEFT:  if (!(dir.x == 1 && dir.y ==  0)) dir = {-1,  0 }; break;
                        case SDLK_RIGHT: if (!(dir.x ==-1 && dir.y ==  0)) dir = { 1,  0 }; break;
                        default: break;
                        }
                    }
                }
            }

            // --- keyboard state for boost ---
            const Uint8* keys = SDL_GetKeyboardState(NULL);
            bool boosting = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
            int effective_tick = boosting ? tick_ms / 3 : tick_ms;

            // --- game update ---
            if (!paused) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tick).count();
                if (elapsed >= effective_tick) {
                    last_tick = now;

                    Point head = snake.back();
                    Point next{ head.x + dir.x, head.y + dir.y };

                    if (!in_bounds(next.x, next.y)) { alive = false; }
                    else {
                        bool will_eat = (next.x == food.x && next.y == food.y);

                        if (!will_eat) {
                            Point tail = snake.front();
                            for (auto& p : snake)
                                if (p.x == next.x && p.y == next.y && !(p.x == tail.x && p.y == tail.y))
                                    { alive = false; break; }
                        } else {
                            for (auto& p : snake)
                                if (p.x == next.x && p.y == next.y) { alive = false; break; }
                        }

                        if (alive) {
                            snake.push_back(next);
                            if (will_eat) {
                                score += SCORE_PER_FOOD;
                                total_eaten++;
                                eaten_since_grade++;
                                if (eaten_since_grade >= GRADE_EVERY) {
                                    eaten_since_grade -= GRADE_EVERY;
                                    grade++;
                                }
                                int calc = TICK_INITIAL - total_eaten * TICK_PER_FOOD;
                                tick_ms = (calc < TICK_MIN) ? TICK_MIN : calc;
                                food = spawn_food(snake);
                                if (food.x == -1) alive = false;
                            } else {
                                snake.pop_front();
                            }
                        }
                    }
                }
            }

            // --- render ---
            SDL_SetRenderDrawColor(renderer, c_bg.r, c_bg.g, c_bg.b, c_bg.a);
            SDL_RenderClear(renderer);

            for (int x = 0; x < GRID_W; x++) { set_cell(x, 0, c_wall); set_cell(x, GRID_H - 1, c_wall); }
            for (int y = 0; y < GRID_H; y++) { set_cell(0, y, c_wall); set_cell(GRID_W - 1, y, c_wall); }

            if (food.x >= 0) set_cell(food.x, food.y, c_food);

            for (size_t i = 0; i < snake.size(); i++)
                set_cell(snake[i].x, snake[i].y, (i + 1 == snake.size()) ? c_head : c_body);

            if (paused) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                int cx = GRID_W * CELL / 2, cy = GRID_H * CELL / 2;
                draw_rect(cx - 11, cy - 16, 6, 32, c_pause);
                draw_rect(cx +  5, cy - 16, 6, 32, c_pause);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            }

            SDL_RenderPresent(renderer);

            char title[96];
            snprintf(title, sizeof(title),
                     "Snake - Score: %d  Grade: %d  %dms%s%s",
                     score, grade, effective_tick,
                     paused ? "  [PAUSED]" : "",
                     boosting ? "  BOOST" : "");
            SDL_SetWindowTitle(window, title);
        }

        // --- game over ---
        {
            if (!snake.empty())
                set_cell(snake.back().x, snake.back().y, SDL_Color{ 200, 0, 0, 255 });

            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_Rect bar{ 0, (GRID_H/2 - 1) * CELL, GRID_W * CELL, 3 * CELL };
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
            SDL_RenderFillRect(renderer, &bar);
            SDL_RenderPresent(renderer);

            char buf[128];
            if (food.x == -1)
                snprintf(buf, sizeof(buf), "You Win!  Score: %d  Grade: %d", score, grade);
            else
                snprintf(buf, sizeof(buf), "Game Over!  Score: %d  Grade: %d", score, grade);
            SDL_SetWindowTitle(window, buf);

            bool waiting = true;
            while (waiting) {
                SDL_Event e;
                while (SDL_PollEvent(&e)) {
                    if (e.type == SDL_QUIT) { waiting = false; quit = true; }
                    if (e.type == SDL_KEYDOWN) {
                        if (e.key.keysym.sym == SDLK_r) waiting = false;
                        if (e.key.keysym.sym == SDLK_ESCAPE) { waiting = false; quit = true; }
                    }
                }
                SDL_Delay(16);
            }
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
