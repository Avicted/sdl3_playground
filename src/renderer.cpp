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

// #define STBI_MALLOC SDL_malloc
// #define STBI_REALLOC SDL_realloc
// #define STBI_FREE SDL_free
// #include <stb_image.h>

// Our code
#include "context.hpp"
#include "includes.hpp"
#include "renderer.hpp"

const char* SamplerNames[] = {
    "PointClamp", "PointWrap",        "LinearClamp",
    "LinearWrap", "AnisotropicClamp", "AnisotropicWrap",
};

// -------------------------------------------------------------------------------
SDL_GPUTransferBuffer*
RendererCreateTransferBuffers(Context* context)
{
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
        .size = static_cast<Uint32>(context->Renderer.imageData->w *
                                    context->Renderer.imageData->h * 4)
    };

    context->Renderer.textureTransferBuffer = SDL_CreateGPUTransferBuffer(
      context->Device, &textureTransferBufferCreateInfo);

    Uint8* textureTransferPtr = static_cast<Uint8*>(SDL_MapGPUTransferBuffer(
      context->Device, context->Renderer.textureTransferBuffer, false));

    SDL_memcpy(textureTransferPtr,
               context->Renderer.imageData->pixels,
               context->Renderer.imageData->w * context->Renderer.imageData->h *
                 4);
    SDL_UnmapGPUTransferBuffer(context->Device,
                               context->Renderer.textureTransferBuffer);

    // Upload the transfer data to the GPU resources
    context->Renderer.uploadCmdBuf =
      SDL_AcquireGPUCommandBuffer(context->Device);

    context->Renderer.copyPass =
      SDL_BeginGPUCopyPass(context->Renderer.uploadCmdBuf);

    context->Renderer.vertexBufferLocation = {
        .transfer_buffer = bufferTransferBuffer,
        .offset = 0,
    };
    SDL_GPUBufferRegion vertexBufferRegion = {
        .buffer = context->Renderer.VertexBuffer,
        .offset = 0,
        .size = sizeof(PositionTextureVertex) * 4
    };
    SDL_UploadToGPUBuffer(context->Renderer.copyPass,
                          &context->Renderer.vertexBufferLocation,
                          &vertexBufferRegion,
                          false);

    SDL_GPUTransferBufferLocation indexBufferLocation = {
        .transfer_buffer = bufferTransferBuffer,
        .offset = sizeof(PositionTextureVertex) * 4
    };
    SDL_GPUBufferRegion indexBufferRegion = {
        .buffer = context->Renderer.IndexBuffer,
        .offset = 0,
        .size = sizeof(Uint16) * 6,
    };
    SDL_UploadToGPUBuffer(context->Renderer.copyPass,
                          &indexBufferLocation,
                          &indexBufferRegion,
                          false);

    SDL_GPUTextureTransferInfo textureTransferInfo = {
        .transfer_buffer = context->Renderer.textureTransferBuffer,
        .offset = 0,
    };
    SDL_GPUTextureRegion textureRegion = {
        .texture = context->Renderer.Texture,
        .w = static_cast<Uint32>(context->Renderer.imageData->w),
        .h = static_cast<Uint32>(context->Renderer.imageData->h),
        .d = 1,
    };
    SDL_UploadToGPUTexture(
      context->Renderer.copyPass, &textureTransferInfo, &textureRegion, false);

    SDL_EndGPUCopyPass(context->Renderer.copyPass);
    SDL_SubmitGPUCommandBuffer(context->Renderer.uploadCmdBuf);
    SDL_DestroySurface(context->Renderer.imageData);

    return bufferTransferBuffer;
}

int
RendererCreateTexture(Context* context)
{
    // Create the GPU resources
    SDL_GPUBufferCreateInfo vertexBufferCreateInfo = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = sizeof(PositionTextureVertex) * 4
    };

    // context->Renderer.uploadCmdBuf =
    //   SDL_AcquireGPUCommandBuffer(context->Device);
    //
    // context->Renderer.copyPass =
    //   SDL_BeginGPUCopyPass(context->Renderer.uploadCmdBuf);

    context->Renderer.VertexBuffer =
      SDL_CreateGPUBuffer(context->Device, &vertexBufferCreateInfo);
    SDL_SetGPUBufferName(context->Device,
                         context->Renderer.VertexBuffer,
                         "TestImage Vertex Buffer");

    SDL_GPUBufferCreateInfo indexBufferCreateInfo = {
        .usage = SDL_GPU_BUFFERUSAGE_INDEX, .size = sizeof(Uint16) * 6
    };
    context->Renderer.IndexBuffer =
      SDL_CreateGPUBuffer(context->Device, &indexBufferCreateInfo);

    SDL_GPUTextureCreateInfo textureCreateInfo = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = static_cast<Uint32>(context->Renderer.imageData->w),
        .height = static_cast<Uint32>(context->Renderer.imageData->h),
        .layer_count_or_depth = 1,
        .num_levels = 1,
    };
    context->Renderer.Texture =
      SDL_CreateGPUTexture(context->Device, &textureCreateInfo);

    SDL_SetGPUTextureName(
      context->Device, context->Renderer.Texture, "TestImage Texture");

    SDL_GPUTransferBuffer* transferBuffer =
      RendererCreateTransferBuffers(context);

    SDL_GPUCommandBuffer* uploadCmdBuf =
      SDL_AcquireGPUCommandBuffer(context->Device);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

    if (copyPass == NULL)
    {
        SDL_Log("Failed to begin GPU copy pass: %s", SDL_GetError());
        return -1;
    }

    SDL_GPUTransferBufferLocation vertexBufferLocation = { .transfer_buffer =
                                                             transferBuffer,
                                                           .offset = 0 };
    SDL_GPUBufferRegion vertexBufferRegion = {
        .buffer = context->Renderer.VertexBuffer,
        .offset = 0,
        .size = sizeof(PositionTextureVertex) * 4
    };
    SDL_UploadToGPUBuffer(
      copyPass, &vertexBufferLocation, &vertexBufferRegion, false);

    SDL_GPUTransferBufferLocation indexBufferLocation = {
        .transfer_buffer = transferBuffer,
        .offset = sizeof(PositionTextureVertex) * 4
    };
    SDL_GPUBufferRegion indexBufferRegion = {
        .buffer = context->Renderer.IndexBuffer,
        .offset = 0,
        .size = sizeof(Uint16) * 6,
    };
    SDL_UploadToGPUBuffer(
      copyPass, &indexBufferLocation, &indexBufferRegion, false);

    SDL_GPUTextureTransferInfo textureTransferInfo = {
        .transfer_buffer = transferBuffer,
        .offset = 0,
    };
    SDL_GPUTextureRegion textureRegion = {
        .texture = context->Renderer.Texture,
        .w = static_cast<Uint32>(context->Renderer.imageData->w),
        .h = static_cast<Uint32>(context->Renderer.imageData->h),
        .d = 1,
    };
    SDL_UploadToGPUTexture(
      copyPass, &textureTransferInfo, &textureRegion, false);

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
    SDL_DestroySurface(context->Renderer.imageData);

    // Finally, print instructions!
    SDL_Log("Press Left/Right to switch between sampler states");
    SDL_Log("Setting sampler state to: %s", SamplerNames[0]);

    return 0;
}

int
RendererCreateSamplers(Context* context)
{
    SDL_ReleaseGPUShader(context->Device, context->Renderer.vertexShader);
    SDL_ReleaseGPUShader(context->Device, context->Renderer.fragmentShader);

    // PointClamp
    SDL_GPUSamplerCreateInfo pointClampSamplerInfo = {
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    context->Renderer.Samplers[0] =
      SDL_CreateGPUSampler(context->Device, &pointClampSamplerInfo);

    // PointWrap
    SDL_GPUSamplerCreateInfo pointWrapSamplerInfo = {
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    };
    context->Renderer.Samplers[1] =
      SDL_CreateGPUSampler(context->Device, &pointWrapSamplerInfo);

    // LinearClamp
    SDL_GPUSamplerCreateInfo linearClampSamplerInfo = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    context->Renderer.Samplers[2] =
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
    context->Renderer.Samplers[3] =
      SDL_CreateGPUSampler(context->Device, &linearWrapSamplerInfo);

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
    context->Renderer.Samplers[4] =
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
    context->Renderer.Samplers[5] =
      SDL_CreateGPUSampler(context->Device, &anisotropicWrapSamplerInfo);

    return 0;
}

int
RendererInitSDL(Context* context, SDL_WindowFlags windowFlags)
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

internal SDL_GPUShader*
LoadShader(Context* context,
           SDL_GPUDevice* device,
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
                     context->BasePath,
                     shaderFilename);
        format = SDL_GPU_SHADERFORMAT_SPIRV;
        entrypoint = "main";
    }
    else if (backendFormats & SDL_GPU_SHADERFORMAT_MSL)
    {
        SDL_snprintf(fullPath,
                     sizeof(fullPath),
                     "%sshaders/compiled/MSL/%s.msl",
                     context->BasePath,
                     shaderFilename);
        format = SDL_GPU_SHADERFORMAT_MSL;
        entrypoint = "main0";
    }
    else if (backendFormats & SDL_GPU_SHADERFORMAT_DXIL)
    {
        SDL_snprintf(fullPath,
                     sizeof(fullPath),
                     "%sshaders/compiled/DXIL/%s.dxil",
                     context->BasePath,
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

int
RendererInitShaders(Context* context)
{
    // Create the shaders
    context->Renderer.vertexShader =
      LoadShader(context, context->Device, "TexturedQuad.vert", 0, 0, 0, 0);
    if (context->Renderer.vertexShader == NULL)
    {
        SDL_Log("Failed to create vertex shader!");
        return -1;
    }

    context->Renderer.fragmentShader =
      LoadShader(context, context->Device, "TexturedQuad.frag", 1, 0, 0, 0);
    if (context->Renderer.fragmentShader == NULL)
    {
        SDL_Log("Failed to create fragment shader!");
        return -1;
    }

    return 0;
}

int
RendererInitPipeline(Context* context)
{
    // Create the pipeline
    SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
        .vertex_shader = context->Renderer.vertexShader,
        .fragment_shader = context->Renderer.fragmentShader,
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

    context->Renderer.Pipeline =
      SDL_CreateGPUGraphicsPipeline(context->Device, &pipelineCreateInfo);
    if (context->Renderer.Pipeline == NULL)
    {
        SDL_Log("Failed to create pipeline!");
        return -1;
    }

    return 0;
}