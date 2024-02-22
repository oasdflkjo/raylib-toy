// workaround for naming conflicts, still prefer c tough :)
#include "raylib.h"
#define Rectangle _Rectangle
#define CloseWindow _CloseWindow
#define ShowCursor _ShowCursor
#include <windows.h>
#undef Rectangle
#undef CloseWindow
#undef ShowCursor

// #include <xmmintrin.h>
#include <immintrin.h> // AVX2 and earlier intrinsics
#include <assert.h>
#include <time.h>

#define PARTICLE_COUNT 360000 //1440000 // TODO: crashes if used for example 130000
#define PARTICLE_COLOR \
    (Color) { 0, 0, 0, 255 }
#define MAX_THREADS 16
#define ALIGNMENT 16
#define TARGET_FPS 160
#define ATTRACTION_STRENGHT 0.2000f
#define FRICTION 0.999f
#define SCREEN_WIDTH 3440
#define SCREEN_HEIGHT 1440

typedef struct Particles
{
    float *posX;
    float *posY;
    float *velX;
    float *velY;
} Particles;

typedef struct
{
    int start;
    int end;
    Particles *particles;
    Vector2 mousePos;
} ThreadArgs;

Particles CreateParticles(int count, int screenWidth, int screenHeight);
void UpdateParticlesMultithreaded(Particles *particles, Vector2 mousePos);
void FreeParticles(Particles *particles);
void HandleKeys(void);
void UpdateOffScreenBufferWithParticles(Particles *particles, Texture2D *offScreenBuffer);

static float globalAttractionStrength = ATTRACTION_STRENGHT;
static float globalFriction = FRICTION;

int main()
{
    assert(PARTICLE_COUNT % 4 == 0 && "Particle count must be a multiple of 4 for SIMD operations");

    // Initialize the screen width and height to the monitor's size
    int screenWidth = GetMonitorWidth(0);
    int screenHeight = GetMonitorHeight(0);

    // Initialize the Raylib window
    InitWindow(screenWidth, screenHeight, "Particle Effect in C");

    // Hide the window's border to simulate fullscreen windowed mode
    // Note: As of my last update, you might need to check if Raylib has introduced a direct function or flag for this
    ToggleFullscreen(); // This can make it fullscreen, but it might not be exactly what you're looking for in "windowed fullscreen"
    SetWindowState(FLAG_FULLSCREEN_MODE);
    //SetWindowState(FLAG_WINDOW_UNDECORATED); // Hides the window border
    //SetWindowState(FLAG_WINDOW_RESIZABLE);   // Optional: Make the window resizable
    // Set the window position to zero to ensure it fills the entire screen
    SetWindowPosition(0, 0);

    Image particleImage = GenImageColor(SCREEN_WIDTH, SCREEN_HEIGHT, BLANK);
    Texture2D offScreenBuffer = LoadTextureFromImage(particleImage);

    UnloadImage(particleImage); // Unload the image once texture is created

    Particles particles = CreateParticles(PARTICLE_COUNT, SCREEN_WIDTH, SCREEN_HEIGHT);

    SetTargetFPS(TARGET_FPS);

    while (!WindowShouldClose())
    {
        HandleKeys();

        Vector2 mousePos = GetMousePosition();
        UpdateParticlesMultithreaded(&particles, mousePos);
        UpdateOffScreenBufferWithParticles(&particles, &offScreenBuffer);

        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawTexture(offScreenBuffer, 0, 0, WHITE);
        DrawFPS(10, 10);
        EndDrawing();
    }

    FreeParticles(&particles);
    CloseWindow();
    return 0;
}

Particles CreateParticles(int count, int screenWidth, int screenHeight)
{
    Particles p;
    p.posX = (float *)_aligned_malloc(count * sizeof(float), ALIGNMENT);
    p.posY = (float *)_aligned_malloc(count * sizeof(float), ALIGNMENT);
    p.velX = (float *)_aligned_malloc(count * sizeof(float), ALIGNMENT);
    p.velY = (float *)_aligned_malloc(count * sizeof(float), ALIGNMENT);

    for (int i = 0; i < count; i++)
    {
        p.posX[i] = GetRandomValue(0, screenWidth - 1);
        p.posY[i] = GetRandomValue(0, screenHeight - 1);
        p.velX[i] = GetRandomValue(-100, 100) / 100.0f;
        p.velY[i] = GetRandomValue(-100, 100) / 100.0f;
    }

    return p;
}

DWORD WINAPI UpdateParticlesSubset(LPVOID param)
{
    ThreadArgs *threadArgs = (ThreadArgs *)param;
    Particles *particles = threadArgs->particles;
    Vector2 mousePos = threadArgs->mousePos;

    for (int i = threadArgs->start; i < threadArgs->end; i += 4)
    {
        __m256 posX = _mm256_load_ps(&particles->posX[i]);
        __m256 posY = _mm256_load_ps(&particles->posY[i]);
        __m256 velX = _mm256_load_ps(&particles->velX[i]);
        __m256 velY = _mm256_load_ps(&particles->velY[i]);

        // Compute differences using AVX instructions
        __m256 mouseX = _mm256_set1_ps(mousePos.x);
        __m256 mouseY = _mm256_set1_ps(mousePos.y);
        __m256 diffX = _mm256_sub_ps(mouseX, posX);
        __m256 diffY = _mm256_sub_ps(mouseY, posY);

        // Compute distance squared and distance using AVX
        __m256 distSq = _mm256_add_ps(_mm256_mul_ps(diffX, diffX), _mm256_mul_ps(diffY, diffY));
        __m256 dist = _mm256_sqrt_ps(distSq);

        // Normalize diff vector (ensure you handle division by zero appropriately)
        __m256 normX = _mm256_div_ps(diffX, dist);
        __m256 normY = _mm256_div_ps(diffY, dist);

        // Compute attraction force using AVX
        __m256 attraction = _mm256_set1_ps(globalAttractionStrength);
        velX = _mm256_add_ps(velX, _mm256_mul_ps(normX, attraction));
        velY = _mm256_add_ps(velY, _mm256_mul_ps(normY, attraction));

        // Apply friction using AVX
        __m256 friction = _mm256_set1_ps(globalFriction);
        velX = _mm256_mul_ps(velX, friction);
        velY = _mm256_mul_ps(velY, friction);

        // Update positions using AVX
        posX = _mm256_add_ps(posX, velX);
        posY = _mm256_add_ps(posY, velY);

        // Store updated positions and velocities back
        _mm256_store_ps(&particles->posX[i], posX);
        _mm256_store_ps(&particles->posY[i], posY);
        _mm256_store_ps(&particles->velX[i], velX);
        _mm256_store_ps(&particles->velY[i], velY);
    }
    return 0;
}

void UpdateParticlesMultithreaded(Particles *particles, Vector2 mousePos)
{
    const int threadCount = MAX_THREADS;
    HANDLE threads[threadCount];
    ThreadArgs args[threadCount];

    int particlesPerThread = PARTICLE_COUNT / threadCount;

    for (int i = 0; i < threadCount; i++)
    {
        args[i].start = i * particlesPerThread;
        args[i].end = (i + 1) * particlesPerThread;
        args[i].particles = particles;
        args[i].mousePos = mousePos;

        threads[i] = CreateThread(NULL, 0, UpdateParticlesSubset, &args[i], 0, NULL);
        if (threads[i] == NULL)
        {
            // Handle thread creation error
        }
    }

    for (int i = 0; i < threadCount; i++)
    {
        WaitForSingleObject(threads[i], INFINITE);
        CloseHandle(threads[i]);
    }
}

void FreeParticles(Particles *particles)
{
    _aligned_free(particles->posX);
    _aligned_free(particles->posY);
    _aligned_free(particles->velX);
    _aligned_free(particles->velY);
}

void UpdateOffScreenBufferWithParticles(Particles *particles, Texture2D *offScreenBuffer)
{
    Color *pixels = (Color *)malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(Color));

    // Initialize pixel data
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
    {
        pixels[i] = BLANK;
    }

    // Set particle pixels
    for (int i = 0; i < PARTICLE_COUNT; i++)
    {
        int x = (int)particles->posX[i];
        int y = (int)particles->posY[i];

        if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT)
        {
            pixels[y * SCREEN_WIDTH + x] = PARTICLE_COLOR;
        }
    }

    // Update the texture with new pixel data
    UpdateTexture(*offScreenBuffer, pixels);

    free(pixels);
}

void HandleKeys(void)
{
    if (IsKeyPressed(KEY_UP))
    {
        globalAttractionStrength += 0.0005f;
        TraceLog(LOG_INFO, "Attraction: %.4f, Friction: %.4f", globalAttractionStrength, globalFriction);
    }
    if (IsKeyPressed(KEY_DOWN))
    {
        globalAttractionStrength -= 0.0005f;
        TraceLog(LOG_INFO, "Attraction: %.4f, Friction: %.4f", globalAttractionStrength, globalFriction);
    }
    if (IsKeyPressed(KEY_RIGHT))
    {
        globalFriction += 0.0005f;
        TraceLog(LOG_INFO, "Attraction: %.4f, Friction: %.4f", globalAttractionStrength, globalFriction);
    }
    if (IsKeyPressed(KEY_LEFT))
    {
        globalFriction -= 0.0005f;
        TraceLog(LOG_INFO, "Attraction: %.4f, Friction: %.4f", globalAttractionStrength, globalFriction);
    }
}
