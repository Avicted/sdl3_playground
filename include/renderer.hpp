#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

// Forward declaration
struct Context;

const int GAME_WIDTH = 640;
const int GAME_HEIGHT = 360;

extern const char* SamplerNames[];
constexpr size_t NumSamplers = 6;

typedef struct PositionTextureVertex
{
    float x, y, z;
    float u, v;
} PositionTextureVertex;

typedef struct GameRenderer
{
    bool isInitialized = false;

    SDL_GPUGraphicsPipeline* Pipeline;
    SDL_GPUBuffer* VertexBuffer;
    SDL_GPUBuffer* IndexBuffer;
    SDL_GPUTexture* Texture;
    SDL_GPUSampler* Samplers[NumSamplers];
    int CurrentSamplerIndex = 1;

    SDL_GPUCopyPass* copyPass;
    SDL_GPUTransferBufferLocation vertexBufferLocation;
    SDL_GPUTransferBuffer* textureTransferBuffer;
    SDL_GPUCommandBuffer* uploadCmdBuf;

    // Shaders
    SDL_GPUShader* vertexShader;
    SDL_GPUShader* fragmentShader;

    // Data
    SDL_Surface* imageData;
} GameRenderer;

extern int
RendererInitShaders(Context* context);

extern int
RendererInitPipeline(Context* context);

extern int
RendererInitSDL(Context* context, SDL_WindowFlags windowFlags);

extern int
RendererCreateSamplers(Context* context);

extern int
RendererCreateTexture(Context* context);

extern SDL_GPUTransferBuffer*
RendererCreateTransferBuffers(Context* context);
