# MiniModelerCross

MiniModelerCross is a cross-platform C++ prototype for a small modeling tool.

It uses:

- CMake
- C++17
- GLFW
- OpenGL 2
- Dear ImGui

This prototype intentionally uses OpenGL 2 because it avoids extra OpenGL loader setup and is easy to build on both macOS and Windows. A later version can move to OpenGL 3, Vulkan, DirectX 11/12, or Metal.

## macOS

Install CMake first:

```sh
brew install cmake
```

Then build:

```sh
cd ~/MiniModelerCross
cmake -S . -B build
cmake --build build
./build/MiniModelerCross
```

To open in Xcode:

```sh
cd ~/MiniModelerCross
cmake -S . -B build-xcode -G Xcode
open build-xcode/MiniModelerCross.xcodeproj
```

## Windows

Install:

- Visual Studio 2022
- Desktop development with C++
- CMake tools for Windows

Then open the `MiniModelerCross` folder directly in Visual Studio, or use:

```bat
cmake -S . -B build
cmake --build build --config Debug
build\Debug\MiniModelerCross.exe
```

## Controls

- Left drag: orbit camera
- Left click: select object
- Right drag: pan camera
- Scroll: zoom
- `A`: add cube
- `P`: add plane
- `U`: add sphere
- `C`: add cylinder
- `Delete` / `Backspace`: delete selected object
- `R`: reset camera
- `E`: toggle edit mode
- `Cmd/Ctrl + S`: save scene JSON
- `Cmd/Ctrl + O`: load scene JSON

## UI

The Dear ImGui panel lets you:

- Add and delete cubes
- Add preset meshes: cube, plane, sphere, cylinder
- Select objects
- Edit mode for selecting vertices, edges, and faces
- Move selected vertices or edge endpoints
- Push/pull selected faces along their face normal
- Edit position, rotation, and scale
- Change material base color
- Toggle a procedural checker texture per object
- Edit directional light color, intensity, ambient, and direction
- Save and load `scene.json`
- Toggle the grid
- Toggle wireframe overlay
- Show the Dear ImGui demo window

## Next Steps

Useful next milestones:

- Transform gizmo
- Drag selected objects in the viewport
- Welding shared vertices for cleaner mesh editing
- Real image texture loading
- OBJ or glTF export
