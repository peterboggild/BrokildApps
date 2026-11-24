/* Writes Sleeper Agent's loops off the main thread.
   Twenty seconds of placed rain take seconds to render on a Quest; done on
   the page, that would stall the headset's frames and break the flicker's
   rhythm. The worker hands back plain channel arrays, which the page wraps
   in AudioBuffers. */
importScripts('../../health-apps/sleep-noise/sound-engine.js?v=1');

self.onmessage = function (e) {
  var m = e.data, ch;
  try {
    ch = m.kind === 'noise'
      ? self.SleeperSound.noiseArrays(m.name, m.sr, m.amt)
      : self.SleeperSound.padArrays(m.name, m.layer, m.sr, m.amt);
    self.postMessage({ id: m.id, ch: ch }, ch.map(function (c) { return c.buffer; }));
  } catch (err) {
    self.postMessage({ id: m.id, error: String(err && err.message || err) });
  }
};
