/* The design object: defaults, presets, validation (hard limits), save/load, JSON export/import, share link.
 * SI-friendly units as named in the keys. Derived values are never stored here. */
import { ECC_MAX } from './physics/orbit.js?v=9';
import { STAR_MASS_MIN, STAR_MASS_MAX } from './physics/star.js?v=9';
import { PLANET_MASS_MIN, PLANET_MASS_MAX } from './physics/structure.js?v=9';

export const APP_ID = 'PlanetDesigner';
export const DESIGN_VERSION = 1;
export const MAX_PLANETS = 8;
export const MAX_HZ = 2;            // owner's hard limit
export const OTHER_TYPES = ['rocky', 'icy', 'iceGiant', 'gasGiant'];

export function earthPlanet(id = 'p1', name = 'Tellus') {
  return {
    id, name, role: 'hz',
    orbit: { aAU: 1.0, e: 0.0167, incDeg: 0 },
    spin: { dayHours: 23.93, obliquityDeg: 23.44 },
    bulk: { massEarth: 1.0, coreMassFraction: 0.325, waterMassFraction: 0.0002, radiogenicRel: 1.0 },
    surface: { oceansEarth: 1.0, landAlbedo: 0.25, terrainSeed: 3 },
    atmosphere: { pN2bar: 0.78, pO2bar: 0.21, pCO2bar: 0.00042, pH2Obar: 'auto', pArbar: 0.0093, pCH4bar: 0.0000019 },
    moons: [{ massMoon: 1.0, aEarthRadii: 60.3 }],
  };
}
export function otherPlanet(id, name, type, aAU, massEarth, e = 0.03) {
  return { id, name, role: 'other', type, orbit: { aAU, e, incDeg: 1 }, bulk: { massEarth } };
}

export function defaultDesign() {
  return {
    app: APP_ID, v: DESIGN_VERSION, name: 'Ny verden',
    star: { massSun: 1.0, ageGyr: 4.57, metallicityFeH: 0.0, activity: 'auto' },
    planets: [earthPlanet(), otherPlanet('g1', 'Bulwark', 'gasGiant', 5.2, 318, 0.048)],
    level: 'explorer',
  };
}

/* Presets: start points for teaching (the challenges of §9 build on them later). */
export const PRESETS = {
  solar: () => defaultDesign(),
  kdwarf: () => {
    const d = defaultDesign(); d.name = 'Second Earth';
    d.star.massSun = 0.75; d.star.ageGyr = 5.5;
    d.planets[0].orbit.aAU = 0.52; d.planets[0].spin.dayHours = 26;
    d.planets[0].moons[0].aEarthRadii = 40;   /* a closer moon: at 60 Earth radii the K dwarf would pull it away */
    d.planets[1].orbit.aAU = 3.5; return d;
  },
  /* Teaching start points. Their numbers are taken from Mars and Venus, but in a DIY system they are new worlds
   * with names of their own (owner: "there is no Mars in a DIY solar system"). */
  dryworld: () => {
    const d = defaultDesign(); d.name = 'Ferrox';
    const p = d.planets[0]; p.name = 'Ferrox';
    Object.assign(p.orbit, { aAU: 1.524, e: 0.0934 });
    Object.assign(p.spin, { dayHours: 24.62, obliquityDeg: 25.19 });
    Object.assign(p.bulk, { massEarth: 0.107, coreMassFraction: 0.25, waterMassFraction: 0 });   /* InSight: core ~1830 km */
    Object.assign(p.surface, { oceansEarth: 0, landAlbedo: 0.25 });
    Object.assign(p.atmosphere, { pN2bar: 0.00017, pO2bar: 0.0000013, pCO2bar: 0.0061, pArbar: 0.00012, pCH4bar: 0 });
    p.moons = []; return d;
  },
  hothouse: () => {
    const d = defaultDesign(); d.name = 'Calida';
    const p = d.planets[0]; p.name = 'Calida';
    Object.assign(p.orbit, { aAU: 0.723, e: 0.0068 });
    Object.assign(p.spin, { dayHours: 5832.5, obliquityDeg: 177.4 });
    Object.assign(p.bulk, { massEarth: 0.815, coreMassFraction: 0.30, waterMassFraction: 0 });
    Object.assign(p.surface, { oceansEarth: 0, landAlbedo: 0.25 });
    Object.assign(p.atmosphere, { pN2bar: 3.2, pO2bar: 0, pCO2bar: 89, pArbar: 0.006, pCH4bar: 0 });
    p.moons = []; return d;
  },
  reddwarf: () => {
    const d = defaultDesign(); d.name = 'Terminator';
    d.star.massSun = 0.3; d.star.ageGyr = 5;
    d.planets[0].orbit.aAU = 0.11; d.planets[0].moons = [];
    d.planets[1].orbit.aAU = 1.2; d.planets[1].bulk.massEarth = 30; d.planets[1].type = 'iceGiant';
    return d;
  },
};

const clamp = (x, lo, hi, def) => (typeof x === 'number' && isFinite(x) ? Math.max(lo, Math.min(hi, x)) : def);

/* Validation: the hard limits only (owner's decision: impossible designs are allowed and flagged red, never
 * blocked; only eccentricity <= 0.4 and at most two HZ planets are hard). Plus the app's slider ranges, so a
 * hand-edited file cannot put NaN or absurd numbers into the physics. Returns a fresh, clean object. */
export function validate(input) {
  const def = defaultDesign();
  const d = input && typeof input === 'object' ? JSON.parse(JSON.stringify(input)) : def;
  const out = { app: APP_ID, v: DESIGN_VERSION, name: String(d.name || def.name).slice(0, 60) };
  const s = d.star || {};
  out.star = {
    massSun: clamp(s.massSun, STAR_MASS_MIN, STAR_MASS_MAX, 1),
    ageGyr: clamp(s.ageGyr, 0.01, 15, 4.57),
    metallicityFeH: clamp(s.metallicityFeH, -1, 0.5, 0),
    activity: ['auto', 'low', 'normal', 'high'].includes(s.activity) ? s.activity : 'auto',
  };
  const planets = Array.isArray(d.planets) ? d.planets.slice(0, MAX_PLANETS) : def.planets;
  let hz = 0;
  out.planets = planets.map((p, i) => {
    const id = String(p.id || 'p' + (i + 1)).slice(0, 12);
    const name = String(p.name || 'Planet ' + (i + 1)).slice(0, 40);
    let role = p.role === 'hz' ? 'hz' : 'other';
    if (role === 'hz' && ++hz > MAX_HZ) role = 'other';
    const o = p.orbit || {};
    const orbit = { aAU: clamp(o.aAU, 0.01, 50, 1), e: clamp(o.e, 0, ECC_MAX, 0), incDeg: clamp(o.incDeg, 0, 10, 0),
      argPeriDeg: clamp(o.argPeriDeg, -360, 360, 0) };
    if (role === 'other') {
      const type = OTHER_TYPES.includes(p.type) ? p.type : 'rocky';
      return { id, name, role, type, orbit, bulk: { massEarth: clamp(p.bulk && p.bulk.massEarth, 0.01, 4000, 1) } };
    }
    const e = earthPlanet(id, name);
    const b = p.bulk || {}, sp = p.spin || {}, su = p.surface || {}, a = p.atmosphere || {};
    return {
      id, name, role, orbit,
      spin: { dayHours: clamp(sp.dayHours, 4, 10000, 24), obliquityDeg: clamp(sp.obliquityDeg, 0, 180, 23.44) },
      bulk: {
        massEarth: clamp(b.massEarth, PLANET_MASS_MIN, PLANET_MASS_MAX, 1),
        coreMassFraction: clamp(b.coreMassFraction, 0, 0.8, 0.325),
        waterMassFraction: clamp(b.waterMassFraction, 0, 0.5, 0.0002),
        radiogenicRel: clamp(b.radiogenicRel, 0.1, 3, 1),
      },
      surface: { oceansEarth: clamp(su.oceansEarth, 0, 50, 1), landAlbedo: clamp(su.landAlbedo, 0.1, 0.4, 0.25),
        terrainSeed: Math.round(clamp(su.terrainSeed, 1, 99999, 3)) },
      atmosphere: {
        pN2bar: clamp(a.pN2bar, 0, 10, e.atmosphere.pN2bar), pO2bar: clamp(a.pO2bar, 0, 2, e.atmosphere.pO2bar),
        pCO2bar: clamp(a.pCO2bar, 0, 100, e.atmosphere.pCO2bar), pH2Obar: 'auto',
        pArbar: clamp(a.pArbar, 0, 1, e.atmosphere.pArbar), pCH4bar: clamp(a.pCH4bar, 0, 0.01, e.atmosphere.pCH4bar),
      },
      moons: (Array.isArray(p.moons) ? p.moons.slice(0, 3) : []).map((m) => ({
        massMoon: clamp(m.massMoon, 0.081, 8.13, 1), aEarthRadii: clamp(m.aEarthRadii, 1.5, 1000, 60),
      })),
    };
  });
  if (!out.planets.length) out.planets = def.planets;
  out.level = ['explorer', 'student', 'scientist'].includes(d.level) ? d.level : 'explorer';
  return out;
}

/* Persistence. localStorage can throw (private windows); everything is wrapped. */
const KEY = 'planetDesigner.design.v1';
export function saveLocal(d) { try { localStorage.setItem(KEY, JSON.stringify(d)); } catch (e) { /* ignore */ } }
export function loadLocal() {
  try { const s = localStorage.getItem(KEY); return s ? validate(JSON.parse(s)) : null; } catch (e) { return null; }
}

/* Share link: the design JSON, deflated (when the browser can) and base64url-encoded into the URL hash. */
const b64u = (bytes) => btoa(String.fromCharCode(...bytes)).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
const unb64u = (s) => Uint8Array.from(atob(s.replace(/-/g, '+').replace(/_/g, '/')), (c) => c.charCodeAt(0));
async function pipe(bytes, stream) {
  return new Uint8Array(await new Response(new Blob([bytes]).stream().pipeThrough(stream)).arrayBuffer());
}
export async function toHash(d) {
  const bytes = new TextEncoder().encode(JSON.stringify(d));
  if (typeof CompressionStream !== 'undefined') return 'z' + b64u(await pipe(bytes, new CompressionStream('deflate-raw')));
  return 'j' + b64u(bytes);
}
export async function fromHash(h) {
  try {
    const kind = h[0], bytes = unb64u(h.slice(1));
    const raw = kind === 'z' ? await pipe(bytes, new DecompressionStream('deflate-raw')) : bytes;
    return validate(JSON.parse(new TextDecoder().decode(raw)));
  } catch (e) { return null; }
}
