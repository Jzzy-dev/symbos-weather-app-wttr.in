/* wttr.c - wttr.in weather viewer for SymbOS (MSX2, 512x212, 16 colours)
 *
 * Shows the standard 125-column wttr.in report with its colours, using a
 * 4x8 custom font and one C_TEXT_CTRL control per line.
 *
 * Menus:
 *   Location > Choose city...      opens a small modal dialog with a text field
 *              My location (IP)    lets wttr.in guess from your IP address
 *              Refresh
 *   View     > 3-day report / Narrow / Today only / Current conditions
 *
 * Build:  cc wttr.c ansi.c -lnet -N "wttr.in"
 */
#include <string.h>
#include <stdlib.h>
#include <symbos.h>
#include <network.h>
#include "ansi.h"
#include "font4x8.h"

#define RAWMAX     11264      /* full 3-day report is ~8.7 KB + headers */
#define LINE_H     8
#define MARGIN     2
#define TIMEOUT    2000       /* 20 s, in 1/100 s system counter ticks */
#define CITYMAX    40

/* menu values */
#define MENU_FULL  10         /* 10..13 = view_opt[0..3] */
#define VIEWS      4
#define MENU_CITY  20
#define MENU_IP    21
#define MENU_REFRESH 22

/* dialog control IDs */
#define ID_INPUT   1
#define ID_OK      2
#define ID_CANCEL  3

/* ------------------------------------------------------------------ data */
char raw[RAWMAX + 1];                     /* HTTP response (code segment)  */
char path[96];
_transfer char netbuf[1024];              /* bounce buffer for TCP_Receive */
_transfer char request[192];

_data char lines[WT_LINES * WT_LINEBUF];  /* C_TEXT_CTRL strings; must share */
                                          /* the data segment with wt_font  */
_data char city[CITYMAX + 1];             /* city currently shown          */
_data char entry[CITYMAX + 1];            /* text field in the dialog      */
_data char status[80];
_data char title[CITYMAX + 12];

char *view_opt[VIEWS] = {"", "n", "1", "0"};  /* full, narrow, today, now */
unsigned char view = 0;
char winID;
signed char dlgID = -1;

/* ------------------------------------------------------------ main window */
_transfer char icon[19] = {0x02, 0x08, 0x08, 0xFF, 0xFF, 0xF8, 0xF1, 0xF8, 0xF1,
                           0x8F, 0x1F, 0x8F, 0x1F, 0x8F, 0x1F, 0x8F, 0x1F, 0xFF, 0xFF};

/* Menus (entries must directly follow their Menu header) */
_transfer Menu locmenu = {4};
_transfer Menu_Entry lm_city    = {MENU_ACTIVE, "Choose city...",   MENU_CITY};
_transfer Menu_Entry lm_ip      = {MENU_ACTIVE, "My location (IP)", MENU_IP};
_transfer Menu_Entry lm_sep     = {MENU_SEPARATOR, 0, 0};
_transfer Menu_Entry lm_refresh = {MENU_ACTIVE, "Refresh",          MENU_REFRESH};

_transfer Menu viewmenu = {VIEWS};
_transfer Menu_Entry vm_full   = {MENU_ACTIVE | MENU_CHECKED, "3-day report", MENU_FULL + 0};
_transfer Menu_Entry vm_narrow = {MENU_ACTIVE, "Narrow (day/night)",          MENU_FULL + 1};
_transfer Menu_Entry vm_today  = {MENU_ACTIVE, "Today only",                  MENU_FULL + 2};
_transfer Menu_Entry vm_now    = {MENU_ACTIVE, "Current conditions",          MENU_FULL + 3};

_transfer Menu mainmenu = {2};
_transfer Menu_Entry mm_loc  = {MENU_ACTIVE | MENU_SUBMENU, "Location", (unsigned short)&locmenu};
_transfer Menu_Entry mm_view = {MENU_ACTIVE | MENU_SUBMENU, "View",     (unsigned short)&viewmenu};

/* Content: black background + one rich-text control per report line.
   Filled in at runtime by setup_content(). */
_transfer Ctrl ctl[1 + WT_LINES];
_transfer Ctrl_Text_Ctrl rec[WT_LINES];
_transfer Ctrl_Group main_group = {1 + WT_LINES, 0, ctl};

_transfer Window form = {
    WIN_NORMAL,
    WIN_CLOSE | WIN_TITLE | WIN_ICON | WIN_RESIZABLE | WIN_STATUS | WIN_MENU,
    0,                                   /* PID */
    0, 0,                                /* x, y */
    WT_COLS * 4 + 2 * MARGIN, 160,       /* w, h */
    0, 0,                                /* xscroll, yscroll */
    WT_COLS * 4 + 2 * MARGIN,            /* wfull */
    WT_LINES * LINE_H + 2 * MARGIN,      /* hfull */
    120, 40,                             /* wmin, hmin */
    WT_COLS * 4 + 2 * MARGIN,            /* wmax */
    WT_LINES * LINE_H + 2 * MARGIN,      /* hmax */
    icon,
    title,
    status,
    &mainmenu,
    &main_group,
    0, 0};                               /* no toolbar */

/* ------------------------------------------------------- "Choose city" dialog */
#define DLG_W 200
#define DLG_H 52

_transfer Ctrl_Text cd_label = {"City, airport code or postcode:",
                                (COLOR_BLACK << 2) | COLOR_ORANGE, ALIGN_LEFT};
_transfer Ctrl_Input cd_entry = {entry, 0, 0, 0, 0, CITYMAX, 0, 0, 0};

_transfer Ctrl d_area   = {0,         C_AREA,   -1, COLOR_ORANGE, 0, 0, 10000, 10000};
_transfer Ctrl d_label  = {0,         C_TEXT,   -1, (unsigned short)&cd_label, 6, 5, DLG_W - 12, 8};
_transfer Ctrl d_input  = {ID_INPUT,  C_INPUT,  -1, (unsigned short)&cd_entry, 6, 16, DLG_W - 12, 12};
_transfer Ctrl d_ok     = {ID_OK,     C_BUTTON, -1, (unsigned short)"OK",     DLG_W - 108, 34, 48, 12};
_transfer Ctrl d_cancel = {ID_CANCEL, C_BUTTON, -1, (unsigned short)"Cancel", DLG_W - 54,  34, 48, 12};
_transfer Ctrl_Group dlg_group = {5, 0, &d_area, 0, 0,
                                  4,            /* Return -> control #4 (OK)     */
                                  5,            /* Escape -> control #5 (Cancel) */
                                  {0, 0, 0, 0},
                                  3,            /* focus  -> control #3 (input)  */
                                  0};

_transfer Window dlg = {
    WIN_NORMAL,
    WIN_CLOSE | WIN_TITLE | WIN_NOTTASKBAR,
    0,
    (512 - DLG_W) / 2, 60,               /* x, y: roughly centred on MSX2 */
    DLG_W, DLG_H,
    0, 0,
    DLG_W, DLG_H,
    DLG_W, DLG_H,
    DLG_W, DLG_H,
    0,                                   /* no icon (not in taskbar) */
    "Choose city",
    0, 0,
    &dlg_group,
    0, 0};

/* --------------------------------------------------------------- helpers */
static void set_status(char *s)
{
    strcpy(status, s);
    Win_Redraw_Status(winID);
}

static void setup_content(void)
{
    unsigned char i;

    ctl[0].value = 0;
    ctl[0].type = C_AREA;
    ctl[0].bank = -1;
    ctl[0].param = COLOR_BLACK | AREA_16COLOR;
    ctl[0].x = 0;
    ctl[0].y = 0;
    ctl[0].w = 10000;
    ctl[0].h = 10000;

    for (i = 0; i < WT_LINES; ++i) {
        lines[(unsigned short)i * WT_LINEBUF] = 0;
        rec[i].text = lines + (unsigned short)i * WT_LINEBUF;
        rec[i].maxlen = WT_LINEBUF;
        rec[i].font = (char *)wt_font;
        rec[i].color = WT_DEFATTR;
        rec[i].underline = 0;

        ctl[i + 1].value = 10 + i;
        ctl[i + 1].type = C_TEXT_CTRL;
        ctl[i + 1].bank = -1;
        ctl[i + 1].param = (unsigned short)&rec[i];
        ctl[i + 1].x = MARGIN;
        ctl[i + 1].y = MARGIN + (unsigned short)i * LINE_H;
        ctl[i + 1].w = WT_COLS * 4;
        ctl[i + 1].h = LINE_H;
    }
}

/* Append city to path, URL-encoded (space -> '+', other specials -> %XX) */
static void append_city(char *dst)
{
    static char hex[] = "0123456789ABCDEF";
    char *s = city;
    unsigned char c;

    dst += strlen(dst);
    while ((c = *s++) != 0 && dst < path + sizeof(path) - 12) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
            || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_'
            || c == '~' || c == ',') {
            *dst++ = c;
        } else if (c == ' ') {
            *dst++ = '+';
        } else {
            *dst++ = '%';
            *dst++ = hex[c >> 4];
            *dst++ = hex[c & 15];
        }
    }
    *dst = 0;
}

/* --------------------------------------------------------- HTTP/1.0 GET */
/* SCC's HTTP_GET() speaks HTTP/1.1 and (as of this writing) stops after the
   first body packet, so we use a minimal HTTP/1.0 client on raw TCP: the
   server then sends a plain (non-chunked) body and closes when done. */

static unsigned short got;

static signed char drain(signed char sock, unsigned short want)
{
    TCP_Trans tr;
    unsigned short n;

    while (want) {
        if (want > sizeof(netbuf))
            want = sizeof(netbuf);
        if (TCP_Receive(sock, _symbank, netbuf, want, &tr))
            return -1;
        n = tr.transferred;
        if (n > RAWMAX - got)
            n = RAWMAX - got;          /* buffer full: discard the rest */
        memcpy(raw + got, netbuf, n);
        got += n;
        want = tr.remaining;
    }
    return 0;
}

/* Returns the HTTP status code (or 0 if none), -1 on network error. */
static int http_get(char **body, unsigned short *bodylen)
{
    char ip[4];
    signed char sock;
    NetStat ns;
    unsigned short deadline;
    char *p;
    int code = 0;

    got = 0;
    if (DNS_Resolve(_symbank, "wttr.in", ip))
        return -1;
    sock = TCP_OpenClient(ip, -1, 80);
    if (sock == -1)
        return -1;

    strcpy(request, "GET ");
    strcat(request, path);
    strcat(request, " HTTP/1.0\r\nHost: wttr.in\r\n"
                    "User-Agent: curl/8.0 (SymbOS)\r\nAccept: */*\r\n\r\n");
    if (TCP_Send(sock, _symbank, request, strlen(request)))
        goto fail;

    deadline = Sys_Counter16() + TIMEOUT;
    for (;;) {
        _symmsg[0] = 0;
        if (Msg_Receive(_sympid, _netpid, _symmsg) & 1) {
            if (_symmsg[0] != NET_TCPEVT)
                continue;
            TCP_Event(_symmsg, &ns);
            if (ns.socket != (unsigned char)sock)
                continue;
            if (drain(sock, ns.bytesrec))
                goto fail;
            deadline = Sys_Counter16() + TIMEOUT;
            if (ns.status == TCP_CLOSING || ns.status == TCP_CLOSED)
                break;
        } else {
            if ((signed short)(Sys_Counter16() - deadline) > 0) {
                _neterr = ERR_TIMEOUT;
                goto fail;
            }
            Idle();
        }
    }
    /* pick up anything that arrived together with the close */
    if (TCP_Status(sock, &ns) == 0 && ns.bytesrec)
        drain(sock, ns.bytesrec);
    TCP_Close(sock);

    raw[got] = 0;
    p = raw;
    if (strncmp(raw, "HTTP/", 5) == 0) {
        p = strchr(raw, ' ');
        if (p)
            code = atoi(p + 1);
        p = strstr(raw, "\r\n\r\n");
        p = p ? p + 4 : raw + got;
    }
    *body = p;
    *bodylen = got - (unsigned short)(p - raw);
    return code;

fail:
    TCP_Disconnect(sock);
    return -1;
}

/* ----------------------------------------------------------------- fetch */
static void set_title(void)
{
    strcpy(title, "wttr.in - ");
    strcat(title, city[0] ? city : "my location");
    Win_Redraw_Title(winID);
}

static void fetch(void)
{
    char *body;
    unsigned short len;
    unsigned char n, i;
    int code;

    if (!_netpid) {
        set_status("No network daemon running");
        return;
    }

    strcpy(path, "/");
    append_city(path);
    strcat(path, "?mAF");               /* metric, force ANSI, no footer */
    strcat(path, view_opt[view]);

    set_title();
    set_status("Fetching...");
    code = http_get(&body, &len);
    if (code < 0) {
        strcpy(status, "Network error ");
        itoa(_neterr, status + strlen(status), 10);
        Win_Redraw_Status(winID);
        return;
    }

    n = ansi_render((unsigned char *)body, len, lines, WT_LINES);
    for (i = n; i < WT_LINES; ++i)
        lines[(unsigned short)i * WT_LINEBUF] = 0;

    form.yscroll = 0;
    form.hfull = (unsigned short)(n ? n : 1) * LINE_H + 2 * MARGIN;
    Win_Redraw(winID, -1, 0);
    Win_Redraw_Slider(winID);

    if (code != 200) {
        strcpy(status, "HTTP ");
        itoa(code, status + 5, 10);
    } else {
        itoa(len, status, 10);
        strcat(status, " bytes");
    }
    Win_Redraw_Status(winID);
}

static void set_view(unsigned char v)
{
    Menu_Entry *e = &vm_full;
    unsigned char i;

    view = v;
    for (i = 0; i < VIEWS; ++i)
        e[i].flags = MENU_ACTIVE | (i == v ? MENU_CHECKED : 0);
    fetch();
}

/* ---------------------------------------------------------------- dialog */
static void dialog_open(void)
{
    unsigned short n;

    if (dlgID >= 0) {
        Win_Focus(dlgID);
        return;
    }
    /* prefill with the current city, all selected so typing replaces it */
    strcpy(entry, city);
    n = strlen(entry);
    cd_entry.scroll = 0;
    cd_entry.len = n;
    cd_entry.cursor = n;
    cd_entry.selection = -(signed short)n;
    cd_entry.flags = 0;

    dlg.state = WIN_NORMAL;
    dlg.flags |= WIN_MODAL;
    dlgID = Win_Open(_symbank, &dlg);
    form.modal = dlgID + 1;
}

static void dialog_close(unsigned char accept)
{
    if (dlgID < 0)
        return;
    Win_Close(dlgID);
    dlgID = -1;
    form.modal = 0;
    dlg.flags &= ~WIN_MODAL;
    if (accept) {
        strcpy(city, entry);
        fetch();
    }
}

/* ------------------------------------------------------------------ main */
int main(int argc, char *argv[])
{
    char ok;

    /* optional start city from the command line: wttr.exe Amsterdam */
    if (argc > 1)
        strncpy(city, argv[1], CITYMAX);

    setup_content();
    strcpy(status, "Location > Choose city... to pick a place");
    strcpy(title, "wttr.in");
    winID = Win_Open(_symbank, &form);

    ok = (Net_Init() == 0);
    if (!ok)
        set_status("No network daemon running");
    else
        fetch();                       /* argv city, or IP-based location */

    for (;;) {
        _symmsg[0] = 0;
        Msg_Sleep(_sympid, -1, _symmsg);

        /* user clicked the main window while the dialog is open */
        if (_symmsg[0] == MSR_DSK_WMODAL) {
            if (dlgID >= 0)
                Win_Focus(dlgID);
            continue;
        }
        if (_symmsg[0] != MSR_DSK_WCLICK)
            continue;

        if (dlgID >= 0 && _symmsg[1] == (unsigned char)dlgID) {
            /* ---- dialog events ---- */
            switch (_symmsg[2]) {
            case DSK_ACT_CLOSE:
                dialog_close(0);
                break;
            case DSK_ACT_CONTENT:
                if (_symmsg[8] == ID_OK)
                    dialog_close(1);
                else if (_symmsg[8] == ID_CANCEL)
                    dialog_close(0);
                break;
            }
            continue;
        }

        /* ---- main window events ---- */
        switch (_symmsg[2]) {
        case DSK_ACT_CLOSE:
            exit(0);
        case DSK_ACT_MENU:
            switch (_symmsg[8]) {
            case MENU_CITY:
                dialog_open();
                break;
            case MENU_IP:
                city[0] = 0;
                fetch();
                break;
            case MENU_REFRESH:
                fetch();
                break;
            default:
                if (_symmsg[8] >= MENU_FULL && _symmsg[8] < MENU_FULL + VIEWS)
                    set_view(_symmsg[8] - MENU_FULL);
            }
            break;
        }
    }
    return 0;
}
