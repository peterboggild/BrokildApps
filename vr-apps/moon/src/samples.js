/* Rock samples (chapter 6): a station's worth of rocks to collect and stow on the rover.
 *
 * A dozen rocks lie about a spot in the valley, each of some kilograms (Apollo 17 brought home about 110 kg). The
 * suit does not bend: the crews used tongs. So a grip with your hand over a rock, within a long arm of it, lifts it
 * to your hand; carry it to the back of the rover and let go there, and it is in the sample bags, weighed. Let go
 * anywhere else and it falls, slowly, as things fall here.
 *
 *   const smp = makeSamples({ moon, rig, toast, bag: () => Vector3 (world: the rover's sample bags), step(k) });
 *   smp.boulders(x, z)                    the big ones that mark the place from afar (solid: smp.solids [x, z, r])
 *   smp.spawn(x, z)                       the rocks, about that spot (world)
 *   smp.grab(hand), smp.release(hand)     a grip closing and opening (hand: { pos: Vector3, rig frame })
 *   smp.key(headGround)                   the keyboard's stand-in: the nearest rock within reach goes straight to the bags
 *   smp.update(dt)                        every frame
 *   smp.kg, smp.target, smp.done */
import * as THREE from 'three';

const G = 1.62;

export function makeSamples(o) {
  const group = new THREE.Group(), rocks = [];
  const mat = new THREE.MeshStandardMaterial({ color: 0x7c7870, roughness: 1, metalness: 0, flatShading: true });
  const matD = new THREE.MeshStandardMaterial({ color: 0x5d5a55, roughness: 1, metalness: 0, flatShading: true });
  let seed = 7; const rnd = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);
  const _v = new THREE.Vector3(), _b = new THREE.Vector3();
  const lump = (detail, k) => {
    const g = new THREE.IcosahedronGeometry(1, detail), p = g.attributes.position;
    /* an angular lump: every corner pulled in or out a little, the same wherever corners meet */
    for (let i = 0; i < p.count; i++) { const h = Math.sin(p.getX(i) * 12.9 + p.getY(i) * 78.2 + p.getZ(i) * 37.7 + k) * 43758.5, f = 0.78 + 0.34 * (h - Math.floor(h)); p.setXYZ(i, p.getX(i) * f, p.getY(i) * f, p.getZ(i) * f); }
    g.computeVertexNormals(); return g;
  };
  const big = new THREE.Group(); group.add(big);
  const api = { group, rocks, big, solids: [], kg: 0, target: 100, at: null,
    get done() { return api.kg >= api.target; },
    /* The boulders: half a dozen, from knee-high to taller than you (the real stations of this valley were at
     * boulders that had rolled down from the massifs). They stand there from the start, so the field can be seen
     * from the LM, with their long shadows; you cannot walk or drive through them. */
    boulders(x, z) {
      let s2 = 91; const r2 = () => ((s2 = (s2 * 1664525 + 1013904223) >>> 0) / 4294967296);
      const spots = [[0, -3.5, 1.7], [4.6, 2.2, 1.15], [-5.2, 1.4, 0.9], [-1.8, 6.0, 0.7], [7.4, -4.8, 0.6], [-8.6, -5.4, 1.3], [2.4, -9.6, 0.5]];
      big.position.set(x, o.moon.heightAt(x, z), z);
      for (let k = 0; k < spots.length; k++) {
        const [dx, dz, r] = spots[k], bx = x + dx, bz = z + dz;
        const m = new THREE.Mesh(lump(2, k * 3.1 + 5), k % 3 === 1 ? matD : mat);
        m.scale.set(r * (0.9 + r2() * 0.3), r * (0.6 + r2() * 0.25), r * (0.9 + r2() * 0.3)); m.rotation.set(r2() * 0.5, r2() * 6, r2() * 0.5);
        m.position.set(dx, o.moon.heightAt(bx, bz) - big.position.y + m.scale.y * 0.5, dz); big.add(m);
        api.solids.push([bx, bz, r * 0.95]);
      }
    },
    spawn(x, z) {
      api.at = new THREE.Vector3(x, o.moon.heightAt(x, z), z);
      for (let k = 0; k < 12; k++) {
        const g = lump(1, k);
        const r = 0.09 + rnd() * 0.07, m = new THREE.Mesh(g, rnd() < 0.3 ? matD : mat);
        m.scale.set(r * (0.8 + rnd() * 0.5), r * (0.6 + rnd() * 0.3), r * (0.8 + rnd() * 0.5)); m.rotation.set(rnd() * 6, rnd() * 6, rnd() * 6);
        /* (among the boulders, not inside one) */
        let rx = x, rz = z;
        for (let n = 0; n < 12; n++) { const a = rnd() * Math.PI * 2, d = 2.5 + rnd() * 7.5; rx = x + Math.cos(a) * d; rz = z + Math.sin(a) * d; if (!api.solids.some(([bx, bz, br]) => Math.hypot(rx - bx, rz - bz) < br + 0.7)) break; }
        m.position.set(rx, o.moon.heightAt(rx, rz) + m.scale.y * 0.55, rz); group.add(m);
        /* (a rock of this size, at 3 g/cm3, a little flattened: 9 to 17 kg) */
        rocks.push({ mesh: m, kg: Math.round((9 + (r - 0.09) / 0.07 * 8) * 10) / 10, state: 'ground', hand: null, vy: 0, home: m.position.clone() });
      }
    },
    grab(hand) {
      if (!hand || !hand.pos) return false;
      const hw = o.rig.localToWorld(_v.copy(hand.pos)); let best = null, bd = 0.95;
      for (const r of rocks) { if (r.state !== 'ground') continue; const d = Math.hypot(r.mesh.position.x - hw.x, r.mesh.position.z - hw.z); if (d < bd && hw.y - r.mesh.position.y < 1.6) { bd = d; best = r; } }
      if (!best) return false;
      best.state = 'held'; best.hand = hand; if (o.step) o.step(0.4); return true;
    },
    stow(r) {
      r.state = 'stowed'; r.mesh.visible = false; api.kg = Math.round((api.kg + r.kg) * 10) / 10;
      if (o.step) o.step(0.7);
      o.toast.say('Sample stowed: ' + r.kg.toFixed(1) + ' kg. ' + Math.min(api.kg, 999).toFixed(0) + ' of ' + api.target + ' kg', 4);
    },
    release(hand) {
      for (const r of rocks) if (r.state === 'held' && r.hand === hand) {
        r.hand = null; _b.copy(o.bag());
        if (r.mesh.position.distanceTo(_b) < 1.1) api.stow(r); else { r.state = 'fall'; r.vy = 0; }
      }
    },
    key(head) {
      if (!head) return false; let best = null, bd = 2.2;
      for (const r of rocks) { if (r.state !== 'ground') continue; const d = Math.hypot(r.mesh.position.x - head.x, r.mesh.position.z - head.z); if (d < bd) { bd = d; best = r; } }
      if (!best) return false;
      _b.copy(o.bag()); if (Math.hypot(_b.x - head.x, _b.z - head.z) > 12) { o.toast.say('Bring the rover nearer: the samples go in its bags', 4, true); return true; }
      api.stow(best); return true;
    },
    /* the nearest rock still on the ground (for where to go next), or null */
    nearest(x, z) { let best = null, bd = 1e9; for (const r of rocks) { if (r.state !== 'ground') continue; const d = Math.hypot(r.mesh.position.x - x, r.mesh.position.z - z); if (d < bd) { bd = d; best = r; } } return best ? { d: bd, x: best.mesh.position.x, z: best.mesh.position.z } : null; },
    update(dt) {
      for (const r of rocks) {
        if (r.state === 'held') {
          if (!r.hand.pos || !r.hand.grip) { api.release(r.hand); continue; }
          o.rig.localToWorld(_v.copy(r.hand.pos)); _v.y -= 0.14;
          r.mesh.position.lerp(_v, Math.min(1, dt * 9)); r.mesh.rotation.y += dt * 0.4;
        } else if (r.state === 'fall') {
          r.vy -= G * dt; r.mesh.position.y += r.vy * dt;
          const gy = o.moon.heightAt(r.mesh.position.x, r.mesh.position.z) + r.mesh.scale.y * 0.55;
          if (r.mesh.position.y <= gy) { r.mesh.position.y = gy; r.state = 'ground'; if (o.step) o.step(0.3); }
        }
      }
    }
  };
  return api;
}
