# PSP toolchain notes

The EBOOT in this repository was cross-compiled with a locally built PSPDEV toolchain:

- binutils 2.44 (allegrex)
- GCC 15.2.0 (allegrex, hard float, newlib, POSIX threads enabled)
- newlib 4.5.0
- PSPSDK master, built manually from source
- pthread-embedded (`platform/psp`) plus PSPSDK `libpthreadglue`

Host build helpers used during this session are cached outside the repository under
`/home/user/.cache/dftopsp/` and are not required to run the prebuilt EBOOT.

To rebuild on a normal PSPDEV install:

```sh
export PSPDEV=/path/to/pspdev
export PATH="$PSPDEV/bin:$PATH"
make -f Makefile.psp
```

The Makefile expects these PSPSDK libraries to be present:
`pspgu`, `pspgum`, `pspdisplay`, `pspctrl`, `pspge`, `pspuser`, `pspsdk`, `pspdebug`,
`cglue`, `pthreadglue`, `pthread`, plus the usual newlib/libstdc++ runtime.

## Local quirks needed in this sandbox

- byacc and reflex were built as host yacc/flex for binutils.
- newlib `sys/features.h` needed `_POSIX_THREADS` exposed for `__PSP__` so GCC
  stage2 could build libgcc/libstdc++ unwind support.
- pthread-embedded needed small header guards for PSP/newlib macro collisions;
  its public headers (`pthread_public.h`, `pte_types.h`, `bits/posix_opt.h`,
  `sys/_pthreadtypes.h`, `sys/pte_generic_osal.h`) were installed into the PSP sysroot.
- PSPSDK was compiled without autotools by expanding the per-function `F_*` stub
  objects manually. `libpspgum` used the FPU objects, not the VFPU duplicates.
- `libpthreadglue` and `libcglue` were rebuilt with pthread support enabled.
- Network stub libraries (`pspnet_inet`, `pspnet_resolver`, etc.) were built because
  libstdc++/newlib reference them on PSP even though the engine does not use networking.
- `src/platform/psp/syscalls.c` provides `_exit` in terms of `sceKernelExitGame`.
- `psp-fixup-imports` currently emits “stubs out of order” warnings with this manually
  bootstrapped SDK/link order. The EBOOT is still produced; real-PSP validation is pending.
