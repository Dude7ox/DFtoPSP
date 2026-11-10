#!/usr/bin/env python3
"""Build & install pspsdk without autotools (the sandbox has none).

Expands every src/*/Makefile.am: compiles C and assembler sources (including
the -DF_<stub> object groups used by the import stub libraries), archives the
lib_LIBRARIES, copies the installed headers and builds the host tools
(pack-pbp, psp-fixup-imports, psp-prxgen, psp-config, ...) into $PSPDEV/bin.
Finally mirrors the lib symlinks that gcc expects next to libpspsdk.a.
"""
import os, re, shutil, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SDK = Path(os.environ.get("PSPSDK_SRC", "/home/user/.cache/dftopsp/pspsdk"))
PSPDEV = Path(os.environ["PSPDEV"])
SDKLIB = PSPDEV / "psp" / "sdk" / "lib"
SDKINC = PSPDEV / "psp" / "sdk" / "include"
INC = PSPDEV / "psp" / "include"
BIN = PSPDEV / "bin"
for d in (SDKLIB, SDKINC, INC, BIN):
    d.mkdir(parents=True, exist_ok=True)

CC = "psp-gcc"
CFLAGS = ["-O2", "-mno-gpopt", "-Wall", "-D_PSP_FW_VERSION=600", "-g0"]
JOBS = "-j2"

def sh(cmd, cwd=None):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if r.returncode != 0:
        print("CMD FAIL:", " ".join(map(str, cmd)))
        print(r.stdout[-3000:]); print(r.stderr[-3000:]); sys.exit(1)
    return r

def parse_am(text):
    """Flatten a Makefile.am into a dict of var -> expanded string."""
    lines = []
    cont = ""
    for ln in text.splitlines():
        ln = ln.rstrip()
        if ln.endswith("\\"):
            cont += ln[:-1] + " "
            continue
        lines.append(cont + ln)
        cont = ""
    if cont: lines.append(cont)
    vars_ = {}
    for ln in lines:
        m = re.match(r"^([A-Za-z0-9_]+)\s*=\s*(.*)$", ln)
        if m:
            vars_[m.group(1)] = m.group(2)
    vars_.setdefault("top_srcdir", str(SDK))
    vars_.setdefault("top_builddir", str(SDK))
    # one level of $(VAR) expansion
    def expand(s, depth=0):
        if depth > 4: return s
        return expand(re.sub(r"\$\(([A-Za-z0-9_]+)\)", lambda m: vars_.get(m.group(1), ""), s), depth + 1)
    return {k: expand(v) for k, v in vars_.items()}, lines

def cppflags_of(vars_, top):
    cp = vars_.get("CPPFLAGS", "")
    cp = cp.replace("$(top_srcdir)", str(top)).replace("@PSPSDK_CFLAGS@", "")
    return cp.split()

built_libs = []
srcmods = sorted([p for p in (SDK / "src").iterdir() if (p / "Makefile.am").exists()])
for mod in srcmods:
    vars_, lines = parse_am((mod / "Makefile.am").read_text())
    cp = cppflags_of(vars_, SDK) + [f"-I{mod}"]
    objs = []   # (source, extra flags, out name or None)
    libs = vars_.get("lib_LIBRARIES", "").split()
    for lib in libs:
        stem = lib[3:-2]
        for src in vars_.get(f"lib{stem}_a_SOURCES", "").split():
            objs.append((src, [], None))
    # grouped stub objects: $(VAR): source.S compiled once per member with -DF_<member>
    for ln in lines:
        m = re.match(r"^\$\(([A-Za-z0-9_]+)\):\s+(\S+\.(?:S|s|c))\s*$", ln)
        if not m: continue
        for obj in vars_.get(m.group(1), "").split():
            objs.append((m.group(2), ["-DF_" + obj[:-2]], obj))
    compiled = []
    for item in objs:
        src, extra = item[0], item[1]
        outname = item[2] if len(item) > 2 else None
        if not (mod / src).exists():
            continue
        if outname is None:
            outname = re.sub(r"\.(c|S|s)$", ".o", src)
        objpath = mod / outname
        if src.endswith(".S") or src.endswith(".s"):
            sh([CC, "-c"] + CFLAGS + cp + extra + [str(mod / src), "-o", str(objpath)])
        else:
            sh([CC, "-c"] + CFLAGS + cp + extra + [str(mod / src), "-o", str(objpath)])
        compiled.append(objpath)
    for lib in libs:
        stem = lib[3:-2]
        want = []
        for src in vars_.get(f"lib{stem}_a_SOURCES", "").split():
            want.append(mod / re.sub(r"\.(c|S|s)$", ".o", src))
        for extra in vars_.get(f"lib{stem}_a_LIBADD", "").split():
            want.append(mod / extra)
        want = [w for w in want if w.exists()]
        if want:
            sh(["psp-ar", "rcs", str(mod / lib)] + [str(w) for w in want])
            shutil.copy(mod / lib, SDKLIB / lib)
            built_libs.append(lib)
    # headers
    for v, val in vars_.items():
        if v.endswith("dir") and "@PSPSDK_INCLUDEDIR@" in val:
            prefix = v[:-3]
            for h in vars_.get(prefix + "_HEADERS", "").split():
                if (mod / h).exists():
                    (INC / h).parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy(mod / h, INC / h)

# startup: crt0 objects install next to the toolchain libs (gcc links crt0.o)
st = SDK / "src" / "startup"
if (st / "crt0.c").exists():
    cp = ["-I%s" % (SDK / "src" / d) for d in ("base", "kernel", "user", "debug")]
    for src, dst in (("crt0.c", PSPDEV / "psp" / "lib" / "crt0.o"),
                     ("crt0_prx.c", PSPDEV / "psp" / "lib" / "crt0_prx.o"),
                     ("prxexports.c", SDKLIB / "prxexports.o")):
        sh([CC, "-c"] + CFLAGS + cp + [str(st / src), "-o", str(dst)])
        print("startup:", dst.name)

# host tools (they expect a generated config.h)
cfg = SDK / "tools" / "config.h"
if not cfg.exists():
    cfg.write_text('#pragma once\n#define HAVE_STDINT_H 1\n#define HAVE_INTTYPES_H 1\n'
                   '#define HAVE_MALLOC_H 1\n#define PSPSDK_TOPDIR "psp/sdk"\n')
tvars, tlines = parse_am((SDK / "tools" / "Makefile.am").read_text())
for prog in tvars.get("bin_PROGRAMS", "").split():
    key = prog.replace("-", "_") + "_SOURCES"
    srcs = tvars.get(key, "").split()
    if not srcs: continue
    out = BIN / prog.replace("_", "-")
    sh(["gcc", "-O2", "-I.", "-I", str(SDK / "tools"),
        "-DPSPSDK_TOPDIR=\"psp/sdk\"", "-DHAVE_UNISTD_H", "-DHAVE_STDINT_H", "-DHAVE_STRING_H",
        "-DHAVE_STDLIB_H", "-DHAVE_STDIO_H"] +
       [str(SDK / "tools" / s) for s in srcs] + ["-o", str(out)])
    print("tool:", out.name)
lic = SDK / "tools" / "psp-create-license-directory"
if lic.exists():
    shutil.copy(lic, BIN / "psp-create-license-directory")
    os.chmod(BIN / "psp-create-license-directory", 0o755)

# symlinks gcc's driver expects next to the standard libs
for lib in ["libcglue", "libpthreadglue", "libpspprof", "libpsputility", "libpsprtc",
            "libpspnet_inet", "libpspnet_resolver", "libpspsdk", "libpspmodinfo",
            "libpspuser", "libpspkernel"]:
    dst = PSPDEV / "psp" / "lib" / (lib + ".a")
    srcp = SDKLIB / (lib + ".a")
    if srcp.exists():
        dst.parent.mkdir(parents=True, exist_ok=True)
        if dst.is_symlink() or dst.exists(): dst.unlink()
        dst.symlink_to(os.path.relpath(srcp, dst.parent))
print("libs:", len(built_libs))
print("PSPSDK_BUILD_COMPLETE")
