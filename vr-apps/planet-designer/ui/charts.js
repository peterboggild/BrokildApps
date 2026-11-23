/* Charts as inline SVG (no library): temperature by latitude from the climate model, and the cosmic shoreline.
 * Axes, units and labels beside the data (owner rule); colours via CSS classes so both themes read. */
import { t, fmt, esc } from '../i18n.js?v=6';
import { SOLAR_BODIES, shorelineI } from '../physics/space.js?v=6';

const W = 340, H = 210, L = 40, R = 12, T = 14, B = 34;
const px = (v, a, b) => L + (v - a) / (b - a) * (W - L - R);
const py = (v, a, b) => H - B - (v - a) / (b - a) * (H - T - B);

export function climateChart(P) {
  if (P.role !== 'hz') return '';
  if (P.climate.runaway) return `<p class="note">${t('ch.runaway')}</p>`;
  if (!P.ebm) return `<p class="note">${t('w.t.pending')}</p>`;
  const r = P.ebm;
  const tmin = Math.min(-60, Math.floor(Math.min(...r.min) / 20) * 20), tmax = Math.max(40, Math.ceil(Math.max(...r.max) / 20) * 20);
  const xs = r.lat;
  const xv = (lat) => (P.locked ? 90 - lat : lat);          /* locked: x is the angle from the substellar point */
  const xa = P.locked ? 0 : -90, xb = P.locked ? 180 : 90;
  const pts = (arr) => arr.map((v, j) => `${px(xv(xs[j]), xa, xb).toFixed(1)},${py(v, tmin, tmax).toFixed(1)}`);
  const band = [...pts(r.max), ...pts(r.min).reverse()].join(' ');
  let g = `<svg class="chart" viewBox="0 0 ${W} ${H}" role="img" aria-label="${esc(t('ch.tlat'))}">`;
  for (let v = tmin; v <= tmax; v += 20) {
    const y = py(v, tmin, tmax);
    g += `<line class="grid" x1="${L}" x2="${W - R}" y1="${y}" y2="${y}"/><text class="ax" x="${L - 5}" y="${y + 4}" text-anchor="end">${v}</text>`;
  }
  const ticks = P.locked ? [0, 45, 90, 135, 180] : [-90, -60, -30, 0, 30, 60, 90];
  for (const v of ticks) g += `<text class="ax" x="${px(v, xa, xb)}" y="${H - B + 14}" text-anchor="middle">${v}°</text>`;
  g += `<text class="ax" x="${(L + W - R) / 2}" y="${H - 4}" text-anchor="middle">${esc(t(P.locked ? 'ch.substellar' : 'ch.lat'))}</text>`;
  g += `<text class="ax" x="10" y="${T + 2}" text-anchor="start">°C</text>`;
  const y0 = py(0, tmin, tmax), y10 = py(-10, tmin, tmax);
  g += `<line class="zero" x1="${L}" x2="${W - R}" y1="${y0}" y2="${y0}"/><line class="ice" x1="${L}" x2="${W - R}" y1="${y10}" y2="${y10}"/>`;
  g += `<polygon class="band" points="${band}"/><polyline class="mean" points="${pts(r.mean).join(' ')}"/>`;
  g += `<text class="lab" x="${W - R - 2}" y="${y0 - 4}" text-anchor="end">0 °C</text><text class="lab icel" x="${W - R - 2}" y="${y10 + 12}" text-anchor="end">−10 °C: ${esc(t('ch.iceEdge'))}</text>`;
  g += '</svg>';
  g += `<div class="legend"><span class="sw mean"></span>${esc(t('ch.mean'))} <span class="sw band"></span>${esc(t('ch.range'))}</div>`;
  return g;
}

export function shorelineChart(P) {
  if (P.role !== 'hz') return '';
  const lv = (v) => Math.log10(v), vA = lv(0.8), vB = lv(40), iA = -4, iB = 4;
  const X = (v) => px(lv(v), vA, vB), Y = (i) => py(Math.max(iA, Math.min(iB, lv(i))), iA, iB);
  let g = `<svg class="chart" viewBox="0 0 ${W} ${H}" role="img" aria-label="${esc(t('ch.shore'))}">`;
  for (let e = iA; e <= iB; e += 2) g += `<line class="grid" x1="${L}" x2="${W - R}" y1="${Y(10 ** e)}" y2="${Y(10 ** e)}"/><text class="ax" x="${L - 5}" y="${Y(10 ** e) + 4}" text-anchor="end">10${e < 0 ? '⁻' : ''}${'⁰¹²³⁴'[Math.abs(e)]}</text>`;
  for (const v of [1, 3, 10, 30]) g += `<text class="ax" x="${X(v)}" y="${H - B + 14}" text-anchor="middle">${v}</text>`;
  g += `<text class="ax" x="${(L + W - R) / 2}" y="${H - 4}" text-anchor="middle">${esc(t('ch.vesc'))}</text>`;
  g += `<text class="ax" x="6" y="${T + 2}" text-anchor="start">${esc(t('ch.xuv'))}</text>`;
  /* the line, and the side without air shaded */
  const v1 = 0.8, v2 = 40;
  g += `<polygon class="noair" points="${X(v1)},${Y(shorelineI(v1))} ${X(v2)},${Y(shorelineI(v2))} ${X(v2)},${Y(1e4)} ${X(v1)},${Y(1e4)}"/>`;
  g += `<line class="shore" x1="${X(v1)}" y1="${Y(shorelineI(v1))}" x2="${X(v2)}" y2="${Y(shorelineI(v2))}"/>`;
  g += `<text class="lab" x="${X(1.2)}" y="${Y(3e3)}">${esc(t('ch.noAir'))}</text><text class="lab" x="${X(14)}" y="${Y(2e-3)}">${esc(t('ch.air'))}</text>`;
  for (const [name, v, i, air] of SOLAR_BODIES) {
    g += `<circle class="${air ? 'body air' : 'body'}" cx="${X(v)}" cy="${Y(i)}" r="3"><title>${esc(name)}</title></circle>`;
    if (['Jorden', 'Mars', 'Merkur', 'Titan', 'Venus', 'Månen'].includes(name)) g += `<text class="bl" x="${X(v) + 5}" y="${Y(i) + 3}">${esc(t('body.' + name))}</text>`;
  }
  const vx = P.vesc / 1e3, ix = P.xuv.fluence;
  g += `<circle class="you" cx="${X(vx)}" cy="${Y(ix)}" r="5.5"/><text class="youl" x="${X(vx) - 8}" y="${Y(ix) - 8}" text-anchor="end">${esc(P.name)}</text>`;
  g += '</svg>';
  g += `<div class="legend"><span class="sw dot air"></span>${esc(t('ch.hasAir'))} <span class="sw dot"></span>${esc(t('ch.noAirB'))} <span class="sw dot you"></span>${esc(t('ch.yours'))}</div>`;
  return g;
}
