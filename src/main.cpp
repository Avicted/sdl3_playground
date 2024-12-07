#include <SDL3/SDL.h>

#include <glad/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <algorithm>

typedef struct Vec2
{
    float x;
    float y;
} Vec2;

typedef struct Ball // @Note: Actually a rectangle
{
    Vec2 position;
    Vec2 velocity;
    Vec2 size;
} Ball;

Ball ball = {
    .position = {320, 180},
    .velocity = {128 * 2, 64 * 2},
    .size = {32, 32},
};

bool isRunning = true;
SDL_Window *window = NULL;
const int windowWidth = 640 * 2;
const int windowHeight = 360 * 2;
SDL_GLContext glContext = NULL;

GLuint fbo, fboTexture;
GLuint fboVAO, fboVBO;
GLuint ballVAO, ballVBO;
GLuint ballTexture;
glm::mat4 projectionMatrix;

// Shaders
GLuint sceneShaderProgram;
GLuint fboShaderProgram;

// -------------------------------------------------------------------------------
static glm::mat4
CalculateProjection(int width, int height)
{
    float aspectRatio = (float)windowWidth / (float)windowHeight;
    float currentAspectRatio = (float)width / (float)height;

    float scaleX = 1.0f;
    float scaleY = 1.0f;

    if (currentAspectRatio > aspectRatio)
    {
        scaleX = currentAspectRatio / aspectRatio;
    }
    else
    {
        scaleY = aspectRatio / currentAspectRatio;
    }

    return glm::ortho(
        -scaleX * windowWidth / 2.0f, scaleX * windowWidth / 2.0f,
        -scaleY * windowHeight / 2.0f, scaleY * windowHeight / 2.0f,
        -1.0f, 1.0f);
}

static void
ResizeFboTexture(int newWidth, int newHeight)
{
    glBindTexture(GL_TEXTURE_2D, fboTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, newWidth, newHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void
ResizeWindow(int newWidth, int newHeight)
{
    // Original aspect ratio of the FBO
    float aspectRatio = (float)windowWidth / (float)windowHeight;

    // Calculate the final rendering dimensions
    int finalWidth = newWidth;
    int finalHeight = (int)(newWidth / aspectRatio);

    if (finalHeight > newHeight)
    {
        finalHeight = newHeight;
        finalWidth = (int)(newHeight * aspectRatio);
    }

    // Compute black bar offsets
    int offsetX = (newWidth - finalWidth) / 2;
    int offsetY = (newHeight - finalHeight) / 2;

    // Update the OpenGL viewport to match the centered render area
    glViewport(offsetX, offsetY, finalWidth, finalHeight);

    ResizeFboTexture(newWidth, newHeight);

    projectionMatrix = CalculateProjection(newWidth, newHeight);
}

static void
SetupFboQuad()
{
    float quadVertices[] = {
        // positions       // texture coords
        -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
        1.0f, -1.0f, 0.0f, 1.0f, 0.0f,

        -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f, 1.0f, 1.0f};

    glGenVertexArrays(1, &fboVAO);
    glGenBuffers(1, &fboVBO);

    glBindVertexArray(fboVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fboVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

static char *
ReadShaderSource(const char *filePath)
{
    FILE *file = fopen(filePath, "r");
    if (!file)
    {
        fprintf(stderr, "Failed to open shader file: %s\n", filePath);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    char *source = (char *)malloc(length + 1);
    if (!source)
    {
        fprintf(stderr, "Failed to allocate memory for shader source\n");
        fclose(file);
        return NULL;
    }

    fread(source, 1, length, file);
    source[length] = '\0';
    fclose(file);

    return source;
}

static GLuint
CompileShader(const char *source, GLenum shaderType)
{
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    // Check for compilation errors
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetShaderInfoLog(shader, sizeof(infoLog), NULL, infoLog);
        fprintf(stderr, "Shader compilation failed:\n%s\n", infoLog);
    }

    return shader;
}

static GLuint
LinkProgram(GLuint vertexShader, GLuint fragmentShader)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // Check for linking errors
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetProgramInfoLog(program, sizeof(infoLog), NULL, infoLog);
        fprintf(stderr, "Program linking failed:\n%s\n", infoLog);
    }

    return program;
}

static GLuint
LoadSceneShader()
{
    const char *vertexSource = ReadShaderSource("./shaders/scene_vertex_shader.glsl");
    const char *fragmentSource = ReadShaderSource("./shaders/scene_fragment_shader.glsl");

    if (!vertexSource || !fragmentSource)
    {
        fprintf(stderr, "Failed to load scene shaders\n");
        return 0;
    }

    float ballVertices[] = {
        // positions        // texture coords
        0.5f, 0.5f, 0.0f, 1.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,

        0.5f, 0.5f, 0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
        -0.5f, 0.5f, 0.0f, 0.0f, 1.0f};

    glGenVertexArrays(1, &ballVAO);
    glGenBuffers(1, &ballVBO);

    glBindVertexArray(ballVAO);

    glBindBuffer(GL_ARRAY_BUFFER, ballVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ballVertices), ballVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    GLuint vertexShader = CompileShader(vertexSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = CompileShader(fragmentSource, GL_FRAGMENT_SHADER);

    GLuint program = LinkProgram(vertexShader, fragmentShader);

    if (!program)
    {
        fprintf(stderr, "Failed to link scene shader program\n");
        fprintf(stderr, "Vertex Shader:\n%s\n", vertexSource);
        fprintf(stderr, "Fragment Shader:\n%s\n", fragmentSource);
        return 0;
    }

    // Cleanup
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    free((void *)vertexSource);
    free((void *)fragmentSource);

    return program;
}

static GLuint
LoadFboShader()
{
    const char *vertexSource = ReadShaderSource("./shaders/fbo_vertex_shader.glsl");
    const char *fragmentSource = ReadShaderSource("./shaders/fbo_fragment_shader.glsl");

    if (!vertexSource || !fragmentSource)
    {
        fprintf(stderr, "Failed to load FBO shaders\n");
        return 0;
    }

    GLuint vertexShader = CompileShader(vertexSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = CompileShader(fragmentSource, GL_FRAGMENT_SHADER);

    GLuint program = LinkProgram(vertexShader, fragmentShader);

    if (!program)
    {
        fprintf(stderr, "Failed to link FBO shader program\n");
        fprintf(stderr, "Vertex Shader:\n%s\n", vertexSource);
        fprintf(stderr, "Fragment Shader:\n%s\n", fragmentSource);
        return 0;
    }

    // Cleanup
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    free((void *)vertexSource);
    free((void *)fragmentSource);

    return program;
}

static int
Init(void)
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL_Init failed (%s)", SDL_GetError());
        return 1;
    }

    SDL_WindowFlags windowFlags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    window = SDL_CreateWindow("SDL Playground", windowWidth, windowHeight, windowFlags);
    if (!window)
    {
        SDL_Log("SDL_CreateWindow failed (%s)", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    glContext = SDL_GL_CreateContext(window);
    if (!gladLoaderLoadGL())
    {
        SDL_Log("Failed to initialize OpenGL context");
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize OpenGL context");
        return 1;
    }

    // Log OpenGL information
    SDL_Log("Vendor: %s", glGetString(GL_VENDOR));
    SDL_Log("Renderer: %s", glGetString(GL_RENDERER));
    SDL_Log("OpenGL Version: %s", glGetString(GL_VERSION));
    SDL_Log("GLSL Version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

    glViewport(0, 0, windowWidth, windowHeight);

    // Create ball texture
    glGenTextures(1, &ballTexture);
    glBindTexture(GL_TEXTURE_2D, ballTexture);
    unsigned char whitePixel[] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Create FBO
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Create texture for the FBO
    glGenTextures(1, &fboTexture);
    glBindTexture(GL_TEXTURE_2D, fboTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, windowWidth, windowHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        SDL_Log("Framebuffer is not complete!");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Load shaders
    sceneShaderProgram = LoadSceneShader();
    fboShaderProgram = LoadFboShader();

    // Call SetupFboQuad to initialize the full-screen quad
    SetupFboQuad();

    return 0;
}

static void
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
            int newWidth, newHeight;
            SDL_GetWindowSizeInPixels(window, &newWidth, &newHeight);

            // Keep the initial aspect ratio of the canvas
            ResizeWindow(newWidth, newHeight);
        }
    }
}

static void
Update(float deltaTime)
{
    // Update ball position using the original window dimensions
    ball.position.x += ball.velocity.x * deltaTime;
    ball.position.y += ball.velocity.y * deltaTime;

    // Bounce off walls using the original window dimensions
    if (ball.position.x < 0)
    {
        ball.position.x = 0;
        ball.velocity.x = -ball.velocity.x;
    }
    else if (ball.position.x + ball.size.x > windowWidth)
    {
        ball.position.x = windowWidth - ball.size.x;
        ball.velocity.x = -ball.velocity.x;
    }

    if (ball.position.y < 0)
    {
        ball.position.y = 0;
        ball.velocity.y = -ball.velocity.y;
    }
    else if (ball.position.y + ball.size.y > windowHeight)
    {
        ball.position.y = windowHeight - ball.size.y;
        ball.velocity.y = -ball.velocity.y;
    }
}

static void
Render(void)
{
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR)
    {
        printf("OpenGL Error: %d\n", err);
    }

    // Clear
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(sceneShaderProgram);
    glBindVertexArray(ballVAO);

    // Draw ball with normalized coordinates using original window dimensions
    glm::mat4 model = glm::mat4(1.0f);

    float normX = (ball.position.x / (float)windowWidth) * 2.0f - 1.0f;
    float normY = 1.0f - (ball.position.y / (float)windowHeight) * 2.0f;
    model = glm::translate(model, glm::vec3(normX, normY, 0.0f));

    float scaleFactor = std::min((float)windowWidth, (float)windowHeight);
    float scaleX = (ball.size.x / scaleFactor) * 2.0f;
    float scaleY = (ball.size.y / scaleFactor) * 2.0f;
    model = glm::scale(model, glm::vec3(scaleX, scaleY, 1.0f));

    glUniformMatrix4fv(glGetUniformLocation(sceneShaderProgram, "u_model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform1i(glGetUniformLocation(sceneShaderProgram, "ourTexture"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ballTexture);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // ----------------------------

    // Render the FBO texture to the screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(fboShaderProgram);
    glUniform1i(glGetUniformLocation(fboShaderProgram, "screenTexture"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(fboVAO);

    glBindTexture(GL_TEXTURE_2D, fboTexture);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // ----------------------------

    // Swap buffers
    SDL_GL_SwapWindow(window);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    int initSuccess = Init();
    if (initSuccess > 0)
    {
        return initSuccess;
    }

    while (isRunning)
    {
        const float deltaTime = 1.0f / 60.0f;

        Input();
        Update(deltaTime);
        Render();
    }

    // SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    // SDL_DestroyTexture(renderTexture);
    SDL_GL_DestroyContext(glContext);
    SDL_Quit();

    return 0;
}
