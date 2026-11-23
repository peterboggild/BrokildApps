/* The sky behind everything: a few thousand stars with real-looking colours and brightness spread, and a faint
 * Milky Way band. Drawn on a sphere that follows the camera, without depth. */
import * as THREE from 'three';
import { snoise } from '../physics/noise.js?v=16';
import { blackbodyRGB } from '../physics/star.js?v=16';

function rng(seed) { let s = seed >>> 0; return () => { s = (s * 1664525 + 1013904223) >>> 0; return s / 4294967296; }; }

export function createSky() {
  const group = new THREE.Group();
  group.renderOrder = -10;

  /* Milky Way: a band along a great circle, with dust lanes, on an inside-out sphere. The noise is evaluated once per
   * vertex in JavaScript (a dense sphere) and interpolated, not per pixel: the sky covers every pixel of both eyes in
   * the headset, and per-pixel noise there was a needless cost. */
  const mwGeo = new THREE.SphereGeometry(1, 160, 80);
  const P = mwGeo.attributes.position, C = new Float32Array(P.count * 3);
  const pole = new THREE.Vector3(0.35, 0.85, -0.4).normalize(), coreDir = new THREE.Vector3(-0.6, 0.12, 0.79).normalize();
  const d = new THREE.Vector3();
  for (let i = 0; i < P.count; i++) {
    d.fromBufferAttribute(P, i).normalize();
    const lat = d.dot(pole);
    const band = Math.exp(-lat * lat * 28);
    const core = Math.exp(-(d.distanceTo(coreDir) ** 2) * 3);
    const n = 0.5 + 0.5 * snoise(d.x * 3, d.y * 3, d.z * 3) * 0.6 + 0.25 * snoise(d.x * 9, d.y * 9, d.z * 9);
    const sm = (a, b, x) => { const t = Math.min(1, Math.max(0, (x - a) / (b - a))); return t * t * (3 - 2 * t); };
    const dust = sm(0.15, 0.65, 0.5 + 0.5 * snoise(d.x * 6 + 3.1, d.y * 6 + 3.1, d.z * 6 + 3.1)) * Math.exp(-lat * lat * 140);
    const I = Math.max(0, band * (0.5 + 0.8 * core) * n * (1 - 0.75 * dust)) * 0.05;
    C[i * 3] = (0.55 + 0.45 * core) * I; C[i * 3 + 1] = (0.6 + 0.26 * core) * I; C[i * 3 + 2] = (0.75 - 0.07 * core) * I;
  }
  mwGeo.setAttribute('color', new THREE.BufferAttribute(C, 3));
  const mw = new THREE.Mesh(mwGeo, new THREE.MeshBasicMaterial({ vertexColors: true, side: THREE.BackSide, depthWrite: false }));
  mw.renderOrder = -10;
  group.add(mw);

  /* Stars: power-law brightness, colours from black bodies 3000-12000 K */
  const N = 5000, r = rng(11);
  const pos = new Float32Array(N * 3), col = new Float32Array(N * 3), size = new Float32Array(N);
  for (let i = 0; i < N; i++) {
    const u = r() * 2 - 1, th = r() * Math.PI * 2, s = Math.sqrt(1 - u * u);
    pos.set([s * Math.cos(th), u, s * Math.sin(th)], i * 3);
    const T = 3000 + 9000 * r() ** 2.2;
    const c = blackbodyRGB(T);
    const mag = r() ** 6;
    const b = 0.25 + 3.0 * mag;
    col.set([c[0] * b, c[1] * b, c[2] * b], i * 3);
    size[i] = 1.0 + 2.2 * mag;
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  g.setAttribute('color', new THREE.BufferAttribute(col, 3));
  g.setAttribute('size', new THREE.BufferAttribute(size, 1));
  const stars = new THREE.Points(g, new THREE.ShaderMaterial({
    depthWrite: false, depthTest: true, transparent: true, blending: THREE.AdditiveBlending,
    uniforms: { uPx: { value: 1 } },
    vertexShader: `attribute vec3 color; attribute float size; varying vec3 vC; uniform float uPx;
      void main() { vC = color; gl_PointSize = size * uPx; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }`,
    fragmentShader: `varying vec3 vC;
      void main() { vec2 d = gl_PointCoord - 0.5; float a = exp(-dot(d, d) * 18.0); gl_FragColor = vec4(vC * a, a); 
#include <tonemapping_fragment>
#include <colorspace_fragment>
}`,
  }));
  stars.renderOrder = -9;
  stars.frustumCulled = false;
  group.add(stars);

  return {
    group,
    /* keep the sky centred on the camera, inside the far plane */
    follow(camera, pixelRatio) {
      group.position.copy(camera.position);
      const R = camera.far * 0.95;
      group.scale.setScalar(R);
      stars.material.uniforms.uPx.value = pixelRatio;
    },
  };
}
