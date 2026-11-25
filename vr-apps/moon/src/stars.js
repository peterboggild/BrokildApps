/* The stars: the Yale Bright Star Catalogue to magnitude 6.5 (tools/stars.py, assets/stars.bin), at infinity.
 *
 * Placed by two directions every page knows: the Sun's, and a pole (the Earth's axis in the launch's pad frame, the
 * Moon's in stage 0's site frame, which is within 1.5 degrees of the ecliptic's). The Sun stands where it stood
 * among the stars in December 1972 (Ophiuchus, by Sagittarius), so the sky behind it is the true one.
 *
 * The setting (start pages, remembered): "faint" (the default: always there, dim), "full", "realistic" (only when
 * you are in shadow and your eyes have opened: the page says how dark it is), "off". */
import * as THREE from 'three';

const D = Math.PI / 180;
const radec = (ra, dec) => new THREE.Vector3(Math.cos(dec * D) * Math.cos(ra * D), Math.cos(dec * D) * Math.sin(ra * D), Math.sin(dec * D));
export const SUN_J2000 = radec(258.3, -23.0);                  /* the Sun on 11 December 1972 */
export const CELESTIAL_POLE = new THREE.Vector3(0, 0, 1);
export const ECLIPTIC_POLE = radec(270, 66.56);
const LEVELS = { off: 0, faint: 0.5, full: 1, realistic: 1 };

export function starMode() {
  const q = new URLSearchParams(location.search).get('stars');
  if (q && q in LEVELS) return q;
  try { const s = localStorage.getItem('moon-stars'); if (s && s in LEVELS) return s; } catch (e) { /* none */ }
  return 'faint';
}
/* a start page's <select id="stars">: shows and remembers the setting */
export function bindStarsSelect(onChange) {
  const el = document.getElementById('stars'); if (!el) return;
  el.value = starMode();
  el.addEventListener('change', () => { try { localStorage.setItem('moon-stars', el.value); } catch (e) { /* private window */ } if (onChange) onChange(el.value); });
}

export function makeStars(poleJ2000 = ECLIPTIC_POLE) {
  const U = { uRot: { value: new THREE.Matrix3() }, uLevel: { value: 0 }, uScale: { value: 1 } };
  const geo = new THREE.BufferGeometry();
  const mat = new THREE.ShaderMaterial({
    uniforms: U, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending,
    vertexShader: /* glsl */`
      uniform mat3 uRot; uniform float uLevel; uniform float uScale;
      attribute float aMag; attribute float aBV; varying vec3 vC; varying float vA;
      void main() {
        vec3 d = uRot * position;
        vec4 c = projectionMatrix * vec4(mat3(viewMatrix) * d, 0.0);
        c.z = c.w * 0.999999;                                   /* at infinity, behind everything */
        gl_Position = c;
        float b = sqrt(clamp(pow(10.0, -0.4 * (aMag - 1.0)), 0.0, 1.0));   /* relative to magnitude 1, as the eye sees it */
        gl_PointSize = (1.5 + 2.5 * b) * uScale;
        vA = (0.12 + 0.88 * b) * uLevel;
        float t = clamp((aBV + 0.3) / 2.0, 0.0, 1.0);            /* blue-white to orange */
        vC = mix(vec3(0.72, 0.82, 1.0), vec3(1.0, 0.78, 0.55), t) + vec3(0.25) * (1.0 - abs(t - 0.4));
      }`,
    fragmentShader: /* glsl */`
      varying vec3 vC; varying float vA;
      void main() { vec2 p = gl_PointCoord - 0.5; float r = dot(p, p) * 4.0; float a = vA * clamp(1.0 - r, 0.0, 1.0);
        if (a < 0.003) discard; gl_FragColor = vec4(vC * a, a); }`
  });
  const points = new THREE.Points(geo, mat);
  points.frustumCulled = false; points.renderOrder = -5;
  fetch(new URL('../assets/stars.bin', import.meta.url)).then(r => r.arrayBuffer()).then(buf => {
    const f = new Float32Array(buf), n = f.length / 5, pos = new Float32Array(n * 3), mag = new Float32Array(n), bv = new Float32Array(n);
    for (let i = 0; i < n; i++) { pos[i * 3] = f[i * 5]; pos[i * 3 + 1] = f[i * 5 + 1]; pos[i * 3 + 2] = f[i * 5 + 2]; mag[i] = f[i * 5 + 3]; bv[i] = f[i * 5 + 4]; }
    geo.setAttribute('position', new THREE.BufferAttribute(pos, 3));
    geo.setAttribute('aMag', new THREE.BufferAttribute(mag, 1));
    geo.setAttribute('aBV', new THREE.BufferAttribute(bv, 1));
  }).catch(e => console.warn('stars: ' + e.message));

  const mode = starMode();
  const a = new THREE.Vector3(), b = new THREE.Vector3(), c = new THREE.Vector3(), mj = new THREE.Matrix3(), mw = new THREE.Matrix3();
  const basis = (s, p, m) => { a.copy(s).normalize(); b.copy(p).addScaledVector(a, -p.dot(a)).normalize(); c.crossVectors(a, b); return m.set(a.x, b.x, c.x, a.y, b.y, c.y, a.z, b.z, c.z); };
  basis(SUN_J2000, poleJ2000, mj); mj.transpose();
  let dark = 0, fade = 1;
  const apply = () => { U.uLevel.value = (mode === 'realistic' ? dark : LEVELS[mode]) * fade; points.visible = U.uLevel.value > 0.002; };
  apply();
  return {
    points, mode,
    /* the Sun and the pole in this page's world */
    orient(sunWorld, poleWorld) { basis(sunWorld, poleWorld, mw); U.uRot.value.multiplyMatrices(mw, mj); },
    /* how dark it is around you (realistic mode): 0 sunlit, 1 in shadow with your eyes opened */
    setDark(k) { dark = k; apply(); },
    /* the sky's own light (in the atmosphere) washing them out: 1 none */
    setFade(k) { fade = k; apply(); },
    setScale(k) { U.uScale.value = k; }
  };
}

/* the start page's setting, wherever stars are drawn (a change reloads: nothing has started on the start page) */
if (typeof document !== 'undefined') bindStarsSelect(() => location.reload());
