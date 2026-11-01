/* App glue: state + undo/autosave, audio loading/analysis, render loop, camera interaction,
   safe start, panels, shortcuts. */
(function () {
  'use strict';
  const FV = window.FV;
  const { h, toast, setIcon, fmtTime, download, slug } = FV.ui;
  const Schema = window.FVSchema;
  const Formulas = window.FVFormulas;
  const Ev = window.FVEvaluator;
  const AudioLib = FV.Audio;
  const Panels = FV.Panels;

  const PROJECT_KEY = 'fractal-video-project-v2';
  const LEGACY_KEY = 'fractal-video-project-v1';
  const THUMBS_KEY = 'fv-thumbs';
  const UI_KEY = 'fv-ui';
  const SESSION_KEY = 'fv-session';
  const $ = (id) => document.getElementById(id);

  // ---------- safe start ----------
  // If the previous session did not close cleanly (crash, GPU reset, killed tab), or the page is
  // embedded (VS Code Simple Browser, iframes), the preview waits for a click instead of
  // starting the GPU straight away. This stops a crash -> reload -> crash loop.
  const qs = new URLSearchParams(location.search);
  let safeReason = null;
  try {
    const prev = JSON.parse(localStorage.getItem(SESSION_KEY) || 'null');
    if (prev && prev.clean === false) safeReason = 'crash';
  } catch (err) { /* ignore */ }
  let inFrame = false;
  try { inFrame = window.self !== window.top; } catch (err) { inFrame = true; }
  if (!safeReason && inFrame) safeReason = 'frame';
  if (qs.get('safe') === '1') safeReason = 'url';
  if (qs.get('autostart') === '1') safeReason = null;
  const markSession = (clean) => { try { localStorage.setItem(SESSION_KEY, JSON.stringify({ clean, t: Date.now() })); } catch (err) { /* ignore */ } };
  markSession(false);
  window.addEventListener('pagehide', () => markSession(true));

  // ---------- persistent state ----------
  function loadJSON(key, fallback) {
    try {
      const raw = localStorage.getItem(key);
      return raw ? JSON.parse(raw) : fallback;
    } catch (err) {
      console.warn(`Could not read ${key}`, err);
      return fallback;
    }
  }

  let project = (() => {
    const v2 = loadJSON(PROJECT_KEY, null);
    if (v2) return Schema.normalizeProject(v2);
    const v1 = loadJSON(LEGACY_KEY, null);
    if (v1) return Schema.normalizeProject(v1);
    return Schema.defaultProject();
  })();

  const settings = Object.assign({ tab: 'shape', view: 'scene', quality: 'balanced', guide: true, sceneId: null }, loadJSON(UI_KEY, {}));
  if (safeReason === 'crash') settings.quality = 'draft';
  let thumbs = loadJSON(THUMBS_KEY, {}) || {};
  const thumbDirty = new Set();
  const expanded = new Set();

  let undoStack = [];
  let redoStack = [];
  let lastSnap = JSON.stringify(project);
  let saveTimer = null;
  let quotaWarned = false;

  function saveNow() {
    clearTimeout(saveTimer);
    saveTimer = null;
    try {
      localStorage.setItem(PROJECT_KEY, JSON.stringify(project));
    } catch (err) {
      if (!quotaWarned) { toast('Could not save to browser storage (full?). Export the project JSON to keep it.', 'err'); quotaWarned = true; }
    }
  }
  const scheduleSave = () => { clearTimeout(saveTimer); saveTimer = setTimeout(saveNow, 600); };
  const saveSettings = () => { try { localStorage.setItem(UI_KEY, JSON.stringify(settings)); } catch (err) { /* ignore */ } };
  const saveThumbs = () => {
    const ids = new Set(project.scenes.map((s) => s.id));
    Object.keys(thumbs).forEach((k) => { if (!ids.has(k)) delete thumbs[k]; });
    try { localStorage.setItem(THUMBS_KEY, JSON.stringify(thumbs)); } catch (err) { thumbs = {}; }
  };
  window.addEventListener('pagehide', () => { if (saveTimer) saveNow(); });

  function commit() {
    const snap = JSON.stringify(project);
    if (snap === lastSnap) return;
    undoStack.push(lastSnap);
    if (undoStack.length > 120) undoStack.shift();
    redoStack = [];
    lastSnap = snap;
    if (settings.sceneId) thumbDirty.add(settings.sceneId);
    scheduleSave();
    updateUndoButtons();
    syncBackdropMedia();
    invalidate();
  }

  function touch() {
    if (settings.sceneId) thumbDirty.add(settings.sceneId);
    scheduleSave();
    invalidate();
  }

  function restore(snap) {
    project = Schema.normalizeProject(JSON.parse(snap));
    lastSnap = snap;
    scheduleSave();
    updateUndoButtons();
    syncAudio();
    syncBackdropMedia();
    rebuild('all');
    invalidate();
  }

  function undo() {
    if (!undoStack.length) return;
    redoStack.push(lastSnap);
    restore(undoStack.pop());
  }

  function redo() {
    if (!redoStack.length) return;
    undoStack.push(lastSnap);
    restore(redoStack.pop());
  }

  function updateUndoButtons() {
    $('btnUndo').disabled = !undoStack.length;
    $('btnRedo').disabled = !redoStack.length;
  }

  function replaceProject(next, label) {
    undoStack.push(lastSnap);
    redoStack = [];
    project = Schema.normalizeProject(next);
    lastSnap = JSON.stringify(project);
    settings.sceneId = project.scenes[0].id;
    saveNow();
    updateUndoButtons();
    syncAudio();
    syncBackdropMedia();
    rebuild('all');
    invalidate();
    if (label) toast(label, 'ok');
  }

  // ---------- selection ----------
  const currentScene = () => project.scenes.find((s) => s.id === settings.sceneId) || project.scenes[0];
  if (!project.scenes.some((s) => s.id === settings.sceneId)) settings.sceneId = project.scenes[0].id;

  function selectScene(id, opts) {
    if (!project.scenes.some((s) => s.id === id)) return;
    const changed = settings.sceneId !== id;
    settings.sceneId = id;
    if (!(opts && opts.keepView) && settings.view !== 'scene') setView('scene');
    saveSettings();
    if (changed) rebuild('scenes', 'inspector', 'shape', 'look', 'mod');
    timeline.markHeld();
    invalidate();
  }

  // ---------- audio ----------
  const transport = new AudioLib.Transport();
  const analysis = { rate: AudioLib.RATE, duration: 0, tracks: {}, master: null, stems: {} };
  const audioState = { master: null, stems: {} };
  const attached = { master: null, stems: new Map() };
  let queue = Promise.resolve();
  const enqueue = (job) => { queue = queue.then(job).catch((err) => console.error(err)); return queue; };

  function rebuildTracks() {
    const tr = {};
    const m = analysis.master;
    if (m) {
      tr['master.rms'] = m.env;
      tr['master.low'] = m.low;
      tr['master.mid'] = m.mid;
      tr['master.high'] = m.high;
      tr['master.onset'] = m.onset;
    }
    Object.keys(analysis.stems).forEach((id) => {
      tr[`stem:${id}.env`] = analysis.stems[id].env;
      tr[`stem:${id}.onset`] = analysis.stems[id].onset;
    });
    analysis.tracks = tr;
    analysis.duration = m ? m.duration : Math.max(0, ...Object.values(analysis.stems).map((f) => f.duration));
    transport.duration = Ev.songDuration(project, analysis);
  }

  let audioUiPending = false;
  let audioUiFull = false;
  function refreshAudioUi(full) {
    if (full) audioUiFull = true;
    if (audioUiPending) return;
    audioUiPending = true;
    requestAnimationFrame(() => {
      const doFull = audioUiFull;
      audioUiPending = false;
      audioUiFull = false;
      Panels.buildAudioPane($('audioPane'), ctx);
      if (doFull) rebuild('timeline', 'mod', 'render', 'shape');
    });
  }

  function computeAlignment(id) {
    const st = audioState.stems[id];
    if (st && analysis.master && analysis.stems[id]) st.align = AudioLib.alignment(analysis.stems[id], analysis.master);
  }

  function attachMaster(blob, key) {
    attached.master = key;
    transport.setMaster(blob);
    const st = (audioState.master = { status: 'busy', stage: 'queued', progress: 0 });
    refreshAudioUi();
    return enqueue(async () => {
      if (attached.master !== key) return;
      try {
        const feats = await AudioLib.featuresFor(blob, key, {
          bands: true,
          onStage: (s) => { st.stage = s; refreshAudioUi(); },
          onProgress: (p) => { st.progress = p; refreshAudioUi(); }
        });
        if (attached.master !== key) return;
        analysis.master = feats;
        st.status = 'ready';
        st.duration = feats.duration;
        Object.keys(analysis.stems).forEach(computeAlignment);
      } catch (err) {
        console.error(err);
        st.status = 'error';
        st.error = 'Could not decode this file (try WAV or MP3).';
        toast(`Master: ${st.error}`, 'err');
      }
      rebuildTracks();
      refreshAudioUi(true);
      invalidate();
    });
  }

  function attachStem(id, blob, key) {
    attached.stems.set(id, key);
    transport.setStem(id, blob);
    const st = (audioState.stems[id] = { status: 'busy', stage: 'queued', progress: 0 });
    refreshAudioUi();
    return enqueue(async () => {
      if (attached.stems.get(id) !== key) return;
      try {
        const feats = await AudioLib.featuresFor(blob, key, {
          bands: false,
          onStage: (s) => { st.stage = s; refreshAudioUi(); },
          onProgress: (p) => { st.progress = p; refreshAudioUi(); }
        });
        if (attached.stems.get(id) !== key) return;
        analysis.stems[id] = feats;
        st.status = 'ready';
        computeAlignment(id);
      } catch (err) {
        console.error(err);
        st.status = 'error';
        st.error = 'Could not decode this file.';
      }
      rebuildTracks();
      refreshAudioUi(true);
      invalidate();
    });
  }

  // Bring loaded audio in line with the project (boot, undo, import).
  async function syncAudio() {
    const mKey = project.master ? project.master.key : null;
    if (attached.master !== mKey) {
      attached.master = null;
      analysis.master = null;
      transport.setMaster(null);
      audioState.master = null;
      if (mKey) {
        let blob = null;
        try { blob = await AudioLib.idb.get('files', mKey); } catch (err) { console.warn(err); }
        if (blob && project.master && project.master.key === mKey) attachMaster(blob, mKey);
        else audioState.master = { status: 'missing' };
      }
    }
    const ids = new Set(project.stems.map((s) => s.id));
    Array.from(attached.stems.keys()).forEach((id) => {
      if (!ids.has(id)) {
        attached.stems.delete(id);
        transport.removeStem(id);
        delete analysis.stems[id];
        delete audioState.stems[id];
      }
    });
    for (const stem of project.stems) {
      if (attached.stems.get(stem.id) === stem.key) continue;
      let blob = null;
      try { blob = await AudioLib.idb.get('files', stem.key); } catch (err) { console.warn(err); }
      if (blob) attachStem(stem.id, blob, stem.key);
      else audioState.stems[stem.id] = { status: 'missing' };
    }
    rebuildTracks();
    refreshAudioUi(true);
  }

  async function storeFile(file) {
    const key = AudioLib.fileKey(file);
    try {
      await AudioLib.idb.put('files', key, file);
    } catch (err) {
      console.warn(err);
      toast(`Could not keep ${file.name} in browser storage; you will need to load it again next time.`, 'warn');
    }
    return key;
  }

  async function loadMasterFile(file) {
    const key = await storeFile(file);
    project.master = { name: file.name, key };
    commit();
    attachMaster(file, key);
  }

  async function addStemFiles(files) {
    for (const file of files) {
      const key = await storeFile(file);
      const id = Schema.uid('stem');
      project.stems.push({ id, name: file.name, key, role: Schema.guessRole(file.name) });
      attachStem(id, file, key);
    }
    commit();
    refreshAudioUi(true);
    toast(`${files.length} stem${files.length > 1 ? 's' : ''} added. Set roles, then "Suggest routes" or key them in Shape → Key to music.`, 'ok', 5000);
  }

  function handleAudioFiles(files) {
    const audio = files.filter((f) => /^audio\//.test(f.type) || /\.(wav|mp3|flac|ogg|m4a|aiff?)$/i.test(f.name));
    if (!audio.length) return;
    if (project.master) { addStemFiles(audio); return; }
    const idx = Math.max(0, audio.findIndex((f) => /master|mix|full|bounce/i.test(f.name)));
    const master = audio.splice(idx, 1)[0];
    loadMasterFile(master);
    toast(`${master.name} is the master (soundtrack).`, 'ok');
    if (audio.length) addStemFiles(audio);
  }

  // ---------- renderer ----------
  const canvas = $('glCanvas');
  let renderer = null;
  try {
    renderer = new FV.Renderer(canvas);
  } catch (err) {
    console.error(err);
  }

  // ---------- backdrops (picture / movie behind each scene) ----------
  const backdrops = new FV.Backdrops({
    idb: AudioLib.idb,
    fileKey: AudioLib.fileKey,
    onChange: () => { invalidate(); rebuild('look'); },
    onForget: (key) => { if (renderer) renderer.forgetBackdrop(key); }
  });
  if (renderer) renderer.setMediaSource(backdrops);

  // Load the backdrops the project refers to and free the ones it no longer uses.
  function syncBackdropMedia() {
    const keys = new Set();
    project.scenes.forEach((s) => {
      if (s.backdrop && s.backdrop.kind !== 'none' && s.backdrop.key) {
        keys.add(s.backdrop.key);
        backdrops.ensure(s.backdrop.key, s.backdrop.kind);
      }
    });
    backdrops.items.forEach((m, k) => { if (m.status === 'loading') keys.add(k); });
    backdrops.retain(keys);
  }

  async function setBackdropFile(scene, file) {
    const isVideo = FV.Backdrops.isVideoFile(file);
    if (isVideo) toast(`Opening ${file.name}…`);
    try {
      const m = await backdrops.add(file);
      Object.assign(scene.backdrop, { kind: m.kind, key: m.key, name: file.name, aspect: m.w / m.h, duration: m.duration });
      commit();
      rebuild('look', 'scenes');
      if (!m.stored) toast(`Could not keep ${file.name} in browser storage; load it again next time.`, 'warn', 6000);
      if (m.big) toast('Large movie. A 1080p H.264 MP4 plays back more smoothly.', 'warn', 6000);
      if (settings.tab !== 'look') toast(`Backdrop set for "${scene.name}". Adjust it in Look → Backdrop / overlay.`, 'ok', 5000);
    } catch (err) {
      toast(err.message, 'err');
    }
  }

  let backdropTarget = null;
  function pickBackdrop(scene) {
    backdropTarget = scene.id;
    $('backdropInput').click();
  }

  let frameDirty = true;
  let lastFrame = null;
  let previewStops = null;
  let evalError = null;
  const invalidate = () => { frameDirty = true; };

  function evaluateNow(t) {
    const scene = currentScene();
    const opts = settings.view === 'scene' ? { sceneId: scene.id, modulate: transport.playing, animate: transport.playing } : {};
    let saved = null;
    if (previewStops) { saved = scene.look.palette.stops; scene.look.palette.stops = previewStops; }
    try {
      return Ev.evaluate(project, analysis, t, opts);
    } finally {
      if (saved) scene.look.palette.stops = saved;
    }
  }

  // ---------- HUD ----------
  let liveSceneId = null;
  function updateHud(frame) {
    const sc = project.scenes.find((s) => s.id === frame.sceneAId);
    let text;
    if (settings.view === 'scene') {
      text = `<span class="tag">Scene</span> ${escapeHtml(currentScene().name)}${transport.playing ? ' · reacting to music' : ''}`;
    } else {
      const sec = project.sections[frame.sectionIndex];
      text = `<span class="tag">Timeline</span> ${escapeHtml(sec ? sec.name : '')} · ${escapeHtml(sc ? sc.name : '')}`;
      if (frame.sceneBId) {
        const b = project.scenes.find((s) => s.id === frame.sceneBId);
        text += ` → ${escapeHtml(b ? b.name : '')} ${Math.round(frame.blend * 100)}%`;
      }
    }
    if (evalError) text += ` <span class="err">${escapeHtml(evalError)}</span>`;
    $('hudScene').innerHTML = text;
    const live = settings.view === 'timeline' ? frame.sceneId : null;
    if (live !== liveSceneId) {
      liveSceneId = live;
      document.querySelectorAll('.scene-card').forEach((el, i) => {
        const s = project.scenes[i];
        el.classList.toggle('live', !!s && s.id === liveSceneId);
      });
    }
  }

  const escapeHtml = (s) => String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));

  function onStats(s) {
    const el = $('hudStats');
    let txt = '';
    if (s.mode === 'live') txt = `Live ${Math.round(s.liveScale * 100)}% · ${Math.round(s.fps)} fps`;
    else if (s.mode === 'refine') txt = `Refining ${s.target > 1 ? `${Math.min(s.target, s.progress).toFixed(1)}/${s.target}` : `${Math.round(s.progress * 100)}%`}`;
    else if (s.mode === 'compiling') txt = 'Compiling shader…';
    else if (s.mode === 'error') txt = '<span class="err">Shader error — see console</span>';
    else txt = `${s.size[0]}×${s.size[1]} · ${s.hdr ? 'HDR' : 'LDR'} · idle`;
    el.innerHTML = txt;
  }

  function updateClock(t) {
    $('clockTime').textContent = fmtTime(t, true);
    const beat = Ev.beatAt(project, t);
    const bpb = project.beatsPerBar;
    const bar = Math.floor(beat / bpb);
    const inBar = Math.floor(beat - bar * bpb);
    $('clockBeat').textContent = `bar ${bar + 1}.${inBar + 1} · ${project.bpm} BPM`;
  }

  function updatePlayButton() {
    setIcon($('btnPlay'), transport.playing ? 'pause' : 'play');
  }

  // ---------- frame guide ----------
  function updateGuide() {
    const g = $('frameGuide');
    if (!renderer || !settings.guide) { g.hidden = true; return; }
    const r = renderer.guideRect();
    const vw = canvas.clientWidth;
    const vh = canvas.clientHeight;
    if (Math.abs(r.w - vw) < 2 && Math.abs(r.h - vh) < 2) { g.hidden = true; return; }
    g.hidden = false;
    Object.assign(g.style, { left: `${r.x}px`, top: `${r.y}px`, width: `${r.w}px`, height: `${r.h}px` });
  }

  function aspectChanged() {
    if (renderer) renderer.setAspect(project.render.width / project.render.height);
    updateGuide();
  }

  // ---------- main loop ----------
  let lastMeters = 0;
  let curT = 0;
  function loop(now) {
    requestAnimationFrame(loop);
    transport.update();
    const t = transport.time();
    if (transport.playing || frameDirty) {
      frameDirty = false;
      curT = t;
      try {
        lastFrame = evaluateNow(t);
        evalError = null;
        if (renderer) renderer.setFrame(lastFrame);
        backdrops.sync(lastFrame, transport.playing);
      } catch (err) {
        if (evalError !== err.message) console.error(err);
        evalError = err.message;
      }
      if (lastFrame) updateHud(lastFrame);
      updateClock(t);
      timeline.setTime(t);
      FV.Keys.sync(ctx, t);
    }
    if (settings.tab === 'mod' && now - lastMeters > 70) {
      lastMeters = now;
      Panels.updateModMeters(ctx, curT);
    }
    if (renderer) renderer.tick(now);
  }

  function seek(t) {
    const v = Math.max(0, Math.min(Number(t) || 0, Math.max(transport.duration, Ev.songDuration(project, analysis))));
    transport.seek(v);
    invalidate();
  }

  async function togglePlay() {
    transport.duration = Ev.songDuration(project, analysis);
    if (transport.playing) transport.pause();
    else {
      if (transport.time() >= transport.duration - 0.05) transport.seek(0);
      await transport.play();
      // Scene mode holds one scene while playing; say so once, since it looks like a stuck timeline.
      if (settings.view === 'scene' && !heldHintShown && new Set(project.sections.map((s) => s.sceneId)).size > 1) {
        heldHintShown = true;
        toast(`Scene mode: the preview holds "${currentScene().name}". Press S or click the timeline to play the arrangement.`, '', 6000);
      }
    }
    updatePlayButton();
    invalidate();
  }

  let heldHintShown = false;
  transport.onEnded = () => { updatePlayButton(); invalidate(); };

  // ---------- camera interaction ----------
  function editableScene() {
    if (settings.view === 'timeline' && lastFrame) {
      const id = lastFrame.sceneId;
      if (id !== settings.sceneId) selectScene(id, { keepView: true });
    }
    return currentScene();
  }

  let wheelCommit = null;
  canvas.addEventListener('contextmenu', (e) => e.preventDefault());
  canvas.addEventListener('pointerdown', (e) => {
    if (!renderer || !renderer.enabled) return;
    const scene = editableScene();
    // Work on the pose as shown (keyed values at the playhead), then write each value back:
    // a key at the playhead for keyed camera values, the scene's own value otherwise.
    const cam = {};
    Object.keys(scene.camera).forEach((k) => { cam[k] = Schema.CAM_KEYS.includes(k) ? FV.Keys.cameraValue(ctx, k) : scene.camera[k]; });
    const write = (keys) => { keys.forEach((k) => FV.Keys.setCamera(ctx, k, cam[k])); };
    const pan = e.button === 2 || e.button === 1 || e.shiftKey;
    canvas.setPointerCapture(e.pointerId);
    canvas.classList.add('dragging');
    let lx = e.clientX;
    let ly = e.clientY;
    let moved = false;
    const move = (ev) => {
      const dx = ev.clientX - lx;
      const dy = ev.clientY - ly;
      lx = ev.clientX;
      ly = ev.clientY;
      if (!dx && !dy) return;
      moved = true;
      if (pan) {
        const b = Ev.cameraBasis(cam);
        const k = (2 * cam.dist * b.fovT) / Math.max(100, renderer.guideRect().h);
        cam.tx += (-dx * b.right[0] + dy * b.up[0]) * k;
        cam.ty += (-dx * b.right[1] + dy * b.up[1]) * k;
        cam.tz += (-dx * b.right[2] + dy * b.up[2]) * k;
        ['tx', 'ty', 'tz'].forEach((key) => { cam[key] = Math.max(-10, Math.min(10, cam[key])); });
        write(['tx', 'ty', 'tz']);
      } else {
        cam.yaw = ((cam.yaw - dx * 0.35 + 540) % 360) - 180;
        cam.pitch = Math.max(-89, Math.min(89, cam.pitch + dy * 0.35));
        write(['yaw', 'pitch']);
      }
      Panels.syncCamera(scene);
      touch();
    };
    const up = () => {
      canvas.removeEventListener('pointermove', move);
      canvas.removeEventListener('pointerup', up);
      canvas.removeEventListener('pointercancel', up);
      canvas.classList.remove('dragging');
      if (moved) { commit(); timeline.redraw(); }
    };
    canvas.addEventListener('pointermove', move);
    canvas.addEventListener('pointerup', up);
    canvas.addEventListener('pointercancel', up);
  });
  canvas.addEventListener('wheel', (e) => {
    e.preventDefault();
    if (!renderer || !renderer.enabled) return;
    const scene = editableScene();
    const spec = Schema.CAMERA.dist;
    const d0 = FV.Keys.cameraValue(ctx, 'dist');
    FV.Keys.setCamera(ctx, 'dist', Math.max(spec.min, Math.min(spec.max, d0 * Math.exp(e.deltaY * (e.deltaMode ? 0.05 : 0.0012)))));
    Panels.syncCamera(scene);
    touch();
    clearTimeout(wheelCommit);
    wheelCommit = setTimeout(commit, 400);
  }, { passive: false });
  canvas.addEventListener('dblclick', () => resetCamera());

  function resetCamera() {
    const scene = editableScene();
    const orbit = scene.camera.orbit;
    const d = Schema.defaultCamera(scene.formula);
    // Keyed values get a key of the default pose at the playhead; the rest reset in the scene.
    Object.keys(d).forEach((k) => { if (k !== 'orbit') FV.Keys.setCamera(ctx, k, d[k]); });
    scene.camera.orbit = orbit;
    Panels.syncCamera(scene);
    commit();
    timeline.redraw();
  }

  // ---------- scenes ----------
  function newScene() {
    const base = currentScene();
    const presets = Schema.LOOK_PRESETS;
    const scene = Schema.makeScene(base ? base.formula : 'mandelbulb', `Scene ${project.scenes.length + 1}`, presets[project.scenes.length % presets.length].id);
    project.scenes.push(scene);
    settings.sceneId = scene.id;
    commit();
    rebuild('all');
    ctx.precompile();
  }

  function duplicateScene(id) {
    const src = project.scenes.find((s) => s.id === id);
    if (!src) return;
    const copy = JSON.parse(JSON.stringify(src));
    copy.id = Schema.uid('scene');
    copy.name = `${src.name} copy`.slice(0, 80);
    project.scenes.splice(project.scenes.indexOf(src) + 1, 0, copy);
    // the copy starts with the same routes on and off as its original
    project.mod.forEach((m) => { if (!Schema.routeOn(m, src.id)) m.off.push(copy.id); });
    FV.Keys.sceneDuplicated(project, src.id, copy.id);
    if (project.flight.on.includes(src.id)) project.flight.on.push(copy.id);
    if (thumbs[src.id]) thumbs[copy.id] = thumbs[src.id];
    settings.sceneId = copy.id;
    commit();
    rebuild('all');
  }

  // ---------- auto-flight ----------
  let flightJob = null;
  function flightStatus() {
    const fl = project.flight;
    if (flightJob) return { state: 'busy', progress: flightJob.progress };
    if (!fl.on.length) return { state: 'off' };
    if (!fl.track) return { state: 'none' };
    return { state: fl.track.sig === Ev.flightSig(project) ? 'ready' : 'stale', runs: fl.track.runs.length };
  }
  async function computeFlight(onProgress) {
    if (flightJob) return;
    if (!renderer || !renderer.canProbe()) { toast('Auto-flight needs the GPU preview running (and float render targets).', 'err'); return; }
    if (renderer.job) { toast('Wait for the current render to finish.', 'warn'); return; }
    if (!project.flight.on.length) { toast('Switch auto-flight on for at least one scene first.', 'warn'); return; }
    const job = { cancel: false, progress: 0 };
    flightJob = job;
    const started = performance.now();
    try {
      const res = await FV.Flight.compute(project, analysis, (fr, pos, dirs, maxRay, eps) => renderer.probe(fr, pos, dirs, maxRay, eps), {
        onProgress: (x) => { job.progress = x; if (onProgress) onProgress(x); },
        cancelled: () => job.cancel || !renderer || renderer.lost
      });
      if (!res || job.cancel) { toast('Flight computation stopped.', 'warn'); return; }
      project.flight.track = res.track;
      const secs = ((performance.now() - started) / 1000).toFixed(1);
      toast(`Flight computed in ${secs} s (${res.stats.steps} steps). Play the timeline to watch it.`, 'ok', 6000);
      commit();
      if (settings.view !== 'timeline') setView('timeline');
    } catch (err) {
      console.error(err);
      toast(`Flight failed: ${err.message}`, 'err', 7000);
    } finally {
      flightJob = null;
      rebuild('shape');
      invalidate();
    }
  }
  const cancelFlight = () => { if (flightJob) flightJob.cancel = true; };

  function deleteScene(id) {
    if (project.scenes.length <= 1) { toast('A project needs at least one scene.', 'warn'); return; }
    const scene = project.scenes.find((s) => s.id === id);
    const used = project.sections.filter((s) => s.sceneId === id);
    const msg = used.length ? `Delete "${scene.name}"? ${used.length} section(s) use it and will switch to another scene.` : `Delete "${scene.name}"?`;
    if (!window.confirm(msg)) return;
    project.scenes.splice(project.scenes.indexOf(scene), 1);
    used.forEach((s) => { s.sceneId = project.scenes[0].id; });
    project.mod.forEach((m) => { m.off = (m.off || []).filter((x) => x !== id); });
    FV.Keys.sceneDeleted(project, id);
    project.flight.on = project.flight.on.filter((x) => x !== id);
    delete thumbs[id];
    saveThumbs();
    if (settings.sceneId === id) settings.sceneId = project.scenes[0].id;
    commit();
    rebuild('all');
  }

  // ---------- thumbnails ----------
  function captureThumb(frame) {
    const id = frame.sceneId;
    if (frame.sceneBId && frame.blend > 0 && frame.blend < 1) return;
    if (thumbs[id] && !thumbDirty.has(id)) return;
    if (previewStops) return;
    const r = renderer.guideRect();
    const sx = canvas.width / Math.max(1, canvas.clientWidth);
    const c = document.createElement('canvas');
    c.width = 160;
    c.height = 90;
    const g = c.getContext('2d');
    const srcAspect = r.w / r.h;
    let w = r.w;
    let hh = r.h;
    if (srcAspect > 16 / 9) w = hh * 16 / 9; else hh = w * 9 / 16;
    g.drawImage(canvas, (r.x + (r.w - w) / 2) * sx, (r.y + (r.h - hh) / 2) * sx, w * sx, hh * sx, 0, 0, 160, 90);
    try {
      thumbs[id] = c.toDataURL('image/jpeg', 0.72);
      thumbDirty.delete(id);
      saveThumbs();
      const card = document.querySelectorAll('.scene-card')[project.scenes.findIndex((s) => s.id === id)];
      const th = card && card.querySelector('.scene-thumb');
      if (th) { th.style.background = ''; th.style.backgroundImage = `url(${thumbs[id]})`; }
    } catch (err) { /* tainted or quota: ignore */ }
  }

  // ---------- tempo ----------
  function detectTempo() {
    if (!analysis.master) { toast('Load a master first.', 'warn'); return; }
    const s = AudioLib.suggestTempo(analysis.master);
    if (!s) { toast('The master is too short to estimate a tempo.', 'warn'); return; }
    if (window.confirm(`Suggested: ${s.bpm} BPM, first downbeat near ${s.offset.toFixed(2)} s.\nThis is only an estimate — check it against the waveform. Apply?`)) {
      project.bpm = s.bpm;
      project.offset = s.offset;
      commit();
      rebuild('audio', 'timeline', 'mod', 'render');
    }
  }

  let taps = [];
  function tapTempo() {
    const now = performance.now();
    taps = taps.filter((t) => now - t < 3000);
    taps.push(now);
    if (taps.length < 4) { toast(`Tap ${4 - taps.length} more…`); return; }
    const iv = [];
    for (let i = 1; i < taps.length; i += 1) iv.push(taps[i] - taps[i - 1]);
    const bpm = Math.round((60000 / (iv.reduce((a, b) => a + b, 0) / iv.length)) * 10) / 10;
    project.bpm = Math.min(400, Math.max(20, bpm));
    commit();
    rebuild('audio', 'timeline', 'mod', 'render');
    toast(`${project.bpm} BPM`);
  }

  function suggestRoutes() {
    if (!project.stems.length) { toast('Add stems first (Audio panel).', 'warn'); return; }
    let added = 0;
    project.stems.forEach((stem) => {
      (Schema.ROLE_ROUTES[stem.role] || []).forEach((r) => {
        const source = `stem:${stem.id}.${r.f}`;
        if (project.mod.some((m) => m.source === source && m.dest === r.dest)) return;
        project.mod.push({ id: Schema.uid('mod'), off: [], source, dest: r.dest, depth: r.depth, attack: r.attack || 0, release: r.release || 0, lead: r.lead || 0 });
        added += 1;
      });
    });
    if (!added) { toast('No new routes: set stem roles first (OTHER has no suggestions).', 'warn'); return; }
    commit();
    rebuild('mod', 'shape');
    toast(`${added} route${added > 1 ? 's' : ''} added from stem roles.`, 'ok');
  }

  function vrOpts(vr, frame) {
    if (!vr) return null;
    const dist = frame && frame.camera && frame.camera.dist > 0 ? frame.camera.dist : 1;
    return { layout: vr.layout, span: vr.span, ipd: vr.depth * dist, level: vr.level };
  }

  // ---------- still render ----------
  // asVr: one frame in the movie's VR format (to check the stereo before a long render)
  async function renderStill(asVr) {
    if (!renderer) return;
    if (transport.playing) { transport.pause(); updatePlayButton(); }
    const r = project.render;
    const frame = evaluateNow(transport.time());
    const modal = $('renderModal');
    const bar = $('renderProgress');
    const R = asVr === true ? movieRange() : null;
    const vr = R && R.vr;
    const sw = vr ? R.w : r.width;
    const sh = vr ? R.h : r.height;
    $('renderModalTitle').textContent = `Rendering ${vr ? 'VR still' : 'still'} ${sw}×${sh}`;
    $('renderModalText').textContent = `${r.samples} samples per pixel, High quality. The preview pauses meanwhile.`;
    $('renderPreview').hidden = true;
    $('renderStats').textContent = '';
    bar.style.width = '0%';
    modal.hidden = false;
    try {
      await backdrops.prepare(frame);
      const blob = await renderer.renderStill(frame, sw, sh, r.samples, (p) => { bar.style.width = `${Math.round(p * 100)}%`; }, false, vr ? { vr: vrOpts(vr, frame) } : null);
      const frameNo = Math.round(transport.time() * r.fps);
      download(`${slug(project.name)}-f${String(frameNo).padStart(5, '0')}${vr ? vr.tag : ''}.png`, blob);
      toast('Still saved to your downloads.', 'ok');
    } catch (err) {
      if (err.message !== 'cancelled') toast(`Still render failed: ${err.message}`, 'err');
    } finally {
      modal.hidden = true;
      invalidate();
    }
  }
  $('renderCancel').addEventListener('click', () => {
    if (movieJob) movieJob.cancelled = true;
    if (renderer) renderer.cancelStill();
  });

  // ---------- movie render ----------
  // Each frame is evaluated from song time, rendered in tiles at High quality (like the still), read
  // back and posted to the local server, which pipes it into ffmpeg with the master as soundtrack.
  // Upload of frame n overlaps with rendering of frame n+1.
  let movieJob = null;

  async function renderApi(op, opts) {
    const o = opts || {};
    const q = new URLSearchParams(o.query || {}).toString();
    const headers = { 'X-FV-Render': '1' };
    if (o.json) headers['Content-Type'] = 'application/json';
    let res;
    try {
      res = await fetch(`/render/${op}${q ? `?${q}` : ''}`, { method: o.method || 'POST', headers, body: o.json ? JSON.stringify(o.json) : o.body });
    } catch (err) {
      throw new Error('The local server is not reachable. Double-click "Start Fractal Video.cmd" in the Fractal Video folder (or run "npm start" there) and open http://localhost:4173/.');
    }
    let data = null;
    try { data = await res.json(); } catch (err) { data = null; }
    if (!res.ok || !data) {
      if (res.status === 404 && !data) throw new Error('This server cannot render movies. Start the app with "npm start" in the Fractal Video folder (it was updated) and reload.');
      throw new Error((data && data.error) || `server error ${res.status}`);
    }
    return data;
  }

  let renderCheckCache = null;
  async function renderCheck(force) {
    if (renderCheckCache && !force) return renderCheckCache;
    try {
      renderCheckCache = await renderApi('check', { method: 'GET' });
    } catch (err) {
      renderCheckCache = { error: err.message };
    }
    return renderCheckCache;
  }

  const movieSettings = () => {
    const m = Object.assign({ start: 0, end: null, crf: 16, draft: false, audio: 'aac', preset: null, vr: 'off', vrWidth: null, vrDepth: 0.03, vrLevel: false }, settings.movie || {});
    const vf = Schema.VR_FORMATS.find((x) => x.id === m.vr) || Schema.VR_FORMATS[0];
    m.vr = vf.id;
    if (vf.widths && !vf.widths.includes(m.vrWidth)) m.vrWidth = vf.widths[0];
    if (!Schema.MOVIE_PRESETS.some((x) => x.id === m.preset)) m.preset = m.draft ? 'test' : 'final'; // older settings
    return m;
  };
  function setMovieSettings(patch) {
    settings.movie = Object.assign(movieSettings(), patch);
    saveSettings();
  }

  const fmtDur = (s) => {
    if (!Number.isFinite(s)) return '…';
    const hh = Math.floor(s / 3600);
    const mm = Math.floor((s % 3600) / 60);
    const ss = Math.floor(s % 60);
    return hh ? `${hh} h ${String(mm).padStart(2, '0')} min` : mm ? `${mm} min ${String(ss).padStart(2, '0')} s` : `${ss} s`;
  };

  function movieRange() {
    const r = project.render;
    const m = movieSettings();
    const dur = Ev.songDuration(project, analysis);
    const start = Math.min(Math.max(0, Number(m.start) || 0), Math.max(0, dur - 1 / r.fps));
    const end = m.end == null ? dur : Math.min(dur, Math.max(start + 1 / r.fps, Number(m.end)));
    const n0 = Math.round(start * r.fps);
    const n1 = Math.max(n0 + 1, Math.ceil(end * r.fps - 1e-6));
    let w = r.width;
    let hh = r.height;
    const vf = Schema.VR_FORMATS.find((x) => x.id === m.vr);
    if (vf.size) [w, hh] = vf.size(m.vrWidth);
    const q = Schema.MOVIE_PRESETS.find((x) => x.id === m.preset);
    w = Math.round(w * q.scale);
    hh = Math.round(hh * q.scale);
    w -= w % 2;
    hh -= hh % 2;
    return { n0, n1, frames: n1 - n0, fps: r.fps, start: n0 / r.fps, end: n1 / r.fps, w, h: hh, samples: q.samples || r.samples, crf: q.crf != null ? q.crf : m.crf, draft: q.id !== 'final', preset: q, march: q.march, audio: m.audio === 'pcm' ? 'pcm' : 'aac',
      vr: vf.size ? { id: vf.id, label: vf.label, layout: vf.layout, span: vf.span, stereo: vf.stereo, bounds: vf.bounds, tag: vf.tag, depth: vf.stereo ? m.vrDepth : 0, level: !!m.vrLevel } : null };
  }

  async function renderMovie() {
    if (movieJob) return;
    if (!renderer || !renderer.enabled || renderer.lost) { toast('Start the preview first: the movie uses the same GPU renderer.', 'warn'); return; }
    const chk = await renderCheck(true);
    if (chk.error) { toast(chk.error, 'err', 8000); return; }
    if (!chk.ffmpeg) { toast('ffmpeg was not found by the server. Install it (winget install Gyan.FFmpeg) and restart "npm start".', 'err', 8000); return; }
    if (transport.playing) { transport.pause(); updatePlayButton(); }
    saveNow();

    const R = movieRange();
    const job = { cancelled: false, id: null };
    movieJob = job;
    const modal = $('renderModal');
    const bar = $('renderProgress');
    const stats = $('renderStats');
    const prev = $('renderPreview');
    prev.hidden = false;
    prev.width = 320;
    prev.height = Math.max(2, Math.round(320 * R.h / R.w));
    prev.getContext('2d').clearRect(0, 0, prev.width, prev.height);
    $('renderModalTitle').textContent = `Rendering ${R.vr ? `VR movie (${R.vr.label})` : 'movie'} ${R.w}×${R.h} · ${R.preset.label}`;
    $('renderModalText').textContent = `${R.frames.toLocaleString()} frames at ${R.fps} fps (${fmtTime(R.start)}–${fmtTime(R.end)}), ${R.samples} sample${R.samples === 1 ? "" : "s"} per pixel, ${R.march} ray marching. Keep this tab visible: browsers pause hidden tabs.`;
    stats.textContent = 'Starting…';
    bar.style.width = '0%';
    modal.hidden = false;
    renderer.setHold(true);
    let wake = null;
    try { wake = navigator.wakeLock ? await navigator.wakeLock.request('screen') : null; } catch (err) { wake = null; }

    const t0 = performance.now();
    let lastPreview = 0;
    const showPreview = async (px) => {
      const now = performance.now();
      if (now - lastPreview < 700) return;
      lastPreview = now;
      try {
        const img = new ImageData(new Uint8ClampedArray(px.buffer, px.byteOffset, px.byteLength), R.w, R.h);
        const bmp = await createImageBitmap(img, { imageOrientation: 'flipY', resizeWidth: prev.width, resizeHeight: prev.height });
        prev.getContext('2d').drawImage(bmp, 0, 0);
        bmp.close();
      } catch (err) { /* preview only */ }
    };
    const showStats = (done) => {
      const el = (performance.now() - t0) / 1000;
      const per = done > 0 ? el / done : NaN;
      bar.style.width = `${Math.min(100, (done / R.frames) * 100).toFixed(1)}%`;
      stats.textContent = `Frame ${Math.min(R.frames, Math.floor(done) + 1).toLocaleString()} of ${R.frames.toLocaleString()} · ${Number.isFinite(per) ? `${per.toFixed(1)} s per frame` : 'measuring…'} · elapsed ${fmtDur(el)} · left about ${fmtDur(per * (R.frames - done))}`;
    };

    let pending = Promise.resolve();
    try {
      const start = await renderApi('start', { json: { name: `${slug(project.name)}${R.preset.suffix}`, width: R.w, height: R.h, fps: R.fps, frames: R.frames, startTime: R.start, outDir: project.render.outDir, crf: R.crf, audio: R.audio, vr: R.vr ? { stereo: R.vr.stereo, bounds: R.vr.bounds, tag: R.vr.tag } : null } });
      job.id = start.id;
      if (project.master && project.master.key) {
        const blob = await AudioLib.idb.get('files', project.master.key).catch(() => null);
        if (blob) {
          stats.textContent = 'Sending the soundtrack…';
          const ext = (project.master.name.match(/\.([a-z0-9]{1,5})$/i) || [null, 'wav'])[1];
          await renderApi('audio', { query: { id: job.id, ext }, body: blob });
        } else toast('The master is not stored in this browser; the movie will be silent. Load it again to include it.', 'warn', 8000);
      }
      for (let i = 0; i < R.frames; i += 1) {
        if (job.cancelled) throw new Error('cancelled');
        const t = (R.n0 + i) / R.fps;
        const frame = Ev.evaluate(project, analysis, t);
        await backdrops.prepare(frame);
        const px = await renderer.renderFrame(frame, R.w, R.h, R.samples, (p) => showStats(i + p), { march: R.march, vr: vrOpts(R.vr, frame) });
        await pending; // previous upload done (ffmpeg keeps up or applies back-pressure)
        if (job.cancelled) throw new Error('cancelled');
        pending = renderApi('frame', { query: { id: job.id, n: i }, body: px });
        pending.catch(() => {});
        showPreview(px);
        showStats(i + 1);
      }
      await pending;
      stats.textContent = 'Finishing the file…';
      const res = await renderApi('finish', { query: { id: job.id } });
      const mb = (res.size / 1048576).toFixed(1);
      const sound = res.audio === 'pcm' ? ' Soundtrack copied untouched (PCM).'
        : res.audio && res.gain < -0.005 ? ` Soundtrack lowered by ${(-res.gain).toFixed(1)} dB (static gain, no compression) so the AAC encode stays below 0 dB${res.aacPeak != null ? ` (peak ${res.aacPeak.toFixed(1)} dBFS)` : ''}.`
        : res.audio ? ' Soundtrack level unchanged.' : '';
      const vrNote = res.vr ? ' VR metadata written (YouTube and headset players will show it as VR).' : '';
      toast(`Movie saved (${mb} MB, ${fmtDur((performance.now() - t0) / 1000)}): ${res.file}.${sound}${vrNote}`, 'ok', 15000);
      console.log('Movie saved:', res.file);
      lastMovie = res;
    } catch (err) {
      await pending.catch(() => {});
      if (job.id) renderApi('cancel', { query: { id: job.id } }).catch(() => {});
      if (err.message !== 'cancelled') toast(`Movie render failed: ${err.message}`, 'err', 10000);
      else toast('Movie render cancelled.', 'warn');
    } finally {
      movieJob = null;
      if (wake) wake.release().catch(() => {});
      renderer.setHold(false);
      modal.hidden = true;
      prev.hidden = true;
      invalidate();
      rebuild('render');
    }
  }
  let lastMovie = null;

  // ---------- live VR (WebXR, e.g. Quest over Horizon Link) ----------
  // The preview's own GL context renders both eyes straight into the headset's framebuffer, at a reduced
  // resolution and ray-march quality. The scene camera is the centre of the room; the head moves and turns
  // inside it (metres scaled by Stereo depth x camera distance / 6.4 cm, the movie's eye spacing rule).
  // The trigger plays / pauses the music; the preview on the PC pauses while the headset is in use.
  let xr = null;
  const liveVrSettings = () => Object.assign({ scale: 0.5, march: 'draft', level: true }, settings.liveVr || {});
  function setLiveVrSettings(patch) {
    settings.liveVr = Object.assign(liveVrSettings(), patch);
    saveSettings();
  }
  async function liveVrSupport() {
    if (!window.isSecureContext) return { ok: false, why: 'WebXR needs http://localhost (not a file or a network address).' };
    if (!navigator.xr) return { ok: false, why: 'This browser has no WebXR. Use Chrome or Edge on this PC.' };
    try {
      if (await navigator.xr.isSessionSupported('immersive-vr')) return { ok: true };
    } catch (err) { return { ok: false, why: err.message }; }
    return { ok: false, why: 'No VR headset found. Connect the Quest with Horizon Link or Air Link, make Meta Horizon Link the active OpenXR runtime (Link app on the PC > Settings > General), then reload this page.' };
  }
  async function startLiveVr() {
    if (xr) { xr.session.end().catch(() => {}); return; }
    if (movieJob) { toast('A movie is rendering.', 'warn'); return; }
    if (!renderer || renderer.lost || !renderer.enabled) { toast('Start the preview first: live VR uses the same GPU renderer.', 'warn'); return; }
    const sup = await liveVrSupport();
    if (!sup.ok) { toast(sup.why, 'warn', 12000); return; }
    const gl = renderer.gl;
    let session;
    try {
      await gl.makeXRCompatible();
      session = await navigator.xr.requestSession('immersive-vr');
    } catch (err) { toast(`VR did not start: ${err.message}`, 'err', 10000); return; }
    const vs = liveVrSettings();
    let layer;
    let ref;
    try {
      layer = new XRWebGLLayer(session, gl, { framebufferScaleFactor: vs.scale, antialias: false, depth: false, stencil: false, alpha: false });
      session.updateRenderState({ baseLayer: layer });
      ref = await session.requestReferenceSpace('local');
    } catch (err) {
      session.end().catch(() => {});
      toast(`VR did not start: ${err.message}`, 'err', 10000);
      return;
    }
    const job = { session, layer, frames: 0, t0: performance.now(), fps: 0, ms: 0, size: '' };
    xr = job;
    renderer.setHold(true);
    session.addEventListener('select', () => { togglePlay(); });
    session.addEventListener('end', () => {
      if (xr === job) xr = null;
      renderer.setHold(false);
      invalidate();
      rebuild('render');
    });
    let msSum = 0;
    const onFrame = (time, xf) => {
      if (xr !== job) return;
      session.requestAnimationFrame(onFrame);
      const pose = xf.getViewerPose(ref);
      if (!pose) return;
      const t1 = performance.now();
      const set = liveVrSettings();
      const frame = evaluateNow(transport.time());
      const dist = frame.camera && frame.camera.dist > 0 ? frame.camera.dist : 1;
      const scale = (movieSettings().vrDepth * dist) / 0.064;
      pose.views.forEach((v) => {
        const vp = layer.getViewport(v);
        renderer.renderXR(frame, v, vp, layer.framebuffer, { scale, level: set.level, quality: set.march });
        job.size = `${vp.width}×${vp.height}`;
      });
      msSum += performance.now() - t1;
      job.frames += 1;
      const el = performance.now() - job.t0;
      if (el > 1000) {
        job.fps = (job.frames * 1000) / el;
        job.ms = msSum / job.frames;
        job.frames = 0;
        msSum = 0;
        job.t0 = performance.now();
        const st = document.getElementById('liveVrStatus');
        if (st) st.textContent = `In VR: ${job.fps.toFixed(0)} frames/s, ${job.size} per eye. Trigger = play / pause. Take the headset off or press the Quest menu button to leave.`;
      }
    };
    session.requestAnimationFrame(onFrame);
    rebuild('render');
  }

  window.addEventListener('beforeunload', (e) => { if (movieJob) { e.preventDefault(); e.returnValue = ''; } });

  function exportProject() {
    saveNow();
    download(`${slug(project.name)}.json`, new Blob([JSON.stringify(project, null, 2)], { type: 'application/json' }));
  }

  // ---------- context for panels ----------
  const ctx = {
    project: () => project,
    scene: currentScene,
    selectScene,
    duplicateScene,
    deleteScene,
    touch,
    commit,
    rebuild: (...parts) => rebuild(...(parts.length ? parts : [settings.tab])),
    toast,
    thumbs: null,
    expanded,
    settings,
    analysis: () => analysis,
    audioState: () => audioState,
    liveSceneId: () => liveSceneId,
    playing: () => transport.playing,
    seek,
    view: () => settings.view,
    setView: (v) => setView(v),
    soloId: () => transport.solo,
    solo: (id) => { transport.setSolo(transport.solo === id ? null : id); refreshAudioUi(); },
    pickMaster: () => $('masterInput').click(),
    pickStems: () => $('stemInput').click(),
    removeMaster: () => {
      if (!project.master) return;
      const key = project.master.key;
      project.master = null;
      commit();
      syncAudio();
      if (key) AudioLib.idb.del('files', key).catch(() => {});
    },
    removeStem: (id) => {
      const s = project.stems.find((x) => x.id === id);
      if (!s) return;
      const routes = project.mod.filter((m) => m.source.startsWith(`stem:${id}.`)).length;
      if (routes && !window.confirm(`Remove ${s.name}? ${routes} modulation route(s) use it and will be removed too.`)) return;
      project.stems.splice(project.stems.indexOf(s), 1);
      project.mod = project.mod.filter((m) => !m.source.startsWith(`stem:${id}.`));
      commit();
      syncAudio();
      rebuild('mod', 'shape');
      if (!project.stems.some((x) => x.key === s.key)) AudioLib.idb.del('files', s.key).catch(() => {});
    },
    detectTempo,
    tapTempo,
    suggestRoutes,
    resetCamera,
    flightStatus,
    computeFlight,
    cancelFlight,
    renderStill,
    renderMovie,
    renderCheck,
    movieSettings,
    setMovieSettings,
    movieRange,
    lastMovie: () => lastMovie,
    liveVrSettings,
    setLiveVrSettings,
    liveVrSupport,
    startLiveVr,
    liveVrActive: () => !!xr,
    playhead: () => transport.time(),
    exportProject,
    aspectChanged,
    setGuide: (v) => { settings.guide = v; saveSettings(); updateGuide(); },
    backdrops,
    pickBackdrop,
    removeBackdrop: (scene) => {
      scene.backdrop = Schema.defaultBackdrop();
      commit();
      rebuild('look', 'scenes');
    },
    backdropToAll: (scene) => {
      project.scenes.forEach((s) => { if (s !== scene) s.backdrop = JSON.parse(JSON.stringify(scene.backdrop)); });
      commit();
      toast('Backdrop copied to every scene.', 'ok');
    },
    precompile: () => { if (renderer) renderer.precompile(Array.from(new Set(project.scenes.map((s) => s.formula)))); },
    previewStops: (stops) => { previewStops = stops ? stops.map((s) => ({ ...s })) : null; invalidate(); },
    paletteChanged: () => {
      const chip = $('inspectorChip');
      const sc = currentScene();
      const css = window.FVPalettes.css(sc.look.palette.stops, sc.look.palette.interp);
      if (chip) chip.style.background = css;
      const card = document.querySelectorAll('.scene-card')[project.scenes.indexOf(sc)];
      const strip = card && card.querySelector('.scene-pal');
      if (strip) strip.style.background = css;
    },
    onDeleteKey: (fn) => { deleteKeyHandler = fn; },
    redrawTimeline: () => timeline.redraw(),
    rebuildParts: (...parts) => rebuild(...parts)
  };
  Object.defineProperty(ctx, 'thumbs', { get: () => thumbs });
  let deleteKeyHandler = null;

  // ---------- panels ----------
  const timeline = new FV.Timeline($('timeline'), ctx);
  const TAB_BUILDERS = {
    shape: (el) => Panels.buildShapeTab(el, ctx),
    look: (el) => { deleteKeyHandler = null; FV.LookTab.build(el, Object.assign({}, ctx, { rebuild: () => rebuild('look', 'scenes', 'timeline', 'inspector') })); },
    mod: (el) => Panels.buildModTab(el, ctx),
    render: (el) => Panels.buildRenderTab(el, ctx)
  };
  const TAB_EL = { shape: 'tabShape', look: 'tabLook', mod: 'tabMod', render: 'tabRender' };
  const staleTabs = new Set(Object.keys(TAB_BUILDERS));

  function rebuild(...parts) {
    const all = parts.includes('all');
    const want = (p) => all || parts.includes(p);
    if (want('scenes')) Panels.buildSceneList($('sceneList'), ctx);
    if (want('inspector')) Panels.buildInspectorHead($('inspectorHead'), ctx, settings.tab);
    if (want('audio')) Panels.buildAudioPane($('audioPane'), ctx);
    if (want('timeline')) timeline.refresh();
    Object.keys(TAB_BUILDERS).forEach((tab) => {
      if (!want(tab)) return;
      if (tab === settings.tab) { TAB_BUILDERS[tab]($(TAB_EL[tab])); staleTabs.delete(tab); } else staleTabs.add(tab);
    });
    if (all) {
      $('projectName').value = project.name;
      aspectChanged();
    }
  }

  function setTab(tab) {
    if (!TAB_BUILDERS[tab]) tab = 'shape';
    settings.tab = tab;
    saveSettings();
    document.querySelectorAll('#tabs button').forEach((b) => b.classList.toggle('on', b.dataset.tab === tab));
    document.querySelectorAll('.tab-page').forEach((p) => p.classList.toggle('on', p.dataset.tab === tab));
    if (staleTabs.has(tab)) { TAB_BUILDERS[tab]($(TAB_EL[tab])); staleTabs.delete(tab); }
    Panels.buildInspectorHead($('inspectorHead'), ctx, tab);
    $('tabBody').scrollTop = 0;
  }

  function setView(view) {
    settings.view = view === 'timeline' ? 'timeline' : 'scene';
    saveSettings();
    document.querySelectorAll('#viewMode button').forEach((b) => b.classList.toggle('on', b.dataset.mode === settings.view));
    if (settings.view === 'scene') {
      liveSceneId = null;
      document.querySelectorAll('.scene-card.live').forEach((el) => el.classList.remove('live'));
    }
    timeline.markHeld();
    invalidate();
  }

  // ---------- overlay (safe start, GPU loss) ----------
  function showOverlay(title, text, primary, showDraft) {
    $('overlayTitle').textContent = title;
    $('overlayText').textContent = text;
    $('overlayBtn').textContent = primary;
    $('overlayDraftBtn').hidden = !showDraft;
    $('overlay').hidden = false;
  }

  function startPreview(quality) {
    $('overlay').hidden = true;
    if (!renderer) return;
    if (quality) { settings.quality = quality; $('qualitySelect').value = quality; renderer.setQuality(quality); saveSettings(); }
    renderer.restart();
    ctx.precompile();
    invalidate();
  }
  $('overlayBtn').addEventListener('click', () => startPreview());
  $('overlayDraftBtn').addEventListener('click', () => startPreview('draft'));

  if (renderer) {
    renderer.on('stats', onStats);
    renderer.on('refined', (frame) => captureThumb(frame));
    renderer.on('lost', ({ count }) => {
      showOverlay('GPU context lost', count >= 2
        ? 'The graphics driver reset twice. The preview is stopped so it cannot take the browser down. Restart it in Draft quality, or lower the iterations of the current scene first.'
        : 'The graphics driver reset. The preview will resume in Draft quality.', 'Restart preview', false);
    });
    renderer.on('restored', ({ enabled }) => {
      settings.quality = renderer.quality;
      $('qualitySelect').value = renderer.quality;
      saveSettings();
      if (enabled) { $('overlay').hidden = true; toast('Preview recovered in Draft quality.', 'warn'); }
    });
    if (safeReason) {
      renderer.setEnabled(false);
      const text = {
        crash: 'The last session did not close normally (crash, GPU reset or killed tab), so the preview did not start on its own. Start it when ready; Draft is the safest.',
        frame: 'This page is embedded (e.g. a VS Code preview), so the GPU preview waits for a click. Opening it in a normal browser tab works best.',
        url: 'Started with ?safe=1.'
      }[safeReason];
      showOverlay('Preview paused', text, 'Start preview', true);
    }
  } else {
    showOverlay('WebGL2 is not available', 'This browser or GPU cannot run the fractal preview. Editing still works; try Chrome or Edge with hardware acceleration on.', 'Retry', false);
    $('overlayBtn').addEventListener('click', () => location.reload());
  }

  // ---------- top bar ----------
  setIcon($('btnHome'), 'home');
  setIcon($('btnUndo'), 'undo');
  setIcon($('btnRedo'), 'redo');
  setIcon($('btnHelp'), 'help');
  setIcon($('btnNewScene'), 'plus', ' New');
  setIcon($('btnDupScene'), 'copy');
  document.querySelectorAll('[data-close]').forEach((b) => { setIcon(b, 'close'); b.addEventListener('click', () => { b.closest('.modal').hidden = true; }); });
  updatePlayButton();
  updateUndoButtons();

  const qSel = $('qualitySelect');
  Object.keys(FV.Renderer.QUALITY).forEach((k) => qSel.appendChild(h('option', { value: k, text: FV.Renderer.QUALITY[k].label })));
  qSel.value = settings.quality;
  if (renderer) renderer.setQuality(settings.quality);
  qSel.addEventListener('change', () => { settings.quality = qSel.value; saveSettings(); if (renderer) renderer.setQuality(qSel.value); });

  $('projectName').value = project.name;
  $('projectName').addEventListener('change', () => { project.name = $('projectName').value.trim().slice(0, 120) || 'Untitled video'; commit(); });
  $('projectName').addEventListener('keydown', (e) => { if (e.key === 'Enter') e.target.blur(); });
  $('btnPlay').addEventListener('click', togglePlay);
  $('btnHome').addEventListener('click', () => seek(0));
  $('btnUndo').addEventListener('click', undo);
  $('btnRedo').addEventListener('click', redo);
  $('btnHelp').addEventListener('click', () => { $('helpModal').hidden = false; });
  $('btnNewScene').addEventListener('click', newScene);
  $('btnDupScene').addEventListener('click', () => duplicateScene(settings.sceneId));
  document.querySelectorAll('#viewMode button').forEach((b) => b.addEventListener('click', () => setView(b.dataset.mode)));
  document.querySelectorAll('#tabs button').forEach((b) => b.addEventListener('click', () => setTab(b.dataset.tab)));

  const menu = $('projectMenu');
  $('btnProject').addEventListener('click', (e) => { e.stopPropagation(); menu.hidden = !menu.hidden; });
  document.addEventListener('click', () => { menu.hidden = true; });
  menu.addEventListener('click', async (e) => {
    const act = e.target.dataset && e.target.dataset.act;
    if (!act) return;
    menu.hidden = true;
    if (act === 'new') {
      if (window.confirm('Start a new project? The current one stays in undo history until you reload; export it first to keep it.')) {
        replaceProject(Schema.defaultProject(), 'New project.');
      }
    } else if (act === 'import') $('importInput').click();
    else if (act === 'export') exportProject();
    else if (act === 'clearAudio') {
      if (!window.confirm('Forget all audio and backdrop files (and audio analysis) stored in this browser? Projects keep their references; you can load the files again.')) return;
      try {
        for (const store of ['files', 'features']) {
          const keys = await AudioLib.idb.keys(store);
          for (const k of keys) await AudioLib.idb.del(store, k);
        }
        toast('Stored audio and backdrop files cleared.', 'ok');
      } catch (err) { toast('Could not clear stored audio.', 'err'); }
    }
  });

  function importFile(file) {
    const reader = new FileReader();
    reader.onload = () => {
      try {
        const data = JSON.parse(String(reader.result));
        if (!data || typeof data !== 'object' || !Array.isArray(data.scenes)) throw new Error('not a Fractal Video project');
        replaceProject(data, `Loaded ${file.name}`);
      } catch (err) {
        toast(`Import failed: ${err.message}`, 'err');
      }
    };
    reader.readAsText(file);
  }

  $('importInput').addEventListener('change', (e) => { const f = e.target.files[0]; if (f) importFile(f); e.target.value = ''; });
  $('masterInput').addEventListener('change', (e) => { const f = e.target.files[0]; if (f) loadMasterFile(f); e.target.value = ''; });
  $('stemInput').addEventListener('change', (e) => { const fs = Array.from(e.target.files || []); if (fs.length) addStemFiles(fs); e.target.value = ''; });
  $('backdropInput').addEventListener('change', (e) => {
    const f = e.target.files[0];
    const scene = project.scenes.find((s) => s.id === backdropTarget) || currentScene();
    if (f) setBackdropFile(scene, f);
    e.target.value = '';
  });

  // drag & drop files anywhere (audio or project JSON); scene-card drags are ignored here
  const isFileDrag = (e) => e.dataTransfer && Array.from(e.dataTransfer.types).includes('Files');
  window.addEventListener('dragover', (e) => { if (isFileDrag(e)) { e.preventDefault(); $('app').classList.add('dragging-files'); } });
  window.addEventListener('dragleave', (e) => { if (!e.relatedTarget) $('app').classList.remove('dragging-files'); });
  window.addEventListener('drop', (e) => {
    if (!isFileDrag(e)) return;
    e.preventDefault();
    $('app').classList.remove('dragging-files');
    const files = Array.from(e.dataTransfer.files || []);
    const json = files.find((f) => /\.json$/i.test(f.name));
    const media = files.find((f) => FV.Backdrops.isVideoFile(f) || FV.Backdrops.isImageFile(f));
    if (json) importFile(json);
    else {
      if (media) setBackdropFile(currentScene(), media);
      handleAudioFiles(files);
    }
  });

  // ---------- keyboard ----------
  let spaceHandled = false;
  window.addEventListener('keydown', (e) => {
    if (movieJob) return; // the render dialog is modal; shortcuts would change what is being rendered
    const tg = e.target;
    const typing = tg && (tg.tagName === 'TEXTAREA' || tg.tagName === 'SELECT' || (tg.tagName === 'INPUT' && !['range', 'checkbox', 'color', 'button'].includes(tg.type)));
    if ((e.ctrlKey || e.metaKey) && !typing) {
      if (e.key === 'z' || e.key === 'Z') { e.preventDefault(); if (e.shiftKey) redo(); else undo(); return; }
      if (e.key === 'y' || e.key === 'Y') { e.preventDefault(); redo(); return; }
      if (e.key === 's' || e.key === 'S') { e.preventDefault(); saveNow(); toast('Saved in this browser. Use Project → Export to save a file.'); return; }
    }
    if (typing || e.ctrlKey || e.metaKey || e.altKey) return;
    if (e.key === 'Escape') { document.querySelectorAll('.modal').forEach((m) => { if (m.id !== 'renderModal') m.hidden = true; }); menu.hidden = true; return; }
    if (e.code === 'Space') { e.preventDefault(); spaceHandled = true; togglePlay(); return; }
    if (e.key === 'Home') { e.preventDefault(); seek(0); return; }
    if (e.key === 'ArrowLeft' || e.key === 'ArrowRight') {
      if (tg && tg.type === 'range') return;
      e.preventDefault();
      const step = (e.shiftKey ? project.beatsPerBar : 1) * Ev.secPerBeat(project) * (e.key === 'ArrowLeft' ? -1 : 1);
      seek(transport.time() + step);
      return;
    }
    if (e.key === 's' || e.key === 'S') { setView(settings.view === 'scene' ? 'timeline' : 'scene'); return; }
    if (e.key === 'f' || e.key === 'F') { resetCamera(); return; }
    if (e.key === 'k' || e.key === 'K') { editableScene(); FV.Keys.keyCamera(ctx); return; }
    if (e.key === '?') { $('helpModal').hidden = false; return; }
    if ((e.key === 'Delete' || e.key === 'Backspace') && settings.tab === 'look' && deleteKeyHandler) { e.preventDefault(); deleteKeyHandler(); return; }
    if (/^[1-9]$/.test(e.key)) {
      const s = project.scenes[Number(e.key) - 1];
      if (s) selectScene(s.id);
    }
  });
  window.addEventListener('keyup', (e) => { if (e.code === 'Space' && spaceHandled) { e.preventDefault(); spaceHandled = false; } });

  window.addEventListener('resize', updateGuide);
  if (typeof ResizeObserver !== 'undefined') new ResizeObserver(updateGuide).observe(canvas);

  // ---------- boot ----------
  setView(settings.view);
  rebuild('all');
  setTab(settings.tab);
  aspectChanged();
  if (renderer && !safeReason) ctx.precompile();
  syncAudio();
  syncBackdropMedia();
  requestAnimationFrame(loop);

  // test hook for headless checks
  FV.app = { ctx, get project() { return project; }, renderer, transport, analysis, backdrops, setBackdropFile, setView, setTab, selectScene, seek, evaluateNow, commit, undo, redo };
})();
