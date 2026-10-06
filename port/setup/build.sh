#!/bin/sh
# Builds the setup window (port/setup/setup.cpp) as ./bt3-setup. Needs g++ and SDL3; Dear ImGui is in the repository.
cd "$(dirname "$0")/../.." || exit 1
I=port/third_party/imgui
mkdir -p port/build/setup
for f in $I/imgui.cpp $I/imgui_draw.cpp $I/imgui_tables.cpp $I/imgui_widgets.cpp $I/imgui_impl_sdl3.cpp $I/imgui_impl_sdlgpu3.cpp port/setup/setup.cpp; do
    o=port/build/setup/$(basename "$f" .cpp).o
    if [ ! -f "$o" ] || [ "$f" -nt "$o" ]; then
        g++ -std=c++17 -O2 -fno-exceptions -fno-rtti -w -I$I -c "$f" -o "$o" || exit 1
    fi
done
g++ -o bt3-setup port/build/setup/*.o -lSDL3 -lpthread && echo "built ./bt3-setup"
