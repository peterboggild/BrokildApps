"""Build the Moon for Journey to the Moon from NASA source data.

The Moon is a set of nested, lat/lon-aligned levels centred on the Apollo 17
landing site, each with its own mesh and one RGBA map:

    G    whole Moon          1.4 km/px   CGI Moon Kit (LOLA 64 px/deg)
    L1   +-12 deg            355 m/px    LOLA 64 px/deg
    L2   +-4 deg (121 km)    59 m/px     SLDEM2015 512 px/deg
    L3   +-22 km             11 m/px     LROC NAC DTM 5 m
    L4   +-5 km              2.4 m/px    LROC NAC DTM 2 m
    L5   +-1 km              0.5 m/px    NAC 2 m + generated craters
    L6   +-150 m             7 cm/px     generated craters, soil relief, rocks

Map channels: R,G = surface normal (east, north), B = elevation angle of the
terrain horizon in the direction of the Sun (so the shader gets cast shadows
for any Sun height by one comparison), A = albedo detail (128 = 1).

Everything is baked here so the headset only does texture lookups.

    python build.py                # all levels, PNG maps (fast, for development)
    python build.py --ktx          # also encode KTX2 (slow)
    python build.py L5 L6          # only these (parents are still computed, not written)

Source data: download.py. Output: ../assets/moon/.
"""
import json, math, os, subprocess, sys, time
import numpy as np
from scipy import ndimage as ndi
import tifffile
from PIL import Image

Image.MAX_IMAGE_PIXELS = None
HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.environ.get("MOON_DATA") or os.path.join(os.environ["LOCALAPPDATA"], "moon-data")
OUT = os.path.join(HERE, "..", "assets", "moon")
CACHE = os.path.join(DATA, "cache")

R_MOON = 1737400.0
SITE_LAT, SITE_LON = 20.1911, 30.7723          # Apollo 17 LM descent stage (LROC)
SUN_EL, SUN_AZ = 13.3, 95.5                     # at landing (Apollo 17 traverse planning data)
D2R = math.pi / 180

H_MIN, H_RANGE = -0.02, 0.52                    # horizon angle encoding (radians) in the B channel

t00 = time.time()


def log(*a):
    print(f"[{time.time() - t00:6.1f}s]", *a, flush=True)


# ---------------------------------------------------------------- frames
def unit(lat, lon):
    """Moon-fixed unit vector (x to 0N 0E, z to the north pole). Degrees in."""
    la, lo = np.asarray(lat) * D2R, np.asarray(lon) * D2R
    return np.stack([np.cos(la) * np.cos(lo), np.cos(la) * np.sin(lo), np.sin(la)], -1)


def enu(lat, lon):
    la, lo = np.asarray(lat) * D2R, np.asarray(lon) * D2R
    e = np.stack([-np.sin(lo), np.cos(lo), np.zeros_like(lo + la)], -1)
    n = np.stack([-np.sin(la) * np.cos(lo), -np.sin(la) * np.sin(lo), np.cos(la) + 0 * lo], -1)
    return e, n, unit(lat, lon)


E0, N0, U0 = enu(SITE_LAT, SITE_LON)
_sl = np.array([math.cos(SUN_EL * D2R) * math.sin(SUN_AZ * D2R),      # east
                math.cos(SUN_EL * D2R) * math.cos(SUN_AZ * D2R),      # north
                math.sin(SUN_EL * D2R)])                              # up
SUN = E0 * _sl[0] + N0 * _sl[1] + U0 * _sl[2]                         # Moon-fixed, unit


def to_local(lat, lon, h, r0):
    """Site frame: X east, Y up, Z south, origin on the ground at the LM. float64 in, float32 out."""
    p = unit(lat, lon) * (R_MOON + np.asarray(h))[..., None]
    return np.stack([p @ E0, p @ U0 - r0, -(p @ N0)], -1).astype(np.float32)


# ---------------------------------------------------------------- sources
class Equirect:
    """A lat/lon raster: heights (metres above the 1737.4 km sphere) or anything else."""

    def __init__(self, arr, lat_max, lat_min, lon_min, lon_max, scale=1.0, offset=0.0, nodata=None, wrap=False):
        # arr may be (loader, shape): the file is then read when first needed (the rasters are large)
        self._a, self._load = (None, arr[0]) if isinstance(arr, tuple) else (arr, None)
        self.scale, self.offset, self.nodata, self.wrap = scale, offset, nodata, wrap
        self.lat_max, self.lat_min, self.lon_min, self.lon_max = lat_max, lat_min, lon_min, lon_max
        self.ny, self.nx = (arr[1] if isinstance(arr, tuple) else arr.shape)[:2]

    @property
    def a(self):
        if self._a is None:
            self._a = self._load()
        return self._a

    def pix(self, lat, lon):
        x = (lon - self.lon_min) / (self.lon_max - self.lon_min) * self.nx - 0.5
        y = (self.lat_max - lat) / (self.lat_max - self.lat_min) * self.ny - 0.5
        return y, x

    def sample(self, lat, lon, order=1):
        """lat, lon: 2D arrays (degrees). Returns float32, NaN where there is no data."""
        y, x = self.pix(lat, lon)
        y0, y1 = int(max(0, math.floor(np.min(y)) - 3)), int(min(self.ny, math.ceil(np.max(y)) + 4))
        x0, x1 = int(max(0, math.floor(np.min(x)) - 3)), int(min(self.nx, math.ceil(np.max(x)) + 4))
        if y1 <= y0 or x1 <= x0:
            return np.full(lat.shape, np.nan, np.float32)
        win = np.asarray(self.a[y0:y1, x0:x1]).astype(np.float32)
        bad = None
        if self.nodata is not None:
            bad = (win < self.nodata) | ~np.isfinite(win)
            if bad.any():
                # holes: put a harmless value in, and report every sample that touches one as NaN
                win[bad] = np.float32(np.median(win[~bad][::97])) if (~bad).any() else 0
                bad = ndi.binary_dilation(bad, iterations=2 if order > 1 else 1)
            else:
                bad = None
        if self.scale != 1.0 or self.offset != 0.0:
            win *= np.float32(self.scale)
            win += np.float32(self.offset)
        mode = "grid-wrap" if self.wrap else "nearest"
        out = np.empty(lat.shape, np.float32)
        step = max(1, 4_000_000 // lat.shape[1])
        for r in range(0, lat.shape[0], step):
            c = np.stack([y[r:r + step] - y0, x[r:r + step] - x0])
            out[r:r + step] = ndi.map_coordinates(win, c, order=order, mode=mode, prefilter=order > 1)
            if bad is not None:
                m = ndi.map_coordinates(bad.view(np.uint8), c, order=0, mode="nearest")
                out[r:r + step][m > 0] = np.nan
        outside = (y < -0.5) | (y > self.ny - 0.5) | (x < -0.5) | (x > self.nx - 0.5)
        if not self.wrap:
            out[outside] = np.nan
        return out


def pds_label(path):
    d = {}
    for line in open(path, encoding="latin1"):
        if "=" in line:
            k, v = line.split("=", 1)
            d[k.strip()] = v.split("<")[0].strip().strip('"')
    return d


def as_memmap(tif_name):
    """The rasters are too large to hold in memory beside the work arrays: convert each once to a .npy file
    next to the source and map it, so only the windows that are sampled are ever read."""
    src = os.path.join(DATA, tif_name)
    dst = os.path.join(DATA, "cache", tif_name + ".npy")
    if not os.path.exists(dst) or os.path.getmtime(dst) < os.path.getmtime(src):
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        log(f"  converting {tif_name} to a mapped file (once)")
        arr = tifffile.imread(src)
        np.save(dst + ".tmp.npy", arr)
        del arr
        os.replace(dst + ".tmp.npy", dst)
    return np.load(dst, mmap_mode="r")


def load_sources():
    S = {}
    log("reading ldem_64 (global LOLA heights)")
    # uint16 half-metres above a 1727.4 km sphere (checked against the float file ldem_16.tif)
    u = as_memmap("ldem_64_uint.tif")
    S["ldem64"] = Equirect(u, 90, -90, -180, 180, scale=0.5, offset=-10000.0, wrap=True)
    p = os.path.join(DATA, "SLDEM2015_512_00N_30N_000_045_FLOAT.IMG")
    if os.path.exists(p) and os.path.getsize(p) == 1415577600:
        lbl = pds_label(p[:-3] + "LBL")
        sc = float(lbl.get("SCALING_FACTOR", 1.0)) * (1000.0 if "KILOMETER" in lbl.get("UNIT", "KILOMETER").upper() else 1.0)
        mm = np.memmap(p, np.dtype("<f4"), "r", shape=(15360, 23040))
        S["sldem"] = Equirect(mm, 30, 0, 0, 45, scale=sc)
        log(f"  SLDEM2015 512 px/deg, scale {sc}")
    for key, base in (("nac5", "NAC_DTM_APOLLO17"), ("nac2", "NAC_DTM_APOLLO17_2M")):
        p = os.path.join(DATA, base + ".TIF")
        if not os.path.exists(p):
            continue
        lbl = pds_label(p[:-3] + "LBL")
        S[key] = Equirect(as_memmap(base + ".TIF"), float(lbl["MAXIMUM_LATITUDE"]), float(lbl["MINIMUM_LATITUDE"]),
                          float(lbl["WESTERNMOST_LONGITUDE"]), float(lbl["EASTERNMOST_LONGITUDE"]), nodata=-1e5)
    return S


# ---------------------------------------------------------------- level geometry
class Level:
    def __init__(self, name, half_m, T, N, src, warp=1.0, parent=None, glob=False):
        self.name, self.src, self.parent, self.warp, self.glob = name, src, parent, warp, glob
        if glob:
            self.lat0, self.lon0, self.dlat, self.dlon = 0.0, 0.0, 90.0, 180.0
            self.Tx, self.Ty, self.Nx, self.Ny = T, T // 2, N, N // 2
        else:
            self.lat0, self.lon0 = SITE_LAT, SITE_LON
            self.dlat = half_m / R_MOON / D2R
            self.dlon = self.dlat / math.cos(SITE_LAT * D2R)
            self.Tx = self.Ty = T
            self.Nx = self.Ny = N
        self.h = self.H = self.pos = None

    # texel centres
    def texel_latlon(self):
        lon = self.lon0 - self.dlon + (np.arange(self.Tx) + 0.5) / self.Tx * 2 * self.dlon
        lat = self.lat0 + self.dlat - (np.arange(self.Ty) + 0.5) / self.Ty * 2 * self.dlat
        if self.glob:
            lat, lon = lat.astype(np.float32), lon.astype(np.float32)
        return np.meshgrid(lat, lon, indexing="ij")

    def metres_per_texel(self):
        dn = 2 * self.dlat * D2R * R_MOON / self.Ty
        lat = self.lat0 + self.dlat - (np.arange(self.Ty) + 0.5) / self.Ty * 2 * self.dlat
        de = 2 * self.dlon * D2R * R_MOON * np.maximum(np.cos(lat * D2R), 1e-3) / self.Tx
        return de.astype(np.float32)[:, None], np.float32(dn)       # east (per row), north

    def w(self, s):
        """Vertex warp: s in [-1,1] -> fraction of the half extent in [-1,1]; denser at the centre for warp<1."""
        return s * (self.warp + (1 - self.warp) * s * s)

    def w_inv(self, f):
        s = np.linspace(-1, 1, 4001)
        return np.interp(f, self.w(s), s)

    def vertex_latlon(self):
        fx = self.w(np.linspace(-1, 1, self.Nx + 1))
        fy = self.w(np.linspace(-1, 1, self.Ny + 1))
        return np.meshgrid(self.lat0 - self.dlat * fy, self.lon0 + self.dlon * fx, indexing="ij")

    def tex_coords(self, lat, lon):
        """Fractional texel coordinates (row, col) of lat/lon in this level's map."""
        x = (lon - (self.lon0 - self.dlon)) / (2 * self.dlon) * self.Tx - 0.5
        y = ((self.lat0 + self.dlat) - lat) / (2 * self.dlat) * self.Ty - 0.5
        return y, x

    def sample_map(self, arr, lat, lon, order=1):
        y, x = self.tex_coords(lat, lon)
        out = np.empty(lat.shape, np.float32)
        step = max(1, 4_000_000 // lat.shape[1])
        mode = "grid-wrap" if self.glob else "nearest"
        for r in range(0, lat.shape[0], step):
            out[r:r + step] = ndi.map_coordinates(arr, np.stack([y[r:r + step], x[r:r + step]]), order=order,
                                                  mode=mode, prefilter=False)
        return out

    def surface(self, lat, lon):
        """The 3D point of this level's MESH (flat triangles, as drawn) under lat/lon. For matching children."""
        fi = (self.w_inv((lon - self.lon0) / self.dlon) + 1) / 2 * self.Nx
        fj = (self.w_inv((self.lat0 - lat) / self.dlat) + 1) / 2 * self.Ny
        ci = np.clip(np.floor(fi).astype(int), 0, self.Nx - 1)
        cj = np.clip(np.floor(fj).astype(int), 0, self.Ny - 1)
        fx, fy = (fi - ci)[..., None], (fj - cj)[..., None]
        P = self.pos.astype(np.float64)
        A, B, C, Dd = P[cj, ci], P[cj + 1, ci], P[cj, ci + 1], P[cj + 1, ci + 1]
        t1 = A + fx * (C - A) + fy * (B - A)
        t2 = Dd + (1 - fx) * (B - Dd) + (1 - fy) * (C - Dd)
        return np.where(fx + fy <= 1, t1, t2)


def edge_weight(n_y, n_x, band):
    """0 at the border of a map, rising smoothly to 1 over `band` (fraction of the width)."""
    def ramp(n):
        d = np.minimum(np.arange(n) + 0.5, n - 0.5 - np.arange(n)) / n
        t = np.clip(d / band, 0, 1)
        return (t * t * (3 - 2 * t)).astype(np.float32)
    return np.minimum.outer(ramp(n_y), ramp(n_x))


# ---------------------------------------------------------------- horizon (baked shadows)
def horizon(h_cast, h_recv, de, dn, dir_e, dir_n, max_px, start_px=1.0, wrap=False):
    """Elevation angle (rad) of the terrain horizon from every texel, looking towards the Sun's azimuth.

    de (per row) and dn are metres per texel; dir_e, dir_n the horizontal unit direction to the Sun (scalars
    or per-texel arrays). Short steps first, then geometric steps sampled from max-pooled heights so that
    thin casters are not stepped over."""
    Ty, Tx = h_recv.shape
    H = np.full((Ty, Tx), H_MIN, np.float32)
    steps, d = [], float(start_px)
    while d <= max_px:
        steps.append(d)
        d = d + 1.5 if d < 40 else d * 1.13
    if not steps:
        return H
    pyr, pyr_k = {1: h_cast}, 1
    mode = "grid-wrap" if wrap else "nearest"
    px_m = float(dn)                                        # nominal metres per pixel step
    chunk = max(1, 3_000_000 // Tx)
    jj = np.arange(Ty, dtype=np.float32)[:, None]
    ii = np.arange(Tx, dtype=np.float32)[None, :]
    scal = np.isscalar(dir_e)
    for k, d in enumerate(steps):
        gap = (steps[k] - steps[k - 1]) if k else max(1.0, 0.13 * d)
        pool = 1
        while pool * 2 <= gap:
            pool *= 2
        if pool != pyr_k:                                   # only one pooled copy in memory at a time
            pyr, pyr_k = {pool: ndi.maximum_filter(h_cast, size=pool + 1, mode=mode)}, pool
        src = pyr[pool]
        dist = d * px_m
        for r in range(0, Ty, chunk):
            s = slice(r, r + chunk)
            if scal:
                y = jj[s] - (dist * dir_n) / dn + 0 * ii
                x = ii + (dist * dir_e) / de[s]
            else:
                y = jj[s] - (dist * dir_n[s]) / dn
                x = ii + (dist * dir_e[s]) / de[s]
            hs = ndi.map_coordinates(src, np.stack([np.broadcast_to(y, x.shape), x]), order=1, mode=mode, prefilter=False)
            ang = np.arctan((hs - h_recv[s]) / dist) - dist / (2 * R_MOON)
            np.maximum(H[s], ang, out=H[s])
    return H


# ---------------------------------------------------------------- generated detail
def crater_profile(r, depth, rim):
    """Height of a simple bowl crater against r (in crater radii). depth and rim in metres."""
    inside = rim - (depth + rim) * (1 - np.minimum(r, 1) ** 2) ** 1.0
    outside = rim * np.minimum(r, 9.0) ** -3.0
    return np.where(r <= 1, inside, outside)


def make_craters(rng, half_m, d_min, d_max, density=0.05, keepout=None):
    """Random craters in equilibrium-like numbers: N(>D) = density * D^-2 per m^2. Returns x, y, D, depth/D, fresh."""
    area = (2 * half_m) ** 2
    n = int(area * density * (d_min ** -2 - d_max ** -2))
    u = rng.random(n)
    D = (d_min ** -2 - u * (d_min ** -2 - d_max ** -2)) ** -0.5
    x, y = (rng.random((2, n)) * 2 - 1) * half_m
    fresh = rng.random(n) ** 2.6                       # most are old and soft; a few are sharp
    ratio = 0.025 + 0.175 * fresh                      # depth / diameter
    if keepout is not None:
        ok = ~((np.hypot(x, y) < keepout) & (D > 1.2))
        x, y, D, ratio, fresh = x[ok], y[ok], D[ok], ratio[ok], fresh[ok]
    order = np.argsort(-D)                             # big first, small ones cut into them
    return x[order], y[order], D[order], ratio[order], fresh[order]


def stamp_craters(h, alb, craters, half_e, half_n):
    """Add craters to a height map (and brighten fresh ones in the albedo map). x east, y north, metres from site."""
    Ty, Tx = h.shape
    sx, sy = Tx / (2 * half_e), Ty / (2 * half_n)      # px per metre
    x, y, D, ratio, fresh = craters
    for k in range(len(D)):
        rad = D[k] / 2
        ext = rad * (2.6 if fresh[k] > 0.3 else 1.8)
        cx, cy = (x[k] + half_e) * sx, (half_n - y[k]) * sy
        i0, i1 = int(cx - ext * sx), int(cx + ext * sx) + 2
        j0, j1 = int(cy - ext * sy), int(cy + ext * sy) + 2
        if i1 <= 0 or j1 <= 0 or i0 >= Tx or j0 >= Ty or rad * sx < 0.7:
            continue
        i0, j0, i1, j1 = max(i0, 0), max(j0, 0), min(i1, Tx), min(j1, Ty)
        X = (np.arange(i0, i1) + 0.5 - cx) / sx
        Y = (np.arange(j0, j1) + 0.5 - cy) / sy
        r = np.hypot(X[None, :], Y[:, None]) / rad
        depth = ratio[k] * D[k]
        rim = 0.2 * depth * (0.3 + 0.7 * fresh[k])
        fade = np.clip((ext / rad - r) / (0.35 * ext / rad), 0, 1)
        prof = crater_profile(r, depth, rim)
        if fresh[k] < 0.5:                             # old craters: rounded rim (blend towards a soft bowl)
            soft = -depth * np.exp(-(r * 1.25) ** 2 * 1.6) * 0.9
            prof = prof * (fresh[k] * 2) + soft * (1 - fresh[k] * 2)
        h[j0:j1, i0:i1] += (prof * fade).astype(np.float32)
        if alb is not None and fresh[k] > 0.85 and D[k] > 0.8:
            alb[j0:j1, i0:i1] += ((fresh[k] - 0.85) * 1.6 * np.exp(-(r / 1.6) ** 2) * fade).astype(np.float32)


def band_noise(rng, n, px_m, wl_min, wl_max, slope=-1.6):
    """Tileable band-limited noise, unit rms: wavelengths between wl_min and wl_max metres."""
    f = np.hypot(*np.meshgrid(np.fft.fftfreq(n, px_m), np.fft.rfftfreq(n, px_m), indexing="ij"))
    amp = np.where((f > 1 / wl_max) & (f < 1 / wl_min), np.maximum(f, 1e-9) ** slope, 0)
    spec = amp * np.exp(2j * np.pi * rng.random(f.shape))
    z = np.fft.irfft2(spec, s=(n, n)).astype(np.float32)
    return z / z.std()


def make_rocks(rng, craters, half_m):
    """Rocks: a thin background plus blocks thrown out of a few sharp craters. Returns (n,5): x, y, size, yaw, shape."""
    out = []
    nb = int((2 * half_m) ** 2 * 0.0035)
    xy = (rng.random((nb, 2)) * 2 - 1) * half_m
    sz = 0.10 * (1 - rng.random(nb) * (1 - (0.10 / 1.1) ** 2.2)) ** (-1 / 2.2)
    out.append(np.column_stack([xy, sz]))
    x, y, D, ratio, fresh = craters
    blocky = np.where((D > 2.5) & (fresh > 0.55) & (np.abs(x) < half_m) & (np.abs(y) < half_m))[0][:26]
    for k in blocky:
        n = int(16 * D[k] * fresh[k] ** 1.5 * (0.5 + rng.random()))
        rr = D[k] / 2 * (0.75 + 2.6 * rng.random(n) ** 1.7)
        th = rng.random(n) * 2 * np.pi
        sz = 0.09 * (1 - rng.random(n) * (1 - (0.09 / (0.22 * D[k])) ** 2.0)) ** (-1 / 2.0)
        out.append(np.column_stack([x[k] + rr * np.cos(th), y[k] + rr * np.sin(th), sz]))
    r = np.concatenate(out)
    r = r[(np.hypot(r[:, 0], r[:, 1]) > 5.5) & (np.abs(r[:, 0]) < half_m - 2) & (np.abs(r[:, 1]) < half_m - 2)]
    return np.column_stack([r, rng.random(len(r)) * 2 * np.pi, rng.integers(0, 6, len(r))]).astype(np.float32)


def stamp_blobs(h, x, y, radius, height, half_e, half_n):
    """Raise rounded bumps (rocks, the LM stand-in) in a caster height map, for the shadow bake only."""
    Ty, Tx = h.shape
    sx, sy = Tx / (2 * half_e), Ty / (2 * half_n)
    for k in range(len(x)):
        cx, cy = (x[k] + half_e) * sx, (half_n - y[k]) * sy
        i0, i1 = max(int(cx - radius[k] * sx) - 1, 0), min(int(cx + radius[k] * sx) + 2, Tx)
        j0, j1 = max(int(cy - radius[k] * sy) - 1, 0), min(int(cy + radius[k] * sy) + 2, Ty)
        if i1 <= i0 or j1 <= j0:
            continue
        X = (np.arange(i0, i1) + 0.5 - cx) / sx
        Y = (np.arange(j0, j1) + 0.5 - cy) / sy
        r2 = (X[None, :] ** 2 + Y[:, None] ** 2) / radius[k] ** 2
        bump = height[k] * np.sqrt(np.clip(1 - r2 ** 2, 0, 1))
        base = h[int(np.clip(cy, 0, Ty - 1)), int(np.clip(cx, 0, Tx - 1))]
        h[j0:j1, i0:i1] = np.where(bump > 0, np.maximum(h[j0:j1, i0:i1], base + bump), h[j0:j1, i0:i1])


# ---------------------------------------------------------------- output
def write_map(lv, nrm_e, nrm_n, H, alb, want, ktx):
    if not want:
        return
    img = np.empty((lv.Ty, lv.Tx, 4), np.uint8)
    # level ground must decode to exactly zero slope, and the rounding is dithered: close to the terminator one
    # 8-bit step of the normal is a fifth of the brightness, and undithered it shows as bands between levels
    dith = np.random.default_rng(7).random((2, lv.Ty, lv.Tx), dtype=np.float32)
    img[..., 0] = np.clip(np.floor(nrm_e * 127 + 128 + dith[0]), 1, 255)
    img[..., 1] = np.clip(np.floor(nrm_n * 127 + 128 + dith[1]), 1, 255)
    del dith
    img[..., 2] = np.clip((H - H_MIN) / H_RANGE * 255 + 0.5, 0, 255)
    img[..., 3] = np.clip(alb * 128, 0, 255) if alb is not None else 128
    Image.fromarray(img, "RGBA").save(os.path.join(OUT, lv.name + ".png"), compress_level=3)
    if ktx:
        encode_ktx(img, lv.name, False)
    log(f"  wrote {lv.name} map {lv.Tx}x{lv.Ty}")


def encode_ktx(img, name, srgb):
    os.makedirs(CACHE, exist_ok=True)
    raw = os.path.join(CACHE, name + ".rgba")
    if img.shape[2] == 3:
        img = np.dstack([img, np.full(img.shape[:2], 255, np.uint8)])
    img.tofile(raw)
    args = ["node", os.path.join(HERE, "ktx2.mjs"), raw, str(img.shape[1]), str(img.shape[0]),
            os.path.join(OUT, name + ".ktx2"), "q=1", "rdo=0"] + (["srgb", "etc1s"] if srgb else [])
    subprocess.run(args, check=True)
    os.remove(raw)


def normals(h, de, dn):
    gj, gi = np.gradient(h)
    ge, gn = gi / de, -gj / dn
    inv = 1.0 / np.sqrt(ge * ge + gn * gn + 1)
    return (-ge * inv).astype(np.float32), (-gn * inv).astype(np.float32)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    ktx = "--ktx" in sys.argv
    want = lambda n: not args or n in args
    os.makedirs(OUT, exist_ok=True)
    S = load_sources()
    rng = np.random.default_rng(1972)

    # ---- the ground under the LM defines the origin
    one = np.array([[SITE_LAT]]), np.array([[SITE_LON]])
    h_site = float(S["nac2"].sample(*one, order=1)[0, 0])
    r0 = R_MOON + h_site
    log(f"site height {h_site:.1f} m (relative to 1737.4 km)")

    G = Level("G", None, 7680, 180, "ldem64", glob=True)
    L = [G,
         Level("L1", 12 * D2R * R_MOON, 2048, 192, "ldem64"),
         Level("L2", 4 * D2R * R_MOON, 4096, 192, "sldem"),
         Level("L3", 22000, 4096, 192, "nac5"),
         Level("L4", 5000, 4096, 192, "nac2"),
         Level("L5", 1000, 4096, 192, "nac2"),
         Level("L6", 150, 4096, 256, "nac2", warp=0.3)]
    for a, b in zip(L[:-1], L[1:]):
        b.parent = a
    manifest = {"radius": R_MOON, "r0": r0, "site": {"lat": SITE_LAT, "lon": SITE_LON, "height": h_site},
                "sun": {"elevation": SUN_EL, "azimuth": SUN_AZ,
                        "local": [float(_sl[0]), float(_sl[2]), float(-_sl[1])]},
                "hEncode": [H_MIN, H_RANGE], "levels": []}

    craters5 = make_craters(rng, 1050, 1.5, 7.0, keepout=7.0)
    craters6 = make_craters(rng, 165, 0.28, 1.5)
    rocks = None

    for lv in L:
        log(f"{lv.name}: heights")
        lat, lon = lv.texel_latlon()
        de, dn = lv.metres_per_texel()
        alb = None
        if lv.glob:
            u = S["ldem64"].a
            h = np.empty((3840, 7680), np.float32)
            for r in range(0, 3840, 256):
                blk = np.asarray(u[r * 3:(r + 256) * 3]).reshape(-1, 3, 7680, 3).mean((1, 3), dtype=np.float64)
                h[r:r + 256] = blk * S["ldem64"].scale + S["ldem64"].offset
        else:
            src = S.get(lv.src)
            par_h = lv.parent.sample_map(lv.parent.h, lat, lon, order=3)
            if src is None:
                log(f"  source {lv.src} missing: using the parent's data")
                h = par_h.copy()
            else:
                h = src.sample(lat, lon, order=3 if lv.name in ("L5", "L6") else 1)
                if lv.src == "nac2":                       # outside the 2 m model, fall back on the 5 m one
                    m = ~np.isfinite(h)
                    if m.any():
                        h5 = S["nac5"].sample(lat, lon, order=1)
                        valid = ndi.gaussian_filter((~m).astype(np.float32), 12)
                        wv = np.clip(valid * 2 - 1, 0, 1)
                        h = np.where(np.isfinite(h), h, h5) * wv + np.where(np.isfinite(h5), h5, par_h) * (1 - wv)
                m = ~np.isfinite(h)
                if m.any():
                    log(f"  {m.mean() * 100:.1f} % without data, filled from the parent")
                    valid = ndi.gaussian_filter((~m).astype(np.float32), 10)
                    wv = np.clip(valid * 2 - 1, 0, 1)
                    h = np.where(m, par_h, h) * wv + par_h * (1 - wv)
            if lv.name in ("L5", "L6"):
                # the stereo model carries metre-scale noise that turns into lumps when magnified: smooth it
                # below its real resolution, then put the missing small scales back as craters and soil relief
                h = ndi.gaussian_filter(h, 2.0 / float(dn))
                alb = np.ones_like(h)
                stamp_craters(h, alb, craters5, lv.dlon * D2R * R_MOON * math.cos(SITE_LAT * D2R), lv.dlat * D2R * R_MOON)
                if lv.name == "L5":
                    h += band_noise(rng, lv.Tx, float(dn), 1.5, 14.0) * 0.035
                else:
                    stamp_craters(h, alb, craters6, 150.0, 150.0)
                    nz = band_noise(rng, lv.Tx, float(dn), 0.16, 14.0, slope=-1.9)
                    h += nz * 0.022
                    alb *= 1 + 0.035 * band_noise(rng, lv.Tx, float(dn), 0.5, 20.0)
            # melt into the parent at the border so the two levels meet without a step
            wgt = edge_weight(lv.Ty, lv.Tx, 0.07)
            h = par_h + (h - par_h) * wgt
            if alb is not None:
                alb = 1 + (alb - 1) * wgt
        lv.h = h.astype(np.float32)

        # ---- baked horizon towards the Sun
        log(f"{lv.name}: horizon")
        if lv.glob or lv.name == "L1":
            la, lo = (lat[:, :1] * D2R).astype(np.float32), (lon[:1, :] * D2R).astype(np.float32)
            sx, sy, sz = (np.float32(v) for v in SUN)
            s_e = -np.sin(lo) * sx + np.cos(lo) * sy + 0 * la
            s_n = -np.sin(la) * (np.cos(lo) * sx + np.sin(lo) * sy) + np.cos(la) * sz
            nrm = np.maximum(np.hypot(s_e, s_n), 1e-4)
            s_e, s_n = (s_e / nrm).astype(np.float32), (s_n / nrm).astype(np.float32)
        else:
            s_e, s_n = float(math.sin(SUN_AZ * D2R)), float(math.cos(SUN_AZ * D2R))
        h_cast = lv.h
        if lv.name == "L6":
            if rocks is None:
                rk = make_rocks(rng, [np.concatenate([a, b]) for a, b in zip(craters5, craters6)], 150.0)
                gy, gx = lv.tex_coords(SITE_LAT + rk[:, 1] / R_MOON / D2R,
                                       SITE_LON + rk[:, 0] / (R_MOON * math.cos(SITE_LAT * D2R)) / D2R)
                gh = ndi.map_coordinates(lv.h, np.stack([gy, gx]), order=1, mode="nearest")
                rocks = np.column_stack([rk[:, 0], gh - h_site, -rk[:, 1], rk[:, 2], rk[:, 3], rk[:, 4]]).astype(np.float32)
            h_cast = lv.h.copy()
            stamp_blobs(h_cast, rocks[:, 0], -rocks[:, 2], rocks[:, 3] * 0.5, rocks[:, 3] * 0.62, 150.0, 150.0)
            # stand-in for the LM (stage 5 replaces it with a real shadow map): descent stage and cabin
            stamp_blobs(h_cast, np.array([0.0, 0.0]), np.array([0.0, 0.0]), np.array([2.3, 1.7]), np.array([3.3, 6.6]), 150.0, 150.0)
        max_px = 420 if lv.glob else (700 if lv.name == "L1" else 1500)
        H = horizon(h_cast, lv.h, de, dn, s_e, s_n, max_px, wrap=lv.glob)
        lv.dir, lv.max_px, lv.de, lv.dn = (s_e, s_n), max_px, de, dn
        lv.Hinh = None
        if lv.parent is not None:
            # occluders beyond this level's own reach come from the parent (and its parents), sampled coarsely
            par = lv.parent
            far = horizon(par.h, par.h, par.de, par.dn, par.dir[0], par.dir[1], par.max_px,
                          start_px=max_px * float(dn) / float(par.dn), wrap=par.glob)
            if par.Hinh is not None:
                far = np.maximum(far, par.Hinh)
            lv.Hinh = lv.parent.sample_map(far, lat, lon, order=1)
            H = np.maximum(H, lv.Hinh)
            par_H = lv.parent.sample_map(lv.parent.H, lat, lon, order=1)
            wgt = edge_weight(lv.Ty, lv.Tx, 0.15)      # wide: fine levels have a rougher, higher horizon
            H = par_H + (H - par_H) * wgt
        lv.H = H.astype(np.float32)

        if want(lv.name):
            ne, nn = normals(lv.h, de, dn)
            write_map(lv, ne, nn, lv.H, alb, True, ktx)
            del ne, nn

        # ---- mesh
        vlat, vlon = lv.vertex_latlon()
        sig = 0.5 * lv.Tx / lv.Nx * (lv.warp if lv.warp < 1 else 1.0)
        hb = ndi.gaussian_filter(lv.h, sig, mode="wrap" if lv.glob else "nearest") if sig > 0.6 else lv.h
        hv = lv.sample_map(hb, vlat, vlon, order=1)
        pos = to_local(vlat, vlon, hv, r0)
        if lv.parent is not None:
            ps = lv.parent.surface(vlat, vlon)
            ring = np.minimum.outer(np.minimum(np.arange(lv.Ny + 1), lv.Ny - np.arange(lv.Ny + 1)),
                                    np.minimum(np.arange(lv.Nx + 1), lv.Nx - np.arange(lv.Nx + 1))).astype(np.float32)
            t = np.clip((ring - 1) / 7.0, 0, 1)
            t = (t * t * (3 - 2 * t))[..., None]
            pos = (ps + (pos.astype(np.float64) - ps) * t).astype(np.float32)
        lv.pos = pos
        if want(lv.name):
            pos.tofile(os.path.join(OUT, lv.name + ".pos.bin"))
        manifest["levels"].append({"name": lv.name, "lat0": lv.lat0, "lon0": lv.lon0, "dlat": lv.dlat, "dlon": lv.dlon,
                                   "nx": lv.Nx, "ny": lv.Ny, "warp": lv.warp, "tx": lv.Tx, "ty": lv.Ty,
                                   "mPerTexel": float(dn)})
        if lv.name == "L4":
            # The valley's heights for the headset (the ground generated round the walker stands on them):
            # 2048 x 2048 over this level, uint16 steps above a floor, row-wise differences, gzip.
            import gzip
            v = lv.h.reshape(2048, 2, 2048, 2).mean((1, 3)) - h_site
            lo, hi = float(v.min()), float(v.max())
            unit = max(0.02, (hi - lo) / 60000.0)
            q = np.round((v - lo) / unit).astype(np.int64)
            dq = q.copy()
            dq[:, 1:] -= q[:, :-1]
            with open(os.path.join(OUT, "valley.dat"), "wb") as f:
                f.write(gzip.compress((dq & 0xFFFF).astype("<u2").tobytes(), 9))
            manifest["valley"] = {"n": 2048, "floor": lo, "unit": unit, "level": "L4"}
            log(f"  valley heights {lo:.0f} .. {hi:.0f} m about the site, unit {unit * 100:.1f} cm")
            # and the craters stamped into L5 (their shading is in L5's map; the headset needs their shape)
            cx_, cy_, cd_, cr_, cf_ = craters5
            rec = np.zeros(len(cd_), dtype=[("x", "<i2"), ("y", "<i2"), ("d", "<u2"), ("r", "u1"), ("f", "u1")])
            rec["x"], rec["y"] = np.round(cx_ * 30), np.round(cy_ * 30)
            rec["d"], rec["r"], rec["f"] = np.round(cd_ * 1000), np.round(cr_ * 1000), np.round(cf_ * 255)
            with open(os.path.join(OUT, "craters.dat"), "wb") as f:
                f.write(gzip.compress(rec.tobytes(), 9))
            manifest["craters"] = {"n": int(len(cd_)), "xyScale": 30, "dScale": 1000, "ratioScale": 1000, "level": "L5", "edgeBand": 0.07}
        if lv.name == "L6":
            # walking heights (cm above the origin), 1025 x 1025 over the level
            wl = np.linspace(-1, 1, 1025)
            wlat, wlon = np.meshgrid(lv.lat0 - lv.dlat * wl, lv.lon0 + lv.dlon * wl, indexing="ij")
            wh = lv.sample_map(ndi.gaussian_filter(lv.h, 1.5), wlat, wlon)
            np.round((wh - h_site) * 100).astype("<i2").tofile(os.path.join(OUT, "walk.bin"))
            rocks.tofile(os.path.join(OUT, "rocks.bin"))
            manifest["walk"] = {"n": 1025, "half": 150.0}
            manifest["rocks"] = int(len(rocks))
            log(f"  {len(rocks)} rocks")
        # free what children do not need
        if lv.name == "L1":
            S.pop("ldem64", None)
        gp = lv.parent.parent if lv.parent is not None else None
        if gp is not None:                                  # nothing below needs the grandparent any more
            gp.h = gp.H = gp.Hinh = gp.dir = gp.pos = None
        del lat, lon
        import gc
        gc.collect()

    # ---- colour
    if want("G") or want("color"):
        log("colour maps")
        c8 = tifffile.imread(os.path.join(DATA, "lroc_color_poles_8k.tif"))[..., :3]
        Image.fromarray(c8).save(os.path.join(OUT, "Gcolor.jpg"), quality=90)
        if ktx:
            encode_ktx(c8, "Gcolor", True)
        big = tifffile.imread(os.path.join(DATA, "lroc_color_poles.tif"))
        lv = L[1]
        t = 2048
        lon = lv.lon0 - lv.dlon + (np.arange(t) + 0.5) / t * 2 * lv.dlon
        lat = lv.lat0 + lv.dlat - (np.arange(t) + 0.5) / t * 2 * lv.dlat
        la, lo = np.meshgrid(lat, lon, indexing="ij")
        y = (90 - la) / 180 * big.shape[0] - 0.5
        x = (lo + 180) / 360 * big.shape[1] - 0.5
        reg = np.dstack([ndi.map_coordinates(big[..., k], np.stack([y, x]), order=1) for k in range(3)]).astype(np.uint8)
        Image.fromarray(reg).save(os.path.join(OUT, "A1color.jpg"), quality=92)
        if ktx:
            encode_ktx(reg, "A1color", True)

    manifest["ext"] = "ktx2" if ktx else "png"
    if not args:
        json.dump(manifest, open(os.path.join(OUT, "moon.json"), "w"), indent=1)
    log("done")


if __name__ == "__main__":
    main()
