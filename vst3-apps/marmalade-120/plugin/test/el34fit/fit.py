"""MARMALADE 120 - the EL34 fit

Fits the pentode model in Source/Circuit.cpp (see Circuit.h) to the Mullard
EL34 data sheet (January 1960 / 1964), digitised in el34_data.py, and scores
Koren's published EL34 fit the same way for comparison.

    python3 fit.py          (numpy + scipy)

Error metric for the fit: (model - sheet) / (5% of the sheet value + 0.5 mA),
so the cutoff tail counts without the few-mA points dominating. Reported:
relative error over every point at 10 mA and up.
"""
import numpy as np
from scipy.optimize import least_squares
from el34_data import C1_IA, C1_IG2, C2, C3, TAB

sp = lambda x: np.logaddexp(0, x)
NAMES = ['G', 'C', 'mu', 'gamma', 'beta', 'vk', 'lam', 'q', 's']

def pentode(p, vg1, vg2, va):
    """Plate and screen current, mA - the equations of deviceCurrents()."""
    G, C, mu, gamma, beta, vk, lam, q, s = p
    vpe = 2 * sp(va / 2)
    Ce = C * (250.0 / vg2) ** s
    ik = G * (sp(Ce * (vg1 + vg2 / mu)) / Ce) ** gamma
    vkn = vk * (vg2 / 250.0) * (max(ik, 1e-12) / 0.1) ** q
    f = (1 - np.exp(-vpe / vkn)) * (1 - beta / (1 + vpe / lam))
    return ik * f * 1e3, ik * (1 - f) * 1e3

def koren(p, vg1, vg2, va):
    mu, ex, kg1, kp, kvb, kg2 = p
    E1 = vg2 / kp * sp(kp * (1 / mu + vg1 / vg2))
    return 2 / kg1 * E1 ** ex * np.arctan(2 * sp(va / 2) / kvb) * 1e3, E1 ** ex / kg2 * 1e3

def points():
    for va, pts in C1_IA.items():
        for vg, ia in pts: yield 'C1 Ia, Va=Vg2=%d' % va, (vg, va, va), 0, ia, 1
    for va, pts in C1_IG2.items():
        for vg, ig in pts: yield 'C1 Ig2, Va=Vg2=%d' % va, (vg, va, va), 1, ig, 1
    for vg, pts in C2.items():
        for va, ia in pts: yield 'C2 Vg2=250, Vg1=%d' % vg, (vg, 250, va), 0, ia, 1
    for va, pts in C3.items():
        for vg, ia in pts: yield 'C3 triode, Va=%d' % va, (vg, va, va), 2, ia, 1
    va, vg2, vg1, ia, ig2 = TAB['pent']
    yield 'TAB Ia', (vg1, vg2, va), 0, ia, 3
    yield 'TAB Ig2', (vg1, vg2, va), 1, ig2, 2
    tva, tvg, tia = TAB['triode']
    yield 'TAB triode Ia', (tvg, tva, tva), 2, tia, 3

def value(model, p, args, kind):
    ip, ig = model(p, *args)
    return ip if kind == 0 else ig if kind == 1 else ip + ig     # triode connected: anode = plate + screen

def residuals(model, p):
    return np.array([w * (value(model, p, a, k) - d) / (0.05 * d + 0.5) for _, a, k, d, w in points()])

def report(model, p, title):
    print(title)
    rows, allv = {}, []
    for tag, a, k, d, w in points():
        if d >= 10 or tag.startswith('TAB') or 'Ig2' in tag:
            e = (value(model, p, a, k) - d) / d
            rows.setdefault(tag, []).append(e)
            allv.append(e)
    for tag, v in rows.items():
        v = np.array(v)
        print("  %-24s rms %5.1f%%  worst %+6.1f%%" % (tag, 100 * np.sqrt(np.mean(v ** 2)), 100 * v[np.argmax(abs(v))]))
    print("  ALL                       rms %5.1f%%\n" % (100 * np.sqrt(np.mean(np.array(allv) ** 2))))

if __name__ == '__main__':
    report(koren, [11.0, 1.35, 650.0, 60.0, 24.0, 4200.0], "Koren's published EL34, against the sheet:")
    best = None
    for C0 in (0.08, 0.15, 0.3):
        for g0 in (1.4, 1.7, 2.1):
            for s0 in (0.0, 0.6, 1.5):
                p0 = [2e-3, C0, 12.0, g0, 0.5, 30.0, 300.0, -0.4, s0]
                f = least_squares(lambda p: residuals(pentode, p), p0,
                                  bounds=([1e-6, 0.005, 3, 1.0, 0.0, 1, 1, -2, -3], [1, 5, 60, 4, 1.0, 300, 1e5, 2, 6]),
                                  x_scale='jac', max_nfev=40000)
                if best is None or f.cost < best.cost: best = f
    print("fitted (paste into pentodeEL34() in Source/Circuit.cpp):")
    print("  " + "  ".join("%s=%.9g" % (n, v) for n, v in zip(NAMES, best.x)))
    report(pentode, best.x, "the fit, against the sheet:")
