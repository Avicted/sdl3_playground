#!/bin/zsh

# Source any required environment profile
source ~/.zshrc  # or another appropriate shell profile

# Check if shadercross is in PATH
if ! command -v shadercross &> /dev/null
then
    echo "  ERROR: shadercross could not be found in your PATH"
    exit 1
fi

set -xe

# Requires shadercross CLI installed from SDL_shadercross
for filename in *.vert.hlsl; do
    if [ -f "$filename" ]; then
        shadercross "$filename" -o "../compiled/SPIRV/${filename/.hlsl/.spv}"
        shadercross "$filename" -o "../compiled/MSL/${filename/.hlsl/.msl}"
        shadercross "$filename" -o "../compiled/DXIL/${filename/.hlsl/.dxil}"
    fi
done

for filename in *.frag.hlsl; do
    if [ -f "$filename" ]; then
        shadercross "$filename" -o "../compiled/SPIRV/${filename/.hlsl/.spv}"
        shadercross "$filename" -o "../compiled/MSL/${filename/.hlsl/.msl}"
        shadercross "$filename" -o "../compiled/DXIL/${filename/.hlsl/.dxil}"
    fi
done

## for filename in *.comp.hlsl; do
##     if [ -f "$filename" ]; then
##         shadercross "$filename" -o "../compiled/SPIRV/${filename/.hlsl/.spv}"
##         shadercross "$filename" -o "../compiled/MSL/${filename/.hlsl/.msl}"
##         shadercross "$filename" -o "../compiled/DXIL/${filename/.hlsl/.dxil}"
##     fi
## done
