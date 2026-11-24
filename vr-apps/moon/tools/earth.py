"""Earth textures for the launch chapter, from NASA's Blue Marble Next Generation (December, no clouds).

    python earth.py

Source files (download to %LOCALAPPDATA%/moon-data/earth, from
https://assets.science.nasa.gov/content/dam/science/esd/eo/images/bmng/bmng-base/december/):
    world.200412.3x21600x10800.jpg      the whole Earth, 2 km per pixel
    world.200412.3x21600x21600.B1.jpg   0-90 N, 90 W-0, 500 m per pixel (for the region the ascent flies over)

Writes ../assets/earth/:
    earth.jpg     whole Earth, 4096 x 2048
    region.jpg    23-41 N, 84-48 W (Florida and the Atlantic down the launch track), 4096 x 2048, about 1 km per pixel
    clouds.png    a generated cloud layer (no NASA cloud composite is to be had any more): 4096 x 2048 grey,
                  band-limited noise shaped by where the Earth is cloudy (the equatorial band, the clear subtropics,
                  the mid-latitude storm tracks), with clearer skies over Florida on the launch day.
"""
import json, os
import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None
HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(os.environ.get("MOON_DATA") or os.path.join(os.environ["LOCALAPPDATA"], "moon-data"), "earth")
OUT = os.path.join(HERE, "..", "assets", "earth")
REGION = {"lat0": 23.0, "lat1": 41.0, "lon0": -84.0, "lon1": -48.0}


def main():
    os.makedirs(OUT, exist_ok=True)
    # whole Earth
    im = Image.open(os.path.join(SRC, "world.200412.3x21600x10800.jpg"))
    im.draft("RGB", (10800, 5400))
    im.convert("RGB").resize((4096, 2048), Image.LANCZOS).save(os.path.join(OUT, "earth.jpg"), quality=88)
    del im
    print("earth.jpg")
    # the region the ascent flies over, from the 500 m tile (decoded at half size to save memory: 1 km)
    im = Image.open(os.path.join(SRC, "world.200412.3x21600x21600.B1.jpg"))
    im.draft("RGB", (10800, 10800))
    im = im.convert("RGB")
    ppd = im.width / 90.0                                  # pixels per degree; the tile spans 90 W-0, 90 N-0
    box = (int((REGION["lon0"] + 90) * ppd), int((90 - REGION["lat1"]) * ppd),
           int((REGION["lon1"] + 90) * ppd), int((90 - REGION["lat0"]) * ppd))
    im.crop(box).resize((4096, 2048), Image.LANCZOS).save(os.path.join(OUT, "region.jpg"), quality=90)
    del im
    print("region.jpg", box)
    clouds()
    json.dump({"region": REGION, "source": "NASA Earth Observatory, Blue Marble Next Generation (December 2004)"},
              open(os.path.join(OUT, "earth.json"), "w"), indent=1)


def clouds():
    W, H = 4096, 2048
    rng = np.random.default_rng(1972)

    def band_noise(slope, kmin, kmax, stretch=1.0):
        fy = np.fft.fftfreq(H)[:, None] * H
        fx = np.fft.rfftfreq(W)[None, :] * W / 2 / stretch
        k = np.hypot(fx, fy)
        amp = np.where((k > kmin) & (k < kmax), np.maximum(k, 1e-6) ** slope, 0)
        z = np.fft.irfft2(amp * np.exp(2j * np.pi * rng.random(amp.shape)), s=(H, W)).astype(np.float32)
        return (z - z.mean()) / z.std()

    lat = np.linspace(90, -90, H, dtype=np.float32)[:, None]
    lon = np.linspace(-180, 180, W, endpoint=False, dtype=np.float32)[None, :]
    # where the Earth tends to be cloudy (zonal mean cloud fraction, roughly)
    clim = (0.42 + 0.30 * np.exp(-((lat - 6) / 7) ** 2)          # the intertropical convergence zone
            - 0.28 * np.exp(-((np.abs(lat) - 22) / 8) ** 2)      # the subtropical highs
            + 0.30 * np.exp(-((np.abs(lat) - 55) / 12) ** 2))    # the storm tracks
    # weather systems: smooth large-scale fields, swirled twice by a slowly varying flow (domain warping), so
    # cloud bands curl round lows instead of lying about as noise; fine texture only roughens the edges
    from scipy import ndimage as ndi
    big = band_noise(-2.4, 2, 28, stretch=1.7)
    mid = band_noise(-2.0, 16, 160, stretch=1.4)
    fine = band_noise(-1.2, 160, 1200)
    field = 0.75 * big + 0.35 * mid
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    for k, amp in ((0, 36.0), (1, 14.0)):
        wx, wy = band_noise(-3.0, 2, 10, stretch=1.5), band_noise(-3.0, 2, 10)
        field = ndi.map_coordinates(field, [np.clip(yy + amp * 0.6 * wy, 0, H - 1), (xx + amp * wx) % W], order=1, mode="grid-wrap")
    field = clim + 0.20 * field
    # a fair day at the Cape: thinner cover over Florida and just off its coast
    d2 = ((lat - 28.6) / 6) ** 2 + ((lon + 79) / 9) ** 2
    field -= 0.22 * np.exp(-d2)
    # density rather than a mask: thin at the edges and in places inside, thick in the cores of the systems
    cells = band_noise(-1.5, 60, 500, stretch=1.2)               # cumulus-scale lumps
    d = np.clip((field - 0.42) / 0.30, 0, 1)
    d = d ** 1.3 * (0.72 + 0.28 * np.clip(cells * 0.5 + 0.5, 0, 1)) * (0.9 + 0.1 * np.clip(fine * 0.5 + 0.5, 0, 1))
    c = np.clip(d * 1.25, 0, 1)
    Image.fromarray((c * 255).astype(np.uint8), "L").save(os.path.join(OUT, "clouds.png"), optimize=True)
    print("clouds.png  cover %.0f %%" % (100 * c.mean()))


if __name__ == "__main__":
    main()
