# blib3d

A small software 3D rasterizer in C++17 for bare-metal microcontrollers and desktop.
No OS, no heap, no dependencies. You own every buffer; the library only writes pixels into them.

Runs on ARM Cortex-M7 (NXP i.MX RT1176) and x86/x64 (MSVC, GCC, Clang).

| Desktop (SDL2) | i.MX RT1176 EVK |
|---|---|
| ![SDL demo](demo/sdl/screenshot.png) | ![Embedded demo](demo/mimxrt1176_lcd/screenshot.jpg) |

> *Writing a software renderer today is mostly done for fun. This one was, and it happens to be useful on chips without a GPU.*

How the renderer works inside (algorithms, numeric formats, memory layout) is described in [docs/internals.md](docs/internals.md).

---

## Features

| Area | Supported |
|---|---|
| Primitives | convex polygons, 3 to 7 vertices (triangles, quads, n-gons) |
| Projection | perspective · orthographic |
| Fill | none (depth only) · solid color · per-vertex RGBA · texture |
| Shade | none · per-vertex light color · lightmap (bilinear) · per-pixel lights |
| Lights | ambient · directional · point · spot, gamma-corrected |
| Blend | none · additive (saturating) · multiply · alpha |
| Textures | 8-bit indexed + 256-entry ARGB palette · nearest or bilinear · per-face mipmapping · palette-alpha transparency |
| Depth | off · write · test · test+write |
| Clipping | homogeneous Sutherland–Hodgman (near via `w_min`, optional near/far with `BLIB3D_CLIP_Z`) |
| Other | wireframe overlay · hierarchical-Z occlusion test · strip rendering for low-RAM targets |

Not included: fog, anti-aliasing, lines/points as primitives, mesh loaders, scene graph, threading.

---

## Quick start

```cpp
#include "render.hpp"
#include "view.hpp"

using namespace blib3d;

float coords[] = {          // x, y, z (camera looks down +z)
     0.0f,  0.5f, 1.0f,
    -0.5f, -0.5f, 1.0f,
     0.5f, -0.5f, 1.0f,
};
render::face faces[] = { { 0, 3 } };    // first vertex, vertex count

render::renderer r;

void setup(int w, int h)
{
    math::mat4x4 proj, viewport;
    view::make_projection_perspective(proj, (float)w / h, math::pi / 2.f);
    view::make_viewport(viewport, (float)w, (float)h);

    r.set_geometry_transform(proj);     // object -> clip (projection * view * model)
    r.set_frame_transform(viewport);    // NDC -> pixels

    r.set_geometry_coord(coords, 3);    // pointer + stride in floats
    r.set_geometry_face(faces, 1);
    r.set_geometry_back_cull(false);

    r.set_depth_type(render::renderer::DEPTH_TEST_WRITE);
    r.set_fill_type(render::renderer::FILL_SOLID);
    r.set_fill_color({ 255, 128, 64, 255 });   // b, g, r, a
    r.set_frame_clear_color({ 0, 0, 0, 255 });
    r.set_frame_clear_depth(0.f);
}

void draw(uint32_t* pixels, float* depth, int w, int h, int stride)
{
    r.set_frame_data(w, h, stride, depth, (raster::ARGB*)pixels);
    r.render_clear_frame();
    r.render_clear_depth();
    r.render_draw();
}
```

The full version is in [`demo/source/triangle.cpp`](demo/source/triangle.cpp).

---

## Concepts

### Data model

- **Frame**: you provide a 32-bit color buffer (`ARGB`, stored as `b g r a` bytes, i.e. `0xAARRGGBB` on little-endian) and a `float` depth buffer, both with the same stride.
- **Vertex streams**: each attribute is a `float*` plus a stride in floats, read in place. Nothing is copied at setup. Only the streams required by the current fill/shade mode are read.

  | Stream | Components | Range |
  |---|---|---|
  | `coord` | x y z | object space |
  | `color` | r g b a | 0–255 |
  | `tex_coord` | u v | normalized, 1.0 = texture size (wraps) |
  | `light_color` | r g b | 0–255, 255 = unlit color |
  | `lmap_coord` | s t | lightmap texels |
  | `norm` | x y z | unit length, same space as `coord` |

- **Faces**: `face { index, count }` points to `count` consecutive vertices. An optional index array selects a subset of faces to draw.
- **State**: mode setters only change a bitfield. `render_draw()` renders the current face list with the current state; change state and call it again for the next material.

### Conventions

- Matrices are row-major `float[16]` and multiply column vectors (`v' = M * v`, translation in elements 3, 7, 11). `math::mul4x4_4x4(out, a, b)` computes `a * b`.
- With `make_projection_perspective` the camera looks down **+z**. The depth buffer stores `-1/z`: **smaller is closer**, clear to `0` (infinitely far). With `make_projection_ortho` it stores `z`, still smaller is closer.
- Pixel centers are at `(x + 0.5, y + 0.5)`; a pixel is covered if its center is inside the polygon (top-left rule).
- Polygons must be convex and planar. With back-face culling enabled, faces that appear counter-clockwise on screen are discarded.

### Pipeline

```
render_draw()
  for each face
    transform        object -> clip, 4x4 matrix
    clip             homogeneous Sutherland-Hodgman, outcode early-out
    project          viewport, divide by w, attributes premultiplied by 1/w
    batch            up to 32 faces, then flush to the rasterizer
rasterizer
  setup              per-polygon screen-space gradients for every attribute
  scan               walk left/right edges, emit horizontal spans
  span               16-pixel blocks: exact 1/w at block ends, 16.16 fixed point inside
  pixel              depth test -> sample -> shade -> blend -> write
```

Each mode combination is a separate template specialization, so the pixel loop has no mode branches. Details in [docs/internals.md](docs/internals.md).

---

## Features in detail

### Textures

- Width and height must be powers of two. Addressing wraps (repeat).
- Texels are 8-bit palette indices. The palette is 256 `ARGB` entries.
- `set_texture_mask(TEXMASK_ON)` enables masked transparency: a texel is skipped (no color or depth write) when the most significant bit of its palette alpha is clear. Mark transparent palette entries with alpha `0x00` and opaque ones with `0xFF`. With bilinear filtering the test runs on the filtered color, so edges are cut where the blended alpha crosses the midpoint.
- Mipmapping (`TEXMIP_FACE`) picks one level per face from the screen-area / texture-area ratio. Levels must follow level 0 contiguously in memory, each half the size of the previous, until either dimension reaches 1.
- `python/mipmap_gen.py` converts an image into this format (`.tex` binary or C++ source):

  ```bash
  pip install pillow numpy
  python python/mipmap_gen.py brick.png brick.tex --save-cpp brick
  ```

### Lighting

- `SHADE_VERTEX`: per-vertex RGB light color, interpolated, multiplied with the fill color.
- `SHADE_LIGHTMAP`: an `ARGB` lightmap with its own coordinates, sampled bilinearly and clamped. The width must be a power of two. Allocate at least one extra row past the declared height: the filter reads the texel below and to the right.
- `SHADE_LIGHT`: position and normal are interpolated and the lights in `set_shade_lights()` are evaluated (diffuse only) every 4 pixels. Positions and normals are taken from the `coord` and `norm` streams before any transform, so light positions and directions must be in the same space. Light intensities are linear, 1.0 = full. See `raster::light` in [`raster_light.hpp`](source/raster_light.hpp) for the parameters. `gamma_set()` sets the output gamma.

### Occlusion (HZB)

`occlusion_build_mipchain()` builds a depth pyramid from the current depth buffer; `occlusion_test_rect()` returns `true` if a screen rectangle whose nearest depth is `depth_min` is hidden.

- The pyramid is written **right after the depth buffer**: allocate about `w*h/3` extra floats.
- The depth buffer stride must equal its width.
- The first level uses `min` to ignore 1-pixel cracks, so the test can be slightly aggressive.

### Strip rendering

For targets that cannot hold a full frame, `view::make_pre_matrix_strip()` and `view::make_post_matrix_strip()` adjust the transforms so that a horizontal band of the frame is rendered into a buffer that is only as tall as the band. Render all bands in sequence to build the frame. See [docs/internals.md](docs/internals.md#strip-rendering) for an example.

### Frustum culling

`view::make_frustum_planes()` returns the clip planes in the space of the matrix you pass, for your own per-object culling. `math::test3_aab_plane()` tests an axis-aligned box against a plane.

### Wireframe

`set_wireframe(true)` draws 1-pixel white outlines of the faces, then fills depth only.

### Profiling

`renderer::prof_geometry` and `renderer::prof_raster` accumulate the time spent in transform/clip and in rasterization. On the RT1176 they read the DWT cycle counter, elsewhere `std::chrono`.

---

## Limits

| | Value | Where |
|---|---|---|
| Vertices per input face | 7 | `render::num_max_vertices` |
| Faces per batch | 32 | `renderer::raster_face_buffer_size` |
| Mip levels | 16 | `raster::mip_table_max_size` |
| Near plane | `w >= 0.01` | `render::clip_w_min` |

---

## Building

The library is a CMake (≥ 3.22) static target with no dependencies:

```cmake
add_subdirectory(blib3d)
target_link_libraries(my_app PRIVATE blib3d)
```

| Define | Effect |
|---|---|
| `BLIB3D_USE_SIMD` | SSE paths for vector/matrix math on x86/x64 (on by default) |
| `BLIB3D_CLIP_Z` | also clip against near/far planes (`z = ±w`) |
| `CPU_MIMXRT1176DVMAA_cm7` | profiling timers use the DWT cycle counter |

### SDL2 demo

SDL2 is expected as a sibling of the repository:

```bash
git clone https://github.com/ballotz/blib3d
git clone https://github.com/libsdl-org/SDL -b SDL2
cmake -S blib3d/demo/sdl -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/sdl-demo
```

### i.MX RT1176 demo

Open `demo/mimxrt1176_lcd` in MCUXpresso IDE, or use its CMake presets with `arm-none-eabi-gcc`. The demo renders at 360×640, doubles the pixels to the panel, and double-buffers through eLCDIF.

`python/swo_profiler.py` samples the PC over SWO with a J-Link and prints hot functions or lines (RT1170 and RT1060):

```bash
python python/swo_profiler.py --elf <firmware.elf> --duration 10 --by-line
```

---

## Layout

```
source/   library
  render    public API, transform, clipping, batching
  raster    scan conversion, span loops, occlusion
  raster_*  pixel helpers: fill/blend/sample, gradients, lights
  view      projection, viewport, strip and frustum helpers
  math      vectors, matrices, fast log2/pow
  timer     profiling (std::chrono or DWT)
demo/     shared demo scenes, SDL2 and RT1176 front ends
docs/     internals
python/   texture converter, SWO profiler
test/     math accuracy and speed checks
```

---

## License

Apache 2.0, see [LICENSE](LICENSE). Copyright 2019 Alessio Ballotti.
