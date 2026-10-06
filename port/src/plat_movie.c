/*
 * Movies on PC: Sony's libmpeg (sceMpeg*) as the game's movie player uses it (src/sys/movie.c).
 *
 * On the PS2 the game reads the .PSS file itself, hands the bytes to the library to split off the video stream,
 * and asks for one decoded picture per frame. A picture comes back as 16 x 16 blocks of RGBA, columns first, which
 * the game uploads straight into the frame buffer. The sound is a separate ADX file (port/src/gs/snd_adx.c).
 *
 * Here the decoding is done by the `ffmpeg` program: when the first picture is asked for, it is started on the
 * movie file (the path the game opened last, from plat_file.c) and writes raw 512 x 448 RGBA frames into a pipe;
 * each sceMpegGetPicture reads one and rearranges it into the block order. The bytes the game feeds to
 * sceMpegDemuxPss are ignored. Without ffmpeg (or without a window) a movie ends at once, as before.
 * A temporary arrangement: a 32-bit MPEG-2 library is not installed; a 64-bit build can link a decoder directly.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MOVIE_W 512
#define MOVIE_H 448

extern char gPortMoviePath[512];
extern int GsGpu_Enabled(void);

static FILE *sPipe;
static int sStarted, sEnd;
static uint8_t sFrame[MOVIE_W * MOVIE_H * 4];

static void movie_close(void) {
    if (sPipe != NULL) {
#ifdef _WIN32
        _pclose(sPipe);
#else
        pclose(sPipe);
#endif
        sPipe = NULL;
    }
    sStarted = 0;
    sEnd = 0;
}

static void movie_start(void) {
    char cmd[800];

    sStarted = 1;
    sEnd = 1;
    /* without a window too when recorded input is played back: the movie has to take the same number of frames */
    if ((!GsGpu_Enabled() && getenv("BT3_PAD_PLAY") == NULL) || gPortMoviePath[0] == '\0' || strchr(gPortMoviePath, '\'') != NULL || getenv("BT3_NOMOVIE") != NULL) {
        return;
    }
#ifdef _WIN32
    if (strchr(gPortMoviePath, '"') != NULL) {
        return;
    }
    snprintf(cmd, sizeof(cmd), "ffmpeg -v error -i \"%s\" -f rawvideo -pix_fmt rgba -s %dx%d - 2>NUL", gPortMoviePath, MOVIE_W, MOVIE_H);
    sPipe = _popen(cmd, "rb"); /* binary: the picture's bytes as they are */
#else
    snprintf(cmd, sizeof(cmd), "ffmpeg -v error -i '%s' -f rawvideo -pix_fmt rgba -s %dx%d - 2>/dev/null", gPortMoviePath, MOVIE_W, MOVIE_H);
    sPipe = popen(cmd, "r");
#endif
    sEnd = sPipe == NULL;
}

int sceMpegInit(void) { return 0; }
int sceMpegCreate(void *mp, void *work, int size) { (void)mp; (void)work; (void)size; movie_close(); return 0; }
int sceMpegDelete(void *mp) { (void)mp; movie_close(); return 0; }
int sceMpegReset(void *mp) { (void)mp; movie_close(); return 0; }
void *sceMpegAddCallback(void *mp, int type, void *cb, void *data) { (void)mp; (void)type; (void)cb; (void)data; return NULL; }
void *sceMpegAddStrCallback(void *mp, int type, int ch, void *cb, void *data) { (void)mp; (void)type; (void)ch; (void)cb; (void)data; return NULL; }
int sceMpegDemuxPss(void *mp, void *data, int size) { (void)mp; (void)data; (void)size; return 0; }

int sceMpegIsEnd(void *mp) {
    (void)mp;
    if (!sStarted) {
        movie_start();
    }
    return sEnd;
}

/* One picture into `rgb`: `blocks` 16 x 16 RGBA blocks of 0x400 bytes, columns first (the order the game's upload
   chain expects); alpha 0x80. */
int sceMpegGetPicture(void *mp, void *rgb, int blocks) {
    uint8_t *out = rgb;
    int cols = MOVIE_W / 16, rows = MOVIE_H / 16, i, j, y, x;

    (void)mp;
    if (!sStarted) {
        movie_start();
    }
    if (sPipe == NULL || fread(sFrame, 1, sizeof(sFrame), sPipe) != sizeof(sFrame)) {
        sEnd = 1;
        return -1;
    }
    for (i = 0; i < cols; i++) {
        for (j = 0; j < rows && i * rows + j < blocks; j++) {
            uint8_t *b = out + (size_t)(i * rows + j) * 0x400;
            for (y = 0; y < 16; y++) {
                const uint8_t *s = &sFrame[((j * 16 + y) * MOVIE_W + i * 16) * 4];
                for (x = 0; x < 16; x++) {
                    b[(y * 16 + x) * 4 + 0] = s[x * 4 + 0];
                    b[(y * 16 + x) * 4 + 1] = s[x * 4 + 1];
                    b[(y * 16 + x) * 4 + 2] = s[x * 4 + 2];
                    b[(y * 16 + x) * 4 + 3] = 0x80;
                }
            }
        }
    }
    return 0;
}
