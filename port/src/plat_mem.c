/*
 * Memory of the PC build.
 *
 * The game was written for a machine with 32 MB at fixed addresses and it stores pointers in 32-bit fields, so the
 * 32-bit PC build gives it memory at PS2 addresses:
 *   0x003BE730..0x02000000  the game heap. The game takes it with ONE malloc at start-up (Heap_Create,
 *                           0x1EFB000 minus the end of the menu overlay); Port_Malloc returns the address the
 *                           PS2's malloc returns (0x3BE730, read from a save state), so heap pointers have the
 *                           same values as on the console and memory can be compared with an emulator dump.
 *   0x10000000, 0x12000000  hardware registers (timers, DMA, GS): plain memory here, so reads give what was last
 *                           written (0 at start: "transfer finished").
 *   0x70000000              the 16 KB scratchpad.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define HEAP_BASE 0x003BE000u  /* page that holds the first address */
#define HEAP_FIRST 0x003BE730u /* what the PS2's malloc returns for the game heap (gHeapStart in a PCSX2 save state) */
#define HEAP_END 0x02000000u

static uint32_t sHeapNext = HEAP_FIRST;

static void map(uint32_t addr, uint32_t size, const char *what) {
    void *p = mmap((void *)(uintptr_t)addr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                   -1, 0);

    if (p != (void *)(uintptr_t)addr) {
        fprintf(stderr, "bt3: cannot map %s at 0x%08X\n", what, addr);
        exit(2);
    }
}

#ifdef __x86_64__
/* The 64-bit build: the game's pointers are still 4 bytes wide (port/tools/ptr32.py), so everything the game can
   point at has to lie below 4 GB.
     - the program itself: linked at 0x20000000 (port/tools/link.py), clear of the regions above;
     - the C library's heap: kept in the break area that follows the program (no mmap, one arena), because
       handles the port gives to the game (files, sound players) come from it;
     - the stack of the thread that runs the game: the game stores addresses of local variables, so its main()
       runs on a thread whose stack is mapped at 0x60000000 (the process's own stack is far above 4 GB). */
#include <malloc.h>
#include <pthread.h>
#define STACK_BASE 0x60000000u
#define STACK_SIZE 0x01000000u
extern int __real_main(int argc, char **argv);
static int sArgc, sResult;
static char **sArgv;

static void *game_thread(void *arg) {
    (void)arg;
    sResult = __real_main(sArgc, sArgv);
    return NULL;
}

int __wrap_main(int argc, char **argv) {
    pthread_attr_t attr;
    pthread_t th;

    sArgc = argc;
    sArgv = argv;
    pthread_attr_init(&attr);
    pthread_attr_setstack(&attr, (void *)(uintptr_t)STACK_BASE, STACK_SIZE);
    if (pthread_create(&th, &attr, game_thread, NULL) != 0) {
        fprintf(stderr, "bt3: cannot start the game thread\n");
        return 2;
    }
    pthread_join(th, NULL);
    return sResult;
}
#endif

static void map(uint32_t addr, uint32_t size, const char *what);
__attribute__((constructor)) static void Port_MapMemory(void) {
#ifdef __x86_64__
    mallopt(M_MMAP_MAX, 0);
    mallopt(M_ARENA_MAX, 1);
    map(STACK_BASE, STACK_SIZE, "the game thread's stack");
#endif
    map(HEAP_BASE, HEAP_END - HEAP_BASE, "the game heap");
    map(0x10000000u, 0x10000, "the hardware registers");
    map(0x12000000u, 0x2000, "the GS registers");
    map(0x70000000u, 0x4000, "the scratchpad");
}

/* The game's malloc (include/port_compat.h renames it): a bump allocator over the heap region, 16-byte aligned. */
void *Port_Malloc(uint32_t size) {
    uint32_t p = (sHeapNext + 15) & ~15u;

    if (size > HEAP_END - p) {
        fprintf(stderr, "bt3: malloc(0x%X) does not fit in the game heap\n", size);
        exit(2);
    }
    sHeapNext = p + size;
    return (void *)(uintptr_t)p;
}

/* Memory the port hands to the game by address (file handles, the "second processor's" memory): zeroed, and below
   4 GB in the 64-bit build, where the game keeps such an address in 4 bytes. The C library's malloc is not safe
   for this there: it moves to mappings far above 4 GB whenever the break area cannot grow. */
void *Port_LowAlloc(size_t size) {
#ifdef __x86_64__
    size_t total = (size + 16 + 4095) & ~(size_t)4095;
    uint64_t *p = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);

    if (p == MAP_FAILED) {
        fprintf(stderr, "bt3: no memory below 4 GB for %zu bytes\n", size);
        exit(2);
    }
    p[0] = total;
    return p + 2;
#else
    return calloc(1, size);
#endif
}

void Port_LowFree(void *addr) {
#ifdef __x86_64__
    if (addr != NULL) {
        uint64_t *p = (uint64_t *)addr - 2;
        munmap(p, p[0]);
    }
#else
    free(addr);
#endif
}
