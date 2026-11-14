/*
 * fx.js — fire, smoke, sparks, debris and synthesised sound.
 *
 * Particles are camera-facing quads drawn as one instanced mesh per blend mode
 * (additive for fire, normal for smoke). They are sized in metres rather than
 * pixels, so they look right in both eyes whatever the headset's resolution.
 * Nothing is loaded from disk: every sound is built from oscillators and noise.
 */
import * as THREE from './vendor/three.module.min.js';

const rand = (a, b) => a + Math.random() * (b - a);

/* ------------------------------------------------------------ billboards */

const VERT = `
attribute vec3 offset; attribute float size; attribute vec4 color; attribute float rot;
varying vec2 vUv; varying vec4 vColor; varying float vRot;
void main(){
  vUv = uv; vColor = color; vRot = rot;
  vec4 mv = modelViewMatrix * vec4(offset, 1.0);
  mv.xy += position.xy * size;
  gl_Position = projectionMatrix * mv;
}`;
const FRAG = `
uniform sampler2D uTex; uniform float uCore;
varying vec2 vUv; varying vec4 vColor; varying float vRot;
void main(){
  vec2 p = vUv - 0.5;
  float d = length(p) * 2.0;
  float c = cos(vRot), s = sin(vRot);
  vec2 q = vec2(c * p.x - s * p.y, s * p.x + c * p.y) + 0.5;     // every puff is turned differently
  float a = clamp(texture2D(uTex, q).a * 1.5, 0.0, 1.0) * smoothstep(1.0, 0.6, d);
  a = max(a, uCore * pow(max(0.0, 1.0 - d), 3.0) * 0.55);          // hot centre for fire
  gl_FragColor = vec4(vColor.rgb, vColor.a * a);
}`;

/* a ragged cloud: dozens of soft blobs piled up, then faded to nothing at the rim */
let CLOUD = null;
function cloudTexture() {
  if (CLOUD) return CLOUD;
  const c = document.createElement('canvas'); c.width = c.height = 128;
  const x = c.getContext('2d');
  for (let i = 0; i < 70; i++) {
    const a = Math.random() * 6.283, r = Math.random() * 36 + 4;
    const cx = 64 + Math.cos(a) * r, cy = 64 + Math.sin(a) * r, rr = 9 + Math.random() * 26;
    const g = x.createRadialGradient(cx, cy, 0, cx, cy, rr);
    g.addColorStop(0, 'rgba(255,255,255,0.2)'); g.addColorStop(1, 'rgba(255,255,255,0)');
    x.fillStyle = g; x.fillRect(0, 0, 128, 128);
  }
  CLOUD = new THREE.CanvasTexture(c);
  return CLOUD;
}

const S = 20;   // floats per particle
class Billboards {
  constructor(scene, max, additive, order) {
    this.max = max; this.n = 0;
    this.P = new Float32Array(max * S);
    const base = new THREE.PlaneGeometry(1, 1);
    const g = new THREE.InstancedBufferGeometry();
    g.index = base.index;
    g.setAttribute('position', base.attributes.position);
    g.setAttribute('uv', base.attributes.uv);
    this.aOff = new THREE.InstancedBufferAttribute(new Float32Array(max * 3), 3).setUsage(THREE.DynamicDrawUsage);
    this.aSize = new THREE.InstancedBufferAttribute(new Float32Array(max), 1).setUsage(THREE.DynamicDrawUsage);
    this.aCol = new THREE.InstancedBufferAttribute(new Float32Array(max * 4), 4).setUsage(THREE.DynamicDrawUsage);
    this.aRot = new THREE.InstancedBufferAttribute(new Float32Array(max), 1).setUsage(THREE.DynamicDrawUsage);
    g.setAttribute('offset', this.aOff); g.setAttribute('size', this.aSize); g.setAttribute('color', this.aCol); g.setAttribute('rot', this.aRot);
    g.instanceCount = 0;
    this.geo = g;
    const m = new THREE.ShaderMaterial({
      vertexShader: VERT, fragmentShader: FRAG, transparent: true, depthWrite: false,
      uniforms: { uTex: { value: cloudTexture() }, uCore: { value: additive ? 1 : 0 } },
      blending: additive ? THREE.AdditiveBlending : THREE.NormalBlending,
    });
    this.mesh = new THREE.Mesh(g, m);
    this.mesh.frustumCulled = false;
    this.mesh.renderOrder = order;
    scene.add(this.mesh);
  }
  /** c0 -> c1 are rgb 0..1; a0 is peak alpha; drag per second; grav in m/s^2 (down). */
  add(x, y, z, vx, vy, vz, life, s0, s1, c0, c1, a0 = 1, drag = 0, grav = 0) {
    if (this.n >= this.max) return;
    const p = this.P, o = this.n++ * S;
    p[o] = x; p[o + 1] = y; p[o + 2] = z; p[o + 3] = vx; p[o + 4] = vy; p[o + 5] = vz;
    p[o + 6] = 0; p[o + 7] = life; p[o + 8] = s0; p[o + 9] = s1;
    p[o + 10] = c0[0]; p[o + 11] = c0[1]; p[o + 12] = c0[2];
    p[o + 13] = c1[0]; p[o + 14] = c1[1]; p[o + 15] = c1[2];
    p[o + 16] = a0; p[o + 17] = drag; p[o + 18] = grav; p[o + 19] = Math.random() * 6.283;
  }
  update(dt) {
    const p = this.P, off = this.aOff.array, sz = this.aSize.array, col = this.aCol.array, rot = this.aRot.array;
    for (let i = 0; i < this.n; i++) {
      const o = i * S;
      p[o + 6] += dt;
      if (p[o + 6] >= p[o + 7]) {                       // dead: swap the last one in
        const l = --this.n * S;
        if (l !== o) { for (let k = 0; k < S; k++) p[o + k] = p[l + k]; }
        i--; continue;
      }
      const dr = Math.max(0, 1 - p[o + 17] * dt);
      p[o + 3] *= dr; p[o + 4] = p[o + 4] * dr - p[o + 18] * dt; p[o + 5] *= dr;
      p[o] += p[o + 3] * dt; p[o + 1] += p[o + 4] * dt; p[o + 2] += p[o + 5] * dt;
    }
    for (let i = 0; i < this.n; i++) {
      const o = i * S, u = p[o + 6] / p[o + 7];
      off[i * 3] = p[o]; off[i * 3 + 1] = p[o + 1]; off[i * 3 + 2] = p[o + 2];
      sz[i] = p[o + 8] + (p[o + 9] - p[o + 8]) * u;
      rot[i] = p[o + 19] + p[o + 6] * 0.7;
      col[i * 4] = p[o + 10] + (p[o + 13] - p[o + 10]) * u;
      col[i * 4 + 1] = p[o + 11] + (p[o + 14] - p[o + 11]) * u;
      col[i * 4 + 2] = p[o + 12] + (p[o + 15] - p[o + 12]) * u;
      col[i * 4 + 3] = p[o + 16] * Math.min(1, u * 12) * (1 - u);
    }
    this.geo.instanceCount = this.n;
    this.aOff.needsUpdate = this.aSize.needsUpdate = this.aCol.needsUpdate = this.aRot.needsUpdate = true;
  }
}

/* ---------------------------------------------------------------- the fx */

const WHITE = [1, 0.95, 0.75], YEL = [1, 0.75, 0.25], ORG = [1, 0.4, 0.08], RED = [0.5, 0.05, 0.0];
const SMOKE0 = [0.34, 0.33, 0.32], SMOKE1 = [0.1, 0.1, 0.1];

export function createFX(scene) {
  const fire = new Billboards(scene, 2600, true, 20);
  const smoke = new Billboards(scene, 1800, false, 19);

  // one real light for the flash of a blast; it is always in the scene so no
  // shader ever has to be recompiled in the middle of a fight
  const flash = new THREE.PointLight(0xffa850, 0, 2.8, 1.6);
  scene.add(flash);
  let flashT = 0;

  /* debris: small boxes and bigger bodies (turrets) that tumble and settle */
  const debris = [];
  const dGeo = new THREE.BoxGeometry(1, 1, 1);
  const dMat = new THREE.MeshStandardMaterial({ color: 0x23251f, roughness: 0.9 });
  const dMatCamo = new THREE.MeshStandardMaterial({ color: 0x4d5a33, roughness: 0.8 });

  const fx = {
    fire, smoke, scene,

    explosion(p, s = 1) {
      // flash
      fire.add(p.x, p.y, p.z, 0, 0, 0, 0.14, 0.5 * s, 1.1 * s, WHITE, YEL, 1);
      // fireball
      for (let i = 0; i < 16; i++) {
        const sp = rand(0.2, 1.4) * s, a = rand(0, 6.283), e = rand(-0.2, 1);
        fire.add(p.x, p.y, p.z, Math.cos(a) * sp, e * sp + 0.3, Math.sin(a) * sp,
          rand(0.3, 0.7), rand(0.08, 0.15) * s, rand(0.28, 0.5) * s, YEL, RED, 0.95, 2.5, -0.4);
      }
      // sparks
      for (let i = 0; i < 28; i++) {
        const sp = rand(1.5, 4.5) * s, a = rand(0, 6.283), e = rand(-0.1, 1);
        fire.add(p.x, p.y, p.z, Math.cos(a) * sp, e * sp + 0.8, Math.sin(a) * sp,
          rand(0.4, 1.0), 0.022 * s, 0.01 * s, WHITE, ORG, 1, 1.2, 7);
      }
      // smoke
      for (let i = 0; i < 12; i++) {
        const a = rand(0, 6.283), sp = rand(0.1, 0.6) * s;
        smoke.add(p.x, p.y, p.z, Math.cos(a) * sp, rand(0.2, 0.7), Math.sin(a) * sp,
          rand(1.2, 2.4), 0.12 * s, rand(0.45, 0.8) * s, SMOKE0, SMOKE1, 0.55, 0.8, -0.05);
      }
      // dust ring if it went off near the floor
      if (p.y < 0.35) for (let i = 0; i < 10; i++) {
        const a = (i / 10) * 6.283;
        smoke.add(p.x, 0.03, p.z, Math.cos(a) * 1.3 * s, 0.05, Math.sin(a) * 1.3 * s,
          0.9, 0.08 * s, 0.3 * s, [0.5, 0.46, 0.4], [0.4, 0.37, 0.33], 0.4, 2.2, 0);
      }
      flash.position.copy(p); flash.intensity = 9 * s; flashT = 0.22;
    },

    /** Small bang: a shell hitting a wall or the floor. */
    puff(p, s = 0.5) {
      fire.add(p.x, p.y, p.z, 0, 0, 0, 0.1, 0.18 * s, 0.4 * s, WHITE, YEL, 1);
      for (let i = 0; i < 8; i++) {
        const a = rand(0, 6.283), sp = rand(0.4, 1.4) * s;
        fire.add(p.x, p.y, p.z, Math.cos(a) * sp, rand(0, 1) * sp, Math.sin(a) * sp, rand(0.2, 0.45), 0.06 * s, 0.14 * s, YEL, RED, 0.9, 2.5, 0);
      }
      for (let i = 0; i < 5; i++)
        smoke.add(p.x, p.y, p.z, rand(-0.2, 0.2), rand(0.1, 0.4), rand(-0.2, 0.2), rand(0.7, 1.4), 0.07 * s, 0.25 * s, SMOKE0, SMOKE1, 0.4, 0.6, 0);
      flash.position.copy(p); flash.intensity = Math.max(flash.intensity, 4 * s * 2); flashT = Math.max(flashT, 0.12);
    },

    /** The kamikaze rat's firecracker: a sharp little bang with coloured sparks, not a battlefield explosion. */
    bang(p, s = 1) {
      fire.add(p.x, p.y, p.z, 0, 0, 0, 0.1, 0.2 * s, 0.55 * s, WHITE, YEL, 1);
      const COL = [[1, 0.3, 0.2], [1, 0.85, 0.2], [0.3, 0.9, 0.4], [0.4, 0.65, 1], [1, 0.5, 0.9]];
      for (let i = 0; i < 18; i++) {
        const a = rand(0, 6.283), sp = rand(0.8, 2.4) * s, e = rand(0.1, 1), c = COL[i % COL.length];
        fire.add(p.x, p.y, p.z, Math.cos(a) * sp, e * sp + 0.6, Math.sin(a) * sp, rand(0.35, 0.8), 0.022 * s, 0.008 * s, c, c, 1, 1.4, 6);
      }
      for (let i = 0; i < 5; i++) {
        const a = rand(0, 6.283);
        smoke.add(p.x, p.y, p.z, Math.cos(a) * 0.3, rand(0.2, 0.5), Math.sin(a) * 0.3, rand(0.6, 1.2), 0.07 * s, 0.25 * s, [0.55, 0.53, 0.5], [0.3, 0.3, 0.3], 0.4, 0.8, 0);
      }
      flash.position.copy(p); flash.intensity = Math.max(flash.intensity, 5 * s); flashT = Math.max(flashT, 0.1);
    },

    /** A rat that has been shot: a cartoon puff and a few yellow stars. */
    poof(p, s = 1) {
      for (let i = 0; i < 9; i++) {
        const a = rand(0, 6.283), sp = rand(0.1, 0.5) * s;
        smoke.add(p.x, p.y, p.z, Math.cos(a) * sp, rand(0.1, 0.4), Math.sin(a) * sp, rand(0.5, 1.0), 0.05 * s, 0.17 * s, [0.85, 0.83, 0.8], [0.6, 0.58, 0.55], 0.55, 1, 0);
      }
      for (let i = 0; i < 8; i++) {
        const a = rand(0, 6.283), sp = rand(0.5, 1.4) * s;
        fire.add(p.x, p.y, p.z, Math.cos(a) * sp, rand(0.4, 1.3) * s, Math.sin(a) * sp, rand(0.4, 0.8), 0.03 * s, 0.012 * s, [1, 0.95, 0.5], [1, 0.8, 0.2], 1, 1.2, 4);
      }
    },

    /** A few sparks and a wisp: a bullet hitting a wall or a tank. No light, so a burst of them stays cheap. */
    spark(p, s = 1) {
      for (let i = 0; i < 4; i++)
        fire.add(p.x, p.y, p.z, rand(-1, 1) * 0.9 * s, rand(0.2, 1.2) * s, rand(-1, 1) * 0.9 * s, rand(0.15, 0.35), 0.014 * s, 0.006 * s, WHITE, ORG, 1, 1.5, 6);
      fire.add(p.x, p.y, p.z, 0, 0, 0, 0.06, 0.05 * s, 0.1 * s, WHITE, YEL, 0.9);
      smoke.add(p.x, p.y, p.z, rand(-0.05, 0.05), rand(0.05, 0.2), rand(-0.05, 0.05), rand(0.4, 0.8), 0.03 * s, 0.08 * s, SMOKE0, SMOKE1, 0.3, 0.8, 0);
    },

    muzzleFlash(p, d) {
      fire.add(p.x + d.x * 0.02, p.y + d.y * 0.02, p.z + d.z * 0.02, 0, 0, 0, 0.09, 0.05, 0.1, WHITE, ORG, 1);
      for (let i = 0; i < 5; i++)
        fire.add(p.x, p.y, p.z, d.x * rand(0.5, 1.5) + rand(-0.2, 0.2), d.y * rand(0.5, 1.5) + rand(-0.2, 0.2), d.z * rand(0.5, 1.5) + rand(-0.2, 0.2),
          rand(0.12, 0.25), 0.025, 0.012, YEL, ORG, 1, 3, 0);
      for (let i = 0; i < 3; i++)
        smoke.add(p.x, p.y, p.z, d.x * 0.3 + rand(-0.05, 0.05), d.y * 0.3 + rand(0, 0.1), d.z * 0.3 + rand(-0.05, 0.05), rand(0.6, 1.1), 0.03, 0.1, SMOKE0, SMOKE1, 0.35, 1, 0);
      flash.position.copy(p); flash.intensity = Math.max(flash.intensity, 1.4); flashT = Math.max(flashT, 0.07);
    },

    debrisBox(p, v, size, camo) {
      const m = new THREE.Mesh(dGeo, camo ? dMatCamo : dMat);
      m.scale.set(size * rand(0.7, 1.4), size * rand(0.4, 1), size * rand(0.7, 1.4));
      m.position.copy(p);
      m.rotation.set(rand(0, 6), rand(0, 6), rand(0, 6));
      scene.add(m);
      debris.push({ obj: m, v: v.clone(), w: new THREE.Vector3(rand(-9, 9), rand(-9, 9), rand(-9, 9)), life: rand(2.5, 4), age: 0, burn: rand(0.4, 1.2), own: false });
    },
    /** A real object (the turret) becomes debris. */
    flingObject(obj, v) {
      scene.attach(obj);       // keep its world transform
      debris.push({ obj, v: v.clone(), w: new THREE.Vector3(rand(-6, 6), rand(-6, 6), rand(-6, 6)), life: 6, age: 0, burn: 3, own: true });
    },

    update(dt) {
      fire.update(dt); smoke.update(dt);
      if (flashT > 0) { flashT -= dt; flash.intensity *= Math.pow(0.0006, dt); if (flashT <= 0) flash.intensity = 0; }
      for (let i = debris.length - 1; i >= 0; i--) {
        const d = debris[i], o = d.obj;
        d.age += dt;
        d.v.y -= 9.8 * dt;
        o.position.addScaledVector(d.v, dt);
        if (o.position.y < 0.025) {                      // bounce, then settle
          o.position.y = 0.025;
          d.v.y *= -0.35; d.v.x *= 0.6; d.v.z *= 0.6; d.w.multiplyScalar(0.5);
          if (Math.abs(d.v.y) < 0.3) d.v.y = 0;
        }
        o.rotation.x += d.w.x * dt; o.rotation.y += d.w.y * dt; o.rotation.z += d.w.z * dt;
        if (d.age < d.burn && Math.random() < dt * 30)   // trailing smoke while it burns
          smoke.add(o.position.x, o.position.y, o.position.z, rand(-0.05, 0.05), 0.15, rand(-0.05, 0.05), 0.8, 0.03, 0.14, SMOKE0, SMOKE1, 0.45, 0.5, 0);
        if (d.age > d.life) {
          scene.remove(o);
          if (!d.own) { /* shared geo/mat */ }
          debris.splice(i, 1);
        }
      }
    },
    clear() {
      for (const d of debris) scene.remove(d.obj);
      debris.length = 0; fire.n = 0; smoke.n = 0;
    },
  };
  return fx;
}

/* ----------------------------------------------------------------- audio */

export function createAudio() {
  let ctx = null, master = null, noiseBuf = null;
  const A = {
    ready: false,
    init() {
      if (ctx) { ctx.resume?.(); return; }
      const AC = window.AudioContext || window.webkitAudioContext;
      if (!AC) return;
      ctx = new AC();
      master = ctx.createGain(); master.gain.value = 0.7;
      const comp = ctx.createDynamicsCompressor();
      master.connect(comp); comp.connect(ctx.destination);
      noiseBuf = ctx.createBuffer(1, ctx.sampleRate * 2, ctx.sampleRate);
      const d = noiseBuf.getChannelData(0);
      for (let i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1;
      A.ready = true;
    },
    /** Position the listener at the player's head. */
    listener(pos, fwd, up) {
      if (!ctx) return;
      const L = ctx.listener;
      if (L.positionX) {
        L.positionX.value = pos.x; L.positionY.value = pos.y; L.positionZ.value = pos.z;
        L.forwardX.value = fwd.x; L.forwardY.value = fwd.y; L.forwardZ.value = fwd.z;
        L.upX.value = up.x; L.upY.value = up.y; L.upZ.value = up.z;
      } else { L.setPosition(pos.x, pos.y, pos.z); L.setOrientation(fwd.x, fwd.y, fwd.z, up.x, up.y, up.z); }
    },
    _out(pos) {
      if (!pos) return master;
      const p = ctx.createPanner();
      p.panningModel = 'HRTF'; p.distanceModel = 'inverse'; p.refDistance = 0.9; p.rolloffFactor = 1.1;
      if (p.positionX) { p.positionX.value = pos.x; p.positionY.value = pos.y; p.positionZ.value = pos.z; } else p.setPosition(pos.x, pos.y, pos.z);
      p.connect(master);
      return p;
    },
    _noise(out, t, dur, type, f0, f1, g, q = 1) {
      const s = ctx.createBufferSource(); s.buffer = noiseBuf; s.loop = true;
      const f = ctx.createBiquadFilter(); f.type = type; f.Q.value = q;
      f.frequency.setValueAtTime(f0, t); f.frequency.exponentialRampToValueAtTime(Math.max(20, f1), t + dur);
      const gn = ctx.createGain();
      gn.gain.setValueAtTime(0.0001, t); gn.gain.exponentialRampToValueAtTime(g, t + 0.01); gn.gain.exponentialRampToValueAtTime(0.0001, t + dur);
      s.connect(f); f.connect(gn); gn.connect(out); s.start(t, Math.random()); s.stop(t + dur + 0.05);
    },
    _tone(out, t, dur, type, f0, f1, g) {
      const o = ctx.createOscillator(); o.type = type;
      o.frequency.setValueAtTime(f0, t); o.frequency.exponentialRampToValueAtTime(Math.max(10, f1), t + dur);
      const gn = ctx.createGain();
      gn.gain.setValueAtTime(g, t); gn.gain.exponentialRampToValueAtTime(0.0001, t + dur);
      o.connect(gn); gn.connect(out); o.start(t); o.stop(t + dur + 0.05);
    },
    explosion(pos, size = 1) {
      if (!ctx) return; const t = ctx.currentTime, out = A._out(pos);
      A._noise(out, t, 0.9 * size + 0.3, 'lowpass', 2600, 90, 1.0 * Math.min(1.3, size));
      A._noise(out, t, 0.25, 'highpass', 3000, 1200, 0.3);
      A._tone(out, t, 0.7, 'sine', 110, 28, 1.0 * Math.min(1.3, size));
    },
    launch(pos) {
      if (!ctx) return; const t = ctx.currentTime, out = A._out(pos);
      A._noise(out, t, 0.9, 'bandpass', 500, 2400, 0.7, 0.8);
      A._tone(out, t, 0.18, 'square', 140, 50, 0.35);
    },
    tankShot(pos) {
      if (!ctx) return; const t = ctx.currentTime, out = A._out(pos);
      A._tone(out, t, 0.35, 'sawtooth', 150, 40, 0.7);
      A._noise(out, t, 0.3, 'lowpass', 2200, 200, 0.8);
    },
    hurt() {
      if (!ctx) return; const t = ctx.currentTime;
      A._tone(master, t, 0.5, 'sine', 90, 30, 0.9);
      A._noise(master, t, 0.35, 'lowpass', 900, 100, 0.6);
      A._tone(master, t, 0.9, 'sine', 1800, 1500, 0.05);     // ear ring
    },
    squeak(pos, hi = 1) {
      if (!ctx) return; const t = ctx.currentTime, out = A._out(pos);
      A._tone(out, t, 0.09, 'triangle', 1500 * hi, 2300 * hi, 0.18);
      A._tone(out, t + 0.11, 0.08, 'triangle', 1900 * hi, 2700 * hi, 0.16);
    },
    fuse(pos) {
      if (!ctx) return; const t = ctx.currentTime, out = A._out(pos);
      A._noise(out, t, 0.45, 'highpass', 5000, 3000, 0.22);
    },
    bangSmall(pos) {
      if (!ctx) return; const t = ctx.currentTime, out = A._out(pos);
      A._noise(out, t, 0.14, 'highpass', 4500, 900, 0.9);
      A._tone(out, t, 0.12, 'sine', 220, 60, 0.7);
    },
    mg(pos) {
      if (!ctx) return; const t = ctx.currentTime, out = A._out(pos);
      A._noise(out, t, 0.07, 'highpass', 1800, 700, 0.35);
      A._tone(out, t, 0.06, 'square', 220, 90, 0.18);
    },
    pickup(heavy) {
      if (!ctx) return; const t = ctx.currentTime;
      A._tone(master, t, 0.12, 'triangle', heavy ? 330 : 440, heavy ? 330 : 440, 0.25);
      A._tone(master, t + 0.09, 0.2, 'triangle', heavy ? 495 : 660, heavy ? 495 : 660, 0.25);
    },
    click(pos) {
      if (!ctx) return; const t = ctx.currentTime, out = A._out(pos);
      A._tone(out, t, 0.06, 'square', 900, 400, 0.15);
    },
    reload(pos) {
      if (!ctx) return; const t = ctx.currentTime, out = A._out(pos);
      A._tone(out, t, 0.05, 'square', 500, 300, 0.2);
      A._tone(out, t + 0.12, 0.08, 'square', 300, 160, 0.25);
    },
    alert(kind) {
      if (!ctx) return; const t = ctx.currentTime;
      if (kind === 'wave') { A._tone(master, t, 0.25, 'triangle', 330, 330, 0.25); A._tone(master, t + 0.18, 0.4, 'triangle', 495, 495, 0.25); }
      else if (kind === 'over') { A._tone(master, t, 1.2, 'sawtooth', 220, 55, 0.35); }
    },
    /** A helicopter: blade slap (noise chopped at the blade-pass rate), a low thump and a turbine whine. */
    rotor() {
      if (!ctx) return { set() {}, stop() {} };
      const p = ctx.createPanner();
      p.panningModel = 'HRTF'; p.distanceModel = 'inverse'; p.refDistance = 0.9; p.rolloffFactor = 1.2;
      const g = ctx.createGain(); g.gain.value = 0;
      const src = ctx.createBufferSource(); src.buffer = noiseBuf; src.loop = true;
      const lp = ctx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 700;
      const am = ctx.createGain(); am.gain.value = 0.3;
      const lfo = ctx.createOscillator(); lfo.type = 'sawtooth'; lfo.frequency.value = 20;
      const lg = ctx.createGain(); lg.gain.value = 0.28;
      lfo.connect(lg); lg.connect(am.gain);
      src.connect(lp); lp.connect(am); am.connect(g);
      const thump = ctx.createOscillator(); thump.type = 'sine'; thump.frequency.value = 52;
      const tg = ctx.createGain(); tg.gain.value = 0.35; thump.connect(tg); tg.connect(g);
      const whine = ctx.createOscillator(); whine.type = 'triangle'; whine.frequency.value = 1150;
      const wg = ctx.createGain(); wg.gain.value = 0.012; whine.connect(wg); wg.connect(g);
      g.connect(p); p.connect(master);
      src.start(0, Math.random()); lfo.start(); thump.start(); whine.start();
      return {
        set(pos, speed) {
          const t = ctx.currentTime;
          if (p.positionX) { p.positionX.setTargetAtTime(pos.x, t, 0.05); p.positionY.setTargetAtTime(pos.y, t, 0.05); p.positionZ.setTargetAtTime(pos.z, t, 0.05); }
          g.gain.setTargetAtTime(0.2 + 0.12 * speed, t, 0.15);
          lfo.frequency.setTargetAtTime(19 + 6 * speed, t, 0.2);
          thump.frequency.setTargetAtTime(50 + 8 * speed, t, 0.2);
          whine.frequency.setTargetAtTime(1100 + 450 * speed, t, 0.2);
        },
        stop() {
          const t = ctx.currentTime;
          g.gain.setTargetAtTime(0, t, 0.25);
          try { src.stop(t + 1.2); lfo.stop(t + 1.2); thump.stop(t + 1.2); whine.stop(t + 1.2); } catch (_) { /* already stopped */ }
        },
      };
    },
    /** A looping engine for one tank. Returns {set(pos, speed01), stop()}. */
    engine() {
      if (!ctx) return { set() {}, stop() {} };
      const p = ctx.createPanner();
      p.panningModel = 'HRTF'; p.distanceModel = 'inverse'; p.refDistance = 0.7; p.rolloffFactor = 1.4;
      const g = ctx.createGain(); g.gain.value = 0;
      const lp = ctx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 260;
      const o1 = ctx.createOscillator(); o1.type = 'sawtooth'; o1.frequency.value = 42;
      const o2 = ctx.createOscillator(); o2.type = 'square'; o2.frequency.value = 63;
      const lfo = ctx.createOscillator(); lfo.frequency.value = 11;
      const lg = ctx.createGain(); lg.gain.value = 0.35;
      const am = ctx.createGain(); am.gain.value = 0.65;
      lfo.connect(lg); lg.connect(am.gain);
      o1.connect(lp); o2.connect(lp); lp.connect(am); am.connect(g); g.connect(p); p.connect(master);
      o1.start(); o2.start(); lfo.start();
      return {
        set(pos, speed) {
          const t = ctx.currentTime;
          if (p.positionX) { p.positionX.setTargetAtTime(pos.x, t, 0.05); p.positionY.setTargetAtTime(pos.y, t, 0.05); p.positionZ.setTargetAtTime(pos.z, t, 0.05); }
          g.gain.setTargetAtTime(0.045 + 0.09 * speed, t, 0.1);
          o1.frequency.setTargetAtTime(40 + 18 * speed, t, 0.15);
          o2.frequency.setTargetAtTime(60 + 26 * speed, t, 0.15);
          lfo.frequency.setTargetAtTime(9 + 8 * speed, t, 0.15);
        },
        stop() {
          const t = ctx.currentTime;
          g.gain.setTargetAtTime(0, t, 0.05);
          o1.stop(t + 0.4); o2.stop(t + 0.4); lfo.stop(t + 0.4);
        },
      };
    },
  };
  return A;
}
