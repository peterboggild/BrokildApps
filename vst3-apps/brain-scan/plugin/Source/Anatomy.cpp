/*  BRAIN SCAN — procedural anatomy in Hounsfield units. See Anatomy.h.

    Everything is a signed test against simple solids — ellipsoids, capsules,
    plates, arches — layered in the order a radiologist would list them, with
    value noise for trabecular texture and a few HU of quantum noise over the
    lot so a narrow window looks like a scan and not like a drawing.
*/
#include "Anatomy.h"

namespace bs { namespace an {

//==============================================================================
//  helpers
static inline float sstep (float e0, float e1, float x)
{
    const float t = clamp01 ((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}
static inline uint32_t hash3 (int x, int y, int z)
{
    uint32_t h = (uint32_t) x * 374761393u + (uint32_t) y * 668265263u + (uint32_t) z * 2147483647u + 0x9E3779B9u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
static inline float vnoise (float x, float y, float z)
{
    const int ix = (int) std::floor (x), iy = (int) std::floor (y), iz = (int) std::floor (z);
    const float fx = x - (float) ix, fy = y - (float) iy, fz = z - (float) iz;
    const float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy), sz = fz * fz * (3 - 2 * fz);
    auto v = [] (int a, int b, int c) { return (float) (hash3 (a, b, c) & 0xffff) / 65535.0f; };
    const float c00 = v (ix, iy, iz) + (v (ix + 1, iy, iz) - v (ix, iy, iz)) * sx;
    const float c10 = v (ix, iy + 1, iz) + (v (ix + 1, iy + 1, iz) - v (ix, iy + 1, iz)) * sx;
    const float c01 = v (ix, iy, iz + 1) + (v (ix + 1, iy, iz + 1) - v (ix, iy, iz + 1)) * sx;
    const float c11 = v (ix, iy + 1, iz + 1) + (v (ix + 1, iy + 1, iz + 1) - v (ix, iy + 1, iz + 1)) * sx;
    const float c0 = c00 + (c10 - c00) * sy, c1 = c01 + (c11 - c01) * sy;
    return c0 + (c1 - c0) * sz;
}
static inline float fbm (float x, float y, float z, int oct)
{
    float a = 0.5f, s = 0, norm = 0;
    for (int o = 0; o < oct; ++o) { s += a * vnoise (x, y, z); norm += a; x *= 2.03f; y *= 2.01f; z *= 1.99f; a *= 0.5f; }
    return s / norm;
}
//  quantum noise: a few HU, per texel, the same texel every build
static inline float grain (float x, float y, float z, float amp)
{
    const uint32_t h = hash3 ((int) (x * 1000.0f), (int) (y * 1000.0f), (int) (z * 1000.0f));
    return ((float) (h & 0xffff) / 65535.0f - 0.5f) * 2.0f * amp;
}
//  trabecular bone: a mottle of a few hundred HU
static inline float trab (float X, float Y, float Z, float scale = 1.0f)
{
    return TRAB + (fbm (X * 1.6f * scale, Y * 1.6f * scale, Z * 1.6f * scale, 3) - 0.5f) * 260.0f;
}
static inline float ell (float px, float py, float pz, float a, float b, float c)
{
    return px * px / (a * a) + py * py / (b * b) + pz * pz / (c * c);
}
//  distance from a point to a segment ab
static inline float capsule (float px, float py, float pz, float ax, float ay, float az, float bx, float by, float bz)
{
    const float dx = bx - ax, dy = by - ay, dz = bz - az;
    const float l2 = dx * dx + dy * dy + dz * dz;
    float t = l2 > 1e-9f ? ((px - ax) * dx + (py - ay) * dy + (pz - az) * dz) / l2 : 0.0f;
    t = clamp01 (t);
    const float qx = ax + dx * t - px, qy = ay + dy * t - py, qz = az + dz * t - pz;
    return std::sqrt (qx * qx + qy * qy + qz * qz);
}
//  distance in the plane from a point to a segment ab
static inline float seg2 (float px, float py, float ax, float ay, float bx, float by)
{
    const float dx = bx - ax, dy = by - ay;
    const float l2 = dx * dx + dy * dy;
    float t = l2 > 1e-9f ? ((px - ax) * dx + (py - ay) * dy) / l2 : 0.0f;
    t = clamp01 (t);
    const float qx = ax + dx * t - px, qy = ay + dy * t - py;
    return std::sqrt (qx * qx + qy * qy);
}

//------------------------------------------------------------------------------
/*  A tooth on an elliptical arch. The arch is centred (cx, cy) with radii
    (rx, ry); teeth sit at fourteen angles from -1.3 to 1.3 (0 = the midline,
    anterior). Returns HU, or -9999 for "no tooth here". Enamel is the outer
    1.5 mm of the crown, dentine the rest, a pulp chamber in the middle. */
static float tooth (float X, float Y, float Z, float cx, float cy, float rx, float ry,
                    float crown0, float crown1, float root0)
{
    if (Z < root0 || Z > crown1) return -9999.0f;
    const float th = std::atan2 (X / rx, (Y - cy) / ry);
    if (std::abs (th) > 1.42f) return -9999.0f;
    const float step = 2.6f / 13.0f;
    int k = (int) std::lround ((th + 1.3f) / step);
    if (k < 0) k = 0; if (k > 13) k = 13;
    const float tk = -1.3f + step * (float) k;
    const float px = rx * std::sin (tk), py = cy + ry * std::cos (tk);
    //  tangent and normal of the arch at the tooth
    float tx = rx * std::cos (tk), ty = -ry * std::sin (tk);
    const float tl = std::sqrt (tx * tx + ty * ty); tx /= tl; ty /= tl;
    const float nx = -ty, ny = tx;
    const float dt = (X - px) * tx + (Y - py) * ty, dr = (X - px) * nx + (Y - py) * ny;
    const bool molar = std::abs (tk) > 0.85f;
    const float hw = molar ? 0.50f : 0.40f, hd = molar ? 0.48f : 0.40f;
    //  a rounded box: superellipse of the two half-widths
    const float q = std::pow (std::abs (dt) / hw, 3.0f) + std::pow (std::abs (dr) / hd, 3.0f);
    if (q > 1.0f) return -9999.0f;
    const bool crown = Z >= crown0;
    if (crown)
    {
        const float top = crown1 - 0.12f;
        if (q > 0.52f || Z > top) return ENAMEL;                          // the cap, all round and on top
        if (q < 0.06f && Z < crown1 - 0.45f) return SOFT;                 // the pulp chamber
        return DENTINE;
    }
    //  root: tapering — the box shrinks toward root0
    const float taper = 0.55f + 0.45f * clamp01 ((Z - root0) / (crown0 - root0));
    if (q > taper) return -9999.0f;
    if (q < 0.05f) return SOFT;                                           // the root canal
    return DENTINE;
}

//==============================================================================
//  THE HEAD — 24 cm cube
float head (float x, float y, float z, bool soft)
{
    const float X = (x - 0.5f) * 24.0f, Y = (y - 0.5f) * 24.0f, Z = (z - 0.5f) * 24.0f;
    float hu = AIR;
    bool bone = false, tooth_ = false;

    //  ---- the couch: a headrest behind the occiput, curved across x --------
    {
        const float yc = -10.95f - 0.012f * X * X;
        if (Y < yc + 0.22f && Y > yc - 0.22f && std::abs (X) < 11.5f) hu = COUCH;
    }

    //  ---- the vault: radial shells about one ellipsoid ---------------------
    const float rv = std::sqrt (ell (X, Y + 0.5f, Z - 1.0f, 7.3f, 9.3f, 7.2f));
    const bool aboveBase = Z > -3.2f;
    if (aboveBase && rv < 1.10f)
    {
        if (rv >= 1.07f)      hu = SOFT;                                     // skin
        else if (rv >= 1.02f) hu = FAT + fbm (X, Y, Z, 2) * 12.0f;           // subcutaneous fat
        else if (rv >= 1.00f) hu = SOFT;                                     // galea
        else if (rv >= 0.90f)
        {
            //  the skull: outer table, diploë, inner table
            bone = true;
            if (rv >= 0.97f)      hu = CORT;
            else if (rv >= 0.925f) hu = trab (X, Y, Z, 1.4f) + 60.0f;
            else                  hu = CORT - 120.0f;
            //  sutures: hairline gaps, zig-zagged by noise
            const float jit = (fbm (X * 1.7f, Y * 1.7f, Z * 1.7f, 2) - 0.5f) * 0.5f;
            const bool sagittal = Z > 1.5f && std::abs (X + jit) < 0.09f && Y > -7.0f && Y < 3.0f;
            const bool coronal  = Z > 0.0f && std::abs (Y - 3.0f + jit + 0.25f * (Z - 1.0f)) < 0.09f && rv > 0.9f;
            const bool lambdoid = Z > -1.5f && std::abs (Y + 6.2f + jit - 0.35f * std::abs (X) * 0.5f) < 0.09f && rv > 0.9f;
            if (sagittal || coronal || lambdoid) { hu = 180.0f; bone = false; }
            //  the frontal sinus, in the bone above the nose
            if (ell (std::abs (X) - 1.0f, Y - 6.9f, Z - 3.1f, 1.25f, 0.85f, 1.05f) < 1.0f) { hu = AIR; bone = false; }
            //  mastoid air cells, behind the ears
            if (std::abs (X) > 5.0f && Y > -3.8f && Y < -0.6f && Z > -3.2f && Z < -0.8f
                && fbm (X * 2.6f, Y * 2.6f, Z * 2.6f, 2) > 0.56f) { hu = AIR; bone = false; }
        }
        else if (rv >= 0.86f) hu = CSF;                                      // subarachnoid space
        else
        {
            //  ---- the brain -------------------------------------------------
            const float fold = fbm (X * 0.55f + 11.0f, Y * 0.55f + 5.0f, Z * 0.55f + 2.0f, 3);
            const float fold2 = fbm (X * 1.9f + 1.0f, Y * 1.9f + 9.0f, Z * 1.9f + 4.0f, 2);
            const float s = std::sin (fold * 15.0f + fold2 * 4.5f);
            hu = WHITE;
            if (rv > 0.80f) hu = GREY;                                       // the cortical ribbon
            if (rv > 0.60f && s > 0.30f) hu = GREY;                          // gyral grey around the sulci
            if (rv > 0.62f && s > 0.68f) hu = CSF;                           // the sulci themselves
            //  the interhemispheric fissure and the falx
            if (Z > -1.0f && rv > 0.30f)
            {
                if (std::abs (X) < 0.26f) hu = CSF;
                if (std::abs (X) < 0.06f) hu = 46.0f;
            }
            //  deep grey: basal ganglia, thalami
            if (ell (std::abs (X) - 2.3f, Y - 0.8f, Z + 0.4f, 1.3f, 1.9f, 1.5f) < 1.0f) hu = 41.0f;
            if (ell (std::abs (X) - 1.1f, Y + 1.0f, Z + 0.6f, 1.0f, 1.4f, 1.0f) < 1.0f) hu = 40.0f;
            //  ventricles
            if (ell (std::abs (X) - 1.5f, Y - 0.3f, Z - 0.6f, 0.75f, 3.4f, 1.0f) < 1.0f) hu = CSF;
            if (ell (X, Y - 0.3f, Z + 0.9f, 0.3f, 1.3f, 0.9f) < 1.0f) hu = CSF;
            //  the cerebellum, folded finer, under the tentorium at the back
            if (Z < -1.8f && Y < -1.0f)
            {
                const float C = ell (X, Y + 5.0f, Z + 3.4f, 4.8f, 3.0f, 2.3f);
                if (C < 1.0f)
                {
                    const float f = fbm (X * 1.4f, Y * 1.4f + 3.0f, Z * 1.4f, 2);
                    const float sc = std::sin (f * 24.0f);
                    hu = sc > 0.25f ? GREY : WHITE;
                    if (sc > 0.86f) hu = CSF;
                    if (C > 0.86f) hu = GREY;
                }
                else if (C < 1.25f) hu = CSF;                               // the cisterns
            }
            //  the brainstem, down through the foramen
            if (capsule (X, Y, Z, 0.0f, -1.5f, -2.0f, 0.0f, -3.0f, -9.0f) < 1.25f) hu = 35.0f;
        }
    }

    //  ---- the skull base and the foramen magnum ---------------------------
    if (! aboveBase && Z > -4.7f && rv < 1.0f)
    {
        const float fm = X * X + (Y + 3.0f) * (Y + 3.0f);
        if (fm < 1.7f * 1.7f) { hu = fm < 1.0f ? 35.0f : CSF; }              // cord in the foramen
        else
        {
            bone = true;
            hu = Z > -3.6f ? CORT - 150.0f : trab (X, Y, Z) + 120.0f;
            //  the sphenoid sinus, in the base behind the nose
            if (ell (X, Y - 1.2f, Z + 2.9f, 1.2f, 1.2f, 0.9f) < 1.0f) { hu = AIR; bone = false; }
            //  petrous bones: dense wedges laterally
            if (std::abs (X) > 3.5f && Y > -3.5f && Y < 0.5f) hu = CORT + 100.0f;
        }
    }

    //  ---- the face --------------------------------------------------------
    {
        //  soft tissue of the face: a rounded block in front, under the vault
        const float F = std::pow (std::abs (X) / 6.4f, 3.0f) + std::pow (std::abs (Y - 4.0f) / 4.6f, 3.0f) + std::pow (std::abs (Z + 4.6f) / 5.0f, 3.0f);
        if (F < 1.0f && hu == AIR) hu = F > 0.72f ? FAT + 10.0f : SOFT;
        //  the nose, joined to the face
        if (ell (X, Y - 8.3f, Z + 3.5f, 1.2f, 1.5f, 1.9f) < 1.0f) hu = SOFT;
        //  the maxilla: the upper alveolar arch, the hard palate across it, and the cheekbones
        {
            const float ux = 4.5f, uy = 6.1f, ucy = 1.7f;
            const float ur = std::sqrt (X * X / (ux * ux) + (Y - ucy) * (Y - ucy) / (uy * uy));
            const float uth = std::atan2 (X / ux, (Y - ucy) / uy);
            if (Z > -5.9f && Z < -2.4f && std::abs (uth) < 1.6f && ur > 0.70f && ur < 1.0f)
            { bone = true; hu = (ur > 0.92f || ur < 0.78f) ? CORT - 150.0f : trab (X, Y, Z) - 40.0f; }
            if (Z > -5.95f && Z < -5.5f && ur < 0.75f && std::abs (uth) < 1.6f) { bone = true; hu = 700.0f; }
            for (int s = -1; s <= 1; s += 2)
                if (ell (X - 5.3f * (float) s, Y - 4.4f, Z + 2.3f, 1.35f, 1.7f, 1.6f) < 1.0f) { bone = true; hu = CORT - 200.0f; }
        }
        //  the zygomatic arches
        if (capsule (X, Y, Z, 6.3f * (X < 0 ? -1.0f : 1.0f), 4.6f, -1.9f, 6.6f * (X < 0 ? -1.0f : 1.0f), -0.6f, -2.1f) < 0.45f) { bone = true; hu = CORT - 60.0f; }
        //  the nasal cavity, with its septum, and the maxillary sinuses
        if (std::abs (X) < 1.15f && Y > 2.6f && Y < 9.4f && Z > -5.4f && Z < -0.9f)
        {
            hu = AIR; bone = false;
            if (std::abs (X) < 0.11f) { hu = 420.0f; bone = true; }
            if (std::abs (X) > 0.55f && std::sin (Z * 2.4f + Y * 0.6f) > 0.55f) hu = SOFT;   // turbinates
        }
        if (ell (std::abs (X) - 2.7f, Y - 4.4f, Z + 4.6f, 1.55f, 1.8f, 1.6f) < 1.0f) { hu = AIR; bone = false; }
        //  the orbits: a bony rim, fat inside, and a globe in each
        //  the orbits open on the face: cones that reach past the frontal bone
        const float O = ell (std::abs (X) - 3.15f, Y - 7.0f, Z + 0.9f, 1.95f, 2.8f, 1.9f);
        if (O >= 1.0f && O < 1.30f && rv < 1.03f && Z > -3.0f && Y > 4.0f) { bone = true; hu = CORT - 250.0f; }
        if (O < 1.0f) { hu = Y > 9.2f ? AIR : FAT + 15.0f; bone = false; }
        if (ell (std::abs (X) - 3.15f, Y - 7.8f, Z + 0.9f, 1.15f, 1.15f, 1.15f) < 1.0f) hu = 32.0f;
        if (O < 1.0f && ell (std::abs (X) - 3.15f, Y - 7.8f, Z + 0.9f, 1.2f, 1.2f, 1.2f) < 1.0f && Y > 8.7f) hu = 60.0f;   // lens
        if (capsule (X, Y, Z, 3.15f * (X < 0 ? -1.0f : 1.0f), 5.5f, -0.9f, 0.9f * (X < 0 ? -1.0f : 1.0f), 1.5f, -1.6f) < 0.28f && O < 1.05f) hu = 36.0f;   // optic nerve
        //  ear canals
        if (std::abs (X) > 5.0f && (Y + 1.0f) * (Y + 1.0f) + (Z + 2.8f) * (Z + 2.8f) < 0.36f * 0.36f) { hu = AIR; bone = false; }
    }

    //  ---- the mandible and the teeth --------------------------------------
    {
        const float acx = 0.0f, acy = 1.5f, arx = 4.9f, ary = 6.5f;
        const float rr = std::sqrt (X * X / (arx * arx) + (Y - acy) * (Y - acy) / (ary * ary));
        const float th = std::atan2 (X / arx, (Y - acy) / ary);
        //  the body: an elliptical tube along the arch, crest at -7.2
        if (std::abs (th) < 1.58f)
        {
            const float dh = (rr - 1.0f) * 5.6f, dv = Z + 8.4f;
            const float q = (dh / 0.78f) * (dh / 0.78f) + (dv / 1.25f) * (dv / 1.25f);
            if (q < 1.0f)
            {
                bone = true;
                hu = q > 0.62f ? CORT : trab (X, Y, Z, 1.3f);
                if ((dh / 0.3f) * (dh / 0.3f) + ((Z + 8.9f) / 0.24f) * ((Z + 8.9f) / 0.24f) < 1.0f) { hu = 30.0f; bone = false; }   // the canal
            }
        }
        //  the rami, up to the condyles under the ears
        for (int s = -1; s <= 1; s += 2)
        {
            const float sx = (float) s;
            if (std::abs (X - 5.0f * sx) < 0.36f && Y > -0.4f && Y < 2.6f && Z > -9.4f && Z < -3.2f) { bone = true; hu = CORT - 80.0f; }
            if (ell (X - 5.0f * sx, Y - 0.3f, Z + 3.0f, 0.75f, 0.75f, 0.7f) < 1.0f) { bone = true; hu = CORT - 200.0f; }
            if (ell (X - 4.9f * sx, Y - 2.4f, Z + 3.6f, 0.5f, 0.55f, 0.9f) < 1.0f) { bone = true; hu = CORT - 200.0f; }   // coronoid
        }
        //  lower teeth on the inner arch, upper teeth hanging from the maxilla
        float t = tooth (X, Y, Z, acx, acy, arx * 0.86f, ary * 0.86f, -7.2f, -6.1f, -8.7f);
        if (t < -9000.0f) t = tooth (X, Y, Z, acx, acy + 0.2f, arx * 0.90f, ary * 0.90f, -6.1f, -5.0f, -3.5f);
        if (t < -9000.0f) t = -9999.0f;
        if (t > -9000.0f) { hu = t; tooth_ = true; bone = false; }
        if (! soft && ! bone && ! tooth_ && hu != COUCH) { /* handled below */ }
    }

    //  ---- the neck and the cervical spine ---------------------------------
    if (Z < -5.4f && ! bone && ! tooth_ && hu != COUCH)
    {
        const float rn = std::sqrt (X * X / (6.6f * 6.6f) + (Y + 1.0f) * (Y + 1.0f) / (6.4f * 6.4f));
        const bool inHead = aboveBase || (rv < 1.1f && Z > -6.5f);
        if (rn < 1.0f && ! inHead)
        {
            hu = rn > 0.86f ? FAT + 8.0f : MUSCLE;
            if ((X * X + (Y - 2.9f) * (Y - 2.9f)) < 0.95f * 0.95f && Z < -7.0f) hu = AIR;               // the airway
            if (ell (std::abs (X) - 2.7f, Y - 0.6f, 0.0f, 0.5f, 0.5f, 1.0f) < 1.0f) hu = BLOOD + 4.0f;    // carotids
        }
    }
    {
        //  C1: a ring; C2: a body with the dens rising through the ring
        const float rc = std::sqrt (X * X + (Y + 3.0f) * (Y + 3.0f));
        if (Z > -6.6f && Z < -5.6f && std::abs (rc - 2.3f) < 0.5f) { bone = true; hu = CORT - 150.0f; }
        if (Z > -9.8f && Z < -6.6f && ell (X, Y + 3.0f, 0.0f, 1.35f, 1.2f, 1.0f) < 1.0f)
        {
            bone = true;
            hu = ell (X, Y + 3.0f, 0.0f, 1.35f, 1.2f, 1.0f) > 0.7f ? CORT - 100.0f : trab (X, Y, Z);
        }
        if (Z > -6.6f && Z < -5.0f && (X * X + (Y + 2.2f) * (Y + 2.2f)) < 0.6f * 0.6f) { bone = true; hu = CORT - 150.0f; }
        //  the cord and its CSF, from the foramen down
        if (Z < -4.7f && rc < 1.15f && rc > 0.7f && Z > -12.0f && ! bone) hu = CSF;
        if (Z < -4.7f && rc <= 0.7f && Z > -12.0f) { hu = 35.0f; bone = false; }
    }

    //  ---- bone only? ------------------------------------------------------
    if (! soft && ! bone && ! tooth_ && hu != COUCH) hu = AIR;

    return hu + grain (x, y, z, 7.0f);
}

//==============================================================================
//  THE THORAX — 36 cm cube
float thorax (float x, float y, float z)
{
    const float X = (x - 0.5f) * 36.0f, Y = (y - 0.5f) * 36.0f, Z = (z - 0.5f) * 36.0f;
    float hu = AIR;
    bool bone = false;

    //  the couch behind the back
    {
        const float yc = -11.4f - 0.006f * X * X;
        if (Y < yc + 0.22f && Y > yc - 0.22f && std::abs (X) < 17.0f) hu = COUCH;
    }
    //  the chest wall
    const float rc = std::sqrt (X * X / (15.5f * 15.5f) + Y * Y / (10.5f * 10.5f));
    if (rc < 1.0f)
    {
        if (rc >= 0.97f)      hu = SOFT;
        else if (rc >= 0.90f) hu = FAT + fbm (X * 0.7f, Y * 0.7f, Z * 0.7f, 2) * 14.0f;
        else if (rc >= 0.80f) hu = MUSCLE + (fbm (X, Y, Z, 2) - 0.5f) * 8.0f;
        else                  hu = FAT + 20.0f;                                   // mediastinal fat, by default
    }
    //  the lungs, with a mottle of small vessels
    const float EL = ell (std::abs (X) - 7.3f, Y + 0.5f, Z - 0.5f, 6.4f, 8.9f, 13.6f);
    if (EL < 1.0f && rc < 0.82f)
    {
        hu = LUNG + (fbm (X * 1.1f, Y * 1.1f, Z * 1.1f, 3) - 0.5f) * 90.0f;
        if (fbm (X * 2.4f + 7.0f, Y * 2.4f, Z * 2.4f + 3.0f, 3) > 0.66f) hu = -420.0f;   // peripheral vessels, partial-volumed
    }
    //  the airways: trachea, main and lobar bronchi — air lumen, a wall
    {
        struct T { float ax, ay, az, bx, by, bz, r; };
        static const T tubes[] = {
            { 0.0f, -1.5f, 18.5f, 0.0f, -2.0f, 3.5f, 0.95f },
            { 0.0f, -2.0f, 3.5f, 3.8f, -1.0f, 0.5f, 0.72f }, { 0.0f, -2.0f, 3.5f, -3.8f, -1.0f, 0.5f, 0.72f },
            { 3.8f, -1.0f, 0.5f, 7.5f, -2.5f, 4.5f, 0.48f }, { -3.8f, -1.0f, 0.5f, -7.5f, -2.5f, 4.5f, 0.48f },
            { 3.8f, -1.0f, 0.5f, 8.0f, 0.5f, -4.0f, 0.5f }, { -3.8f, -1.0f, 0.5f, -8.0f, 0.5f, -4.0f, 0.5f },
            { 3.8f, -1.0f, 0.5f, 9.5f, -3.0f, -1.5f, 0.42f }, { -3.8f, -1.0f, 0.5f, -9.5f, -3.0f, -1.5f, 0.42f },
        };
        for (const T& t : tubes)
        {
            const float d = capsule (X, Y, Z, t.ax, t.ay, t.az, t.bx, t.by, t.bz);
            if (d < t.r + 0.22f) hu = 160.0f;
            if (d < t.r) hu = AIR;
        }
    }
    //  the pulmonary vessels: from the hila, branching outward
    {
        struct T { float ax, ay, az, bx, by, bz, r; };
        static const T v[] = {
            { 2.5f, 1.0f, 1.5f, 7.0f, -1.5f, 4.0f, 0.9f }, { -2.5f, 1.0f, 1.5f, -7.0f, -1.5f, 4.0f, 0.9f },
            { 2.5f, 1.0f, 1.5f, 8.5f, 1.5f, -3.5f, 0.7f }, { -2.5f, 1.0f, 1.5f, -8.5f, 1.5f, -3.5f, 0.7f },
            { 7.0f, -1.5f, 4.0f, 11.0f, 2.0f, 6.5f, 0.42f }, { -7.0f, -1.5f, 4.0f, -11.0f, 2.0f, 6.5f, 0.42f },
            { 7.0f, -1.5f, 4.0f, 10.5f, -4.5f, 0.5f, 0.45f }, { -7.0f, -1.5f, 4.0f, -10.5f, -4.5f, 0.5f, 0.45f },
            { 8.5f, 1.5f, -3.5f, 10.5f, -1.0f, -8.5f, 0.45f }, { -8.5f, 1.5f, -3.5f, -10.5f, -1.0f, -8.5f, 0.45f },
            { 8.5f, 1.5f, -3.5f, 11.5f, 4.0f, -6.0f, 0.36f }, { -8.5f, 1.5f, -3.5f, -11.5f, 4.0f, -6.0f, 0.36f },
        };
        for (const T& t : v)
            if (capsule (X, Y, Z, t.ax, t.ay, t.az, t.bx, t.by, t.bz) < t.r) hu = BLOOD;
    }
    //  the heart, in its fat, and the aorta
    {
        const float H = ell (X - 2.2f, Y - 2.5f, Z + 4.5f, 5.7f, 4.7f, 5.8f);
        if (H < 1.18f && H >= 1.0f) hu = FAT + 5.0f;
        if (H < 1.0f)
        {
            hu = 42.0f;
            //  chambers: a little brighter blood in the ventricles and atria
            if (ell (X - 3.2f, Y - 3.2f, Z + 5.5f, 2.4f, 2.0f, 2.6f) < 1.0f) hu = BLOOD;
            if (ell (X - 0.2f, Y - 1.2f, Z + 3.0f, 1.9f, 1.7f, 2.2f) < 1.0f) hu = BLOOD;
        }
        if (capsule (X, Y, Z, 1.0f, 3.0f, 1.0f, 0.5f, 2.0f, 7.5f) < 1.5f) hu = BLOOD + 2.0f;
        if (capsule (X, Y, Z, 0.5f, 2.0f, 7.5f, 2.5f, -4.5f, 7.0f) < 1.4f) hu = BLOOD + 2.0f;
        if (capsule (X, Y, Z, 2.5f, -4.8f, 7.0f, 2.3f, -5.2f, -18.0f) < 1.3f) hu = BLOOD + 2.0f;
        //  the oesophagus, collapsed, behind the trachea
        if (capsule (X, Y, Z, 0.0f, -4.2f, 18.0f, 0.3f, -4.6f, -8.0f) < 0.7f) hu = SOFT;
    }
    //  the liver under the right dome, the stomach under the left
    {
        const float Lv = ell (X + 5.5f, Y - 1.0f, Z + 13.5f, 9.8f, 8.6f, 7.2f);
        if (Lv < 1.0f && rc < 0.82f) hu = LIVER + (fbm (X, Y, Z, 2) - 0.5f) * 10.0f;
        const float St = ell (X - 5.8f, Y - 3.5f, Z + 12.5f, 3.6f, 3.1f, 3.2f);
        if (St < 1.0f && rc < 0.82f) hu = Z > -12.5f ? AIR : 30.0f;
    }
    //  ---- the bones -------------------------------------------------------
    //  ribs: eight each side, sloping down toward the front
    {
        const float phi = std::atan2 (std::abs (X) / 15.5f, -Y / 10.5f);          // 0 at the spine, pi at the sternum
        const float dr = (rc - 0.845f) * 12.5f;
        for (int k = 0; k < 8; ++k)
        {
            const float zc = 11.5f - 2.75f * (float) k - 2.6f * (phi / 3.14159f);
            const float dz = Z - zc;
            const float q = (dr / 0.7f) * (dr / 0.7f) + (dz / 0.55f) * (dz / 0.55f);
            if (q < 1.0f && phi > 0.12f && phi < 2.95f)
            {
                if (phi < 2.55f) { bone = true; hu = q > 0.55f ? CORT - 300.0f : trab (X, Y, Z, 1.5f); }
                else hu = CARTILAGE;
            }
        }
        //  sternum
        if (std::abs (X) < 1.9f && Y > 9.0f && Y < 9.9f && Z > -3.0f && Z < 9.0f) { bone = true; hu = 560.0f + (fbm (X, Y, Z, 2) - 0.5f) * 120.0f; }
        //  clavicles
        for (int s = -1; s <= 1; s += 2)
            if (capsule (X, Y, Z, 1.6f * (float) s, 8.6f, 12.5f, 13.0f * (float) s, 4.5f, 11.2f) < 0.62f) { bone = true; hu = CORT - 350.0f; }
        //  scapulae: thin plates behind, tilted
        {
            const float ax = std::abs (X);
            const float yp = -9.0f + 0.10f * (ax - 6.5f);
            if (ax > 6.5f && ax < 12.5f && std::abs (Y - yp) < 0.32f && Z > -1.0f && Z < 10.0f && rc < 0.95f) { bone = true; hu = 620.0f; }
        }
        //  the spine: bodies and discs on a 2.8 cm period, canal, processes
        {
            const float rb = std::sqrt (X * X / (1.75f * 1.75f) + (Y + 7.5f) * (Y + 7.5f) / (1.5f * 1.5f));
            const float zm = Z - 2.8f * std::floor (Z / 2.8f);
            const bool inBody = zm > 0.3f && zm < 2.5f;
            if (rb < 1.0f)
            {
                if (inBody)
                {
                    bone = true;
                    hu = (rb > 0.88f || zm < 0.45f || zm > 2.35f) ? CORT - 200.0f
                                                                : TRAB - 40.0f + (fbm (X * 2.0f, Y * 2.0f, Z * 0.6f, 3) - 0.5f) * 240.0f;
                }
                else if (rb < 0.95f) hu = DISC;
            }
            const float rcn = std::sqrt (X * X + (Y + 9.6f) * (Y + 9.6f));
            if (rcn < 0.8f) hu = rcn < 0.45f ? 35.0f : CSF;
            if (inBody)
            {
                if (std::abs (X) < 0.35f && Y < -10.3f && Y > -12.3f && zm > 0.5f && zm < 2.2f) { bone = true; hu = 700.0f; }
                if (seg2 (X, Y, 1.3f, -9.0f, 0.0f, -10.4f) < 0.3f || seg2 (X, Y, -1.3f, -9.0f, 0.0f, -10.4f) < 0.3f) { bone = true; hu = 700.0f; }
                if (std::abs (Y + 9.0f) < 0.35f && std::abs (X) < 3.2f && std::abs (zm - 1.5f) < 0.4f) { bone = true; hu = 600.0f; }
            }
        }
    }
    (void) bone;
    return hu + grain (x, y, z, 8.0f);
}

//==============================================================================
//  THE LUMBAR SPINE — 12 cm cube, three levels
float vertebra (float x, float y, float z)
{
    const float X = (x - 0.5f) * 12.0f, Y = (y - 0.5f) * 12.0f, Z = (z - 0.5f) * 12.0f;
    //  the cube is inside the body: fat by default
    float hu = FAT + (fbm (X * 0.8f, Y * 0.8f, Z * 0.8f, 2) - 0.5f) * 14.0f;
    //  muscles
    if (ell (std::abs (X) - 3.9f, Y - 1.6f, 0.0f, 1.5f, 1.3f, 1.0f) < 1.0f) hu = MUSCLE - 2.0f + (fbm (X, Y, Z, 2) - 0.5f) * 8.0f;   // psoas
    if (ell (std::abs (X) - 2.6f, Y + 4.0f, 0.0f, 2.3f, 1.9f, 1.0f) < 1.0f) hu = MUSCLE + (fbm (X, Y, Z, 2) - 0.5f) * 8.0f;          // erector spinae
    //  vessels in front of the spine
    if ((X - 0.9f) * (X - 0.9f) + (Y - 3.6f) * (Y - 3.6f) < 1.0f) hu = BLOOD;                            // aorta
    if (ell (X + 1.3f, Y - 3.4f, 0.0f, 1.0f, 0.7f, 1.0f) < 1.0f) hu = SOFT;                                 // vena cava
    //  bowel loops at the front corners: air over fluid
    for (int s = -1; s <= 1; s += 2)
        if (ell (X - 4.0f * (float) s, Y - 4.4f, Z - 3.0f * std::sin (0.7f * (float) s), 1.3f, 1.2f, 2.6f) < 1.0f) hu = Y > 4.4f ? AIR : 25.0f;
    //  ---- the vertebrae ---------------------------------------------------
    const float zm = (Z + 5.7f) - 3.8f * std::floor ((Z + 5.7f) / 3.8f);
    const bool inBody = zm > 0.55f && zm < 3.25f;
    const bool inPost = zm > 0.9f && zm < 2.9f;
    const float ry = Y < 1.2f ? 1.5f : 1.7f;
    const float rb = std::sqrt (X * X / (2.3f * 2.3f) + (Y - 1.2f) * (Y - 1.2f) / (ry * ry));
    if (rb < 1.0f)
    {
        if (inBody)
        {
            //  cortex, endplates, and trabeculae striated along z
            if (rb > 0.9f || zm < 0.72f || zm > 3.08f) hu = CORT - 250.0f;
            else hu = TRAB - 60.0f + (fbm (X * 2.2f, Y * 2.2f, Z * 0.7f, 3) - 0.5f) * 260.0f;
        }
        else if (rb < 0.95f) hu = rb < 0.55f ? DISC - 10.0f : DISC + 12.0f;     // nucleus, annulus
    }
    //  the canal and its contents
    {
        const float rcn = std::sqrt (X * X + (Y + 1.2f) * (Y + 1.2f));
        if (rcn < 0.85f)
        {
            hu = CSF;
            //  the cauda equina: a handful of rootlets
            for (int k = 0; k < 6; ++k)
            {
                const float a = 1.05f * (float) k, rr = 0.42f + 0.18f * std::sin (2.3f * (float) k + Z * 0.4f);
                const float px = rr * std::cos (a), py = -1.2f + rr * std::sin (a);
                if ((X - px) * (X - px) + (Y - py) * (Y - py) < 0.09f * 0.09f) hu = 36.0f;
            }
        }
    }
    if (inPost)
    {
        for (int s = -1; s <= 1; s += 2)
        {
            const float sx = (float) s;
            if (seg2 (X, Y, 1.6f * sx, -0.3f, 1.6f * sx, -1.9f) < 0.45f) hu = CORT - 300.0f;                 // pedicle
            if (seg2 (X, Y, 1.6f * sx, -1.9f, 0.0f, -3.2f) < 0.34f) hu = CORT - 300.0f;                       // lamina
            if (seg2 (X, Y, 1.6f * sx, -1.3f, 4.6f * sx, -1.7f) < 0.38f) hu = CORT - 380.0f;                  // transverse process
            if (ell (X - 1.9f * sx, Y + 2.4f, zm - 1.9f, 0.55f, 0.55f, 1.3f) < 1.0f) hu = CORT - 320.0f;      // facets
        }
        if (std::abs (X) < 0.35f && Y < -3.1f && Y > -5.4f) hu = CORT - 380.0f + (fbm (X, Y, Z, 2) - 0.5f) * 80.0f;   // spinous process
    }
    return hu + grain (x, y, z, 8.0f);
}

//==============================================================================
//  THE THIGH — 16 cm cube, a femoral shaft with its ends
float femur (float x, float y, float z)
{
    const float X = (x - 0.5f) * 16.0f, Y = (y - 0.5f) * 16.0f, Z = (z - 0.5f) * 16.0f;
    float hu = AIR;
    //  the couch behind
    {
        const float yc = -7.45f - 0.008f * X * X;
        if (Y < yc + 0.2f && Y > yc - 0.2f) hu = COUCH;
    }
    const float rs = std::sqrt (X * X / (7.4f * 7.4f) + Y * Y / (6.8f * 6.8f));
    if (rs < 1.0f)
    {
        if (rs >= 0.965f)     hu = SOFT;
        else if (rs >= 0.84f) hu = FAT - 10.0f + (fbm (X * 0.9f, Y * 0.9f, Z * 0.9f, 2) - 0.5f) * 20.0f;
        else
        {
            hu = MUSCLE - 2.0f + (fbm (X * 1.2f, Y * 1.2f, Z * 1.2f, 3) - 0.5f) * 10.0f;
            //  fascial planes between the compartments, as fat lines from the bone out
            const float bx = -1.6f, by = 1.4f;
            const float ang[3] = { 0.7f, 2.6f, 4.55f };
            for (float a : ang)
                if (seg2 (X, Y, bx, by, bx + 9.0f * std::cos (a), by + 9.0f * std::sin (a)) < 0.14f) hu = FAT + 15.0f;
            //  vessels and the nerve
            if ((X - 3.2f) * (X - 3.2f) + (Y - 2.8f) * (Y - 2.8f) < 0.42f * 0.42f) hu = BLOOD + 2.0f;     // femoral artery
            if ((X - 2.5f) * (X - 2.5f) + (Y - 2.0f) * (Y - 2.0f) < 0.52f * 0.52f) hu = SOFT;             // femoral vein
            if ((X - 1.0f) * (X - 1.0f) + (Y + 3.3f) * (Y + 3.3f) < 0.45f * 0.45f) hu = 36.0f;            // sciatic nerve
        }
        if (rs >= 0.84f && rs < 0.965f && (X - 5.4f) * (X - 5.4f) + (Y - 3.4f) * (Y - 3.4f) < 0.3f * 0.3f) hu = SOFT;   // saphenous vein
    }
    //  the femur: a cortical tube that flares and thins toward the ends
    {
        const float flare = 1.0f + 0.9f * sstep (4.0f, 8.0f, std::abs (Z));
        const float rf = std::sqrt (((X + 1.6f) / 1.45f) * ((X + 1.6f) / 1.45f) + ((Y - 1.4f) / 1.35f) * ((Y - 1.4f) / 1.35f)) / flare;
        if (rf < 1.0f)
        {
            const float t = (0.62f - 0.42f * sstep (4.0f, 8.0f, std::abs (Z))) / 1.4f;
            if (rf > 1.0f - t) hu = CORT + 200.0f - 300.0f * sstep (4.0f, 8.0f, std::abs (Z));
            else
            {
                const float end = sstep (4.0f, 7.0f, std::abs (Z));
                const float marrow = -70.0f + (fbm (X * 1.5f, Y * 1.5f, Z * 1.5f, 2) - 0.5f) * 30.0f;
                const float spongy = TRAB - 60.0f + (fbm (X * 2.4f, Y * 2.4f, Z * 1.2f, 3) - 0.5f) * 280.0f;
                hu = marrow + (spongy - marrow) * end;
            }
        }
    }
    return hu + grain (x, y, z, 7.0f);
}

//==============================================================================
//  THE JAW — 12 cm cube: mandible, both rows of teeth, tongue, palate
float jaw (float x, float y, float z)
{
    const float X = (x - 0.5f) * 12.0f, Y = (y - 0.5f) * 12.0f, Z = (z - 0.5f) * 12.0f;
    float hu = AIR;
    bool bone = false, tooth_ = false;
    //  the face: soft tissue out to the skin
    const float rf = std::sqrt (X * X / (6.3f * 6.3f) + (Y + 1.0f) * (Y + 1.0f) / (6.9f * 6.9f));
    if (rf < 1.0f) hu = rf > 0.955f ? SOFT : (rf > 0.84f ? FAT + 8.0f + (fbm (X, Y, Z, 2) - 0.5f) * 16.0f : MUSCLE - 4.0f);
    //  the airway behind the tongue
    if (X * X + (Y + 5.2f) * (Y + 5.2f) < 0.9f * 0.9f && Z < 1.0f) hu = AIR;
    //  the mandible: arch centred (0, -1.6), radii (4.7, 5.6)
    const float acx = 0.0f, acy = -1.6f, arx = 4.7f, ary = 5.6f;
    const float rr = std::sqrt (X * X / (arx * arx) + (Y - acy) * (Y - acy) / (ary * ary));
    const float th = std::atan2 (X / arx, (Y - acy) / ary);
    //  tongue and floor of mouth, inside the arch
    if (rr < 0.82f && Z < 0.9f && Z > -1.6f) hu = 45.0f + (fbm (X * 1.3f, Y * 1.3f, Z * 1.3f, 2) - 0.5f) * 10.0f;
    if (rr < 0.82f && Z <= -1.6f && Z > -3.8f) hu = MUSCLE - 4.0f;
    if (ell (X, Y + 0.4f, Z + 0.1f, 3.2f, 3.6f, 1.5f) < 1.0f) hu = 46.0f + (fbm (X * 1.6f, Y * 1.6f + 2.0f, Z * 1.6f, 2) - 0.5f) * 12.0f;
    if (std::abs (th) < 1.52f)
    {
        const float dh = (rr - 1.0f) * 5.1f, dv = Z + 2.4f;
        const float q = (dh / 0.78f) * (dh / 0.78f) + (dv / 1.25f) * (dv / 1.25f);
        if (q < 1.0f)
        {
            bone = true;
            hu = q > 0.6f ? CORT : trab (X, Y, Z, 1.3f);
            if ((dh / 0.28f) * (dh / 0.28f) + ((Z + 2.8f) / 0.22f) * ((Z + 2.8f) / 0.22f) < 1.0f) { hu = 28.0f; bone = false; }
        }
    }
    for (int s = -1; s <= 1; s += 2)
    {
        const float sx = (float) s;
        if (std::abs (X - 4.7f * sx) < 0.36f && Y > -2.9f && Y < -0.3f && Z > -3.6f && Z < 3.7f) { bone = true; hu = CORT - 80.0f; }
        if (ell (X - 4.7f * sx, Y + 1.0f, Z - 3.7f, 0.75f, 0.75f, 0.7f) < 1.0f) { bone = true; hu = CORT - 200.0f; }
        if (ell (X - 4.7f * sx, Y - 0.4f, Z - 3.0f, 0.5f, 0.55f, 0.9f) < 1.0f) { bone = true; hu = CORT - 200.0f; }
    }
    //  the maxilla above the upper teeth, the hard palate, the sinuses over it
    {
        const float rm = std::sqrt (X * X / (4.9f * 4.9f) + (Y - acy - 0.2f) * (Y - acy - 0.2f) / (5.8f * 5.8f));
        if (Z > 0.8f && Z < 2.7f && rm < 1.0f && rm > 0.76f && std::abs (th) < 1.5f) { bone = true; hu = rm > 0.9f || rm < 0.82f ? CORT - 150.0f : trab (X, Y, Z, 1.3f); }
        if (Z > 2.3f && Z < 2.75f && rm < 0.95f) { bone = true; hu = 720.0f; }                                 // hard palate
        if (Z > 2.75f && rm < 0.95f)
        {
            hu = SOFT;
            if (std::abs (X) < 1.05f) hu = AIR;                                                              // nasal cavity
            if (std::abs (X) < 0.1f) { hu = 420.0f; bone = true; }                                          // septum
            if (ell (std::abs (X) - 2.8f, Y - 0.2f, Z - 4.6f, 1.6f, 1.9f, 1.8f) < 1.0f) hu = AIR;              // maxillary sinuses
        }
    }
    //  lower teeth, then upper teeth
    float t = tooth (X, Y, Z, acx, acy, arx * 0.85f, ary * 0.85f, -1.2f, -0.2f, -2.9f);
    if (t < -9000.0f) t = tooth (X, Y, Z, acx, acy + 0.2f, arx * 0.89f, ary * 0.89f, -0.2f, 0.8f, 2.3f);
    if (t > -9000.0f) { hu = t; tooth_ = true; }
    (void) bone; (void) tooth_;
    return hu + grain (x, y, z, 7.0f);
}

} } // namespace bs::an
