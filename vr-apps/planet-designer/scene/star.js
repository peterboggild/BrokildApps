/* The star: limb darkening, animated granulation, spots for active stars, and a corona with streamers.
 * Colour from the black-body colour of its effective temperature (physics/star.js). HDR values feed the bloom. */
import * as THREE from 'three';
import { NOISE } from './glsl.js?v=4';

export function createStar() {
  const uniforms = {
    uColor: { value: new THREE.Color(1, 1, 1) }, uTime: { value: 0 }, uActivity: { value: 0.3 },
    uGain: { value: 6.0 }, uLimb: { value: 0.6 },
  };
  const surface = new THREE.Mesh(new THREE.SphereGeometry(1, 96, 64), new THREE.ShaderMaterial({
    uniforms,
    vertexShader: `varying vec3 vN; varying vec3 vP; varying vec3 vV;
      void main() { vP = position; vN = normalize(normalMatrix * normal);
        vec4 mv = modelViewMatrix * vec4(position, 1.0); vV = normalize(-mv.xyz); gl_Position = projectionMatrix * mv; }`,
    fragmentShader: NOISE + `
      varying vec3 vN; varying vec3 vP; varying vec3 vV;
      uniform vec3 uColor; uniform float uTime, uActivity, uGain, uLimb;
      void main() {
        float mu = clamp(dot(normalize(vN), normalize(vV)), 0.0, 1.0);
        /* quadratic limb darkening, roughly solar (Neckel & Labs 1994 fit simplified) */
        float limb = 1.0 - uLimb * (1.0 - mu) - 0.2 * (1.0 - mu) * (1.0 - mu);
        vec3 p = normalize(vP);
        /* granulation: bright cells with dark lanes, two scales, slowly boiling */
        float g1 = snoise(p * 38.0 + vec3(0.0, uTime * 0.05, 0.0));
        float g2 = snoise(p * 95.0 + vec3(uTime * 0.09, 0.0, 0.0));
        float gran = 0.88 + 0.08 * g1 + 0.05 * g2;
        /* spots in the activity belts (|latitude| < 35 deg) for active stars */
        float belt = smoothstep(0.62, 0.2, abs(p.y));
        float s = snoise(p * 7.0 + vec3(uTime * 0.004, 2.0, 0.0));
        float spot = smoothstep(0.78 - 0.25 * uActivity, 0.95, s) * belt * uActivity;
        float fac = smoothstep(0.55, 0.8, s) * belt * uActivity * 0.15;   /* bright faculae around spots, near the limb */
        float I = limb * gran * (1.0 - 0.75 * spot) + fac * (1.0 - mu);
        gl_FragColor = vec4(uColor * I * uGain, 1.0);
      }`,
  }));

  /* Corona: a camera-facing disc with radial streamers, additive */
  const corona = new THREE.Mesh(new THREE.PlaneGeometry(1, 1), new THREE.ShaderMaterial({
    uniforms, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending,
    vertexShader: `varying vec2 vUv; void main() { vUv = uv * 2.0 - 1.0;
      vec4 mv = modelViewMatrix * vec4(0.0, 0.0, 0.0, 1.0);
      float s = length(modelMatrix[0].xyz);
      mv.xy += position.xy * s;
      gl_Position = projectionMatrix * mv; }`,
    fragmentShader: NOISE + `
      varying vec2 vUv; uniform vec3 uColor; uniform float uTime, uActivity, uGain;
      void main() {
        float r = length(vUv) * 4.0;          /* in star radii: the plane spans 4 radii each way */
        if (r < 0.98) discard;
        float a = atan(vUv.y, vUv.x);
        float streak = 0.55 + 0.45 * snoise(vec3(cos(a) * 3.0, sin(a) * 3.0, uTime * 0.02));
        streak = pow(streak, 2.0);
        float glow = pow(max(r - 0.9, 0.02), -1.6) * 0.012;
        float rays = streak * exp(-(r - 1.0) * (2.2 - 0.8 * uActivity)) * 0.1;
        float I = (glow + rays) * smoothstep(4.0, 2.2, r);
        gl_FragColor = vec4(mix(uColor, vec3(1.0), 0.35) * I * uGain * 0.35, 1.0);
      }`,
  }));
  corona.scale.setScalar(8);           /* 4 star radii each way */
  corona.renderOrder = 2;

  const group = new THREE.Group();
  group.add(surface, corona);
  return {
    group, uniforms,
    set({ rgb, activity }) {
      uniforms.uColor.value.setRGB(rgb[0], rgb[1], rgb[2]);
      uniforms.uActivity.value = activity === 'high' ? 1 : activity === 'normal' ? 0.5 : 0.2;
    },
    tick(t) { uniforms.uTime.value = t; },
  };
}
