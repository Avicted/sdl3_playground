#include <SDL3/SDL.h>

#include <glad/gl.h>

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
    .velocity = {0.6 * 3, 0.4 * 3},
    .size = {32, 32},
};

bool isRunning = true;
SDL_Window *window = NULL;
// SDL_Renderer *renderer = NULL;
// SDL_Texture *renderTexture = NULL;
const int windowWidth = 640;
const int windowHeight = 360;
SDL_GLContext glContext = NULL;
static GLuint shaderProgram;

static void
SetupShaders(void)
{
    const char *vertexShaderSource =
        "#version 330 core\n"
        "layout (location = 0) in vec2 aPos;\n"
        "uniform vec2 uResolution;\n"
        "uniform vec2 uTranslation;\n"
        "void main()\n"
        "{\n"
        "    vec2 zeroToOne = (aPos + uTranslation) / uResolution;\n"
        "    vec2 zeroToTwo = zeroToOne * 2.0;\n"
        "    vec2 clipSpace = zeroToTwo - 1.0;\n"
        "    gl_Position = vec4(clipSpace * vec2(1.0, -1.0), 0.0, 1.0);\n"
        "}\n";

    const char *fragmentShaderSource =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "void main()\n"
        "{\n"
        "    FragColor = vec4(0.9, 0.2, 0.3, 1.0);\n"
        "}\n";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
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

    // if (!SDL_CreateWindowAndRenderer("SDL Playground", windowWidth, windowHeight, windowFlags, &window, &renderer))
    // {
    //     SDL_Log("SDL_CreateWindowAndRenderer failed (%s)", SDL_GetError());
    //     SDL_Quit();
    //     return 1;
    // }

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

    // renderTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, windowWidth, windowHeight);
    // if (!renderTexture)
    // {
    //     SDL_Log("SDL_CreateTexture failed (%s)", SDL_GetError());
    //     SDL_DestroyWindow(window);
    //     SDL_Quit();
    //     return 1;
    // }

    SetupShaders();

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
    }
}

static void
Update(float deltaTime)
{
    // Update ball, bounce off walls
    ball.position.x += ball.velocity.x * deltaTime;
    ball.position.y += ball.velocity.y * deltaTime;

    if (ball.position.x - ball.size.x < 0 || ball.position.x + ball.size.x > windowWidth)
    {
        ball.velocity.x = -ball.velocity.x;
    }

    if (ball.position.y - ball.size.y < 0 || ball.position.y + ball.size.y > windowHeight)
    {
        ball.velocity.y = -ball.velocity.y;
    }

    if (ball.position.x - ball.size.x < 0)
    {
        ball.position.x = ball.size.x;
    }
    else if (ball.position.x + ball.size.x > windowWidth)
    {
        ball.position.x = windowWidth - ball.size.x;
    }

    if (ball.position.y - ball.size.y < 0)
    {
        ball.position.y = ball.size.y;
    }
    else if (ball.position.y + ball.size.y > windowHeight)
    {
        ball.position.y = windowHeight - ball.size.y;
    }
}

static void
Render(void)
{
    // Clear the screen
    glClearColor(0.156f, 0.156f, 0.313f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Set up ball vertices
    float vertices[] = {
        -ball.size.x, -ball.size.y, // Bottom-left
        ball.size.x, -ball.size.y,  // Bottom-right
        -ball.size.x, ball.size.y,  // Top-left
        ball.size.x, ball.size.y    // Top-right
    };

    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    // Use the shader program
    glUseProgram(shaderProgram);

    // Set uniforms
    GLint resolutionLoc = glGetUniformLocation(shaderProgram, "uResolution");
    GLint translationLoc = glGetUniformLocation(shaderProgram, "uTranslation");

    glUniform2f(resolutionLoc, (float)windowWidth, (float)windowHeight);
    glUniform2f(translationLoc, ball.position.x, ball.position.y);

    // Draw the ball
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Cleanup
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);

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
