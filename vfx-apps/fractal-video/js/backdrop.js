/* Backdrop media: pictures and movies shown behind the fractal, one per scene.
   Files are kept in the browser (IndexedDB 'files' store, same as the audio) and referenced by key.
   Pictures become ImageBitmaps (downscaled to MAX_SIDE); movies are muted <video> elements that are
   never shown, only uploaded to a texture by the renderer whenever `version` changes.
   Movie time is decided by the evaluator (song time or section time, looped), so preview, still render
   and a later video render all agree; here the elements are only steered towards that time. */
(function () {
  'use strict';
  const FV = (window.FV = window.FV || {});

  const MAX_SIDE = 4096;
  const BIG_FILE = 400 * 1024 * 1024;

  const isVideoFile = (f) => /^video\//.test(f.type) || /\.(mp4|m4v|webm|mov|ogv|mkv)$/i.test(f.name);
  const isImageFile = (f) => /^image\//.test(f.type) || /\.(jpe?g|png|webp|gif|avif|bmp)$/i.test(f.name);

  class Backdrops {
    constructor(opts) {
      this.idb = opts.idb;
      this.fileKey = opts.fileKey;
      this.onChange = opts.onChange || null; // (item) after a load finishes or fails
      this.onForget = opts.onForget || null; // (key) when an item is released
      this.items = new Map();
    }

    // Renderer-facing: the item if it can be uploaded.
    get(key) {
      const m = this.items.get(key);
      return m && m.ready ? m : null;
    }

    status(key) {
      const m = this.items.get(key);
      return m ? m.status : 'none';
    }

    // Store a dropped/picked file and load it. Resolves to the item ({ kind, w, h, duration, ... }).
    async add(file) {
      const kind = isVideoFile(file) ? 'video' : isImageFile(file) ? 'image' : null;
      if (!kind) throw new Error(`${file.name} is not a picture or a movie.`);
      const key = this.fileKey(file);
      let stored = true;
      try { await this.idb.put('files', key, file); } catch (err) { stored = false; console.warn(err); }
      this._release(key);
      const m = this._item(key, kind);
      await this._attach(m, file);
      if (m.status === 'error') throw new Error(m.error);
      m.stored = stored;
      m.big = file.size > BIG_FILE;
      return m;
    }

    // Load a stored file once (project open, undo). Missing files are reported via status 'missing'.
    async ensure(key, kind) {
      if (!key) return null;
      const have = this.items.get(key);
      if (have) return have;
      const m = this._item(key, kind);
      let blob = null;
      try { blob = await this.idb.get('files', key); } catch (err) { console.warn(err); }
      if (this.items.get(key) !== m) return null; // released meanwhile
      if (!blob) {
        m.status = 'missing';
        if (this.onChange) this.onChange(m);
        return m;
      }
      await this._attach(m, blob);
      return m;
    }

    // Keep only the given keys; free the rest (video elements, bitmaps, object URLs, textures).
    retain(keys) {
      Array.from(this.items.keys()).forEach((k) => { if (!keys.has(k)) this._release(k); });
    }

    _item(key, kind) {
      const m = { key, kind, ready: false, status: 'loading', version: 0, el: null, url: null, w: 0, h: 0, duration: 0, error: null };
      this.items.set(key, m);
      return m;
    }

    async _attach(m, blob) {
      try {
        if (m.kind === 'video') {
          const v = document.createElement('video');
          v.muted = true;
          v.playsInline = true;
          v.loop = true;
          v.preload = 'auto';
          m.url = URL.createObjectURL(blob);
          await new Promise((resolve, reject) => {
            const timer = setTimeout(() => reject(new Error('The movie took too long to open.')), 20000);
            v.addEventListener('loadeddata', () => { clearTimeout(timer); resolve(); }, { once: true });
            v.addEventListener('error', () => { clearTimeout(timer); reject(new Error('The browser cannot play this movie format (use H.264 MP4 or WebM).')); }, { once: true });
            v.src = m.url;
          });
          v.addEventListener('seeked', () => { m.version += 1; });
          m.el = v;
          m.w = v.videoWidth;
          m.h = v.videoHeight;
          m.duration = Number.isFinite(v.duration) ? v.duration : 0;
        } else {
          // straight (not premultiplied) alpha, so transparent PNG overlays keep their colours
          const opts = { premultiplyAlpha: 'none' };
          let bmp = await createImageBitmap(blob, opts);
          const s = Math.min(1, MAX_SIDE / Math.max(bmp.width, bmp.height));
          if (s < 1) {
            const w = Math.round(bmp.width * s);
            const h = Math.round(bmp.height * s);
            bmp.close();
            bmp = await createImageBitmap(blob, Object.assign({ resizeWidth: w, resizeHeight: h, resizeQuality: 'high' }, opts));
          }
          m.el = bmp;
          m.w = bmp.width;
          m.h = bmp.height;
        }
        if (!m.w || !m.h) throw new Error('The file has no picture.');
        m.ready = true;
        m.status = 'ready';
        m.version += 1;
      } catch (err) {
        m.status = 'error';
        m.error = err.message;
        if (m.url) { URL.revokeObjectURL(m.url); m.url = null; }
      }
      if (this.items.get(m.key) === m && this.onChange) this.onChange(m);
    }

    _release(key) {
      const m = this.items.get(key);
      if (!m) return;
      this.items.delete(key);
      if (m.el && m.kind === 'video') {
        m.el.pause();
        m.el.removeAttribute('src');
        m.el.load();
      } else if (m.el && m.el.close) m.el.close();
      if (m.url) URL.revokeObjectURL(m.url);
      if (this.onForget) this.onForget(key);
    }

    // Steer the movies of the current frame towards their evaluated time. Playing: let the element
    // run and only correct drift; paused: seek exactly (the renderer redraws on 'seeked').
    sync(frame, playing) {
      const layers = frame && frame.gpu.backdrop ? frame.gpu.backdrop.layers : [];
      const active = new Set();
      layers.forEach((L) => {
        if (L.kind !== 'video') return;
        const m = this.items.get(L.key);
        if (!m || !m.ready) return;
        active.add(L.key);
        const v = m.el;
        const rate = Math.min(4, Math.max(0.1, L.speed || 1));
        if (Math.abs(v.playbackRate - rate) > 1e-3) v.playbackRate = rate;
        if (playing) {
          if (v.paused) {
            v.currentTime = L.time;
            v.play().catch(() => {});
          } else {
            const d = m.duration;
            let diff = v.currentTime - L.time;
            if (d > 0) diff = ((((diff + d / 2) % d) + d) % d) - d / 2;
            if (Math.abs(diff) > 0.3) v.currentTime = L.time;
          }
          m.version += 1; // upload the current movie frame with every drawn frame
        } else {
          if (!v.paused) v.pause();
          if (!v.seeking && Math.abs(v.currentTime - L.time) > 0.02) v.currentTime = L.time;
        }
      });
      this.items.forEach((m, key) => {
        if (m.kind === 'video' && m.ready && !active.has(key) && !m.el.paused) m.el.pause();
      });
    }

    // Before a still render: put every movie of the frame exactly on its frame time.
    prepare(frame) {
      const layers = frame && frame.gpu.backdrop ? frame.gpu.backdrop.layers : [];
      return Promise.all(layers.map((L) => {
        const m = this.get(L.key);
        if (!m || m.kind !== 'video') return null;
        const v = m.el;
        v.pause();
        if (Math.abs(v.currentTime - L.time) < 1e-3 && !v.seeking) return null;
        return new Promise((resolve) => {
          const timer = setTimeout(resolve, 4000);
          v.addEventListener('seeked', () => { clearTimeout(timer); resolve(); }, { once: true });
          v.currentTime = L.time;
        });
      })).then(() => {
        layers.forEach((L) => { const m = this.get(L.key); if (m) m.version += 1; });
      });
    }

    // Small preview for the Look tab.
    drawThumb(key, canvas) {
      const m = this.get(key);
      const g = canvas.getContext('2d');
      g.fillStyle = '#0b0d12';
      g.fillRect(0, 0, canvas.width, canvas.height);
      if (!m) return false;
      const s = Math.max(canvas.width / m.w, canvas.height / m.h);
      const w = m.w * s;
      const h = m.h * s;
      try { g.drawImage(m.el, (canvas.width - w) / 2, (canvas.height - h) / 2, w, h); } catch (err) { return false; }
      return true;
    }

    // Average colours of the top and bottom thirds (linear-light mean, returned as sRGB hex).
    sampleColors(key) {
      const m = this.get(key);
      if (!m) return null;
      const c = document.createElement('canvas');
      c.width = 24;
      c.height = 24;
      const g = c.getContext('2d', { willReadFrequently: true });
      try { g.drawImage(m.el, 0, 0, 24, 24); } catch (err) { return null; }
      const px = g.getImageData(0, 0, 24, 24).data;
      const toLin = (v) => { const x = v / 255; return x <= 0.04045 ? x / 12.92 : Math.pow((x + 0.055) / 1.055, 2.4); };
      const toHex = (lin) => '#' + lin.map((x) => {
        const s = x <= 0.0031308 ? x * 12.92 : 1.055 * Math.pow(x, 1 / 2.4) - 0.055;
        return Math.round(Math.min(1, Math.max(0, s)) * 255).toString(16).padStart(2, '0');
      }).join('');
      const mean = (y0, y1) => {
        const acc = [0, 0, 0];
        let n = 0;
        for (let y = y0; y < y1; y += 1) {
          for (let x = 0; x < 24; x += 1) {
            const i = (y * 24 + x) * 4;
            acc[0] += toLin(px[i]); acc[1] += toLin(px[i + 1]); acc[2] += toLin(px[i + 2]);
            n += 1;
          }
        }
        return toHex(acc.map((v) => v / n));
      };
      return { top: mean(0, 8), bottom: mean(16, 24), all: mean(0, 24) };
    }
  }

  Backdrops.isVideoFile = isVideoFile;
  Backdrops.isImageFile = isImageFile;
  FV.Backdrops = Backdrops;
})();
