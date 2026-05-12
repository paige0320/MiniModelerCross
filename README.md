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
- Import OBJ meshes with the Import panel
- Select objects
- Edit mode for selecting vertices, edges, and faces
- Move selected vertices or edge endpoints
- Push/pull selected faces along their face normal
- Edit position, rotation, and scale
- Change material base color
- Toggle a procedural checker texture per object
- Load 24-bit uncompressed BMP textures per object
- Edit directional light color, intensity, ambient, and direction
- Save and load `scene.json`
- Toggle the grid
- Toggle wireframe overlay
- Show the Dear ImGui demo window

## Next Steps

Useful next milestones:

- True FBX import through Assimp or Autodesk FBX SDK
- Transform gizmo
- Drag selected objects in the viewport
- Welding shared vertices for cleaner mesh editing
- More image formats through stb_image
- OBJ or glTF export

## Import And Texture Notes

OBJ import is available from the `Import` panel. Try:

```text
assets/sample_pyramid.obj
```

Texture loading currently supports uncompressed 24-bit `.bmp` files. This keeps the prototype dependency-light and portable; PNG/JPG support should be added later with `stb_image`.

## Lighting Logic

The current renderer uses OpenGL fixed-function lighting, which is equivalent to a simple shader model:

```text
finalColor = textureOrBaseColor * (ambient + diffuse) + specular
diffuse = max(dot(normal, lightDirection), 0) * lightColor * intensity
specular = pow(max(dot(viewDirection, reflectedLight), 0), shininess)
```

In a modern shader version, the vertex shader would transform position/normal into view or world space, and the fragment shader would compute ambient, diffuse, texture sampling, and specular lighting per pixel.
