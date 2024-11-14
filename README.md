Collection of examples to demonstrate the usage of the SDL_GPU API.

To clone and build:
```
git clone https://github.com/libsdl-org/SDL
cd SDL
mkdir build
cd build
cmake ..

cd ../..
git clone https://github.com/TheSpydog/SDL_gpu_examples
cd SDL_gpu_examples
mkdir build
cd build
cmake .. -DSDL3_DIR="full/path/to/SDL/build"
```
then run `make` or your favorite IDE.

The shaders in the repository are written in HLSL and offline compiled from `Content/Shaders/Source` to `Content/Shaders/Compiled` using [SDL_shadercross](https://github.com/libsdl-org/SDL_shadercross). If you want to build the shaders yourself, you must install `SDL_shadercross`, navigate to the shader source directory, and call `compile.sh`.
