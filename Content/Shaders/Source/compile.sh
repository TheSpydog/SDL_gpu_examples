# Requires glslangValidator installed from the Vulkan SDK
for filename in *.vert; do
    glslangValidator -V "$filename" -o "../Compiled/$filename.spv"
done

for filename in *.frag; do
    glslangValidator -V "$filename" -o "../Compiled/$filename.spv"
done

for filename in *.comp; do
    glslangValidator -V "$filename" -o "../Compiled/$filename.spv"
done
