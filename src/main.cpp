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

internal SDL_Surface*
LoadImage(Context* context, const char* imageFilename, int desiredChannels)
{
    char fullPath[256];
    SDL_Surface* result;
    SDL_PixelFormat format;

    SDL_snprintf(fullPath,
                 sizeof(fullPath),
                 "%sresources/%s",
                 context->BasePath,
                 imageFilename);

    result = SDL_LoadBMP(fullPath);
    if (result == NULL)
    {
        SDL_Log("Failed to load BMP: %s", SDL_GetError());
        return NULL;
    }

    if (desiredChannels == 4)
    {
        format = SDL_PIXELFORMAT_ABGR8888;
    }
    else
    {
        SDL_assert(!"Unexpected desiredChannels");
        SDL_DestroySurface(result);
        return NULL;
    }
    if (result->format != format)
    {
        SDL_Surface* next = SDL_ConvertSurface(result, format);
        SDL_DestroySurface(result);
        result = next;
    }

    return result;
}

internal void
ResizeWindow(Context* context, int w, int h)
{
    // -- Calculate scale to fit the 16:9 resolution
    context->scaleX = w / GAME_WIDTH;
    context->scaleY = h / GAME_HEIGHT;
    context->scale = std::min(context->scaleX, context->scaleY);

    // -- Calculate offsets to center the game content in the window
    context->offsetX = (w - GAME_WIDTH * context->scale) / 2;
    context->offsetY = (h - GAME_HEIGHT * context->scale) / 2;

    printf("Scale: %d x %d\n", context->scaleX, context->scaleY);
    printf("Offset: %d x %d\n", context->offsetX, context->offsetY);
    printf("Window size: %d x %d\n", w, h);
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

    // -- Calculate scale and offsets for initial window size
    int w, h;
    SDL_GetWindowSize(context->Window, &w, &h);
    ResizeWindow(context, GAME_WIDTH, GAME_HEIGHT);

    printf("Window size: %d x %d\n", w, h);
    printf("Scale: %d x %d\n", context->scaleX, context->scaleY);
    printf("Offset: %d x %d\n", context->offsetX, context->offsetY);

    // Load the image
    context->Renderer.imageData = LoadImage(context, "uv_test.bmp", 4);
    if (context->Renderer.imageData == NULL)
    {
        SDL_Log("Could not load image data!");
        return -1;
    }

    // Create the pipeline
    RendererInitPipeline(context);
    RendererCreateSamplers(context);
    RendererCreateTexture(context);

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
            ResizeWindow(context, context->windowWidth, context->windowHeight);
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

internal void
Cleanup(Context* context)
{
    SDL_ReleaseGPUGraphicsPipeline(context->Device, context->Renderer.Pipeline);
    SDL_ReleaseGPUBuffer(context->Device, context->Renderer.VertexBuffer);
    SDL_ReleaseGPUBuffer(context->Device, context->Renderer.IndexBuffer);
    SDL_ReleaseGPUTexture(context->Device, context->Renderer.Texture);
    SDL_ReleaseGPUTransferBuffer(context->Device,
                                 context->Renderer.textureTransferBuffer);

    for (long unsigned int i = 0; i < SDL_arraysize(context->Renderer.Samplers);
         i += 1)
    {
        SDL_ReleaseGPUSampler(context->Device, context->Renderer.Samplers[i]);
    }

    context->Renderer.CurrentSamplerIndex = 0;

    SDL_ReleaseWindowFromGPUDevice(context->Device, context->Window);
    SDL_DestroyWindow(context->Window);
    SDL_DestroyGPUDevice(context->Device);

    free(context);
}

int
main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    Context* context = (Context*)calloc(1, sizeof(Context));
    context->GameName = "SDL2 Playground";
    context->BasePath = SDL_GetBasePath();
    context->Window = NULL;
    context->Device = NULL;
    context->DeltaTime = 0.0f;
    context->ball.position = glm::vec2(320.0f, 180.0f);
    context->ball.velocity = glm::vec2(400.0f, 400.0f);
    context->ball.radius = 32.0f;
    context->isRunning = true;

    context->windowWidth = GAME_WIDTH;
    context->windowHeight = GAME_HEIGHT;

    int initSuccess = Init(context);
    if (initSuccess > 0)
    {
        return initSuccess;
    }

    Uint64 lastTime = SDL_GetPerformanceCounter();

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

    Cleanup(context);

    return 0;
}
