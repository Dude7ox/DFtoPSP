#include <pspkernel.h>
#include <unistd.h>
void _exit(int status) { (void)status; sceKernelExitGame(); for(;;) sceKernelDelayThread(1000000); }
