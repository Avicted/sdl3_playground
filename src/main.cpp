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

typedef struct Ball // @Note: Actually a rectangle
{
    glm::vec2 position;
    glm::vec2 velocity;
    float radius;
} Ball;

Ball ball = {
    .position = { 0.0f, 0.0f },
    .velocity = { 256.4f, 128.6f },
    .radius = 64.0f,
};

bool isRunning = true;
SDL_Window* window = NULL;
bool isFullscreen = false;

int windowWidth = GAME_WIDTH;
int windowHeight = GAME_HEIGHT;
int scaleX, scaleY, scale, offsetX, offsetY;

global_variable const char* BasePath = NULL;
// -------------------------------------------------------------------------------
internal void
InitializeAssetLoader()
{
    BasePath = SDL_GetBasePath();
}

internal SDL_Surface*
LoadImage(const char* imageFilename, int desiredChannels)
{
    char fullPath[256];
    SDL_Surface* result;
    SDL_PixelFormat format;

    SDL_snprintf(
      fullPath, sizeof(fullPath), "%sresources/%s", BasePath, imageFilename);

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

internal int
Init(Context* context)
{
    int result = RendererInitSDL(context, SDL_WINDOW_RESIZABLE);
    if (result < 0)
    {
        return result;
    }

    InitializeAssetLoader();

    RendererInitShaders(context);

    // Load the image
    context->Renderer.imageData = LoadImage("uv_test.bmp", 4);
    if (context->Renderer.imageData == NULL)
    {
        SDL_Log("Could not load image data!");
        return -1;
    }

    // Create the pipeline
    RendererInitPipeline(context);
    RendererCreateSamplers(context);
    RendererCreateTexture(context);

    // -- Calculate scale and offsets for initial window size
    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    scaleX = GAME_WIDTH;
    scaleY = GAME_HEIGHT;
    scale = 1;
    offsetX = 0;
    offsetY = 0;

    return 0;
}

internal void
ResizeWindow(int w, int h)
{
    // -- Calculate scale to fit the 16:9 resolution
    scaleX = w / GAME_WIDTH;
    scaleY = h / GAME_HEIGHT;
    scale = std::min(scaleX, scaleY);

    // -- Calculate offsets to center the game content in the window
    offsetX = (w - GAME_WIDTH * scale) / 2;
    offsetY = (h - GAME_HEIGHT * scale) / 2;

    // @Note(Victor): Anything else?
}

internal int
Input(Context* context)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT)
        {
            isRunning = false;
        }

        if (event.type == SDL_EVENT_WINDOW_RESIZED)
        {
            windowWidth = event.window.data1;
            windowHeight = event.window.data2;
            ResizeWindow(windowWidth, windowHeight);
        }

        if (event.type == SDL_EVENT_KEY_DOWN)
        {
            if (event.key.key == SDLK_F11)
            {
                isFullscreen = !isFullscreen;
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
UpdateBall(float deltaTime)
{
    // Update the ball's position
    ball.position.x += ball.velocity.x * deltaTime;
    ball.position.y += ball.velocity.y * deltaTime;

    // Check for collisions with the left and right edges of the window
    if (ball.position.x - ball.radius < 0.0f) // Ball hits left edge
    {
        ball.position.x =
          ball.radius; // Prevent the ball from going out of bounds
        ball.velocity.x = -ball.velocity.x; // Reverse the horizontal velocity
    }
    else if (ball.position.x + ball.radius >
             windowWidth) // Ball hits right edge
    {
        ball.position.x =
          windowWidth -
          ball.radius; // Prevent the ball from going out of bounds
        ball.velocity.x = -ball.velocity.x; // Reverse the horizontal velocity
    }

    // Check for collisions with the top and bottom edges of the window
    if (ball.position.y - ball.radius < 0.0f) // Ball hits top edge
    {
        ball.position.y =
          ball.radius; // Prevent the ball from going out of bounds
        ball.velocity.y = -ball.velocity.y; // Reverse the vertical velocity
    }
    else if (ball.position.y + ball.radius >
             windowHeight) // Ball hits bottom edge
    {
        ball.position.y =
          windowHeight -
          ball.radius; // Prevent the ball from going out of bounds
        ball.velocity.y = -ball.velocity.y; // Reverse the vertical velocity
    }
}

internal void
Update(float deltaTime)
{
    UpdateBall(deltaTime);
}

internal int
Render(Context* context)
{
    SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(context->Device);
    if (cmdbuf == NULL)
    {
        SDL_Log("AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return -1;
    }

    SDL_GPUTexture* swapchainTexture;
    if (!SDL_AcquireGPUSwapchainTexture(
          cmdbuf, context->Window, &swapchainTexture, NULL, NULL))
    {
        SDL_Log("AcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        return -1;
    }

    if (swapchainTexture != NULL)
    {

        SDL_GPUColorTargetInfo colorTargetInfo = { 0 };
        colorTargetInfo.texture = swapchainTexture;
        colorTargetInfo.clear_color = (SDL_FColor){ 0.05f, 0.0f, 0.05f, 1.0f };
        colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

        // Update the texture coordinates of the texture quad to the ball's
        // position
        {
            PositionTextureVertex transferData[4];

            // Calculate normalized position of the ball (center of the ball)
            float left =
              (ball.position.x - ball.radius) / (float)GAME_WIDTH * 2.0f - 1.0f;
            float right =
              (ball.position.x + ball.radius) / (float)GAME_WIDTH * 2.0f - 1.0f;
            float top =
              (ball.position.y - ball.radius) / (float)GAME_HEIGHT * 2.0f -
              1.0f;
            float bottom =
              (ball.position.y + ball.radius) / (float)GAME_HEIGHT * 2.0f -
              1.0f;

            // Update the vertices to form a rectangle with the ball's position
            // and radius
            transferData[0] =
              (PositionTextureVertex){ left, top, 0, 0, 0 }; // Top-left corner
            transferData[1] = (PositionTextureVertex){
                right, top, 0, 1, 0
            }; // Top-right corner
            transferData[2] = (PositionTextureVertex){
                right, bottom, 0, 1, 1
            }; // Bottom-right corner
            transferData[3] = (PositionTextureVertex){
                left, bottom, 0, 0, 1
            }; // Bottom-left corner

            SDL_GPUTransferBufferCreateInfo bufferTransferBufferCreateInfo = {
                .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                .size = sizeof(PositionTextureVertex) * 4
            };
            SDL_GPUTransferBuffer* bufferTransferBuffer =
              SDL_CreateGPUTransferBuffer(context->Device,
                                          &bufferTransferBufferCreateInfo);

            PositionTextureVertex* transferDataPtr =
              static_cast<PositionTextureVertex*>(SDL_MapGPUTransferBuffer(
                context->Device, bufferTransferBuffer, false));

            SDL_memcpy(transferDataPtr, transferData, sizeof(transferData));
            SDL_GPUTransferBufferLocation vertexBufferLocation = {
                .transfer_buffer = bufferTransferBuffer, .offset = 0
            };
            SDL_GPUBufferRegion vertexBufferRegion = {
                .buffer = context->Renderer.VertexBuffer,
                .offset = 0,
                .size = sizeof(PositionTextureVertex) * 4
            };

            // Create a copy pass
            SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);

            // Upload the updated vertex data to the GPU
            SDL_UploadToGPUBuffer(
              copyPass, &vertexBufferLocation, &vertexBufferRegion, false);

            SDL_UnmapGPUTransferBuffer(context->Device, bufferTransferBuffer);
            SDL_EndGPUCopyPass(copyPass);
        }

        SDL_GPURenderPass* renderPass =
          SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, NULL);

        SDL_BindGPUGraphicsPipeline(renderPass, context->Renderer.Pipeline);

        SDL_GPUBufferBinding vertexBufferBinding = {
            .buffer = context->Renderer.VertexBuffer,
            .offset = 0,
        };
        SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBufferBinding, 1);

        SDL_GPUBufferBinding indexBufferBinding = {
            .buffer = context->Renderer.IndexBuffer,
            .offset = 0,
        };
        SDL_BindGPUIndexBuffer(
          renderPass, &indexBufferBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        SDL_GPUTextureSamplerBinding textureSamplerBinding = {
            .texture = context->Renderer.Texture,
            .sampler =
              context->Renderer.Samplers[context->Renderer.CurrentSamplerIndex]
        };
        SDL_BindGPUFragmentSamplers(renderPass, 0, &textureSamplerBinding, 1);

        SDL_DrawGPUIndexedPrimitives(renderPass, 6, 1, 0, 0, 0);

        SDL_EndGPURenderPass(renderPass);
    }

    SDL_SubmitGPUCommandBuffer(cmdbuf);

    return 0;
}

internal void
Cleanup(Context* context)
{
    SDL_ReleaseGPUGraphicsPipeline(context->Device, context->Renderer.Pipeline);
    SDL_ReleaseGPUBuffer(context->Device, context->Renderer.VertexBuffer);
    SDL_ReleaseGPUBuffer(context->Device, context->Renderer.IndexBuffer);
    SDL_ReleaseGPUTexture(context->Device, context->Renderer.Texture);

    for (long unsigned int i = 0; i < SDL_arraysize(context->Renderer.Samplers);
         i += 1)
    {
        SDL_ReleaseGPUSampler(context->Device, context->Renderer.Samplers[i]);
    }

    context->Renderer.CurrentSamplerIndex = 0;

    SDL_ReleaseWindowFromGPUDevice(context->Device, context->Window);
    SDL_DestroyWindow(context->Window);
    SDL_DestroyGPUDevice(context->Device);
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

    int initSuccess = Init(context);
    if (initSuccess > 0)
    {
        return initSuccess;
    }

    Uint64 lastTime = SDL_GetPerformanceCounter();

    // Game loop
    while (isRunning)
    {
        Uint64 currentTime = SDL_GetPerformanceCounter();
        const float deltaTime =
          (currentTime - lastTime) /
          static_cast<float>(SDL_GetPerformanceFrequency());
        lastTime = currentTime;

        Input(context);
        Update(deltaTime);
        Render(context);
    }

    Cleanup(context);
    free(context);

    return 0;
}
