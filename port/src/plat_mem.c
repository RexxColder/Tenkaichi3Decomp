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

/* ---- The game's own data, fetched from the user's disc (release builds).
   A program made by port/tools/strip_data.py has the data tables of the game's two programs and the VU1
   microprograms set to zero, and a list next to it (<program>.dat) of where each run of bytes comes from: the
   user's SLUS_216.78 (as a flat image from 0x100000) or BIN/DBZP.BIN. This copies them in before anything else
   runs. A program without that list still has the data linked in (the developer's build) and nothing happens. */
#include <string.h>
#include <unistd.h>
#define ROM_BASE 0x100000u
#define ROM_END 0x2FF180u

/* Set to 1 in the file by strip_data.py: this program does not work without its list. (2, not 0: it has to be in
   the file, not in the zero-filled part of memory.) */
volatile uint32_t gPortDataStripped = 2;

static uint8_t *read_file(const char *path, size_t *size) {
    FILE *fp = fopen(path, "rb");
    uint8_t *buf;
    long n;

    if (fp == NULL) {
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buf = malloc((size_t)n + 1);
    if (buf == NULL || fread(buf, 1, (size_t)n, fp) != (size_t)n) {
        fclose(fp);
        free(buf);
        return NULL;
    }
    fclose(fp);
    *size = (size_t)n;
    return buf;
}

static void data_fail(const char *what, const char *path) {
    fprintf(stderr, "bt3: %s%s%s\n     The game's data comes from your own disc image; run the setup (bt3-setup) to unpack it.\n", what,
            path != NULL ? ": " : "", path != NULL ? path : "");
    exit(2);
}

static void Port_LoadGameData(void) {
    char exe[1024], path[1200];
    const char *root = getenv("BT3_DATA") != NULL ? getenv("BT3_DATA") : "gamedata";
    uint8_t *list, *elf, *src[2] = {NULL, NULL};
    size_t listSize = 0, elfSize = 0, srcSize[2] = {0, 0};
    uint64_t hash = 0xCBF29CE484222325ull, want;
    uint32_t count, i, k;
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);

    if (n <= 0) {
        return;
    }
    exe[n] = '\0';
    snprintf(path, sizeof(path), "%s.dat", exe);
    list = read_file(path, &listSize);
    if (list == NULL) {
        if (gPortDataStripped == 1) {
            data_fail("this program needs the file that came with it", path);
        }
        return; /* no list: the data is linked in */
    }
    if (listSize < 16 || memcmp(list, "BT3D", 4) != 0) {
        data_fail("damaged file", path);
    }
    memcpy(&count, list + 4, 4);
    memcpy(&want, list + 8, 8);
    if (listSize < 16 + (size_t)count * 16) {
        data_fail("damaged file", path);
    }
    snprintf(path, sizeof(path), "%s/disc/SLUS_216.78", root);
    elf = read_file(path, &elfSize);
    if (elf == NULL || elfSize < 0x34 || memcmp(elf, "\177ELF", 4) != 0) {
        data_fail("the game's program was not found", path);
    }
    {   /* the loaded sections as one image from 0x100000 */
        uint32_t shoff, sh[6];
        uint16_t shentsize, shnum;
        memcpy(&shoff, elf + 0x20, 4);
        memcpy(&shentsize, elf + 0x2E, 2);
        memcpy(&shnum, elf + 0x30, 2);
        srcSize[0] = ROM_END - ROM_BASE;
        src[0] = calloc(1, srcSize[0]);
        for (i = 0; i < shnum && (size_t)shoff + (size_t)(i + 1) * shentsize <= elfSize; i++) {
            memcpy(sh, elf + shoff + (size_t)i * shentsize, sizeof(sh)); /* name, type, flags, addr, offset, size */
            if ((sh[2] & 2) && sh[1] != 8 && sh[5] != 0 && sh[3] >= ROM_BASE && sh[3] + sh[5] <= ROM_END &&
                (size_t)sh[4] + sh[5] <= elfSize) {
                memcpy(src[0] + (sh[3] - ROM_BASE), elf + sh[4], sh[5]);
            }
        }
        free(elf);
    }
    snprintf(path, sizeof(path), "%s/disc/BIN/DBZP.BIN", root);
    src[1] = read_file(path, &srcSize[1]);
    if (src[1] == NULL) {
        data_fail("the game's menu program was not found", path);
    }
    for (i = 0; i < count; i++) {
        uint32_t r[4]; /* address in this program, source, offset in it, length */
        const uint8_t *from;
        memcpy(r, list + 16 + (size_t)i * 16, 16);
        if (r[1] > 1 || (size_t)r[2] + r[3] > srcSize[r[1]]) {
            data_fail("damaged file (list of the game's data)", NULL);
        }
        from = src[r[1]] + r[2];
        memcpy((void *)(uintptr_t)r[0], from, r[3]);
        for (k = 0; k < r[3]; k++) {
            hash = (hash ^ from[k]) * 0x100000001B3ull;
        }
    }
    free(src[0]);
    free(src[1]);
    free(list);
    if (hash != want) {
        data_fail("the game's programs in your game data are not the unmodified USA release (SLUS-21678)", NULL);
    }
}

static void map(uint32_t addr, uint32_t size, const char *what);
__attribute__((constructor)) static void Port_MapMemory(void) {
    Port_LoadGameData();
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
