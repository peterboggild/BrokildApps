/* Where you got to, kept in the browser, so that after a crash (or a break) you can go on from there: each chapter
 * saves its page and mission time every few seconds while it runs, and at its end the start of the next chapter.
 * A chapter's start page offers "Continue" at the top of its start list when the save is for it; the front page
 * (index.html) shows a Continue link to the chapter. Owner's headset test: a crash late in a long chapter meant
 * starting it over. */
const KEY = 'moon-progress';
let last = 0;
const fmt = s => { const h = Math.floor(s / 3600), m = Math.floor(s % 3600 / 60); return h + ':' + String(m).padStart(2, '0'); };
export const CHAPTERS = { 'launch.html': 'chapter 1, the launch', 'orbit.html': 'chapter 2, to the Moon', 'lunar.html': 'chapter 3, lunar orbit',
  'lm.html': 'chapter 4, into the LM', 'descent.html': 'chapter 5, the descent', 'surface.html': 'chapter 6, on the Moon', 'liftoff.html': 'chapter 7, the lift-off', 'rendezvous.html': 'chapter 8, the rendezvous' };

export function loadProgress() {
  try { const p = JSON.parse(localStorage.getItem(KEY) || 'null'); return p && CHAPTERS[p.page] ? p : null; } catch (e) { return null; }
}
/* every frame is fine: it writes at most every 5 s (now: the time of the call, ms), or at once with force */
export function saveProgress(page, t, now = performance.now(), force = false) {
  if (!force && now - last < 5000) return;
  last = now;
  try { localStorage.setItem(KEY, JSON.stringify({ page, t, at: Date.now() })); } catch (e) { /* not kept */ }
}
/* the chapter's start list: "Continue from GET h:mm" first and chosen, when the save is for this page */
export function offerContinue(page, select) {
  const p = loadProgress();
  if (!select || !p || p.page !== page || typeof p.t !== 'number') return null;
  const o = document.createElement('option');
  o.value = String(p.t); o.textContent = 'Continue where you were: GET ' + fmt(p.t);
  select.insertBefore(o, select.firstChild); select.value = o.value;
  return p;
}
/* the front page: a Continue link above the chapters */
export function continueLink(before) {
  const p = loadProgress();
  if (!p || !before) return;
  const a = document.createElement('a');
  a.href = p.page; a.textContent = 'Continue: ' + CHAPTERS[p.page] + (p.page === 'surface.html' ? '' : typeof p.t === 'number' ? ', GET ' + fmt(p.t) : ', from its start');
  a.style.cssText = before.style.cssText; a.style.background = '#1f2a18'; a.style.borderColor = '#5f7c36';
  before.parentNode.insertBefore(a, before);
}

/* Staying in VR from one chapter to the next: when a page is left while in VR, the Quest's browser tells the next page
 * (the 'sessiongranted' event) that it may enter VR by itself. Then enter as soon as the page is ready. Elsewhere
 * nothing happens and the start page shows as before. */
export function autoEnter(isReady, enterVR) {
  if (!navigator.xr || !navigator.xr.addEventListener) return;
  navigator.xr.addEventListener('sessiongranted', () => { const t = setInterval(() => { if (isReady()) { clearInterval(t); enterVR(); } }, 200); });
}
