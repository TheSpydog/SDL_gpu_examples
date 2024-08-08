WIP collection of examples to demonstrate the usage of the SDL_gpu proposal.

To clone and build:
```
git clone https://github.com/thatcosmonaut/SDL -b gpu
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
