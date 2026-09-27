#pragma once
/*  THIN WALLS - rooms that are not boxes.

    The image-source method in Engine.cpp mirrors a source across a shoebox by
    index arithmetic, which is exact and fast and cannot represent a wall that is
    not parallel to an axis. This is the general version: a room is a list of
    PLANAR surfaces, a candidate path is a sequence of them, and an image is
    mirrored across each in turn and then validated by tracing back from the
    listener and checking every reflection point lands on the surface it is
    supposed to and that nothing is in the way.

    It is used ONLY for a room whose walls have been broken. A room still square
    keeps the shoebox path, so every room that sounds a particular way today
    still does, exactly.

    What a broken wall is: one of a room's vertical walls, split at a point along
    it, with that point pushed out of (or into) the room. The two panels are then
    no longer parallel to the wall facing them, so reflections stop landing at
    arithmetic multiples of the room dimension and the comb never builds. Pushing
    the point OUT disperses; pushing it IN makes a concave wall, which focuses,
    and is worse than leaving it flat - the panel says so.

    Header-only and free of engine state, so the bench can test the geometry on
    its own.
*/

#include "Engine.h"
#include <algorithm>
#include <cmath>

namespace tw
{

//------------------------------------------------------------------------------
// which two walls of each room may be broken: the ones that carry no doorway.
// 0 = x0 (west), 1 = x1 (east), 2 = y0 (south), 3 = y1 (north).
constexpr int BREAK_WALL[NUM_ROOMS][2] =
{
    { 0, 2 },     // LARGE: its doors are on the north and east walls
    { 0, 3 },     // SMALL: its doors are on the south and east walls
    { 1, 3 },     // GIANT: its doors are both on the west wall
};

struct WallBreak
{
    float along = 0.5f;    // where along the wall, 0..1
    float push  = 0.0f;    // metres out of the room; negative is into it (concave)
    bool  flat() const { return std::abs (push) < 1.0e-4f; }
};

struct RoomBreaks
{
    WallBreak w[2];
    bool flat() const { return w[0].flat() && w[1].flat(); }
};

//------------------------------------------------------------------------------
struct Surface
{
    Vec3  p;            // a point on the plane
    Vec3  n;            // unit normal, pointing INTO the room
    int   wallId = 0;   // 0..3 the original wall it belongs to, 4 floor, 5 ceiling
    // a vertical wall is bounded by its plan segment and by [0, h];
    // the floor and the ceiling are bounded by the room's plan polygon
    Vec3  a, b;
    float h = 2.5f;
    bool  vertical = true;
};

struct RoomGeom
{
    Surface s[12];
    int     n = 0;
    float   px[10], py[10];      // the plan polygon, anticlockwise
    int     np = 0;
    float   height = 2.5f;
    float   area = 0;            // plan area, m^2
    float   perimeter = 0;
    float   volume() const { return area * height; }
    float   surface() const { return 2.0f * area + perimeter * height; }
};

//------------------------------------------------------------------------------
/*  Build a room's surfaces. Walking the four corners anticlockwise and emitting
    one or two panels per wall, so a broken wall simply becomes two entries and
    everything downstream is unchanged. */
inline RoomGeom buildRoomGeom (int room, const RoomBreaks& br)
{
    const Room& R = ROOMS[room];
    RoomGeom g;
    g.height = R.h;

    // the four corners, anticlockwise seen from above, and the wall each edge is
    const float cx[4] = { R.x0, R.x1, R.x1, R.x0 };
    const float cy[4] = { R.y0, R.y0, R.y1, R.y1 };
    const int   edgeWall[4] = { 2, 1, 3, 0 };          // south, east, north, west
    // the outward normal of each edge
    const float enx[4] = { 0, 1, 0, -1 };
    const float eny[4] = { -1, 0, 1, 0 };

    g.np = 0;
    for (int e = 0; e < 4; ++e)
    {
        const float ax = cx[e], ay = cy[e];
        const float bx = cx[(e + 1) & 3], by = cy[(e + 1) & 3];
        g.px[g.np] = ax; g.py[g.np] = ay; ++g.np;

        // is this edge one of the two breakable walls, and is it broken?
        int which = -1;
        for (int k = 0; k < 2; ++k) if (BREAK_WALL[room][k] == edgeWall[e]) which = k;
        if (which >= 0 && ! br.w[which].flat())
        {
            const float t = std::min (0.85f, std::max (0.15f, br.w[which].along));
            const float mx = ax + (bx - ax) * t + enx[e] * br.w[which].push;
            const float my = ay + (by - ay) * t + eny[e] * br.w[which].push;
            g.px[g.np] = mx; g.py[g.np] = my; ++g.np;
        }
    }

    // the polygon's area and perimeter (shoelace)
    g.area = 0; g.perimeter = 0;
    for (int i = 0; i < g.np; ++i)
    {
        const int j = (i + 1) % g.np;
        g.area += g.px[i] * g.py[j] - g.px[j] * g.py[i];
        g.perimeter += std::sqrt ((g.px[j] - g.px[i]) * (g.px[j] - g.px[i]) + (g.py[j] - g.py[i]) * (g.py[j] - g.py[i]));
    }
    g.area = std::abs (g.area) * 0.5f;

    // one surface per polygon edge, normal pointing into the room
    g.n = 0;
    int corner = 0;
    for (int e = 0; e < 4; ++e)
    {
        int panels = 1;
        int which = -1;
        for (int k = 0; k < 2; ++k) if (BREAK_WALL[room][k] == edgeWall[e]) which = k;
        if (which >= 0 && ! br.w[which].flat()) panels = 2;
        for (int q = 0; q < panels; ++q)
        {
            const int i = corner, j = (corner + 1) % g.np;
            Surface& s = g.s[g.n];
            s.a = Vec3 (g.px[i], g.py[i], 0.0f);
            s.b = Vec3 (g.px[j], g.py[j], 0.0f);
            s.p = s.a;
            const float dx = g.px[j] - g.px[i], dy = g.py[j] - g.py[i];
            const float len = std::sqrt (dx * dx + dy * dy);
            // anticlockwise polygon: the inward normal is the left of the edge
            s.n = len > 1e-6f ? Vec3 (-dy / len, dx / len, 0.0f) : Vec3 (1, 0, 0);
            s.wallId = edgeWall[e];
            s.h = R.h;
            s.vertical = true;
            ++g.n; ++corner;
        }
    }
    // the floor and the ceiling
    { Surface& s = g.s[g.n++]; s.p = Vec3 (0, 0, 0.0f);  s.n = Vec3 (0, 0, 1);  s.wallId = 4; s.h = R.h; s.vertical = false; }
    { Surface& s = g.s[g.n++]; s.p = Vec3 (0, 0, R.h);   s.n = Vec3 (0, 0, -1); s.wallId = 5; s.h = R.h; s.vertical = false; }
    return g;
}

//------------------------------------------------------------------------------
inline Vec3 mirrorIn (const Vec3& q, const Surface& s)
{
    const float d = (q - s.p).dot (s.n);
    return q - s.n * (2.0f * d);
}

// where the segment P->Q meets the surface's plane; false if it does not
inline bool planeHit (const Vec3& P, const Vec3& Q, const Surface& s, Vec3& X, float& t)
{
    const float dp = (P - s.p).dot (s.n), dq = (Q - s.p).dot (s.n);
    const float den = dp - dq;
    if (std::abs (den) < 1e-9f) return false;
    t = dp / den;
    if (t < 1e-5f || t > 1.0f - 1e-5f) return false;
    X = P + (Q - P) * t;
    return true;
}

inline bool insidePolygon (const RoomGeom& g, float x, float y)
{
    bool in = false;
    for (int i = 0, j = g.np - 1; i < g.np; j = i++)
        if (((g.py[i] > y) != (g.py[j] > y))
            && (x < (g.px[j] - g.px[i]) * (y - g.py[i]) / (g.py[j] - g.py[i]) + g.px[i]))
            in = ! in;
    return in;
}

// is X actually ON this surface, not merely on its infinite plane?
inline bool onSurface (const RoomGeom& g, const Surface& s, const Vec3& X)
{
    if (! s.vertical) return X.z > -0.01f && X.z < s.h + 0.01f && insidePolygon (g, X.x, X.y);
    if (X.z < -0.01f || X.z > s.h + 0.01f) return false;
    const float dx = s.b.x - s.a.x, dy = s.b.y - s.a.y;
    const float len2 = dx * dx + dy * dy;
    if (len2 < 1e-9f) return false;
    const float u = ((X.x - s.a.x) * dx + (X.y - s.a.y) * dy) / len2;
    return u > -0.002f && u < 1.002f;
}

/*  Does the open segment P->Q pass through any wall on its way? In a convex room
    it cannot, but a wall pushed INTO the room makes it possible, and then the
    path is not real. Reflection points sit exactly on a surface, so a hit within
    a whisker of either end is the path touching its own wall and is allowed. */
inline bool blocked (const RoomGeom& g, const Vec3& P, const Vec3& Q, int skipA, int skipB)
{
    for (int i = 0; i < g.n; ++i)
    {
        if (i == skipA || i == skipB) continue;
        const Surface& s = g.s[i];
        Vec3 X; float t;
        if (! planeHit (P, Q, s, X, t)) continue;
        if (t < 0.002f || t > 0.998f) continue;
        if (onSurface (g, s, X)) return true;
    }
    return false;
}

//------------------------------------------------------------------------------
/*  Validate one candidate: the source S, the listener L, and a sequence of
    surface indices in the order the sound meets them. Fills the reflection
    points in that same order and the total path length.

    The image chain runs forward from the source; the validation runs backwards
    from the listener, which is the only order in which each reflection point can
    be found before the one before it is known. */
inline bool tracePath (const RoomGeom& g, const Vec3& S, const Vec3& L,
                       const int* seq, int nseq, Vec3* hits, float& length)
{
    if (nseq > 3) return false;
    Vec3 img[4];
    img[0] = S;
    for (int k = 0; k < nseq; ++k) img[k + 1] = mirrorIn (img[k], g.s[seq[k]]);

    Vec3 P = L;
    for (int k = nseq - 1; k >= 0; --k)
    {
        Vec3 X; float t;
        if (! planeHit (P, img[k + 1], g.s[seq[k]], X, t)) return false;
        if (! onSurface (g, g.s[seq[k]], X)) return false;
        hits[k] = X;
        P = X;
    }
    // nothing in the way, along any leg
    Vec3 prev = S;
    for (int k = 0; k < nseq; ++k)
    {
        if (blocked (g, prev, hits[k], k > 0 ? seq[k - 1] : -1, seq[k])) return false;
        prev = hits[k];
    }
    if (blocked (g, prev, L, nseq > 0 ? seq[nseq - 1] : -1, -1)) return false;

    length = 0;
    prev = S;
    for (int k = 0; k < nseq; ++k) { length += (hits[k] - prev).len(); prev = hits[k]; }
    length += (L - prev).len();
    return true;
}

} // namespace tw
