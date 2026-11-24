"""Fetch the public-domain source data for Journey to the Moon.

Raw data is gigabytes and is NOT kept in the repository or in Dropbox: it goes
to %LOCALAPPDATA%/moon-data (override with MOON_DATA). Re-running resumes
partial files (curl -C -).

    python download.py            # everything
    python download.py ldem_16    # only files whose name contains the word
"""
import os, subprocess, sys

DATA = os.environ.get("MOON_DATA") or os.path.join(os.environ["LOCALAPPDATA"], "moon-data")

SVS = "https://svs.gsfc.nasa.gov/vis/a000000/a004700/a004720/"
LROC = "https://pds.lroc.im-ldi.com/data/LRO-L-LROC-5-RDR-V1.0/LROLRC_2001/DATA/SDP/NAC_DTM/"
SLDEM = "http://imbrium.mit.edu/DATA/SLDEM2015/TILES/FLOAT_IMG/"

# (url, expected bytes) -- sizes as served in November 2026
FILES = [
    # NASA SVS CGI Moon Kit (svs.gsfc.nasa.gov/4720): colour and LOLA elevation, whole Moon
    (SVS + "ldem_16.tif", 66378634),
    (SVS + "lroc_color_poles_8k.tif", 50641970),
    (SVS + "lroc_color_poles.tif", 518090428),
    (SVS + "ldem_64_uint.tif", 530934146),
    # LROC NAC digital terrain models of the Apollo 17 site (5 m and 2 m per pixel)
    (LROC + "APOLLO17/NAC_DTM_APOLLO17.LBL", None),
    (LROC + "APOLLO17/NAC_DTM_APOLLO17.TIF", 463397471),
    (LROC + "APOLLO17_2M/NAC_DTM_APOLLO17_2M.LBL", None),
    (LROC + "APOLLO17_2M/NAC_DTM_APOLLO17_2M.TIF", 952511711),
    # SLDEM2015 (LOLA + Kaguya), 512 px/degree (59 m), tile 0-30 N, 0-45 E: site and approach
    (SLDEM + "SLDEM2015_512_00N_30N_000_045_FLOAT.LBL", None),
    (SLDEM + "SLDEM2015_512_00N_30N_000_045_FLOAT.IMG", 1415577600),
]


def main():
    os.makedirs(DATA, exist_ok=True)
    want = sys.argv[1:]
    for url, size in FILES:
        name = url.rsplit("/", 1)[1]
        if want and not any(w in name for w in want):
            continue
        dst = os.path.join(DATA, name)
        have = os.path.getsize(dst) if os.path.exists(dst) else 0
        if size and have == size or (size is None and have > 0):
            print("ok      ", name, flush=True)
            continue
        print("fetching", name, flush=True)
        r = subprocess.run(["curl", "-L", "-sS", "--retry", "5", "-C", "-", "-o", dst, url])
        have = os.path.getsize(dst) if os.path.exists(dst) else 0
        if r.returncode or (size and have != size):
            print("FAILED  ", name, "got", have, "expected", size, flush=True)
        else:
            print("done    ", name, have, flush=True)


if __name__ == "__main__":
    main()
