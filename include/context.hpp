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

    // Surrounding walls
    b2BodyDef bodyDefTop;
    b2BodyDef bodyDefBottom;
    b2BodyDef bodyDefLeft;
    b2BodyDef bodyDefRight;

    b2BodyId topBoxId;
    b2Polygon topBoxPolygon;
    b2ShapeDef groundShapeDef;

    // Game data
    Ball ball;
} Context;
