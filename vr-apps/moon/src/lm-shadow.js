/* The LM's shadow on the ground: its silhouette as the low Sun sees it (legs, descent stage, cabin, tunnel), laid
 * along the ground away from the Sun and draped over the terrain each frame. Stage 0's terrain shader casts no
 * shadows of things standing on it; this is the one that matters (the descending crews used it as their last
 * altitude cue, and it is in every photograph of an LM on the Moon).
 * update(x, z, h, sun): the point under the LM, the height of its footpads over the ground there, the Sun (unit).
 * bake(renderer, model, sun, { clip: [planes], read: false, nx, ny }): the model's own silhouette (a part of it only,
 * cut by the planes; read: false keeps no copy for lit()). lift(alt): the shadow of the same thing alt metres up. */
import * as THREE from 'three';

export function makeLMShadow(moon) {
  const tex = (() => {
    const c = document.createElement('canvas'); c.width = 256; c.height = 256; const x = c.getContext('2d');
    x.clearRect(0, 0, 256, 256); x.fillStyle = '#fff'; x.strokeStyle = '#fff';
    const X = u => 128 + u * 26, Y = v => 256 - v * 34;                    /* metres: u across (-4.7..4.7), v up (0..7) */
    x.fillRect(X(-2.1), Y(2.45), X(2.1) - X(-2.1), Y(0.8) - Y(2.45));       /* descent stage */
    x.beginPath(); x.moveTo(X(-1.25), Y(2.45)); x.lineTo(X(-1.4), Y(5.2)); x.lineTo(X(-0.6), Y(5.9)); x.lineTo(X(0.6), Y(5.9)); x.lineTo(X(1.4), Y(5.2)); x.lineTo(X(1.25), Y(2.45)); x.fill();
    x.fillRect(X(-0.4), Y(6.3), X(0.4) - X(-0.4), Y(5.85) - Y(6.3));        /* tunnel */
    x.lineWidth = 6; for (const s of [-1, 1]) { x.beginPath(); x.moveTo(X(s * 2.9), Y(2.2)); x.lineTo(X(s * 4.3), Y(0.15)); x.stroke(); x.beginPath(); x.moveTo(X(s * 1.6), Y(0.9)); x.lineTo(X(s * 3.2), Y(1.1)); x.stroke(); }
    x.lineWidth = 4; x.beginPath(); x.moveTo(X(0), Y(0.8)); x.lineTo(X(0), Y(0.15)); x.stroke();
    for (const u of [-4.3, 4.3, 0]) { x.beginPath(); x.ellipse(X(u), Y(0.1), 12, 5, 0, 0, Math.PI * 2); x.fill(); }
    return new THREE.CanvasTexture(c);
  })();
  let NX = 8, NY = 24, baked = null;
  let geo = new THREE.PlaneGeometry(1, 1, NX, NY);
  const mesh = new THREE.Mesh(geo, new THREE.MeshBasicMaterial({ color: 0x000000, alphaMap: tex, transparent: true, opacity: 0.9, depthWrite: false,
    polygonOffset: true, polygonOffsetFactor: -4, polygonOffsetUnits: -8 }));
  mesh.frustumCulled = false; mesh.renderOrder = 20;
  const sh = new THREE.Vector3(), sx = new THREE.Vector3();
  /* The real silhouette (chapter 6, the LM standing): the model itself as the Sun sees it, drawn once into a texture
   * and laid out along the ground; a finer drape, a little higher, so the ground's small bumps do not poke through
   * it (owner's headset test: holes in the shadow). Image geometry: across is horizontal, square to the Sun; up in
   * the image is w = y cos(el) + (distance down-Sun) sin(el), and a point's shadow falls w / sin(el) down-Sun. */
  const W0 = -1.6, W1 = 8.6, HALF = 5.4, SZ = 1024;
  let pix = null, cosEl = 1; const _r = new THREE.Vector3();
  /* 1 in the Sun, 0 in the LM's shadow (a world point; anywhere down-Sun of the LM) */
  function lit(p) {
    if (!baked || !pix) return 1;
    _r.copy(p).sub(baked.R);
    const u = _r.dot(baked.right), along = _r.dot(baked.sh), w = _r.y * cosEl + along * baked.sinEl;
    if (along * cosEl - _r.y * baked.sinEl < -4.5) return 1;                 /* up-Sun of the LM */
    const i = Math.round((u + HALF) / (2 * HALF) * (SZ - 1)), j = Math.round((w - W0) / (W1 - W0) * (SZ - 1));
    if (i < 0 || j < 0 || i >= SZ || j >= SZ) return 1;
    return 1 - pix[(j * SZ + i) * 4 + 1] / 255;
  }
  function bake(renderer, model, sun, opt = {}) {
    const rt = baked ? baked.rt : new THREE.WebGLRenderTarget(SZ, SZ, { samples: 4 });
    const sh = new THREE.Vector3(-sun.x, 0, -sun.z).normalize(), el = Math.asin(sun.y);
    const upP = new THREE.Vector3(sh.x * Math.sin(el), Math.cos(el), sh.z * Math.sin(el));
    model.updateMatrixWorld(true);
    const R = new THREE.Vector3().setFromMatrixPosition(model.matrixWorld); R.y = moon.heightAt(R.x, R.z);
    const cam = new THREE.OrthographicCamera(-HALF, HALF, W1, W0, 1, 200);
    cam.up.copy(upP); cam.position.copy(R).addScaledVector(sun, 80); cam.lookAt(R); cam.updateMatrixWorld(true);
    const right = new THREE.Vector3().setFromMatrixColumn(cam.matrixWorld, 0);
    /* the model alone, in white on black */
    const sc = new THREE.Scene(); sc.overrideMaterial = new THREE.MeshBasicMaterial({ color: 0xffffff });
    if (opt.clip) sc.overrideMaterial.clippingPlanes = opt.clip;
    const parent = model.parent, at = parent ? parent.children.indexOf(model) : -1;
    const prev = renderer.getRenderTarget(), xr = renderer.xr.enabled, cc = new THREE.Color(), ca = renderer.getClearAlpha(); renderer.getClearColor(cc);
    sc.add(model);
    renderer.xr.enabled = false; renderer.setRenderTarget(rt); renderer.setClearColor(0x000000, 1); renderer.clear();
    renderer.render(sc, cam);
    renderer.setRenderTarget(prev); renderer.setClearColor(cc, ca); renderer.xr.enabled = xr;
    /* the silhouette, kept on this side too: lit(point) asks whether the Sun reaches a point (lmp.js, your own suit) */
    if (opt.read !== false) try { pix = pix || new Uint8Array(SZ * SZ * 4); renderer.readRenderTargetPixels(rt, 0, 0, SZ, SZ, pix); cosEl = Math.cos(el); } catch (e) { pix = null; }
    if (parent) { parent.add(model); parent.children.splice(at, 0, parent.children.pop()); }
    sc.overrideMaterial.dispose();
    if (!baked) { NX = opt.nx || 20; NY = opt.ny || 88; geo.dispose(); geo = new THREE.PlaneGeometry(1, 1, NX, NY); mesh.geometry = geo; mesh.material.alphaMap = rt.texture; mesh.material.needsUpdate = true; }
    baked = { rt, R, right, sh, sinEl: Math.sin(el), tanEl: Math.tan(el), alt: -1 };
    lift(0);
  }
  /* laid out on the ground: of the thing where it was drawn (alt 0), or of the same thing alt metres up (the ascent
   * stage after lift-off: a point alt higher throws its shadow alt / tan(elevation) further down-Sun) */
  function lift(alt = 0) {
    if (!baked || Math.abs(alt - baked.alt) < 0.002) return;
    baked.alt = alt;
    const { R, right, sh } = baked, off = alt / Math.max(0.05, baked.tanEl), pos = geo.attributes.position;
    for (let j = 0; j <= NY; j++) for (let i = 0; i <= NX; i++) {
      const u = (i / NX - 0.5) * 2 * HALF, v = (W1 - j / NY * (W1 - W0)) / baked.sinEl + off;
      const x = R.x + right.x * u + sh.x * v, z = R.z + right.z * u + sh.z * v;
      pos.setXYZ(j * (NX + 1) + i, x, moon.heightAt(x, z) + 0.07, z);
    }
    pos.needsUpdate = true; geo.computeBoundingSphere();
  }
  function update(gx, gz, h, sun, opacity = 0.9) {
    if (baked) return;
    const tanEl = Math.max(0.05, sun.y / Math.hypot(sun.x, sun.z));
    sh.set(-sun.x, 0, -sun.z).normalize(); sx.set(-sh.z, 0, sh.x);
    const base = Math.max(0, h) / tanEl, len = 7.0 / tanEl, half = 4.9;
    const pos = geo.attributes.position;
    for (let j = 0; j <= NY; j++) for (let i = 0; i <= NX; i++) {
      const k = j * (NX + 1) + i, u = (i / NX - 0.5) * 2 * half, v = base + (1 - j / NY) * len;
      const x = gx + sh.x * v + sx.x * u, z = gz + sh.z * v + sx.z * u;
      pos.setXYZ(k, x, moon.heightAt(x, z) + 0.05, z);
    }
    pos.needsUpdate = true; geo.computeBoundingSphere();
    mesh.material.opacity = opacity;
  }
  return { mesh, update, bake, lit, lift };
}
