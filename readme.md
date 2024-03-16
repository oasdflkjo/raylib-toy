# Raylib-Toy

This particle simulation leverages a sophisticated architecture designed for high performance and real-time rendering of thousands of particles. At its core, the system utilizes a Struct of Arrays (SoA) for particle data management, enhancing data locality and SIMD (Single Instruction, Multiple Data) processing efficiency. Multithreading via the Windows Thread Pool API allows the workload to be distributed across multiple cores, ensuring that each thread updates a segment of particles concurrently, which maximizes CPU utilization and minimizes processing time.

Key highlights of the simulation architecture include:

- Efficient Particle Data Structure: Utilizes SoA to improve cache efficiency and SIMD compatibility, storing particle positions and velocities in separate arrays.
- SIMD Optimizations: Employs AVX2 instructions for vectorized operations, allowing multiple particle updates in parallel, significantly speeding up computations.
- Multithreaded Updates: Leverages a thread pool for distributing particle updates across multiple threads, enhancing performance on multi-core systems.
- Resource Management: Ensures aligned memory allocations for efficient SIMD processing.
- By integrating these approaches, the simulation achieves fluid motion and dynamic particle interactions, targeting high frame rates and delivering a visually compelling experience.

[![youtube](https://img.youtube.com/vi/Wg_yjpatF90/0.jpg)](https://www.youtube.com/watch?v=Wg_yjpatF90)


## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. Have fun :)

### Prerequisites

- GCC Compiler
- Make

### Installation

1. **Clone the Repository Recursively**

    This project uses Raylib as a submodule. Clone the repository recursively to include the Raylib submodule:

    ```bash
    git clone --recurse-submodules https://github.com/oasdflkjo/raylib-toy.git
    cd raylib-toy
    ```

2. **Build the Project with Make**

    The project includes a Makefile for easy compilation. Simply run:

    ```bash
    make
    ```

    This will compile Raylib and the main application.

### Running

After compilation, run the program:

```bash
./main.exe
```

### License

This project is licensed under the MIT License

### Acknowledgments

Thanks to ChatGPT for assisting in structuring and documenting the project.
