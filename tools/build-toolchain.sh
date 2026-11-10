#!/bin/bash
# Resumable from-source PSP toolchain build (binutils/gcc/newlib/pthread) for the
# DFtoPSP port. Stages drop marks in $CACHE/marks so a re-run skips finished work.
# All sources come from GitHub mirrors because the sandbox only reaches github.com.
set -u
CACHE=/home/user/.cache/dftopsp
SRC=$CACHE/toolchain-src
MARKS=$CACHE/marks
PRE=/home/user/.local/pspdev/prereq
PSPDEV=/home/user/.local/pspdev/pspdev
LOG=$CACHE/toolchain.log
mkdir -p "$SRC" "$MARKS" "$PRE" "$PSPDEV"
export PSPDEV
export PATH="$PRE/bin:$PSPDEV/bin:$PATH"
J=-j2

# Git checkouts write files sequentially, so configure.ac can end up newer than
# the generated aclocal.m4 and make tries to run the missing autotools. Force
# generated files to look newest instead.
fixts(){ for f in aclocal.m4 configure Makefile.in config.h.in; do [ -e "$f" ] && touch "$f"; done; return 0; }
log(){ echo "[$(date +%H:%M:%S)] $*" | tee -a "$LOG"; }
run(){ log "RUN $*"; "$@" >>"$LOG" 2>&1 || { log "FAIL $*"; exit 1; }; }
stage(){ STAGE=$1; if [ -f "$MARKS/$1" ]; then log "skip $1 (mark)"; return 1; fi; log "===== STAGE $1 ====="; return 0; }
done_(){ log "===== OK $STAGE ====="; touch "$MARKS/$STAGE"; }

if stage clone; then
  clone(){ # clone <dir> <url> [ref]
    if [ -d "$SRC/$1/.git" ]; then return 0; fi
    rm -rf "$SRC/$1"
    if [ -n "${3:-}" ]; then run git clone --depth 1 -b "$3" "$2" "$SRC/$1"; else run git clone --depth 1 "$2" "$SRC/$1"; fi
  }
  clone gmp     https://github.com/alisw/GMP.git v6.2.1
  clone mpfr    https://github.com/dym21/mpfr-loongarch-4.2.1.git
  clone mpc     https://github.com/val-verde/mpc.git
  clone byacc   https://github.com/qzmfranklin/byacc.git
  clone reflex  https://github.com/ThomasDickey/reflex-snapshots.git t20260131
  clone binutils https://github.com/pspdev/binutils-gdb.git allegrex-v2.44
  clone gcc     https://github.com/pspdev/gcc.git allegrex-v15.2.0
  clone newlib  https://github.com/pspdev/newlib.git allegrex-v4.5.0
  clone pthread https://github.com/pspdev/pthread-embedded.git platform_agnostic
  done_
fi

if stage prereq; then
  cd "$SRC/gmp"   && fixts && run env M4=/bin/true ./configure --prefix="$PRE" --quiet --enable-cxx=no --disable-assembly \
    && run make $J MAKEINFO=/bin/true && run make MAKEINFO=/bin/true install
  cd "$SRC/mpfr"  && fixts && run env M4=/bin/true ./configure --prefix="$PRE" --quiet --enable-cxx=no --disable-assembly --with-gmp="$PRE" \
    && run make $J MAKEINFO=/bin/true && run make MAKEINFO=/bin/true install
  cd "$SRC/mpc"   && fixts && run env M4=/bin/true ./configure --prefix="$PRE" --quiet --enable-cxx=no --disable-assembly --with-gmp="$PRE" --with-mpfr="$PRE" \
    && run make $J MAKEINFO=/bin/true && run make MAKEINFO=/bin/true install
  done_
fi

if stage byacc; then
  cd "$SRC/byacc" && fixts && run ./configure --prefix="$PRE" --quiet && run make $J && run make install
  cd "$SRC/reflex" && fixts && run ./configure --prefix="$PRE" --quiet && run make $J && run make install
  # binutils links -lfl but never calls flex code paths we need: empty archive.
  run ar rcs "$PRE/lib/libfl.a"
  for n in lex flex; do [ -e "$PRE/bin/$n" ] || ln -s "$PRE/bin/reflex" "$PRE/bin/$n"; done
  ls "$PRE/bin/yacc"
  done_
fi

if stage binutils; then
  cd "$SRC/binutils" && fixts && rm -rf build-psp && mkdir -p build-psp && cd build-psp
  run ../configure --quiet --target=psp --prefix="$PSPDEV" --with-sysroot="$PSPDEV/psp" \
    --disable-nls --disable-werror --disable-gdb --disable-gdbserver --disable-sim --disable-readline \
    --with-gmp="$PRE" --with-mpfr="$PRE" --with-mpc="$PRE"
  run make $J MAKEINFO=/bin/true
  run make MAKEINFO=/bin/true install-strip
  done_
fi

if stage gcc1; then
  cd "$SRC/gcc" && fixts && rm -rf build-psp-stage1 && mkdir -p build-psp-stage1 && cd build-psp-stage1
  run ../configure --quiet --target=psp --prefix="$PSPDEV" --with-sysroot="$PSPDEV/psp" \
    --with-native-system-header-dir=/include --enable-languages=c --with-newlib --disable-libssp \
    --disable-multilib --enable-threads=no --disable-tls --disable-nls \
    --with-gmp="$PRE" --with-mpfr="$PRE" --with-mpc="$PRE"
  run make $J MAKEINFO=/bin/true all-gcc
  run make MAKEINFO=/bin/true install-gcc
  done_
fi

if stage newlib; then
  cd "$SRC/newlib" && fixts && rm -rf build-psp && mkdir -p build-psp && cd build-psp
  run ../configure --quiet --prefix="$PSPDEV" --target=psp --with-sysroot="$PSPDEV/psp" \
    --enable-newlib-retargetable-locking --enable-newlib-multithread --enable-newlib-io-c99-formats \
    --enable-newlib-iconv --enable-newlib-iconv-encodings=us_ascii,utf8,utf16,utf_16be,utf_16le,ucs_2,ucs_2be,ucs_2le,ucs_2_internal,ucs_4_internal,iso_8859_1 \
    --disable-multilib
  run make $J MAKEINFO=/bin/true all
  run make MAKEINFO=/bin/true install-strip
  done_
fi

if stage pthread; then
  # newlib gates POSIX thread visibility per target; the PSP needs it on so
  # gcc stage2 libgcc/libstdc++ unwind support finds <pthread.h> users.
  FH="$PSPDEV/psp/include/sys/features.h"
  if [ -f "$FH" ] && ! grep -q "DFtoPSP posix threads" "$FH"; then
    printf '\n/* DFtoPSP posix threads */\n#if defined(__PSP__) && !defined(_POSIX_THREADS)\n#define _POSIX_THREADS 1\n#endif\n' >>"$FH"
  fi
  # pthread-embedded ships CMake only; compile the library directly.
  cd "$SRC/pthread"
  rm -rf build-ptw && mkdir -p build-ptw
  objs=""
  for f in *.c platform/psp/psp_osal.c platform/helper/*.c; do
    [ -e "$f" ] || continue
    o="build-ptw/$(echo "$f" | tr / _ | sed 's/\.c$/.o/')"
    run psp-gcc -c -O3 -Wall -fno-strict-aliasing -I. -Iplatform/psp -Iplatform/helper "$f" -o "$o"
    objs="$objs $o"
  done
  run psp-ar rcs "$PSPDEV/psp/lib/libpthread.a" $objs
  for h in pthread.h pthread_public.h sched.h semaphore.h; do [ -e "$h" ] && cp "$h" "$PSPDEV/psp/include/$h"; done
  mkdir -p "$PSPDEV/psp/include/sys" "$PSPDEV/psp/include/bits"
  for h in sys/_pthreadtypes.h sys/sched.h; do [ -e "$h" ] && cp "$h" "$PSPDEV/psp/include/$h"; done
  [ -e bits/posix_opt.h ] && cp bits/posix_opt.h "$PSPDEV/psp/include/bits/"
  [ -e platform/psp/pte_types.h ] && cp platform/psp/pte_types.h "$PSPDEV/psp/include/"
  mkdir -p "$PSPDEV/psp/include/sys"
  [ -e pte_generic_osal.h ] && cp pte_generic_osal.h "$PSPDEV/psp/include/sys/"
  [ -e platform/helper/tls-helper.h ] && cp platform/helper/tls-helper.h "$PSPDEV/psp/include/sys/"
  ls -l "$PSPDEV/psp/include/pthread.h" "$PSPDEV/psp/lib/libpthread.a"
  done_
fi

if stage gcc2; then
  cd "$SRC/gcc" && fixts && rm -rf build-psp-stage2 && mkdir -p build-psp-stage2 && cd build-psp-stage2
  run ../configure --quiet --target=psp --prefix="$PSPDEV" --with-sysroot="$PSPDEV/psp" \
    --with-native-system-header-dir=/include --enable-languages=c,c++ --with-float=hard --with-newlib \
    --disable-libssp --disable-multilib --enable-threads=posix --disable-tls --disable-nls \
    --with-gmp="$PRE" --with-mpfr="$PRE" --with-mpc="$PRE"
  run make $J MAKEINFO=/bin/true all
  run make MAKEINFO=/bin/true install
  done_
fi

log TOOLCHAIN_STAGES_COMPLETE
"$PSPDEV/bin/psp-g++" --version | head -1
