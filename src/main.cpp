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
#include "includes.h"

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

const int GAME_WIDTH = 640;
const int GAME_HEIGHT = 360;
int windowWidth = GAME_WIDTH;
int windowHeight = GAME_HEIGHT;

typedef struct Context
{
    const char* GameName;
    const char* BasePath;
    SDL_Window* Window;
    SDL_GPUDevice* Device;
    float DeltaTime;
} Context;

typedef struct PositionTextureVertex
{
    float x, y, z;
    float u, v;
} PositionTextureVertex;

global_variable const char* SamplerNames[] = {
    "PointClamp", "PointWrap",        "LinearClamp",
    "LinearWrap", "AnisotropicClamp", "AnisotropicWrap",
};

global_variable SDL_GPUGraphicsPipeline* Pipeline;
global_variable SDL_GPUBuffer* VertexBuffer;
global_variable SDL_GPUBuffer* IndexBuffer;
global_variable SDL_GPUTexture* Texture;
global_variable SDL_GPUSampler* Samplers[SDL_arraysize(SamplerNames)];

global_variable int CurrentSamplerIndex = 1;

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

internal SDL_GPUShader*
LoadShader(SDL_GPUDevice* device,
           const char* shaderFilename,
           Uint32 samplerCount,
           Uint32 uniformBufferCount,
           Uint32 storageBufferCount,
           Uint32 storageTextureCount)
{
    // Auto-detect the shader stage from the file name for convenience
    SDL_GPUShaderStage stage;
    if (SDL_strstr(shaderFilename, ".vert"))
    {
        stage = SDL_GPU_SHADERSTAGE_VERTEX;
    }
    else if (SDL_strstr(shaderFilename, ".frag"))
    {
        stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    }
    else
    {
        SDL_Log("Invalid shader stage!");
        return NULL;
    }

    char fullPath[256];
    SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(device);
    SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
    const char* entrypoint;

    if (backendFormats & SDL_GPU_SHADERFORMAT_SPIRV)
    {
        SDL_snprintf(fullPath,
                     sizeof(fullPath),
                     "%sshaders/compiled/SPIRV/%s.spv",
                     BasePath,
                     shaderFilename);
        format = SDL_GPU_SHADERFORMAT_SPIRV;
        entrypoint = "main";
    }
    else if (backendFormats & SDL_GPU_SHADERFORMAT_MSL)
    {
        SDL_snprintf(fullPath,
                     sizeof(fullPath),
                     "%sshaders/compiled/MSL/%s.msl",
                     BasePath,
                     shaderFilename);
        format = SDL_GPU_SHADERFORMAT_MSL;
        entrypoint = "main0";
    }
    else if (backendFormats & SDL_GPU_SHADERFORMAT_DXIL)
    {
        SDL_snprintf(fullPath,
                     sizeof(fullPath),
                     "%sshaders/compiled/DXIL/%s.dxil",
                     BasePath,
                     shaderFilename);
        format = SDL_GPU_SHADERFORMAT_DXIL;
        entrypoint = "main";
    }
    else
    {
        SDL_Log("%s", "Unrecognized backend shader format!");
        return NULL;
    }

    size_t codeSize;
    void* code = SDL_LoadFile(fullPath, &codeSize);
    if (code == NULL)
    {
        SDL_Log("Failed to load shader from disk! %s", fullPath);
        return NULL;
    }

    SDL_GPUShaderCreateInfo shaderInfo = {
        .code_size = codeSize,
        .code = static_cast<const Uint8*>(code),
        .entrypoint = entrypoint,
        .format = format,
        .stage = stage,
        .num_samplers = samplerCount,
        .num_storage_textures = storageTextureCount,
        .num_storage_buffers = storageBufferCount,
        .num_uniform_buffers = uniformBufferCount,
        .props = 0,
    };
    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &shaderInfo);
    if (shader == NULL)
    {
        SDL_Log("Failed to create shader!");
        SDL_free(code);
        return NULL;
    }

    SDL_free(code);
    return shader;
}

internal int
CommonInit(Context* context, SDL_WindowFlags windowFlags)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return 1;
    }

    context->Device =
      SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, NULL);

    if (context->Device == NULL)
    {
        SDL_Log("GPUCreateDevice failed: %s", SDL_GetError());
        return -1;
    }

    context->Window =
      SDL_CreateWindow(context->GameName, GAME_WIDTH, GAME_HEIGHT, windowFlags);
    if (context->Window == NULL)
    {
        SDL_Log("CreateWindow failed: %s", SDL_GetError());
        return -1;
    }

    if (!SDL_ClaimWindowForGPUDevice(context->Device, context->Window))
    {
        SDL_Log("GPUClaimWindow failed");
        return -1;
    }

    return 0;
}

internal int
Init(Context* context)
{
    int result = CommonInit(context, SDL_WINDOW_RESIZABLE);
    if (result < 0)
    {
        return result;
    }

    InitializeAssetLoader();

    // Create the shaders
    SDL_GPUShader* vertexShader =
      LoadShader(context->Device, "TexturedQuad.vert", 0, 0, 0, 0);
    if (vertexShader == NULL)
    {
        SDL_Log("Failed to create vertex shader!");
        return -1;
    }

    SDL_GPUShader* fragmentShader =
      LoadShader(context->Device, "TexturedQuad.frag", 1, 0, 0, 0);
    if (fragmentShader == NULL)
    {
        SDL_Log("Failed to create fragment shader!");
        return -1;
    }

    // Load the image
    SDL_Surface* imageData = LoadImage("uv_test.bmp", 4);
    if (imageData == NULL)
    {
        SDL_Log("Could not load image data!");
        return -1;
    }

    // Create the pipeline
    SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
        .vertex_shader = vertexShader,
        .fragment_shader = fragmentShader,
        .vertex_input_state =
          (SDL_GPUVertexInputState){
            .vertex_buffer_descriptions =
              (SDL_GPUVertexBufferDescription[]){
                {
                  .slot = 0,
                  .pitch = sizeof(PositionTextureVertex),
                  .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                  .instance_step_rate = 0,
                },

              },
            .num_vertex_buffers = 1,
            .vertex_attributes =
              (SDL_GPUVertexAttribute[]){
                { .location = 0,
                  .buffer_slot = 0,
                  .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                  .offset = 0 },
                { .location = 1,
                  .buffer_slot = 0,
                  .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                  .offset = sizeof(float) * 3 } },
            .num_vertex_attributes = 2,
          },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state =
          (SDL_GPURasterizerState){
            .fill_mode = SDL_GPU_FILLMODE_FILL,
            .cull_mode = SDL_GPU_CULLMODE_NONE,
            .front_face = SDL_GPU_FRONTFACE_CLOCKWISE,
          },
        .multisample_state = (SDL_GPUMultisampleState){
          .sample_count = SDL_GPUSampleCount::SDL_GPU_SAMPLECOUNT_1,
          .sample_mask = 0xFFFFFFFF,
          .enable_mask = false,
        },
        .depth_stencil_state = (SDL_GPUDepthStencilState){
          .compare_op = SDL_GPU_COMPAREOP_ALWAYS,
          .back_stencil_state = (SDL_GPUStencilOpState){
            .fail_op = SDL_GPU_STENCILOP_KEEP,
            .pass_op = SDL_GPU_STENCILOP_KEEP,
            .depth_fail_op = SDL_GPU_STENCILOP_KEEP,
            .compare_op = SDL_GPU_COMPAREOP_ALWAYS,
          },
          .front_stencil_state = (SDL_GPUStencilOpState){
            .fail_op = SDL_GPU_STENCILOP_KEEP,
            .pass_op = SDL_GPU_STENCILOP_KEEP,
            .depth_fail_op = SDL_GPU_STENCILOP_KEEP,
            .compare_op = SDL_GPU_COMPAREOP_ALWAYS,
          },
          .compare_mask = 0xFF,
          .write_mask = 0xFF,
          .enable_depth_test = false,
          .enable_depth_write = false,
          .enable_stencil_test = false,
          .padding1 = 0,
          .padding2 = 0,
          .padding3 = 0,
        },
        .target_info = {
          .color_target_descriptions = (SDL_GPUColorTargetDescription[]){ { .format = SDL_GetGPUSwapchainTextureFormat(context->Device, context->Window) } },
          .num_color_targets = 1,
        },
        .props = 0,
    };

    Pipeline =
      SDL_CreateGPUGraphicsPipeline(context->Device, &pipelineCreateInfo);
    if (Pipeline == NULL)
    {
        SDL_Log("Failed to create pipeline!");
        return -1;
    }

    SDL_ReleaseGPUShader(context->Device, vertexShader);
    SDL_ReleaseGPUShader(context->Device, fragmentShader);

    // PointClamp
    SDL_GPUSamplerCreateInfo pointClampSamplerInfo = {
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    Samplers[0] = SDL_CreateGPUSampler(context->Device, &pointClampSamplerInfo);

    // PointWrap
    SDL_GPUSamplerCreateInfo pointWrapSamplerInfo = {
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    };
    Samplers[1] = SDL_CreateGPUSampler(context->Device, &pointWrapSamplerInfo);

    // LinearClamp
    SDL_GPUSamplerCreateInfo linearClampSamplerInfo = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    Samplers[2] =
      SDL_CreateGPUSampler(context->Device, &linearClampSamplerInfo);

    // LinearWrap
    SDL_GPUSamplerCreateInfo linearWrapSamplerInfo = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    };
    Samplers[3] = SDL_CreateGPUSampler(context->Device, &linearWrapSamplerInfo);

    // AnisotropicClamp
    SDL_GPUSamplerCreateInfo anisotropicClampSamplerInfo = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .mip_lod_bias = 0.0f,
        .max_anisotropy = 4,
        .enable_anisotropy = true,
    };
    Samplers[4] =
      SDL_CreateGPUSampler(context->Device, &anisotropicClampSamplerInfo);

    // AnisotropicWrap
    SDL_GPUSamplerCreateInfo anisotropicWrapSamplerInfo = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .mip_lod_bias = 0.0f,
        .max_anisotropy = 4,
        .enable_anisotropy = true,
    };
    Samplers[5] =
      SDL_CreateGPUSampler(context->Device, &anisotropicWrapSamplerInfo);

    // Create the GPU resources
    SDL_GPUBufferCreateInfo vertexBufferCreateInfo = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = sizeof(PositionTextureVertex) * 4
    };
    VertexBuffer =
      SDL_CreateGPUBuffer(context->Device, &vertexBufferCreateInfo);
    SDL_SetGPUBufferName(
      context->Device, VertexBuffer, "TestImage Vertex Buffer");

    SDL_GPUBufferCreateInfo indexBufferCreateInfo = {
        .usage = SDL_GPU_BUFFERUSAGE_INDEX, .size = sizeof(Uint16) * 6
    };
    IndexBuffer = SDL_CreateGPUBuffer(context->Device, &indexBufferCreateInfo);

    SDL_GPUTextureCreateInfo textureCreateInfo = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = static_cast<Uint32>(imageData->w),
        .height = static_cast<Uint32>(imageData->h),
        .layer_count_or_depth = 1,
        .num_levels = 1,
    };
    Texture = SDL_CreateGPUTexture(context->Device, &textureCreateInfo);
    SDL_SetGPUTextureName(context->Device, Texture, "TestImage Texture");

    // Set up buffer data
    SDL_GPUTransferBufferCreateInfo bufferTransferBufferCreateInfo = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = (sizeof(PositionTextureVertex) * 4) + (sizeof(Uint16) * 6)
    };
    SDL_GPUTransferBuffer* bufferTransferBuffer = SDL_CreateGPUTransferBuffer(
      context->Device, &bufferTransferBufferCreateInfo);

    PositionTextureVertex* transferData = static_cast<PositionTextureVertex*>(
      SDL_MapGPUTransferBuffer(context->Device, bufferTransferBuffer, false));

    transferData[0] = (PositionTextureVertex){ -1, 1, 0, 0, 0 };
    transferData[1] = (PositionTextureVertex){ 1, 1, 0, 4, 0 };
    transferData[2] = (PositionTextureVertex){ 1, -1, 0, 4, 4 };
    transferData[3] = (PositionTextureVertex){ -1, -1, 0, 0, 4 };

    Uint16* indexData = (Uint16*)&transferData[4];
    indexData[0] = 0;
    indexData[1] = 1;
    indexData[2] = 2;
    indexData[3] = 0;
    indexData[4] = 2;
    indexData[5] = 3;

    SDL_UnmapGPUTransferBuffer(context->Device, bufferTransferBuffer);

    // Set up texture data
    SDL_GPUTransferBufferCreateInfo textureTransferBufferCreateInfo = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = static_cast<Uint32>(imageData->w * imageData->h * 4)
    };
    SDL_GPUTransferBuffer* textureTransferBuffer = SDL_CreateGPUTransferBuffer(
      context->Device, &textureTransferBufferCreateInfo);

    Uint8* textureTransferPtr = static_cast<Uint8*>(
      SDL_MapGPUTransferBuffer(context->Device, textureTransferBuffer, false));

    SDL_memcpy(
      textureTransferPtr, imageData->pixels, imageData->w * imageData->h * 4);
    SDL_UnmapGPUTransferBuffer(context->Device, textureTransferBuffer);

    // Upload the transfer data to the GPU resources
    SDL_GPUCommandBuffer* uploadCmdBuf =
      SDL_AcquireGPUCommandBuffer(context->Device);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

    SDL_GPUTransferBufferLocation vertexBufferLocation = {
        .transfer_buffer = bufferTransferBuffer, .offset = 0
    };
    SDL_GPUBufferRegion vertexBufferRegion = {
        .buffer = VertexBuffer,
        .offset = 0,
        .size = sizeof(PositionTextureVertex) * 4
    };
    SDL_UploadToGPUBuffer(
      copyPass, &vertexBufferLocation, &vertexBufferRegion, false);

    SDL_GPUTransferBufferLocation indexBufferLocation = {
        .transfer_buffer = bufferTransferBuffer,
        .offset = sizeof(PositionTextureVertex) * 4
    };
    SDL_GPUBufferRegion indexBufferRegion = {
        .buffer = IndexBuffer,
        .offset = 0,
        .size = sizeof(Uint16) * 6,
    };
    SDL_UploadToGPUBuffer(
      copyPass, &indexBufferLocation, &indexBufferRegion, false);

    SDL_GPUTextureTransferInfo textureTransferInfo = {
        .transfer_buffer = textureTransferBuffer,
        .offset = 0,
    };
    SDL_GPUTextureRegion textureRegion = {
        .texture = Texture,
        .w = static_cast<Uint32>(imageData->w),
        .h = static_cast<Uint32>(imageData->h),
        .d = 1,
    };
    SDL_UploadToGPUTexture(
      copyPass, &textureTransferInfo, &textureRegion, false);

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
    SDL_DestroySurface(imageData);

    // Finally, print instructions!
    SDL_Log("Press Left/Right to switch between sampler states");
    SDL_Log("Setting sampler state to: %s", SamplerNames[0]);

    return 0;
}

internal int
Input(void)
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
                CurrentSamplerIndex -= 1;
                if (CurrentSamplerIndex < 0)
                {
                    CurrentSamplerIndex = SDL_arraysize(Samplers) - 1;
                }
                SDL_Log("Setting sampler state to: %s",
                        SamplerNames[CurrentSamplerIndex]);
            }
            if (event.key.key == SDLK_RIGHT)
            {
                CurrentSamplerIndex =
                  (CurrentSamplerIndex + 1) % SDL_arraysize(Samplers);
                SDL_Log("Setting sampler state to: %s",
                        SamplerNames[CurrentSamplerIndex]);
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
                .buffer = VertexBuffer,
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

        SDL_BindGPUGraphicsPipeline(renderPass, Pipeline);

        SDL_GPUBufferBinding vertexBufferBinding = { .buffer = VertexBuffer,
                                                     .offset = 0 };
        SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBufferBinding, 1);

        SDL_GPUBufferBinding indexBufferBinding = { .buffer = IndexBuffer,
                                                    .offset = 0 };
        SDL_BindGPUIndexBuffer(
          renderPass, &indexBufferBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        SDL_GPUTextureSamplerBinding textureSamplerBinding = {
            .texture = Texture, .sampler = Samplers[CurrentSamplerIndex]
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
    SDL_ReleaseGPUGraphicsPipeline(context->Device, Pipeline);
    SDL_ReleaseGPUBuffer(context->Device, VertexBuffer);
    SDL_ReleaseGPUBuffer(context->Device, IndexBuffer);
    SDL_ReleaseGPUTexture(context->Device, Texture);

    for (long unsigned int i = 0; i < SDL_arraysize(Samplers); i += 1)
    {
        SDL_ReleaseGPUSampler(context->Device, Samplers[i]);
    }

    CurrentSamplerIndex = 0;

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

        Input();
        Update(deltaTime);
        Render(context);
    }

    Cleanup(context);
    free(context);

    return 0;
}
