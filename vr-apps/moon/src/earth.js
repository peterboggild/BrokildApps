/* Everything outside the spacecraft during the launch: the sky, the Earth, the Sun, and on the pad the ground and
 * the launch umbilical tower. Seen only through the windows (stencil), so the cost is small.
 *
 * Floating origin: the camera rig stays at the origin; every frame the world is placed relative to the vehicle
 * (positions computed in float64 in ascent.js). Directions are in the pad frame. */
import * as THREE from 'three';
import { RE, EYE_H, SUN, PAD_TO_ECEF } from './ascent.js?v=4';

export const OUTSIDE_STENCIL = { stencilWrite: true, stencilRef: 1, stencilFunc: THREE.NotEqualStencilFunc,
  stencilFail: THREE.KeepStencilOp, stencilZFail: THREE.KeepStencilOp, stencilZPass: THREE.KeepStencilOp, stencilWriteMask: 0 };

const DISPLAY = /* glsl */`
uniform float uExposure;
vec3 display(vec3 c) {
  c *= uExposure;
  c = 1.0 - exp(-c * (1.0 + 0.35 * c) / (1.0 + 0.9 * c));
  return pow(clamp(c, 0.0, 1.0), vec3(1.0 / 2.2));
}`;

/* sky table lookup: direction -> (in-scattered light, transmittance), in the local frame under the vehicle */
const SKY_GLSL = /* glsl */`
uniform sampler2D uSky;
uniform vec3 uUp;
uniform vec3 uSunDir;
uniform float uLimb;
vec4 skyLookup(vec3 d) {
  float el = asin(clamp(dot(d, uUp), -1.0, 1.0));
  vec3 hs = uSunDir - uUp * dot(uSunDir, uUp), hd = d - uUp * dot(d, uUp);
  float ca = dot(normalize(hs + 1e-6), normalize(hd + 1e-6));
  float u = acos(clamp(ca, -1.0, 1.0)) / 3.14159265;
  float x = el - uLimb;
  float v = x >= 0.0 ? 0.5 + 0.5 * sqrt(x / (1.5707963 - uLimb)) : 0.5 - 0.5 * sqrt(-x / (1.5707963 + uLimb));
  return texture2D(uSky, vec2(u, v));
}`;

/* a tiling value-noise texture for fine cloud detail */
function detailTexture() {
  const n = 512, d = new Uint8Array(n * n * 4), h = new Float32Array(n * n);
  let seed = 3;
  const rnd = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);
  for (let oct = 0; oct < 6; oct++) {
    const c = 4 << oct, g = new Float32Array(c * c), amp = Math.pow(0.55, oct);
    for (let i = 0; i < g.length; i++) g[i] = rnd();
    for (let y = 0; y < n; y++) for (let x = 0; x < n; x++) {
      const fx = x / n * c, fy = y / n * c, x0 = Math.floor(fx), y0 = Math.floor(fy);
      let tx = fx - x0, ty = fy - y0; tx = tx * tx * (3 - 2 * tx); ty = ty * ty * (3 - 2 * ty);
      const x1 = (x0 + 1) % c, y1 = (y0 + 1) % c;
      h[y * n + x] += amp * ((g[y0 * c + x0] * (1 - tx) + g[y0 * c + x1] * tx) * (1 - ty) + (g[y1 * c + x0] * (1 - tx) + g[y1 * c + x1] * tx) * ty);
    }
  }
  let lo = 1e9, hi = -1e9; for (const v of h) { lo = Math.min(lo, v); hi = Math.max(hi, v); }
  for (let i = 0; i < n * n; i++) { const v = Math.round((h[i] - lo) / (hi - lo) * 255); d[i * 4] = d[i * 4 + 1] = d[i * 4 + 2] = v; d[i * 4 + 3] = 255; }
  const t = new THREE.DataTexture(d, n, n, THREE.RGBAFormat);
  t.wrapS = t.wrapT = THREE.RepeatWrapping; t.generateMipmaps = true; t.minFilter = THREE.LinearMipmapLinearFilter; t.magFilter = THREE.LinearFilter; t.anisotropy = 4; t.needsUpdate = true;
  return t;
}

export function makeOutside(renderer) {
  const far = new THREE.Group(), near = new THREE.Group();
  const shared = {
    uExposure: { value: 1.25 },
    uSky: { value: null },
    uUp: { value: new THREE.Vector3(0, 1, 0) },
    uSunDir: { value: new THREE.Vector3().fromArray(SUN) },
    uLimb: { value: -0.01 }
  };

  /* ---------------- the sky table, from the worker */
  const skyTex = new THREE.DataTexture(new Uint16Array(4 * 4 * 4), 4, 4, THREE.RGBAFormat, THREE.HalfFloatType);
  skyTex.magFilter = skyTex.minFilter = THREE.LinearFilter;
  skyTex.wrapS = THREE.ClampToEdgeWrapping; skyTex.wrapT = THREE.ClampToEdgeWrapping;
  shared.uSky.value = skyTex;
  const worker = new Worker(new URL('./sky-worker.js?v=4', import.meta.url), { type: 'module' });
  let skyBusy = false, skyAlt = -1, skySun = 9, nextId = 1, skyReady = null;
  const ready = new Promise(r => { skyReady = r; });
  worker.onmessage = e => {
    const m = e.data;
    /* half floats: filtered linearly everywhere WebGL 2 runs (32-bit float filtering is optional on mobile GPUs) */
    const hf = new Uint16Array(m.data.length);
    for (let i = 0; i < hf.length; i++) hf[i] = THREE.DataUtils.toHalfFloat(Math.min(m.data[i], 60000));
    const tex = new THREE.DataTexture(hf, m.nu, m.nv, THREE.RGBAFormat, THREE.HalfFloatType);
    tex.magFilter = tex.minFilter = THREE.LinearFilter;
    tex.wrapS = tex.wrapT = THREE.ClampToEdgeWrapping; tex.needsUpdate = true;
    const old = shared.uSky.value; shared.uSky.value = tex; old.dispose();
    shared.uLimb.value = m.limb;
    skyBusy = false; skyReady();
  };
  function updateSky(alt, sunEl) {
    if (skyBusy) return;
    const rel = Math.abs(alt - skyAlt) / Math.max(200, skyAlt);
    if (rel < 0.04 && Math.abs(sunEl - skySun) < 0.004) return;
    skyBusy = true; skyAlt = alt; skySun = sunEl;
    worker.postMessage({ id: nextId++, alt, sunEl });
  }

  /* ---------------- sky dome (around the camera, behind everything) */
  const skyMat = new THREE.ShaderMaterial({
    uniforms: shared, depthTest: false, depthWrite: false, side: THREE.BackSide, ...OUTSIDE_STENCIL,
    vertexShader: `varying vec3 vDir; void main(){ vDir = position; vec4 p = projectionMatrix * modelViewMatrix * vec4(position, 1.0); gl_Position = p.xyww; }`,
    fragmentShader: `precision highp float; varying vec3 vDir; ${SKY_GLSL} ${DISPLAY}
      void main(){
        vec3 d = normalize(vDir);
        vec4 s = skyLookup(d);
        /* the Sun: the disc (0.27 deg radius) and a glare, dimmed by the air in front of it */
        float c = dot(d, normalize(uSunDir));
        float a = acos(clamp(c, -1.0, 1.0)) * 57.2958;
        float sun = smoothstep(0.29, 0.25, a) * 400.0 + 6.0 * exp(-a * 1.6) + 0.6 * exp(-a * 0.35);
        gl_FragColor = vec4(display(s.rgb + vec3(1.0, 0.97, 0.92) * sun * s.a), 1.0);
      }`
  });
  const dome = new THREE.Mesh(new THREE.SphereGeometry(1, 48, 32), skyMat);
  dome.frustumCulled = false; dome.renderOrder = -10;
  far.add(dome);

  /* ---------------- the Earth */
  const loader = new THREE.TextureLoader(), base = new URL('../assets/earth/', import.meta.url).href;
  const tex = (n, srgb) => { const t = loader.load(base + n); t.colorSpace = srgb ? THREE.SRGBColorSpace : THREE.NoColorSpace; t.anisotropy = 4; return t; };
  const earthTex = tex('earth.jpg', true), regionTex = tex('region.jpg', true), cloudTex = tex('clouds.png', false);
  earthTex.wrapS = cloudTex.wrapS = THREE.RepeatWrapping;
  const earthU = {
    ...shared,
    uEarth: { value: earthTex }, uRegion: { value: regionTex }, uClouds: { value: cloudTex },
    uRegionBox: { value: new THREE.Vector4(23, 41, -84, -48) },
    uDetail: { value: detailTexture() },
    uPadToEcef: { value: new THREE.Matrix3().set(...PAD_TO_ECEF) },
    uCentre: { value: new THREE.Vector3() }
  };
  const earthMat = new THREE.ShaderMaterial({
    uniforms: earthU, ...OUTSIDE_STENCIL,
    vertexShader: `varying vec3 vPos; varying vec3 vN; void main(){ vN = normal; vec4 w = modelMatrix * vec4(position, 1.0); vPos = w.xyz; gl_Position = projectionMatrix * viewMatrix * w; }`,
    fragmentShader: `precision highp float;
      uniform sampler2D uEarth; uniform sampler2D uRegion; uniform sampler2D uClouds; uniform sampler2D uDetail; uniform vec4 uRegionBox; uniform mat3 uPadToEcef;
      varying vec3 vPos; varying vec3 vN;
      ${SKY_GLSL} ${DISPLAY}
      void main(){
        vec3 n = normalize(vN);
        vec3 e = uPadToEcef * n;
        float lat = asin(clamp(e.z, -1.0, 1.0)), lon = atan(e.y, e.x);
        vec2 uv = vec2(lon / 6.2831853 + 0.5, lat / 3.14159265 + 0.5);
        vec3 alb = texture2D(uEarth, uv).rgb;
        vec2 ld = vec2(lat, lon) * 57.29578;
        vec2 ruv = vec2((ld.y - uRegionBox.z) / (uRegionBox.w - uRegionBox.z), (ld.x - uRegionBox.x) / (uRegionBox.y - uRegionBox.x));
        if (ruv.x > 0.0 && ruv.x < 1.0 && ruv.y > 0.0 && ruv.y < 1.0) {
          vec2 eb = min(ruv, 1.0 - ruv);
          alb = mix(alb, texture2D(uRegion, ruv).rgb, smoothstep(0.0, 0.03, min(eb.x, eb.y)));
        }
        /* the cloud map is 10 km a pixel: fine cloud detail on top, which also gives the edges their crispness */
        float c0 = texture2D(uClouds, uv).r;
        float sx = cos(lat);
        float d1 = texture2D(uDetail, uv * vec2(520.0 * sx, 260.0)).r, d2 = texture2D(uDetail, uv * vec2(4100.0 * sx, 2050.0) + 0.37).r;
        float cd = c0 + ((d1 - 0.5) * 0.5 + (d2 - 0.5) * 0.18) * smoothstep(0.0, 0.35, c0) * smoothstep(1.05, 0.55, c0);
        float cl = smoothstep(0.12, 0.72, cd);
        vec3 sun = normalize(uSunDir);
        float mu = dot(n, sun);
        float day = smoothstep(-0.03, 0.12, mu);
        /* sunlight reaching the ground: dimmed and reddened by the air towards a low Sun */
        vec3 sunT = exp(-vec3(5.8e-6, 13.5e-6, 33.1e-6) * 8000.0 / max(mu + 0.15, 0.05) - 0.08 / max(mu + 0.15, 0.05));
        vec3 V = normalize(-vPos);
        float ocean = smoothstep(0.06, 0.02, dot(alb, vec3(0.3, 0.5, 0.2))) * step(alb.r, alb.b + 0.02);
        vec3 H = normalize(V + sun);
        float glint = ocean * pow(max(dot(n, H), 0.0), 180.0) * 1.6;
        vec3 ground = alb * max(mu, 0.0) * 6.4 * sunT + glint * sunT * day * 6.0;
        /* clouds: bright tops, and a little shadow on the ground under them */
        float cs = texture2D(uClouds, uv + vec2(0.0008, 0.0006)).r;
        ground *= 1.0 - 0.45 * cs * (1.0 - cl);
        vec3 cloud = vec3(0.92) * (0.25 + 0.75 * max(mu, 0.0)) * (0.82 + 0.3 * d1) * 6.4 * sunT * day;
        vec3 col = mix(ground, cloud, cl);
        vec4 s = skyLookup(normalize(vPos));
        gl_FragColor = vec4(display(col * s.a + s.rgb), 1.0);
      }`
  });
  const earth = new THREE.Mesh(new THREE.SphereGeometry(RE, 256, 128), earthMat);
  earth.frustumCulled = false; earth.renderOrder = 0;
  far.add(earth);

  /* ---------------- on the pad: the ground near the pad (a curved disc) and the launch umbilical tower */
  const padU = { ...earthU };
  const groundMat = new THREE.ShaderMaterial({
    uniforms: padU, ...OUTSIDE_STENCIL, transparent: false,
    vertexShader: `varying vec3 vPos; varying vec3 vN; varying vec2 vXZ; void main(){ vXZ = position.xz; vN = normalize(vec3(position.x, position.y + ${RE.toFixed(1)}, position.z)); vec4 w = modelMatrix * vec4(position, 1.0); vPos = w.xyz; gl_Position = projectionMatrix * viewMatrix * w; }`,
    fragmentShader: `precision highp float;
      uniform sampler2D uRegion; uniform vec4 uRegionBox; uniform mat3 uPadToEcef;
      varying vec3 vPos; varying vec3 vN; varying vec2 vXZ;
      ${SKY_GLSL} ${DISPLAY}
      float h(vec2 p){ return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
      float vn(vec2 p){ vec2 i = floor(p), f = fract(p); f = f*f*(3.0-2.0*f);
        return mix(mix(h(i), h(i+vec2(1,0)), f.x), mix(h(i+vec2(0,1)), h(i+vec2(1,1)), f.x), f.y); }
      void main(){
        vec3 n = normalize(vN), e = uPadToEcef * n;
        vec2 ld = vec2(asin(e.z), atan(e.y, e.x)) * 57.29578;
        vec2 ruv = vec2((ld.y - uRegionBox.z) / (uRegionBox.w - uRegionBox.z), (ld.x - uRegionBox.x) / (uRegionBox.y - uRegionBox.x));
        vec3 alb = texture2D(uRegion, ruv).rgb;
        /* detail the 1 km map does not have: scrub, sand and the pad's own concrete */
        float d = length(vXZ);
        float t = vn(vXZ / 35.0) * 0.6 + vn(vXZ / 9.0) * 0.3 + vn(vXZ / 2.5) * 0.1;
        alb *= 0.75 + 0.5 * t;
        alb = mix(vec3(0.42, 0.41, 0.38), alb, smoothstep(55.0, 70.0, d));          /* the pad hardstand */
        alb = mix(alb, vec3(0.16, 0.15, 0.13), (1.0 - smoothstep(9.0, 11.0, abs(vXZ.x))) * (1.0 - smoothstep(30.0, 34.0, abs(vXZ.y - 10.0))) * 0.8); /* flame trench */
        vec3 sun = normalize(uSunDir);
        float mu = max(dot(n, sun), 0.0);
        vec3 col = alb * mu * 6.4 * exp(-vec3(5.8e-6, 13.5e-6, 33.1e-6) * 8000.0 / max(mu + 0.15, 0.05) - 0.08 / max(mu + 0.15, 0.05));
        vec4 s = skyLookup(normalize(vPos));
        float fade = smoothstep(26000.0, 30000.0, d);
        if (fade >= 1.0) discard;
        gl_FragColor = vec4(display(col * s.a + s.rgb), 1.0);
      }`
  });
  const R = 30000, rings = 64, segs = 96, gp = [], gi = [];
  for (let j = 0; j <= rings; j++) {
    const r = j === 0 ? 0 : R * Math.pow(j / rings, 2.2);
    for (let i = 0; i < segs; i++) { const a = i / segs * Math.PI * 2; gp.push(Math.cos(a) * r, -r * r / (2 * RE), Math.sin(a) * r); }
  }
  for (let j = 0; j < rings; j++) for (let i = 0; i < segs; i++) {
    const a = j * segs + i, b = j * segs + (i + 1) % segs, c = a + segs, d = b + segs;
    gi.push(a, c, b, b, c, d);
  }
  const gg = new THREE.BufferGeometry(); gg.setAttribute('position', new THREE.Float32BufferAttribute(gp, 3)); gg.setIndex(gi);
  const ground = new THREE.Mesh(gg, groundMat); ground.frustumCulled = false;
  const pad = new THREE.Group(); pad.add(ground); near.add(pad);

  /* the launch umbilical tower: red lattice, 120 m, north of the vehicle; the crew access arm swung back */
  const red = new THREE.MeshLambertMaterial({ color: 0x8a2a1c, ...OUTSIDE_STENCIL });
  const white = new THREE.MeshLambertMaterial({ color: 0xd8d6cf, ...OUTSIDE_STENCIL });
  const grey = new THREE.MeshLambertMaterial({ color: 0x6b6d70, ...OUTSIDE_STENCIL });
  const tower = new THREE.Group(); tower.position.set(0, 0, -24); pad.add(tower);
  const bars = [], W2 = 6, H = 122, DY = 6;
  const bar = (a, b, w) => bars.push([a, b, w]);
  for (const [x, z] of [[-W2, -W2], [W2, -W2], [W2, W2], [-W2, W2]]) bar([x, 7.6, z], [x, H, z], 0.9);
  for (let y = 7.6; y < H; y += DY) {
    const c = [[-W2, -W2], [W2, -W2], [W2, W2], [-W2, W2]];
    for (let k = 0; k < 4; k++) {
      const p = c[k], q = c[(k + 1) % 4];
      bar([p[0], y, p[1]], [q[0], y, q[1]], 0.45);
      bar([p[0], y, p[1]], [q[0], Math.min(H, y + DY), q[1]], 0.3);
    }
  }
  const bGeo = new THREE.BoxGeometry(1, 1, 1), im = new THREE.InstancedMesh(bGeo, red, bars.length);
  const m4 = new THREE.Matrix4(), v1 = new THREE.Vector3(), v2 = new THREE.Vector3(), qq = new THREE.Quaternion(), sc = new THREE.Vector3();
  bars.forEach(([a, b, w], k) => {
    v1.fromArray(a); v2.fromArray(b);
    const len = v1.distanceTo(v2);
    qq.setFromUnitVectors(new THREE.Vector3(0, 1, 0), v2.clone().sub(v1).normalize());
    m4.compose(v1.clone().add(v2).multiplyScalar(0.5), qq, sc.set(w, len, w));
    im.setMatrixAt(k, m4);
  });
  tower.add(im);
  /* the mobile launcher deck and the hammerhead crane */
  const deck = new THREE.Mesh(new THREE.BoxGeometry(49, 7.6, 41), grey); deck.position.set(0, 3.8, 14); pad.add(deck);
  const crane = new THREE.Mesh(new THREE.BoxGeometry(3, 3, 26), red); crane.position.set(0, H + 2, 6); tower.add(crane);
  /* swing arms (they swing away at liftoff); arm 9, the crew access arm with the White Room, parked at 12 degrees */
  const arms = [];
  for (const [y, len, w] of [[55, 18, 2.2], [70, 17, 2.2], [85, 17, 2.4], [100, 16, 2.4]]) {
    const pivot = new THREE.Group(); pivot.position.set(W2, y, W2); tower.add(pivot);
    const a = new THREE.Mesh(new THREE.BoxGeometry(w, 2.6, len), red); a.position.set(-W2, 0, len / 2); pivot.add(a);
    arms.push(pivot);
  }
  const arm9 = new THREE.Group(); arm9.position.set(W2, EYE_H - 1.6, W2); tower.add(arm9);
  const a9 = new THREE.Mesh(new THREE.BoxGeometry(1.8, 2.4, 13), red); a9.position.set(-3, 0, 6.5); arm9.add(a9);
  const wr = new THREE.Mesh(new THREE.BoxGeometry(3.6, 3, 3.2), white); wr.position.set(-3, 0.3, 13.8); arm9.add(wr);
  arm9.rotation.y = -12 * Math.PI / 180;

  /* ---------------- the escape tower and boost cover leaving, seen ahead through the windows */
  const les = new THREE.Group();
  const bpc = new THREE.Mesh(new THREE.ConeGeometry(2.0, 3.4, 32, 1, true), white); bpc.rotation.x = Math.PI; les.add(bpc);
  const lesTower = new THREE.Mesh(new THREE.CylinderGeometry(0.35, 0.6, 6, 6), red); lesTower.position.y = -4.5; les.add(lesTower);
  const flameMat = new THREE.MeshBasicMaterial({ color: 0xffd9a0, transparent: true, opacity: 0.9, blending: THREE.AdditiveBlending, depthWrite: false, ...OUTSIDE_STENCIL });
  const flame = new THREE.Mesh(new THREE.ConeGeometry(0.9, 9, 12, 1, true), flameMat); flame.position.y = -8.5; les.add(flame);
  les.visible = false; near.add(les);

  /* ---------------- per frame */
  const _c = new THREE.Vector3(), _up = new THREE.Vector3(), _s = new THREE.Vector3().fromArray(SUN);
  function update(st, t, events) {
    const p = st.pos;
    /* Earth's centre relative to the eye (float64 arithmetic, then into float32) */
    earth.position.set(-p[0], -RE - p[1], -p[2]);
    pad.position.set(-p[0], -p[1], -p[2]);
    pad.visible = st.alt < 12000;
    _up.fromArray(st.up);
    shared.uUp.value.copy(_up);
    const sunEl = Math.asin(Math.min(1, Math.max(-1, _up.dot(_s))));
    updateSky(Math.max(EYE_H, st.alt + EYE_H), sunEl);
    /* swing arms away at liftoff */
    const sw = Math.min(1, Math.max(0, (t + 0.3) / 6));
    arms.forEach(a => { a.rotation.y = -sw * 1.2; });
    arm9.rotation.y = (-12 - (t > -30 ? 0 : 0)) * Math.PI / 180;
    /* escape tower leaving: forward along the old nose direction, its motor burning */
    const dt = t - events.towerJett;
    les.visible = dt > 0 && dt < 12;
    if (les.visible) {
      const n = st.nose, hd = st.head, s = 3 + 30 * dt * dt + 6 * dt, side = 4 * dt;
      les.position.set(n[0] * s + hd[0] * side, n[1] * s + hd[1] * side, n[2] * s + hd[2] * side);
      les.quaternion.setFromUnitVectors(new THREE.Vector3(0, -1, 0), new THREE.Vector3(n[0], n[1], n[2]));
      flame.visible = dt < 4; flameMat.opacity = 0.9 * Math.max(0, 1 - dt / 4);
    }
  }
  return { far, near, shared, update, ready, dome, earth, tower, pad };
}
