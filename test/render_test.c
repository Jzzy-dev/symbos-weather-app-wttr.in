/* Host-side test: run ansi_render() on a captured wttr.in response.
 *   gcc -I.. -o render_test render_test.c ../ansi.c
 *   ./render_test default.txt lines.bin
 * Writes WT_LINES * WT_LINEBUF bytes (the exact buffers the SymbOS app uses)
 * and prints per-line usage so we can check WT_LINEBUF is large enough. */
#include <stdio.h>
#include <string.h>
#include "ansi.h"

static unsigned char src[16384];
static char lines[WT_LINES * WT_LINEBUF];

int main(int argc, char **argv)
{
    FILE *f;
    size_t n;
    unsigned char rows, r;
    int maxused = 0;

    if (argc < 3) {
        fprintf(stderr, "usage: %s input.ansi lines.bin\n", argv[0]);
        return 1;
    }
    f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    n = fread(src, 1, sizeof src, f);
    fclose(f);

    memset(lines, 0, sizeof lines);
    rows = ansi_render(src, (unsigned short)n, lines, WT_LINES);

    for (r = 0; r < rows; ++r) {
        const char *l = lines + r * WT_LINEBUF;
        int used = (int)strlen(l) + 1, vis = 0, i;
        for (i = 0; l[i]; ++i) {
            if (l[i] == 1) { ++i; continue; }
            ++vis;
        }
        if (used > maxused) maxused = used;
        if (vis > WT_COLS) printf("line %d too wide: %d\n", r, vis);
    }
    printf("input %zu bytes -> %u lines, max line buffer use %d/%d bytes\n",
           n, rows, maxused, WT_LINEBUF);

    f = fopen(argv[2], "wb");
    fwrite(lines, 1, sizeof lines, f);
    fclose(f);
    return 0;
}
