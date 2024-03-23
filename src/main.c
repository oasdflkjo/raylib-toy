/**
 * @file particle_system.c
 * @brief Particle System Simulation using Raylib and SIMD Instructions.
 *
 * This file contains the implementation of a particle system simulation,
 * utilizing the Raylib library for rendering and the Windows Thread Pool API
 * for multithreading. It demonstrates advanced techniques such as SIMD
 * instructions for efficient computation, dynamic memory allocation for
 * particle data, and the use of boolean buffers for particle state representation.
 * The simulation includes particle attraction to mouse position, friction effects,
 * and rendering optimizations.
 *
 * @author oasdflkjo
 * @date 2024-03-16
 *
 * @see https://www.raylib.com/
 * @see https://docs.microsoft.com/en-us/windows/win32/procthread/using-the-thread-pool-functions
 */

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
#define PARTICLE_COUNT 3096000 // 360000 // 720000 // 1440000 // 2880000 // 5760000 // 4953600 // 2476800
#define MAX_THREADS 12
#define TARGET_FPS 80
#define ATTRACTION_STRENGHT 0.2000f // 0.2000f
#define FRICTION 0.999f             // 0.999f
#define SCREEN_WIDTH 3440
#define SCREEN_HEIGHT 1440

/* ========================================================================= */
/*                            Global Variables                               */
/* ========================================================================= */
//
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
 * @brief Creates a new set of particles with the specified count and screen dimensions.
 *
 * @param count The number of particles to create.
 * @param screenWidth The width of the screen to place the particles on.
 * @param screenHeight The height of the screen to place the particles on.
 *
 * @return A Particles structure containing the particle positions and velocities.
 */
Particles CreateParticles(int count, int screenWidth, int screenHeight);

/**
 * @brief Updates the positions and velocities of the particles in a multithreaded environment.
 *
 * This function updates the positions and velocities of the particles in a multithreaded environment
 * using the Windows Thread Pool API. It creates a thread pool with a maximum number of threads
 * and divides the work of updating the particles among the threads. Each thread updates a subset
 * of the particles based on their current positions, velocities, and the influence of an external force
 * (e.g., attraction to a point, simulated by mouse position). The calculations include computing the
 * distance to the attraction point, applying an attraction force, and adjusting for friction.
 *
 * @param particles A pointer to the Particles structure containing the particle positions.
 * @param mousePos The current position of the mouse, used to simulate an external force.
 */
void UpdateParticlesMultithreaded(Particles *particles, Vector2 mousePos);

/**
 * @brief Frees the memory allocated for the particles.
 *
 * @param particles A pointer to the Particles structure to free.
 */
void FreeParticles(Particles *particles);

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
void UpdateBufferWithParticleDensity(int *buffer, Particles *particles, int start, int end, int bufferWidth, int bufferHeight);

    /**
     * @brief Callback function for updating a boolean buffer with particle positions in a multithreaded environment.
     *
     * This function is called by a thread pool work item to update a boolean buffer with the positions of particles.
     * It sets the buffer to true at the positions of the particles and false at all other positions. The function
     * processes the particles in a specified range and updates the buffer based on their positions. It is designed
     * to be used with the Windows Thread Pool API and expects the Context parameter to be of type (UpdateContext*).
     *
     * @param Instance Unused. A pointer to a TP_CALLBACK_INSTANCE structure that defines the callback instance.
     * @param Context A pointer to user-defined data passed to the function. This should be a pointer to an UpdateContext
     *                structure containing information about the boolean buffer to update, the range of particles this
     *                callback is responsible for, and the dimensions of the buffer.
     * @param Work Unused. A pointer to a TP_WORK structure that represents the work item that generated the callback.
     */
    void CALLBACK UpdateBufferWorkCallback(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work);

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
static inline uint32_t PackColor(Color color);

/**
 * Allocates memory for a 2D boolean buffer with alignment suitable for SIMD operations.
 *
 * @param width The width of the buffer.
 * @param height The height of the buffer.
 * @return A pointer to the allocated buffer or NULL if allocation fails.
 */
BOOL *AllocateAlignedIntBuffer(int width, int height);

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
void CombineBuffersAndConvertToPixelsSIMD(const BOOL *bufferA, const BOOL *bufferB, Color *pixels, int bufferSize);

void ApplyDensityToPixelsSIMD(const int *densityBuffer, Color *pixels, int bufferSize, int maxDensity) {
    for (int i = 0; i < bufferSize; i += 8) {
        // Load 8 density values
        __m256i density = _mm256_load_si256((__m256i *)(densityBuffer + i));
        
        // Map density to brightness (simple linear mapping capped by maxDensity)
        // Note: Adjust the scaling factor based on your desired visual effect and maxDensity
        __m256 scale = _mm256_set1_ps(255.0f / maxDensity);
        __m256i brightness = _mm256_min_epi32(_mm256_cvtps_epi32(_mm256_mul_ps(_mm256_cvtepi32_ps(density), scale)), _mm256_set1_epi32(255));

        // Expand brightness to RGBA (assuming we set R=G=B for grayscale and A=255 for full opacity)
        __m256i r = brightness;
        __m256i g = brightness;
        __m256i b = brightness;
        __m256i a = _mm256_set1_epi32(255);

        // Pack R, G, B, and A into a single 32-bit integer per pixel
        __m256i color = _mm256_or_si256(_mm256_or_si256(r, _mm256_slli_epi32(g, 8)), _mm256_or_si256(_mm256_slli_epi32(b, 16), _mm256_slli_epi32(a, 24)));

        // Store the result
        _mm256_storeu_si256((__m256i *)(pixels + i), color);
    }
}

void CombineDensityBuffers(const int *bufferA, const int *bufferB, int *combinedBuffer, int bufferSize) {
    for (int i = 0; i < bufferSize; i += 8) {
        // Load 8 elements from each buffer
        __m256i a = _mm256_load_si256((__m256i *)(bufferA + i));
        __m256i b = _mm256_load_si256((__m256i *)(bufferB + i));

        // Combine buffers by adding values
        __m256i combined = _mm256_add_epi32(a, b);

        // Store the result
        _mm256_storeu_si256((__m256i *)(combinedBuffer + i), combined);
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

    // Initialize the window and set the target frame rate
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Particle System");
    SetWindowState(FLAG_FULLSCREEN_MODE);
    SetTargetFPS(TARGET_FPS);

    // Initialize audio device
    InitAudioDevice();
    Music music = LoadMusicStream("legendary-cinematic-piano-by-ob-13554-1min.mp3");
    PlayMusicStream(music);

    HideCursor(); // Hide the system cursor
    // Set the initial position of the mouse to the center of the screen
    SetMousePosition(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);

    // Load the main render texture and allocate memory for the pixel buffer
    RenderTexture2D mainBuffer = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
    Color *pixels = (Color *)malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(Color));

    // Allocate aligned memory for the buffers
    int *bufferA = AllocateAlignedIntBuffer(SCREEN_WIDTH, SCREEN_HEIGHT);
    int *bufferB = AllocateAlignedIntBuffer(SCREEN_WIDTH, SCREEN_HEIGHT);

    Particles particles = CreateParticles(PARTICLE_COUNT, SCREEN_WIDTH, SCREEN_HEIGHT);

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

    while (!WindowShouldClose())
    {
        UpdateMusicStream(music);
        Vector2 mousePos = GetMousePosition();

        // update particles state
        UpdateParticlesMultithreaded(&particles, mousePos);

        // tranform particles to buffer
        PTP_WORK workItemA = CreateThreadpoolWork(UpdateBufferWorkCallback, &updateContextA, &CallBackEnviron);
        PTP_WORK workItemB = CreateThreadpoolWork(UpdateBufferWorkCallback, &updateContextB, &CallBackEnviron);
        SubmitThreadpoolWork(workItemA);
        SubmitThreadpoolWork(workItemB);
        WaitForThreadpoolWorkCallbacks(workItemA, FALSE);
        WaitForThreadpoolWorkCallbacks(workItemB, FALSE);
        CloseThreadpoolWork(workItemA);
        CloseThreadpoolWork(workItemB);

        // combine buffers and transform to pixels
        CombineDensityBuffers(bufferA, bufferB, bufferA, SCREEN_WIDTH * SCREEN_HEIGHT);
        ApplyDensityToPixelsSIMD(bufferA, pixels, SCREEN_WIDTH * SCREEN_HEIGHT, 50);

        // update texture and draw
        UpdateTexture(mainBuffer.texture, pixels);
        BeginDrawing();
        DrawTexture(mainBuffer.texture, 0, 0, WHITE);
        DrawCircleV(mousePos, 5.0f, RED);
        DrawFPS(10, 10);
        EndDrawing();
    }

    FreeParticles(&particles);

    // Unload the music stream
    UnloadMusicStream(music);
    // De-initialize audio device
    CloseAudioDevice();

    // Unload the render texture and free the pixel buffer
    UnloadRenderTexture(mainBuffer);
    free(pixels);

    // Free the allocated memory for the boolean buffers
    _aligned_free(bufferA);
    _aligned_free(bufferB);

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

Particles CreateParticles(int count, int screenWidth, int screenHeight)
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

void inline clearBufferSIMD(BOOL *buffer, int count)
{
    // Process 8 bools at a time with AVX2
    for (int i = 0; i <= count - 8; i += 8)
    {
        _mm256_store_si256((__m256i *)&buffer[i], _mm256_setzero_si256());
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
    UpdateBufferWithParticleDensity(updateContext->buffer, updateContext->particles,
                                    updateContext->start, updateContext->end,
                                    updateContext->bufferWidth, updateContext->bufferHeight);
}

// Move inline definition to the header or right before its first use.
static inline uint32_t PackColor(Color color)
{
    return color.r | (color.g << 8) | (color.b << 16) | (color.a << 24);
}

void CombineBuffersAndConvertToPixelsSIMD(const BOOL *bufferA, const BOOL *bufferB, Color *pixels, int bufferSize)
{
    // Define colors in packed format for SIMD
    uint32_t packedTrueColor = PackColor((Color){0, 0, 0, 255});        // Black
    uint32_t packedFalseColor = PackColor((Color){130, 130, 130, 255}); // Gray

    __m256i vTrueColor = _mm256_set1_epi32(packedTrueColor);
    __m256i vFalseColor = _mm256_set1_epi32(packedFalseColor);

    for (int i = 0; i <= bufferSize - 8; i += 8)
    {
        __m256i vA = _mm256_load_si256((const __m256i *)&bufferA[i]); // Load 8 elements from bufferA
        __m256i vB = _mm256_load_si256((const __m256i *)&bufferB[i]); // Load 8 elements from bufferB

        // Combine buffers using OR (or any other logical operation you prefer)
        __m256i combined = _mm256_or_si256(vA, vB);
        //__m256i combined = _mm256_xor_si256(vA, vB);

        // Generate a mask based on the combined result to select between trueColor and falseColor
        // Assuming non-zero values in combined mean true, and zero means false
        __m256i mask = _mm256_cmpeq_epi32(combined, _mm256_setzero_si256()); // Elements are 0xFFFFFFFF if false, 0x0 if true

        // Blend pixels based on the mask: true values select from vTrueColor, false values from vFalseColor
        __m256i result = _mm256_blendv_epi8(vTrueColor, vFalseColor, mask);

        // Store the result directly into the pixels array
        _mm256_storeu_si256((__m256i *)&pixels[i], result);
    }
}

// Corrected AllocateAlignedIntBuffer function for int buffer
int *AllocateAlignedIntBuffer(int width, int height)
{
    size_t totalSize = width * height * sizeof(int);
    int *buffer = (int *)_aligned_malloc(totalSize, 32);
    if (buffer)
    {
        memset(buffer, 0, totalSize);
    }
    return buffer;
}

void UpdateBufferWithParticleDensity(int *buffer, Particles *particles, int start, int end, int bufferWidth, int bufferHeight)
{
    // Clear buffer to 0 for each frame
    memset(buffer, 0, bufferWidth * bufferHeight * sizeof(int));

    for (int i = start; i < end; i++)
    {
        int x = (int)particles->posX[i];
        int y = (int)particles->posY[i];
        if (x >= 0 && x < bufferWidth && y >= 0 && y < bufferHeight)
        {
            int index = y * bufferWidth + x;
            buffer[index] += 1; // Increment count
        }
    }
}