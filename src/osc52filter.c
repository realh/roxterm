/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2024 Tony Houghton <h@realh.co.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <ctype.h>
#include <dlfcn.h>

#include "glib.h"
#include "intptrmap.h"
#include "roxterm.h"
#include "vte/vte.h"
#include "osc52filter.h"

#define ESC_CODE 0x1b
#define OSC_CODE 0x9d
#define OSC_AFTER_ESC ']'
#define TERM_CODE 0x9c
#define TERM_AFTER_ESC '\\'
#define BEL_CODE 7

typedef enum {
    STATE_DEFAULT,          // Default state, wait for start of Escape sequence
    STATE_ESC_RECEIVED,     // Just received ESC_CODE
    STATE_OSC_RECEIVED,     // Just received OSC_CODE or
                            // ESC_CODE + OSC_AFTER_ESC
    STATE_5_RECEIVED,       // Received '5' as first digit of OSC number
    STATE_52_RECEIVED,      // Received "52" as OSC number
    STATE_CAPTURE_OSC52,    // Got OSC 52, capturing data
    STATE_CAPTURE_TERM_ESC, // Received ESC_CODE while capturing,
                            // expecting TERM_AFTER_ESC
    STATE_OTHER_ESC,        // Waiting for terminator sequence without capture
    STATE_OTHER_TERM,       // Received ESC_CODE in STATE_OTHER_ESC,
                            // expecting TERM_AFTER_ESC
} Osc52State;

struct Osc52Filter {
    ROXTermData *roxterm;
    int pts_fd;
    Osc52State state;
    size_t max_buflen;
    guint8 *data;           // data being collected for OSC 52 payload
    size_t data_len;
    const guint8 *buf;      // points to next byte to be read from `read` buf
    size_t buflen;          // number of bytes remaining in buf
};

// static char const *osc52_state_names[] = {
//     "DEFAULT",
//     "ESC_RECEIVED",
//     "OSC_RECEIVED",
//     "5_RECEIVED",
//     "52_RECEIVED",
//     "CAPTURE_OSC52",
//     "CAPTURE_TERM_ESC",
//     "OTHER_ESC",
//     "OTHER_TERM",
// };

static inline void osc52filter_free(Osc52Filter *oflt)
{
    g_free(oflt->data);
    g_free(oflt);
}

typedef struct {
    IntPointerMap fd_map;
} Osc52Global;

static Osc52Global osc52filter_global;

gboolean osc52filter_global_initialised = FALSE;

static Osc52Global *
osc52filter_global_init(Osc52Global *og)
{
    int_pointer_map_init(&og->fd_map);
    return og;
}

static inline void osc52filter_ensure_global_init()
{
    if (!osc52filter_global_initialised)
    {
        osc52filter_global_init(&osc52filter_global);
        osc52filter_global_initialised = TRUE;
    }
}

Osc52Filter *osc52filter_create(ROXTermData *roxterm, size_t buflen)
{
    VteTerminal *vte = roxterm_get_vte_terminal(roxterm);
    VtePty *pty = vte ? vte_terminal_get_pty(vte) : NULL;
    int fd = pty ? vte_pty_get_fd(pty) : -1;
    if (fd <= 0)
    {
        g_debug("Pty not available yet for roxterm %p", roxterm);
        return NULL;
    }
    osc52filter_ensure_global_init();
    Osc52Filter *oflt = g_new0(Osc52Filter, 1);
    oflt->roxterm = roxterm;
    oflt->pts_fd = fd;
    oflt->max_buflen = buflen;
    int_pointer_map_insert(&osc52filter_global.fd_map, fd, oflt);
    g_debug("osc52: Launching roxterm %p with pty fd %d", roxterm, fd);
    return oflt;
}

void osc52filter_set_buffer_size(Osc52Filter *oflt, size_t buflen)
{
    oflt->max_buflen = buflen;
    // TODO: If the filter already contains data longer than this,
    // discard it.
}

void osc52filter_remove(Osc52Filter *oflt)
{
    int_pointer_map_remove(&osc52filter_global.fd_map, oflt->pts_fd);
    osc52filter_free(oflt);
}

inline static guint8 osc52filter_get_next_byte(Osc52Filter *oflt)
{
    --oflt->buflen;
    return *(oflt->buf++);
}

inline static void osc52filter_unpeek(Osc52Filter *oflt)
{
    ++oflt->buflen;
    oflt->buf--;
}

static inline const char *osc52filter_code_desc(guint8 byte)
{
    static char bdesc[32];
    if (isprint(byte))
        sprintf(bdesc, "%d '%c'", byte, byte);
    else
        sprintf(bdesc, "%d", byte);
    return bdesc;
}

static inline void osc52filter_set_state(Osc52Filter *oflt, guint8 byte,
                                         Osc52State state)
{
    (void) byte;
    // g_debug("osc52: Code %s triggered state change from %s to %s",
    //         osc52filter_code_desc(byte),
    //         osc52_state_names[oflt->state],
    //         osc52_state_names[state]);
    oflt->state = state;
}

static void osc52filter_cancel_paste(Osc52Filter *oflt)
{
    g_debug("osc52: cancelling");
    g_free(oflt->data);
    oflt->data = NULL;
    oflt->data_len = 0;
}

typedef struct {
    ROXTermData *roxterm;
    guint8 *data;
    size_t data_len;
} Osc52PasteClosure;

static int osc52_deferred_paste(Osc52PasteClosure *closure)
{
    // Make sure this terminal hasn't been destroyed in the meantime
    gboolean ok = roxterm_is_valid(closure->roxterm);
    if (!ok)
    {
        g_debug("osc52: roxterm %p was destroyed before handling clipboard",
                closure->roxterm);
    }
    char *semi = NULL;
    if (ok)
        semi = strchr((char *) closure->data, ';');
    if (semi)
    {
        *semi = 0;
    }
    else
    {
        ok = FALSE;
        g_warning("osc52: ';' missing from payload");
    }
    if (ok)
    {
        gsize offset = (guint8 *) (semi + 1) - closure->data;
        roxterm_osc52_handler(closure->roxterm, (const char *) closure->data,
                              closure->data, offset,
                              closure->data_len - offset);
    }
    else
    {
        g_free(closure->data);
    }
    g_free(closure);
    return G_SOURCE_REMOVE;
}

static void osc52filter_complete_paste(Osc52Filter *oflt)
{
    g_debug("osc52: completing");
    Osc52PasteClosure *closure = g_new(Osc52PasteClosure, 1);
    closure->roxterm = oflt->roxterm;
    closure->data = oflt->data;
    closure->data_len = oflt->data_len;
    oflt->data = NULL;
    oflt->data_len = 0;
    g_idle_add((GSourceFunc) osc52_deferred_paste, closure);
}

static void osc52filter_capture_buffer(Osc52Filter *oflt)
{
    // g_debug("osc52: Capturing up to %ld bytes", oflt->buflen);
    const guint8 *buf_start = oflt->buf;
    guint8 byte = 0;
    while (oflt->buflen)
    {
        byte = osc52filter_get_next_byte(oflt);
        if (byte == ESC_CODE)
        {
            osc52filter_set_state(oflt, byte, STATE_CAPTURE_TERM_ESC);
            break;
        }
        else if (byte == TERM_CODE || byte == BEL_CODE)
        {
            osc52filter_set_state(oflt, byte, STATE_DEFAULT);
            break;
        }
        else if (byte == '?')
        {
            // Clipboard query is not supported
            osc52filter_set_state(oflt, byte, STATE_OTHER_ESC);
            osc52filter_cancel_paste(oflt);
            return;
        }
    }
    size_t caplen = oflt->buf - buf_start;
    if (oflt->state != STATE_CAPTURE_OSC52)
    {
        --caplen;
        // if (byte == ESC_CODE)
        // {
        //     g_debug("osc52: captured buf contains ESC @ %ld", caplen);
        //     if (oflt->buflen)
        //     {
        //         g_debug("osc52: ESC followed by %s",
        //                 osc52filter_code_desc(*oflt->buf));
        //     } else {
        //         g_debug("osc52: ESC is at end of buf");
        //     }
        // }
        // else if (byte == TERM_CODE)
        // {
        //     g_debug("osc52: captured buf contains TERM_CODE @ %ld", caplen);
        // }
        // else if (byte == BEL_CODE)
        // {
        //     g_debug("osc52: captured buf contains BEL_CODE @ %ld", caplen);
        // }
    }
    // else
    // {
    //     g_debug("osc52: No terminator in this buf");
    // }
    // g_debug("osc52: %ld bytes can be captured, new total %ld",
    //         caplen, oflt->data_len + caplen);
    if (caplen)
    {
        // Make sure the capture is terminated by 0 so GLib's base64 decoder
        // can handle it
        oflt->data = g_realloc(oflt->data, oflt->data_len + caplen + 1);
        memcpy(oflt->data + oflt->data_len, buf_start, caplen);
        oflt->data_len += caplen;
        oflt->data[caplen] = 0;
    }
    // g_debug("osc52: total captured data: %s", (const char *) oflt->data);
    if (oflt->state == STATE_DEFAULT)
    {
        osc52filter_complete_paste(oflt);
    }
}

// This overrides the system read. When it's called on an fd in the map of
// pts fds it pushes a chunk containing a copy of the data read.
ssize_t read(int fd, void *buf, size_t nbyte)
{
    static ssize_t (*real_read)(int, void *, size_t) = NULL;
    if (!real_read) {
        real_read = dlsym(RTLD_NEXT, "read");
    }
    ssize_t n = real_read(fd, buf, nbyte);
    if (!osc52filter_global_initialised)
        return n;
    if (n <= 0 || !int_pointer_map_contains(&osc52filter_global.fd_map, fd))
        return n;
    Osc52Filter *oflt =
        int_pointer_map_lookup(&osc52filter_global.fd_map, fd);
    g_return_val_if_fail(oflt != NULL, n);
    oflt->buf = buf;
    oflt->buflen = n;
    while (oflt->buflen)
    {
        guint8 byte = osc52filter_get_next_byte(oflt);
        switch (oflt->state)
        {
            case STATE_DEFAULT:
                if (byte == ESC_CODE)
                    osc52filter_set_state(oflt, byte, STATE_ESC_RECEIVED);
                else if (byte == OSC_CODE)
                    osc52filter_set_state(oflt, byte, STATE_OSC_RECEIVED);
                break;
            case STATE_ESC_RECEIVED:
                if (byte == OSC_AFTER_ESC)
                    osc52filter_set_state(oflt, byte, STATE_OSC_RECEIVED);
                else if (byte != ESC_CODE)
                    osc52filter_set_state(oflt, byte, STATE_OTHER_ESC);
                break;
            case STATE_OSC_RECEIVED:
                if (byte == '5')
                {
                    osc52filter_set_state(oflt, byte, STATE_5_RECEIVED);
                }
                else
                {
                    osc52filter_unpeek(oflt);
                    osc52filter_set_state(oflt, byte, STATE_OTHER_ESC);
                }
                break;
            case STATE_5_RECEIVED:
                if (byte == '2')
                {
                    osc52filter_set_state(oflt, byte, STATE_52_RECEIVED);
                }
                else
                {
                    osc52filter_unpeek(oflt);
                    osc52filter_set_state(oflt, byte, STATE_OTHER_ESC);
                }
                break;
            case STATE_52_RECEIVED:
                if (byte == ';')
                {
                    osc52filter_set_state(oflt, byte, STATE_CAPTURE_OSC52);
                    osc52filter_capture_buffer(oflt);
                }
                else
                {
                    osc52filter_set_state(oflt, byte, STATE_OTHER_ESC);
                }
                break;
            case STATE_CAPTURE_OSC52:
                osc52filter_capture_buffer(oflt);
                break;
            case STATE_CAPTURE_TERM_ESC:
                if (byte == TERM_AFTER_ESC)
                {
                    osc52filter_complete_paste(oflt);
                }
                else
                {
                    osc52filter_cancel_paste(oflt);
                    osc52filter_unpeek(oflt);
                    osc52filter_set_state(oflt, byte, STATE_ESC_RECEIVED);
                }
                break;
            case STATE_OTHER_ESC:
                if (byte == ESC_CODE)
                    osc52filter_set_state(oflt, byte, STATE_OTHER_TERM);
                else if (byte == TERM_CODE || byte == BEL_CODE)
                    osc52filter_set_state(oflt, byte, STATE_DEFAULT);
                break;
            case STATE_OTHER_TERM:
                if (byte == TERM_AFTER_ESC)
                {
                    osc52filter_set_state(oflt, byte, STATE_DEFAULT);
                }
                else
                {
                    osc52filter_unpeek(oflt);
                    osc52filter_set_state(oflt, byte, STATE_ESC_RECEIVED);
                }
                break;
        }
    }
    return n;
}

/* vi:set sw=4 ts=4 et cindent cino= */
