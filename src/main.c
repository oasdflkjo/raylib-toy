#define RAYLIB_CUSTOM_RAYLIB_H
#include "raylib.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#define Rectangle _Rectangle
#define CloseWindow _CloseWindow
#define ShowCursor _ShowCursor
#include <windows.h>
#undef Rectangle
#undef CloseWindow
#undef ShowCursor

#include <stdlib.h>
#include <math.h>

#define PARTICLE_COUNT 100000
#define MAX_THREADS 64

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

typedef struct Particle {
    Vector2 pos;
    Vector2 vel;
    Color color;
} Particle;

typedef struct ThreadData {
    int start;
    int end;
    Particle* particles;
    Vector2 mousePos;
} ThreadData;

Particle CreateParticle(int screenWidth, int screenHeight);
void UpdateParticles(void* data);
void Attract(Particle* p, Vector2 toPos);
void ApplyFriction(Particle* p, float amount);
void MoveParticle(Particle* p, int screenWidth, int screenHeight);
void DrawParticle(Particle p);

const int screenWidth = 800;
const int screenHeight = 800;

int main() {
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    int numThreads = min(sysinfo.dwNumberOfProcessors, MAX_THREADS);

    InitWindow(screenWidth, screenHeight, "Particle Effect in C");

    Particle* particles = (Particle*) malloc(PARTICLE_COUNT * sizeof(Particle));
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        particles[i] = CreateParticle(screenWidth, screenHeight);
    }

    HANDLE threadHandles[MAX_THREADS];
    ThreadData threadData[MAX_THREADS];

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        Vector2 mousePos = GetMousePosition();

        int chunkSize = PARTICLE_COUNT / numThreads;
        for (int i = 0; i < numThreads; i++) {
            threadData[i].start = i * chunkSize;
            threadData[i].end = (i == numThreads - 1) ? PARTICLE_COUNT : (i + 1) * chunkSize;
            threadData[i].particles = particles;
            threadData[i].mousePos = mousePos;

            threadHandles[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)UpdateParticles, &threadData[i], 0, NULL);
        }

        WaitForMultipleObjects(numThreads, threadHandles, TRUE, INFINITE);

        for (int i = 0; i < numThreads; i++) {
            CloseHandle(threadHandles[i]);
        }

        BeginDrawing();
            ClearBackground(RAYWHITE);
            for (int i = 0; i < PARTICLE_COUNT; i++) {
                DrawParticle(particles[i]);
            }
            DrawFPS(10, 10);
        EndDrawing();
    }

    free(particles);
    CloseWindow(); // Use the Raylib function with the custom naming if required

    return 0;
}

Particle CreateParticle(int screenWidth, int screenHeight) {
    Particle p;
    p.pos.x = GetRandomValue(0, screenWidth - 1);
    p.pos.y = GetRandomValue(0, screenHeight - 1);
    p.vel.x = GetRandomValue(-100, 100) / 100.0f;
    p.vel.y = GetRandomValue(-100, 100) / 100.0f;
    p.color = (Color){ 0, 0, 0, 100 };
    return p;
}

void UpdateParticles(void* data) {
    ThreadData* threadData = (ThreadData*)data;
    for (int i = threadData->start; i < threadData->end; i++) {
        Attract(&threadData->particles[i], threadData->mousePos);
        ApplyFriction(&threadData->particles[i], 0.99f);
        MoveParticle(&threadData->particles[i], screenWidth, screenHeight);
    }
}

void Attract(Particle* p, Vector2 toPos) {
    Vector2 diff = { toPos.x - p->pos.x, toPos.y - p->pos.y };
    float distance = sqrtf(diff.x * diff.x + diff.y * diff.y);
    distance = (distance < 0.5f) ? 0.5f : distance; // Prevent division by zero

    Vector2 norm = { diff.x / distance, diff.y / distance };

    p->vel.x += norm.x / distance;
    p->vel.y += norm.y / distance;
}

void ApplyFriction(Particle* p, float amount) {
    p->vel.x *= amount;
    p->vel.y *= amount;
}

void MoveParticle(Particle* p, int screenWidth, int screenHeight) {
    p->pos.x += p->vel.x;
    p->pos.y += p->vel.y;

    if (p->pos.x < 0) p->pos.x += screenWidth;
    else if (p->pos.x >= screenWidth) p->pos.x -= screenWidth;

    if (p->pos.y < 0) p->pos.y += screenHeight;
    else if (p->pos.y >= screenHeight) p->pos.y -= screenHeight;
}

void DrawParticle(Particle p) {
    DrawPixelV(p.pos, p.color);
}
