/* Settings you can change without leaving the experience: on the wrist panel's second page (left stick press, twice), the left
 * stick up and down to choose, left and right to change. Kept in the browser (localStorage), so they follow you from
 * chapter to chapter. Each chapter lists the ones that mean something there and applies them (applyCommon does the
 * frame rate, foveation and volumes). Resolution is not here: a WebXR session cannot change it while running, so it
 * stays on the start page. */

export const DEFS = {
  ff: { label: 'Fast forward', options: [['events', 'stops at events and voices'], ['major', 'stops at major events'], ['free', 'never stops']], def: 'events' },
  turn: { label: 'Turning (comfort)', options: [[0.75, 'gentle, 0.75 deg/s'], [1.5, 'normal, 1.5 deg/s'], [3, 'quick, 3 deg/s']], def: 1.5 },
  hz: { label: 'Frame rate', options: [[0, 'headset default'], [72, '72 Hz'], [90, '90 Hz'], [120, '120 Hz']], def: 0 },
  fov: { label: 'Foveation', options: [[0, 'off (sharpest)'], [0.5, 'medium'], [1, 'high (fastest)']], def: 0.5 },
  voices: { label: 'Radio voices', options: [[0, 'off'], [0.5, 'low'], [1, 'normal']], def: 1 },
  sound: { label: 'Engines and cabin', options: [[0, 'off'], [0.5, 'low'], [1, 'normal']], def: 1 },
  hand: { label: 'Hand angle', options: [[0, '0 deg (the bare grip)'], [10, '+10 deg'], [17, '+17 deg'], [25, '+25 deg'], [32, '+32 deg']], def: 17 },
  walk: { label: 'Moon walk (comfort)', options: [[1, 'hopping steps'], [0, 'level']], def: 1 },
  body: { label: 'Your own body', options: [[true, 'shown'], [false, 'hidden']], def: true },
  text: { label: 'Messages on screen', options: [[0, 'off (only what you must know)'], [1, 'on: hints and radio text']], def: 0 }
};
const KEY = 'moon-settings';
const val = {};
try { Object.assign(val, JSON.parse(localStorage.getItem(KEY) || '{}')); } catch (e) { /* private window: defaults */ }
for (const k in DEFS) if (!DEFS[k].options.some(([v]) => v === val[k])) val[k] = DEFS[k].def;
const listeners = [];

export const settings = {
  get: k => val[k],
  set(k, v) {
    val[k] = v;
    try { localStorage.setItem(KEY, JSON.stringify(val)); } catch (e) { /* not kept */ }
    listeners.forEach(f => f(k, v));
  },
  /* the next or previous option (dir +1 / -1) */
  step(k, dir) { const o = DEFS[k].options, i = o.findIndex(([v]) => v === val[k]); settings.set(k, o[(i + dir + o.length) % o.length][0]); },
  text(k) { const o = DEFS[k].options.find(([v]) => v === val[k]); return o ? o[1] : ''; },
  onChange(f) { listeners.push(f); }
};

/* the ones every chapter applies the same way: frame rate and foveation in VR, the two volumes */
export function applyCommon(renderer, sound) {
  const apply = k => {
    const s = renderer.xr.getSession && renderer.xr.getSession();
    if (k === 'hz' && s && val.hz && s.updateTargetFrameRate && (!s.supportedFrameRates || [...s.supportedFrameRates].includes(val.hz))) s.updateTargetFrameRate(val.hz).catch(() => {});
    if (k === 'fov' && s) renderer.xr.setFoveation(val.fov);
    if ((k === 'voices' || k === 'sound') && sound && sound.setLevels) sound.setLevels({ voices: val.voices, sound: val.sound });
  };
  settings.onChange(apply);
  return { all() { for (const k of ['hz', 'fov', 'voices']) apply(k); } };
}

/* The headset's own "reset view" puts its origin under you: the pose you are given jumps, and a seat that was set
 * from the old origin leaves you somewhere else (outside the spacecraft: owner's headset test). Call the returned
 * function once a frame in VR: it is true when that has just happened (the reference space says so, or the pose
 * jumps further than a head moves in one frame); the chapter then seats you again where you belong. */
export function watchRecentre(renderer, camera) {
  let bound = null, flag = false, have = false, lx = 0, ly = 0, lz = 0, lyaw = 0;
  return () => {
    const rs = renderer.xr.getReferenceSpace && renderer.xr.getReferenceSpace();
    if (rs && rs !== bound && rs.addEventListener) { bound = rs; rs.addEventListener('reset', () => { flag = true; }); }
    const p = camera.position, q = camera.quaternion, yaw = Math.atan2(2 * (q.w * q.y + q.x * q.z), 1 - 2 * (q.x * q.x + q.y * q.y));
    let hit = flag; flag = false;
    if (have) {
      const d = Math.hypot(p.x - lx, p.y - ly, p.z - lz), dy = Math.abs(Math.atan2(Math.sin(yaw - lyaw), Math.cos(yaw - lyaw)));
      if (d > 0.4 || dy > 1.0) hit = true;
    }
    lx = p.x; ly = p.y; lz = p.z; lyaw = yaw; have = p.lengthSq() > 0;
    return hit;
  };
}

/* the stretches the clock will not fast-forward across, by the fast-forward setting: every event, the major ones, or
 * none. Each is [from, to, name] (GET); inside one the clock runs at real time (or at most x10 where a chapter says so) */
export function holdsFor(all, major) { const m = val.ff; return m === 'free' ? [] : m === 'major' ? major : all; }
