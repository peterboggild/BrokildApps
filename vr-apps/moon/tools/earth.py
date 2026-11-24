"""Earth textures for the launch chapter, from NASA's Blue Marble Next Generation (December, no clouds).

    python earth.py

Source files (download to %LOCALAPPDATA%/moon-data/earth, from
https://assets.science.nasa.gov/content/dam/science/esd/eo/images/bmng/bmng-base/december/):
    world.200412.3x21600x10800.jpg      the whole Earth, 2 km per pixel
    world.200412.3x21600x21600.B1.jpg   0-90 N, 90 W-0, 500 m per pixel (for the region the ascent flies over)

Writes ../assets/earth/:
    earth.jpg     whole Earth, 4096 x 2048
    region.jpg    23-41 N, 84-48 W (Florida and the Atlantic down the launch track), 4096 x 2048, about 1 km per pixel
    clouds.jpg, clouds-region.jpg   the real clouds of one December day (see clouds()), whole Earth and region
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
    json.dump({"region": REGION, "cloudDay": CLOUD_DAY, "source": "NASA Earth Observatory, Blue Marble Next Generation (December 2004); clouds: NASA GIBS, VIIRS Suomi NPP true colour"},
              open(os.path.join(OUT, "earth.json"), "w"), indent=1)


CLOUD_DAY = "2019-12-06"


def cloud_cover(real, clear):
    """Cloud cover 0..1 from a real satellite picture of the day and the cloud-free Blue Marble of the same
    place: clouds are what is whiter than the clear ground. Snow, ice and bright desert are bright in both."""
    real = np.asarray(real, np.float32)
    clear = np.asarray(clear, np.float32)
    v = real.min(2)                                   # whiteness: the darkest channel
    sat = real.max(2) - v
    vc = np.maximum(clear.min(2), 14.0)
    a = (v - vc - 26.0) / 205.0                        # thin cloud stays thin: cover follows how white it is
    a *= np.clip(1.0 - (sat - 28.0) / 60.0, 0.0, 1.0)  # coloured things (sunglint on shallows, dust) are not cloud
    return np.clip(a, 0, 1) ** 1.15


def clouds():
    """Real clouds: NASA's VIIRS true-colour mosaic of one December day (Suomi NPP, from NASA GIBS), against the
    cloud-free Blue Marble. Download with (BBOX quadrants 0,-180,90,0 / 0,0,90,180 / -90,-180,0,0 / -90,0,0,180
    at 4096x2048 as viirs_<day>.q0..q3.jpg, and 23,-84,41,-48 as viirs_<day>.region.jpg):
    https://gibs.earthdata.nasa.gov/wms/epsg4326/best/wms.cgi?SERVICE=WMS&REQUEST=GetMap&VERSION=1.3.0
        &LAYERS=VIIRS_SNPP_CorrectedReflectance_TrueColor&FORMAT=image/jpeg&CRS=EPSG:4326&TIME=<day>&BBOX=..."""
    q = [Image.open(os.path.join(SRC, f"viirs_{CLOUD_DAY}.q{k}.jpg")).convert("RGB") for k in range(4)]
    world = Image.new("RGB", (8192, 4096))
    for k, im in enumerate(q):
        world.paste(im, ((k % 2) * 4096, (k // 2) * 2048))
    world = world.resize((4096, 2048), Image.LANCZOS)
    clear = Image.open(os.path.join(OUT, "earth.jpg")).convert("RGB")
    c = cloud_cover(world, clear)
    # the polar night has no picture: no cloud information there (it is dark there anyway)
    dark = np.asarray(world, np.float32).max(2) < 6
    c[dark] = 0
    Image.fromarray((c * 255).astype(np.uint8), "L").save(os.path.join(OUT, "clouds.jpg"), quality=90)
    print("clouds.jpg  cover %.0f %%" % (100 * c.mean()))
    reg = Image.open(os.path.join(SRC, f"viirs_{CLOUD_DAY}.region.jpg")).convert("RGB")
    cr = cloud_cover(reg, Image.open(os.path.join(OUT, "region.jpg")).convert("RGB"))
    Image.fromarray((cr * 255).astype(np.uint8), "L").save(os.path.join(OUT, "clouds-region.jpg"), quality=92)
    print("clouds-region.jpg  cover %.0f %%" % (100 * cr.mean()))
    old = os.path.join(OUT, "clouds.png")
    if os.path.exists(old):
        os.remove(old)


if __name__ == "__main__":
    main()
