/* ========================================================================= */
/*                             Header Includes                               */
/* ========================================================================= */
#include "raylib.h"
#define Rectangle _Rectangle
#define CloseWindow _CloseWindow
#define ShowCursor _ShowCursor
#include <windows.h>
#undef Rectangle
#undef CloseWindow
#undef ShowCursor
// workaround for naming conflicts

#include <immintrin.h>
#include <assert.h>

#include "../include/threadpool.h"

/* ========================================================================= */
/*                            Defines                                        */
/* ========================================================================= */
#define PARTICLE_COUNT 1440000 // 360000 // 1440000 // 2880000 TODO: crashes if used for example 
#define MAX_THREADS 12
#define ALIGNMENT 16
#define TARGET_FPS 160
#define ATTRACTION_STRENGHT 0.2000f
#define FRICTION 0.999f
#define SCREEN_WIDTH 3440
#define SCREEN_HEIGHT 1440

/* ========================================================================= */
/*                            Global Variables                               */
/* ========================================================================= */
static TP_CALLBACK_ENVIRON CallBackEnviron;

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

/* ========================================================================= */
/*                           Function Prototypes                             */
/* ========================================================================= */
Particles CreateParticles(int count, int screenWidth, int screenHeight);
void UpdateParticlesMultithreaded(Particles *particles, Vector2 mousePos);
void FreeParticles(Particles *particles);
void UpdateOffScreenBufferWithParticles(Particles *particles, Color *pixels);
void CALLBACK UpdateParticlesWorkCallback(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work);

/* ========================================================================= */
/*                            Main                                           */
/* ========================================================================= */
int main(void)
{
    assert(PARTICLE_COUNT % 8 == 0 && "Particle count must be a multiple of 4 for SIMD operations");
   
    // Initialize the screen width and height to the monitor's size
    int screenWidth = GetMonitorWidth(0);
    int screenHeight = GetMonitorHeight(0);
    InitWindow(screenWidth, screenHeight, "Particle System");
    SetWindowState(FLAG_FULLSCREEN_MODE);
    SetTargetFPS(TARGET_FPS);
    RenderTexture2D mainBuffer = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
    Color *pixels = (Color *)malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(Color));

    // Initialize your particle system here
    Particles particles = CreateParticles(PARTICLE_COUNT, SCREEN_WIDTH, SCREEN_HEIGHT);

    while (!WindowShouldClose())
    {
        Vector2 mousePos = GetMousePosition();
        UpdateParticlesMultithreaded(&particles, mousePos);
        UpdateOffScreenBufferWithParticles(&particles, pixels);
        UpdateTexture(mainBuffer.texture, pixels);
        BeginDrawing();
        ClearBackground(GRAY);
        // Draw the texture of the off-screen buffer to the screen.
        DrawTexture(mainBuffer.texture, 0, 0, WHITE);
        DrawFPS(20, 20);
        EndDrawing();
    }

    FreeParticles(&particles);
    CloseWindow();

    return 0;
}

/* ========================================================================= */
/*                            Private functions                              */
/* ========================================================================= */
inline void UpdateOffScreenBufferWithParticles(Particles *particles, Color *pixels)
{
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
    {
        pixels[i] = GRAY;
    }

    for (int i = 0; i < PARTICLE_COUNT; i++)
    {
        int x = (int)particles->posX[i];
        int y = (int)particles->posY[i];

        if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT)
        {
            pixels[y * SCREEN_WIDTH + x] = BLACK;
        }
    }
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

void FreeParticles(Particles *particles)
{
    _aligned_free(particles->posX);
    _aligned_free(particles->posY);
    _aligned_free(particles->velX);
    _aligned_free(particles->velY);
}

void CALLBACK UpdateParticlesWorkCallback(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
{
    // Explicitly mark unused parameters to avoid warnings
    (void)Instance;
    (void)Work;
    ThreadArgs *args = (ThreadArgs *)Context;

    Particles *particles = args->particles;
    Vector2 mousePos = args->mousePos;

    for (int i = args->start; i < args->end; i += 8)
    { // Adjusted for AVX2 processing 8 floats at a time
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
        __m256 attraction = _mm256_set1_ps(ATTRACTION_STRENGHT);
        velX = _mm256_add_ps(velX, _mm256_mul_ps(normX, attraction));
        velY = _mm256_add_ps(velY, _mm256_mul_ps(normY, attraction));

        // Apply friction using AVX
        __m256 friction = _mm256_set1_ps(FRICTION);
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
}

void UpdateParticlesMultithreaded(Particles *particles, Vector2 mousePos)
{
    const int threadCount = MAX_THREADS;
    PTP_WORK workItems[threadCount];
    ThreadArgs args[threadCount];
    int particlesPerThread = PARTICLE_COUNT / threadCount;

    for (int i = 0; i < threadCount; i++)
    {
        args[i].start = i * particlesPerThread;
        args[i].end = (i + 1) * particlesPerThread;
        args[i].particles = particles;
        args[i].mousePos = mousePos;

        workItems[i] = CreateThreadpoolWork(UpdateParticlesWorkCallback, &args[i], &CallBackEnviron);
        SubmitThreadpoolWork(workItems[i]);
    }
    // Wait for all work items to complete
    for (int i = 0; i < threadCount; i++)
    {
        WaitForThreadpoolWorkCallbacks(workItems[i], FALSE);
        CloseThreadpoolWork(workItems[i]);
    }
}
