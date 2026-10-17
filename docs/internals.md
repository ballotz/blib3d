# blib3d internals

This document describes how the renderer works inside: data flow, algorithms, numeric formats and memory layout. For usage, see the [README](../README.md).

- [Module layering](#module-layering)
- [Anatomy of a draw call](#anatomy-of-a-draw-call)
- [Transform](#transform)
- [Clipping](#clipping)
- [Projection and depth](#projection-and-depth)
- [Scan conversion](#scan-conversion)
- [Attribute gradients](#attribute-gradients)
- [Span loop and perspective correction](#span-loop-and-perspective-correction)
- [Pixel pipeline specialization](#pixel-pipeline-specialization)
- [Textures](#textures)
- [Packed color math](#packed-color-math)
- [Lighting](#lighting)
- [Hierarchical Z occlusion](#hierarchical-z-occlusion)
- [Strip rendering](#strip-rendering)
- [Wireframe](#wireframe)
- [Memory budget](#memory-budget)
- [Adding a mode](#adding-a-mode)
- [Known limitations](#known-limitations)

---

## Module layering

Each layer uses only the layers below it.

```
┌─────────────────────────────────────────────────────────────┐
│                         render                              │
│   renderer class · attribute sources · clip · batch flush   │
├─────────────────┬───────────────────────────────────────────┤
│     raster      │              raster_light                 │
│  scan · span ·  │  ambient · directional · point · spot     │
│  fill · blend · │                                           │
│  depth · HZB    │                                           │
├─────────────────┴───────────────────────────────────────────┤
│                          math                               │
│   vec2/3/4 · mat3x3/4x4 · SSE · powfast · log2 · box/plane  │
├─────────────────────────────────────────────────────────────┤
│                         shared                              │
│              arch detect · compiler macros                  │
└─────────────────────────────────────────────────────────────┘

  view   ─── uses math · projection, viewport, strip, frustum matrices
  timer  ─── standalone · std::chrono or DWT cycle counter
```

`raster` is split into:

| File | Content |
|---|---|
| `raster.hpp` | `config` (everything the rasterizer needs for one batch), mode flag bits, occlusion types |
| `raster.cpp` | scan conversion, span loops for every mode, factory, wireframe, HZB |
| `raster_interp.hpp` | per-polygon gradient setup and back-face test |
| `raster_fill.hpp` | blend, depth, texture sample and mask policies, packed color helpers |
| `raster_light.hpp` | `light` struct and light models |

---

## Anatomy of a draw call

`renderer::render_draw()` does the following:

1. **Select attribute sources.** From the fill and shade modes it builds a list of up to 4 streams (pointer, component count, stride). `coord` is always first. For `SHADE_LIGHT`, `coord` is read a second time as an attribute, together with `norm`.
2. **For each face** (directly or through the face index array):
   1. If the batch has no room for another face, flush it to the rasterizer.
   2. Transform the position to clip space and copy the attributes into `render_buffer[0]`.
   3. Clip. Faces with fewer than 3 vertices left are dropped.
   4. Apply the frame transform, divide by `w`, store `1/w`, premultiply attributes by `1/w`.
   5. Append the vertices to the batch.
3. Flush the remaining batch.

The vertex layout seen by the rasterizer is `x y z 1/w` followed by the attributes, all premultiplied by `1/w`:

| Fill | Shade | Attributes after `x y z 1/w` |
|---|---|---|
| solid | none | – |
| solid | vertex | `sr sg sb` |
| solid | lightmap | `su sv` |
| solid | light | `px py pz nx ny nz` |
| vertex | none | `fr fg fb fa` |
| vertex | vertex / lightmap / light | `fr fg fb fa` + shade attributes |
| texture | none | `fs ft` |
| texture | vertex / lightmap / light | `fs ft` + shade attributes |

`FILL_NONE` uses only `z`.

The batch is flushed either when the face buffer (32 faces) is full or when the geometry buffer (1024 floats) cannot hold another worst-case clipped polygon (12 vertices × component count). With 14 components the second limit can trigger before 32 faces.

---

## Transform

`set_geometry_transform()` and `set_frame_transform()` store their matrices **transposed**. The transposed matrix holds the columns of `M` contiguously, so `v' = M * v` becomes

```
v' = col0 * x + col1 * y + col2 * z + col3
```

four 4-wide multiply-adds on contiguous memory. That is what the SSE path in `math::mul4x4t_3` / `mul4x4t_4` does with one broadcast per input component; the scalar path uses the same layout.

---

## Clipping

Clipping runs in homogeneous clip space, before the divide, with Sutherland–Hodgman (`clip_face` in `render.cpp`).

**Planes.** By default 5: `w ≥ w_min`, `x ≥ -w`, `x ≤ w`, `y ≥ -w`, `y ≤ w`. With `BLIB3D_CLIP_Z` also `z ≥ -w` and `z ≤ w`.

The near clip is done against `w = w_min` (0.01) instead of `z = 0`. Every vertex that survives has `w > 0`, so the later divide by `w` is always safe, and the x/y planes keep geometry behind the camera from wrapping around.

**Early out.** Each vertex gets an outcode with one bit per plane. In one pass:

```
any_out = OR  of all outcodes   → 0: fully inside, no clipping
all_out = AND of all outcodes   → non-zero: fully outside one plane, discard
```

Most faces take one of these two exits. Planes that no vertex crosses are skipped during clipping as well.

**Ping-pong.** Two vertex buffers alternate as input and output, one plane at a time:

```
buffer[0] → plane 0 → buffer[1] → plane 1 → buffer[0] → ...
```

`render_buffer[2][12][14]` lives inside `renderer`; no allocation.

**Intersection.** For a plane `c = γ·w` (with `γ = ±1` and `c` = x, y or z) the edge parameter is

```
α = (γ·w0 - c0) / ((c1 - c0) - γ·(w1 - w0))
```

and for the `w_min` plane `α = (w_min - w0) / (w1 - w0)`. All components are interpolated linearly with `α`. New vertices get a fresh outcode for the following planes.

**Vertex budget.** Clipping a convex polygon against one plane adds at most one vertex. The rasterizer accepts 12 vertices, so the input is limited to `12 - planes` = 7 (5 with `BLIB3D_CLIP_Z`).

---

## Projection and depth

`make_projection_perspective()` produces

```
| xs  0  0  0 |
|  0 ys  0  0 |
|  0  0  0 -1 |
|  0  0  1  0 |
```

so `z_clip = -1` and `w = z_view`. After the divide the stored depth is `-1/z_view`: negative, larger in magnitude when closer. The depth test passes when `buffer > depth`, i.e. when the new value is smaller (closer). Clearing to `0` means "infinitely far".

This is a reciprocal (1/z) depth, the same quantity that is linear in screen space, so it can be interpolated with a plain gradient and needs no per-pixel divide. Precision is concentrated near the camera.

`make_projection_ortho()` keeps `z` and `w = 1`, so depth is `z_view`, with the same "smaller is closer" rule.

`make_viewport()` maps NDC `[-1, 1]` to `[0, w] × [0, h]` with y pointing down.

---

## Scan conversion

The rasterizer walks polygon edges in float (`scan::scan_face`).

**Pixel centers.** Sample points are at pixel centers:

```c
real_to_raster(v) = ceil(v - 0.5)
raster_to_real(v) = v + 0.5
```

Rows run from `real_to_raster(y_top)` to `real_to_raster(y_bottom)` exclusive, and spans from `real_to_raster(x_left)` to `real_to_raster(x_right)` exclusive. A pixel is covered when its center is inside the polygon or on its top or left edge (top-left rule).

**Edge walking.** The top and bottom vertices are found first. The left and right chains are then followed from the top, in opposite directions around the polygon, according to the winding. Each edge keeps an `x` and a `dx/dy`, initialized at the first row center below its top vertex and advanced with one addition per row. When a chain reaches the end of its edge, the next edge on that side is set up.

Two polygons sharing an edge walk it from the same top vertex to the same bottom vertex with the same arithmetic, so they produce the same `x` on every row: no cracks and no double hits.

**Robustness.**

- Edges are walked in float, not in fixed point with an integer error term. Integer walking is exact but slower; the float version only needs the guard `if (x0 < x1)` before emitting a span, which absorbs crossing edges near degenerate vertices.
- An edge that goes upward (possible if a nearly degenerate polygon becomes slightly concave after clipping and projection) is treated as horizontal and skipped, so the loop always terminates.

---

## Attribute gradients

Every attribute `c` divided by `w` (and `1/w` itself, and depth) is a linear function of screen position over a planar polygon:

```
c(x, y) = g.dx * x + g.dy * y + g.d
```

`interp_setup_face` computes `g` for the whole polygon at once, which is what allows n-gons without triangulating. It fans from vertex 0 and sums 2D cross products:

```
area2xy  += cross( x1-x0, y1-y0,  x2-x0, y2-y0 )
area2cx  += cross( c1-c0, x1-x0,  c2-c0, x2-x0 )
area2cy  += cross( c1-c0, y1-y0,  c2-c0, y2-y0 )

g.dx = area2cy / area2xy
g.dy = -area2cx / area2xy
g.d  = c0 - g.dx * x0 - g.dy * y0
```

Summing over the fan is exact for a planar convex polygon.

The same pass decides visibility:

- `|area2xy| < 1` (less than half a pixel of area): discarded.
- The sign of `area2xy` gives the winding. Positive is clockwise on screen (y down). With back-face culling enabled, counter-clockwise faces are discarded.

---

## Span loop and perspective correction

Perspective-correct interpolation needs `c = (c/w) / (1/w)` at every pixel. This uses the same approach as the Quake span renderer: the divide is done only every 16 pixels, with linear interpolation in between.

```
span
├── block (16 px)
│     at block start and end: c = (c/w) * (1 / (1/w))  → 16.16 fixed point
│     step = (end - start) / 16                         → arithmetic shift
│     16 × { use c; c += step }                         → integer add
├── block (16 px)
│     ...
└── last block (1..15 px)
      step = (end - start) / count                      → switch on count
```

- One reciprocal per 16 pixels instead of one per pixel.
- Texture, lightmap and color coordinates are 16.16 fixed point inside the block. Depth stays in float and steps with its own gradient.
- The full-block step uses `div_shift_round_zero` (shift with a bias for negative values) and the last block uses `div_fast_round_zero` (a `switch` so each case divides by a constant). Both round toward zero, so `start + count * step` never overshoots the exact value at the end of the block. Coordinates clamped at the block ends (lightmap, colors) therefore stay in range.
- The per-light attributes of `SHADE_LIGHT` (position and normal) are interpolated in float with the same scheme.

The error inside a block is the difference between linear and hyperbolic interpolation over 16 pixels, which is not visible at typical angles.

---

## Pixel pipeline specialization

The pixel loop must not branch on modes. Every combination is a separate type:

```
raster_<fill>_shade_<shade><blend, depth>                    // solid and vertex fill
raster_texture_shade_<shade><sample, blend, depth, mask>     // texture fill
raster_depth<depth>                                          // FILL_NONE
```

That is 13 struct templates (3 fills × 4 shades, plus depth only) and the policies:

| Policy | Types |
|---|---|
| depth | `depth_notest_nowrite` · `depth_notest_write` · `depth_test_nowrite` · `depth_test_write` |
| blend | `blend_none` · `blend_add` · `blend_mul` · `blend_alpha` |
| sample | `sample_nearest` · `sample_bilinear` |
| mask | `mask_texture_off` · `mask_texture_on` |

Policies are structs with static `blib3d_force_inline` functions. In an instantiation the unused ones collapse to nothing (no depth read with `depth_notest_*`, no mask compare with `mask_texture_off`).

Each struct derives from `scan` and implements three virtual functions: `setup` (per batch), `setup_face` (per polygon) and `process_span` (per span). The pixel loop inside `process_span` is written out by hand with all state in locals, so the compiler keeps it in registers.

**Factory.** `scan_faces()` decodes `config.flags` through a chain of `switch` and `if constexpr`:

```
create_raster → depth → fill → shade → blend → texmask → texfilter
```

The chosen type is constructed with placement new into a stack buffer sized by a union of all raster types (`raster_pool`). One virtual call per span is the only dispatch cost.

---

## Textures

**Addressing.** Coordinates are 16.16 texels (the texture gradients are pre-scaled by the texture size in `setup_face`). With power-of-two sizes, wrap and row offset need no multiply:

```
index = ((s & smask) >> 16) + ((t & tmask) >> tshift)
smask  = (width  - 1) << 16
tmask  = (height - 1) << 16
tshift = 16 - log2(width)        // (t >> 16) * width in one shift
texel  = lut[data[index]]
```

**Bilinear.** `sample_bilinear` subtracts half a texel (`0x8000`) from both coordinates, so texel centers fall on integer positions, then reads `(s, t)`, `(s+1, t)`, `(s, t+1)`, `(s+1, t+1)` with the same wrap masks and blends them with `bilinear62` (see below).

**Mask.** `mask_texture_on` keeps the texel when bit 31 is set, i.e. when the palette alpha is ≥ 128. It is applied after sampling, so with bilinear filtering it thresholds the filtered alpha.

**Mip selection.** With `TEXMIP_FACE`, `setup_face` computes one level per polygon:

```
A_xy = screen area of the polygon (pixels²)
A_uv = texture area of the polygon (texels² at level 0)
l    = sqrt(|A_uv / A_xy|)              // texels per pixel, linear
level = clamp(ceil(log2(l)), 0, last)
```

`ceil` biases toward the next coarser level (blurrier rather than aliased). The level's base pointer comes from a table built once per batch from the contiguous chain; the chain stops when either dimension reaches 1 or after 16 levels.

---

## Packed color math

Colors are `uint32_t` `0xAARRGGBB`. Channels are processed together with ordinary integer instructions (SWAR).

**Modulation.** Shading multiplies each channel by an 8-bit factor and keeps the high byte:

```
out = ((c * s) >> 8) + 1        // per channel, the +0x00010101 bias maps 255 * 255 to 255
```

**Saturating add** (`adds_X888`). Add all channels at once, detect the carries out of each byte, turn them into `0xFF` masks:

```
s = a + b
c = (a ^ b ^ s) & 0x01010100     // carry into bits 8, 16, 24
m = c - (c >> 8) + 0xFF000000    // 0xFF in every byte that overflowed
r = (s - c) | m
```

**Multiply** (`mul_X888`). Three multiplies, one per color channel, aligned so each product lands in its own byte range.

**Alpha** (`alpha_8888`). Red and blue are processed together in one multiply (`0x00FF00FF` mask), green in another:

```
a1 = src_alpha, a0 = 256 - a1
rb = (dst & 0x00FF00FF) * a0 + (src & 0x00FF00FF) * a1
g  = (dst & 0x0000FF00) * a0 + (src & 0x0000FF00) * a1
```

The result alpha is forced to `0xFF`.

**Bilinear, 4 multiplies** (`bilinear62`, textures). The four texels are pre-shifted to 6 bits per channel and the weights are 2-bit (0..4, summing to 4):

```
w3 = (a0 * a1) >> 2
w2 = a1 - w3
w1 = a0 - w3
w0 = 4 + w3 - a0 - a1
r  = ((v0 & 0xFCFCFCFC) >> 2) * w0 + ((v1 & 0xFCFCFCFC) >> 2) * w1
   + ((v2 & 0xFCFCFCFC) >> 2) * w2 + ((v3 & 0xFCFCFCFC) >> 2) * w3
```

Each channel is at most 63 × 4 = 252 after the sum, so no byte carries into the next: all four channels of all four texels need 4 multiplies. A full 8-bit filter needs 8 (two channel pairs × 4 texels). The cost is precision: 6 bits per channel and 4 sub-texel positions per axis.

**Bilinear, 8-bit** (`bilinear88`, lightmaps). Same structure with 8-bit weights and channels split into two `0x00FF00FF` pairs. Lightmaps are low resolution and magnified a lot, so they need the extra sub-texel precision.

---

## Lighting

### Sampling cadence

Lightmap and per-pixel light evaluation are expensive, so they run once every 4 pixels (`shade_hold = 4`) and the result is held. The phase is shifted by 2 on odd rows:

```c
shade_counter = ((y & 1 ? shade_hold >> 1 : 0) + x0) & shade_mask;
```

```
row 0:   S . . . S . . . S . . .        S = sample
row 1:   . . S . . . S . . . S .        . = held
row 2:   S . . . S . . . S . . .
row 3:   . . S . . . S . . . S .
```

The samples form a staggered grid, so the holding error does not line up into vertical bands and reads as fine noise instead.

**Depth-break resample.** Bit 31 of `shade_counter` is a "resample now" flag. It is set at the start of each span and whenever a pixel fails the depth test; the next pixel that passes samples immediately and clears it:

```c
if (depth_type::process_test(depth_addr, depth))
{
    if (((shade_counter & shade_mask) == 0) | (shade_counter >> 31))
    {
        shade_counter &= ~0x80000000;
        // sample lightmap or lights
    }
    // shade, blend, write
}
else
{
    shade_counter |= 0x80000000;
}
shade_counter++;
```

Without it, the first visible pixel after an occluded run would reuse a value computed at a different position.

`SHADE_VERTEX` is interpolated per pixel and does not use the hold.

### Light models

All vectors are in the space of the `coord` and `norm` streams. `R = light.position - surface_position`, `d = |R|`, `N` is the interpolated normal (not renormalized).

| Type | Contribution |
|---|---|
| ambient | `intensity` |
| directional | `intensity * max(0, -direction · N)` |
| point | if `d < radius`: `intensity * max(0, R·N / d) / (k0 + k1·d + k2·d²)` |
| spot | point term × `min(1, (cosφ - spot_costh_min) * spot_costh_range_inv)`, where `cosφ = -direction · R / d`, zero outside the cone |

`k0 k1 k2` are `damping[0..2]`. `spot_costh_range_inv` is `1 / (cos_inner - cos_outer)`. The code works on unnormalized `R` and folds the `1/d` factors into the denominator to avoid a divide. The sum of all lights is clamped to 1 per channel, gamma-encoded and scaled to 0..255.

### Gamma with `powfast`

`gamma_set(g)` builds a `powfast_table` for the exponent `1/g`. `powfast` splits the float bits into the top 9 bits (sign and exponent) and the next 9 mantissa bits and returns

```
pow(v) ≈ exponent_table[top 9 bits] * mantissa_table[next 9 bits]
```

two lookups and one multiply, using `pow(2^e · m) = pow(2^e) · pow(m)`. The tables take 4 KB inside `renderer`.

---

## Hierarchical Z occlusion

`occlusion_build_mipchain()` reduces the depth buffer into a pyramid stored right after it:

```
depth buffer   w   × h      (input)
level 0        w/2 × h/2    min of 2×2
level 1        w/4 × h/4    max of 2×2
level 2        w/8 × h/8    max of 2×2
...            down to 1×1 or 16 levels
```

With "smaller is closer", `max` is the farthest depth in the block. Level 0 uses `min` instead: a block counts as covered if any of its 4 pixels is, which hides 1-pixel cracks between occluders. This makes the test slightly aggressive.

`occlusion_test_rect(min, max, depth_min)`:

1. Convert the rectangle to pixels, clip it to the frame, map it to level 0.
2. Halve it until it spans at most 2 cells in one of the two directions (or the last level is reached).
3. Read the cells under the rectangle and take the maximum.
4. Occluded if `depth_min > max`: the nearest point of the object is farther than the farthest occluder in the area.

`depth_min` is in depth buffer units (`-1/z` for perspective). For a bounding box, project its corners, take the screen rectangle and the smallest depth.

The pyramid assumes `stride == width` (see `occlusion_config`, TODO), and needs about `w*h/3` floats after the depth buffer.

---

## Strip rendering

`make_pre_matrix_strip()` rescales clip-space y so that the band `[strip_y_src, strip_y_src + strip_height)` of the full frame covers `[-1, 1]`. The regular clipper then cuts everything outside the band. `make_post_matrix_strip()` maps it back to rows `[strip_y_dst, strip_y_dst + strip_height)` of a frame-sized viewport. With `strip_y_dst = 0` the band lands in the first `strip_height` rows, so color and depth buffers only need that many rows.

```cpp
math::mat4x4 pre, post;     // pre = projection * view * model, viewport for full W × H
for (int y = 0; y < H; y += h)
{
    math::mat4x4 pre_s, post_s;
    view::make_pre_matrix_strip(pre_s, pre, (float)y, (float)h, (float)H);
    view::make_post_matrix_strip(post_s, post, 0.f, (float)h, (float)H);
    r.set_geometry_transform(pre_s);
    r.set_frame_transform(post_s);
    r.set_frame_data(W, h, W, strip_depth, strip_color);
    r.render_clear_frame();
    r.render_clear_depth();
    r.render_draw();
    // send strip_color to display rows [y, y + h)
}
```

`H` must be a multiple of `h`, or the last band set up with its real height. All geometry is transformed and clipped once per band.

---

## Wireframe

With `set_wireframe(true)`, each batch goes through `scan_faces_wireframe`:

1. Draw every polygon edge as a 1-pixel line (float DDA along the major axis, pixel centers), color `0xFFFFFFFF`, no depth test.
2. Run the depth-only rasterizer with the current depth mode, so depth is still written for later passes.

---

## Memory budget

All memory is either inside `renderer` or on the stack during `render_draw()`.

| Buffer | Size |
|---|---|
| `render_buffer` (clip ping-pong) | 2 × 12 × 14 floats = 1344 B |
| `raster_geometry_buffer` (batch) | 1024 floats = 4 KB |
| `raster_vertex_count_buffer` | 32 × 4 B |
| `gamma_table` | 2 × 512 floats = 4 KB |
| raster object (stack) | size of the largest raster type |

Caller-owned: color buffer (4 B/pixel), depth buffer (4 B/pixel, plus about 1/3 for the HZB if used), textures, palettes, lightmaps, vertex data.

---

## Adding a mode

For example a new blend or fill mode:

1. `raster.hpp`: add the value to the mode bits (`*_SHIFT`, `*_MASK`, values). Widen the field if it is full.
2. `render.hpp`: add the matching value to the public `renderer` enum; the setter already shifts it in place.
3. `render.cpp`: if the mode needs a new vertex stream, add it in the source selection at the top of `render_draw()` and a setter for it.
4. `raster_fill.hpp`: for a new policy, add a struct with the same static interface as its siblings.
5. `raster.cpp`: for a new fill or shade, write the raster struct (copy the closest one), add it to the factory chain and to the `raster_pool` union in `scan_faces()`.

Every new policy multiplies the number of instantiations, which affects code size on the MCU.

---

## Known limitations

- **Polygon size.** 7 input vertices (5 with `BLIB3D_CLIP_Z`).
- **No far plane** unless `BLIB3D_CLIP_Z`; depth is reciprocal, so distant geometry loses precision but does not overflow.
- **Mip level per face.** Large faces at grazing angles use one level everywhere.
- **Texture bilinear precision.** 6 bits per channel and 4 sub-texel positions per axis.
- **Lighting cadence.** Lights are held for 4 pixels; fast spatial changes (small spot cones, close point lights) can show blockiness.
- **Diffuse only.** No specular term.
- **Wireframe.** Fixed white color, lines are not depth tested.
- **HZB.** Assumes `stride == width`; level 0 is non-conservative.
- **Repeat only.** Textures always wrap; lightmaps always clamp.
- **One state per draw.** Change modes between `render_draw()` calls.
