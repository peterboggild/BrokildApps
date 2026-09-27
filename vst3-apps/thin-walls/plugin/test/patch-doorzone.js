// The doorway transition zone (2026-09-24): continuous paths through an open
// door, 25 ms fades for paths that come and go, symmetric late-field blend,
// MAX_PATHS 192 with refusals counted. Validates every anchor, then writes.
const fs = require('fs');
const path = require('path');
const src = path.join(__dirname, '..', 'Source');
const miss = [];
const files = {};
function load(f) { if (!files[f]) files[f] = fs.readFileSync(path.join(src, f), 'utf8'); }
function rep(f, a, b, count = 1) {
  load(f);
  const n = files[f].split(a).length - 1;
  if (n !== count) { miss.push(f + ': ' + JSON.stringify(a.slice(0, 70)) + ' x' + n); return; }
  files[f] = files[f].split(a).join(b);
}
function between(f, startMark, endMark, replacement) {
  load(f);
  const s = files[f];
  const i = s.indexOf(startMark), j = s.indexOf(endMark);
  if (i < 0 || j < 0 || j < i || s.indexOf(startMark, i + 1) >= 0 || s.indexOf(endMark, j + 1) >= 0) {
    miss.push(f + ': between ' + JSON.stringify(startMark.slice(0, 50))); return;
  }
  files[f] = s.slice(0, i) + replacement + s.slice(j);
}

const E = 'Engine.cpp';

// ---- 1. imagePath: the wall a crossing lands on alternates along an axis
rep(E, '            cr[nc].t = t; cr[nc].wall = a * 2 + (n[a] > 0 ? 1 : 0); ++nc;',
       '            // the unfolded planes alternate walls: hi, lo, hi ... going up, lo, hi ... going down\n' +
       '            cr[nc].t = t; cr[nc].wall = a * 2 + (((n[a] > 0) == (k % 2 == 0)) ? 1 : 0); ++nc;');

// ---- 2. every silent cap becomes a counted one
rep(E, 'if (nspecs >= MAX_PATHS - 8) return;', 'if (pathsFull (8)) return;', 2);
rep(E, '            if (nspecs >= MAX_PATHS - 4) return;\n            const int nx = (w == 0)',
       '            if (pathsFull (4)) return;\n            const int nx = (w == 0)');
rep(E, 'doorNow[d2] > 0.02f && nspecs < MAX_PATHS - 4)', 'doorNow[d2] > 0.02f && ! pathsFull (4))');
rep(E, 'if (closed < 0.02f || nspecs >= MAX_PATHS - 2) continue;', 'if (closed < 0.02f || pathsFull (2)) continue;');
rep(E, 'sharedWall (rs, rl, axis, pos, s0, s1, h) && nspecs < MAX_PATHS - 2)', 'sharedWall (rs, rl, axis, pos, s0, s1, h) && ! pathsFull (2))');
rep(E, 'if (nspecs >= MAX_PATHS - 1) return;', 'if (pathsFull (1)) return;');

// ---- 3. the portal model: the source room's images to second order, continuous with the in-room model
rep(E, '        const int wallS = doorWall (rs, d), wallL = doorWall (rl, d);\n',
       '        const int wallL = doorWall (rl, d);\n');
between(E, '        // (a) direct, and (b) the source\'s first-order images in its own room',
           '        // (c) the direct portal path reflected once in the LISTENER\'s room',
`        /*  The transition zone. u is how far the listener stands from the opening
            (past the plane, or beside the strip), in zone widths; w fades in what
            exists only on this side of the door. At the plane, inside the
            opening, w = 0 and litFade = 0. */
        float tPast = 0;
        const float zu = std::min (1.0f, doorZone (d, lisPos, tPast) / DOOR_ZONE_M);
        const float w = zu * zu * (3.0f - 2.0f * zu);
        const float litFade = std::min (1.0f, tPast / DOOR_ZONE_M);
        const int mat = matNow[rs];

        /*  (a) + (b): the source room's own image paths, direct and to second
            order, seen through the opening. Same keys, materials, splay and
            arrival direction as the in-room model (addImagePaths): with the
            listener IN the opening they are the same paths, so walking over the
            threshold carries every slot on instead of swapping one model for
            another - which is what the old first-order set did, in 2.7 ms, and it
            was heard as a bump. A path that is line of sight through the opening
            arrives from its own last bounce; a bent one arrives from the doorway.
            The lit side's edge loss fades in over the zone, because a listener
            standing in the opening hears the room as if there were no wall. */
        for (int order = 0; order <= 2; ++order)
            for (int nx = -order; nx <= order; ++nx)
                for (int ny = -order; ny <= order; ++ny)
                    for (int nz = -order; nz <= order; ++nz)
                    {
                        if (std::abs (nx) + std::abs (ny) + std::abs (nz) != order) continue;
                        if (pathsFull (4)) return;
                        const Vec3 I = imageOf (rs, S, nx, ny, nz);
                        Vec3 Xp; float delta;
                        if (! portalPoint (d, doorNow[d], I, lisPos, Xp, delta)) continue;
                        Vec3 b[4]; int nb = 0, walls[4]; float dummy;
                        if (order > 0 && ! imagePath (rs, S, Xp, nx, ny, nz, b, nb, walls, dummy)) continue;
                        const bool lit = delta <= 0.0f;
                        PathSpec& s = specs[(size_t) nspecs]; s = PathSpec();
                        s.key = (uint32_t) ((nx + 2) * 25 + (ny + 2) * 5 + (nz + 2));
                        s.kind = order == 0 ? (lit ? PathKind::Direct : PathKind::Portal)
                                            : (order == 1 ? PathKind::Refl1 : PathKind::Refl2);
                        for (int i = 0; i < nb; ++i)
                        {
                            const int sm = surfNow[rs].of (walls[i]);
                            for (int band = 0; band < NBAND; ++band)
                                s.bandDb[band] += 10.0f * std::log10 (std::max (1e-4f,
                                    (1.0f - MATERIAL_ALPHA[sm][band]) * (1.0f - MATERIAL_SCATTER[sm][band])));
                        }
                        addDiffraction (s, lit ? delta * litFade : delta);
                        s.npts = 0; s.pts[s.npts++] = S;
                        for (int i = nb - 1; i >= 0; --i) s.pts[s.npts++] = b[i];
                        if (! lit) s.pts[s.npts++] = Xp;
                        s.pts[s.npts++] = lisPos;
                        const Vec3 arriveFrom = lit ? (nb > 0 ? b[0] : S) : Xp;
                        const Vec3 departTo = nb > 0 ? b[nb - 1] : (lit ? lisPos : Xp);
                        const float len = (Xp - I).len() + (lisPos - Xp).len();
                        float dLen = 0, dAz = 0;
                        splayOf (nx, ny, nz, order, MATERIAL_SPLAY[mat], dLen, dAz);
                        finishSpec (s, arriveFrom, departTo, dLen != 0.0f ? std::max (0.2f, len + dLen) : len);
                        s.az += dAz;
                        if (order == 0 && lit)
                        {
                            const float sy = std::sin (rad (lisYaw)), cy = std::cos (rad (lisYaw));
                            const Vec3 rightV (sy, -cy, 0.0f);
                            const float half = 0.5f * target.earSpan;
                            s.gainL = len / std::max ((S - (lisPos - rightV * half)).len(), 0.08f);
                            s.gainR = len / std::max ((S - (lisPos + rightV * half)).len(), 0.08f);
                        }
                        ++nspecs;
                    }

`);
rep(E, '            finishSpec (s, nb > 0 ? b[0] : Xp, Xp, len);\n            ++nspecs;',
       '            finishSpec (s, nb > 0 ? b[0] : Xp, Xp, len);\n            s.gain *= w;                             // exists only once past the plane\n            ++nspecs;');

// transmission: fades in with the zone of the door between the two rooms
rep(E, `void Engine::addTransmissionPaths (const Vec3& S, int rs, int rl)
{
    if (rs == rl || rs < 0 || rl < 0) return;
`, `void Engine::addTransmissionPaths (const Vec3& S, int rs, int rl)
{
    if (rs == rl || rs < 0 || rl < 0) return;

    // near an open door between the two rooms these fade in with the zone, like
    // everything else that exists only on the far side of the plane
    float wz = 1.0f;
    for (int d = 0; d < NUM_DOORS; ++d)
    {
        const Door& D = DOORS[d];
        if (! ((D.roomA == rs && D.roomB == rl) || (D.roomB == rs && D.roomA == rl))) continue;
        float past; const float zu = std::min (1.0f, doorZone (d, lisPos, past) / DOOR_ZONE_M);
        wz = std::min (wz, zu * zu * (3.0f - 2.0f * zu));
    }
`);
rep(E, '        p.gain *= std::sqrt (closed);\n', '        p.gain *= std::sqrt (closed) * wz;\n');
rep(E, `        p.key = 0x50000u + (uint32_t) pairIndex (rs, rl); p.kind = PathKind::Wall;
        for (int band = 0; band < NBAND; ++band) p.bandDb[band] -= WALL_TL_DB[band];
        const float len = (C - S).len() + (lisPos - C).len();
        p.npts = 0; p.pts[p.npts++] = S; p.pts[p.npts++] = C; p.pts[p.npts++] = lisPos;
        finishSpec (p, C, C, len);
`, `        p.key = 0x50000u + (uint32_t) pairIndex (rs, rl); p.kind = PathKind::Wall;
        for (int band = 0; band < NBAND; ++band) p.bandDb[band] -= WALL_TL_DB[band];
        const float len = (C - S).len() + (lisPos - C).len();
        p.npts = 0; p.pts[p.npts++] = S; p.pts[p.npts++] = C; p.pts[p.npts++] = lisPos;
        finishSpec (p, C, C, len);
        p.gain *= wz;
`);

// ---- 4. door fields: per room assignment, scaled, keyed by the room they carry
rep(E, `void Engine::addDoorFieldPaths()
{
    const int rl = roomOf (lisPos.x, lisPos.y);
    if (rl < 0) return;`, `void Engine::addDoorFieldPaths (int rl, float scale)
{
    if (rl < 0 || scale < 1e-4f) return;`);
rep(E, `            p.key = 0xF0000000u + (uint32_t) d * 2u + (uint32_t) e;`,
       `            // keyed by the room whose field it carries: crossing a door used to
            // hand the same key a different feed, and the slot read on regardless
            p.key = 0xF0000000u + ((uint32_t) other << 8) + (uint32_t) d * 2u + (uint32_t) e;`);
rep(E, `            p.gain = 1.0f / (R * std::sqrt (2.0f));`, `            p.gain = scale / (R * std::sqrt (2.0f));`);

// ---- 5. buildPaths: the late fields blend across the zone
rep(E, `void Engine::buildPaths()
{
    nspecs = 0;`, `void Engine::buildPaths()
{
    nspecs = 0;
    pathsDropped = 0;`);
rep(E, `    addDoorFieldPaths();

    // the room weights for the diffuse render
    for (int r = 0; r < NUM_ROOMS; ++r) rooms[(size_t) r].weightTarget = (r == rl) ? 1.0f : 0.0f;
`, `    /*  The late fields. Inside a doorway's transition zone the room on the other
        side is a second "listener's room": the two are blended on POWER (two
        diffuse fields are uncorrelated), half and half at the plane, so a walk
        through the door hands one field to the other without a step. Each
        assignment brings its own door fields, scaled the same way. */
    int zoneRoom = -1; float wHere = 1.0f;
    if (rl >= 0)
    {
        float best = DOOR_ZONE_M;
        for (int d = 0; d < NUM_DOORS; ++d)
        {
            const Door& D = DOORS[d];
            if (D.roomA != rl && D.roomB != rl) continue;
            float past; const float z = doorZone (d, lisPos, past);
            if (z < best) { best = z; zoneRoom = D.roomA == rl ? D.roomB : D.roomA; }
        }
        if (zoneRoom >= 0) { const float u = best / DOOR_ZONE_M; wHere = 0.5f + 0.5f * u * u * (3.0f - 2.0f * u); }
    }
    addDoorFieldPaths (rl, std::sqrt (wHere));
    if (zoneRoom >= 0) addDoorFieldPaths (zoneRoom, std::sqrt (1.0f - wHere));

    // the room weights for the diffuse render (amplitude, hence the square roots)
    for (int r = 0; r < NUM_ROOMS; ++r)
        rooms[(size_t) r].weightTarget = (r == rl) ? std::sqrt (wHere) : (r == zoneRoom ? std::sqrt (1.0f - wHere) : 0.0f);
`);

// ---- 6. slots fade in and out over PATH_FADE_S
rep(E, `        if (! found) s.gainTarget = 0;`, `        if (! found) s.envTarget = 0;         // fade out on its last target, not in one block`);
rep(E, `            slot->active = true; slot->key = sp.key; slot->fresh = true; slot->gain = 0;`,
       `            slot->active = true; slot->key = sp.key; slot->fresh = true; slot->gain = 0;
            slot->env = snapFades ? 1.0f : 0.0f;`);
rep(E, `        slot->kind = sp.kind; slot->feed = sp.feed;`, `        slot->kind = sp.kind; slot->feed = sp.feed; slot->envTarget = 1.0f;`);
rep(E, `        s.active = false; s.gain = 0; s.gainTarget = 0; s.fresh = true; s.crossing = false;`,
       `        s.active = false; s.gain = 0; s.gainTarget = 0; s.fresh = true; s.crossing = false; s.env = s.envTarget = 0;`);
rep(E, `    inSq = outSq = directSq = revSq = 0;
    activePaths = 0;`, `    inSq = outSq = directSq = revSq = 0;
    activePaths = 0;
    snapFades = true;`);
rep(E, `    buildPaths();
    assignSlots();
`, `    buildPaths();
    assignSlots();
    snapFades = false;
`);
rep(E, `        if (s.gainTarget <= 0 && s.gain < 1e-6f) s.active = false;`,
       `        if ((s.envTarget <= 0 && s.env <= 0) || (s.gainTarget <= 0 && s.gain < 1e-6f)) { s.active = false; s.env = 0; }`);
rep(E, `    const float g0 = p.gain, g1 = p.gainTarget;`, `    const float g0 = p.gain, g1 = p.gainTarget;
    const float fadeStep = (float) n / (PATH_FADE_S * (float) fs);
    const float e0 = p.env;
    const float e1 = p.envTarget > p.env ? std::min (p.envTarget, p.env + fadeStep) : std::max (p.envTarget, p.env - fadeStep);`);
rep(E, `        const float g = g0 + (g1 - g0) * (float) (i + 1) / (float) n;
        x = p.filt.process (x) * g;`, `        const float fr = (float) (i + 1) / (float) n;
        const float g = (g0 + (g1 - g0) * fr) * (e0 + (e1 - e0) * fr);
        x = p.filt.process (x) * g;`);
rep(E, `    p.gain = g1;
    if (p.crossing`, `    p.gain = g1;
    p.env = e1;
    if (p.crossing`);

// ---- 7. walking through an OPEN doorway glides; only a move through a wall jumps
rep(E, `if (roomOf (sT.x, sT.y) != roomOf (srcPos[s].x, srcPos[s].y)) srcPos[s] = sT;`,
       `if (roomOf (sT.x, sT.y) != roomOf (srcPos[s].x, srcPos[s].y) && ! throughOpenDoor (srcPos[s], sT)) srcPos[s] = sT;`);
rep(E, `if (roomOf (lT.x, lT.y) != roomOf (lisPos.x, lisPos.y)) lisPos = lT;`,
       `if (roomOf (lT.x, lT.y) != roomOf (lisPos.x, lisPos.y) && ! throughOpenDoor (lisPos, lT)) lisPos = lT;`);

// ---- 8. the field weights now follow a continuous target: glide fast
rep(E, `    const float wk = 1.0f - std::exp (-(float) n / (0.2f * (float) fs));`,
       `    const float wk = 1.0f - std::exp (-(float) n / (0.05f * (float) fs));`);

// ---- 9. report, and the two helpers
rep(E, `    sc.lisRoom = std::max (0, roomOf (lisPos.x, lisPos.y));`, `    sc.lisRoom = std::max (0, roomOf (lisPos.x, lisPos.y));
    sc.pathsDropped = pathsDropped;`);
rep(E, `void Engine::addPortalPaths (const Vec3& S, int rs, int rl)
{`, `/*  How far P stands from door d's opening: its distance from the plane (also
    returned as past), combined with how far it is beside the open strip. Very
    large for a door with no opening. */
float Engine::doorZone (int d, const Vec3& P, float& past) const
{
    const Door& D = DOORS[d];
    past = std::abs (planeCoord (D, P) - D.pos);
    float lo, hi; openStrip (d, doorNow[d], lo, hi);
    if (hi - lo < 0.02f) return 1e9f;
    const float s = spanCoord (D, P);
    const float ls = std::max ({ lo - s, 0.0f, s - hi });
    return std::sqrt (past * past + ls * ls);
}

// does the straight move a -> b pass through the opening of a door between their two rooms?
bool Engine::throughOpenDoor (const Vec3& a, const Vec3& b) const
{
    const int ra = roomOf (a.x, a.y), rb = roomOf (b.x, b.y);
    if (ra < 0 || rb < 0 || ra == rb) return false;
    for (int d = 0; d < NUM_DOORS; ++d)
    {
        const Door& D = DOORS[d];
        if (! ((D.roomA == ra && D.roomB == rb) || (D.roomA == rb && D.roomB == ra))) continue;
        float lo, hi; openStrip (d, doorNow[d], lo, hi);
        if (hi - lo < 0.02f) continue;
        Vec3 X; if (! crossPlane (D, a, b, X)) continue;
        const float s = spanCoord (D, X);
        if (s >= lo && s <= hi) return true;
    }
    return false;
}

void Engine::addPortalPaths (const Vec3& S, int rs, int rl)
{`);

rep('PluginProcessor.cpp', `    obj->setProperty ("lisYaw", sc.lisYaw);`, `    obj->setProperty ("lisYaw", sc.lisYaw);
    obj->setProperty ("pathsDropped", sc.pathsDropped);`);

if (miss.length) { console.log('NOT WRITTEN, anchors missed:\n  ' + miss.join('\n  ')); process.exit(1); }
for (const f of Object.keys(files)) fs.writeFileSync(path.join(src, f), files[f]);
console.log('written: ' + Object.keys(files).join(', '));
