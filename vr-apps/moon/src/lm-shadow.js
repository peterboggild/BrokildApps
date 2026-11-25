/* The LM's shadow on the ground: its silhouette as the low Sun sees it (legs, descent stage, cabin, tunnel), laid
 * along the ground away from the Sun and draped over the terrain each frame. Stage 0's terrain shader casts no
 * shadows of things standing on it; this is the one that matters (the descending crews used it as their last
 * altitude cue, and it is in every photograph of an LM on the Moon).
 * update(x, z, h, sun): the point under the LM, the height of its footpads over the ground there, the Sun (unit). */
import * as THREE from 'three';

export function makeLMShadow(moon) {
  const tex = (() => {
    const c = document.createElement('canvas'); c.width = 256; c.height = 256; const x = c.getContext('2d');
    x.clearRect(0, 0, 256, 256); x.fillStyle = '#fff'; x.strokeStyle = '#fff';
    const X = u => 128 + u * 26, Y = v => 256 - v * 34;                    /* metres: u across (-4.7..4.7), v up (0..7) */
    x.fillRect(X(-2.1), Y(2.45), X(2.1) - X(-2.1), Y(0.8) - Y(2.45));       /* descent stage */
    x.beginPath(); x.moveTo(X(-1.25), Y(2.45)); x.lineTo(X(-1.4), Y(5.2)); x.lineTo(X(-0.6), Y(5.9)); x.lineTo(X(0.6), Y(5.9)); x.lineTo(X(1.4), Y(5.2)); x.lineTo(X(1.25), Y(2.45)); x.fill();
    x.fillRect(X(-0.4), Y(6.3), X(0.4) - X(-0.4), Y(5.85) - Y(6.3));        /* tunnel */
    x.lineWidth = 6; for (const s of [-1, 1]) { x.beginPath(); x.moveTo(X(s * 2.0), Y(2.3)); x.lineTo(X(s * 4.15), Y(0.15)); x.stroke(); x.beginPath(); x.moveTo(X(s * 1.6), Y(0.9)); x.lineTo(X(s * 3.2), Y(1.1)); x.stroke(); }
    x.lineWidth = 4; x.beginPath(); x.moveTo(X(0), Y(0.8)); x.lineTo(X(0), Y(0.15)); x.stroke();
    for (const u of [-4.15, 4.15, 0]) { x.beginPath(); x.ellipse(X(u), Y(0.1), 12, 5, 0, 0, Math.PI * 2); x.fill(); }
    return new THREE.CanvasTexture(c);
  })();
  const NX = 8, NY = 24;
  const geo = new THREE.PlaneGeometry(1, 1, NX, NY);
  const mesh = new THREE.Mesh(geo, new THREE.MeshBasicMaterial({ color: 0x000000, alphaMap: tex, transparent: true, opacity: 0.9, depthWrite: false,
    polygonOffset: true, polygonOffsetFactor: -4, polygonOffsetUnits: -8 }));
  mesh.frustumCulled = false; mesh.renderOrder = 20;
  const sh = new THREE.Vector3(), sx = new THREE.Vector3();
  function update(gx, gz, h, sun, opacity = 0.9) {
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
  return { mesh, update };
}
