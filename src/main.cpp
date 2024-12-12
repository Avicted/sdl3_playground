#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec2.hpp>

#include <box2d/box2d.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC SDL_malloc
#define STBI_REALLOC SDL_realloc
#define STBI_FREE SDL_free
#include <stb_image.h>

// Our code
#include "context.hpp"
#include "includes.hpp"

// -------------------------------------------------------------------------------
internal void
InitializeAssetLoader(Context* context)
{
    context->BasePath = SDL_GetBasePath();
}

internal int
Init(Context* context)
{
    int result = RendererInitSDL(context, SDL_WINDOW_RESIZABLE);
    if (result < 0)
    {
        return result;
    }

    InitializeAssetLoader(context);

    RendererInitShaders(context);

    int w, h;
    SDL_GetWindowSize(context->Renderer.Window, &w, &h);
    RendererResizeWindow(context, GAME_WIDTH, GAME_HEIGHT);

    printf("Window size: %d x %d\n", w, h);
    printf("Scale: %d x %d\n", context->scaleX, context->scaleY);
    printf("Offset: %d x %d\n", context->offsetX, context->offsetY);

    context->Renderer.imageData = RendererLoadImage(context, "uv_test.bmp", 4);
    if (context->Renderer.imageData == NULL)
    {
        SDL_Log("Could not load image data!");
        return -1;
    }

    RendererInitPipeline(context);
    RendererCreateSamplers(context);
    RendererCreateTexture(context);
    context->Renderer.isInitialized = true;

    return 0;
}

internal int
Input(Context* context)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT)
        {
            context->isRunning = false;
        }

        if (event.type == SDL_EVENT_WINDOW_RESIZED)
        {
            context->windowWidth = event.window.data1;
            context->windowHeight = event.window.data2;
            RendererResizeWindow(
              context, context->windowWidth, context->windowHeight);
        }

        if (event.type == SDL_EVENT_KEY_DOWN)
        {
            if (event.key.key == SDLK_F11)
            {
                context->isFullscreen = !context->isFullscreen;
            }
        }

        // User keyboard input
        if (event.type == SDL_EVENT_KEY_DOWN)
        {
            if (event.key.key == SDLK_LEFT)
            {
                context->Renderer.CurrentSamplerIndex -= 1;
                if (context->Renderer.CurrentSamplerIndex < 0)
                {
                    context->Renderer.CurrentSamplerIndex =
                      SDL_arraysize(context->Renderer.Samplers) - 1;
                }
                SDL_Log("Setting sampler state to: %s",
                        SamplerNames[context->Renderer.CurrentSamplerIndex]);
            }
            if (event.key.key == SDLK_RIGHT)
            {
                context->Renderer.CurrentSamplerIndex =
                  (context->Renderer.CurrentSamplerIndex + 1) %
                  SDL_arraysize(context->Renderer.Samplers);
                SDL_Log("Setting sampler state to: %s",
                        SamplerNames[context->Renderer.CurrentSamplerIndex]);
            }
        }
    }

    return 0;
}

internal void
UpdateBall(float deltaTime, Context* context)
{
    // Update the ball's position
    context->ball.position.x += context->ball.velocity.x * deltaTime;
    context->ball.position.y += context->ball.velocity.y * deltaTime;

    // Check for collisions with the left and right edges using game dimensions
    if (context->ball.position.x - context->ball.radius <
        0.0f) // Ball hits left edge
    {
        context->ball.position.x =
          context->ball.radius; // Prevent the ball from going out of bounds
        context->ball.velocity.x =
          -context->ball.velocity.x; // Reverse the horizontal velocity
    }
    else if (context->ball.position.x + context->ball.radius >
             GAME_WIDTH) // Ball hits right edge
    {
        context->ball.position.x =
          GAME_WIDTH -
          context->ball.radius; // Prevent the ball from going out of bounds
        context->ball.velocity.x =
          -context->ball.velocity.x; // Reverse the horizontal velocity
    }

    // Check for collisions with the top and bottom edges using game dimensions
    if (context->ball.position.y - context->ball.radius <
        0.0f) // Ball hits top edge
    {
        context->ball.position.y =
          context->ball.radius; // Prevent the ball from going out of bounds
        context->ball.velocity.y =
          -context->ball.velocity.y; // Reverse the vertical velocity
    }
    else if (context->ball.position.y + context->ball.radius >
             GAME_HEIGHT) // Ball hits bottom edge
    {
        context->ball.position.y =
          GAME_HEIGHT -
          context->ball.radius; // Prevent the ball from going out of bounds
        context->ball.velocity.y =
          -context->ball.velocity.y; // Reverse the vertical velocity
    }
}

internal void
Update(float deltaTime, Context* context)
{
    UpdateBall(deltaTime, context);
}

internal int
Render(Context* context)
{
    return RendererRenderFrame(context);
}

int
main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    Context* context = (Context*)calloc(1, sizeof(Context));
    context->GameName = "SDL2 Playground";
    context->BasePath = SDL_GetBasePath();
    context->DeltaTime = 0.0f;
    context->ball.position = glm::vec2(320.0f, 180.0f);
    context->ball.velocity = glm::vec2(100.0f, 150.0f);
    context->ball.radius = 64.0f;
    context->windowWidth = GAME_WIDTH;
    context->windowHeight = GAME_HEIGHT;
    context->Renderer.isInitialized = false;
    context->Renderer = { 0 };

    int initSuccess = Init(context);
    if (initSuccess > 0)
    {
        return initSuccess;
    }

    Uint64 lastTime = SDL_GetPerformanceCounter();
    context->isRunning = true;

    // Game loop
    while (context->isRunning)
    {
        Uint64 currentTime = SDL_GetPerformanceCounter();
        const float deltaTime =
          (currentTime - lastTime) /
          static_cast<float>(SDL_GetPerformanceFrequency());
        lastTime = currentTime;

        Input(context);
        Update(deltaTime, context);
        Render(context);
    }

    RendererDestroy(context);

    return 0;
}
