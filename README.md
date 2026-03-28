# blib3d

There is no point in wasting time writing a software renderer nowadays, except for the fun of the writer. This is enough as a motivation, though.

The code is plain C++, aims to be cross platform and os-unaware.

Fixed pipeline: vertex pre-transform -> vertex clipping -> vertex post-transform -> rastering

Render triangles, quads, up to 7 vertices per polygon. For consistent non coplanar attributes interpolation (vertex lighting colors) use triangles. Coplanar texture and lightmap coordinates are ok with any polygon.

Frame buffer format is 32bit RGB, Z-buffer format is 32bit float.

Texure format is 8bit with ARGB look-up table, size must be a power of 2, fixed repeat mode.

Lightmap format is 32bit RGB, size must be a power of 2, fixed clamp mode.

Support for hierarchical z-buffer and occlusion queries.

Fill modes
- wireframe
- depth only (z buffer)
- solid RGBA color
- per vertex RGBA color
- texture

Shade modes
- off
- vertex shading
- RGB lightmap shading
- raster lightsources

Blend modes
- off
- mask (indexed transparency for textures)
- add
- mul
- alpha blend

Texture filtering
- off
- bilinear

Mip mapping
- off
- on (whole surface)

## Demo

### SDL2

clone SDL2 repository in the same folder of blib3d repository

demo/sdl

![image](demo/sdl/screenshot.png)

### NXP RT1176 Eval Board with Display

demo/mimxrt1176_lcd

![image](demo/mimxrt1176_lcd/screenshot.jpg)

## Modules

### math

math types and generic computations. contains some interesting bit hacks

### raster

low level rasterizer, you don't need to use it directly if using the renderer

vertex attributes are expected in this order (s: shade color, p: position, n: normal, f: fill color):

    [x y] z w
    [x y] z w sr sg sb
    [x y] z w su sv
    [x y] z w px py pz nx ny nz
    [x y] z w fr fg fb fa
    [x y] z w fr fg fb fa sr sg sb
    [x y] z w fr fg fb fa su sv
    [x y] z w fr fg fb fa px py pz nx ny nz
    [x y] z w fs ft
    [x y] z w fr fg sr sg sb
    [x y] z w fr fg su sv
    [x y] z w fr fg px py pz nx ny nz

### render

interface for primitives drawing

example of vertex buffers format convention:

           0           3               7           10          13              17
    buf0: |xyz,xyz,xyz|xyz,xyz,xyz,xyz|xyz,xyz,xyz|xyz,xyz,xyz|xyz,xyz,xyz,xyz|xyz,xyz,xyz|
          vertex stride: 3

           0        3           7        10       13          17
    buf1: |st,st,st|st,st,st,st|st,st,st|st,st,st|st,st,st,st|st,st,st|
          vertex stride: 2
    
    face: |vertex index in buffer,number of vertices|

    meterial0
        faces: |0,3|3,4|7,3|
    meterial1
        faces: |10,3|13,4|17,3|

### timer

profiling, time elapsed

### view

generate pre transform and post transform matrix from parameters
