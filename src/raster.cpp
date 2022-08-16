#include "raster.hpp"
#include "raster_interp.hpp"
#include "raster_fill.hpp"

namespace lib3d::raster
{

//------------------------------------------------------------------------------

struct abstract_raster
{
    bool is_clockwise;

    virtual bool setup_face(const float* pv[], uint32_t vertex_count) = 0;
    virtual void process_span(int32_t y, int32_t x0, int32_t x1) = 0;
};

//------------------------------------------------------------------------------

/*
          v0
          *
         *  *
        *     * v1
       *  *
   v2 *

    (1)
    exact edge rasterization could be done using
        * fixed point integer x and y coordinates with subpixel precision
        * integer edge walking with an integer error term
    this avoids floating point error to cause, in some nearly degenerate triangles
        * sign changes in the computation of the cross product (winding test, not here anymore)
        * error accumulation to make some edges intersect before the endpoint
    but using integer coordinates along with integer line interpolation for edges
    is slower than using float with the x[0] < x[1] check

    (2)
    having surfaces with more than 3 vertices introduced the issue of handling concave shapes
    this should not happen, but if it does for numerical precision or whatever
    this could cause the scanning loop to not terminate
    to avoid this situation backward tilted edges are treated as horizontal and skipped
*/
void scan_face(const float* v[], int32_t num_vertices, abstract_raster* r)
{
    struct edge
    {
        float x;
        float dx_dy;

        force_inline void setup(const float* v0, const float* v1, float y0)
        {
            dx_dy = (v1[0] - v0[0]) / (v1[1] - v0[1]);
            x = v0[0] + (y0 - v0[1]) * dx_dy;
        }

        force_inline void advance()
        {
            x += dx_dy;
        }
    };

    struct util
    {
        static force_inline int32_t wrap(int32_t v, int32_t last)
        {
            if (v < 0)
                return last;
            if (v > last)
                return 0;
            return v;
        };
    };

    int32_t vi{ r->is_clockwise ? -1 : 1 }; // vertex index increment

    int32_t vt{ 0 }; // top vertex index
    int32_t vb{ 0 }; // bottom vertex index
    for (int32_t n{ 1 }; n < num_vertices; ++n)
    {
        if (v[vt][1] > v[n][1])
            vt = n;
        if (v[vb][1] < v[n][1])
            vb = n;
    }

    int32_t ylimit[2]
    {
        real_to_raster(v[vt][1]),
        real_to_raster(v[vb][1])
    };

    if (ylimit[0] == ylimit[1])
        return;

    int32_t last_vertex{ num_vertices - 1 };

    int32_t vl[2]{ vt, util::wrap(vt + vi, last_vertex) }; // left edge vertex indexes
    int32_t vr[2]{ vt, util::wrap(vt - vi, last_vertex) }; // right edge vertex indexes

    int32_t yl[2]{ ylimit[0], real_to_raster(v[vl[1]][1]) }; // left edge y
    int32_t yr[2]{ ylimit[0], real_to_raster(v[vr[1]][1]) }; // right edge y

    edge e[2]; // e[0] left, e[1] right

    int32_t y{ ylimit[0] };
    int32_t yend{ y };
    for (;;)
    {
        if (y == yend)
        {
            if (y >= yl[1]) // advance left edge (2)
            {
                vl[0] = util::wrap(vl[0] + vi, last_vertex);
                vl[1] = util::wrap(vl[1] + vi, last_vertex);
                if (yl[1] == ylimit[1]) // next top is polygon bottom
                    break;
                yl[0] = yl[1];
                yl[1] = real_to_raster(v[vl[1]][1]);
                continue;
            }
            if (y >= yr[1]) // advance right edge (2)
            {
                vr[0] = util::wrap(vr[0] - vi, last_vertex);
                vr[1] = util::wrap(vr[1] - vi, last_vertex);
                if (yr[1] == ylimit[1]) // next top is polygon bottom
                    break;
                yr[0] = yr[1];
                yr[1] = real_to_raster(v[vr[1]][1]);
                continue;
            }
            if (y >= yl[0]) // setup left edge (2)
                e[0].setup(v[vl[0]], v[vl[1]], raster_to_real(y));
            if (y >= yr[0]) // setup right edge (2)
                e[1].setup(v[vr[0]], v[vr[1]], raster_to_real(y));
            yend = yl[1] < yr[1] ? yl[1] : yr[1]; // next nearest end point
        }
        //assert(y < ylimit[1]);
        int32_t x[2]
        {
            real_to_raster(e[0].x),
            real_to_raster(e[1].x)
        };
        if (x[0] < x[1]) // float error guard (1)
            r->process_span(y, x[0], x[1]);
        e[0].advance();
        e[1].advance();
        ++y;
    }
}

//------------------------------------------------------------------------------

/*
    span fill algorithm
*/
template<typename raster_type>
force_inline void process_span_algo(raster_type& raster, int32_t y, int32_t x0, int32_t x1)
{
    //assert(x0 < x1);
    raster.interp.setup_span(y, x0);
    raster.fill.setup_span(y, x0);
    int32_t n{ x1 - x0 };
    while (n)
    {
        int32_t c{ math::min(n, interp_span_block_size) };
        n -= c;
        raster.interp.setup_subspan(c);
        while (c--)
        {
            raster.fill.write(raster.interp);
            raster.fill.advance();
            raster.interp.advance();
        }
    }
}

//------------------------------------------------------------------------------

struct raster_depth : public abstract_raster
{
    raster_depth(const config* c)
    {
        interp.setup(c);
        fill.setup(c);
    }

    // abstract_raster

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        bool result{ interp.setup_face(pv, vertex_count, is_clockwise) };
        if (result)
            fill.setup_face();
        return result;
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        process_span_algo(*this, y, x0, x1);
    }

    // data

    interp<1> interp;

    fill_span_depth fill;
};

//------------------------------------------------------------------------------

template<typename fill_span_type>
struct raster_solid_shade_none : public abstract_raster
{
    raster_solid_shade_none(const config* c)
    {
        interp.setup(c);
        fill.setup(c);
    }

    // abstract_raster

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        bool result{ interp.setup_face(pv, vertex_count, is_clockwise) };
        if (result)
            fill.setup_face();
        return result;
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        process_span_algo(*this, y, x0, x1);
    }

    // data

    interp<1> interp;

    fill_span_type fill;
};

template<typename fill_span_type>
struct raster_solid_shade_vertex : public abstract_raster
{
    raster_solid_shade_vertex(const config* c)
    {
        interp.setup(c);
        fill.setup(c);
    }

    // abstract_raster

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        bool result{ interp.setup_face(pv, vertex_count, is_clockwise) };
        if (result)
            fill.setup_face();
        return result;
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        process_span_algo(*this, y, x0, x1);
    }

    // data

    interp<5> interp;

    fill_span_type fill;
};

template<typename fill_span_type>
struct raster_vertex_shade_none : public abstract_raster
{
    raster_vertex_shade_none(const config* c)
    {
        interp.setup(c);
        fill.setup(c);
    }

    // abstract_raster

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        bool result{ interp.setup_face(pv, vertex_count, is_clockwise) };
        if (result)
            fill.setup_face();
        return result;
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        process_span_algo(*this, y, x0, x1);
    }

    // data

    interp<6> interp;

    fill_span_type fill;
};

template<typename fill_span_type>
struct raster_vertex_shade_vertex : public abstract_raster
{
    raster_vertex_shade_vertex(const config* c)
    {
        interp.setup(c);
        fill.setup(c);
    }

    // abstract_raster

    bool setup_face(const float* pv[], uint32_t vertex_count) override
    {
        bool result{ interp.setup_face(pv, vertex_count, is_clockwise) };
        if (result)
            fill.setup_face();
        return result;
    }

    void process_span(int32_t y, int32_t x0, int32_t x1) override
    {
        process_span_algo(*this, y, x0, x1);
    }

    // data

    interp<9> interp;

    fill_span_type fill;
};

//------------------------------------------------------------------------------

void scan_faces(const config* c)
{
    abstract_raster* r{};

    //------------------------------------//

    union raster_pool
    {
        raster_depth r0;
        raster_solid_shade_none<fill_span_solid_shade_none_blend_none> r1;
        raster_solid_shade_none<fill_span_solid_shade_none_blend_add> r2;
        raster_solid_shade_none<fill_span_solid_shade_none_blend_mul> r3;
        raster_solid_shade_none<fill_span_solid_shade_none_blend_alpha> r4;
        raster_solid_shade_vertex<fill_span_solid_shade_vertex_blend_none> r5;
        raster_solid_shade_vertex<fill_span_solid_shade_vertex_blend_add> r6;
        raster_solid_shade_vertex<fill_span_solid_shade_vertex_blend_mul> r7;
        raster_solid_shade_vertex<fill_span_solid_shade_vertex_blend_alpha> r8;
        raster_vertex_shade_none<fill_span_vertex_shade_none_blend_none> r9;
        raster_vertex_shade_none<fill_span_vertex_shade_none_blend_add> r10;
        raster_vertex_shade_none<fill_span_vertex_shade_none_blend_mul> r11;
        raster_vertex_shade_none<fill_span_vertex_shade_none_blend_alpha> r12;
        raster_vertex_shade_vertex<fill_span_vertex_shade_vertex_blend_none> r13;
        raster_vertex_shade_vertex<fill_span_vertex_shade_vertex_blend_add> r14;
        raster_vertex_shade_vertex<fill_span_vertex_shade_vertex_blend_mul> r15;
        raster_vertex_shade_vertex<fill_span_vertex_shade_vertex_blend_alpha> r16;
    };
    alignas(alignof(raster_pool)) uint8_t raster[sizeof(raster_pool)];

    switch (c->flags)
    {
    case (FILL_NONE):
        r = new (raster) raster_depth(c);
        break;
    case (FILL_SOLID | SHADE_NONE | BLEND_NONE):
        r = new (raster) raster_solid_shade_none<fill_span_solid_shade_none_blend_none>(c);
        break;
    case (FILL_SOLID | SHADE_NONE | BLEND_ADD):
        r = new (raster) raster_solid_shade_none<fill_span_solid_shade_none_blend_add>(c);
        break;
    case (FILL_SOLID | SHADE_NONE | BLEND_MUL):
        r = new (raster) raster_solid_shade_none<fill_span_solid_shade_none_blend_mul>(c);
        break;
    case (FILL_SOLID | SHADE_NONE | BLEND_ALPHA):
        r = new (raster) raster_solid_shade_none<fill_span_solid_shade_none_blend_alpha>(c);
        break;
    case (FILL_SOLID | SHADE_VERTEX | BLEND_NONE):
        r = new (raster) raster_solid_shade_vertex<fill_span_solid_shade_vertex_blend_none>(c);
        break;
    case (FILL_SOLID | SHADE_VERTEX | BLEND_ADD):
        r = new (raster) raster_solid_shade_vertex<fill_span_solid_shade_vertex_blend_add>(c);
        break;
    case (FILL_SOLID | SHADE_VERTEX | BLEND_MUL):
        r = new (raster) raster_solid_shade_vertex<fill_span_solid_shade_vertex_blend_mul>(c);
        break;
    case (FILL_SOLID | SHADE_VERTEX | BLEND_ALPHA):
        r = new (raster) raster_solid_shade_vertex<fill_span_solid_shade_vertex_blend_alpha>(c);
        break;
    case (FILL_VERTEX | SHADE_NONE | BLEND_NONE):
        r = new (raster) raster_vertex_shade_none<fill_span_vertex_shade_none_blend_none>(c);
        break;
    case (FILL_VERTEX | SHADE_NONE | BLEND_ADD):
        r = new (raster) raster_vertex_shade_none<fill_span_vertex_shade_none_blend_add>(c);
        break;
    case (FILL_VERTEX | SHADE_NONE | BLEND_MUL):
        r = new (raster) raster_vertex_shade_none<fill_span_vertex_shade_none_blend_mul>(c);
        break;
    case (FILL_VERTEX | SHADE_NONE | BLEND_ALPHA):
        r = new (raster) raster_vertex_shade_none<fill_span_vertex_shade_none_blend_alpha>(c);
        break;
    case (FILL_VERTEX | SHADE_VERTEX | BLEND_NONE):
        r = new (raster) raster_vertex_shade_vertex<fill_span_vertex_shade_vertex_blend_none>(c);
        break;
    case (FILL_VERTEX | SHADE_VERTEX | BLEND_ADD):
        r = new (raster) raster_vertex_shade_vertex<fill_span_vertex_shade_vertex_blend_add>(c);
        break;
    case (FILL_VERTEX | SHADE_VERTEX | BLEND_MUL):
        r = new (raster) raster_vertex_shade_vertex<fill_span_vertex_shade_vertex_blend_mul>(c);
        break;
    case (FILL_VERTEX | SHADE_VERTEX | BLEND_ALPHA):
        r = new (raster) raster_vertex_shade_vertex<fill_span_vertex_shade_vertex_blend_alpha>(c);
        break;
    default:
        break;
    }

    //------------------------------------//

    if (r == nullptr)
        return;

    uint32_t num_faces{ c->num_faces };
    const face* faces{ c->faces };
    const float* vertex_data{ c->vertex_data };
    uint32_t vertex_stride{ c->vertex_stride };

    while (num_faces)
    {
        uint32_t vertex_index{ faces->index };
        uint32_t vertex_count{ faces->count };
        faces++;

        const float* pv[num_max_vertices];
        const float* p{ &vertex_data[vertex_index] };
        for (uint32_t nv{}; nv < vertex_count; ++nv)
        {
            pv[nv] = p;
            p += vertex_stride;
        }

        if (r->setup_face(pv, vertex_count))
            scan_face(pv, vertex_count, r);

        num_faces--;
    }
}

} // namespace lib3d::raster
