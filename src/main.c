#include <SDL3/SDL.h>

#include <glad/gl.h>

#include <glm/glm.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

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
    .velocity = {128, 64},
    .size = {32, 32},
};

bool isRunning = true;
SDL_Window *window = NULL;
const int windowWidth = 640;
const int windowHeight = 360;
SDL_GLContext glContext = NULL;

GLuint fbo, fboTexture;
GLuint fboVAO, fboVBO;
GLuint ballVAO, ballVBO;

// Shaders
GLuint sceneShaderProgram;
GLuint fboShaderProgram;

// -------------------------------------------------------------------------------

static void
ResizeWindow(void)
{

    // int newWindowWidth = newWidth;
    // int newwindowHeight = newHeight;

    // Update the projection matrix for correct aspect ratio
    //  glm::mat4 projection = glm::perspective(
    //      glm::radians(45.0f),
    //      (float)newindowWidth / (float)newWindowHeight,
    //      0.1f,
    //      100.0f);
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

    // Setup ball VAO and VBO
    float ballVertices[] = {
        // positions       // texture coords
        0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f,

        0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f, 0.0f};

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

            float aspectRatio = (float)windowWidth / (float)windowHeight;
            float newAspectRatio = (float)newWidth / (float)newHeight;

            if (newAspectRatio > aspectRatio)
            {
                // Wider: Add black bars on the sides
                int viewportWidth = (int)(newHeight * aspectRatio);
                int xOffset = (newWidth - viewportWidth) / 2;
                glViewport(xOffset, 0, viewportWidth, newHeight);
            }
            else
            {
                // Taller: Add black bars on the top and bottom
                int viewportHeight = (int)(newWidth / aspectRatio);
                int yOffset = (newHeight - viewportHeight) / 2;
                glViewport(0, yOffset, newWidth, viewportHeight);
            }

            // ResizeWindow(newWidth, newHeight);
            ResizeWindow();
        }
    }
}

static void
Update(float deltaTime)
{
    // Update ball, bounce off walls
    ball.position.x += ball.velocity.x * deltaTime;
    ball.position.y += ball.velocity.y * deltaTime;

    if (ball.position.x < 0 || ball.position.x + ball.size.x > windowWidth)
    {
        ball.velocity.x = -ball.velocity.x;
    }

    if (ball.position.y < 0 || ball.position.y + ball.size.y > windowHeight)
    {
        ball.velocity.y = -ball.velocity.y;
    }

    if (ball.position.x < 0)
    {
        ball.position.x = 0;
    }
    else if (ball.position.x + ball.size.x > windowWidth)
    {
        ball.position.x = windowWidth - ball.size.x;
    }

    if (ball.position.y < 0)
    {
        ball.position.y = 0;
    }
    else if (ball.position.y + ball.size.y > windowHeight)
    {
        ball.position.y = windowHeight - ball.size.y;
    }
}

static void
Render(void)
{
    // Step 1: Render to the FBO
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Render scene (ball) to the FBO
    glUseProgram(sceneShaderProgram);
    glUniform2f(glGetUniformLocation(sceneShaderProgram, "uResolution"), windowWidth, windowHeight);
    glUniform2f(glGetUniformLocation(sceneShaderProgram, "uPosition"), ball.position.x, ball.position.y);
    glUniform2f(glGetUniformLocation(sceneShaderProgram, "uSize"), ball.size.x, ball.size.y);

    glBindVertexArray(ballVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Step 2: Render the FBO texture to the default framebuffer (the window)
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // Bind to the default framebuffer (the window)
    glClear(GL_COLOR_BUFFER_BIT);

    // Use the FBO shader to display the FBO texture to the screen
    glUseProgram(fboShaderProgram);
    glBindVertexArray(fboVAO);
    glBindTexture(GL_TEXTURE_2D, fboTexture);
    glDrawArrays(GL_TRIANGLES, 0, 6); // Draw a full-screen quad with the FBO texture

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
