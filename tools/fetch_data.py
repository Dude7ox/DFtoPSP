#!/usr/bin/env python3
"""Fetch original Daggerfall ARENA2 files from the cached GitHub mirror.

The sandbox can only reach github.com, so the freeware data used by this port is
pulled from `PetrasBartusis/Daggerfall` (branch `contents/arena2`). This helper
resolves each requested file name to a blob SHA through the git trees API and
downloads the raw bytes, then verifies the size against the tree entry so a
truncated download cannot slip into `data/MANIFEST.json`.

    python3 tools/fetch_data.py --out data/ARENA2 NAMES...
    python3 tools/fetch_data.py --out data/ARENA2 --list /tmp/want-names.txt

Requires the `gh` CLI (authenticated) or a network path to api.github.com.
"""
import argparse
import json
import os
import subprocess
import sys

REPO = os.environ.get("DF_DATA_REPO", "PetrasBartusis/Daggerfall")
SUBDIR = os.environ.get("DF_DATA_SUBDIR", "arena2")


def gh(args):
    """Run a gh api call and return stdout bytes."""
    out = subprocess.run(["gh", "api"] + args, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
    if out.returncode != 0:
        raise RuntimeError("gh api %s: %s" % (" ".join(args[:2]),
                                              out.stderr.decode()[:200]))
    return out.stdout


def tree():
    """path -> (sha, size) for every blob under the mirror's ARENA2 folder."""
    data = json.loads(gh(["repos/%s/git/trees/HEAD?recursive=1" % REPO]))
    prefix = SUBDIR + "/"
    res = {}
    for entry in data["tree"]:
        if entry["type"] != "blob" or not entry["path"].startswith(prefix):
            continue
        res[entry["path"][len(prefix):]] = (entry["sha"], entry.get("size", -1))
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="data/ARENA2", help="destination folder")
    ap.add_argument("--list", help="file with one name per line")
    ap.add_argument("--force", action="store_true", help="re-download existing files")
    ap.add_argument("names", nargs="*")
    args = ap.parse_args()

    names = list(args.names)
    if args.list:
        with open(args.list) as f:
            names += [ln.split()[0] for ln in f if ln.strip()]
    names = sorted(set(names))
    if not names:
        ap.error("no file names given")

    entries = tree()
    print("mirror lists %d files under %s" % (len(entries), SUBDIR))
    os.makedirs(args.out, exist_ok=True)
    fetched = skipped = missing = bad = 0
    for name in names:
        dest = os.path.join(args.out, name)
        if name not in entries:
            print("MISSING-IN-MIRROR %s" % name)
            missing += 1
            continue
        sha, size = entries[name]
        if os.path.exists(dest) and os.path.getsize(dest) == size and not args.force:
            skipped += 1
            continue
        blob = gh(["repos/%s/git/blobs/%s" % (REPO, sha),
                   "-H", "Accept: application/vnd.github.raw"])
        with open(dest, "wb") as f:
            f.write(blob)
        if len(blob) != size:
            print("SIZE-MISMATCH %s got %d want %d" % (name, len(blob), size))
            bad += 1
            os.remove(dest)
            continue
        fetched += 1
        print("ok %-22s %9d" % (name, size))
    print("fetched %d, skipped %d, missing %d, bad %d" % (fetched, skipped, missing, bad))
    return 1 if (missing or bad) else 0


if __name__ == "__main__":
    sys.exit(main())
