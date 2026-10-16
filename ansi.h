/* ansi.h - wttr.in ANSI/UTF-8 text -> SymbOS C_TEXT_CTRL line buffers
 *
 * Plain C, no SymbOS dependencies, so it also compiles with gcc for testing.
 */
#ifndef WT_ANSI_H
#define WT_ANSI_H

#define WT_COLS    125   /* visible characters per line (wttr.in v1 width) */
#define WT_LINES   44    /* lines kept; full 3-day report is 40 */
#define WT_LINEBUF 200   /* bytes per encoded line incl. colour codes + 0 */

/* SymbOS palette indices used as defaults (see SCC graphics.md) */
#define WT_DEF_FG  8     /* white  */
#define WT_BG      1     /* black  */
#define WT_ATTR(fg, bg) ((unsigned char)(((fg) << 4) | (bg)))
#define WT_DEFATTR WT_ATTR(WT_DEF_FG, WT_BG)

/* Convert `len` bytes of wttr.in output at `src` into up to `maxlines`
 * C_TEXT_CTRL strings. Line i is written to lines + i * WT_LINEBUF.
 * Colour changes are encoded as 0x01, (fg << 4) | bg. UTF-8 box-drawing,
 * arrows and weather symbols are mapped to the custom font's 0x80+ range;
 * anything unknown becomes '?'. Returns the number of lines produced. */
unsigned char ansi_render(const unsigned char *src, unsigned short len,
                          char *lines, unsigned char maxlines);

#endif
