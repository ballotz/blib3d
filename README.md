# blib3d

> *There is no point in wasting time writing a software renderer nowadays, except for the fun of the writer. This is enough as a motivation, though.*

A portable, zero-dependency software 3D renderer written in C++. No OS, no heap allocator, no runtime required. Tested on desktop (x86/x64) and bare-metal embedded systems (ARM Cortex-M7).

**Screenshot — SDL2 demo on desktop:**

![SDL demo](demo/sdl/screenshot.png)

**Running on an NXP MIMXRT1176 Cortex-M7 evaluation board:**

![Embedded demo](demo/mimxrt1176_lcd/screenshot.jpg)

---

## Table of contents

- [Features](#features)
- [Architecture overview](#architecture-overview)
- [The rendering pipeline](#the-rendering-pipeline)
  - [Stage 1 — Geometry & transform](#stage-1--geometry--transform)
  - [Stage 2 — Clipping](#stage-2--clipping)
  - [Stage 3 — Rasterization](#stage-3--rasterization)
- [Algorithms](#algorithms)
  - [Perspective-correct attribute interpolation for polygons](#perspective-correct-attribute-interpolation-for-polygons)
  - [Affine texture mapping with perspective correction via sub-spans](#affine-texture-mapping-with-perspective-correction-via-sub-spans)
  - [Light sampling with spatial dithering](#light-sampling-with-spatial-dithering)
  - [The templated pixel pipeline](#the-templated-pixel-pipeline)
  - [Bilinear filtering in 4 multiplications instead of 8](#bilinear-filtering-in-4-multiplications-instead-of-8)
  - [Hierarchical Z-buffer occlusion culling](#hierarchical-z-buffer-occlusion-culling)
- [Modules](#modules)
- [Building](#building)
- [Usage example](#usage-example)

---

## Features

**Fill modes** — wireframe · depth only · solid RGBA · per-vertex RGBA · texture (8bpp + 256-color LUT)

**Shade modes** — none · vertex · RGB lightmap · raster light sources (ambient, directional, point, spot)

**Blend modes** — none · mask (indexed transparency) · add · multiply · alpha blend

**Texture filtering** — none · bilinear

**Mip mapping** — none · per-face level selection

**Projection** — orthographic · perspective

**Occlusion** — hierarchical Z-buffer with mip chain

---

## Architecture overview

blib3d is organized as five independent modules. Each layer uses only the layers below it — there are no upward dependencies.

```
┌─────────────────────────────────────────────────────────────┐
│                         render                              │
│   renderer class · geometry buffers · clip · batch flush    │
├─────────────────┬───────────────────────────────────────────┤
│     raster      │              raster_light                 │
│  scan · fill ·  │  ambient · directional · point · spot     │
│  blend · depth  │                                           │
├─────────────────┴───────────────────────────────────────────┤
│                          math                               │
│   vec2/3/4 · mat3x3/4x4 · SIMD SSE · powfast · geometry     │
├─────────────────────────────────────────────────────────────┤
│                         shared                              │
│        arch detect · compiler macros · USE_SIMD             │
└─────────────────────────────────────────────────────────────┘

  view   ─── standalone · builds projection & viewport matrices
  timer  ─── standalone · profiling helpers
```

The `render` module is the public API. You set geometry buffers, material parameters, and call `render_draw()`. It handles clipping and feeds batches of rasterized vertices to the `raster` module, which writes pixels directly into your frame buffer.

---

## The rendering pipeline

Every call to `render_draw()` drives the following pipeline:

```
  Your geometry buffers
        │
        ▼
  ┌─────────────────────┐
  │  pre-transform      │  multiply each vertex by the combined
  │  (world → clip)     │  view-projection matrix (transposed,
  └─────────┬───────────┘  for better cache access)
            │
            ▼
  ┌─────────────────────┐
  │   clip_face         │  Sutherland-Hodgman against 5 planes
  │   (clip space)      │  (+ optional near/far = 7 planes)
  └─────────┬───────────┘
            │  clipped polygon (up to 12 vertices)
            ▼
  ┌─────────────────────┐
  │  post-transform     │  perspective divide + viewport
  │  (clip → screen)    │  scale to pixel coordinates
  └─────────┬───────────┘
            │
            ▼
  ┌─────────────────────┐
  │  batch buffer       │  faces accumulate until the batch
  │  (32 faces)         │  is full, then flushed to raster
  └─────────┬───────────┘
            │
            ▼
  ┌─────────────────────────────────────────────────────┐
  │  scan_faces  →  scan_face  →  process_span          │
  │                                                     │
  │  for each polygon:                                  │
  │    walk left & right edges simultaneously           │
  │    emit horizontal spans (x0 → x1 per scanline)     │
  │                                                     │
  │  for each span:                                     │
  │    sub-span block (16px) → setup_subspan            │
  │    per-pixel fill loop   → fill()                   │
  └─────────────────────────────────────────────────────┘
            │
            ▼
    frame buffer   +   depth buffer
```

### Stage 1 — Geometry & transform

Vertices live in your own arrays. You give the renderer a pointer, a stride, and a face list (`face { index, count }`). No copying into internal vertex buffers happens at setup time.

The **pre-transform matrix is stored transposed** (`pre_matrix_t`).

```
normal layout:     row 0 | row 1 | row 2 | row 3
                    ↑ each row is a separate cache line fetch per column access

transposed layout: col 0 | col 1 | col 2 | col 3
                    ↑ reading column = reading a row = sequential, all in one line
```

### Stage 2 — Clipping

Clipping runs in homogeneous clip space (before perspective divide), using a minimal **Sutherland-Hodgman** implementation. The key to keeping it lean is the two-buffer ping-pong approach:

```
buffer[0]  →  clip against plane 0  →  buffer[1]
buffer[1]  →  clip against plane 1  →  buffer[0]
buffer[0]  →  clip against plane 2  →  buffer[1]
   ...
```

Only one allocation needed (the `render_buffer[2][12][components]` stack array), no heap. The number of active planes per face is tracked via a bit mask computed in a single pass before clipping begins:

```
any_out = OR  of all vertex flags   → if 0: fully inside, skip clipping
all_out = AND of all vertex flags   → if non-zero: fully outside, discard face
```

Faces that are fully inside (the common case for nearby geometry) skip all plane loops entirely.

The near-plane clip is handled specially: instead of clipping against z = 0 (which causes division by zero during perspective divide), the renderer clips against `w < w_min` (a small positive constant). This keeps all divisions safe.

### Stage 3 — Rasterization

After the post-transform, screen coordinates are in float. The rasterizer snaps to pixel centers using:

```c
real_to_raster(v)  =  ceil(v − 0.5)
raster_to_real(v)  =  v + 0.5
```

This places the sample point at the center of each pixel, giving consistent edge behavior across all polygon orientations.

The polygon scan proceeds by walking a **left edge** and **right edge** simultaneously, emitting horizontal spans. The vertex with the minimum Y is found first; then the polygon is traversed clockwise and counter-clockwise simultaneously. Each edge maintains a floating-point X position and a slope (`dx/dy`), updated by one addition per scanline.

---

## Algorithms

### Perspective-correct attribute interpolation for polygons

Texture coordinates, vertex colors, and other attributes must be interpolated with perspective correction, or they slide in a visually wrong way (the "rubber sheet" effect of early 3D games).

The standard approach — divide each attribute by W, interpolate linearly, then multiply back by W per pixel — works for triangles. But blib3d supports polygons up to 7 vertices, which cannot be naively triangulated at interpolation setup time.

The trick is to compute a **single plane equation** (gradient) for each attribute across the entire polygon, even if it has more than 3 vertices. This works because the polygon is convex and planar in 3D, so each attribute *is* planar in screen space (after division by W).

For each attribute `c` (like texture S coordinate), compute the weighted sum of cross products across all triangles formed by fan-triangulating from vertex 0:

```
area2cx[c] += cross( (c[v1]-c[v0]), (x[v1]-x[v0]),
                     (c[v2]-c[v0]), (x[v2]-x[v0]) )

area2cy[c] += cross( (c[v1]-c[v0]), (y[v1]-y[v0]),
                     (c[v2]-c[v0]), (y[v2]-y[v0]) )
```

Then the gradient (how much the attribute changes per screen pixel) is:

```
g.dx = area2cy / area2xy      ← change per pixel in X
g.dy = -area2cx / area2xy     ← change per pixel in Y
g.d  = c[v0] - g.dx*x[v0] - g.dy*y[v0]    ← value at origin
```

During rasterization, evaluating the attribute at pixel (x, y) is then just:

```
value = g.dx * x + g.dy * y + g.d
```

One multiply-add chain per attribute per pixel. No divisions, no triangle fan. The same gradients work for any convex polygon, not just triangles.

---

### Affine texture mapping with perspective correction via sub-spans

This part is heavily inspired by Quake software renderer.
Per-pixel perspective-correct texture mapping requires a reciprocal divide per pixel — expensive on any CPU. A classic optimization is to compute exact perspective-correct UV only at the *endpoints* of fixed-size blocks, and linearly interpolate (affinely) within each block. The error is bounded and invisible at typical viewing angles.

blib3d uses 16-pixel sub-spans:

```
span (variable length)
│
├── sub-span 0 (16px) ───────────────────────────┐
│   ┌──────────────────────────────────────────┐ │
│   │  setup_subspan():                        │ │
│   │    attrib_int      = perspective(x_start)│ │
│   │    attrib_int_next = perspective(x_end)  │ │
│   │    dx = (next - cur) >> 4    ← divide 16 │ │
│   │                               by shifting│ │
│   └──────────────────────────────────────────┘ │
│   → fill() x16: UV += dx each pixel (cheap)    │
├────────────────────────────────────────────────┘
│
├── sub-span 1 (16px) ───────────────────────────┐
│   ...                                          │
```

The divide-by-16 at sub-span setup is implemented as a right shift (`>> 4`), costing nothing. The expensive reciprocal `1/W` is computed only once per 16 pixels. At 640 pixels wide, that's 40 reciprocals per scanline instead of 640.

Attributes are stored in **16.16 fixed point** during pixel fill: the integer part in the high 16 bits, the fractional part in the low 16 bits. This means UV increments are integer additions per pixel, with texture addressing extracted by a single right shift.

---

### Light sampling with spatial dithering

Evaluating a lightmap sample or computing raster light sources per pixel is expensive. Sampling every pixel on every span would make lit rendering prohibitively slow, especially on embedded targets.

blib3d samples the light **once every 4 pixels**, holding the result constant for the intervening 3 pixels (`shade_hold = 4`). This alone would produce visible 4-pixel banding across surfaces. The fix is a two-part trick.

**Row offset.** The sampling phase is shifted by 2 pixels on odd scanlines:

```c
s.shade_counter = ((y & 1 ? shade_hold >> 1u : 0u) + x0) & shade_mask;
```

This means even and odd rows sample at interleaved positions:

```
even row:  S . . . S . . . S . . . S . . .    (S = sample, . = held)
odd  row:  . . S . . . S . . . S . . . S .

combined view on screen:
  row 0:   S . . . S . . . S . . .
  row 1:   . . S . . . S . . . S .
  row 2:   S . . . S . . . S . . .
  row 3:   . . S . . . S . . . S .
```

The resulting pattern creates a uniform 2D distribution of sample points — no pixel is further than 2 pixels away from a sample in any direction. The visual result is smooth and artefact-free even at 1/4 sampling density.

**Depth-break reset.** When a pixel fails the depth test — meaning it is occluded by geometry in front — the shade cache is invalidated:

```c
if (depth_type::process_test(s.depth_addr, s.depth))
{
    if (((s.shade_counter & shade_mask) == 0) | s.shade_trigger)
    {
        s.shade_trigger = 0;
        // ... resample lightmap / lights ...
    }
    blend_type::process(s.frame_addr, color);
}
else
{
    s.shade_trigger = 1;   // ← reset: next visible pixel will resample
}
```

Without this, after a run of occluded pixels the next visible pixel would inherit a stale shade value from a completely different surface position. Setting `shade_trigger = 1` on depth failure ensures the first pixel that becomes visible after an occlusion boundary always gets a fresh sample, regardless of where `shade_counter` happens to be in its 4-pixel cycle.

---

### The templated pixel pipeline

The pixel fill loop runs millions of times per frame. Different combinations of fill mode, shade mode, blend mode, and filter mode would normally require branching inside the hot loop — killing branch prediction and throughput.

blib3d avoids this completely with **C++ template policies**: each combination of modes is a distinct type, resolved at compile time. The fill loop for `FILL_TEXTURE | SHADE_LIGHTMAP | BLEND_NONE | FILTER_LINEAR` is a completely different function from `FILL_SOLID | SHADE_NONE | BLEND_ALPHA` — no runtime conditionals inside either.

```cpp
// Each policy type has a static fill() method — no virtual dispatch
template<
    typename sample_type,   // sample_nearest  or  sample_bilinear
    typename blend_type,    // blend_none / blend_add / blend_alpha / ...
    typename depth_type,    // depth_off / depth_test / depth_test_write
    typename mask_type>     // mask_texture_on / mask_texture_off
struct raster_texture_shade_lightmap : public scan
{
    static force_inline void fill(span_data& s)
    {
        if (depth_type::process_test(s.depth_addr, s.depth))
        {
            uint32_t texel = sample_type::process_texel(...);
            if (mask_type::process(texel))
            {
                // lightmap shade ...
                blend_type::process(s.frame_addr, color);
                depth_type::process_write(s.depth_addr, s.depth);
            }
        }
        // advance ...
    }
};
```

At the `scan_faces` call site, `new (storage) raster_texture_shade_lightmap<sample_bilinear, blend_none, depth_test_write, mask_texture_off>(config)` constructs the right specialization into a small stack buffer. The virtual `process_span()` dispatch happens once per polygon — thousands of times cheaper than branching inside the pixel loop.

The compiler sees each instantiation as a distinct, fully inlineable function. Dead code (e.g., the mask test when `mask_texture_off` is selected) is completely eliminated. The pixel loop has exactly the instructions needed for the active mode combination, and nothing else.

---

### Bilinear filtering in 4 multiplications instead of 8

Standard bilinear filtering blends 4 neighboring texels using weights that sum to 1. Each weight is a product of the fractional UV parts, requiring 4 multiplications (one per weight), plus 8 more multiplications to apply those weights to each channel of each texel — 12 multiplications total per sample.

The `bilinear62` variant exploits integer packing to halve the channel multiplications:

```c
uint32_t bilinear62(uint32_t v0, uint32_t v1,
                    uint32_t v2, uint32_t v3,
                    uint32_t a0, uint32_t a1)
{
    uint32_t w3{ a0 * a1 >> 2u };          // weight: corner
    uint32_t w2{ a1 - w3 };                //         edge
    uint32_t w1{ a0 - w3 };                //         edge
    uint32_t w0{ 4u + w3 - a0 - a1 };     //         opposite corner

    return
        ((v0 & 0xFCFCFCFCu) >> 2u) * w0 +
        ((v1 & 0xFCFCFCFCu) >> 2u) * w1 +
        ((v2 & 0xFCFCFCFCu) >> 2u) * w2 +
        ((v3 & 0xFCFCFCFCu) >> 2u) * w3;
}
```

The trick: `a0` and `a1` are 2-bit weights (0–3) instead of 8-bit (0–255). Masking with `0xFCFCFCFC` and shifting right by 2 reduces all four channels to 6-bit precision simultaneously. Now each channel value fits in 6 bits, and four channels can be packed into a 32-bit word without overflow during the multiply-accumulate.

The result is 4 multiplications total for all 4 channels of all 4 texels — instead of 8. Benchmarked speedup on x86: **1.7× faster** than the full-precision `bilinear88` variant, with visually imperceptible quality loss at 6 bits per channel.

The trade-off is intentional and tunable — the codebase also includes `bilinear44` (4-bit weights, slightly faster), `bilinear53` (5+3 bits), and `bilinear88` (full 8-bit precision) as alternatives.

---

### Hierarchical Z-buffer occlusion culling

Large occluded geometry (walls behind walls) can waste significant fill rate. blib3d includes a **hierarchical Z-buffer (HZB)**: a mip chain of the depth buffer where each level stores the *maximum* depth of a 2×2 block from the level below.

```
Level 0 (full res):   actual depth buffer  640 × 480
Level 1 (half):       max of 2×2 blocks    320 × 240
Level 2 (quarter):    max of 2×2 blocks    160 × 120
   ...
Level N (1×1):        single max value       1 × 1
```

To test whether a bounding box is occluded (safe to skip rendering):

1. Project the 8 corners of the 3D bounding box to screen space
2. Find the screen-space rectangle (max X, Y) and the minimum depth
3. Choose the mip level where the rectangle covers ~1 texel in the shortest dimension
4. Read the pre-computed maximum depths at that level
5. If `hzb_max_depth < box_min_depth` → the entire box is behind existing geometry → **skip**

The test is O(1) — few table lookups at the chosen mip level. Building the chain is one pass over the depth buffer each frame.

---

## Modules

| Module | Files | Purpose |
|---|---|---|
| `math` | `math.hpp` `math.cpp` | Vector/matrix math, SIMD SSE, powfast, geometry tests |
| `raster` | `raster.hpp` `raster.cpp` `raster_fill.hpp` `raster_interp.hpp` `raster_light.hpp` | Low-level polygon rasterizer, pixel fill loop |
| `render` | `render.hpp` `render.cpp` | Public renderer API, clipping, geometry batching |
| `view` | `view.hpp` `view.cpp` | Projection and viewport matrix construction |
| `timer` | `timer.hpp` `timer.cpp` | Profiling helpers |
| `shared` | `shared.hpp` | Compiler/arch detection, `force_inline`, `USE_SIMD` |

---

## Building Demo

Requires C++17. CMake ≥ 3.15.

```bash
# Desktop (SDL2 demo)
git clone https://github.com/ballotz/blib3d
git clone https://github.com/libsdl-org/SDL -b SDL2
cd blib3d/demo/sdl
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/sdl-demo
```

For the NXP MIMXRT1176 target, open `demo/mimxrt1176_lcd/` in MCUXpresso IDE.

---

## Usage example

The minimal setup to get a solid-colored triangle on screen can be found in

    demo/source/triangle.hpp
    demo/source/triangle.cpp

```cpp
#include "../../source/render.hpp"
#include "../../source/view.hpp"

namespace triangle
{

blib3d::render::renderer renderer;

//------------------------------------------------------------------------------

// --- Geometry: one triangle, vertices in world space ---
//     x       y      z
float coords[] =
{
    0.0f,  0.5f,  1.0f,   // top center
   -0.5f, -0.5f,  1.0f,   // bottom left
    0.5f, -0.5f,  1.0f,   // bottom right
};
blib3d::render::face faces[] = { {0, 3} }; // index=0, count=3

//------------------------------------------------------------------------------

int32_t screen_width{ 0 };
int32_t screen_height{ 0 };

void setup(int32_t width, int32_t height)
{
    screen_width = width;
    screen_height = height;

    renderer.set_frame_clear_color({ 0x00, 0x00, 0x00, 0xFF });
    renderer.set_frame_clear_depth(0);

    blib3d::math::mat4x4 mat_proj;
    blib3d::math::mat4x4 mat_view;

    float screen_ratio{ (float)screen_width / (float)screen_height };
    blib3d::view::make_projection_perspective(
        mat_proj,
        screen_ratio,
        blib3d::math::pi / 2.f,
        blib3d::view::PROJECTION_Y);
    blib3d::view::make_viewport(mat_view, (float)screen_width, (float)screen_height);

    renderer.set_geometry_transform(mat_proj);
    renderer.set_frame_transform(mat_view);

    renderer.set_geometry_coord(coords, 3);
    renderer.set_geometry_face(faces, 1);
    renderer.set_geometry_back_cull(false);

    renderer.set_fill_type(blib3d::render::renderer::FILL_SOLID);
    renderer.set_fill_color({ 64, 128, 255, 255 });
    renderer.set_shade_type(blib3d::render::renderer::SHADE_NONE);
    renderer.set_blend_type(blib3d::render::renderer::BLEND_NONE);
}

//------------------------------------------------------------------------------

void draw(uint32_t* pixels, float* zbuffer, int32_t stride)
{
    renderer.set_frame_data(screen_width, screen_height, stride, zbuffer, (blib3d::raster::ARGB*)pixels);
    renderer.render_begin();

    renderer.render_clear_frame();
    renderer.render_clear_depth();

    renderer.render_draw();

    renderer.render_end();
}

} // namespace demo
```

---

## License

Apache 2.0 — see [LICENSE](LICENSE).

*Copyright 2019 Alessio Ballotti*
