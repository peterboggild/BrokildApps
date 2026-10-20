"""ECC81 / 12AT7 in the Dempwolf & Zoelzer form - an interim fit.

No ECC81 data sheet could be reached from the build machine, so this fits
the Dempwolf cathode-current form to Norman Koren's published 12AT7 model
(mu 60, ex 1.35, kg1 460, kp 300, kvb 300; Koren, "Improved VT models for
SPICE simulations", 2003), itself a fit to the RCA 12AT7 sheet, over the
region a guitar amp uses: Va 50 - 350 V, Vg -6 .. 0 V, Ip >= 0.1 mA.

It is a fit to a fit, and says so in the solver. Replace it with a fit to
the data sheet itself (as test/el34fit does for the EL34) when one is to
hand.

Run: python3 fit.py      (numpy and scipy)
"""
import numpy as np
from scipy.optimize import least_squares

def koren(va, vg, mu=60.0, ex=1.35, kg1=460.0, kp=300.0, kvb=300.0):
    e1 = va / kp * np.log1p(np.exp(kp * (1.0 / mu + vg / np.sqrt(kvb + va * va))))
    return np.where(e1 > 0, 2.0 * e1 ** ex / kg1, 0.0)        # amps

def softplus(a):
    return np.where(a > 30, a, np.log1p(np.exp(np.minimum(a, 30))))

def dz(p, va, vg):
    G, C, mu, gamma = p
    return G * (softplus(C * (va / mu + vg)) / C) ** gamma

va, vg = np.meshgrid(np.linspace(50, 350, 31), np.linspace(-6, 0, 25))
va, vg = va.ravel(), vg.ravel()
ip = koren(va, vg)
keep = ip >= 1e-4
va, vg, ip = va[keep], vg[keep], ip[keep]

res = lambda p: (dz(p, va, vg) - ip) / (ip + 1e-3)   # relative above 1 mA, absolute-ish below
fit = least_squares(res, [1e-3, 3.0, 60.0, 1.4], bounds=([1e-6, 0.1, 10, 1.0], [1e-1, 20, 150, 2.5]))
G, C, mu, gamma = fit.x
rel = np.abs(dz(fit.x, va, vg) - ip) / ip
big = ip >= 1e-3
print(f"G={G:.8e} C={C:.8f} mu={mu:.8f} gamma={gamma:.8f}")
print(f"{keep.sum()} points; rms error {100*np.sqrt(np.mean(rel[big]**2)):.2f}% at >= 1 mA, worst {100*rel[big].max():.2f}%")
