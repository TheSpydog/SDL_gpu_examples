cmake_minimum_required(VERSION 3.10)
project(SDL_gpu_examples)
find_package(SDL3 REQUIRED)

add_executable(SDL_gpu_examples
    main.c
    ClearScreenExample.c
    ClearScreenMultiWindowExample.c
    SDL_gpu_examples.h
)

target_link_libraries(SDL_gpu_examples
    SDL3::SDL3
    SDL3::Headers
)