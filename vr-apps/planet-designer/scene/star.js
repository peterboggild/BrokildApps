/* The star: limb darkening, animated granulation, spots for active stars, and a corona with streamers.
 * Colour from the black-body colour of its effective temperature (physics/star.js). HDR values feed the bloom. */
import * as THREE from 'three';
import { NOISE } from './glsl.js?v=12';

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
        #include <tonemapping_fragment>
        #include <colorspace_fragment>
      }`,
  }));

  /* Corona: a shell of 4 star radii. Each pixel finds how close its view ray passes to the star's centre and glows by
   * that distance; the streamers are noise on the direction of that closest point in the star's own frame. So the
   * glow stands still in space and does not turn with the head (a camera-facing disc did, in the headset). */
  const corona = new THREE.Mesh(new THREE.SphereGeometry(4, 48, 24), new THREE.ShaderMaterial({
    uniforms, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, side: THREE.BackSide,
    vertexShader: `varying vec3 vWP; void main() { vWP = (modelMatrix * vec4(position, 1.0)).xyz; gl_Position = projectionMatrix * viewMatrix * vec4(vWP, 1.0); }`,
    fragmentShader: NOISE + `
      varying vec3 vWP; uniform vec3 uColor; uniform float uTime, uActivity, uGain;
      uniform mat4 modelMatrix;
      void main() {
        vec3 c = modelMatrix[3].xyz;
        float R = length(modelMatrix[0].xyz);
        vec3 rd = normalize(vWP - cameraPosition);
        vec3 oc = c - cameraPosition;
        float tp = dot(oc, rd);
        if (tp < 0.0) discard;
        vec3 q = cameraPosition + rd * tp - c;           /* closest approach, from the centre */
        float r = length(q) / R;                        /* in star radii */
        if (r < 1.0 || r > 4.0) discard;
        vec3 dir = normalize(transpose(mat3(modelMatrix)) * q);
        float streak = 0.55 + 0.45 * snoise(dir * 3.0 + vec3(0.0, 0.0, uTime * 0.02));
        streak = streak * streak;
        float glow = pow(max(r - 0.9, 0.02), -1.6) * 0.012;
        float rays = streak * exp(-(r - 1.0) * (2.2 - 0.8 * uActivity)) * 0.1;
        float I = (glow + rays) * smoothstep(4.0, 2.2, r);
        gl_FragColor = vec4(mix(uColor, vec3(1.0), 0.35) * I * uGain * 0.35, 1.0);
        #include <tonemapping_fragment>
        #include <colorspace_fragment>
      }`,
  }));
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
