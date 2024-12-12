#pragma once

#include <SDL3/SDL.h>

#include <box2d/box2d.h>

typedef struct Ball // @Note: Actually a rectangle
{
    glm::vec2 position;
    glm::vec2 velocity;
    float radius;

    // Physics
    b2BodyId* body;
} Ball;
