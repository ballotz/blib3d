# lib3d - simple portable 3D rendering

C++ platform independent 3D rendering library, data oriented, no memory allocation

Output buffer/frame format is 32bit RGB, Z-buffer is 32bit float

Texure format is 8bit with ARGB look-up table

Lightmap format is 32bit RGB

Render triangles, quads, up to 7 vertices per polygon

Pipeline: vertex pre-transform -> vertex clipping -> vertex post-transform -> raster

Support for hierarchical z-buffer and occlusion queries

Fill modes
- depth only (z buffer)
- solid RGBA color
- per vertex RGBA color
- texture

Shade modes
- off
- vertex shading
- RGB lightmap shading

Blend modes
- off
- mask (indexed transparency for textures)
- add
- mul
- alpha blend

Texture filtering
- off
- bilinear

## Demo/Test

### SDL2 - Visual Studio 2019

test/sdl/win

![image](test/sdl/screenshot.png)

### NXP RT1176 Eval Board with Display

test/sdl/mimxrt1176_lcd

![image](test/mimxrt1176_lcd/screenshot.jpg)
