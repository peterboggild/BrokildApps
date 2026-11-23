/* Challenges: open goals that send the student away from "copy the Earth" and towards the question of which other
 * worlds could work, for life in general and for humans. Each is checked on every climate result; all were checked
 * to be reachable in the model (see README). Texts in i18n: ch.<id>.title / .goal / .think. */
const row = (L, id) => { const r = L.rows.find((x) => x.id === id); return r ? r.status : null; };

export const CHALLENGES = [
  { id: 'microbes', check: (P, D, L) => L.pass[2] && !L.pass[4] && row(L, 'liquid') === 'green' },
  { id: 'far', check: (P, D, L) => D.star.massSun >= 0.9 && D.star.massSun <= 1.1 && P.aAU >= 1.3 && L.pass[3] && row(L, 'liquid') === 'green' },
  { id: 'k', check: (P, D, L) => D.star.massSun <= 0.8 && D.star.ageGyr >= 7 && L.pass[4] },
  { id: 'cold', check: (P, D, L) => L.pass[4] && P.fluxMean <= 0.8 },
  { id: 'hot', check: (P, D, L) => L.pass[4] && P.fluxMean >= 1.1 },
  { id: 'heavy', check: (P, D, L) => L.pass[4] && P.massEarth >= 2 },
  { id: 'red', check: (P, D, L) => D.star.massSun <= 0.5 && L.pass[2] },
  { id: 'snowball', check: (P) => P.fluxMean >= 0.95 && P.fluxMean <= 1.05 && !!P.ebm && P.iceFrac >= 0.5 },
  { id: 'water', check: (P, D, L) => P.oceans >= 3 && L.pass[2] },
  { id: 'nomoon', check: (P, D, L) => L.pass[4] && P.moons.length === 0 },
];

const KEY = 'planetDesigner.challenges';
export function loadDone() {
  try { return new Set(JSON.parse(localStorage.getItem(KEY) || '[]')); } catch (e) { return new Set(); }
}
export function saveDone(done) { try { localStorage.setItem(KEY, JSON.stringify([...done])); } catch (e) { /* ignore */ } }

/* Which challenges the current design meets (only planets with a finished climate result count). */
export function meets(D, ledgerFor) {
  const out = new Set();
  for (const P of D.planets) {
    if (P.role !== 'hz' || (!P.ebm && !P.climate.runaway)) continue;
    const L = ledgerFor(P);
    for (const c of CHALLENGES) { try { if (c.check(P, D, L)) out.add(c.id); } catch (e) { /* not yet derivable */ } }
  }
  return out;
}
