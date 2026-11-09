/* VR mode: flying, scaling and twisting the fractal with the controllers, a quick-change panel
   (fractal type, look, play/hold, sound recipes) drawn inside the headset view, and a frame-rate
   guard that drops the resolution the moment the headset struggles and offers faster fractals.

   Rig: world = O + S * R * room. R = [c0, c1, c2] (columns, an orthonormal basis), S = world units
   per metre. The renderer turns the head pose into an eye ray through the rig (renderXR opts.rig).
   "Ride" = the rig follows the scene camera every frame (as before); any stick, grip or pinch
   switches to "fly", where the rig is ours. B goes back to the ride.

   Controls (Quest, xr-standard gamepad): left stick = fly forward/back + sideways, right stick =
   turn left/right (x) and rise/sink (y); one trigger held for a moment = that hand shapes two of
   the fractal's parameters (up/down and right/left); both triggers held = scale (pull apart = bigger) and drag;
   a grip held = the fractal turns with that hand; trigger click = press a panel tile, or (pointing
   at the fractal) open the sound palette; A = play/pause, B = back to the scene camera, Y = panel,
   X = sound palette. */
(function () {
  'use strict';
  const FV = (window.FV = window.FV || {});

  // ---------- vector / 3x3 maths (column vectors, R = [c0, c1, c2]) ----------
  const add = (a, b) => [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
  const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
  const mul = (a, k) => [a[0] * k, a[1] * k, a[2] * k];
  const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
  const len = (a) => Math.hypot(a[0], a[1], a[2]);
  const norm = (a) => { const l = len(a) || 1; return [a[0] / l, a[1] / l, a[2] / l]; };
  const clamp = (v, a, b) => Math.min(b, Math.max(a, v));
  const mv = (R, v) => add(add(mul(R[0], v[0]), mul(R[1], v[1])), mul(R[2], v[2]));
  const mm = (A, B) => [mv(A, B[0]), mv(A, B[1]), mv(A, B[2])];
  const mT = (A) => [[A[0][0], A[1][0], A[2][0]], [A[0][1], A[1][1], A[2][1]], [A[0][2], A[1][2], A[2][2]]];
  const fromXR = (m) => [[m[0], m[1], m[2]], [m[4], m[5], m[6]], [m[8], m[9], m[10]]];
  const posXR = (m) => [m[12], m[13], m[14]];
  function axisAngle(axis, a) {
    const [x, y, z] = norm(axis);
    const c = Math.cos(a), s = Math.sin(a), t = 1 - c;
    return [[t * x * x + c, t * x * y + s * z, t * x * z - s * y], [t * x * y - s * z, t * y * y + c, t * y * z + s * x], [t * x * z + s * y, t * y * z - s * x, t * z * z + c]];
  }
  function orthonormal(R) {
    const c0 = norm(R[0]);
    const c1 = norm(sub(R[1], mul(c0, dot(c0, R[1]))));
    return [c0, c1, cross(c0, c1)];
  }

  const Rig = {
    // the scene camera as a level room: heading only (as Live VR's "Keep the room level")
    fromCamera(g, scale) {
      let fh = [g.camFwd[0], 0, g.camFwd[2]];
      if (Math.hypot(fh[0], fh[2]) < 1e-3) fh = [g.camUp[0], 0, g.camUp[2]];
      fh = norm(fh);
      return { O: g.camPos.slice(), R: [[-fh[2], 0, fh[0]], [0, 1, 0], mul(fh, -1)], S: scale };
    },
    // the scene camera with its pitch and roll (the room is not kept level)
    fromCameraFull(g, scale) {
      return { O: g.camPos.slice(), R: [g.camRight.slice(), g.camUp.slice(), mul(g.camFwd, -1)], S: scale };
    },
    toWorld: (rig, p) => add(rig.O, mul(mv(rig.R, p), rig.S)),
    // move by a room-space displacement (metres)
    fly(rig, d) { return { O: add(rig.O, mul(mv(rig.R, d), rig.S)), R: rig.R, S: rig.S }; },
    // turn about the room's up axis through the head (which stays where it is)
    turn(rig, head, a) {
      const hw = Rig.toWorld(rig, head);
      const R = orthonormal(mm(rig.R, axisAngle([0, 1, 0], a)));
      return { O: sub(hw, mul(mv(R, head), rig.S)), R, S: rig.S };
    },
    // two hands: start = { rig, d, mid } at the moment both triggers went down
    pinchStart(rig, a, b) {
      const mid = mul(add(a, b), 0.5);
      return { rig, d: Math.max(0.02, len(sub(a, b))), mw: Rig.toWorld(rig, mid) };
    },
    pinch(st, a, b, minS, maxS) {
      const mid = mul(add(a, b), 0.5);
      const S = clamp(st.rig.S * st.d / Math.max(0.02, len(sub(a, b))), minS, maxS);
      return { O: sub(st.mw, mul(mv(st.rig.R, mid), S)), R: st.rig.R, S };
    },
    // one hand: the fractal turns as the hand turns, about a world pivot (the scene's target)
    twistStart(rig, handR, pivot) {
      return { rig, h0: handR, pivot, pr: mul(mv(mT(rig.R), sub(pivot, rig.O)), 1 / rig.S) };
    },
    twist(st, handR) {
      const D = mm(handR, mT(st.h0));
      const R = orthonormal(mm(st.rig.R, mT(D)));
      return { O: sub(st.pivot, mul(mv(R, st.pr), st.rig.S)), R, S: st.rig.S };
    },
    // the music's camera push (cam.dist routes) as a scale about the pivot while flying
    pulse(rig, pivot, k) { return { O: add(pivot, mul(sub(rig.O, pivot), k)), R: rig.R, S: rig.S * k }; }
  };

  // ---------- frame-rate guard ----------
  // Internal resolution (share of the XR layer) follows the frame time: a hard drop at once when frames
  // are very late, a gentle step down when they are a bit late, and a slow climb back. "heavy" fires when
  // even the lowest resolution cannot hold a usable rate: that is when a faster fractal is offered.
  class Perf {
    constructor(hz, opts) {
      const o = opts || {};
      this.target = 1000 / (hz || 72);
      this.min = o.min || 0.3;
      this.max = o.max || 1;
      this.res = clamp(o.start || 0.75, this.min, this.max);
      this.ema = this.target;
      this.late = 0;
      this.tCheck = 0;
      this.tUp = 0;
      this.tHeavy = 0;
      this.heavy = false;
      this.t = 0;
      this.dropCool = 0;
    }
    setHz(hz) { this.target = 1000 / hz; }
    // dt in ms since the previous XR frame. Returns true when the resolution changed.
    frame(dt) {
      if (!(dt > 0) || dt > 2000) return false; // first frame, or the session was paused
      this.t += dt;
      this.ema += (dt - this.ema) * 0.15;
      const r0 = this.res;
      this.late = dt > 2.4 * this.target ? this.late + 1 : 0;
      if (this.late >= 2 && this.t > this.dropCool) {
        this.res = Math.max(this.min, this.res * 0.6);
        this.dropCool = this.t + 200;
        this.late = 0;
        this.ema = this.target * 1.3;
      } else if (this.t - this.tCheck > 400) {
        this.tCheck = this.t;
        if (this.ema > 1.3 * this.target) this.res = Math.max(this.min, this.res * 0.85);
        else if (this.ema < 1.1 * this.target && this.t - this.tUp > 1500) { this.res = Math.min(this.max, this.res * 1.07); this.tUp = this.t; }
      }
      const atFloor = this.res <= this.min * 1.001;
      // at the floor, every frame slower than ~42 fps (at 72 Hz) counts; one second of them = heavy
      if (atFloor && dt > 1.7 * this.target) {
        this.tHeavy += dt;
        if (this.tHeavy > 1000) this.heavy = true;
      } else this.tHeavy = Math.max(0, this.tHeavy - dt);
      return this.res !== r0;
    }
    fps() { return 1000 / this.ema; }
    // pixels per frame-time, relative: higher = cheaper fractal (learned per formula)
    score() { return (this.res * this.res * this.target) / this.ema; }
  }

  // Before anything is measured: a rough cost order (folding IFS cheap, bulbs and hybrids dear).
  const PRIOR = { sierpinski: 1, octahedra: 0.95, kifsMenger: 0.9, mandelbox: 0.85, amazingBox: 0.8, kaleidoscopic: 0.75, icosahedral: 0.7, julia: 0.65, apollonian: 0.65, kaliBox: 0.6, mandelbulb: 0.4, hybrid: 0.3 };
  function fastest(formulas, learned, exclude, n) {
    const sc = (id) => (learned && learned[id] != null ? learned[id] : (PRIOR[id] != null ? PRIOR[id] * 0.5 : 0.25));
    return formulas.map((f) => f.id).filter((id) => id !== exclude).sort((a, b) => sc(b) - sc(a)).slice(0, n || 3);
  }

  // ---------- sound recipes (routes on the scene being shown) ----------
  const RECIPES = [
    { id: 'bass', label: 'Bass', sub: 'size pulse', source: 'master.low', dest: 'cam.dist', depth: 0.25, attack: 0.02, release: 0.25 },
    { id: 'beats', label: 'Beats', sub: 'flash', source: 'master.onset', dest: 'look.exposure', depth: 0.35, attack: 0, release: 0.12 },
    { id: 'highs', label: 'Highs', sub: 'colour shift', source: 'master.high', dest: 'look.palOffset', depth: 0.08, attack: 0.05, release: 0.4 },
    { id: 'mids', label: 'Mids', sub: 'twist', source: 'master.mid', dest: 'space.twist', depth: 0.04, attack: 0.15, release: 0.6 },
    { id: 'level', label: 'Level', sub: 'glow', source: 'master.rms', dest: 'look.glow', depth: 0.4, attack: 0.05, release: 0.3 }
  ];
  const SENS = [{ id: 'calm', label: 'Calm', k: 0.5 }, { id: 'normal', label: 'Normal', k: 1 }, { id: 'wild', label: 'Wild', k: 2 }];
  const recipeRoute = (project, r) => project.mod.find((m) => m.source === r.source && m.dest === r.dest);
  function recipeOn(project, sceneId, r) {
    const m = recipeRoute(project, r);
    return !!m && !(m.off || []).includes(sceneId);
  }
  // returns true when the project changed
  function toggleRecipe(project, sceneId, r, sensK, uid) {
    const m = recipeRoute(project, r);
    if (m) {
      m.off = m.off || [];
      const i = m.off.indexOf(sceneId);
      if (i >= 0) m.off.splice(i, 1);
      else m.off.push(sceneId);
      return true;
    }
    project.mod.push({ id: uid('mod'), off: project.scenes.filter((s) => s.id !== sceneId).map((s) => s.id), source: r.source, dest: r.dest, depth: r.depth * sensK, attack: r.attack, release: r.release, lead: 0 });
    return true;
  }
  function setSensitivity(project, sceneId, k) {
    RECIPES.forEach((r) => {
      const m = recipeRoute(project, r);
      if (m && !(m.off || []).includes(sceneId)) m.depth = r.depth * k;
    });
  }

  // ---------- GL overlay: textured quads (panels), rays and dots ----------
  const UI_VS = `#version 300 es
uniform mat4 uMVP;
uniform int uMode;
uniform vec3 uA, uB;
out vec2 vUv;
void main() {
  if (uMode == 1) { vUv = vec2(0.0); gl_Position = uMVP * vec4(gl_VertexID == 0 ? uA : uB, 1.0); return; }
  vec2 c[6] = vec2[6](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));
  vec2 q = c[gl_VertexID];
  vUv = vec2(q.x, 1.0 - q.y);
  gl_Position = uMVP * vec4(q.x - 0.5, q.y - 0.5, 0.0, 1.0);
}`;
  const UI_FS = `#version 300 es
precision highp float;
precision highp int;
uniform sampler2D uTex;
uniform vec4 uColor;
uniform int uMode;
in vec2 vUv;
out vec4 o;
void main() {
  if (uMode == 0) { o = texture(uTex, vUv) * uColor; return; }
  if (uMode == 2) { float d = length(vUv - 0.5) * 2.0; if (d > 1.0) discard; o = vec4(uColor.rgb, uColor.a * smoothstep(1.0, 0.6, d)); return; }
  o = uColor;
}`;

  function m4mul(a, b) {
    const o = new Float32Array(16);
    for (let c = 0; c < 4; c += 1) for (let r = 0; r < 4; r += 1) {
      let s = 0;
      for (let k = 0; k < 4; k += 1) s += a[k * 4 + r] * b[c * 4 + k];
      o[c * 4 + r] = s;
    }
    return o;
  }
  // model matrix of a quad centred at c with half-axes along right*w and up*h (unit square -> panel)
  function quadModel(c, right, up, w, h) {
    const n = cross(right, up);
    return new Float32Array([right[0] * w, right[1] * w, right[2] * w, 0, up[0] * h, up[1] * h, up[2] * h, 0, n[0], n[1], n[2], 0, c[0], c[1], c[2], 1]);
  }

  class Overlay {
    constructor(gl) {
      this.gl = gl;
      const sh = (type, src) => { const s = gl.createShader(type); gl.shaderSource(s, src); gl.compileShader(s); return s; };
      const vs = sh(gl.VERTEX_SHADER, UI_VS);
      const fs = sh(gl.FRAGMENT_SHADER, UI_FS);
      const p = gl.createProgram();
      gl.attachShader(p, vs);
      gl.attachShader(p, fs);
      gl.linkProgram(p);
      if (!gl.getProgramParameter(p, gl.LINK_STATUS)) console.error('VR overlay shader:', gl.getShaderInfoLog(fs) || gl.getShaderInfoLog(vs) || gl.getProgramInfoLog(p));
      this.prog = p;
      this.u = {};
      ['uMVP', 'uMode', 'uA', 'uB', 'uTex', 'uColor'].forEach((n) => { this.u[n] = gl.getUniformLocation(p, n); });
    }
    texture(canvas, tex) {
      const gl = this.gl;
      const t = tex || gl.createTexture();
      gl.bindTexture(gl.TEXTURE_2D, t);
      gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, false);
      gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, false);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, canvas);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      return t;
    }
    begin(fbo, vp, view) {
      const gl = this.gl;
      gl.bindFramebuffer(gl.FRAMEBUFFER, fbo);
      gl.viewport(vp.x, vp.y, vp.width, vp.height);
      gl.enable(gl.BLEND);
      gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
      gl.useProgram(this.prog);
      this.vpm = m4mul(view.projectionMatrix, view.transform.inverse.matrix);
    }
    quad(model, tex, color) {
      const gl = this.gl;
      gl.uniformMatrix4fv(this.u.uMVP, false, m4mul(this.vpm, model));
      gl.uniform1i(this.u.uMode, tex ? 0 : 3);
      gl.uniform4fv(this.u.uColor, color || [1, 1, 1, 1]);
      if (tex) { gl.activeTexture(gl.TEXTURE0); gl.bindTexture(gl.TEXTURE_2D, tex); gl.uniform1i(this.u.uTex, 0); }
      gl.drawArrays(gl.TRIANGLES, 0, 6);
    }
    line(a, b, color) {
      const gl = this.gl;
      gl.uniformMatrix4fv(this.u.uMVP, false, this.vpm);
      gl.uniform1i(this.u.uMode, 1);
      gl.uniform3fv(this.u.uA, a);
      gl.uniform3fv(this.u.uB, b);
      gl.uniform4fv(this.u.uColor, color);
      gl.drawArrays(gl.LINES, 0, 2);
    }
    dot(c, right, up, r, color) {
      const gl = this.gl;
      gl.uniformMatrix4fv(this.u.uMVP, false, m4mul(this.vpm, quadModel(c, right, up, r * 2, r * 2)));
      gl.uniform1i(this.u.uMode, 2);
      gl.uniform4fv(this.u.uColor, color);
      gl.drawArrays(gl.TRIANGLES, 0, 6);
    }
    end() { this.gl.disable(this.gl.BLEND); }
    dispose() { this.gl.deleteProgram(this.prog); }
  }

  // ---------- a flat panel in the room: canvas + tiles + ray hit ----------
  class Panel {
    constructor(w, h, metres) {
      this.cw = w;
      this.ch = h;
      this.mw = metres;
      this.mh = metres * h / w;
      this.canvas = typeof OffscreenCanvas !== 'undefined' ? new OffscreenCanvas(w, h) : Object.assign(document.createElement('canvas'), { width: w, height: h });
      this.g = this.canvas.getContext('2d');
      this.tiles = [];
      this.hover = null;
      this.dirty = true;
      this.tex = null;
      this.open = false;
    }
    // place it `dist` metres in front of the head, facing it, a little below the eyes
    placeFacing(head, fwd, dist, drop) {
      const f = norm([fwd[0], 0, fwd[2]]);
      this.c = add(add(head, mul(f, dist)), [0, -(drop || 0), 0]);
      this.n = mul(f, -1);
      this.up = [0, 1, 0];
      this.right = norm(cross(this.up, this.n));
    }
    placeAt(point, head) {
      this.c = point;
      const f = norm([head[0] - point[0], 0, head[2] - point[2]]);
      this.n = f;
      this.up = [0, 1, 0];
      this.right = norm(cross(this.up, this.n));
    }
    // ray (origin o, direction d, room space) -> { t, x, y } in canvas pixels, or null
    hit(o, d) {
      if (!this.open || !this.c) return null;
      const den = dot(d, this.n);
      if (Math.abs(den) < 1e-6) return null;
      const t = dot(sub(this.c, o), this.n) / den;
      if (t <= 0) return null;
      const p = add(o, mul(d, t));
      const u = dot(sub(p, this.c), this.right) / this.mw + 0.5;
      const v = 0.5 - dot(sub(p, this.c), this.up) / this.mh;
      if (u < 0 || u > 1 || v < 0 || v > 1) return null;
      return { t, x: u * this.cw, y: v * this.ch, p };
    }
    tileAt(x, y) { return this.tiles.find((t) => !t.disabled && x >= t.x && x <= t.x + t.w && y >= t.y && y <= t.y + t.h) || null; }
    model() { return quadModel(this.c, this.right, this.up, this.mw, this.mh); }
  }

  // drawing helpers for the panels
  function rr(g, x, y, w, h, r) {
    g.beginPath();
    g.moveTo(x + r, y);
    g.arcTo(x + w, y, x + w, y + h, r);
    g.arcTo(x + w, y + h, x, y + h, r);
    g.arcTo(x, y + h, x, y, r);
    g.arcTo(x, y, x + w, y, r);
    g.closePath();
  }
  const COL = { bg: 'rgba(12,16,26,0.92)', line: '#2a3346', tile: '#1a2233', hover: '#2a3a58', on: '#6cc4ff', onText: '#061018', text: '#e6edf7', faint: '#8d9ab0', warn: '#ffc46b', danger: '#ff6b7a' };
  function drawTile(g, t, hover) {
    rr(g, t.x, t.y, t.w, t.h, 10);
    g.fillStyle = t.active ? COL.on : (hover ? COL.hover : (t.fill || COL.tile));
    g.fill();
    if (t.strip) {
      const gr = g.createLinearGradient(t.x + 8, 0, t.x + t.w - 8, 0);
      t.strip.forEach((s) => { const pp = Number(s.p); if (Number.isFinite(pp) && s.c) gr.addColorStop(clamp(pp, 0, 1), s.c); });
      g.fillStyle = gr;
      rr(g, t.x + 8, t.y + t.h - 18, t.w - 16, 10, 4);
      g.fill();
    }
    if (hover && !t.active) { rr(g, t.x + 1, t.y + 1, t.w - 2, t.h - 2, 10); g.strokeStyle = COL.on; g.lineWidth = 2; g.stroke(); }
    g.fillStyle = t.active ? COL.onText : (t.disabled ? COL.faint : COL.text);
    g.textAlign = 'center';
    g.textBaseline = 'middle';
    const big = t.font || 24;
    g.font = `600 ${big}px system-ui, sans-serif`;
    const cy = t.strip ? t.y + (t.h - 16) / 2 : t.y + t.h / 2;
    if (t.sub) {
      g.fillText(t.label, t.x + t.w / 2, cy - big * 0.45, t.w - 12);
      g.font = `400 ${Math.round(big * 0.75)}px system-ui, sans-serif`;
      g.fillStyle = t.active ? COL.onText : COL.faint;
      g.fillText(t.sub, t.x + t.w / 2, cy + big * 0.55, t.w - 12);
    } else g.fillText(t.label, t.x + t.w / 2, cy, t.w - 12);
  }

  // ---------- the session controller ----------
  // deps: { gl, renderer, session, layer, ref, Formulas, Schema, Pal, project(), evaluate(t), time(), playing(),
  //         togglePlay(), commit(), holdScene(id), arrangement(), isHeld(), precompile(id), mic, depthK(),
  //         settings(), saveSettings(patch), status(text) }
  function create(deps) {
    const gl = deps.gl;
    const overlay = new Overlay(gl);
    const S0 = deps.settings();
    const perf = new Perf(72, { start: 0.75, min: 0.3 });
    const learned = Object.assign({}, S0.perf || {});
    const st = {
      mode: 'ride', rig: null, pinch: null, twist: null, twistHand: null, held: {}, press: { left: null, right: null }, param: { left: null, right: null }, tab: null,
      lastTime: 0, pending: null, msg: '', msgUntil: 0, heavyShownFor: null, hintUntil: performance.now() + 9000,
      sens: S0.sens || 'normal', auto: S0.auto !== false, res: perf.res, lastFormula: null, frame: null
    };
    const main = new Panel(1280, 900, 1.15);
    const sound = new Panel(820, 820, 0.62);
    const hint = new Panel(1100, 230, 0.8);
    hint.open = true;

    if (deps.session.supportedFrameRates && deps.session.updateTargetFrameRate) {
      const rates = Array.from(deps.session.supportedFrameRates);
      if (rates.includes(72)) deps.session.updateTargetFrameRate(72).then(() => perf.setHz(72)).catch(() => {});
      else if (rates.length) perf.setHz(Math.min(...rates));
    } else if (deps.session.frameRate) perf.setHz(deps.session.frameRate);

    const say = (text, ms) => { st.msg = text; st.msgUntil = performance.now() + (ms || 3000); main.dirty = true; sound.dirty = true; };
    const project = () => deps.project();
    const sceneOf = (frame) => project().scenes.find((s) => s.id === (frame ? frame.sceneId : null)) || project().scenes[0];

    // --- actions ---
    function pickFormula(id) {
      const sc = sceneOf(st.frame);
      if (!sc || sc.formula === id) return;
      deps.precompile(id);
      st.pending = { sceneId: sc.id, formula: id, since: performance.now() };
      main.dirty = true;
    }
    function applyPending() {
      const p = st.pending;
      if (!p) return;
      const entry = deps.renderer._formulaProgram(p.formula);
      if (entry.failed) { st.pending = null; say('That fractal did not compile here.', 4000); return; }
      if (!deps.renderer._checkProgram(entry)) return; // still compiling: keep showing the old one
      const sc = project().scenes.find((s) => s.id === p.sceneId);
      st.pending = null;
      if (!sc) return;
      const nf = deps.Formulas.get(p.formula);
      sc.formula = nf.id;
      sc.params = deps.Formulas.defaults(nf.id);
      sc.iter = nf.iter.def;
      sc.space.step = nf.step;
      const orbit = sc.camera.orbit;
      sc.camera = Object.assign(deps.Schema.defaultCamera(nf.id), { orbit });
      deps.holdScene(sc.id);
      deps.commit();
      st.mode = 'ride'; // seat the viewer at the new fractal's own camera
      st.heavyShownFor = null;
      perf.heavy = false;
      perf.res = Math.max(perf.res, 0.6);
      main.dirty = true;
    }
    function pickLook(id) {
      const sc = sceneOf(st.frame);
      if (!sc) return;
      const pal = sc.look.palette;
      const keep = { source: pal.source, wrap: pal.wrap, interp: pal.interp };
      sc.look = deps.Schema.applyLookPreset(sc.look, id);
      Object.assign(sc.look.palette, keep);
      deps.holdScene(sc.id);
      deps.commit();
      main.dirty = true;
    }
    /* Picking a palette on its own, which the panel could not do before: a look preset carries a
       palette plus light and grade, and sometimes you only want the colours changed. The stops are
       copied rather than referenced, or every scene on that preset would share one array and
       editing one would edit them all. */
    function pickPalette(id) {
      const sc = sceneOf(st.frame);
      const preset = deps.Pal.PRESET_BY_ID[id];
      if (!sc || !preset) return;
      sc.look.palette.stops = preset.stops.map((s2) => ({ p: s2.p, c: s2.c }));
      deps.holdScene(sc.id);
      deps.commit();
      main.dirty = true;
    }
    function sensK() { return (SENS.find((s) => s.id === st.sens) || SENS[1]).k; }
    function toggleRec(r) {
      const sc = sceneOf(st.frame);
      if (!sc) return;
      toggleRecipe(project(), sc.id, r, sensK(), deps.Schema.uid);
      deps.commit();
      if (!deps.mic.active && !deps.hasMaster()) say('These follow the microphone: switch it on (centre).', 4000);
      sound.dirty = true;
    }
    function setSens(id) {
      st.sens = id;
      const sc = sceneOf(st.frame);
      if (sc) setSensitivity(project(), sc.id, sensK());
      deps.commit();
      deps.saveSettings({ sens: id });
      sound.dirty = true;
    }
    function toggleMic() {
      if (deps.mic.active) { deps.mic.stop(); sound.dirty = true; return; }
      say('Starting the microphone…', 2000);
      deps.mic.start().then(() => say('Listening.', 2000)).catch((err) => say(`Microphone: ${err.message} Allow it on the page first (outside VR).`, 7000));
    }
    function backToRide() { st.mode = 'ride'; st.pinch = null; st.twist = null; say('Back on the scene camera.', 1500); }

    // --- panel layouts ---
    function layoutMain() {
      const P = main;
      const g = P.g;
      const frame = st.frame;
      const sc = sceneOf(frame);
      P.tiles = [];
      g.clearRect(0, 0, P.cw, P.ch);
      rr(g, 2, 2, P.cw - 4, P.ch - 4, 26);
      g.fillStyle = COL.bg;
      g.fill();
      g.strokeStyle = COL.line;
      g.lineWidth = 3;
      g.stroke();
      g.textAlign = 'left';
      g.textBaseline = 'middle';
      g.fillStyle = COL.text;
      g.font = '700 30px system-ui, sans-serif';
      g.fillText('Fractal Video', 30, 42);
      g.font = '400 22px system-ui, sans-serif';
      g.fillStyle = COL.faint;
      g.fillText(`${Math.round(perf.fps())} fps · resolution ${Math.round(perf.res * 100)} %${st.auto ? ' (auto)' : ''} · ${sc ? sc.name : ''}`, 260, 43);
      P.tiles.push({ id: 'close', x: P.cw - 86, y: 14, w: 64, h: 56, label: '✕', font: 28, on: () => { P.open = false; } });
      /* Tabs across the top, one per family plus Looks and Palettes.

         The panel used to show every fractal, every look and the action bar at once, which meant
         34 names in a seven-column grid at 21px and 18 looks at 18px. On a headset that is
         readable right up until the renderer throttles the resolution, which is exactly when you
         are reaching for the panel. Showing one tab at a time costs a trigger press and buys back
         enough room for 32px names. */
      const TABS = deps.Formulas.CATEGORIES.map((c) => ({ id: `cat:${c.id}`, label: c.label }))
        .concat([{ id: 'look', label: 'Looks' }, { id: 'pal', label: 'Palettes' }]);
      if (!st.tab || !TABS.some((t) => t.id === st.tab)) {
        // open on the family the current fractal belongs to, so the panel comes up where you are
        const cur = sc && deps.Formulas.get(sc.formula);
        st.tab = `cat:${(cur && cur.cat) || 'mixed'}`;
      }
      const tabW = (P.cw - 60 - (TABS.length - 1) * 8) / TABS.length;
      TABS.forEach((t, i) => P.tiles.push({
        id: `tab:${t.id}`, x: 30 + i * (tabW + 8), y: 84, w: tabW, h: 60, label: t.label, font: 23,
        active: st.tab === t.id, on: () => { st.tab = t.id; main.dirty = true; }
      }));

      let y = 160;
      if (perf.heavy) {
        rr(g, 24, y, P.cw - 48, 112, 14);
        g.fillStyle = 'rgba(255,196,107,0.14)';
        g.fill();
        g.fillStyle = COL.warn;
        g.font = '600 24px system-ui, sans-serif';
        g.textAlign = 'left';
        g.fillText(`Heavy here: ${Math.round(perf.fps())} fps at the lowest resolution. A faster fractal:`, 44, y + 28);
        const fast = fastest(deps.Formulas.FORMULAS, learned, sc && sc.formula, 3);
        fast.forEach((id, i) => {
          const f = deps.Formulas.get(id);
          P.tiles.push({ id: `fast:${id}`, x: 44 + i * 260, y: y + 50, w: 244, h: 50, label: f.name, font: 22, fill: '#3a3020', on: () => pickFormula(id) });
        });
        P.tiles.push({ id: 'keep', x: P.cw - 44 - 220, y: y + 50, w: 220, h: 50, label: 'Keep this one', font: 22, on: () => { perf.heavy = false; st.heavyShownFor = sc && sc.formula; } });
        y += 126;
      }

      // the action bar keeps its place at the bottom whatever the tab shows, so the controls you
      // use every few seconds do not move around under your hand
      const barY = P.ch - 110;
      const grid = (items, cols, gap, maxH, make) => {
        const rects = gridRects(items.length, cols, gap, maxH, y, barY - 16, P.cw);
        items.forEach((item, i) => P.tiles.push(Object.assign(rects[i], make(item, i))));
      };

      if (st.tab.indexOf('cat:') === 0) {
        const cat = st.tab.slice(4);
        // nine at the most in any family, so three columns leave room for 32px names
        grid(deps.Formulas.FORMULAS.filter((f) => f.cat === cat), 3, 14, 170, (f) => {
          const pend = st.pending && st.pending.formula === f.id;
          return { id: `f:${f.id}`, label: pend ? 'loading…' : f.name, font: 32,
            active: sc && sc.formula === f.id, on: () => pickFormula(f.id) };
        });
      } else if (st.tab === 'look') {
        grid(deps.Schema.LOOK_PRESETS, 4, 12, 112, (p2) => ({
          id: `l:${p2.id}`, label: p2.name, font: 26,
          strip: (deps.Pal.PRESET_BY_ID[p2.palette] || { stops: [] }).stops,
          active: false, on: () => pickLook(p2.id)
        }));
      } else {
        /* Fifty-one palettes will not go large however they are arranged, so these stay small and
           lean on the colour strip, which is what you actually recognise a palette by. */
        const cur = (sc && sc.look && sc.look.palette && sc.look.palette.stops) || [];
        const same = (a, b) => a.length === b.length && a.every((s2, i) => s2.c === b[i].c && Math.abs(s2.p - b[i].p) < 1e-6);
        grid(deps.Pal.PRESETS, 6, 8, 66, (p2) => ({
          id: `p:${p2.id}`, label: p2.name || p2.id, font: 20, strip: p2.stops,
          active: same(cur, p2.stops), on: () => pickPalette(p2.id)
        }));
      }

      const bw = (P.cw - 60 - 4 * 12) / 5;
      const bar = [
        { id: 'play', label: deps.playing() ? 'Pause' : 'Play', sub: 'A', on: () => deps.togglePlay() },
        { id: 'hold', label: deps.isHeld() ? 'Play the arrangement' : 'Hold this scene', sub: deps.isHeld() ? 'holding one scene' : 'arrangement playing', on: () => { if (deps.isHeld()) deps.arrangement(); else if (sc) deps.holdScene(sc.id); } },
        { id: 'reset', label: 'Scene camera', sub: 'B', on: backToRide },
        { id: 'sound', label: 'Sound…', sub: 'X, or point + trigger', on: () => openSound(null) },
        { id: 'auto', label: st.auto ? 'Auto quality' : 'Fixed quality', sub: st.auto ? 'follows frame rate' : `${Math.round(perf.res * 100)} %`, active: st.auto, on: () => { st.auto = !st.auto; deps.saveSettings({ auto: st.auto }); } }
      ];
      bar.forEach((b, i) => P.tiles.push(Object.assign({ x: 30 + i * (bw + 12), y: barY, w: bw, h: 72, font: 22 }, b)));
      if (st.msg && performance.now() < st.msgUntil) {
        g.fillStyle = COL.warn;
        g.font = '500 22px system-ui, sans-serif';
        g.textAlign = 'left';
        g.fillText(st.msg, 30, P.ch - 20, P.cw - 60);
      }
      P.tiles.forEach((t) => drawTile(g, t, P.hover === t.id));
    }

    function layoutSound() {
      const P = sound;
      const g = P.g;
      const sc = sceneOf(st.frame);
      P.tiles = [];
      g.clearRect(0, 0, P.cw, P.ch);
      g.beginPath();
      g.arc(P.cw / 2, 380, 372, 0, Math.PI * 2);
      g.fillStyle = COL.bg;
      g.fill();
      g.strokeStyle = COL.line;
      g.lineWidth = 3;
      g.stroke();
      rr(g, 60, 700, P.cw - 120, 112, 20);
      g.fillStyle = COL.bg;
      g.fill();
      g.stroke();
      RECIPES.forEach((r, i) => {
        const a = -Math.PI / 2 + i * (2 * Math.PI / RECIPES.length);
        const cx = P.cw / 2 + Math.cos(a) * 250, cy = 380 + Math.sin(a) * 250;
        P.tiles.push({ id: `r:${r.id}`, x: cx - 105, y: cy - 56, w: 210, h: 112, label: r.label, sub: r.sub, font: 28, active: sc && recipeOn(project(), sc.id, r), on: () => toggleRec(r) });
      });
      const on = deps.mic.active;
      P.tiles.push({ id: 'mic', x: P.cw / 2 - 110, y: 380 - 62, w: 220, h: 124, label: on ? 'Mic on' : 'Mic off', sub: on ? 'listening' : (deps.mic.starting ? 'starting…' : 'tap to listen'), font: 30, active: on, fill: '#26324a', on: toggleMic });
      const sw = (P.cw - 160 - 2 * 10) / 3;
      SENS.forEach((s, i) => P.tiles.push({ id: `s:${s.id}`, x: 80 + i * (sw + 10), y: 740, w: sw, h: 56, label: s.label, font: 24, active: st.sens === s.id, on: () => setSens(s.id) }));
      g.fillStyle = COL.faint;
      g.font = '600 18px system-ui, sans-serif';
      g.textAlign = 'center';
      g.fillText('SENSITIVITY', P.cw / 2, 720);
      P.tiles.push({ id: 'close', x: P.cw - 120, y: 30, w: 64, h: 56, label: '✕', font: 28, on: () => { P.open = false; } });
      if (st.msg && performance.now() < st.msgUntil) {
        g.fillStyle = COL.warn;
        g.font = '500 20px system-ui, sans-serif';
        g.fillText(st.msg, P.cw / 2, 680, P.cw - 120);
      }
      P.tiles.forEach((t) => drawTile(g, t, P.hover === t.id));
    }

    function layoutHint() {
      const P = hint;
      const g = P.g;
      g.clearRect(0, 0, P.cw, P.ch);
      rr(g, 2, 2, P.cw - 4, P.ch - 4, 22);
      g.fillStyle = 'rgba(12,16,26,0.82)';
      g.fill();
      g.fillStyle = COL.text;
      g.textAlign = 'center';
      g.textBaseline = 'middle';
      g.font = '600 28px system-ui, sans-serif';
      g.fillText('Left stick fly · right stick turn · both triggers: pull apart to scale', P.cw / 2, 46, P.cw - 40);
      g.fillText('Hold one trigger and move that hand to shape the fractal', P.cw / 2, 92, P.cw - 40);
      g.fillText('Grip: twist the fractal · Y panel · A play / pause · B scene camera', P.cw / 2, 138, P.cw - 40);
      g.fillStyle = COL.faint;
      g.font = '400 24px system-ui, sans-serif';
      g.fillText('Panel: tabs along the top · families, Looks, Palettes · point and pull the trigger', P.cw / 2, 182, P.cw - 40);
    }

    function openMain(head, fwd) {
      main.placeFacing(head, fwd, 1.0, 0.12);
      main.open = true;
      main.dirty = true;
    }
    let lastHead = [0, 1.6, 0], lastFwd = [0, 0, -1];
    function openSound(ray) {
      if (ray) sound.placeAt(add(ray.o, mul(ray.d, 0.8)), lastHead);
      else sound.placeFacing(lastHead, lastFwd, 0.85, 0.05);
      sound.open = true;
      sound.dirty = true;
    }

    // --- input ---
    const BTN = { trigger: 0, grip: 1, stick: 3, a: 4, b: 5 };

    // --- live parameter control: hold one trigger and move that hand ---
    // Each trigger is two parameters: the hand's up/down and right/left, measured against where
    // you are facing rather than against the room. Up and right raise the value.
    //
    // Nothing moves until the trigger has been held for PARAM_ARM. That wait is what lets the
    // two-handed scale gesture through untouched: its second trigger always lands a few frames
    // after the first, and until the wait is up neither hand has grabbed anything. It also leaves
    // the panel alone — a click is a short press, and a press that started on a panel never grabs.
    //
    // Control is relative: the anchor remembers where the hand was and what the values were, and
    // PARAM_SPAN of movement covers the range. Let go, move back, grab again, like a mouse.
    const PARAM_ARM = 350;    // ms a trigger must be held before the hand starts changing values
    const PARAM_SPAN = 0.35;  // metres of hand movement covering a parameter's whole range

    // Four per formula, in order: [left up/down, left right/left, right up/down, right right/left].
    // Values stay inside each parameter's `calm` band from formulas.js — the range its author
    // marked as the one worth playing in.
    const PARAM_MAP = {
      mandelbulb:     ['shape.power', 'shape.theta', 'shape.julia', 'shape.cx'],
      mandelbox:      ['shape.scale', 'shape.fold', 'shape.minR', 'shape.fixedR'],
      amazingBox:     ['shape.scale', 'shape.fold', 'shape.minR', 'shape.rotY'],
      kifsMenger:     ['shape.scale', 'shape.offX', 'shape.offY', 'shape.rotY'],
      sierpinski:     ['shape.scale', 'shape.offset', 'shape.rotY', 'shape.rotX'],
      octahedra:      ['shape.scale', 'shape.offX', 'shape.offY', 'shape.rotY'],
      kaleidoscopic:  ['shape.scale', 'shape.offX', 'shape.preY', 'shape.postZ'],
      hybrid:         ['shape.power', 'shape.fold', 'shape.rotY', 'shape.rotZ'],
      julia:          ['shape.cx', 'shape.cy', 'shape.cz', 'shape.cw'],
      apollonian:     ['shape.k', 'space.bound', 'space.repeat', 'space.twist'],
      pseudoKleinian: ['shape.boxXY', 'shape.boxZ', 'shape.size', 'shape.thick'],
      kaliBox:        ['shape.scale', 'shape.minR2', 'shape.jy', 'shape.ty'],
      amazingSurf:    ['shape.scale', 'shape.fold', 'shape.lift', 'shape.minR'],
      icosahedral:    ['shape.scale', 'shape.wB', 'shape.reach', 'shape.postZ'],
      hyperbolic:     ['shape.order', 'shape.travel', 'shape.drift', 'shape.edge'],
      vaulted:        ['shape.scale', 'shape.vault', 'shape.foldY', 'shape.fold'],
      ziggurat:       ['shape.scale', 'shape.spread', 'shape.rise', 'shape.steep'],
      skyline:        ['shape.height', 'shape.density', 'shape.cell', 'shape.setback'],
      turbine:        ['shape.spin', 'shape.pitch', 'shape.taper', 'shape.radius'],
      greeble:        ['shape.scale', 'shape.plate', 'shape.offY', 'shape.inset'],
      pipeworks:      ['shape.scale', 'shape.pipe', 'shape.joint', 'shape.offX'],
      pistons:        ['shape.phase', 'shape.stroke', 'shape.wave', 'shape.base'],
      clockwork:      ['shape.spin', 'shape.ratio', 'shape.windows', 'shape.tooth'],
      truss:          ['shape.scale', 'shape.stretch', 'shape.beam', 'shape.offZ'],
      reactor:        ['shape.spin', 'shape.tilt', 'shape.ratio', 'shape.width'],
      terrain:        ['shape.height', 'shape.ridge', 'shape.freq', 'shape.warp'],
      kaliPlain:      ['shape.cx', 'shape.cy', 'shape.height', 'shape.focus'],
      ridges:         ['shape.height', 'shape.erosion', 'shape.canyon', 'shape.freq']
    };
    // A formula with no entry (a new one) gets its first four parameters, padded with space warps.
    const PARAM_PAD = ['space.twist', 'space.bound', 'space.repeat', 'space.detail'];
    function paramDests(formulaId) {
      if (PARAM_MAP[formulaId]) return PARAM_MAP[formulaId];
      const keys = deps.Formulas.get(formulaId).params.slice(0, 4).map((q) => `shape.${q.key}`);
      while (keys.length < 4) keys.push(PARAM_PAD[keys.length]);
      return keys;
    }
    const paramBox = (sc, group) => (group === 'shape' ? sc.params : group === 'space' ? sc.space : group === 'look' ? sc.look : null);
    function paramRange(dest, formulaId) {
      const spec = deps.Schema.keySpec(dest, formulaId);
      if (!spec) return null;
      let lo = spec.min, hi = spec.max;
      if (dest.indexOf('shape.') === 0) {
        const q = deps.Formulas.get(formulaId).params.find((x) => x.key === dest.slice(6));
        if (q && q.calm) { lo = q.calm[0]; hi = q.calm[1]; }
      }
      return { lo, hi, min: spec.min, max: spec.max, step: spec.step, label: spec.label };
    }
    function paramRead(sc, dest) {
      const [g, k] = dest.split('.');
      const box = paramBox(sc, g);
      if (!box || !(k in box)) return null;
      const v = Number(box[k]);
      return Number.isFinite(v) ? v : null;
    }
    function paramWrite(sc, dest, v, r) {
      const [g, k] = dest.split('.');
      const box = paramBox(sc, g);
      if (!box) return false;
      v = clamp(v, r.min, r.max);
      if (r.step >= 1) v = Math.round(v / r.step) * r.step; // tiers, blades, teeth: whole numbers
      if (v === box[k]) return false;
      box[k] = v;
      return true;
    }
    // Let go: one undo entry for the whole gesture, not one per frame.
    function endParam(side) {
      const g = st.param[side];
      st.param[side] = null;
      if (g && g.moved) deps.commit();
    }
    function paramGesture(side, hands, frame, headFwd) {
      const h = hands[side];
      const p = st.press[side];
      if (!h || !h.trig || !p || p.pinch || p.onPanel || st.pinch || st.twist) { endParam(side); return; }
      if (performance.now() - p.t < PARAM_ARM) return;
      const sc = sceneOf(frame);
      const dests = paramDests(sc.formula);
      const base = side === 'left' ? 0 : 2;
      let g = st.param[side];
      if (!g || g.sceneId !== sc.id || g.formula !== sc.formula) {
        const right = norm(cross(headFwd, [0, 1, 0]));
        g = st.param[side] = {
          sceneId: sc.id, formula: sc.formula, pos: h.pos, right, moved: false,
          a: paramRead(sc, dests[base]), b: paramRead(sc, dests[base + 1])
        };
        const ra = paramRange(dests[base], sc.formula), rb = paramRange(dests[base + 1], sc.formula);
        say(`${ra ? ra.label : '—'} (up) · ${rb ? rb.label : '—'} (right)`, 1800);
        return; // anchor this frame, move from the next
      }
      const d = sub(h.pos, g.pos);
      [[dests[base], g.a, d[1]], [dests[base + 1], g.b, dot(d, g.right)]].forEach(([dest, from, moved]) => {
        if (from == null) return;
        const r = paramRange(dest, sc.formula);
        if (r && paramWrite(sc, dest, from + (moved / PARAM_SPAN) * (r.hi - r.lo), r)) g.moved = true;
      });
    }
    function readInputs(xf) {
      const hands = {};
      deps.session.inputSources.forEach((src) => {
        if (!src.gamepad || (src.handedness !== 'left' && src.handedness !== 'right')) return;
        const rp = src.targetRaySpace && xf.getPose(src.targetRaySpace, deps.ref);
        const gp = (src.gripSpace && xf.getPose(src.gripSpace, deps.ref)) || rp;
        if (!rp) return;
        const m = rp.transform.matrix;
        const b = src.gamepad.buttons;
        const ax = src.gamepad.axes;
        hands[src.handedness] = {
          o: posXR(m), d: norm(mul([m[8], m[9], m[10]], -1)),
          pos: gp ? posXR(gp.transform.matrix) : posXR(m), rot: fromXR((gp || rp).transform.matrix),
          trig: !!(b[BTN.trigger] && b[BTN.trigger].pressed), grip: !!(b[BTN.grip] && b[BTN.grip].pressed),
          a: !!(b[BTN.a] && b[BTN.a].pressed), b: !!(b[BTN.b] && b[BTN.b].pressed),
          sx: ax.length > 3 ? ax[2] : (ax[0] || 0), sy: ax.length > 3 ? ax[3] : (ax[1] || 0)
        };
      });
      return hands;
    }
    const edge = (key, now) => { const was = !!st.held[key]; st.held[key] = now; return now && !was; };
    const dead = (v) => (Math.abs(v) < 0.15 ? 0 : (v - Math.sign(v) * 0.15) / 0.85);

    function toFly(frame, scale) {
      if (st.mode === 'fly') return;
      st.rig = rideRig(frame, scale);
      st.mode = 'fly';
    }
    function rideRig(frame, scale) {
      return deps.level() ? Rig.fromCamera(frame.gpu, scale) : Rig.fromCameraFull(frame.gpu, scale);
    }

    function handle(hands, head, headFwd, dtS, frame, scale) {
      const L = hands.left, R = hands.right;
      // buttons
      if (R && edge('A', R.a)) { deps.togglePlay(); main.dirty = true; }
      if (R && edge('B', R.b)) backToRide();
      if (L && edge('Y', L.b)) { if (main.open) main.open = false; else openMain(head, headFwd); }
      if (L && edge('X', L.a)) { if (sound.open) sound.open = false; else openSound(null); }
      // pointing: what each ray hits (nearest panel), hover per panel, trigger click on release
      // (unless the press turned into a two-hand pinch, or was held long)
      ['left', 'right'].forEach((side) => {
        const h = hands[side];
        if (!h) return;
        let best = null;
        [main, sound].forEach((P) => {
          const hit = P.hit(h.o, h.d);
          if (hit && (!best || hit.t < best.hit.t)) best = { P, hit };
        });
        h.hit = best;
        h.tile = best ? best.P.tileAt(best.hit.x, best.hit.y) : null;
      });
      [main, sound].forEach((P) => {
        const hh = [R, L].find((h) => h && h.hit && h.hit.P === P);
        const id = hh && hh.tile ? hh.tile.id : null;
        if (P.hover !== id) { P.hover = id; P.dirty = true; }
      });
      ['left', 'right'].forEach((side) => {
        const h = hands[side];
        if (!h) return;
        if (edge(`trig:${side}`, h.trig)) st.press[side] = { t: performance.now(), pinch: false, onPanel: !!h.hit };
        else if (!h.trig && st.press[side]) {
          const s = st.press[side];
          st.press[side] = null;
          if (s.pinch || performance.now() - s.t > 600) return;
          if (h.tile) { h.tile.on(); h.hit.P.dirty = true; }
          else if (h.hit) { /* on a panel, between tiles */ }
          else if (main.open || sound.open) { main.open = false; sound.open = false; }
          else openSound(h);
        }
      });
      // both triggers: scale (pull apart = bigger) and drag
      if (L && R && L.trig && R.trig) {
        if (st.press.left) st.press.left.pinch = true;
        if (st.press.right) st.press.right.pinch = true;
        toFly(frame, scale);
        if (!st.pinch) st.pinch = Rig.pinchStart(st.rig, L.pos, R.pos);
        st.rig = Rig.pinch(st.pinch, L.pos, R.pos, scale * 1e-4, scale * 50);
      } else st.pinch = null;
      // one trigger held, not pinching: that hand drives two of the fractal's parameters
      ['left', 'right'].forEach((side) => paramGesture(side, hands, frame, headFwd));
      // grip: twist about the scene's target
      const gh = (st.twistHand && hands[st.twistHand] && hands[st.twistHand].grip) ? st.twistHand : (R && R.grip ? 'right' : (L && L.grip ? 'left' : null));
      if (gh && !st.pinch) {
        toFly(frame, scale);
        if (!st.twist || st.twistHand !== gh) {
          const c = frame.camera;
          st.twist = Rig.twistStart(st.rig, hands[gh].rot, [c.tx || 0, c.ty || 0, c.tz || 0]);
          st.twistHand = gh;
        }
        st.rig = Rig.twist(st.twist, hands[gh].rot);
      } else { st.twist = null; st.twistHand = null; }
      // sticks: fly along the gaze and sideways; turn and rise with the right stick
      const fy = L ? -dead(L.sy) : 0, fx = L ? dead(L.sx) : 0;
      const tx = R ? dead(R.sx) : 0, ty = R ? -dead(R.sy) : 0;
      if (fy || fx || tx || ty) {
        toFly(frame, scale);
        const speed = 1.4 * dtS; // metres of the room per second at full stick; the world scale makes it fractal-sized
        const right = norm(cross(headFwd, [0, 1, 0]));
        let d = add(mul(headFwd, fy * speed), mul(right, fx * speed));
        d = add(d, [0, ty * speed * 0.7, 0]);
        st.rig = Rig.fly(st.rig, d);
        if (tx) st.rig = Rig.turn(st.rig, head, -tx * 1.2 * dtS);
      }
    }

    // --- per frame ---
    function onFrame(time, xf) {
      const pose = xf.getViewerPose(deps.ref);
      if (!pose) return null;
      const dt = st.lastTime ? time - st.lastTime : 0;
      st.lastTime = time;
      if (st.auto && perf.frame(dt)) main.dirty = true;
      else if (!st.auto) perf.ema += ((dt > 0 && dt < 2000 ? dt : perf.ema) - perf.ema) * 0.15;
      applyPending();
      const frame = deps.evaluate();
      st.frame = frame;
      if (st.lastFormula !== frame.formula) {
        if (st.lastFormula) { perf.heavy = false; st.heavyShownFor = null; }
        st.lastFormula = frame.formula;
        main.dirty = true;
      }
      const dist = frame.camera && frame.camera.dist > 0 ? frame.camera.dist : 1;
      const scale = (deps.depthK() * dist) / 0.064;
      const hm = pose.transform.matrix;
      const head = posXR(hm);
      const headFwd = norm(mul([hm[8], hm[9], hm[10]], -1));
      lastHead = head;
      lastFwd = headFwd;
      const hands = readInputs(xf);
      st.hands = hands;
      handle(hands, head, headFwd, dt > 0 && dt < 200 ? dt / 1000 : 0, frame, scale);

      // the heavy offer opens the panel once per fractal
      if (perf.heavy && st.heavyShownFor !== frame.formula) {
        st.heavyShownFor = frame.formula;
        openMain(head, headFwd);
      }
      // learn how cheap this fractal is (only while the guard is steering)
      if (st.auto && dt > 0 && dt < 500) {
        const s = perf.score();
        learned[frame.formula] = learned[frame.formula] == null ? s : learned[frame.formula] + (s - learned[frame.formula]) * 0.02;
      }

      let rig;
      if (st.mode === 'ride') rig = rideRig(frame, scale);
      else {
        // the music's camera push still breathes while flying: scale the world about the target
        const base = deps.evaluateBase();
        const bd = base && base.camera && base.camera.dist > 0 ? base.camera.dist : dist;
        const c = frame.camera;
        rig = Rig.pulse(st.rig, [c.tx || 0, c.ty || 0, c.tz || 0], clamp(dist / bd, 0.2, 5));
      }

      if (main.open && main.dirty) { layoutMain(); main.tex = overlay.texture(main.canvas, main.tex); main.dirty = false; }
      if (sound.open && sound.dirty) { layoutSound(); sound.tex = overlay.texture(sound.canvas, sound.tex); sound.dirty = false; }
      const showHint = performance.now() < st.hintUntil && !main.open && !sound.open;
      if (showHint && !hint.tex) { layoutHint(); hint.tex = overlay.texture(hint.canvas, null); }
      if (showHint && !hint.c) hint.placeFacing(head, headFwd, 1.3, 0.35);

      const views = [];
      pose.views.forEach((v) => {
        const vp = deps.layer.getViewport(v);
        if (!vp || vp.width < 1 || vp.height < 1) return;
        deps.renderer.renderXR(frame, v, vp, deps.layer.framebuffer, { rig, res: perf.res, quality: deps.march(), level: false });
        overlay.begin(deps.layer.framebuffer, vp, v);
        if (showHint) overlay.quad(hint.model(), hint.tex, [1, 1, 1, Math.min(1, (st.hintUntil - performance.now()) / 1500)]);
        if (main.open) overlay.quad(main.model(), main.tex);
        if (sound.open) overlay.quad(sound.model(), sound.tex);
        ['left', 'right'].forEach((side) => {
          const h = hands[side];
          if (!h) return;
          const far = h.hit ? h.hit.hit.t : (main.open || sound.open ? 2.5 : 0.35);
          const tip = add(h.o, mul(h.d, far));
          overlay.line(h.o, tip, h.hit ? [0.42, 0.77, 1, 0.95] : [1, 1, 1, 0.35]);
          if (h.hit) {
            const P = h.hit.P;
            overlay.dot(add(tip, mul(P.n, 0.002)), P.right, P.up, 0.008, [0.42, 0.77, 1, 1]);
          }
        });
        overlay.end();
        views.push(`${vp.width}×${vp.height}`);
      });
      if (dt > 0 && Math.floor(time / 1000) !== Math.floor((time - dt) / 1000)) {
        deps.saveSettings({ perf: learned });
        deps.status(`In VR: ${Math.round(perf.fps())} frames/s, ${views[0] || ''} per eye, fractal at ${Math.round(perf.res * 100)} %${deps.mic.active ? ', listening' : ''}. Y = panel. Press the Quest menu button to leave.`);
      }
      return { fps: perf.fps(), res: perf.res, mode: st.mode, rig, panel: main.open, sound: sound.open, heavy: perf.heavy };
    }

    function end() {
      deps.saveSettings({ perf: learned });
      [main, sound, hint].forEach((P) => { if (P.tex) gl.deleteTexture(P.tex); });
      overlay.dispose();
    }

    // redrawPanels is here so the panel can be drawn and screenshotted outside a headset; the
    // layout is the part that goes wrong, and checking it in VR means putting the Quest on.
    return { onFrame, end, state: st, perf, panels: { main, sound, hint }, openMain: () => openMain(lastHead, lastFwd), openSound: () => openSound(null), redrawPanels: () => { layoutMain(); layoutSound(); } };
  }

  /* Where the tiles of one tab go. Pulled out of the panel drawing so it can be checked without
     a headset: the arithmetic is the part that breaks - a family one row taller than expected and
     the bottom row lands under the action bar, where it is still clickable but invisible. */
  function gridRects(count, cols, gap, maxH, top, bottom, cw, margin) {
    const m = margin == null ? 30 : margin;
    const w = (cw - m * 2 - (cols - 1) * gap) / cols;
    const rows = Math.max(1, Math.ceil(count / cols));
    const h = Math.min(maxH, (bottom - top - (rows - 1) * gap) / rows);
    // A family with six fractals would otherwise sit in the top half with a hole under it, so the
    // block is centred in the body. Tabs that fill their space are unaffected.
    const y0 = top + Math.max(0, (bottom - top - (rows * h + (rows - 1) * gap)) / 2);
    const out = [];
    for (let i = 0; i < count; i += 1) {
      out.push({ x: m + (i % cols) * (w + gap), y: y0 + Math.floor(i / cols) * (h + gap), w, h });
    }
    return out;
  }

  FV.VrMode = { Rig, Perf, RECIPES, SENS, recipeOn, toggleRecipe, setSensitivity, fastest, create, gridRects, math: { add, sub, mul, dot, cross, len, norm, mv, mm, mT, axisAngle } };
  if (typeof module !== 'undefined' && module.exports) module.exports = FV.VrMode;
})();
