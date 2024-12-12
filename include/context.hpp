#pragma once

#include <SDL3/SDL.h>

#include "ball.hpp"
#include "renderer.hpp"

typedef struct Context
{
    const char* GameName;
    const char* BasePath;
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
