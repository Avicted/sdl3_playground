#pragma once

#include <SDL3/SDL.h>

#include "renderer.hpp"

typedef struct Context
{
    const char* GameName;
    const char* BasePath;
    SDL_Window* Window;
    SDL_GPUDevice* Device;
    float DeltaTime;

    GameRenderer Renderer;
} Context;
