/* Worker entry for the heavy models (spec §10.6): the energy balance model now, the time machine later. */
import { runEBM } from './climate.js?v=9';

self.onmessage = (ev) => {
  const { id, key, params } = ev.data;
  try {
    const result = runEBM(params);
    self.postMessage({ id, key, result });
  } catch (e) {
    self.postMessage({ id, key, error: String(e) });
  }
};
