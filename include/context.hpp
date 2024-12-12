#pragma once

#include <SDL3/SDL.h>

#include <box2d/box2d.h>

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

    // Physics
    b2WorldDef worldDef;
    b2WorldId worldId;

    // Game data
    Ball ball;
} Context;
