#pragma once

#include <SDL3/SDL.h>

#include "renderer.hpp"

typedef struct Ball // @Note: Actually a rectangle
{
    glm::vec2 position;
    glm::vec2 velocity;
    float radius;
} Ball;

typedef struct Context
{
    const char* GameName;
    const char* BasePath;
    SDL_Window* Window;
    SDL_GPUDevice* Device;
    float DeltaTime;

    bool isRunning = true;
    bool isFullscreen = false;

    int windowWidth = 640;
    int windowHeight = 360;
    int scaleX, scaleY, scale, offsetX, offsetY;

    GameRenderer Renderer;

    // Game data
    Ball ball;
} Context;
