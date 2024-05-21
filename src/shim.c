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

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "config.h"
#include "roxterm-error.h"
#include "send-to-pipe.h"
#include "thread-compat.h"

#define MAX_OF(a, b) ((a) >= (b) ? (a) : (b))
#define MIN_OF(a, b) ((a) <= (b) ? (a) : (b))

#define DEFAULT_CAPACITY 2048
#define MIN_CAPACITY 1024
#define EXCESS_CAPACITY 4096
#define MAX_CAPACITY OSC52_SIZE_LIMIT
#define MAX_MOVE DEFAULT_CAPACITY

#define ESC_CODE 0x1b
#define OSC_CODE 0x9d
#define OSC_ESC ']'
#define ST_CODE 0x9c
#define ST_ESC '\\'
#define BEL_CODE 7

typedef struct Osc52FilterContext Osc52FilterContext;

// An item queued for output.
typedef struct OutputItem {
    struct OutputItem *next;
    guint8 *buf;
    gsize start;
    gsize size;
} OutputItem;

inline static void output_item_free(OutputItem *item)
{
    g_free(item->buf);
    g_free(item);
}

typedef struct {
    int fd;
    const char *name;
    GMutex lock;
    GCond cond;
    OutputItem *head;
    OutputItem *tail;
    gssize last_result; // <= -1 error, 0 EOF, >= 1 good
    gboolean stop;
    GThread *thread;
} OutputQueue;

static void output_queue_init(OutputQueue *oq, int fd, const char *name)
{
    oq->fd = fd;
    oq->name = name;
    g_mutex_init(&oq->lock);
    g_cond_init(&oq->cond);
    oq->head = NULL;
    oq->tail = NULL;
    oq->last_result = 1;
    oq->stop = FALSE;
    oq->thread = NULL;
}

// Call while the mutex is locked. Returns the result of the previous write.
static gssize queue_item_for_writing(OutputQueue *oq, OutputItem *item)
{

    gssize last_result = oq->last_result;
    if (last_result <= 0)
    {
        output_item_free(item);
        return last_result;
    }
    if (oq->tail)
        oq->tail->next = item;
    else
        oq->head = item;
    oq->tail = item;
    g_cond_signal(&oq->cond);
    return last_result;
}

// Typically writes (all) data from item to oq->fd. handle can be used to
// provide additional context.
typedef gssize (*OutputQueueItemFunc)(OutputQueue *oq, OutputItem *item,
                                      gpointer handle);

// Keeps popping items from the queue and passes them  to item_processor
static void queue_processor(OutputQueue *oq,
                            OutputQueueItemFunc item_processor,
                            gpointer handle)
{
    g_mutex_lock(&oq->lock);
    while (!oq->stop || oq->head)
    {
        if (!oq->head)
            g_cond_wait(&oq->cond, &oq->lock);
        OutputItem *item = oq->head;
        if (!item)
            continue;
        if (item == oq->tail)
            oq->tail = NULL;
        oq->head = item->next;
        g_mutex_unlock(&oq->lock);
        gssize nprocessed = item_processor(oq, item, handle);
        output_item_free(item);
        g_mutex_lock(&oq->lock);
        if (nprocessed <= 0)
            break;
    }
    g_mutex_unlock(&oq->lock);
}

// Signals the thread to stop when queue is empty
static void stop_queue_processor(OutputQueue *oq)
{
    g_mutex_lock(&oq->lock);
    oq->stop = TRUE;
    g_cond_signal(&oq->cond);
    g_mutex_unlock(&oq->lock);
}

struct Osc52FilterContext {
    int input_fd;
    guint8 *buf;
    gsize start;    // Offset to start of current data
    gsize end;      // Offset to one after current data
    gsize capacity; // Total capacity of buf
    GThread *input_thread;
    OutputQueue output_queue;
    OutputQueue debug_queue;
    // ptp_queue is a pointer because it may be shared by stdout and stderr
    OutputQueue *ptp_queue;
    FILE *debug_fp;
    GMutex debug_fp_lock;
};

// Returns number of bytes of data between ofc->start and ofc->end.
inline static gsize ofc_len(Osc52FilterContext *ofc)
{
    return ofc->end - ofc->start;
}

static char *log_dir = NULL;

// Returns number of bytes written, 0 for EOF, <0 for error
static gssize write_item_to_fd(OutputQueue *oq, OutputItem *item,
                               G_GNUC_UNUSED gpointer handle)
{
    const guint8 *data = item->buf + item->start;
    gsize length = item->size;
    gsize nwritten = 0;
    while (nwritten < length)
    {
        gssize n = write(oq->fd, data + nwritten, length - nwritten);
        if (n > 0)
            nwritten += n;
        else
            return n;
    }
    return nwritten;
}

static gssize write_item_to_fp(G_GNUC_UNUSED OutputQueue *oq, OutputItem *item,
                               gpointer handle)
{
    Osc52FilterContext *ofc = handle;
    FILE *fp = ofc->debug_fp;
    char *s = (char *) item->buf + item->start;
    int length = (int) item->size;
    gssize nwritten;
    g_mutex_lock(&ofc->debug_fp_lock);
    if (s[length - 1] == '\n')
        nwritten = fprintf(fp, "%*s", length, s);
    else
        nwritten = fprintf(fp, "%*s\n", length, s);
    fflush(fp);
    g_mutex_unlock(&ofc->debug_fp_lock);
    return nwritten;
}

static void ofc_debug(Osc52FilterContext *ofc, const char *format, ...)
{
    char *msg;
    va_list ap;
    va_start(ap, format);
    msg = g_strdup_vprintf(format, ap);
    va_end(ap);
    OutputItem *item = g_new(OutputItem, 1);
    item->buf = (guint8 *) msg;
    item->start = 0;
    item->size = strlen(msg);
    item->next = NULL;
    queue_item_for_writing(&ofc->debug_queue, item);
}

static void ofc_debug_data(Osc52FilterContext *ofc, const char *msg,
                          const guint8 *data, gsize length)
{
    GString *s = g_string_new_len((const char *) data, length);
    g_string_replace(s, "\033", "<ESC>", 0);
    g_string_replace(s, "\007", "<BEL>", 0);
    //g_string_replace(s, "\n", "\\n", 0);
    ofc_debug(ofc, "%s: %s", msg, s->str);
    g_string_free(s, TRUE);
}

static void ofc_debug_buf(Osc52FilterContext *ofc, const char *msg)
{
    ofc_debug(ofc, "ODB: %p->%p start %ld size %ld\n", ofc,
        ofc->buf, ofc->start, ofc_len(ofc));
    ofc_debug_data(ofc, msg, ofc->buf + ofc->start, ofc_len(ofc));
}

// This may leak if called with fd > 2
static const char *fd_name(int fd)
{
    switch (fd)
    {
        case 0:
            return "stdin";
        case 1:
            return "stdout";
        case 2:
            return "stderr";
        default:
            return g_strdup_printf("fd%d", fd);
    }
}

static gpointer debug_thread(gpointer handle)
{
    Osc52FilterContext *ofc = handle;
    queue_processor(&ofc->debug_queue, write_item_to_fp, ofc);
    return NULL;
}

static Osc52FilterContext *osc52_filter_context_new(int input_fd, int output_fd,
                                                    OutputQueue *ptp_queue)
{
    const char *output_name = fd_name(output_fd);
    Osc52FilterContext *ofc = g_new(Osc52FilterContext, 1);
    ofc->input_thread = NULL;
    ofc->input_fd = input_fd;
    ofc->buf = g_new(guint8, DEFAULT_CAPACITY);
    ofc->start = 0;
    ofc->end = 0;
    ofc->capacity = DEFAULT_CAPACITY;
    ofc->ptp_queue = ptp_queue;

    output_queue_init(&ofc->output_queue, output_fd, output_name);
    g_mutex_init(&ofc->debug_fp_lock);

    char *log_leaf = g_strdup_printf("osc52-log-%ld-%d-%s.txt",
        g_get_real_time(), getpid(), output_name);
    char *log_file = g_build_filename(log_dir, log_leaf, NULL);
    ofc->debug_fp = fopen(log_file, "w");
    g_free(log_file);
    g_free(log_leaf);
    char *debug_name = g_strdup_printf("debug-%s", output_name);
    output_queue_init(&ofc->debug_queue, -1, debug_name);
    fprintf(ofc->debug_fp, "Created ofc %p\n", ofc);
    ofc->debug_queue.thread = g_thread_new(debug_name, debug_thread, ofc);
    return ofc;
}

// Same as write_item_to_fd but logs errors/EOF
static gssize write_output(OutputQueue *oq, OutputItem *item,
                           gpointer handle)
{
    Osc52FilterContext *ofc = handle;
    gssize n = write_item_to_fd(oq, item, ofc);
    if (!ofc)
        return n;
    if (n == 0)
    {
        ofc_debug(ofc, "No bytes written to %s (EOF?)\n",
            oq->name);
    }
    else if (n < 0)
    {
        ofc_debug(ofc, "Error writing to %s: %s\n",
                oq->name, strerror(errno));
    }
    return n;
}

// As write_item_to_fd but prefixes data with its length by using send_to_pipe.
static gssize send_with_size(OutputQueue *oq, OutputItem *item,
                             gpointer handle)
{
    Osc52FilterContext *ofc = handle;
    if (ofc)
    {
        ofc_debug(ofc, "SWS: Sending %d bytes of OSC52 data", item->size);
    }
    int n = send_to_pipe(oq->fd, (const char *) item->buf + item->start,
                            item->size);
    if (!ofc) return n;
    if (n == 0)
    {
        ofc_debug(ofc, "SWS: No bytes written to %s (EOF?)\n",
            oq->name);
    }
    else if (n < 0)
    {
        ofc_debug(ofc, "SWS: Error writing to %s: %s\n",
                oq->name, strerror(errno));
    }
    return n;
}

inline static guint8 ofc_char(Osc52FilterContext *ofc, gsize index)
{
    return ofc->buf[ofc->start + index];
}

// If spare_capacity is 1, allows buf to exceed MAX_CAPACITY to avoid
// frustratingly giving up when all we need is one more byte to check for ST.
// Returns FALSE if buffer has reached max capacity.
static gboolean ensure_spare_capacity(Osc52FilterContext *ofc,
                                      gsize spare_capacity)
{
    if (ofc->capacity - ofc->end >= spare_capacity)
        return TRUE;

    gsize old_size = ofc->end - ofc->start;

    // Only shift data to the start if it will free up sufficient capacity,
    // otherwise it will be moved twice (inefficient).
    if (ofc->start >= spare_capacity)
    {
        if (old_size)
            memmove(ofc->buf, ofc->buf + ofc->start, old_size);
        ofc->start = 0;
        ofc->end = old_size;
    }
    if (ofc->capacity - ofc->end >= spare_capacity)
        return TRUE;

    gsize capacity = ofc->capacity * 2;

    // Capacity limit needs to be considered relative to start, because in some
    // circumstances start could already be near the capacity.
    gsize max_capacity = MAX_CAPACITY + ofc->start;
    if (capacity > max_capacity)
        capacity = max_capacity;
    if (capacity <= ofc->capacity)
    {
        if (spare_capacity == 1 && capacity < max_capacity + 1)
        {
            capacity++;
        }
        else
        {
            ofc_debug(ofc, "GMD: Buffer capacity %ld is at/beyond max\n",
                ofc->capacity);
            return FALSE;
        }
    }
    assert(capacity >= ofc->capacity);
    if (capacity > ofc->capacity)
    {
        // Not realloc, because usually the valid data won't occupy the whole
        // buffer
        guint8 *new_buf = g_new(guint8, capacity);
        if (old_size)
            memcpy(new_buf, ofc->buf + ofc->start, old_size);
        ofc->buf = new_buf;
        ofc->start = 0;
        ofc->end = old_size;
        ofc->capacity = capacity;
        ofc_debug(ofc, "Ensure: reallocated %p %ld\n",
            ofc->buf, DEFAULT_CAPACITY);
    }
    return TRUE;
}

// Reads more data into buf at end, extending buf's capacity if necessary.
// If buf is already at MAX_CAPACITY, no data will be read, and end will
// remain unchanged. Returns result of the read operation.
static gssize get_more_data(Osc52FilterContext *ofc, gsize min_capacity)
{
    ofc_debug(ofc,
              "GMD: Want %ld bytes, buf content %ld-%ld, "
              "capacity %ld\n",
              min_capacity, ofc->start, ofc->end, ofc->capacity);
    ensure_spare_capacity(ofc, min_capacity);
    ofc_debug(ofc,
              "GMD after ensure: Want %ld bytes, buf content %ld-%ld, "
              "capacity %ld\n",
              min_capacity, ofc->start, ofc->end, ofc->capacity);
    gsize spare_capacity = ofc->capacity - ofc->end;
    gssize count = MIN_OF(spare_capacity, DEFAULT_CAPACITY);
    gsize offset = ofc->end;
    ofc_debug(ofc, "GMD: Reading %ld bytes to %p + %ld\n",
            count, ofc->buf, ofc->start);
    gssize nread = read(ofc->input_fd, ofc->buf + offset, count);
    if (nread < 0)
        ofc_debug(ofc, "GMD: Read error %s\n", strerror(errno));
    else if (nread == 0)
        ofc_debug(ofc, "GMD: Read EOF\n");
    if (nread > 0)
        ofc->end += nread;
    ofc_debug(ofc, "Read %ld/%ld/%ld, start %ld, end %ld\n",
        nread, count, spare_capacity, ofc->start, ofc->end);
    return nread;
}

// Queues nwrite bytes starting from from ofc->start and updates start to point
// to one past the written data. If nwrite is 0, discard the data and reset buf
// to default size. Buf size is also reset if all valid data was written.
static gssize send_data_to_output_queue(Osc52FilterContext *ofc,
                                        OutputQueue *oq, gsize nwrite)
{
    if (nwrite > 0)
    {
        // Prepare the OutputItem before locking to check whether it can be
        // flushed, because if it can't be flushed, the time wasted by this
        // is irrelevant, and the lock should be held for as short a time as
        // possible.
        OutputItem *item = g_new(OutputItem, 1);
        item->next = NULL;
        // If the data to be written is smaller than the remainder of the
        // main buffer, copy the former to a new buffer, otherwise use ofc's
        // buf in the item and replace it with a copy of the remainder.
        gsize surplus = ofc_len(ofc) - nwrite;
        if (nwrite < surplus)
        {
            ofc_debug(ofc,
                "Q: Copying %ld bytes to q buffer, %ld surplus\n",
                nwrite, surplus);
            item->buf = g_new(guint8, nwrite);
            memcpy(item->buf, ofc->buf + ofc->start, nwrite);
            item->start = 0;
            item->size = nwrite;
            ofc->start += nwrite;
        }
        else
        {
            ofc_debug(ofc,
                "Q: Queuing %ld bytes in buf, %ld surplus to new buf\n",
                nwrite, surplus);
            item->buf = ofc->buf;
            item->start = ofc->start;
            item->size = nwrite;
            gsize capacity = MAX_OF(surplus, DEFAULT_CAPACITY);
            ofc->buf = g_new(guint8, capacity);
            ofc->capacity = capacity;
            ofc_debug(ofc, "Q: reallocated %p %ld\n",
                ofc->buf, capacity);
            if (surplus)
                memcpy(ofc->buf, item->buf + item->start + nwrite, surplus);
            ofc->start = 0;
            ofc->end = surplus;
        }

        g_mutex_lock(&oq->lock);
        nwrite = queue_item_for_writing(oq, item);
        g_mutex_unlock(&oq->lock);
        if (nwrite <= 0)
        {
            ofc_debug(ofc, "Q: Previous output result %ld\n", nwrite);
            return nwrite;
        }
    }

    if (!nwrite || (ofc->start == ofc->end && ofc->start != 0))
    {
        ofc->start = 0;
        ofc->end = 0;
        if (ofc->capacity > DEFAULT_CAPACITY)
        {
            // Don't use realloc because the data is invalid so copying it
            // is wasteful
            g_free(ofc->buf);
            ofc->buf = g_new(guint8, DEFAULT_CAPACITY);
            ofc->capacity = DEFAULT_CAPACITY;
        }
    }
    return nwrite;
}

inline static gssize flush_up_to(Osc52FilterContext *ofc, gsize nwrite)
{
    return send_data_to_output_queue(ofc, &ofc->output_queue, nwrite);
}

typedef enum {
    IsUnknownEsc,   // ESC character, but missing subsequent data
    IsOtherEsc,     // ESC, but not ST or OSC
    IsUnknownOsc,   // OSC, number unknown
    IsOtherOsc,     // OSC but confirmed not 52
    IsOsc52,
    IsST,           // ST = String terminator
    IsBel,          // Also acceptable as an alternative to ST
    IsNotEsc,
} EscStatus;

static char const *esc_status_names[] = {
    "IsUnknownEsc",
    "IsOtherEsc",
    "IsUnknownOsc",
    "IsOtherOsc",
    "IsOsc52",
    "IsST",
    "IsBel",
    "IsNotEsc",
};

// offset is relative to ofc->start. If the buffer contains 52; but not a valid
// *write* request, returns IsOtherOsc.
static EscStatus buf_contains_52(Osc52FilterContext *ofc, gsize offset)
{
    gsize buflen = ofc_len(ofc);
    ofc_debug_data(ofc, "Looking for '52;...' in ", 
        ofc->buf + ofc->start + offset, buflen - offset);
    if (offset < buflen - 1 && ofc_char(ofc, offset) != '5')
        return IsOtherOsc;
    if (offset < buflen - 2 && ofc_char(ofc, offset + 1) != '2')
        return IsOtherOsc;
    if (offset < buflen - 3 && ofc_char(ofc, offset + 2) != ';')
        return IsOtherOsc;
    if (offset + 3 >= buflen)
        return IsUnknownOsc;
    // Don't allow more than one each of 'c' and 'p'
    // Other selection characters are allowed, but will be ignored in the
    // absence of c or p.
    gboolean have_c = FALSE;
    gboolean have_p = FALSE;
    gboolean have_semi = FALSE;
    for (offset = offset + 3; offset < buflen; ++offset)
    {
        guint8 c = ofc_char(ofc, offset);
        if (have_semi)
            return c == '?' ? IsOtherOsc : IsOsc52;
        if (c == ';')
        {
            if (have_c || have_p)
            {
                have_semi = TRUE;
                continue;
            }
            else
            {
                return IsOtherOsc;
            }
        }
        else if (c == 'c' && !have_c)
            have_c = TRUE;
        else if (c == 'p' && !have_p)
            have_p = TRUE;
        else if (!strchr("qs01234567", (char) c))
        {
            ofc_debug(ofc, "BC52: Invalid '%c' in selection\n", c);
            return IsOtherOsc;
        }
    }
    ofc_debug(ofc, "BC52: Finished without detecting OSC52\n");
    return IsOtherOsc;
}

// The osc52 argument determines whether it looks for 52 after finding OSC.
static EscStatus get_esc_status_at_offset(Osc52FilterContext *ofc, gsize offset,
                                          gboolean osc52)
{
    gsize buflen = ofc_len(ofc);
    guint8 c = ofc_char(ofc, offset);
    gsize esc_size = 0;
    
    switch (c)
    {
        case OSC_CODE:
            esc_size = 1;
            break;
        case ESC_CODE:
            esc_size = 2;
            break;
        case BEL_CODE:
            return IsBel;
            break;
        case ST_CODE:
            return IsST;
            break;
        default:
            return IsNotEsc;
    }
    EscStatus esc_status = IsOtherEsc;
    if (esc_size == 1)
    {
        esc_status = IsUnknownOsc;
    }
    else if (esc_size == 2)
    {
        if (offset + 1 >= buflen)
            esc_status = IsUnknownEsc;
        else if ((c = ofc_char(ofc, offset + 1)) == OSC_ESC)
            esc_status = IsUnknownOsc;
        else if (c == ST_ESC)
            return IsST;
        else
            esc_status = IsOtherEsc;
    }
    if (esc_status == IsUnknownOsc && osc52)
        esc_status = buf_contains_52(ofc, offset + esc_size);
    if (esc_status != IsOtherEsc)
    {
        ofc_debug(ofc, "FOS: esc_status %s at offset %ld/%ld\n",
            esc_status_names[esc_status], offset, ofc_len(ofc));
    }
    return esc_status;
}

typedef enum {
    EscNotFound,
    EscFound,
    EscInvalid,
    EscIOError,
} FindEscResult;

// osc52 determines whether to find OSC 52 (TRUE) or ST/BEL (FALSE). Search
// starts from *offset_in_out, which gets overwritten with the matching/
// offending offset relative to ofc->start.
static FindEscResult find_esc(Osc52FilterContext *ofc, gboolean osc52,
                              gsize *offset_in_out)
{
    // If we're looking for a terminator, we need to skip char at 0 because
    // it will be the ESC at the start of the OSC 52 and will look invalid
    gsize offset = *offset_in_out;
    for (; offset < ofc_len(ofc); ++offset)
    {
        EscStatus esc_status = get_esc_status_at_offset(ofc, offset, osc52);
        if (esc_status == IsUnknownEsc || (osc52 && esc_status == IsUnknownOsc))
        {
            // Don't flush if we're looking for a terminator
            if (osc52)
            {
                flush_up_to(ofc, offset);
                offset = 0;
            }
            // Get more data and check the status again.
            gsize capacity = esc_status == IsUnknownEsc ? 1 : MIN_CAPACITY;
            if (get_more_data(ofc, capacity) <= 0)
            {
                if (osc52)
                    flush_up_to(ofc, offset);
                *offset_in_out = 0;
                return EscIOError;
            }
            esc_status = get_esc_status_at_offset(ofc, offset, osc52);
        }

        *offset_in_out = offset;
        
        switch (esc_status)
        {
        case IsNotEsc:
            break;
        case IsUnknownEsc:
            ofc_debug(ofc, "FE: OscStatus %s, fail!\n",
                esc_status_names[esc_status]);
            break;
        case IsUnknownOsc:
            if (osc52)
            {
                ofc_debug(ofc, "FOS: OscStatus %s, fail!\n",
                    esc_status_names[esc_status]);
            }
            break;
        case IsOsc52:
            return osc52 ? EscFound : EscInvalid;
            break;
        case IsOtherEsc:
        case IsOtherOsc:
            if (!osc52)
                return EscInvalid;
            break;
        case IsST:
        case IsBel:
            if (!osc52)
                return EscFound;
            break;
        }
    }

    return EscNotFound;
}

// If data between ofc->start and ofc->end contains the start of an OSC 52,
// the data leading up to it is flushed, and this returns 1. The ESC will now
// be at offset 0. Otherwise returns 0 for no match, -1 for error/EOF.
static int find_osc_start(Osc52FilterContext *ofc)
{
    ofc_debug(ofc, "Looking for OSC 52 in ofc %p\n", ofc);
    ofc_debug_buf(ofc, "Looking for OSC 52 in");
    gsize offset = ofc->start;
    FindEscResult esc = find_esc(ofc, TRUE, &offset);
    switch (esc)
    {
    case EscNotFound:
        break;
    case EscInvalid:    // Shouldn't be possible when OSC 52 is TRUE
        ofc_debug(ofc, "FOS: EscStatus invalid, fail!\n");
        break;
    case EscFound:
        if (offset)
            flush_up_to(ofc, offset);
        return 1;
    case EscIOError:
        return -1;
    }
    return 0;
}

static void discard_esc_sequence(Osc52FilterContext *ofc, gsize offset)
{
    // Discards buffer from start to offset. If the character at offset is
    // ESC_CODE, discard the next byte too.
    if (ofc_char(ofc, offset) == ESC_CODE)
    {
        if (offset + 1 + ofc->start >= ofc->end)
            ofc_debug(ofc, "discard_esc_sequence: truncated at esc\n");
        else
            ++offset;
    }
    ofc->start += offset + 1;
}

// A return value of 0 means we didn't find a terminator and buffer is full,
// so data should be skipped until a terminator is found. >0 means end found
// at that offset, -1 means error.
static gssize find_osc52_end(Osc52FilterContext *ofc, gboolean skip)
{
    gsize offset = 1;

    while (TRUE)
    {
        gssize nread;
        ofc_debug_data(ofc, "Looking for ESC terminator in",
            ofc->buf + offset, ofc_len(ofc) - offset);
        FindEscResult esc = find_esc(ofc, FALSE, &offset);

        switch (esc) {
        case EscNotFound:
            if (ofc->end >= MAX_CAPACITY)
            {
                // Empty buffer without saving, then continue, skipping until
                // terminator or some other ESC sequence.
                ofc_debug(ofc,
                    "FOE: OSC 52 buffer reached max capacity\n");
                discard_esc_sequence(ofc, offset);
                return 0;
            }
            ofc_debug(ofc, "FOE: Requesting %ld more bytes\n",
                DEFAULT_CAPACITY);
            nread = get_more_data(ofc, DEFAULT_CAPACITY);
            ofc_debug(ofc, "FOE: Got %ld more bytes\n", nread);
            if (nread <= 0)
                return -1;
            ++offset;
            break;
        case EscInvalid:
            // Discard the buffer and continue as normal from the new ESC.
            ofc_debug(ofc, "FOE: Invalid ESC\n");
            discard_esc_sequence(ofc, offset);
            return 1;
        case EscFound:
            if (skip) {
                ofc_debug(ofc, "FOE: Found belated terminator\n");
                discard_esc_sequence(ofc, offset);
                return 0;
            } else {
                gsize end = ofc_char(ofc, offset) == ESC_CODE
                        ? offset + 1 : offset;
                ofc_debug_data(ofc, "FOE: OSC 52", ofc->buf + ofc->start,
                               end);
                return offset;
            }
        case EscIOError:
            return -1;
        }
    }
    return 0;
}

static void queue_osc52_message(Osc52FilterContext *ofc, gssize len)
{
    OutputItem *item = g_new(OutputItem, 1);

    // Skip one or two chars of leading ESC + "52;"
    int offset = (ofc_char(ofc, 0) == ESC_CODE) ? 5 : 4;
    len -= offset;

    // Message must be 0-terminated
    item->size = len + 1;

    item->buf = g_malloc(len + 1);
    item->start = 0;
    item->next = NULL;
    item->buf[len] = 0;
    memcpy(item->buf, ofc->buf + ofc->start + offset, len);
    ofc_debug(ofc, "Sending OSC52 message %ld '%s'", len, item->buf);
    ofc_debug(ofc, "Original followed by %d",
        ofc->buf[ofc->start + len + offset]);
    g_mutex_lock(&ofc->ptp_queue->lock);
    queue_item_for_writing(ofc->ptp_queue, item);
    g_mutex_unlock(&ofc->ptp_queue->lock);
}

static gpointer input_filter(Osc52FilterContext *ofc)
{
    gboolean skip = FALSE;

    while (TRUE)
    {
        if (ofc->start == ofc->end)
        {
            gssize nread = get_more_data(ofc, MIN_CAPACITY);
            if (nread == 0) {
                ofc_debug(ofc, "IF: Read EOF\n");
                return NULL;
            } else if (nread < 0) {
                ofc_debug(ofc, "IF: Read error %s\n",
                        strerror(errno));
                return NULL;
            }
            ofc_debug(ofc, "IF: Read %ld, buflen %ld, start %ld, end %ld",
                    nread, ofc_len(ofc), ofc->start, ofc->end);
        }

        gssize osc52_end_result = skip ? find_osc52_end(ofc, skip) : -1;

        if (!skip)
        {
            switch (find_osc_start(ofc)) {
            case -1:
                return NULL;
            case 0:
                // No OSC52, flush anyway and continue
                if (flush_up_to(ofc, ofc_len(ofc)) <= 0)
                    return NULL;
                break;
            default:
                osc52_end_result = find_osc52_end(ofc, skip);
                break;
            }
        }

        switch (osc52_end_result)
        {
            case -1:
                flush_up_to(ofc, ofc_len(ofc));
                return NULL;
            case 0:
                skip = TRUE;
                break;
            default:
                queue_osc52_message(ofc, osc52_end_result);
                discard_esc_sequence(ofc, osc52_end_result);
                break;
        }
    }
    return NULL;
}

static gpointer output_writer(Osc52FilterContext *ofc)
{
    queue_processor(&ofc->output_queue, write_output, ofc);
    return NULL;
}

static gpointer ptp_writer(Osc52FilterContext *ofc)
{
    queue_processor(ofc->ptp_queue, send_with_size, ofc);
    return NULL;
}

static GThread *run_osc52_thread(GThreadFunc thread_func,
                                 Osc52FilterContext *ofc,
                                 OutputQueue *oq,
                                 const char *name_prefix)
{
    const char *fd_name = oq ? oq->name : ofc->output_queue.name;
    char *thread_name = g_strdup_printf("%s-%s",
        name_prefix, fd_name);
    GThread *thread = g_thread_new(thread_name, thread_func, ofc);
    if (!thread)
    {
        ofc_debug(ofc, "Failed to launch %s thread\n", thread_name);
        exit(1);
    }
    ofc_debug(ofc, "Launched %s thread copying from %d to %s\n",
            thread_name, ofc->input_fd, fd_name);
    return thread;
}

static gboolean start_threads(Osc52FilterContext *ofc)
{
    ofc->input_thread = run_osc52_thread((GThreadFunc) input_filter,
        ofc, NULL, "input");
    if (!ofc->input_thread)
        return FALSE;
    GThread *thread =
        run_osc52_thread((GThreadFunc) output_writer, ofc,
                         &ofc->output_queue, "output");
    ofc->output_queue.thread = thread;
    return ofc->output_queue.thread != NULL;
}

// Waits for the threads to finish. This should only be called after the child
// process has closed the outputs the filters are reading, otherwise the input
// thread would block indefinitely. 
static void stop_and_join_threads(Osc52FilterContext *ofc)
{
    g_thread_join(ofc->input_thread);
    stop_queue_processor(&ofc->output_queue);
    g_thread_join(ofc->output_queue.thread);
    stop_queue_processor(&ofc->debug_queue);
    g_thread_join(ofc->debug_queue.thread);
    fprintf(ofc->debug_fp, "Shutdown cleanly\n");
}

static void err_to_parent(GError *error, int pipe)
{
    char *data = g_strdup_printf("ERR %u,%d,%s", error->domain,
        error->code, error->message);
    send_to_pipe(pipe, data, -1);
}

int main(int argc, char **argv)
{
    log_dir = g_build_filename(g_get_user_cache_dir(), "roxterm", NULL);
    g_mkdir_with_parents(log_dir, 0755);

    int pipe_to_parent = strtoul(argv[1], NULL, 10);

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

    //int stdout_pipe = -1;
    int stderr_pipe = -1;

    GError *error = NULL;
    if (g_spawn_async_with_pipes(
        NULL, argv2, NULL,
        G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_CHILD_INHERITS_STDIN |
        (login ? G_SPAWN_FILE_AND_ARGV_ZERO : G_SPAWN_SEARCH_PATH),
        NULL, NULL,
        &pid, NULL,
        NULL, &stderr_pipe,
        &error))
    {
        OutputQueue *ptp_queue = g_new(OutputQueue, 1);
        output_queue_init(ptp_queue, pipe_to_parent, "ptp");

        // TODO: stdout
        Osc52FilterContext *ofc_stderr = osc52_filter_context_new(
            stderr_pipe, 2, ptp_queue);

        g_free(log_dir);

#if !GLIB_CHECK_VERSION(2, 32, 0)
        g_thread_init(NULL);
#endif

        // Lock this before starting its thread to make sure "OK " is the first
        // message sent
        g_mutex_lock(&ptp_queue->lock);

        ptp_queue->thread =
            g_thread_new("ptp-writer", (GThreadFunc) ptp_writer, ofc_stderr);

        if (!ptp_queue->thread || !start_threads(ofc_stderr))
        {
            error = g_error_new(ROXTERM_SPAWN_ERROR, ROXTermSpawnThreadError,
                "Failed to create threads");
            err_to_parent(error, pipe_to_parent);
            exit(1);
        }

        OutputItem *ok = g_new(OutputItem, 1);
        ok->next = NULL;
        ok->buf = (guchar *) g_strdup_printf("OK %d", pid);
        ok->start = 0;
        ok->size = strlen((char *) ok->buf) + 1;
        queue_item_for_writing(ptp_queue, ok);
        g_mutex_unlock(&ptp_queue->lock);

        int status = 0;
        waitpid(pid, &status, 0);
        ofc_debug(ofc_stderr, "Child exited: %d, status %d\n",
                WIFEXITED(status), WEXITSTATUS(status));
        stop_and_join_threads(ofc_stderr);
        stop_queue_processor(ptp_queue);
        g_thread_join(ptp_queue->thread);
        exit(status);
    }
    else
    {
        fprintf(stderr, "Failed to run command: %s\n",
                error ? error->message : "unknown error");
        err_to_parent(error, pipe_to_parent);
        exit(1);
    }
    
    return 0;
}

/* vi:set sw=4 ts=4 et cindent cino= */
