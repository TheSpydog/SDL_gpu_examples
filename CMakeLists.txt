cmake_minimum_required(VERSION 3.10)
project(SDL_gpu_examples)
find_package(SDL3 REQUIRED)

add_executable(SDL_gpu_examples
    main.c
    SDL_gpu_examples.h
    Examples/Common.c
    Examples/ClearScreen.c
    Examples/ClearScreenMultiWindow.c
    Examples/BasicTriangle.c
    Examples/BasicStencil.c
    Examples/BasicCompute.c
)

target_include_directories(SDL_gpu_examples PRIVATE .)

target_link_libraries(SDL_gpu_examples
    SDL3::SDL3
    SDL3::Headers
)

add_custom_command(TARGET SDL_gpu_examples POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Content $<TARGET_FILE_DIR:SDL_gpu_examples>/Content
)
