#pragma once

#include <SDL3/SDL.h>

typedef struct Ball // @Note: Actually a rectangle
{
    glm::vec2 position;
    glm::vec2 velocity;
    float radius;
} Ball;
