#include "common.h"
#include "sys/iop_heap.h"

/*
 * I/O-processor heap wrappers, 0x11EC10..0x11ECB8. Each one only forwards to Sony's libkernl iopheap call
 * (SIF RPC server 0x80000003). Used by the sound module (bank header / sequence memory) and by boot.
 */

/* Binds the IOP heap RPC service. */
void Sys_InitIopHeap(void) {
    sceSifInitIopHeap();
}

/* Allocates `size` bytes of IOP memory; returns the IOP address, or NULL. */
void *IopHeap_Alloc(s32 size) {
    return sceSifAllocIopHeap(size);
}

/* Frees IOP memory returned by IopHeap_Alloc. */
s32 IopHeap_Free(void *addr) {
    return sceSifFreeIopHeap(addr);
}

/* Total free IOP memory in bytes, -1 when the RPC fails. */
s32 IopHeap_GetTotalFree(void) {
    return sceSifQueryTotalFreeMemSize();
}

/* Largest free IOP block in bytes, -1 when the RPC fails. */
s32 IopHeap_GetMaxFree(void) {
    return sceSifQueryMaxFreeMemSize();
}

/*
 * Queries both free sizes and drops the results (what used them, presumably a debug print, is compiled out).
 * Not matched: the original saves and restores $s0 without using it and does not tail-call; this version saves
 * only $ra and tail-calls IopHeap_GetMaxFree. The two calls and their order are the same.
 */
#if 0
void IopHeap_PrintFree(void) {
    s32 total = IopHeap_GetTotalFree();
    s32 max = IopHeap_GetMaxFree();
}
#endif
INCLUDE_ASM("asm/nonmatchings/sys/iop_heap", IopHeap_PrintFree);
