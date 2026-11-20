/*
 * Bordbyen — the cartoon look: flat colour in three tones (toon shading) and black ink lines round everything.
 *
 * The ink is an "inverted hull": each mesh is drawn a second time, back faces only, in near-black, pushed outwards a fixed number of pixels in
 * screen space. Where the hull shows past the mesh's own silhouette (its outline, and the edges of every box that stands out from another) it reads
 * as a drawn line of even width, near or far. The push follows a smoothed normal (the average of all faces meeting at a point), so corners close up
 * instead of cracking. The hull shares the mesh's geometry; it only adds that one attribute.
 *
 * Flat parts that lie on others (asphalt, road markings, paths) are left without ink, or the town would be scribbled over.
 * In fog the ink fades to the fog colour like everything else.
 */
import * as THREE from './vendor/three.module.min.js';

const ramp = (() => { const d = new Uint8Array([90, 170, 255]); const t = new THREE.DataTexture(d, 3, 1, THREE.RedFormat); t.minFilter = t.magFilter = THREE.NearestFilter; t.generateMipmaps = false; t.needsUpdate = true; return t; })();
/** a toon material with per-vertex colour */
export function toonMat(extra = {}) { return new THREE.MeshToonMaterial(Object.assign({ vertexColors: true, gradientMap: ramp }, extra)); }

export const INK = { uW: { value: 0.0032 }, uInk: { value: new THREE.Color(0x15131a) }, uFog: { value: new THREE.Color(0xdbe9f3) }, uNear: { value: 1e6 }, uFar: { value: 2e6 } };
const inkMat = new THREE.ShaderMaterial({ side: THREE.BackSide, uniforms: INK,
  vertexShader: `attribute vec3 sn; uniform float uW; varying float vD;
    void main() { vec4 c = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
      vec4 n = projectionMatrix * modelViewMatrix * vec4(position + sn * 0.02, 1.0);
      vec2 d = n.xy / n.w - c.xy / c.w; float l = length(d); if (l > 1e-7) c.xy += d / l * uW * c.w;
      vD = c.w; gl_Position = c; }`,
  fragmentShader: `precision highp float; uniform vec3 uInk; uniform vec3 uFog; uniform float uNear; uniform float uFar; varying float vD;
    void main() { gl_FragColor = vec4(mix(uInk, uFog, smoothstep(uNear, uFar, vD)), 1.0); }` });

/** the smoothed normals a hull is pushed along: faces meeting at one point share the average of their normals */
function smoothNormals(g) {
  if (g.getAttribute('sn')) return;
  const p = g.getAttribute('position'), n = g.getAttribute('normal') || (g.computeVertexNormals(), g.getAttribute('normal')), map = new Map(), keys = new Array(p.count);
  for (let i = 0; i < p.count; i++) {
    const k = Math.round(p.getX(i) * 200) + ',' + Math.round(p.getY(i) * 200) + ',' + Math.round(p.getZ(i) * 200); keys[i] = k;
    let a = map.get(k); if (!a) { a = [0, 0, 0]; map.set(k, a); } a[0] += n.getX(i); a[1] += n.getY(i); a[2] += n.getZ(i);
  }
  const sn = new Float32Array(p.count * 3);
  for (let i = 0; i < p.count; i++) { const a = map.get(keys[i]), l = Math.hypot(a[0], a[1], a[2]) || 1; sn[i * 3] = a[0] / l; sn[i * 3 + 1] = a[1] / l; sn[i * 3 + 2] = a[2] / l; }
  g.setAttribute('sn', new THREE.BufferAttribute(sn, 3));
}
/** give a mesh its ink: a hull drawn just before it, sharing its geometry */
export function ink(mesh) {
  smoothNormals(mesh.geometry);
  const hull = new THREE.Mesh(mesh.geometry, inkMat); hull.renderOrder = mesh.renderOrder; hull.frustumCulled = mesh.frustumCulled; hull.name = 'ink';
  mesh.add(hull); return mesh;
}
