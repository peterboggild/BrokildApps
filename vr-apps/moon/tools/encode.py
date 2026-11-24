"""Encode the development maps in ../assets/moon (PNG, JPG) to KTX2 and switch moon.json to them.

    python encode.py            # everything, three encoders at a time
    python encode.py L5 L6      # only these

Run after build.py when the maps are final: the KTX2 files are what is published (the PNG and JPG files are
ignored by git).
"""
import json, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "assets", "moon")
CACHE = os.path.join(os.environ.get("MOON_DATA") or os.path.join(os.environ["LOCALAPPDATA"], "moon-data"), "cache")


def encode(fn):
    name, ext = os.path.splitext(fn)
    srgb = ext == ".jpg"
    im = Image.open(os.path.join(OUT, fn)).convert("RGBA")
    big = srgb and im.width * im.height > 20_000_000
    if big:
        # the WebAssembly encoder runs out of memory on the 8k colour map in ETC1S mode: publish it at 4k, UASTC
        im = im.resize((im.width // 2, im.height // 2), Image.LANCZOS)
    img = np.asarray(im)
    raw = os.path.join(CACHE, name + ".rgba")
    img.tofile(raw)
    args = ["node", os.path.join(HERE, "ktx2.mjs"), raw, str(img.shape[1]), str(img.shape[0]),
            os.path.join(OUT, name + ".ktx2"), "q=1", "rdo=0"] + ((["srgb"] if big else ["srgb", "etc1s"]) if srgb else [])
    r = subprocess.run(args, capture_output=True, text=True)
    os.remove(raw)
    if r.returncode:
        return f"FAILED {fn}: " + (r.stderr.strip() or r.stdout.strip())[-300:]
    return r.stdout.strip().splitlines()[-1]


def main():
    os.makedirs(CACHE, exist_ok=True)
    want = sys.argv[1:]
    files = sorted(f for f in os.listdir(OUT) if f.endswith((".png", ".jpg")) and (not want or os.path.splitext(f)[0] in want))
    with ThreadPoolExecutor(3) as ex:
        for line in ex.map(encode, files):
            print(line, flush=True)
    p = os.path.join(OUT, "moon.json")
    m = json.load(open(p))
    m["ext"] = "ktx2"
    json.dump(m, open(p, "w"), indent=1)
    total = sum(os.path.getsize(os.path.join(OUT, f)) for f in os.listdir(OUT) if not f.endswith((".png", ".jpg")))
    print(f"published size: {total / 1048576:.1f} MB")


if __name__ == "__main__":
    main()
