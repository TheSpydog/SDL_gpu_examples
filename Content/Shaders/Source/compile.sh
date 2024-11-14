# Requires shadercross CLI installed from SDL_shadercross
for filename in *.vert.hlsl; do
    if [ -f "$filename" ]; then
        shadercross "$filename" -o "../Compiled/SPIRV/${filename/.hlsl/.spv}"
        shadercross "$filename" -o "../Compiled/MSL/${filename/.hlsl/.msl}"
        shadercross "$filename" -o "../Compiled/DXIL/${filename/.hlsl/.dxil}"
    fi
done

for filename in *.frag.hlsl; do
    if [ -f "$filename" ]; then
        shadercross "$filename" -o "../Compiled/SPIRV/${filename/.hlsl/.spv}"
        shadercross "$filename" -o "../Compiled/MSL/${filename/.hlsl/.msl}"
        shadercross "$filename" -o "../Compiled/DXIL/${filename/.hlsl/.dxil}"
    fi
done

for filename in *.comp.hlsl; do
    if [ -f "$filename" ]; then
        shadercross "$filename" -o "../Compiled/SPIRV/${filename/.hlsl/.spv}"
        shadercross "$filename" -o "../Compiled/MSL/${filename/.hlsl/.msl}"
        shadercross "$filename" -o "../Compiled/DXIL/${filename/.hlsl/.dxil}"
    fi
done
