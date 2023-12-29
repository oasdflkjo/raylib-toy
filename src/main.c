// make clean includes cos this hacking to avoid name collitions

#define RAYLIB_CUSTOM_RAYLIB_H
#include "raylib.h"



#define Rectangle _Rectangle
#define CloseWindow _CloseWindow
#define ShowCursor _ShowCursor
#include <windows.h>
#undef Rectangle
#undef CloseWindow
#undef ShowCursor

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <xmmintrin.h>

#define PARTICLE_COUNT 120000
#define PARTICLE_COLOR (Color){0, 0, 0, 255}
#define MAX_THREADS 12
#define ALIGNMENT 16
#define FRAME_HISTORY_COUNT 10
#define TARGET_FPS 160
#define ATTRACTION_STRENGHT 0.2000f
#define FRICTION 0.999f
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 800

typedef struct Particles
{
    float *posX;
    float *posY;
    float *velX;
    float *velY;
} Particles;

typedef struct {
    int start;
    int end;
    Particles *particles;
    Vector2 mousePos;
} ThreadArgs;


Particles CreateParticles(int count, int screenWidth, int screenHeight);
void UpdateParticlesMultithreaded(Particles *particles, Vector2 mousePos);
void DrawParticles(Particles *particles, int count);
void FreeParticles(Particles *particles);

unsigned char drawnPixels[SCREEN_WIDTH * SCREEN_HEIGHT] = {0};
static float globalAttractionStrength = ATTRACTION_STRENGHT;
static float globalFriction = FRICTION;

int main()
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Particle Effect in C");

    double frameTimes[FRAME_HISTORY_COUNT] = {0};
    int frameCount = 0;
    Particles particles = CreateParticles(PARTICLE_COUNT, SCREEN_WIDTH, SCREEN_HEIGHT);

    SetTargetFPS(TARGET_FPS);

    while (!WindowShouldClose())
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
        double startTime = GetTime();

        Vector2 mousePos = GetMousePosition();
        UpdateParticlesMultithreaded(&particles, mousePos);

        double endTime = GetTime();
        frameTimes[frameCount % FRAME_HISTORY_COUNT] = endTime - startTime;
        frameCount++;

        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawParticles(&particles, PARTICLE_COUNT);
        DrawFPS(10, 10);
        char infoText[128];
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

DWORD WINAPI UpdateParticlesSubset(LPVOID param) {
    ThreadArgs *threadArgs = (ThreadArgs *)param;
    Particles *particles = threadArgs->particles;
    Vector2 mousePos = threadArgs->mousePos;

    for (int i = threadArgs->start; i < threadArgs->end; i += 4) {
        // Load positions and velocities of 4 particles into SIMD registers
        __m128 posX = _mm_load_ps(&particles->posX[i]);
        __m128 posY = _mm_load_ps(&particles->posY[i]);
        __m128 velX = _mm_load_ps(&particles->velX[i]);
        __m128 velY = _mm_load_ps(&particles->velY[i]);

        // Load mouse position into SIMD registers
        __m128 mouseX = _mm_set1_ps(mousePos.x);
        __m128 mouseY = _mm_set1_ps(mousePos.y);

        // Compute differences
        __m128 diffX = _mm_sub_ps(mouseX, posX);
        __m128 diffY = _mm_sub_ps(mouseY, posY);

        // Compute distance squared and distance
        __m128 distSq = _mm_add_ps(_mm_mul_ps(diffX, diffX), _mm_mul_ps(diffY, diffY));
        __m128 dist = _mm_sqrt_ps(distSq);

        // Normalize diff vector
        __m128 normX = _mm_div_ps(diffX, dist);
        __m128 normY = _mm_div_ps(diffY, dist);

        // Compute attraction force (this is a basic example, you might want a more complex calculation)
        __m128 attraction = _mm_set1_ps(globalAttractionStrength); // Attraction strength
        velX = _mm_add_ps(velX, _mm_mul_ps(normX, attraction));
        velY = _mm_add_ps(velY, _mm_mul_ps(normY, attraction));

        // Apply friction
        __m128 friction = _mm_set1_ps(globalFriction);
        velX = _mm_mul_ps(velX, friction);
        velY = _mm_mul_ps(velY, friction);

        // Update positions
        posX = _mm_add_ps(posX, velX);
        posY = _mm_add_ps(posY, velY);

        // Store updated positions and velocities back
        _mm_store_ps(&particles->posX[i], posX);
        _mm_store_ps(&particles->posY[i], posY);
        _mm_store_ps(&particles->velX[i], velX);
        _mm_store_ps(&particles->velY[i], velY);
    }
}

void UpdateParticlesMultithreaded(Particles *particles, Vector2 mousePos) {
    const int threadCount = MAX_THREADS;
    HANDLE threads[threadCount];
    ThreadArgs args[threadCount];

    int particlesPerThread = PARTICLE_COUNT / threadCount;

    for (int i = 0; i < threadCount; i++) {
        args[i].start = i * particlesPerThread;
        args[i].end = (i + 1) * particlesPerThread;
        args[i].particles = particles;
        args[i].mousePos = mousePos;

        threads[i] = CreateThread(NULL, 0, UpdateParticlesSubset, &args[i], 0, NULL);
        if (threads[i] == NULL) {
            // Handle thread creation error
        }
    }

    for (int i = 0; i < threadCount; i++) {
        WaitForSingleObject(threads[i], INFINITE);
        CloseHandle(threads[i]);
    }
}

// TODO: filter out particles outside the window
void DrawParticles(Particles *particles, int count) {
    for (int i = 0; i < count; i++) {
        int x = (int)particles->posX[i];
        int y = (int)particles->posY[i];

        // Check bounds and if pixel is already drawn
        if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT &&
            !drawnPixels[y * SCREEN_WIDTH + x]) {
            
            DrawRectangle(x, y, 2, 2, PARTICLE_COLOR);

            // Mark these pixels as drawn
            drawnPixels[y * SCREEN_WIDTH + x] = 1;
            if (x + 1 < SCREEN_WIDTH) {
                drawnPixels[y * SCREEN_WIDTH + (x + 1)] = 1;
            }
            if (y + 1 < SCREEN_HEIGHT) {
                drawnPixels[(y + 1) * SCREEN_WIDTH + x] = 1;
                if (x + 1 < SCREEN_WIDTH) {
                    drawnPixels[(y + 1) * SCREEN_WIDTH + (x + 1)] = 1;
                }
            }
        }
    }

    // Reset drawnPixels for the next frame
    memset(drawnPixels, 0, sizeof(drawnPixels));
}

void FreeParticles(Particles *particles)
{
    _aligned_free(particles->posX);
    _aligned_free(particles->posY);
    _aligned_free(particles->velX);
    _aligned_free(particles->velY);
}
