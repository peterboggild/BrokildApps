/* Timeline: ruler, contiguous sections, master waveform and one lane per stem.
   The canvas only covers the visible width (virtualised), so long songs at high zoom stay cheap.
   Sections are DOM blocks: click to select, drag the right edge to resize (snaps to bars, Alt: beats),
   drop a scene card on one to assign it. */
(function () {
  'use strict';
  const FV = (window.FV = window.FV || {});
  const { h, button, fmtTime } = FV.ui;
  const Ev = window.FVEvaluator;
  const Pal = window.FVPalettes;

  const RULER_H = 20;
  const SEC_H = 36;
  const WAVE_H = 46;
  const STEM_H = 20;
  const KEY_H = 22;
  // Camera keys of all camera lanes, merged per beat: [{ beat, qs, members: [{lane, q}] }].
  function groupPoints(p, row, starts) {
    const map = new Map();
    row.lanes.forEach((lane) => Ev.keyPoints(p, lane, starts).forEach((pt) => {
      const k = Math.round(pt.beat * 64);
      if (!map.has(k)) map.set(k, { beat: pt.beat, qs: [], members: [] });
      const e = map.get(k);
      e.qs.push(pt.q);
      e.members.push({ lane, q: pt.q });
    }));
    return Array.from(map.values()).sort((a, b) => a.beat - b.beat);
  }
  const KEY_COLOR = '#ffc35a';

  class Timeline {
    constructor(host, ctx) {
      this.host = host;
      this.ctx = ctx;
      this.ppb = 0;           // pixels per beat (0 = fit on first layout)
      this.follow = true;
      this.selId = null;
      this.t = 0;
      this._build();
    }

    _build() {
      const host = this.host;
      host.innerHTML = '';
      this.resizeHandle = h('div', { class: 'tl-resize', title: 'Drag to resize the timeline' });
      host.appendChild(this.resizeHandle);
      this._initResize();

      this.secEdit = h('div', { class: 'tl-sec-edit' });
      this.followBtn = button('', { class: 'icon-btn on', icon: 'follow', title: 'Follow the playhead while playing', onClick: () => { this.follow = !this.follow; this.followBtn.classList.toggle('on', this.follow); } });
      // Shown in Scene mode: the preview holds one scene, so the arrangement below is not what you see.
      this.heldChip = button('', { class: 'tl-held', title: 'The preview is in Scene mode and holds this scene while playing. Click (or press S) to preview the timeline instead.', onClick: () => this.ctx.setView('timeline') });
      this.toolbar = h('div', { class: 'tl-toolbar' },
        button('Section', { icon: 'plus', title: 'Append an 8-bar section', onClick: () => this._addSection() }),
        button('Split', { icon: 'split', title: 'Split the section under the playhead at the nearest bar', onClick: () => this._split() }),
        button('', { class: 'icon-btn', icon: 'trash', title: 'Delete the selected section (its time goes to the previous one)', onClick: () => this._delete() }),
        h('span', { class: 'sep' }),
        this.secEdit,
        h('div', { class: 'tl-right' },
          this.heldChip,
          button('Fit to song', { title: 'Stretch the last section to the end of the master', onClick: () => this._fitToSong() }),
          h('span', { class: 'sep' }),
          button('', { class: 'icon-btn', icon: 'zoomOut', title: 'Zoom out (Ctrl+wheel)', onClick: () => this._zoom(1 / 1.5) }),
          button('', { class: 'icon-btn', icon: 'zoomIn', title: 'Zoom in (Ctrl+wheel)', onClick: () => this._zoom(1.5) }),
          button('', { class: 'icon-btn', icon: 'fit', title: 'Fit the whole song', onClick: () => { this.ppb = 0; this.refresh(); } }),
          this.followBtn));
      host.appendChild(this.toolbar);

      this.gutter = h('div', { class: 'tl-gutter' });
      this.gutterInner = h('div', { class: 'tl-gutter-inner' });
      this.gutter.appendChild(this.gutterInner);
      this.canvas = h('canvas', { class: 'tl-canvas' });
      this.sections = h('div', { class: 'tl-sections', style: { top: `${RULER_H}px` } });
      this.playhead = h('div', { class: 'tl-playhead' });
      this.content = h('div', { class: 'tl-content' }, this.canvas, this.sections, this.playhead);
      this.scroll = h('div', { class: 'tl-scroll' }, this.content);
      host.appendChild(h('div', { class: 'tl-main' }, this.gutter, this.scroll));

      this.scroll.addEventListener('scroll', () => {
        this.gutterInner.style.transform = `translateY(${-this.scroll.scrollTop}px)`;
        this._scheduleDraw();
      });
      this.scroll.addEventListener('wheel', (e) => {
        if (e.ctrlKey) {
          e.preventDefault();
          const rect = this.scroll.getBoundingClientRect();
          this._zoom(e.deltaY < 0 ? 1.2 : 1 / 1.2, e.clientX - rect.left);
        } else if (Math.abs(e.deltaY) > Math.abs(e.deltaX) && this.scroll.scrollHeight <= this.scroll.clientHeight + 1) {
          e.preventDefault();
          this.scroll.scrollLeft += e.deltaY;
        }
      }, { passive: false });
      this.content.addEventListener('pointerdown', (e) => this._onPointerDown(e));
      this.content.addEventListener('contextmenu', (e) => {
        const rect = this.content.getBoundingClientRect();
        if (this._keyHit(e.clientX - rect.left, e.clientY - rect.top)) e.preventDefault();
      });
      if (typeof ResizeObserver !== 'undefined') new ResizeObserver(() => this.refresh()).observe(this.scroll);
    }

    _initResize() {
      let stored = 0;
      try { stored = Number(localStorage.getItem('fv-tl-h')) || 0; } catch (err) { stored = 0; }
      if (stored) document.documentElement.style.setProperty('--tl-h', `${stored}px`);
      this.resizeHandle.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        const startY = e.clientY;
        const startH = this.host.getBoundingClientRect().height;
        const move = (ev) => {
          const hh = Math.round(Math.min(window.innerHeight * 0.6, Math.max(140, startH + (startY - ev.clientY))));
          document.documentElement.style.setProperty('--tl-h', `${hh}px`);
          stored = hh;
        };
        const up = () => {
          window.removeEventListener('pointermove', move);
          window.removeEventListener('pointerup', up);
          try { localStorage.setItem('fv-tl-h', String(stored)); } catch (err) { /* ignore */ }
        };
        window.addEventListener('pointermove', move);
        window.addEventListener('pointerup', up);
      });
    }

    // ---------- geometry ----------
    _geom() {
      const p = this.ctx.project();
      const analysis = this.ctx.analysis();
      const beatMin = Math.min(0, Math.floor(Ev.beatAt(p, 0)));
      const endBeat = Math.max(Ev.totalBeats(p), Ev.beatAt(p, Ev.songDuration(p, analysis)));
      const span = Math.max(4, endBeat - beatMin + p.beatsPerBar);
      return { p, analysis, beatMin, span };
    }

    _x(beat) { return (beat - this.g.beatMin) * this.ppb; }
    _beat(x) { return x / this.ppb + this.g.beatMin; }

    _zoom(f, anchorX) {
      if (!this.ppb) return;
      const ax = anchorX !== undefined ? anchorX : this.scroll.clientWidth / 2;
      const beatAtAnchor = this._beat(this.scroll.scrollLeft + ax);
      this.ppb = Math.min(120, Math.max(0.25, this.ppb * f));
      this.refresh();
      this.scroll.scrollLeft = this._x(beatAtAnchor) - ax;
    }

    // ---------- build / refresh ----------
    refresh() {
      if (!this.scroll.clientWidth) return;
      this.g = this._geom();
      const { p } = this.g;
      if (!this.ppb) this.ppb = Math.max(0.25, (this.scroll.clientWidth - 12) / this.g.span);
      const width = Math.ceil(this.g.span * this.ppb);
      const lanes = 1 + p.stems.length;
      // Camera lanes share one row: a camera key is one diamond (all its values at that beat).
      const camLanes = (p.keys || []).filter((k) => k.dest.startsWith('cam.'));
      this.keyRows = (p.keys || []).filter((k) => !k.dest.startsWith('cam.'));
      if (camLanes.length) this.keyRows.push({ group: true, lanes: camLanes, id: 'camera', off: camLanes[0].off });
      this.height = RULER_H + SEC_H + this.keyRows.length * KEY_H + WAVE_H + p.stems.length * STEM_H;
      this.content.style.width = `${width}px`;
      this.content.style.height = `${this.height}px`;
      this.playhead.style.height = `${this.height}px`;
      this.sections.style.width = `${width}px`;

      // gutter labels
      this.gutterInner.innerHTML = '';
      // (a custom property cannot be set through Object.assign on style, so set the height directly)
      const lab = (text, hgt, dot, title) => h('div', { class: 'tl-lane-label', style: { height: `${hgt}px` }, title: title || text }, dot ? h('i', { style: { background: dot } }) : null, text);
      this.gutterInner.appendChild(lab('Bars', RULER_H));
      this.gutterInner.appendChild(lab('Sections', SEC_H));
      this.keyRows.forEach((lane) => {
        if (lane.group) {
          const on = p.scenes.filter((s) => lane.lanes.some((l) => window.FVSchema.routeOn(l, s.id))).map((s) => s.name);
          this.gutterInner.appendChild(lab('◆ Camera', KEY_H, null,
            `Camera keys (${groupPoints(p, lane).length}), on in ${on.join(', ') || 'no scene'}.
Each diamond is one camera pose. Click to jump, drag to move it, right-click to delete.
Moving the camera (mouse or Camera sliders) with the playhead on a diamond changes that key.`));
          return;
        }
        const spec = window.FVSchema.keySpec(lane.dest) || { label: lane.dest };
        const on = p.scenes.filter((s) => window.FVSchema.routeOn(lane, s.id)).map((s) => s.name);
        this.gutterInner.appendChild(lab(`◆ ${spec.label}`, KEY_H, null,
          `Keyframes: ${spec.label} (${lane.pts.length}), on in ${on.join(', ') || 'no scene'}.\nClick a diamond to jump to it, drag to move it (snaps to quarter beats; Alt: free), right-click to delete.\nDimmed stretches show scenes the keys do not act in.`));
      });
      this.gutterInner.appendChild(lab(p.master ? 'Master' : 'Master (none)', WAVE_H, '#6cc4ff', p.master ? p.master.name : 'Load a master in the Audio panel'));
      p.stems.forEach((s) => this.gutterInner.appendChild(lab(s.name.replace(/\.[^.]+$/, ''), STEM_H, FV.Panels.ROLE_COLORS[s.role], `${s.name} (${s.role})`)));
      void lanes;

      this._buildSections();
      this._buildSecEdit();
      this.setTime(this.t, true);
      this._scheduleDraw();
    }

    _buildSections() {
      const { p } = this.g;
      this.sections.innerHTML = '';
      const starts = Ev.sectionStarts(p);
      if (!this.selId || !p.sections.some((s) => s.id === this.selId)) this.selId = p.sections[0] ? p.sections[0].id : null;
      p.sections.forEach((sec, i) => {
        const scene = p.scenes.find((s) => s.id === sec.sceneId) || p.scenes[0];
        const x = this._x(starts[i]);
        const w = sec.length * this.ppb;
        const el = h('div', {
          class: `tl-sec${sec.id === this.selId ? ' sel' : ''}`,
          title: `${sec.name} — ${scene.name}\nbars ${(starts[i] / p.beatsPerBar + 1).toFixed(2).replace(/\.00$/, '')}–${((starts[i] + sec.length) / p.beatsPerBar + 1).toFixed(2).replace(/\.00$/, '')}\nClick to select, double-click to jump here, drag the right edge to resize.`,
          style: { left: `${x}px`, width: `${Math.max(2, w - 1)}px`, background: Pal.css(scene.look.palette.stops, scene.look.palette.interp) }
        }, h('b', { text: sec.name }), h('small', { text: scene.name }));
        const next = p.sections[i + 1];
        if (next && next.transition > 0 && next.sceneId !== sec.sceneId) {
          const tw = Math.min(sec.length, next.transition) * this.ppb;
          el.appendChild(h('div', { class: 'trans', style: { left: 'auto', right: '0', width: `${tw}px` }, title: `Transition into ${next.name}: ${next.transition} beats` }));
        }
        const handle = h('div', { class: 'handle', title: 'Drag to resize (snaps to bars; hold Alt for beats)' });
        el.appendChild(handle);
        handle.addEventListener('pointerdown', (e) => this._resizeSection(e, sec, starts[i]));
        el.addEventListener('click', (e) => {
          e.stopPropagation();
          this.selId = sec.id;
          this.ctx.selectScene(sec.sceneId, { keepView: true });
          this._markSelected();
          this._buildSecEdit();
        });
        el.addEventListener('dblclick', (e) => { e.stopPropagation(); this.ctx.setView('timeline'); this.ctx.seek(Ev.timeAtBeat(p, starts[i])); });
        el.addEventListener('pointerdown', (e) => e.stopPropagation());
        el.addEventListener('dragover', (e) => {
          if (Array.from(e.dataTransfer.types).includes('text/fv-scene')) { e.preventDefault(); el.classList.add('drop'); }
        });
        el.addEventListener('dragleave', () => el.classList.remove('drop'));
        el.addEventListener('drop', (e) => {
          e.preventDefault();
          el.classList.remove('drop');
          const id = e.dataTransfer.getData('text/fv-scene');
          if (id && p.scenes.some((s) => s.id === id)) {
            sec.sceneId = id;
            this.selId = sec.id;
            this.ctx.commit('Assign scene');
            this.ctx.rebuild('timeline', 'scenes');
          }
        });
        el.dataset.id = sec.id;
        el.dataset.scene = sec.sceneId;
        this.sections.appendChild(el);
      });
      this.sections.style.height = `${SEC_H}px`;
      this.markHeld();
    }

    // Scene mode: dim the sections of other scenes and show which scene the preview holds.
    markHeld() {
      const held = this.ctx.view() === 'scene' ? this.ctx.scene() : null;
      Array.from(this.sections.children).forEach((el) => el.classList.toggle('held-out', !!held && el.dataset.scene !== held.id));
      this.heldChip.hidden = !held;
      if (held) {
        this.heldChip.innerHTML = '';
        this.heldChip.append(h('span', { class: 'long', text: 'Scene only · ' }), 'Play timeline');
        this.heldChip.title = `The preview is in Scene mode and holds "${held.name}" while playing. Click (or press S) to preview the timeline instead.`;
      }
    }

    _markSelected() {
      Array.from(this.sections.children).forEach((el) => el.classList.toggle('sel', el.dataset.id === this.selId));
    }

    _buildSecEdit() {
      const p = this.ctx.project();
      const sec = p.sections.find((s) => s.id === this.selId);
      this.secEdit.innerHTML = '';
      if (!sec) return;
      const bpb = p.beatsPerBar;
      const name = h('input', { type: 'text', value: sec.name, title: 'Section name', spellcheck: false });
      name.addEventListener('change', () => { sec.name = name.value.trim().slice(0, 60) || sec.name; this.ctx.commit('Rename section'); this.refresh(); });
      const scene = h('select', { title: 'Scene shown in this section' }, ...p.scenes.map((s) => h('option', { value: s.id, text: s.name })));
      scene.value = sec.sceneId;
      scene.addEventListener('change', () => { sec.sceneId = scene.value; this.ctx.commit('Assign scene'); this.ctx.rebuild('timeline', 'scenes'); });
      const len = h('input', { type: 'number', min: String(1 / bpb), step: '0.25', value: String(+(sec.length / bpb).toFixed(2)), title: 'Length in bars' });
      len.addEventListener('change', () => {
        const bars = Number(len.value);
        if (!(bars > 0)) { len.value = String(sec.length / bpb); return; }
        sec.length = Math.max(1, Math.round(bars * bpb));
        this.ctx.commit('Section length');
        this.ctx.rebuild('timeline', 'render');
      });
      const tr = h('input', { type: 'number', min: '0', max: '64', step: '1', value: String(sec.transition), title: 'Transition from the previous scene, in beats before this section starts' });
      tr.addEventListener('change', () => {
        sec.transition = Math.max(0, Math.min(64, Math.round(Number(tr.value) || 0)));
        this.ctx.commit('Transition');
        this.refresh();
      });
      this.secEdit.appendChild(h('label', null, name));
      this.secEdit.appendChild(h('label', null, 'Scene', scene));
      this.secEdit.appendChild(h('label', null, 'Bars', len));
      this.secEdit.appendChild(h('label', { title: 'Blend time into this section (beats)' }, 'Blend in', tr));
    }

    // ---------- edits ----------
    _resizeSection(e, sec, start) {
      e.preventDefault();
      e.stopPropagation();
      const p = this.ctx.project();
      const rect = this.content.getBoundingClientRect();
      const orig = sec.length;
      const move = (ev) => {
        const beat = this._beat(ev.clientX - rect.left);
        const unit = ev.altKey ? 1 : p.beatsPerBar;
        const len = Math.max(unit, Math.round((beat - start) / unit) * unit);
        if (len !== sec.length) {
          sec.length = len;
          this._buildSections();
          this.ctx.touch();
        }
      };
      const up = () => {
        window.removeEventListener('pointermove', move);
        window.removeEventListener('pointerup', up);
        if (sec.length !== orig) {
          this.ctx.commit('Resize section');
          this.ctx.rebuild('timeline', 'render');
        }
      };
      window.addEventListener('pointermove', move);
      window.addEventListener('pointerup', up);
    }

    _addSection() {
      const p = this.ctx.project();
      const scene = this.ctx.scene() || p.scenes[0];
      const sec = { id: window.FVSchema.uid('sec'), name: `Section ${p.sections.length + 1}`, length: 8 * p.beatsPerBar, sceneId: scene.id, transition: 4 };
      p.sections.push(sec);
      this.selId = sec.id;
      this.ctx.commit('Add section');
      this.ctx.rebuild('timeline', 'scenes', 'render');
    }

    _split() {
      const p = this.ctx.project();
      const beat = Ev.beatAt(p, this.t);
      const starts = Ev.sectionStarts(p);
      const i = Ev.sectionIndexAt(p, beat);
      const sec = p.sections[i];
      const cut = Math.round((beat - starts[i]) / p.beatsPerBar) * p.beatsPerBar;
      if (cut <= 0 || cut >= sec.length) { this.ctx.toast('Move the playhead inside a section, at least one bar from its edges.', 'warn'); return; }
      const second = { id: window.FVSchema.uid('sec'), name: `${sec.name} b`, length: sec.length - cut, sceneId: sec.sceneId, transition: 0 };
      sec.length = cut;
      p.sections.splice(i + 1, 0, second);
      window.FV.Keys.sectionSplit(p, sec.id, second.id, cut);
      this.selId = second.id;
      this.ctx.commit('Split section');
      this.ctx.rebuild('timeline');
    }

    _delete() {
      const p = this.ctx.project();
      const i = p.sections.findIndex((s) => s.id === this.selId);
      if (i < 0) return;
      if (p.sections.length === 1) { this.ctx.toast('The song needs at least one section.', 'warn'); return; }
      const removed = p.sections.splice(i, 1)[0];
      const into = p.sections[Math.max(0, i - 1)];
      window.FV.Keys.sectionMerged(p, removed.id, removed.length, into.id, into.length, i > 0);
      into.length += removed.length;
      this.selId = into.id;
      this.ctx.commit('Delete section');
      this.ctx.rebuild('timeline', 'scenes');
    }

    _fitToSong() {
      const p = this.ctx.project();
      const a = this.ctx.analysis();
      if (!a || !a.duration) { this.ctx.toast('Load a master first.', 'warn'); return; }
      const end = Ev.beatAt(p, a.duration);
      const starts = Ev.sectionStarts(p);
      const last = p.sections[p.sections.length - 1];
      const len = Math.ceil((end - starts[starts.length - 1]) / p.beatsPerBar) * p.beatsPerBar;
      if (len < p.beatsPerBar) { this.ctx.toast('Sections already reach past the end of the master.', 'warn'); return; }
      last.length = len;
      this.ctx.commit('Fit sections to song');
      this.ctx.rebuild('timeline', 'render');
    }

    // ---------- scrubbing ----------
    _onPointerDown(e) {
      const rect = this.content.getBoundingClientRect();
      const hit = this._keyHit(e.clientX - rect.left, e.clientY - rect.top);
      if (hit && hit.pt && (e.button === 0 || e.button === 2)) { this._keyPointer(e, hit, rect); return; }
      if (e.button !== 0) return;
      if (this.ctx.view() !== 'timeline') this.ctx.setView('timeline');
      const p = this.ctx.project();
      const seekAt = (clientX, snap) => {
        let beat = this._beat(clientX - rect.left);
        if (snap) beat = Math.round(beat);
        this.ctx.seek(Math.max(0, Ev.timeAtBeat(p, beat)));
      };
      seekAt(e.clientX, e.shiftKey);
      const move = (ev) => seekAt(ev.clientX, ev.shiftKey);
      const up = () => {
        window.removeEventListener('pointermove', move);
        window.removeEventListener('pointerup', up);
      };
      window.addEventListener('pointermove', move);
      window.addEventListener('pointerup', up);
    }

    setTime(t, force) {
      this.t = t;
      if (!this.g || !this.ppb) return;
      const x = this._x(Ev.beatAt(this.g.p, t));
      if (force || Math.abs(x - (this._lastX || -1)) >= 0.5) {
        this.playhead.style.transform = `translateX(${x}px)`;
        this._lastX = x;
      }
      if (this.follow && this.ctx.playing()) {
        const left = this.scroll.scrollLeft;
        const w = this.scroll.clientWidth;
        if (x > left + w - 30 || x < left) this.scroll.scrollLeft = x - w * 0.1;
      }
    }

    // ---------- canvas ----------
    _scheduleDraw() {
      if (this._drawPending) return;
      this._drawPending = true;
      requestAnimationFrame(() => {
        this._drawPending = false;
        this._draw();
      });
    }

    _draw() {
      if (!this.g) return;
      const { p, analysis } = this.g;
      const dpr = Math.min(window.devicePixelRatio || 1, 2);
      const vw = this.scroll.clientWidth;
      const vh = this.height;
      if (!vw) return;
      if (this.canvas.width !== Math.round(vw * dpr) || this.canvas.height !== Math.round(vh * dpr)) {
        this.canvas.width = Math.round(vw * dpr);
        this.canvas.height = Math.round(vh * dpr);
        this.canvas.style.width = `${vw}px`;
        this.canvas.style.height = `${vh}px`;
      }
      const g = this.canvas.getContext('2d');
      g.setTransform(dpr, 0, 0, dpr, 0, 0);
      g.clearRect(0, 0, vw, vh);
      const sl = this.scroll.scrollLeft;
      const bpb = p.beatsPerBar;
      const b0 = this._beat(sl);
      const b1 = this._beat(sl + vw);

      // ruler
      g.fillStyle = '#0d1016';
      g.fillRect(0, 0, vw, RULER_H);
      g.font = '10px Segoe UI, system-ui, sans-serif';
      g.textBaseline = 'middle';
      const barPx = bpb * this.ppb;
      let every = 1;
      while (barPx * every < 36) every *= 2;
      const firstBar = Math.floor(b0 / bpb);
      for (let bar = firstBar; bar * bpb <= b1; bar += 1) {
        const x = this._x(bar * bpb) - sl;
        const major = bar % every === 0;
        g.fillStyle = major ? '#3a4255' : '#262c3a';
        g.fillRect(Math.round(x), major ? 6 : 12, 1, RULER_H - (major ? 6 : 12));
        if (major && bar >= 0) {
          g.fillStyle = '#8b94a8';
          g.fillText(String(bar + 1), x + 3, 9);
        }
        g.fillStyle = 'rgba(255,255,255,0.035)';
        g.fillRect(Math.round(x), RULER_H + SEC_H, 1, vh - RULER_H - SEC_H);
        if (this.ppb > 10) {
          for (let k = 1; k < bpb; k += 1) {
            const bx = this._x(bar * bpb + k) - sl;
            g.fillStyle = '#20252f';
            g.fillRect(Math.round(bx), 15, 1, RULER_H - 15);
          }
        }
      }
      // master waveform
      this._drawKeys(g, vw, sl);
      const wy = RULER_H + SEC_H + this.keyRows.length * KEY_H;
      g.fillStyle = '#0b0e14';
      g.fillRect(0, wy, vw, WAVE_H);
      const mf = analysis && analysis.master;
      if (mf) {
        const mid = wy + WAVE_H / 2;
        const amp = WAVE_H / 2 - 2;
        g.fillStyle = 'rgba(108,196,255,0.75)';
        const rate = mf.rate;
        for (let x = 0; x < vw; x += 1) {
          const t0 = Ev.timeAtBeat(p, this._beat(sl + x));
          const t1 = Ev.timeAtBeat(p, this._beat(sl + x + 1));
          let i0 = Math.floor(t0 * rate);
          let i1 = Math.max(i0 + 1, Math.floor(t1 * rate));
          if (i1 <= 0 || i0 >= mf.peakMin.length) continue;
          i0 = Math.max(0, i0);
          i1 = Math.min(mf.peakMin.length, i1);
          let lo = 0;
          let hi = 0;
          for (let i = i0; i < i1; i += 1) {
            if (mf.peakMin[i] < lo) lo = mf.peakMin[i];
            if (mf.peakMax[i] > hi) hi = mf.peakMax[i];
          }
          const y0 = mid - hi * amp;
          g.fillRect(x, y0, 1, Math.max(1, (hi - lo) * amp));
        }
      } else {
        g.fillStyle = '#5c6478';
        g.fillText(p.master ? 'Master not loaded in this browser — load it again in the Audio panel' : 'No master loaded — drop an audio file anywhere, or use the Audio panel', 8, wy + WAVE_H / 2);
      }

      // stem lanes
      p.stems.forEach((stem, k) => {
        const y = wy + WAVE_H + k * STEM_H;
        g.fillStyle = k % 2 ? '#0d1016' : '#0b0e14';
        g.fillRect(0, y, vw, STEM_H);
        const f = analysis && analysis.stems && analysis.stems[stem.id];
        if (!f) return;
        const colr = FV.Panels.ROLE_COLORS[stem.role] || '#adb5bd';
        g.fillStyle = colr + '88';
        for (let x = 0; x < vw; x += 1) {
          const t = Ev.timeAtBeat(p, this._beat(sl + x + 0.5));
          const i = Math.floor(t * f.rate);
          if (i < 0 || i >= f.env.length) continue;
          const v = f.env[i];
          if (v > 0.01) g.fillRect(x, y + STEM_H - 1 - v * (STEM_H - 3), 1, v * (STEM_H - 3));
        }
        if (f.onsetTimes && this.ppb > 2) {
          g.fillStyle = colr;
          const tA = Ev.timeAtBeat(p, b0);
          const tB = Ev.timeAtBeat(p, b1);
          for (let j = 0; j < f.onsetTimes.length; j += 1) {
            const ot = f.onsetTimes[j];
            if (ot < tA) continue;
            if (ot > tB) break;
            const x = this._x(Ev.beatAt(p, ot)) - sl;
            g.fillRect(Math.round(x), y + 1, 1, 4);
          }
        }
      });
    }

    redraw() { this._scheduleDraw(); }

    // ---------- keyframe lanes ----------
    _drawKeys(g, vw, sl) {
      const { p } = this.g;
      const Schema = window.FVSchema;
      const starts = Ev.sectionStarts(p);
      this.keyRows.forEach((lane, k) => {
        const y = RULER_H + SEC_H + k * KEY_H;
        g.fillStyle = k % 2 ? '#12151d' : '#0f1219';
        g.fillRect(0, y, vw, KEY_H);
        const onIn = (sid) => (lane.group ? lane.lanes.some((l) => Schema.routeOn(l, sid)) : Schema.routeOn(lane, sid));
        // dim the sections whose scene the keys do not act in
        p.sections.forEach((sec, i) => {
          if (onIn(sec.sceneId)) return;
          const x0 = this._x(starts[i]) - sl;
          g.fillStyle = 'rgba(0,0,0,0.55)';
          g.fillRect(x0, y, sec.length * this.ppb, KEY_H);
          g.fillStyle = 'rgba(255,255,255,0.05)';
          for (let hx = x0 - KEY_H; hx < x0 + sec.length * this.ppb; hx += 8) { g.save(); g.beginPath(); g.rect(x0, y, sec.length * this.ppb, KEY_H); g.clip(); g.fillRect(hx, y, 2, KEY_H); g.restore(); }
        });
        if (lane.group) { this._drawCamRow(g, vw, sl, lane, y, starts); return; }
        const spec = Schema.keySpec(lane.dest);
        if (!spec || !lane.pts.length) {
          g.fillStyle = '#5c6478';
          g.fillText('no keys yet: move the slider to set one at the playhead', 8, y + KEY_H / 2);
          return;
        }
        const yv = (v) => y + KEY_H - 4 - ((v - spec.min) / Math.max(1e-9, spec.max - spec.min)) * (KEY_H - 8);
        g.strokeStyle = 'rgba(255,195,90,0.7)';
        g.lineWidth = 1.2;
        g.beginPath();
        // only where the keys act: sections whose scene the lane is on in
        let pen = false;
        for (let x = 0; x <= vw; x += 2) {
          const bt = this._beat(sl + x);
          const si = Ev.sectionIndexAt(p, Math.max(0, bt));
          if (bt < 0 || bt > Ev.totalBeats(p) || !Schema.routeOn(lane, p.sections[si].sceneId)) { pen = false; continue; }
          const v = Ev.keyValueAt(p, lane, bt, starts);
          if (!pen) g.moveTo(x, yv(v)); else g.lineTo(x, yv(v));
          pen = true;
        }
        g.stroke();
        Ev.keyPoints(p, lane, starts).forEach((pt) => {
          const x = this._x(pt.beat) - sl;
          if (x < -6 || x > vw + 6) return;
          const cy = yv(pt.v);
          g.fillStyle = pt.q === this._dragKey ? '#ffffff' : KEY_COLOR;
          g.beginPath();
          g.moveTo(x, cy - 5); g.lineTo(x + 5, cy); g.lineTo(x, cy + 5); g.lineTo(x - 5, cy);
          g.closePath();
          g.fill();
          g.strokeStyle = '#1a1206';
          g.stroke();
        });
      });
    }

    _drawCamRow(g, vw, sl, row, y, starts) {
      const p = this.g.p;
      const pts = groupPoints(p, row, starts);
      const cy = y + KEY_H / 2;
      if (!pts.length) {
        g.fillStyle = '#5c6478';
        g.fillText('no camera keys yet: move the camera (or press K) to set one at the playhead', 8, cy);
        return;
      }
      g.strokeStyle = 'rgba(120,200,255,0.55)';
      g.lineWidth = 2;
      g.beginPath();
      g.moveTo(Math.max(0, this._x(pts[0].beat) - sl), cy);
      g.lineTo(Math.min(vw, this._x(pts[pts.length - 1].beat) - sl), cy);
      g.stroke();
      pts.forEach((pt) => {
        const x = this._x(pt.beat) - sl;
        if (x < -6 || x > vw + 6) return;
        g.fillStyle = pt.qs.includes(this._dragKey) ? '#ffffff' : '#7cc8ff';
        g.beginPath();
        g.moveTo(x, cy - 6); g.lineTo(x + 6, cy); g.lineTo(x, cy + 6); g.lineTo(x - 6, cy);
        g.closePath();
        g.fill();
        g.strokeStyle = '#061420';
        g.stroke();
      });
    }

    // The key under a content-relative point, if any.
    _keyHit(cx, cy) {
      const k = Math.floor((cy - RULER_H - SEC_H) / KEY_H);
      if (!this.keyRows || k < 0 || k >= this.keyRows.length) return null;
      const lane = this.keyRows[k];
      const p = this.ctx.project();
      let best = null;
      (lane.group ? groupPoints(p, lane) : Ev.keyPoints(p, lane)).forEach((pt) => {
        const d = Math.abs(this._x(pt.beat) - cx);
        if (d <= 7 && (!best || d < best.d)) best = { d, pt, lane };
      });
      return best || { lane, pt: null };
    }

    _keyPointer(e, hit, rect) {
      const p = this.ctx.project();
      e.preventDefault();
      e.stopPropagation();
      const members = hit.pt.members || [{ lane: hit.lane, q: hit.pt.q }];
      if (e.button === 2) {
        members.forEach((m) => m.lane.pts.splice(m.lane.pts.indexOf(m.q), 1));
        this.ctx.commit('Delete keyframe');
        this.ctx.rebuild('timeline', 'mod');
        window.FV.Keys.sync(this.ctx, this.ctx.playhead());
        return;
      }
      const q = members[0].q;
      const orig = { sec: q.sec, b: q.b };
      this._dragKey = q;
      this.ctx.seek(Ev.timeAtBeat(p, hit.pt.beat));
      const total = Ev.totalBeats(p);
      const snap = window.FV.Keys.SNAP;
      const move = (ev) => {
        let beat = this._beat(ev.clientX - rect.left);
        if (!ev.altKey) beat = Math.round(beat * snap) / snap;
        beat = Math.max(0, Math.min(total, beat));
        const at = window.FV.Keys.locate(p, beat);
        members.forEach((m) => { m.q.sec = at.sec; m.q.b = at.b; });
        this.ctx.seek(Ev.timeAtBeat(p, beat));
        this.ctx.touch();
        this._scheduleDraw();
      };
      const up = () => {
        window.removeEventListener('pointermove', move);
        window.removeEventListener('pointerup', up);
        this._dragKey = null;
        if (q.sec !== orig.sec || q.b !== orig.b) this.ctx.commit('Move keyframe');
        this._scheduleDraw();
      };
      window.addEventListener('pointermove', move);
      window.addEventListener('pointerup', up);
      this._scheduleDraw();
    }
  }

  FV.Timeline = Timeline;
})();
