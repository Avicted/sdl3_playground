#include <SDL3/SDL.h>

#include <glad/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

typedef struct Vec2
{
    float x;
    float y;

    Vec2 &operator+=(const Vec2 &other)
    {
        x += other.x;
        y += other.y;
        return *this;
    }
} Vec2;

typedef struct Ball // @Note: Actually a rectangle
{
    Vec2 position;
    Vec2 velocity;
    float radius;
} Ball;

Ball ball = {
    .position = {0.0f, 0.0f},
    .velocity = {0.4f, 0.6f},
    .radius = 64.0f,
};

bool isRunning = true;
SDL_Window *window = NULL;
bool isFullscreen = false;
const int GAME_WIDTH = 640;
const int GAME_HEIGHT = 360;
SDL_GLContext glContext = NULL;
int windowWidth = GAME_WIDTH;
int windowHeight = GAME_HEIGHT;

const float left = -GAME_WIDTH / 2;
const float right = GAME_WIDTH / 2;
const float top = GAME_HEIGHT / 2;
const float bottom = -GAME_HEIGHT / 2;

GLuint fbo, fboTexture;
GLuint fboVAO, fboVBO;
GLuint ballVAO, ballVBO;
GLuint ballTexture;
glm::mat4 projectionMatrix = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);

// Shaders
GLuint sceneShaderProgram;
GLuint fboShaderProgram;

// -------------------------------------------------------------------------------
static glm::mat4
CalculateProjection(int width, int height, int offsetX, int offsetY)
{
    static int cachedWidth = 0;
    static int cachedHeight = 0;
    static int cachedOffsetX = 0;
    static int cachedOffsetY = 0;
    static glm::mat4 cachedProjectionMatrix;

    if (width == cachedWidth && height == cachedHeight && offsetX == cachedOffsetX && offsetY == cachedOffsetY)
    {
        return cachedProjectionMatrix;
    }

    float aspectRatio = static_cast<float>(GAME_WIDTH) / static_cast<float>(GAME_HEIGHT);
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

    cachedProjectionMatrix = glm::ortho(
        -scaleX * GAME_WIDTH / 2.0f + (offsetX / scaleX), scaleX * GAME_WIDTH / 2.0f + (offsetX / scaleX),
        -scaleY * GAME_HEIGHT / 2.0f + (offsetY / scaleY), scaleY * GAME_HEIGHT / 2.0f + (offsetY / scaleY),
        -1.0f, 1.0f);

    cachedWidth = width;
    cachedHeight = height;
    cachedOffsetX = offsetX;
    cachedOffsetY = offsetY;

    return cachedProjectionMatrix;
}

static void
ResizeFboTexture(int newWidth, int newHeight)
{
    glBindTexture(GL_TEXTURE_2D, fboTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, newWidth, newHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void ResizeWindow(int width, int height)
{
    float gameAspectRatio = (float)GAME_WIDTH / (float)GAME_HEIGHT;
    float currentAspectRatio = (float)width / (float)height;

    int viewportWidth, viewportHeight;
    int viewportX = 0, viewportY = 0;

    if (currentAspectRatio > gameAspectRatio)
    {
        viewportHeight = height;
        viewportWidth = (int)(height * gameAspectRatio);
        viewportX = (width - viewportWidth) / 2;
    }
    else
    {
        viewportWidth = width;
        viewportHeight = (int)(width / gameAspectRatio);
        viewportY = (height - viewportHeight) / 2;
    }

    glViewport(viewportX, viewportY, viewportWidth, viewportHeight);

    glm::mat4 projection = CalculateProjection(viewportWidth, viewportHeight, viewportX, viewportY);
    glUseProgram(sceneShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(sceneShaderProgram, "u_mvp"), 1, GL_FALSE, glm::value_ptr(projection));

    ResizeFboTexture(viewportWidth, viewportHeight);
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
    window = SDL_CreateWindow("SDL Playground", GAME_WIDTH, GAME_HEIGHT, windowFlags);
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

    glViewport(0, 0, GAME_WIDTH, GAME_HEIGHT);

    // Create ball texture
    glGenTextures(1, &ballTexture);
    glBindTexture(GL_TEXTURE_2D, ballTexture);
    unsigned char whitePixel[] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Load resources/uv_test.png
    int width, height, channels;
    unsigned char *image = stbi_load("resources/uv_test.png", &width, &height, &channels, 0);
    if (!image)
    {
        SDL_Log("Failed to load image: %s", stbi_failure_reason());
        return 1;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    // glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(image);

    // Create FBO
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Create texture for the FBO
    glGenTextures(1, &fboTexture);
    glBindTexture(GL_TEXTURE_2D, fboTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GAME_WIDTH, GAME_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
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

    ResizeWindow(GAME_WIDTH, GAME_HEIGHT);

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

        if (event.type == SDL_EVENT_KEY_DOWN)
        {
            if (event.key.key == SDLK_F11)
            {
                isFullscreen = !isFullscreen;
                SDL_SetWindowFullscreen(window, isFullscreen ? SDL_WINDOW_FULLSCREEN : 0);
            }
        }
    }
}

static void
UpdateBall(Ball &ball, float left, float right, float top, float bottom)
{
    // Update position
    ball.position.x += ball.velocity.x;
    ball.position.y += ball.velocity.y;

    // Check for collisions with boundaries
    if (ball.position.x - (ball.radius / 2) < left || ball.position.x + (ball.radius / 2) > right)
    {
        ball.velocity.x *= -1; // Reverse X velocity
        ball.position.x = glm::clamp(ball.position.x, left + (ball.radius / 2), right - (ball.radius / 2));
    }

    if (ball.position.y - (ball.radius / 2) < bottom || ball.position.y + (ball.radius / 2) > top)
    {
        ball.velocity.y *= -1; // Reverse Y velocity
        ball.position.y = glm::clamp(ball.position.y, bottom + (ball.radius / 2), top - (ball.radius / 2));
    }

    // printf("Ball position: (%f, %f)\n", ball.position.x, ball.position.y);
}

static void
Update(void)
{
    UpdateBall(ball, left, right, top, bottom);
}

static void
Render(void)
{
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR)
    {
        printf("OpenGL Error: %d\n", err);
    }

    // Bind the FBO and clear it
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glClearColor(0.1f, 0.0f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Use the scene shader program
    glUseProgram(sceneShaderProgram);
    glBindVertexArray(ballVAO);

    // Ball rendering with aspect ratio correction
    float gameAspectRatio = (float)GAME_WIDTH / (float)GAME_HEIGHT;
    float currentAspectRatio = (float)windowWidth / (float)windowHeight;
    float scaleX = ball.radius;
    float scaleY = ball.radius;

    if (currentAspectRatio > gameAspectRatio)
    {
        scaleY *= currentAspectRatio / gameAspectRatio;
    }
    else
    {
        scaleX *= gameAspectRatio / currentAspectRatio;
    }

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(ball.position.x, ball.position.y, 0.0f));
    model = glm::scale(model, glm::vec3(scaleX, scaleY, 1.0f));

    glm::mat4 projection = CalculateProjection(windowWidth, windowHeight, 0, 0);
    glm::mat4 mvp = projection * model;
    glUniformMatrix4fv(glGetUniformLocation(sceneShaderProgram, "u_mvp"), 1, GL_FALSE, glm::value_ptr(mvp));

    glUniform1i(glGetUniformLocation(sceneShaderProgram, "ourTexture"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ballTexture);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Unbind the FBO
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Clear the default framebuffer
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Use the FBO shader program
    glUseProgram(fboShaderProgram);
    glUniform1i(glGetUniformLocation(fboShaderProgram, "screenTexture"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fboTexture);
    glBindVertexArray(fboVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

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
        // const float deltaTime = 1.0f / 60.0f;

        Input();
        Update();
        Render();
    }

    SDL_DestroyWindow(window);
    SDL_GL_DestroyContext(glContext);
    SDL_Quit();

    return 0;
}
