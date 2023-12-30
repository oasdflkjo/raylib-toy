# Raylib-Toy

A simple program to generate particle effects, featuring an windows multithreading and simd. This project utilizes [Raylib](https://www.raylib.com/), a simple and easy-to-use library to enjoy videogames programming.

- Simulating 720,000 moving particles at 160fps
- Multithreading
- SIMD

[![youtube link](https://img.youtube.com/vi/_Yh6UAYJCzw/0.jpg)](https://www.youtube.com/watch?v=_Yh6UAYJCzw)

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes.

### Prerequisites

- GCC Compiler

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