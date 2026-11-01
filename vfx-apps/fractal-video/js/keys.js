/* Keyframes on parameters, and the per-scene on/off control shared by routes and keyframe lanes.

   Right-click a keyable slider (formula parameters, Space, Ray march, numeric Look values) to keyframe it.
   From then on, in the scenes the lane is on in, moving that slider writes a key at the playhead instead of
   changing the scene, and the slider follows the keyed value as the playhead moves. Keys live in
   project.keys, stored relative to their section; the evaluator applies them after the scene blend and
   before the modulation routes. */
(function () {
  'use strict';
  const FV = (window.FV = window.FV || {});
  const { h } = FV.ui;
  const Schema = window.FVSchema;
  const Ev = window.FVEvaluator;

  const SNAP = 4; // keys snap to quarter beats (hold Alt while dragging a key in the timeline for free placement)
  const registry = new Set(); // { dest, s, label, base }

  const laneOf = (project, dest) => (project.keys || []).find((k) => k.dest === dest);
  // Keyed here = a lane exists and is on in the selected scene.
  function keyedHere(ctx, dest) {
    const lane = laneOf(ctx.project(), dest);
    const scene = ctx.scene();
    return lane && scene && Schema.routeOn(lane, scene.id) ? lane : null;
  }

  function playheadBeat(ctx) {
    const p = ctx.project();
    const total = Ev.totalBeats(p);
    const b = Math.round(Ev.beatAt(p, ctx.playhead()) * SNAP) / SNAP;
    return Math.max(0, Math.min(total, b));
  }

  // Section-relative position of an absolute beat.
  function locate(project, beat) {
    const starts = Ev.sectionStarts(project);
    const i = Ev.sectionIndexAt(project, Math.min(beat, Ev.totalBeats(project) - 1e-9));
    return { sec: project.sections[i].id, b: Math.max(0, beat - starts[i]), index: i };
  }

  function pointAt(project, lane, beat) {
    return Ev.keyPoints(project, lane).find((x) => Math.abs(x.beat - beat) < 0.5 / SNAP);
  }

  function sceneName(project, id) {
    const s = project.scenes.find((x) => x.id === id);
    return s ? s.name : '?';
  }

  // The playhead must be over a scene the lane acts in, or the key would change something else.
  function blockedReason(ctx, lane, beat) {
    const p = ctx.project();
    const ss = Ev.sceneStateAt(p, beat);
    if (Schema.routeOn(lane, ss.a.id) || (ss.b && Schema.routeOn(lane, ss.b.id))) return null;
    const on = p.scenes.filter((s) => Schema.routeOn(lane, s.id)).map((s) => s.name);
    return `These keys act in ${on.length ? on.join(', ') : 'no scene'}; the playhead is in "${ss.section.name}" (${ss.a.name}). Move the playhead there, or switch the keys on in this scene (Modulation tab → Keyframes).`;
  }

  let warned = 0;
  function writeKey(ctx, lane, v) {
    const p = ctx.project();
    const beat = playheadBeat(ctx);
    const why = blockedReason(ctx, lane, beat);
    if (why) {
      if (Date.now() - warned > 4000) { warned = Date.now(); ctx.toast(why, 'warn', 7000); }
      return false;
    }
    const hit = pointAt(p, lane, beat);
    if (hit) hit.q.v = v;
    else {
      const at = locate(p, beat);
      lane.pts.push({ sec: at.sec, b: at.b, v });
    }
    return true;
  }

  /* Called from a slider's onInput. Returns true when the parameter is keyed in the selected scene,
     in which case the value went into a key (or was refused) and the scene must not change. */
  function input(ctx, dest, v) {
    const lane = keyedHere(ctx, dest);
    if (!lane) return false;
    if (writeKey(ctx, lane, v)) {
      ctx.touch();
      if (ctx.redrawTimeline) ctx.redrawTimeline();
    } else {
      sync(ctx, ctx.playhead());
    }
    return true;
  }

  function commit(ctx, dest) {
    if (!keyedHere(ctx, dest)) return false;
    ctx.commit('Keyframe');
    if (ctx.redrawTimeline) ctx.redrawTimeline();
    return true;
  }

  // ---------- context menu ----------
  let menuEl = null;
  function closeMenu() {
    if (menuEl) { menuEl.remove(); menuEl = null; }
    window.removeEventListener('pointerdown', outside, true);
    window.removeEventListener('keydown', onKey, true);
  }
  function outside(e) { if (menuEl && !menuEl.contains(e.target)) closeMenu(); }
  function onKey(e) { if (e.key === 'Escape') closeMenu(); }

  function menu(x, y, items) {
    closeMenu();
    menuEl = h('div', { class: 'ctx-menu', role: 'menu' });
    items.forEach((it) => {
      if (!it) return;
      if (it.sep) { menuEl.appendChild(h('div', { class: 'ctx-sep' })); return; }
      if (it.note) { menuEl.appendChild(h('div', { class: 'ctx-note', text: it.note })); return; }
      const b = h('button', { type: 'button', class: `ctx-item${it.on ? ' on' : ''}`, text: it.label, title: it.title || '' });
      if (it.disabled) b.disabled = true;
      b.addEventListener('click', () => { closeMenu(); it.run(); });
      menuEl.appendChild(b);
    });
    document.body.appendChild(menuEl);
    const r = menuEl.getBoundingClientRect();
    menuEl.style.left = `${Math.max(4, Math.min(x, window.innerWidth - r.width - 4))}px`;
    menuEl.style.top = `${Math.max(4, Math.min(y, window.innerHeight - r.height - 4))}px`;
    setTimeout(() => {
      window.addEventListener('pointerdown', outside, true);
      window.addEventListener('keydown', onKey, true);
    }, 0);
  }

  function changed(ctx, what) {
    ctx.commit(what);
    (ctx.rebuildParts || ctx.rebuild)('shape', 'look', 'mod', 'timeline');
    sync(ctx, ctx.playhead());
  }

  function openMenu(ctx, dest, e) {
    e.preventDefault();
    const p = ctx.project();
    const scene = ctx.scene();
    const spec = Schema.keySpec(dest, scene && scene.formula);
    const lane = laneOf(p, dest);
    const items = [];
    if (!lane) {
      items.push({
        label: `Keyframe "${spec.label}" in ${scene.name}`,
        title: 'From now on, moving this slider writes a key at the playhead (in this scene).',
        run: () => {
          if (!p.keys) p.keys = [];
          p.keys.push({ id: Schema.uid('key'), dest, interp: 'smooth', off: p.scenes.filter((s) => s.id !== scene.id).map((s) => s.id), pts: [] });
          changed(ctx, `Keyframe ${spec.label}`);
          ctx.toast(`"${spec.label}" is keyframed in ${scene.name}. Move the playhead, then the slider: each move sets a key.`, 'ok', 6000);
        }
      });
    } else {
      const here = Schema.routeOn(lane, scene.id);
      const beat = playheadBeat(ctx);
      const hit = pointAt(p, lane, beat);
      items.push({ note: `${lane.pts.length} key${lane.pts.length === 1 ? '' : 's'} · on in ${p.scenes.filter((s) => Schema.routeOn(lane, s.id)).length} of ${p.scenes.length} scenes` });
      if (!here) {
        items.push({ label: `Switch keys on in ${scene.name}`, run: () => { lane.off = lane.off.filter((x) => x !== scene.id); changed(ctx, 'Keyframe scenes'); } });
      } else {
        const cur = registryValue(dest);
        items.push({
          label: hit ? 'Key at playhead: set to this value' : 'Add key at playhead (this value)',
          run: () => { if (writeKey(ctx, lane, cur)) changed(ctx, 'Add keyframe'); }
        });
        if (hit) items.push({ label: 'Delete key at playhead', run: () => { lane.pts.splice(lane.pts.indexOf(hit.q), 1); changed(ctx, 'Delete keyframe'); } });
        items.push({ label: `Switch keys off in ${scene.name}`, title: 'This scene goes back to its own value; the keys stay for the other scenes.', run: () => { lane.off.push(scene.id); changed(ctx, 'Keyframe scenes'); } });
      }
      items.push({ sep: true });
      Schema.KEY_INTERPS.forEach((it) => items.push({ label: `Between keys: ${it.label}`, on: lane.interp === it.id, run: () => { lane.interp = it.id; changed(ctx, 'Keyframe interpolation'); } }));
      items.push({ sep: true });
      items.push({
        label: 'Remove all keys of this parameter',
        title: 'Every scene goes back to its own value.',
        run: () => {
          if (lane.pts.length > 1 && !window.confirm(`Remove all ${lane.pts.length} keys of "${spec.label}"?`)) return;
          p.keys.splice(p.keys.indexOf(lane), 1);
          changed(ctx, 'Remove keyframes');
        }
      });
    }
    menu(e.clientX, e.clientY, items);
  }

  function registryValue(dest) {
    for (const r of registry) if (r.dest === dest && r.s.el.isConnected) return r.s.get();
    return 0;
  }

  /* Hook a slider up: right-click menu, keyed marker, and playhead following.
     base() returns the scene's own value (shown when the parameter has no keys here). */
  function attach(ctx, dest, s, base) {
    if (!Schema.keySpec(dest)) return s;
    const label = s.el.querySelector('.ctl-label');
    s.el.addEventListener('contextmenu', (e) => openMenu(ctx, dest, e));
    const entry = { dest, s, label, base };
    registry.add(entry);
    mark(ctx, entry);
    const lane = keyedHere(ctx, dest);
    if (lane && lane.pts.length) {
      const v = Ev.keyValueAt(ctx.project(), lane, Ev.beatAt(ctx.project(), ctx.playhead()));
      if (v !== null) s.set(v);
    }
    return s;
  }

  function mark(ctx, entry) {
    const lane = laneOf(ctx.project(), entry.dest);
    const here = !!keyedHere(ctx, entry.dest);
    entry.label.classList.toggle('keyed', here);
    entry.label.classList.toggle('keyed-elsewhere', !!lane && !here);
    entry.s.el.classList.toggle('keyed-row', here);
    const base = entry.label.dataset.baseTitle || entry.label.title;
    entry.label.dataset.baseTitle = base;
    entry.label.title = base + (here ? `\nKeyframed: moving it writes a key at the playhead (${lane.pts.length} keys). Right-click for options.`
      : lane ? '\nKeyframed in other scenes. Right-click to switch it on here.' : '\nRight-click to keyframe it.');
  }

  // Keep keyed sliders on the value at the playhead.
  function sync(ctx, t) {
    const p = ctx.project();
    const beat = Ev.beatAt(p, t);
    for (const r of Array.from(registry)) {
      if (!r.s.el.isConnected) { registry.delete(r); continue; }
      const lane = keyedHere(ctx, r.dest);
      if (!lane) continue;
      if (r.s.el.contains(document.activeElement) && document.activeElement.type === 'number') continue;
      const v = lane.pts.length ? Ev.keyValueAt(p, lane, beat) : (r.base ? r.base() : null);
      if (v !== null && v !== undefined && Math.abs(v - r.s.get()) > 1e-9) r.s.set(v);
    }
  }

  /* ---------- per-scene on/off (routes and keyframe lanes) ----------
     row.off lists the scenes the row is switched off in. Returns { chip, strip } to place in a row. */
  function sceneScope(ctx, row, opts) {
    const o = opts || {};
    const project = ctx.project();
    const scene = ctx.scene();
    const sid = scene ? scene.id : null;
    const parts = o.parts || ['mod', 'shape'];
    const rows = o.lanes || [row]; // several lanes switched together (the camera)
    const isOn = (id) => rows.some((r) => Schema.routeOn(r, id));
    const done = (what) => { ctx.commit(what); (ctx.rebuildParts || ctx.rebuild)(...parts); if (o.after) o.after(); };
    const setOn = (id, v) => rows.forEach((r) => { r.off = (r.off || []).filter((x) => x !== id); if (!v) r.off.push(id); });
    const nOn = project.scenes.filter((s) => isOn(s.id)).length;
    const key = `scope:${row.id}`;
    const chip = h('button', {
      type: 'button', class: `mod-scope${nOn === project.scenes.length ? '' : ' partial'}`,
      text: nOn === project.scenes.length ? 'all' : `${nOn}/${project.scenes.length}`,
      title: `Scenes this ${o.noun || 'route'} is on in. Click to choose.`
    });
    const strip = h('div', { class: 'mod-scenes' });
    strip.hidden = !ctx.expanded.has(key);
    project.scenes.forEach((s, i) => {
      const on = isOn(s.id);
      const b = h('button', {
        type: 'button', class: `mod-scene${on ? ' on' : ''}${s.id === sid ? ' cur' : ''}`,
        text: `${i + 1} ${s.name}`, title: `${on ? 'On' : 'Off'} in "${s.name}". Click to switch.`
      });
      b.addEventListener('click', () => { setOn(s.id, !on); done(`${o.noun === 'keyframe lane' ? 'Keyframe' : 'Route'} scenes`); });
      strip.appendChild(b);
    });
    const all = h('button', { type: 'button', class: 'mod-scene-all', text: 'All', title: 'On in every scene' });
    all.addEventListener('click', () => { rows.forEach((r) => { r.off = []; }); done('Scenes: all'); });
    const none = h('button', { type: 'button', class: 'mod-scene-all', text: 'None', title: 'Off in every scene' });
    none.addEventListener('click', () => { rows.forEach((r) => { r.off = project.scenes.map((s) => s.id); }); done('Scenes: none'); });
    strip.appendChild(all);
    strip.appendChild(none);
    chip.addEventListener('click', () => {
      strip.hidden = !strip.hidden;
      if (strip.hidden) ctx.expanded.delete(key); else ctx.expanded.add(key);
    });
    return { chip, strip, setOn, sid, isOn };
  }

  /* ---------- camera ----------
     Each pose value (distance, yaw, pitch, roll, fov, target) is its own lane cam.<k>, so the mouse,
     the Camera sliders and the routes all meet in one place. Auto-orbit is not keyable (it is a rate). */
  // Value the selected scene shows for camera key k at the playhead: the keyed value if keyed here.
  function cameraValue(ctx, k) {
    const scene = ctx.scene();
    const lane = keyedHere(ctx, `cam.${k}`);
    if (lane && lane.pts.length) {
      const v = Ev.keyValueAt(ctx.project(), lane, Ev.beatAt(ctx.project(), ctx.playhead()));
      if (v !== null) return v;
    }
    return scene.camera[k];
  }
  // Set a camera value from the mouse or a button: a key at the playhead when keyed, else the scene.
  function setCamera(ctx, k, v) {
    if (Schema.CAM_KEYS.includes(k) && input(ctx, `cam.${k}`, v)) return true;
    ctx.scene().camera[k] = v;
    return false;
  }
  const cameraKeyed = (ctx) => Schema.CAM_KEYS.some((k) => keyedHere(ctx, `cam.${k}`));

  // Keyframe the whole camera in the selected scene and put a key of the current pose at the playhead.
  function keyCamera(ctx) {
    const p = ctx.project();
    const scene = ctx.scene();
    if (!p.keys) p.keys = [];
    const pose = {};
    Schema.CAM_KEYS.forEach((k) => { pose[k] = cameraValue(ctx, k); });
    let made = 0;
    Schema.CAM_KEYS.forEach((k) => {
      const dest = `cam.${k}`;
      let lane = laneOf(p, dest);
      if (!lane) {
        lane = { id: Schema.uid('key'), dest, interp: 'smooth', off: p.scenes.filter((s) => s.id !== scene.id).map((s) => s.id), pts: [] };
        p.keys.push(lane);
        made += 1;
      } else if (!Schema.routeOn(lane, scene.id)) {
        lane.off = lane.off.filter((x) => x !== scene.id);
        made += 1;
      }
    });
    let ok = true;
    Schema.CAM_KEYS.forEach((k) => { if (!writeKey(ctx, laneOf(p, `cam.${k}`), pose[k])) ok = false; });
    if (!ok && !made) return;
    changed(ctx, 'Key camera');
    if (made && ok) ctx.toast(`The camera is keyframed in ${scene.name}. Move the playhead and orbit, pan or zoom: each move sets a camera key there.`, 'ok', 7000);
  }
  function deleteCameraKeyHere(ctx) {
    const p = ctx.project();
    const beat = playheadBeat(ctx);
    let n = 0;
    Schema.CAM_KEYS.forEach((k) => {
      const lane = keyedHere(ctx, `cam.${k}`);
      const hit = lane && pointAt(p, lane, beat);
      if (hit) { lane.pts.splice(lane.pts.indexOf(hit.q), 1); n += 1; }
    });
    if (n) changed(ctx, 'Delete camera key'); else ctx.toast('No camera key at the playhead.', 'warn');
  }
  function removeCameraKeys(ctx) {
    const p = ctx.project();
    const lanes = (p.keys || []).filter((k) => k.dest.startsWith('cam.'));
    if (!lanes.length) return;
    const n = lanes.reduce((a, l) => a + l.pts.length, 0);
    if (n > 1 && !window.confirm(`Remove all camera keys (${n} keys in ${lanes.length} lanes, every scene)?`)) return;
    p.keys = p.keys.filter((k) => !k.dest.startsWith('cam.'));
    changed(ctx, 'Remove camera keys');
  }

  // Keep lanes consistent when scenes or sections change.
  function sceneDuplicated(project, srcId, copyId) {
    (project.keys || []).forEach((k) => { if (!Schema.routeOn(k, srcId)) k.off.push(copyId); });
  }
  function sceneDeleted(project, id) {
    (project.keys || []).forEach((k) => { k.off = k.off.filter((x) => x !== id); });
  }
  // A section split at `cut` beats: keys at or after the cut move to the new second half.
  function sectionSplit(project, secId, newId, cut) {
    (project.keys || []).forEach((k) => k.pts.forEach((q) => { if (q.sec === secId && q.b >= cut) { q.sec = newId; q.b -= cut; } }));
  }
  // A section removed and its time given to `intoId` (before it if intoBefore, else after it).
  // Every key keeps its absolute position.
  function sectionMerged(project, removedId, removedLen, intoId, intoOldLen, intoBefore) {
    (project.keys || []).forEach((k) => k.pts.forEach((q) => {
      if (q.sec === removedId) { q.sec = intoId; if (intoBefore) q.b += intoOldLen; }
      else if (q.sec === intoId && !intoBefore) q.b += removedLen;
    }));
  }

  FV.Keys = {
    laneOf, keyedHere, input, commit, attach, sync, menu, sceneScope, locate,
    cameraValue, setCamera, cameraKeyed, keyCamera, deleteCameraKeyHere, removeCameraKeys,
    sceneDuplicated, sceneDeleted, sectionSplit, sectionMerged, SNAP
  };
})();
