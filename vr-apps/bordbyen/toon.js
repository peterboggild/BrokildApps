/*
 * Ræs i Bordbyen — the cartoon look: flat colour in three tones (toon shading) and black ink lines round everything.
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
/* The dark graphic-novel style (an option): every toon material is darkened, drained of colour and tinted cold; where it is in shadow it is
 * cross-hatched with pen lines (two layers, the second only in the darkest tone). The lines are laid in the city's own metres on whichever plane the
 * surface faces most (walls up and across, ground flat), so they stay put on the buildings in both eyes and in both scales, and fade out where they
 * would be finer than a pixel. Street lamps glow and some windows are lit, picked by a hash of where they are. */
export const NOIR = { uNoir: { value: 0 }, uCityInv: { value: new THREE.Matrix4() }, uTime: { value: 0 } };
const NOIR_VS_HEAD = 'uniform mat4 uCityInv; varying vec3 vNP; varying vec3 vNN;\n';
const NOIR_VS_BODY = '\n  { vec4 nw = modelMatrix * vec4(transformed, 1.0); vNP = (uCityInv * nw).xyz; vNN = normalize(mat3(uCityInv) * mat3(modelMatrix) * objectNormal); }\n';
const NOIR_FS_HEAD = `uniform float uNoir; varying vec3 vNP; varying vec3 vNN;
vec3 noirize(vec3 col) {
  if (uNoir <= 0.0) return col;
  vec3 base = vec3(1.0);
#ifdef USE_COLOR
  base = vColor.rgb;
#endif
  float lum = dot(col, vec3(0.299, 0.587, 0.114));
  vec3 dark = mix(vec3(lum), col, 0.3) * vec3(0.52, 0.58, 0.74) * 0.8;
  vec3 an = abs(normalize(vNN)); vec2 uv = an.y > 0.6 ? vNP.xz : (an.x > an.z ? vNP.zy : vNP.xy);
  float s1 = (uv.x + uv.y) * 3.0, s2 = (uv.x - uv.y) * 3.0, aa = max(fwidth(s1), 1e-4);
  float fade = 1.0 - smoothstep(0.3, 0.7, aa);
  float l1 = 1.0 - smoothstep(0.16 - aa, 0.16 + aa, abs(fract(s1) - 0.5) * 2.0), l2 = 1.0 - smoothstep(0.16 - aa, 0.16 + aa, abs(fract(s2) - 0.5) * 2.0);
  float hatch = max(l1 * (1.0 - smoothstep(0.38, 0.55, lum)), l2 * (1.0 - smoothstep(0.18, 0.3, lum))) * fade;
  dark *= 1.0 - 0.6 * hatch;
  float lamp = step(0.93, base.r) * step(0.88, base.g) * step(base.b, 0.8);
  float win = step(base.r + 0.18, base.b) * step(0.55, base.b) * step(an.y, 0.5);
  vec2 cell = floor(vNP.xz * 0.45) + floor(vNP.y / 3.1) * 17.0;
  float lit = win * step(0.58, fract(sin(dot(cell, vec2(12.9898, 78.233))) * 43758.5453));
  dark = mix(dark, vec3(1.0, 0.8, 0.42) * 1.05, lit);
  dark = mix(dark, vec3(1.0, 0.92, 0.6) * 1.5, lamp);
  return mix(col, dark, uNoir);
}
`;
function noirPatch(sh) {
  Object.assign(sh.uniforms, NOIR);
  sh.vertexShader = NOIR_VS_HEAD + sh.vertexShader.replace('#include <project_vertex>', '#include <project_vertex>' + NOIR_VS_BODY);
  sh.fragmentShader = sh.fragmentShader.replace('#include <color_pars_fragment>', '#include <color_pars_fragment>\n' + NOIR_FS_HEAD).replace('#include <opaque_fragment>', 'outgoingLight = noirize(outgoingLight);\n#include <opaque_fragment>');
}
/** a toon material with per-vertex colour (and the graphic-novel style built in, off unless chosen) */
export function toonMat(extra = {}) { const m = new THREE.MeshToonMaterial(Object.assign({ vertexColors: true, gradientMap: ramp }, extra)); m.onBeforeCompile = noirPatch; m.customProgramCacheKey = () => 'noir1'; return m; }

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
