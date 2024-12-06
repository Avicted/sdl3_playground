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
    .velocity = {0.6 * 3, 0.4 * 3},
    .size = {32, 32},
};

bool isRunning = true;
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *renderTexture = NULL;
const int windowWidth = 640;
const int windowHeight = 360;

static int
Init(void)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL_Init failed (%s)", SDL_GetError());
        return 1;
    }

    SDL_WindowFlags windowFlags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    if (!SDL_CreateWindowAndRenderer("SDL Playground", windowWidth, windowHeight, windowFlags, &window, &renderer))
    {
        SDL_Log("SDL_CreateWindowAndRenderer failed (%s)", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    renderTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, windowWidth, windowHeight);

    return 0;
}

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

    if (ball.position.x - ball.size.x < 0 || ball.position.x + ball.size.x > windowWidth)
    {
        ball.velocity.x = -ball.velocity.x;
    }

    if (ball.position.y - ball.size.y < 0 || ball.position.y + ball.size.y > windowHeight)
    {
        ball.velocity.y = -ball.velocity.y;
    }

    if (ball.position.x - ball.size.x < 0)
    {
        ball.position.x = ball.size.x;
    }
    else if (ball.position.x + ball.size.x > windowWidth)
    {
        ball.position.x = windowWidth - ball.size.x;
    }

    if (ball.position.y - ball.size.y < 0)
    {
        ball.position.y = ball.size.y;
    }
    else if (ball.position.y + ball.size.y > windowHeight)
    {
        ball.position.y = windowHeight - ball.size.y;
    }
}

static void
Render(SDL_Renderer *renderer)
{
    // Set render target to texture
    SDL_SetRenderTarget(renderer, renderTexture);

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

    // Reset render target to default
    SDL_SetRenderTarget(renderer, NULL);

    // Clear the default render target
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    // Copy texture to renderer, preserving aspect ratio
    int currentWindowWidth, currentWindowHeight;
    SDL_GetWindowSize(window, &currentWindowWidth, &currentWindowHeight);

    const float aspectRatio = (float)windowWidth / (float)windowHeight;
    int newWidth = currentWindowWidth;
    int newHeight = (int)(currentWindowWidth / aspectRatio);

    if (newHeight > currentWindowHeight)
    {
        newHeight = currentWindowHeight;
        newWidth = (int)(currentWindowHeight * aspectRatio);
    }

    SDL_FRect dstRect = {
        .x = (currentWindowWidth - newWidth) / 2,
        .y = (currentWindowHeight - newHeight) / 2,
        .w = newWidth,
        .h = newHeight,
    };

    SDL_RenderTexture(renderer, renderTexture, NULL, &dstRect);

    SDL_RenderPresent(renderer);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    int initSuccess = Init();
    if (initSuccess > 0)
    {
        return initSuccess;
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
    SDL_DestroyTexture(renderTexture);

    SDL_Quit();
    return 0;
}
