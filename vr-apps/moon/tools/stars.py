"""Bake the stars: the Yale Bright Star Catalogue (5th revised ed., BSC5P) from NASA's HEASARC archive, every star to
visual magnitude 6.5, into assets/stars.bin: per star five float32 (x, y, z: unit vector, J2000 equatorial, x to the
vernal equinox, z to the celestial north pole; V magnitude; B-V colour index).

Source (public domain, NASA HEASARC):
  https://heasarc.gsfc.nasa.gov/FTP/heasarc/dbase/tdat_files/heasarc_bsc5p.tdat.gz
saved to %LOCALAPPDATA%\\moon-data\\stars\\bsc5p.tdat.gz (never into the repository).

  python tools/stars.py
"""
import gzip, math, os, struct, sys

SRC = os.path.join(os.environ['LOCALAPPDATA'], 'moon-data', 'stars', 'bsc5p.tdat.gz')
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'assets', 'stars.bin')
MAG = 6.5

lines = gzip.open(SRC, 'rt', encoding='latin-1').read().splitlines()
order = next(l for l in lines if l.startswith('line[1]')).split('=', 1)[1].split()
i0 = next(k for k, l in enumerate(lines) if l.startswith('<DATA>')) + 1
ira, idec, imag, ibv = (order.index(k) for k in ('ra', 'dec', 'vmag', 'bv_color'))
stars = []
for l in lines[i0:]:
    if l.startswith('<END>'):
        break
    f = l.split('|')
    try:
        ra, dec, mag = float(f[ira]), float(f[idec]), float(f[imag])
    except ValueError:
        continue
    bv = float(f[ibv]) if f[ibv].strip() else 0.6
    if mag > MAG:
        continue
    a, d = math.radians(ra), math.radians(dec)
    stars.append((math.cos(d) * math.cos(a), math.cos(d) * math.sin(a), math.sin(d), mag, bv))
stars.sort(key=lambda s: s[3])
with open(OUT, 'wb') as fo:
    for s in stars:
        fo.write(struct.pack('<5f', *s))
print(f'{len(stars)} stars to magnitude {MAG}, brightest {stars[0][3]:.2f}, {os.path.getsize(OUT)} bytes -> {os.path.normpath(OUT)}')
