/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2015 Tony Houghton <h@realh.co.uk>

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

#define G_LOG_DOMAIN "roxterm-shim"

#include <glib.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MIN_CAPACITY 1024
#define EXCESS_CAPACITY 2048
#define MAX_CAPACITY 1024 * 1024

static FILE *debug_out = NULL;

typedef struct {
    int fd;
    guint8 *buf;
    gssize capacity;
    gsize offset_to_next_str;
    gsize offset_to_end;
    gboolean eof;
} BufferedReader;

static const char *fd_name(int fd)
{
    static char *name = NULL;

    switch (fd)
    {
        case 0:
            return "stdin";
        case 1:
            return "stdout";
        case 2:
            return "stderr";
        default:
            g_free(name);
            name = g_strdup_printf("fd%d", fd);
            return name;
    }
}

static inline BufferedReader *buffered_reader_new(int fd)
{
    BufferedReader *reader = g_new0(BufferedReader, 1);
    reader->fd = fd;
    return reader;
}

typedef enum {
    Normal,         // Just read up to buffer limit
    ForceEsc,       // Try to read until we get an ESC
    GetOsc,         // Make sure we've got enough to identify an OSC sequence
} BufferStrategy;

/**
 * Attempts to read from fd up to and including an escape character. If the
 * escape is a double byte (ESC something), it will include the second byte.
 * If strategy is ForceEsc it will enlarge the buffer (up to a point) to
 * accommodate a long clipboard, otherwise it may return before encountering an
 * ESC.
 *
 * The result should not be freed and will be overwritten on the next call.
 * end_out is the offset of the byte following the last byte read or one past
 * the escape character. Returns NULL without setting errmsg if no bytes are
 * read (assumes EOF).
 */
static const guint8 *read_until_esc(BufferedReader *reader,
    gsize *end_out, BufferStrategy strategy, char const **errmsg)
{
    /* If there's stuff left over from the previous read, move it to the start
     * of the buffer.
     */
    if (reader->offset_to_next_str)
    {
        memcpy(reader->buf, reader->buf + reader->offset_to_next_str,
            reader->offset_to_end - reader->offset_to_next_str);
        reader->offset_to_end -= reader->offset_to_next_str;
        reader->offset_to_next_str = 0;
    }
    gsize chunk_offset = 0;

    /* Start reading */
    while (TRUE)
    {
        gboolean want_esc_code = FALSE;
        /* See if we can return the current buffered content. */
        for (gsize i = chunk_offset; i < reader->offset_to_end; ++i)
        {
            switch (reader->buf[i])
            {
                case 0x1b:      // ESC
                    if (i + 1 < reader->offset_to_end)
                    {
                        ++i;
                        G_GNUC_FALLTHROUGH;
                    }
                    else
                    {
                        want_esc_code = TRUE;
                        break;
                    }
                case 0x9c:      // ST
                case 0x9d:      // OSC
                case 0x07:      // BEL
                    *end_out = i + 1;
                    reader->offset_to_next_str = i + 1;
                    return reader->buf;
            }
        }

        /* Any surplus from previous read should be sent to pty asap.
         */
        gboolean strategy_satisifed;
        if (strategy == Normal)
        {
            strategy_satisifed = TRUE;
        }
        else if (strategy == GetOsc)
        {
            const guint8 *b = reader->buf;
            gsize l = reader->offset_to_end;
            strategy_satisifed = FALSE;
            if (l >= 18)
            {
                strategy_satisifed = TRUE;
            }
            else if (l >= 6 && b[0] == '5' && b[1] == '2' && b[3] == ';')
            {
                for (gsize n = 3; n < l; ++n)
                {
                    if (b[n] == ';' && n < l - 1)
                    {
                        strategy_satisifed = TRUE;
                        break;
                    }
                }
            }
        }
        else
        {
            strategy_satisifed = FALSE;
        }
        if (strategy_satisifed && !want_esc_code && reader->offset_to_end) {
            *end_out = reader->offset_to_end;
            reader->offset_to_next_str = 0;
            reader->offset_to_end = 0;
            return reader->buf;
        }

        if (reader->eof)
            return NULL;

        /* Buf was empty, or didn't contain c, read more data. */
        chunk_offset = reader->offset_to_end;
        gsize spare_capacity = reader->capacity - chunk_offset;
        gsize new_capacity = 0;

        /* Make sure we have enough capacity for a decent read, but if we
         * have excess, shrink the buffer to make sure memory is freed after
         * a huge paste and that we're not reading big chunks which might
         * increase latency.
         */
        if (spare_capacity < MIN_CAPACITY)
            new_capacity = reader->capacity + MIN_CAPACITY;
        else if (spare_capacity > EXCESS_CAPACITY)
            new_capacity = chunk_offset + MIN_CAPACITY;
        if (new_capacity > MAX_CAPACITY && !want_esc_code)
        {
            if (strategy == ForceEsc)
                strategy = Normal;
            continue;
        }
        if (new_capacity)
        {
            reader->buf = g_realloc(reader->buf, new_capacity);
            reader->capacity = new_capacity;
            spare_capacity = new_capacity - chunk_offset;
        }
        if (want_esc_code)
            spare_capacity = 1;

        ssize_t nread = read(reader->fd, reader->buf + reader->offset_to_end,
                             spare_capacity);
        if (nread == 0)
        {
            reader->eof = TRUE;
        }
        else if (nread < 0)
        { 
            *errmsg = strerror(errno);
            return NULL;
        }
        reader->offset_to_end += nread;
    }
    return NULL;
}

// Returns number of bytes written, 0 for EOF, <0 for error
static gssize blocking_write(int fd, const guint8 *data, gsize length,
                             char const *name)
{
    gsize nwritten = 0;
    while (nwritten < length)
    {
        gssize n = write(fd, data + nwritten, length - nwritten);
        if (n == 0)
        {
            fprintf(debug_out, "No bytes written to %s (EOF?)\n", name);
            return 0;
        }
        else if (n < 0)
        {
            fprintf(debug_out, "Error writing to %s: %s\n",
                    name, strerror(errno));
            return n;
        }
        else
        {
            nwritten += n;
        }
    }
    return nwritten;
}

typedef struct {
    int output;
    int pipe_from_child;
    guintptr roxterm_tab_id;
} RoxtermPipeContext;

typedef enum {
    NormalMode,             // Receiving naything other than OSC 52 payload
    StartedOsc,             // Last chunk ended with OSC code
    ReceivingClipBoard,     // We are receiving clipboard, not terminated yet
    EscTooLarge,            // Esc sequence has filled oversized buffer
} PipeFilterState;

// If buf ends with an OSC code, returns true and sets (esc1_out, esc2_out)
// to (0x1b, ']') or (0x9d, 0) accordingly. Otherwise returns FALSE and
// leaves esc1, esc2 unset.
static gboolean check_buf_end_for_osc(const guint8 *buf, gsize buflen,
                                      guint8 *esc1_out, guint8 *esc2_out)
{
    guint8 c = buf[buflen - 1];
    if (buflen >= 2 && buf[buflen - 2] == 0x1b)
    {
        *esc1_out = 0x1b;
        *esc2_out = c;
        return c == ']';
    }
    if (c == 0x9d)
    {
        *esc1_out = c;
        *esc2_out = 0;
        return TRUE;
    }
    return FALSE;

}

// Returns TRUE if the buffer ends with a valid terminator.
static gboolean check_buf_end_for_terminator(const guint8 *buf, gsize buflen)
{
    switch (buf[buflen - 1])
    {
        case 0x9c:
        case 0x07:
            return TRUE;
        case '\\':
            return buflen >= 2 && buf[buflen - 2] == 0x1b;
        default:
            return FALSE;
    }
}

// Processes clipboard content. part1 is optional, only present if the OSC 52
// data was split across two buffers. It will be freed if so. Although part2
// is const it will actually have its terminator replaced with 0.
static void process_clipboard_write(guint8 **part1,
                                    gsize *part1_size,
                                    const guint8 *part2,
                                    gsize part2_size,
                                    guintptr roxterm_tab_id)
{
    const guint8 *p1 = *part1 ? *part1 : part2;
    gsize p1_size = *part1 ? *part1_size : part2_size;
    gssize semicolon = -1;
    gboolean own_base64;

    for (gsize i = 0; i < p1_size; ++i)
    {
        if (p1[i] == ';')
        {
            semicolon = i;
            break;
        }
    }
    if (semicolon == -1)
        goto free_part1;
    char *selection = g_strndup((const char *) p1, semicolon);
    if ((gssize) strspn(selection, "cpqs01234567") != semicolon)
    {
        g_free(selection);
        goto free_part1;
    }
    gsize total_size = *part1_size + part2_size - semicolon;
    guint8 *base64;
    gsize p1_size_after_semi = p1_size - semicolon - 1;
    if (*part1)
    {
        own_base64 = TRUE;
        base64 = g_new(guint8, total_size);
        memcpy(base64, *part1 + semicolon + 1, p1_size_after_semi);
        memcpy(base64 + p1_size_after_semi, part2, part2_size);
    }
    else
    {
        own_base64 = FALSE;
        base64 = (guint8 *) part2 + semicolon + 1;
    }
    if (base64[total_size - 2] == 0x1b)
        base64[total_size - 2] = 0;
    else
        base64[total_size - 1] = 0;
    if (base64[0])
    {
        gsize decoded_size;
        g_base64_decode_inplace((char *) base64, &decoded_size);
        fprintf(debug_out, "Decoded clipboard: %s\n", (char *) base64);
    }
    if (own_base64)
        g_free(base64);
free_part1:
    g_free(*part1);
    *part1 = NULL;
    *part1_size = 0;
}

// Returns TRUE if buf starts with "52;" and indicates it's a clipboard write
// and not a read. If TRUE semicolon_out will contain the index into buf of
// the semicolon separating the selection description from the payload.
static gboolean check_for_osc52_write(const guint8 *buf, gsize buflen)
{
    if (buflen < 5)
        return FALSE;
    if (buf[0] != '5' || buf[1] != '2' || buf[2] != ';')
        return FALSE;
    gsize n;
    for (n = 3; n < buflen && buf[n] != ';'; ++n);
    if (buf[n] != ';')
        return FALSE;
    if (buf[n + 1] == '?')
        return FALSE;
    return TRUE;
}

static gpointer osc52_pipe_filter(RoxtermPipeContext *rpc)
{
    BufferedReader *reader = buffered_reader_new(rpc->pipe_from_child);
    char *outname = g_strdup_printf("%lx:%s", rpc->roxterm_tab_id,
                                    fd_name(rpc->output));
    const char *errmsg = NULL;
    PipeFilterState pf_state = NormalMode;
    guint8 esc[2];
    guint8 *partial_clipboard = NULL;
    gsize partial_len = 0;

    while (TRUE)
    {
        const guint8 *buf;
        gsize buflen;
        // Only force read to next ESC if we know we're receiving a clipboard.
        // Other ESC sequences should be processed in smaller chunks to keep
        gboolean force_esc = pf_state == ReceivingClipBoard ||
            pf_state == StartedOsc;

        buf = read_until_esc(reader, &buflen, force_esc, &errmsg);
        if (errmsg)
        {
            fprintf(debug_out, "Error reading from %s: %s\n", outname, errmsg);
            exit(1);
        }
        else if (!buf || !buflen)
        {
            fprintf(debug_out, "No data read from %s (EOF?)\n", outname);
            return NULL;
        }

        switch (pf_state)
        {
            case NormalMode:
                if (check_buf_end_for_osc(buf, buflen, esc, esc + 1))
                    pf_state = StartedOsc;
                break;
            case StartedOsc:
                // Last read ended with OSC, this one should start with a 
                // number then ';'
                if (check_for_osc52_write(buf, buflen))
                {
                    if (check_buf_end_for_terminator(buf, buflen))
                    {
                        process_clipboard_write(&partial_clipboard,
                                                &partial_len, buf + 3,
                                                buflen - 3,
                                                rpc->roxterm_tab_id);
                        pf_state = NormalMode;
                    }
                    else
                    {
                        partial_clipboard = g_new(guint8, buflen);
                        partial_len = buflen;
                        memcpy(partial_clipboard, buf, buflen);
                        pf_state = ReceivingClipBoard;
                    }
                    continue;
                }
                else
                {
                    // Not OSC 52 write; send the code we stashed earlier
                    // before the payload we just received, then continue
                    // as normal.
                    int n = blocking_write(rpc->output,
                                           esc, esc[1] ? 2 : 1,
                                           outname);
                    if (n == 0)
                        return NULL;
                    else if (n < 0)
                        exit(1);
                    pf_state = NormalMode;
                }
                break;
            case ReceivingClipBoard:
                if (check_buf_end_for_terminator(buf, buflen))
                {
                    process_clipboard_write(&partial_clipboard,
                                            &partial_len, buf + 3,
                                            buflen - 3,
                                            rpc->roxterm_tab_id);
                    pf_state = NormalMode;
                    continue;
                }
                else
                {
                    // Clipboard is too long, ignore it
                    partial_clipboard = NULL;
                    partial_len = 0;
                    pf_state = EscTooLarge;
                    continue;
                }
                break;
            case EscTooLarge:
                if (check_buf_end_for_terminator(buf, buflen))
                    pf_state = NormalMode;
                continue;
        }

        gssize n = blocking_write(rpc->output, buf, buflen, outname);
        if (n < 0)
            exit(1);
        else if (!n)
            return NULL;
    }
    return NULL;
}

int main(int argc, char **argv)
{
    char *log_dir = g_build_filename(g_get_user_cache_dir(), "roxterm", NULL);
    g_mkdir_with_parents(log_dir, 0755);
    char *log_leaf = g_strdup_printf("shim-log-%s.txt", argv[1]);
    char *log_file = g_build_filename(log_dir, log_leaf, NULL);
    debug_out = fopen(log_file, "a");
    g_free(log_file);
    g_free(log_leaf);
    g_free(log_dir);

    RoxtermPipeContext *rpc_stdout = g_new(RoxtermPipeContext, 1);
    rpc_stdout->roxterm_tab_id = strtoul(argv[1], NULL, 16);
    rpc_stdout->output = 1;

    RoxtermPipeContext *rpc_stderr = g_new(RoxtermPipeContext, 1);
    rpc_stderr->roxterm_tab_id = rpc_stdout->roxterm_tab_id;
    rpc_stderr->output = 2;

    GPid pid;
    argv += 2;
    argc -= 2;
    char *cmd_leaf = g_path_get_basename(argv[0]);
    gboolean login = argv[1] != NULL && argv[1][0] == '-' &&
        !strcmp(argv[1] + 1, cmd_leaf);
    g_free(cmd_leaf);

    // g_spawn_async_with_pipes requires that argv is null-terminated and
    // there's no guarantee that the main argument is.
    char **argv2 = g_new(char *, argc + 1);
    memcpy(argv2, argv, argc * sizeof(char *));
    argv2[argc] = NULL;

    GError *error = NULL;
    if (g_spawn_async_with_pipes(
        NULL, argv2, NULL,
        G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_CHILD_INHERITS_STDIN |
        (login ? G_SPAWN_FILE_AND_ARGV_ZERO : G_SPAWN_SEARCH_PATH),
        NULL, NULL,
        &pid, NULL,
        &rpc_stdout->pipe_from_child,
        &rpc_stderr->pipe_from_child, &error))
    {
        if (!g_thread_new("stdout",
                          (GThreadFunc) osc52_pipe_filter, rpc_stdout))
        {
            fprintf(debug_out, "Failed to launch thread for stdout\n");
            exit(1);
        }
        if (!g_thread_new("stderr",
                          (GThreadFunc) osc52_pipe_filter, rpc_stderr))
        {
            fprintf(debug_out, "Failed to launch thread for stderr\n");
            exit(1);
        }
        int status = 0;
        waitpid(pid, &status, 0);
        fprintf(debug_out, "Child exited: %d, status %d\n",
                WIFEXITED(status), WEXITSTATUS(status));
        exit(status);
    }
    else
    {
        fprintf(debug_out, "Failed to run command: %s\n",
                error ? error->message : "unknown error");
        exit(1);
    }
    
    return 0;
}

/* vi:set sw=4 ts=4 et cindent cino= */
