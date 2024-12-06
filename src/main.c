#include <SDL3/SDL.h>

typedef struct Vec2
{
    float x;
    float y;
} Vec2;

typedef struct Ball // @Note: Actually a rectangle
{
    Vec2 position;
    Vec2 velocity;
    Vec2 size;
} Ball;

Ball ball = {
    .position = {320, 180},
    .velocity = {0.6, 0.4},
    .size = {32, 32},
};

bool isRunning = true;

static void
Input(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT)
        {
            isRunning = false;
        }
    }
}

static void
Update(float deltaTime)
{
    // Update ball, bounce off walls
    ball.position.x += ball.velocity.x * deltaTime;
    ball.position.y += ball.velocity.y * deltaTime;

    if (ball.position.x - ball.size.x < 0 || ball.position.x + ball.size.x > 640)
    {
        ball.velocity.x = -ball.velocity.x;
    }

    if (ball.position.y - ball.size.y < 0 || ball.position.y + ball.size.y > 360)
    {
        ball.velocity.y = -ball.velocity.y;
    }

    if (ball.position.x - ball.size.x < 0)
    {
        ball.position.x = ball.size.x;
    }
    else if (ball.position.x + ball.size.x > 640)
    {
        ball.position.x = 640 - ball.size.x;
    }

    if (ball.position.y - ball.size.y < 0)
    {
        ball.position.y = ball.size.y;
    }
    else if (ball.position.y + ball.size.y > 360)
    {
        ball.position.y = 360 - ball.size.y;
    }
}

static void
Render(SDL_Renderer *renderer)
{
    // Render background
    SDL_SetRenderDrawColor(renderer, 40, 40, 80, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    // Render ball
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
    SDL_FRect rect = {
        .x = ball.position.x - ball.size.x,
        .y = ball.position.y - ball.size.y,
        .w = ball.size.x * 2,
        .h = ball.size.y * 2,
    };

    SDL_RenderFillRect(renderer, &rect);

    SDL_RenderPresent(renderer);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL_Init failed (%s)", SDL_GetError());
        return 1;
    }

    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;

    if (!SDL_CreateWindowAndRenderer("SDL Playground", 640, 360, 0, &window, &renderer))
    {
        SDL_Log("SDL_CreateWindowAndRenderer failed (%s)", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    while (isRunning)
    {
        const float deltaTime = 1.0f / 60.0f;

        Input();
        Update(deltaTime);
        Render(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
    return 0;
}
