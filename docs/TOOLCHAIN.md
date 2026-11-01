# PSP toolchain notes

The EBOOT in this workspace was cross-compiled with a locally built PSPDEV toolchain:

- binutils 2.44 (allegrex)
- GCC 15.2.0 (allegrex, hard float, newlib)
- newlib 4.5.0
- PSPSDK master, built manually from source

Host build helpers used during this session are cached outside the repository under
`/home/user/.cache/dftopsp/` and are not required to run the prebuilt EBOOT.

To rebuild on a normal PSPDEV install:

```sh
export PSPDEV=/path/to/pspdev
export PATH="$PSPDEV/bin:$PATH"
make -f Makefile.psp
```

Local quirks that were needed in this sandbox:
- byacc and reflex were built as host yacc/flex for binutils.
- newlib `sys/features.h` needed `_POSIX_THREADS` exposed for `__PSP__` so GCC
  stage2 could build libgcc/libstdc++ unwind support.
- PSPSDK was compiled without autotools by expanding the per-function `F_*` stub
  objects manually; `libpspgum` used the FPU objects, not the VFPU duplicates.
- A single-threaded `pthread_stubs.c` satisfies libgcc/libstdc++ link dependencies.
