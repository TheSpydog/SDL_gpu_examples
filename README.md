WIP collection of examples to demonstrate the usage of the SDL_GPU API.

To clone and build:
```
git clone https://github.com/libsdl-org/SDL
cd SDL
mkdir build
cd build
cmake ..

cd ../..
git clone --recursive https://github.com/TheSpydog/SDL_gpu_examples
cd SDL_gpu_examples
mkdir build
cd build
cmake .. -DSDL3_DIR="full/path/to/SDL/build"
```
then run `make` or your favorite IDE.

