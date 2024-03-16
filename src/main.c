/* ========================================================================= */
/*                             Header Includes                               */
/* ========================================================================= */
#include "raylib.h"
#define Rectangle _Rectangle
#define CloseWindow _CloseWindow
#define ShowCursor _ShowCursor
#define DrawText _DrawText
#include <windows.h>
#undef Rectangle
#undef CloseWindow
#undef ShowCursor
#undef DrawText
// workaround for naming conflicts

#include <immintrin.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
/* ========================================================================= */
/*                            Defines                                        */
/* ========================================================================= */
#define PARTICLE_COUNT 5760000 // 360000 // 720000 // 1440000 // 2880000 // 5760000
#define MAX_THREADS 12
#define ALIGNMENT 32
#define TARGET_FPS 160
#define ATTRACTION_STRENGHT 0.2000f
#define FRICTION 0.999f
#define SCREEN_WIDTH 3440
#define SCREEN_HEIGHT 1440
#define MY_BLACK 0x000000FF
#define MY_GRAY 0x808080FF

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
    int count;
} Particles;

typedef struct UpdateContext
{
    BOOL *buffer;         // Pointer to the boolean buffer
    Particles *particles; // Pointer to the particle data
    int start;            // Start index of particles for this update
    int end;              // End index of particles for this update
    int bufferWidth;      // Width of the buffer (to calculate positions)
    int bufferHeight;     // Height of the buffer
} UpdateContext;

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

/**
 * @brief Creates a Particles structure and initializes the particle positions and velocities.
 *
 * @param count The number of particles to create.
 * @param screenWidth The width of the screen to use as the maximum x position for the particles.
 * @param screenHeight The height of the screen to use as the maximum y position for the particles.
 * @return Particles The Particles structure containing the particle positions and velocities.
 */
Particles CreateParticlesSIMD(int count, int screenWidth, int screenHeight);

/**
 * @brief Updates the positions and velocities of the particles in a multithreaded environment.
 *
 * @param particles A pointer to the Particles structure containing the particle positions and velocities.
 * @param mousePos The current mouse position to use as an attraction point for the particles.
 */
void UpdateParticlesMultithreaded(Particles *particles, Vector2 mousePos);

/**
 * @brief Frees the memory allocated for the Particles structure.
 *
 * @param particles A pointer to the Particles structure to free.
 */
void FreeParticles(Particles *particles);

/**
 * @brief Updates the off-screen buffer with the current particle positions.
 *
 * @param particles A pointer to the Particles structure containing the particle positions.
 * @param pixels A pointer to the off-screen buffer to update with the particle positions.
 */
inline void UpdateOffScreenBufferWithParticles(Particles *particles, Color *pixels);

/**
 * @brief Callback function for updating particle positions and velocities in a multithreaded environment.
 *
 * This function is called by a thread pool work item to update a subset of particles
 * based on their current positions, velocities, and the influence of an external force
 * (e.g., attraction to a point, simulated by mouse position). It utilizes AVX2 instructions
 * for SIMD (Single Instruction, Multiple Data) processing to efficiently compute the new
 * state of each particle in the subset. The calculations include computing the distance
 * to the attraction point, applying an attraction force, and adjusting for friction.
 *
 * @param Instance Unused. A pointer to a TP_CALLBACK_INSTANCE structure that defines the callback instance.
 * @param Context A pointer to user-defined data passed to the function. This should be a pointer
 *                to a ThreadArgs structure containing information about the particles to update,
 *                the range of particles this callback is responsible for, and the current mouse position.
 * @param Work Unused. A pointer to a TP_WORK structure that represents the work item that generated the callback.
 *
 * @note This function is designed to be used with the Windows Thread Pool API and expects
 * the Context parameter to be of type (ThreadArgs*).
 *
 * @warning Ensure AVX2 support is available on the executing hardware to avoid runtime issues.
 */
void CALLBACK UpdateParticlesWorkCallback(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work);

/**
 * @brief Sets the color of a buffer using SIMD instructions.
 *
 * This function sets the color of a buffer using SIMD instructions to efficiently process
 * multiple pixels at once. It utilizes AVX2 instructions for SIMD (Single Instruction, Multiple Data)
 * processing to efficiently set the color of a buffer to a specified value. The function processes
 * the buffer in chunks of 8 pixels at a time, which is the optimal size for AVX2 processing.
 *
 * @param pixels A pointer to the buffer to set the color of.
 * @param count The number of pixels in the buffer.
 * @param color The color to set the buffer to.
 */
void InitializeThreadPoolWithMaxThreads(PTP_POOL *pool, PTP_CALLBACK_ENVIRON callBackEnviron, DWORD maxThreads);

void UpdateBufferWithParticles(BOOL *buffer, Particles *particles, int start, int end, int bufferWidth, int bufferHeight)
{
    // Initialize buffer to false, representing an empty or "black" state
    for (int i = 0; i < bufferWidth * bufferHeight; i++)
    {
        buffer[i] = false; // No particle present at this position
    }

    // Update buffer with particles
    for (int i = start; i < end; i++)
    {
        int x = (int)particles->posX[i];
        int y = (int)particles->posY[i];

        // Ensure the particle is within the bounds of the buffer
        if (x >= 0 && x < bufferWidth && y >= 0 && y < bufferHeight)
        {
            int index = y * bufferWidth + x;
            buffer[index] = true; // Particle present at this position
        }
    }
}

void CALLBACK UpdateBufferWorkCallback(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
{
    // Explicitly mark unused parameters to avoid compiler warnings
    (void)Instance;
    (void)Work;

    // Convert context to our structure that holds necessary information
    UpdateContext *updateContext = (UpdateContext *)Context;

    // Call the updated function that works with a boolean buffer
    UpdateBufferWithParticles(updateContext->buffer, updateContext->particles,
                              updateContext->start, updateContext->end,
                              updateContext->bufferWidth, updateContext->bufferHeight);
}

void UpdateBufferWithParticlesBitBuffer(int *bitBuffer, Particles *particles, int start, int end, int bufferWidth, int bufferHeight)
{
    // Calculate the size of the bit buffer
    int bitBufferSize = (bufferWidth * bufferHeight + 31) / 32; // +31 to round up to the nearest multiple of 32 bits

    // Initialize bit buffer to 0, representing an empty or "black" state
    memset(bitBuffer, 0, bitBufferSize * sizeof(int));

    // Update bit buffer with particles
    for (int i = start; i < end; i++)
    {
        int x = (int)particles->posX[i];
        int y = (int)particles->posY[i];

        // Ensure the particle is within the bounds of the buffer
        if (x >= 0 && x < bufferWidth && y >= 0 && y < bufferHeight)
        {
            int index = y * bufferWidth + x; // Calculate the linear index
            int bitIndex = index % 32;       // Find the bit index within the integer
            int intIndex = index / 32;       // Find the integer index within the array

            bitBuffer[intIndex] |= (1 << bitIndex); // Set the bit to indicate a particle is present
        }
    }
}

void CombineBuffersSIMD(int *bufferA, int *bufferB, int *finalBuffer, int bufferSize)
{
    // Process 8 integers at a time with AVX2
    for (int i = 0; i <= bufferSize - 8; i += 8)
    {
        __m256i vA = _mm256_load_si256((__m256i *)&bufferA[i]); // Load 8 integers from bufferA
        __m256i vB = _mm256_load_si256((__m256i *)&bufferB[i]); // Load 8 integers from bufferB
        // compute with OR
        __m256i a = _mm256_or_si256(vA, vB);     // OR
        //__m256i a = _mm256_xor_si256(vA, vB);    // XOR
        //__m256i a = _mm256_and_si256(vA, vB);    // AND         
        _mm256_store_si256((__m256i *)&finalBuffer[i], a); // Store the result
    }
}

// Helper function to pack a Color
static inline uint32_t PackColor(Color color)
{
    return color.r | (color.g << 8) | (color.b << 16) | (color.a << 24);
}

void ConvertBoolToPixelsSIMD(const int *finalBuffer, Color *pixels, int bufferSize)
{
    // Define colors in packed format
    uint32_t packedBlack = PackColor((Color){0, 0, 0, 255});
    uint32_t packedGray = PackColor((Color){130, 130, 130, 255});

    __m256i vBlack = _mm256_set1_epi32(packedBlack);
    __m256i vGray = _mm256_set1_epi32(packedGray);

    for (int i = 0; i <= bufferSize - 8; i += 8)
    {
        __m256i mask = _mm256_loadu_si256((__m256i *)&finalBuffer[i]);    // Load 8 bools (as ints)
        __m256i vMask = _mm256_cmpeq_epi32(mask, _mm256_setzero_si256()); // Compare mask elements to 0
        __m256i vResult = _mm256_blendv_epi8(vBlack, vGray, vMask);       // Blend pixels based on mask
        _mm256_storeu_si256((__m256i *)&pixels[i], vResult);              // Store the result
    }
}

/* ========================================================================= */
/*                            Main                                           */
/* ========================================================================= */
int main(void)
{
    assert(PARTICLE_COUNT % 8 == 0 && "Particle count must be a multiple of 8 for AVX2 processing!");

    // Initialize the thread pool environment and set the maximum number of threads.
    PTP_POOL pool;
    InitializeThreadPoolWithMaxThreads(&pool, &CallBackEnviron, 12);

    // Initialize the screen width and height to the monitor's size
    int screenWidth = GetMonitorWidth(0);
    int screenHeight = GetMonitorHeight(0);
    InitWindow(screenWidth, screenHeight, "Particle System");
    SetWindowState(FLAG_FULLSCREEN_MODE);
    SetTargetFPS(TARGET_FPS);
    InitAudioDevice(); // Initialize audio device
    Music music = LoadMusicStream("midnight-forest-184304.mp3");
    PlayMusicStream(music);
    int centerX = SCREEN_WIDTH / 2;
    int centerY = SCREEN_HEIGHT / 2;
    HideCursor(); // Hide the system cursor
    SetMousePosition(centerX, centerY);
    Color redDotColor = RED;
    float redDotRadius = 5.0f; // Size of the red dot
    RenderTexture2D mainBuffer = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
    Color *pixels = (Color *)malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(Color));
    // Allocate aligned memory for the buffers
    BOOL *bufferA = (BOOL *)_aligned_malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(BOOL), 32);
    BOOL *bufferB = (BOOL *)_aligned_malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(BOOL), 32);
    BOOL *finalBuffer = (BOOL *)_aligned_malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(BOOL), 32);

    memset(bufferA, 0, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(BOOL));
    memset(bufferB, 0, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(BOOL));
    memset(finalBuffer, 0, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(BOOL));

    Particles particles = CreateParticlesSIMD(PARTICLE_COUNT, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Define and initialize update contexts for each buffer
    UpdateContext updateContextA = {
        .buffer = bufferA,            // Target buffer for the first half of particles
        .particles = &particles,      // Pointer to the particle data
        .start = 0,                   // Starting index of particles for the first update context
        .end = PARTICLE_COUNT / 2,    // End index (process the first half of particles)
        .bufferWidth = SCREEN_WIDTH,  // Width of the buffer
        .bufferHeight = SCREEN_HEIGHT // Height of the buffer
    };

    UpdateContext updateContextB = {
        .buffer = bufferB,            // Target buffer for the second half of particles
        .particles = &particles,      // Pointer to the same particle data
        .start = PARTICLE_COUNT / 2,  // Starting index for the second half
        .end = PARTICLE_COUNT,        // Process to the end of the particle array
        .bufferWidth = SCREEN_WIDTH,  // Width of the buffer
        .bufferHeight = SCREEN_HEIGHT // Height of the buffer
    };

    // Initialize your particle system here
//#define PROFILING
#ifdef PROFILING
    while (!WindowShouldClose())
    {
        double startTime = GetTime();

        // Particles update
        double particlesStartTime = GetTime();
        Vector2 mousePos = GetMousePosition();
        UpdateParticlesMultithreaded(&particles, mousePos);
        TraceLog(LOG_INFO, "Particles update took %f seconds", GetTime() - particlesStartTime);

        // Threadpool work
        double threadPoolStartTime = GetTime();
        PTP_WORK workItemA = CreateThreadpoolWork(UpdateBufferWorkCallback, &updateContextA, &CallBackEnviron);
        PTP_WORK workItemB = CreateThreadpoolWork(UpdateBufferWorkCallback, &updateContextB, &CallBackEnviron);
        SubmitThreadpoolWork(workItemA);
        SubmitThreadpoolWork(workItemB);
        WaitForThreadpoolWorkCallbacks(workItemA, FALSE);
        WaitForThreadpoolWorkCallbacks(workItemB, FALSE);
        CloseThreadpoolWork(workItemA);
        CloseThreadpoolWork(workItemB);
        TraceLog(LOG_INFO, "Threadpool work took %f seconds", GetTime() - threadPoolStartTime);

        // Buffers combination and conversion
        double bufferStartTime = GetTime();
        CombineBuffersSIMD(bufferA, bufferB, finalBuffer, SCREEN_WIDTH * SCREEN_HEIGHT);
        ConvertBoolToPixelsSIMD(finalBuffer, pixels, SCREEN_WIDTH * SCREEN_HEIGHT);
        TraceLog(LOG_INFO, "Buffers combination and conversion took %f seconds", GetTime() - bufferStartTime);

        // Texture update and drawing
        double drawingStartTime = GetTime();
        UpdateTexture(mainBuffer.texture, pixels);
        BeginDrawing();
        DrawTexture(mainBuffer.texture, 0, 0, WHITE);
        DrawCircleV(mousePos, redDotRadius, redDotColor);
        EndDrawing();
        TraceLog(LOG_INFO, "Texture update and drawing took %f seconds", GetTime() - drawingStartTime);

        TraceLog(LOG_INFO, "Total loop iteration took %f seconds", GetTime() - startTime);
    }
#else
    while (!WindowShouldClose())
    {
        UpdateMusicStream(music);
        Vector2 mousePos = GetMousePosition();
        UpdateParticlesMultithreaded(&particles, mousePos);

        // update off-screen buffers with particles
        PTP_WORK workItemA = CreateThreadpoolWork(UpdateBufferWorkCallback, &updateContextA, &CallBackEnviron);
        PTP_WORK workItemB = CreateThreadpoolWork(UpdateBufferWorkCallback, &updateContextB, &CallBackEnviron);
        SubmitThreadpoolWork(workItemA);
        SubmitThreadpoolWork(workItemB);
        WaitForThreadpoolWorkCallbacks(workItemA, FALSE);
        WaitForThreadpoolWorkCallbacks(workItemB, FALSE);
        CloseThreadpoolWork(workItemA);
        CloseThreadpoolWork(workItemB);

        // combine buffers and convert to pixels
        CombineBuffersSIMD(bufferA, bufferB, finalBuffer, SCREEN_WIDTH * SCREEN_HEIGHT);
        ConvertBoolToPixelsSIMD(finalBuffer, pixels, SCREEN_WIDTH * SCREEN_HEIGHT);

        // update texture and draw
        UpdateTexture(mainBuffer.texture, pixels);
        BeginDrawing();
        DrawTexture(mainBuffer.texture, 0, 0, WHITE);
        DrawCircleV(mousePos, redDotRadius, redDotColor);
        DrawFPS(10, 10);
        EndDrawing();
    }
#endif
    FreeParticles(&particles);

    // Close the window and clean up resources
    CloseWindow();

    DestroyThreadpoolEnvironment(&CallBackEnviron);
    if (pool)
    {
        CloseThreadpool(pool);
    }

    return 0;
}

/* ========================================================================= */
/*                            Private functions                              */
/* ========================================================================= */
void setBufferColorSIMD(Color *pixels, int count, Color color)
{
    // Assuming Color is a struct of 4 bytes (RGBA)
    int packedColor = color.r | (color.g << 8) | (color.b << 16) | (color.a << 24);

    // Create an AVX2 vector with 8 packed color integers (since we're dealing
    // with 32-bit ints, and AVX2 can handle 256 bits at a time)
    __m256i packedColors = _mm256_set1_epi32(packedColor);

    // Process 8 pixels per iteration
    for (int i = 0; i < count; i += 8)
    {
        _mm256_storeu_si256((__m256i *)(pixels + i), packedColors);
    }
}

inline void UpdateOffScreenBufferWithParticles(Particles *particles, Color *pixels)
{
    // Clear the off-screen buffer to black
    setBufferColorSIMD(pixels, SCREEN_WIDTH * SCREEN_HEIGHT, GRAY);

    for (int i = 0; i < PARTICLE_COUNT; i++)
    {
        if (particles->posX[i] >= 0 && particles->posX[i] < SCREEN_WIDTH && particles->posY[i] >= 0 && particles->posY[i] < SCREEN_HEIGHT)
        {
            int index = (int)particles->posY[i] * SCREEN_WIDTH + (int)particles->posX[i];
            pixels[index] = BLACK;
        }
    }
}

Particles CreateParticlesSIMD(int count, int screenWidth, int screenHeight)
{
    Particles p;
    p.count = count;
    p.posX = (float *)_aligned_malloc(count * sizeof(float), 32);
    p.posY = (float *)_aligned_malloc(count * sizeof(float), 32);
    p.velX = (float *)_aligned_malloc(count * sizeof(float), 32);
    p.velY = (float *)_aligned_malloc(count * sizeof(float), 32);

    // Place particles in a scanline manner, starting from the top-left pixel.
    // Continue "below" the screen if there are more particles than fit on the screen.
    for (int i = 0; i < count; ++i)
    {
        int x = i % screenWidth;
        int y = i / screenWidth;

        p.posX[i] = (float)x;
        p.posY[i] = (float)y;

        // Initialize velocity to zero or a small random value for initial movement
        p.velX[i] = 0.0f;
        p.velY[i] = 0.0f;
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
    // Explicitly mark unused parameters to avoid compiler warnings
    (void)Instance;
    (void)Work;
    ThreadArgs *args = (ThreadArgs *)Context;

    Particles *particles = args->particles;
    Vector2 mousePos = args->mousePos;

    // Process in chunks of 8 for AVX2
    int i = args->start;
    for (; i + 7 < args->end; i += 8)
    {
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

void InitializeThreadPoolWithMaxThreads(PTP_POOL *pool, PTP_CALLBACK_ENVIRON callBackEnviron, DWORD maxThreads)
{
    // Create a new thread pool.
    *pool = CreateThreadpool(NULL);
    if (!*pool)
    {
        // Handle error.
        return;
    }

    // Set the thread pool's minimum and maximum thread counts.
    // Note: Setting the minimum is required before setting the maximum.
    SetThreadpoolThreadMinimum(*pool, 1); // Ensure at least one thread.
    SetThreadpoolThreadMaximum(*pool, maxThreads);

    // Initialize the callback environment.
    InitializeThreadpoolEnvironment(callBackEnviron);

    // Associate the thread pool with the callback environment.
    SetThreadpoolCallbackPool(callBackEnviron, *pool);
}
