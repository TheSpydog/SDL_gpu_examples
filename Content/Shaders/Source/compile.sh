# Requires glslangValidator installed from the Vulkan SDK
for filename in *.vert; do
    glslangValidator -V "$filename" --entry-point "vs_main" --source-entrypoint "main" -o "../Compiled/$filename.spv"
done

for filename in *.frag; do
    glslangValidator -V "$filename" --entry-point "fs_main" --source-entrypoint "main" -o "../Compiled/$filename.spv"
done