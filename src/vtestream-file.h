/*
 * Copyright (C) 2009,2010 Red Hat, Inc.
 * Copyright (C) 2013 Google, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Red Hat Author(s): Behdad Esfahbod
 * Google Author(s): Behdad Esfahbod
 * Independent Author(s): Egmont Koblinger
 */

/*
 * VteFileStream is implemented as three layers above each other.
 *
 * o The bottom layer is VteSnake. It provides a mapping from logical offsets
 *   to physical file offsets, storing the stream in at most 3 continuous
 *   regions of the file. See below for details how this mapping is done.
 *
 *   It operates with a fixed block size (64kB at the moment), allows
 *   random-access-read of a single block, random-access-overwrite of a single
 *   block within the stream, write (append) a single block right after the
 *   current head, advancing the tail by arbitrary number of blocks, and
 *   resetting. The appended block can be shorter, in that case we still
 *   advance by 64kB and let the operating system leave a gap (sparse blocks)
 *   in the file which is crucial for compression.
 *
 *   (Random-access-overwrite within the existing area is a rare event, occurs
 *   only when the terminal window size changes. We use it to redo differently
 *   the latest appends. In the topmost layer it's achieved by truncating at
 *   the head of the stream, and then appending again. In this layer and also
 *   in the next one, offering random-access-overwrite instead of truncation
 *   makes the implementation a lot easier. It's also essential to maintain a
 *   unique encryption IV in a forthcoming version.)
 *
 *   The name was chosen because VteFileStream's way of advancing the head and
 *   the tail is kinda like a snake, and the mapping to file offsets reminds
 *   me of the well-known game on old mobile phones.
 *
 * o The middle layer is called VteBoa. It does compression and is planned to
 *   do encryption along with integrity check. It has (almost) the same API as
 *   the snake, but the blocksize is a bit smaller to leave room for the
 *   required overhead.
 *
 *   The name was chosen because the world of encryption is full of three
 *   letter abbreviations. At this moment we're planning to use GNU TLS's
 *   method for doing AES GCM. Also, because grown-ups might think it's a hat,
 *   when actually it's a boa constrictor digesting an elephant :)
 *
 * o The top layer is VteFileStream. It does buffering and caching. As opposed
 *   to the previous layers, this one provides methods on arbitrary amount of
 *   data. It doesn't offer random-access-writes, instead, it offers appending
 *   data, and truncating the head (undoing the latest appends). Write
 *   requests are batched up until there's a complete block to be compressed
 *   and written to disk. Read requests are answered by reading and decrypting
 *   possibly more underlying blocks, and sped up by caching the result.
 *
 * Design discussions: https://bugzilla.gnome.org/show_bug.cgi?id=738601
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include "vteutils.h"

#ifndef VTESTREAM_MAIN
# define VTE_SNAKE_BLOCKSIZE 65536
typedef guint32 _vte_block_datalength_t;
#else
/* Smaller sizes for unit testing */
# define VTE_SNAKE_BLOCKSIZE     8
typedef guint8 _vte_block_datalength_t;
#endif

#define VTE_BLOCK_DATALENGTH_SIZE  sizeof(_vte_block_datalength_t)
#define VTE_BOA_BLOCKSIZE (VTE_SNAKE_BLOCKSIZE - VTE_BLOCK_DATALENGTH_SIZE)

#define OFFSET_BOA_TO_SNAKE(x) ((x) / VTE_BOA_BLOCKSIZE * VTE_SNAKE_BLOCKSIZE)
#define ALIGN_BOA(x) ((x) / VTE_BOA_BLOCKSIZE * VTE_BOA_BLOCKSIZE)
#define MOD_BOA(x)   ((x) % VTE_BOA_BLOCKSIZE)

/******************************************************************************************/

#ifndef HAVE_PREAD
#define pread _pread
static inline gsize
pread (int fd, char *data, gsize len, gsize offset)
{
	if (-1 == lseek (fd, offset, SEEK_SET))
		return -1;
	return read (fd, data, len);
}
#endif

#ifndef HAVE_PWRITE
#define pwrite _pwrite
static inline gsize
pwrite (int fd, char *data, gsize len, gsize offset)
{
	if (-1 == lseek (fd, offset, SEEK_SET))
		return -1;
	return write (fd, data, len);
}
#endif


static inline void
_file_close (int fd)
{
       if (G_UNLIKELY (fd == -1))
               return;

       close (fd);
}

static gboolean
_file_try_truncate (int fd, gsize offset)
{
	int ret;

        if (G_UNLIKELY (fd == -1))
                return FALSE;

	do {
                ret = ftruncate (fd, offset);
	} while (ret == -1 && errno == EINTR);

	return !ret;
}

static void
_file_reset (int fd)
{
        _file_try_truncate (fd, 0);
}

static gboolean
_file_try_punch_hole (int fd, gsize offset, gsize len)
{
#ifndef VTESTREAM_MAIN
# ifdef FALLOC_FL_PUNCH_HOLE
        static int n = 0;

        if (G_UNLIKELY (fd == -1))
                return FALSE;

        /* Punching hole is slow for me (Linux 3.16, ext4),
         * causing a ~10% overall performance regression.
         * On the other hand, it's required to see benefits from
         * compression in the finite scrollback case, without this
         * a smaller (better compressed) block will only overwrite
         * the first part of a larger (less compressed) block.
         * As a compromise, punch hole "randomly" with 1/16 chance.
         * TODOegmont: This is still very slow for me, no clue why. */
        if (G_UNLIKELY ((n++ & 0x0F) == 0)) {
                fallocate (fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, offset, len);
        }

        return TRUE;
# else
        return FALSE;
# endif
#else /* VTESTREAM_MAIN */
        /* For unittesting, overwrite the part with dots. */
        char c = '.';
        while (len--) {
                if (pwrite(fd, &c, 1, offset++) != 1)
                        return FALSE;
        }
        return TRUE;
#endif
}

static gsize
_file_read (int fd, char *data, gsize len, gsize offset)
{
	gsize ret, total = 0;

        if (G_UNLIKELY (fd == -1))
		return 0;

	while (len) {
                ret = pread (fd, data, len, offset);
		if (G_UNLIKELY (ret == (gsize) -1)) {
			if (errno == EINTR)
				continue;
			else
				break;
		}
		if (G_UNLIKELY (ret == 0))
			break;
		data += ret;
		len -= ret;
		offset += ret;
		total += ret;
	}
	return total;
}

static void
_file_write (int fd, const char *data, gsize len, gsize offset)
{
	gsize ret;

        if (G_UNLIKELY (fd == -1))
                return;

	while (len) {
                ret = pwrite (fd, data, len, offset);
		if (G_UNLIKELY (ret == (gsize) -1)) {
			if (errno == EINTR)
				continue;
			else
				break;
		}
		if (G_UNLIKELY (ret == 0))
			break;
		data += ret;
		len -= ret;
		offset += ret;
	}
}

/******************************************************************************************/

/*
 * VteSnake:
 *
 * The data structure implemented here remembers the last certain amount of
 * data written, with the size dynamically changing. Basic operations include
 * appending data (advancing the head), forgetting old data (advancing the
 * tail), and random access read. In these cases the logical head and tail
 * offsets can only increase. Rare special operations that don't influence the
 * overall design are overwriting existing data, and resetting the stream.
 *
 * The mapping from logical to physical offsets can be in one of 4 states:
 * 1. One continuous region;
 * 2. Two continuous regions, the second one preceding the first;
 * 3. Three continuous regions, the second one preceding the first, and the
 *    third being at the end;
 * 4. Two continuous regions, the first one preceding the second.
 *
 * In the example below, each lowercase letter represents a 64kB block, and
 * dots denote blocks whose contents are no longer important.
 *
 * Initially, data is simply written to the file, we're in state 1:
 * (A) abcd
 *
 * Later the tail starts to advance too, but we're still in state 1:
 * (B) ...de
 *
 * At some point, based on heuristics, we wrap to the beginning, into state 2:
 * (C) f..de
 *
 * The trivial case: If tail reaches EOF before head would bite tail, we're
 * back at state 1 and we can even truncate the file:
 * (D) fg...
 * (E) fg
 *
 * Let's write some more data and advance the tail to get back to a state
 * equivalent to (C).
 * (F) k..ij
 *
 * If head would bite tail, we continue appending at EOF, entering state 3:
 * (G) klmijno
 *
 * As tail keeps advancing, we're still in state 3:
 * (H) klm.jnop
 *
 * When tail finishes with the middle segment, we enter state 4:
 * (I) klm..nop
 *
 * Further advancing tail keeps us in state 4:
 * (J) ..m..nopqr
 *
 * When tail finishes with the first segment, we return to state 1:
 * (K) .....nopqr
 *
 * Depending on the aforementioned heuristics, we might stay in state 1:
 * (L) .....nopqrs
 *
 * But probably we soon return to state 2:
 * (M) ......opqrs
 * (N) tu....opqrs
 *
 * and so on...
 */

typedef struct _VteSnake {
        GObject parent;
        int fd;
        int state;
        struct {
                gsize st_tail;  /* Stream's logical tail offset. */
                gsize st_head;  /* Stream's logical head offset. */
                gsize fd_tail;  /* FD's physical tail offset. */
                gsize fd_head;  /* FD's physical head offset. One of these four is redundant, nevermind. */
        } segment[3];           /* At most 3 segments, [0] at the tail. */
        gsize tail, head;       /* These are redundant too, for convenience. */
} VteSnake;
#define VTE_SNAKE_SEGMENTS(s) ((s)->state == 4 ? 2 : (s)->state)

typedef struct _VteSnakeClass {
        GObjectClass parent_class;

        void (*reset) (VteSnake *snake, gsize offset);
        void (*write) (VteSnake *snake, gsize offset, const char *data, gsize len);
        gboolean (*read) (VteSnake *snake, gsize offset, char *data);
        void (*advance_tail) (VteSnake *snake, gsize offset);
        gsize (*tail) (VteSnake *snake);
        gsize (*head) (VteSnake *snake);
} VteSnakeClass;

static GType _vte_snake_get_type (void);
#define VTE_TYPE_SNAKE _vte_snake_get_type ()
#define VTE_SNAKE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), VTE_TYPE_SNAKE, VteSnakeClass))

G_DEFINE_TYPE (VteSnake, _vte_snake, G_TYPE_OBJECT)

static void
_vte_snake_init (VteSnake *snake)
{
        snake->fd = -1;
        snake->state = 1;
}

static void
_vte_snake_finalize (GObject *object)
{
        VteSnake *snake = (VteSnake *) object;

        _file_close (snake->fd);

        G_OBJECT_CLASS (_vte_snake_parent_class)->finalize(object);
}

static inline void
_vte_snake_ensure_file (VteSnake *snake)
{
        if (G_LIKELY (snake->fd != -1))
                return;

        snake->fd = _vte_mkstemp ();
}

static void
_vte_snake_reset (VteSnake *snake, gsize offset)
{
        g_assert_cmpuint (offset % VTE_SNAKE_BLOCKSIZE, ==, 0);

        _file_reset (snake->fd);

        snake->segment[0].st_tail = snake->segment[0].st_head = snake->tail = snake->head = offset;
        snake->segment[0].fd_tail = snake->segment[0].fd_head = 0;
        snake->state = 1;
}

/*
 * Turn a logical offset into a physical one; only for offsets that are
 * already within the streams, not for appending a new block.
 */
static gsize
_vte_snake_offset_map (VteSnake *snake, gsize offset)
{
        int i;
        int segments = VTE_SNAKE_SEGMENTS(snake);

        g_assert_cmpuint (offset % VTE_SNAKE_BLOCKSIZE, ==, 0);

        for (i = 0; i < segments; i++) {
                if (offset >= snake->segment[i].st_tail && offset < snake->segment[i].st_head)
                        return offset - snake->segment[i].st_tail + snake->segment[i].fd_tail;
        }
        g_assert_not_reached();
}

/* Place VTE_SNAKE_BLOCKSIZE bytes at data */
static gboolean
_vte_snake_read (VteSnake *snake, gsize offset, char *data)
{
        gsize fd_offset;

        g_assert_cmpuint (offset % VTE_SNAKE_BLOCKSIZE, ==, 0);

        if (G_UNLIKELY (offset < snake->tail || offset >= snake->head))
                return FALSE;

        fd_offset = _vte_snake_offset_map(snake, offset);

        return (_file_read (snake->fd, data, VTE_SNAKE_BLOCKSIZE, fd_offset) == VTE_SNAKE_BLOCKSIZE);
}

/*
 * offset is either within the stream (overwrite data), or at its head (append data).
 * data is at most VTE_SNAKE_BLOCKSIZE bytes large; if shorter then the remaining amount is skipped.
 * When reading back, that skipped area will contain garbage (e.g. when the FS doesn't support
 * punching holes), the caller needs to deal with it.
 *
 * When appending, the following state transfers can occur:
 * 1->2, 2->3.
 */
static void
_vte_snake_write (VteSnake *snake, gsize offset, const char *data, gsize len)
{
        gsize fd_offset;

        g_assert_cmpuint (offset, >=, snake->tail);
        g_assert_cmpuint (offset, <=, snake->head);
        g_assert_cmpuint (offset % VTE_SNAKE_BLOCKSIZE, ==, 0);

        if (G_LIKELY (offset == snake->head)) {
                /* Appending a new block to the head. */
                _vte_snake_ensure_file (snake);
                if (G_UNLIKELY (snake->state == 1 && 2 * snake->segment[0].fd_tail > snake->segment[0].fd_head)) {
                        /* State 1 -> 2 based on heuristics. The only crucial thing is that fd_tail needs to be greater than 0.
                         * Note: changing the heuristics might break the unit tests! */
                        snake->segment[1].st_tail = snake->segment[0].st_head;
                        snake->segment[1].st_head = snake->segment[0].st_head + VTE_SNAKE_BLOCKSIZE;
                        snake->segment[1].fd_tail = fd_offset = 0;
                        snake->segment[1].fd_head = VTE_SNAKE_BLOCKSIZE;
                        snake->state = 2;
                } else if (G_UNLIKELY (snake->state == 2 && snake->segment[1].fd_head == snake->segment[0].fd_tail)) {
                        /* State 2 -> 3 when head would bite the tail. */
                        snake->segment[2].st_tail = snake->segment[1].st_head;
                        snake->segment[2].st_head = snake->segment[1].st_head + VTE_SNAKE_BLOCKSIZE;
                        snake->segment[2].fd_tail = fd_offset = snake->segment[0].fd_head;
                        snake->segment[2].fd_head = snake->segment[0].fd_head + VTE_SNAKE_BLOCKSIZE;
                        snake->state = 3;
                } else {
                        /* No state change. */
                        int last_segment = VTE_SNAKE_SEGMENTS(snake) - 1;
                        fd_offset = snake->segment[last_segment].fd_head;
                        snake->segment[last_segment].st_head += VTE_SNAKE_BLOCKSIZE;
                        snake->segment[last_segment].fd_head += VTE_SNAKE_BLOCKSIZE;
                }
                if (snake->state != 2) {
                        /* Grow the file with sparse blocks to make sure that later pread() can
                         * read back a whole block, even if we are about to write a shorter one. */
                        _file_try_truncate (snake->fd, fd_offset + VTE_SNAKE_BLOCKSIZE);
#ifdef VTESTREAM_MAIN
                        /* For convenient unit testing only: fill with dots. */
                        _file_try_punch_hole (snake->fd, fd_offset, VTE_SNAKE_BLOCKSIZE);
#endif
                }
                snake->head = offset + VTE_SNAKE_BLOCKSIZE;
        } else {
                /* Overwriting an existing block. The new block might be shorter than the old one,
                 * punch a hole to potentially free up disk space (and for easier unit testing). */
                fd_offset = _vte_snake_offset_map(snake, offset);
                _file_try_punch_hole (snake->fd, fd_offset, VTE_SNAKE_BLOCKSIZE);
        }
        _file_write (snake->fd, data, len, fd_offset);
}

/*
 * When advancing the tail, the following state transfers can occur (even more
 * of them if the amount discarded is large enough):
 * 2->1, 3->4, 4->1.
 */
static void
_vte_snake_advance_tail (VteSnake *snake, gsize offset)
{
        g_assert_cmpuint (offset, >=, snake->tail);
        g_assert_cmpuint (offset, <=, snake->head);
        g_assert_cmpuint (offset % VTE_SNAKE_BLOCKSIZE, ==, 0);

        if (G_UNLIKELY (offset == snake->head)) {
                _vte_snake_reset (snake, offset);
		return;
        }

        while (offset > snake->segment[0].st_tail) {
                if (offset < snake->segment[0].st_head) {
                        /* Drop some (but not all) bytes from the first segment. */
                        _file_try_punch_hole (snake->fd, snake->segment[0].fd_tail, offset - snake->tail);
                        snake->segment[0].fd_tail += offset - snake->tail;
                        snake->segment[0].st_tail = snake->tail = offset;
                        return;
                } else {
                        /* Drop the entire first segment. */
                        switch (snake->state) {
                        case 1:
                                g_assert_not_reached();
                                break;
                        case 2:
                                snake->segment[0] = snake->segment[1];
                                _file_try_truncate (snake->fd, snake->segment[0].fd_head);
                                snake->state = 1;
                                break;
                        case 3:
                                _file_try_punch_hole (snake->fd, snake->segment[0].fd_tail, snake->segment[0].fd_head - snake->segment[0].fd_tail);
                                snake->segment[0] = snake->segment[1];
                                snake->segment[1] = snake->segment[2];
                                snake->state = 4;
                                break;
                        case 4:
                                _file_try_punch_hole (snake->fd, snake->segment[0].fd_tail, snake->segment[0].fd_head - snake->segment[0].fd_tail);
                                snake->segment[0] = snake->segment[1];
                                snake->state = 1;
                                break;
                        default:
                                g_assert_not_reached();
                                break;
                        }
                }
        }
        g_assert_cmpuint (snake->segment[0].st_tail, ==, offset);
        snake->tail = offset;
}

static gsize
_vte_snake_tail (VteSnake *snake)
{
        return snake->tail;
}

static gsize
_vte_snake_head (VteSnake *snake)
{
        return snake->head;
}

static void
_vte_snake_class_init (VteSnakeClass *klass)
{
        GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

        gobject_class->finalize = _vte_snake_finalize;

        klass->reset = _vte_snake_reset;
        klass->read = _vte_snake_read;
        klass->write = _vte_snake_write;
        klass->advance_tail = _vte_snake_advance_tail;
        klass->tail = _vte_snake_tail;
        klass->head = _vte_snake_head;
}

/******************************************************************************************/

/*
 * VteBoa: Compress the data.
 *
 * The data is stored as 4 bytes (1 byte for unit testing) denoting the length
 * of the compressed block, followed by the compressed data. The data is
 * stored uncompressed if compression didn't result in a smaller size.
 */

typedef struct _VteBoa {
        VteSnake parent;
        gsize tail, head;

        int compressBound;
} VteBoa;

typedef struct _VteBoaClass {
        GObjectClass parent_class;

        void (*reset) (VteBoa *boa, gsize offset);
        void (*write) (VteBoa *boa, gsize offset, const char *data);
        gboolean (*read) (VteBoa *boa, gsize offset, char *data);
        void (*advance_tail) (VteBoa *boa, gsize offset);
        gsize (*tail) (VteBoa *boa);
        gsize (*head) (VteBoa *boa);
} VteBoaClass;

static GType _vte_boa_get_type (void);
#define VTE_TYPE_BOA _vte_boa_get_type ()
#define VTE_BOA_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), VTE_TYPE_BOA, VteBoaClass))

G_DEFINE_TYPE (VteBoa, _vte_boa, VTE_TYPE_SNAKE)

/*----------------------------------------------------------------------------------------*/

/* Thin wrapper layers above the compression routines, for unit testing. */

static int
_vte_boa_compressBound (unsigned int len)
{
#ifndef VTESTREAM_MAIN
        return compressBound(len);
#else
        return 2 * len;
#endif
}

/* Compress; returns the compressed size which might be bigger than the original. */
static unsigned int
_vte_boa_compress (char *dst, unsigned int dstlen, const char *src, unsigned int srclen)
{
#ifndef VTESTREAM_MAIN
        uLongf dstlen_ulongf = dstlen;
        g_assert_cmpuint (compress2 ((Bytef *) dst, &dstlen_ulongf, (const Bytef *) src, srclen, 1), ==, Z_OK);
        return dstlen_ulongf;
#else
        /* Fake compression for unit testing:
         * Each char gets prefixed by a repetition count. This prefix is omitted if it would be the
         * same as the previous.
         * E.g. abcdef <-> 1abcdef
         *      www <-> 3w
         *      Mississippi <-> 1Mi2s1i2s1i2p1i
         *      bookkeeper <-> 1b2oke1per
         * The uncompressed string shouldn't contain digits, or more than 9 consecutive identical chars.
         */
        unsigned int len = 0, prevrepeat = 0;
        while (srclen) {
                unsigned int repeat = 1;
                while (repeat < srclen && src[repeat] == src[0]) repeat++;
                if (repeat != prevrepeat) {
                        *dst++ = '0' + repeat;
                        prevrepeat = repeat;
                        len++;
                }
                *dst++ = src[0];
                src += repeat, srclen -= repeat;
                len++;
        }
        return len;
#endif
}

/* Uncompress; returns the uncompressed size. */
static unsigned int
_vte_boa_uncompress (char *dst, unsigned int dstlen, const char *src, unsigned int srclen)
{
#ifndef VTESTREAM_MAIN
        uLongf dstlen_ulongf = dstlen;
        g_assert_cmpuint (uncompress ((Bytef *) dst, &dstlen_ulongf, (const Bytef *) src, srclen), ==, Z_OK);
        return dstlen_ulongf;
#else
        /* Fake decompression for unit testing; see above. */
        unsigned int len = 0, repeat = 0;
        while (srclen) {
                unsigned char c = *src;
                if (c >= '0' && c <= '9') {
                        repeat = c - '0';
                } else {
                        memset (dst, c, repeat);
                        dst += repeat, len += repeat;
                }
                src++; srclen--;
        }
        return len;
#endif
}

/*----------------------------------------------------------------------------------------*/

static void
_vte_boa_init (VteBoa *boa)
{
        boa->compressBound = _vte_boa_compressBound(VTE_BOA_BLOCKSIZE);
}

static void
_vte_boa_finalize (GObject *object)
{
        G_OBJECT_CLASS (_vte_boa_parent_class)->finalize(object);
}

static void
_vte_boa_reset (VteBoa *boa, gsize offset)
{
        g_assert_cmpuint (offset % VTE_BOA_BLOCKSIZE, ==, 0);

        _vte_snake_reset (&boa->parent, OFFSET_BOA_TO_SNAKE(offset));

        boa->tail = boa->head = offset;
}

/* Place VTE_BOA_BLOCKSIZE bytes at data. */
static gboolean
_vte_boa_read (VteBoa *boa, gsize offset, char *data)
{
        _vte_block_datalength_t compressed_len;
        gboolean ret = FALSE;
        char *buf = g_malloc(VTE_SNAKE_BLOCKSIZE);

        g_assert_cmpuint (offset % VTE_BOA_BLOCKSIZE, ==, 0);

        /* Read */
        if (G_UNLIKELY (!_vte_snake_read (&boa->parent, OFFSET_BOA_TO_SNAKE(offset), buf)))
                goto out;

        compressed_len = *((_vte_block_datalength_t *) buf);

        /* We could have read an empty block due to a previous disk full. Treat that as an error too. Perform other sanity checks. */
        if (G_UNLIKELY (compressed_len <= 0 || compressed_len > VTE_BOA_BLOCKSIZE))
                goto out;

        /* Uncompress, or copy if wasn't compressable */
        if (G_UNLIKELY (compressed_len >= VTE_BOA_BLOCKSIZE)) {
                memcpy (data, buf + VTE_BLOCK_DATALENGTH_SIZE, VTE_BOA_BLOCKSIZE);
        } else {
                g_assert_cmpuint (_vte_boa_uncompress(data, VTE_BOA_BLOCKSIZE, buf + VTE_BLOCK_DATALENGTH_SIZE, compressed_len), ==, VTE_BOA_BLOCKSIZE);
        }
        ret = TRUE;

out:
        g_free(buf);
        return ret;
}

/*
 * offset is either within the stream (overwrite data), or at its head (append data).
 * data is VTE_BOA_BLOCKSIZE bytes large.
 */
static void
_vte_boa_write (VteBoa *boa, gsize offset, const char *data)
{
        _vte_block_datalength_t compressed_len = boa->compressBound;

        /* The helper buffer should be large enough to contain a whole snake block,
         * and also large enough to compress data that actually grows bigger during compression. */
        char *buf = g_malloc(MAX(VTE_SNAKE_BLOCKSIZE,
                                 VTE_BLOCK_DATALENGTH_SIZE + boa->compressBound));

        g_assert_cmpuint (offset, >=, boa->tail);
        g_assert_cmpuint (offset, <=, boa->head);
        g_assert_cmpuint (offset % VTE_BOA_BLOCKSIZE, ==, 0);

        /* Compress, or copy if uncompressable */
        compressed_len = _vte_boa_compress (buf + VTE_BLOCK_DATALENGTH_SIZE, boa->compressBound,
                                        data, VTE_BOA_BLOCKSIZE);
        if (G_UNLIKELY (compressed_len >= VTE_BOA_BLOCKSIZE)) {
                memcpy (buf + VTE_BLOCK_DATALENGTH_SIZE, data, VTE_BOA_BLOCKSIZE);
                compressed_len = VTE_BOA_BLOCKSIZE;
        }

        *((_vte_block_datalength_t *) buf) = (_vte_block_datalength_t) compressed_len;

        /* Write */
        _vte_snake_write (&boa->parent, OFFSET_BOA_TO_SNAKE(offset), buf, VTE_BLOCK_DATALENGTH_SIZE + compressed_len);

        if (G_LIKELY (offset == boa->head)) {
                boa->head += VTE_BOA_BLOCKSIZE;
        }

        g_free(buf);
}

static void
_vte_boa_advance_tail (VteBoa *boa, gsize offset)
{
        g_assert_cmpuint (offset, >=, boa->tail);
        g_assert_cmpuint (offset, <=, boa->head);
        g_assert_cmpuint (offset % VTE_BOA_BLOCKSIZE, ==, 0);

        _vte_snake_advance_tail (&boa->parent, OFFSET_BOA_TO_SNAKE(offset));

        boa->tail = offset;
}

static gsize
_vte_boa_tail (VteBoa *boa)
{
        return boa->tail;
}

static gsize
_vte_boa_head (VteBoa *boa)
{
        return boa->head;
}

static void
_vte_boa_class_init (VteBoaClass *klass)
{
        GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

        gobject_class->finalize = _vte_boa_finalize;

        klass->reset = _vte_boa_reset;
        klass->read = _vte_boa_read;
        klass->write = _vte_boa_write;
        klass->advance_tail = _vte_boa_advance_tail;
        klass->tail = _vte_boa_tail;
        klass->head = _vte_boa_head;
}

/******************************************************************************************/

/*
 * VteFileStream: Implement buffering/caching on top of VteBoa.
 */

typedef struct _VteFileStream {
        GObject parent;

        VteBoa *boa;

        char *rbuf;
        /* Offset of the cached record, always a multiple of block size.
         * Use a value of 1 (or anything that's not a multiple of block size)
         * to denote if no record is cached. */
        gsize rbuf_offset;

        char *wbuf;
        gsize wbuf_len;

        gsize head, tail;
} VteFileStream;

typedef VteStreamClass VteFileStreamClass;

static GType _vte_file_stream_get_type (void);
#define VTE_TYPE_FILE_STREAM _vte_file_stream_get_type ()

G_DEFINE_TYPE (VteFileStream, _vte_file_stream, VTE_TYPE_STREAM)

VteStream *
_vte_file_stream_new (void)
{
	return (VteStream *) g_object_new (VTE_TYPE_FILE_STREAM, NULL);
}

static void
_vte_file_stream_init (VteFileStream *stream)
{
        stream->boa = g_object_new (VTE_TYPE_BOA, NULL);
        _vte_boa_init (stream->boa);

        stream->rbuf = g_malloc(VTE_BOA_BLOCKSIZE);
        stream->wbuf = g_malloc(VTE_BOA_BLOCKSIZE);
        stream->rbuf_offset = 1;  /* Invalidate */
}

static void
_vte_file_stream_finalize (GObject *object)
{
        VteFileStream *stream = (VteFileStream *) object;

        g_free(stream->rbuf);
        g_free(stream->wbuf);
        g_object_unref (stream->boa);

        G_OBJECT_CLASS (_vte_file_stream_parent_class)->finalize(object);
}

static void
_vte_file_stream_reset (VteStream *astream, gsize offset)
{
	VteFileStream *stream = (VteFileStream *) astream;
        gsize offset_aligned = ALIGN_BOA(offset);

        _vte_boa_reset (stream->boa, offset_aligned);
        stream->tail = stream->head = offset;

        /* When resetting at a non-aligned offset, initial bytes of the write buffer
         * will eventually be written to disk, although doesn't contain useful information.
         * Rather than leaving garbage there, fill it with zeros.
         * For unit testing, fill it with dashes for convenience. */
#ifndef VTESTREAM_MAIN
        memset(stream->wbuf, 0, MOD_BOA(offset));
#else
        memset(stream->wbuf, '-', MOD_BOA(offset));
#endif

        stream->wbuf_len = MOD_BOA(offset);
        stream->rbuf_offset = 1;  /* Invalidate */
}

static gboolean
_vte_file_stream_read (VteStream *astream, gsize offset, char *data, gsize len)
{
	VteFileStream *stream = (VteFileStream *) astream;

        /* Out of bounds request.
         * Note: It needs to detect when offset is extremely large
         * (actually a negative value stored in unsigned gsize),
         * and the read attempt wraps around to a sane offset:
         * https://bugzilla.gnome.org/show_bug.cgi?id=740347#c3
         * FIXME this is ugly and shouldn't be necessary, should fix our callers.
         */
        if (G_UNLIKELY (offset < stream->tail || offset + len > stream->head || offset + len < offset)) {
                /* If completely out of bounds, the caller expects a FALSE. */
                if (G_LIKELY (offset + len <= stream->tail || offset >= stream->head))
                        return FALSE;
                /* Partial out of bounds requests never happen. */
                g_assert_not_reached();
        }

        while (len && offset < ALIGN_BOA(stream->head)) {
                gsize l = MIN(VTE_BOA_BLOCKSIZE - MOD_BOA(offset), len);
                gsize offset_aligned = ALIGN_BOA(offset);
                if (offset_aligned != stream->rbuf_offset) {
                        if (G_UNLIKELY (!_vte_boa_read (stream->boa, offset_aligned, stream->rbuf)))
                                return FALSE;
                        stream->rbuf_offset = offset_aligned;
                }
                memcpy(data, stream->rbuf + MOD_BOA(offset), l);
                offset += l; data += l; len -= l;
        }
        if (len) {
                g_assert_cmpuint (MOD_BOA(offset) + len, <=, stream->wbuf_len);
                memcpy(data, stream->wbuf + MOD_BOA(offset), len);
        }
        return TRUE;
}

static void
_vte_file_stream_append (VteStream *astream, const char *data, gsize len)
{
	VteFileStream *stream = (VteFileStream *) astream;

        while (len) {
                gsize l = MIN(VTE_BOA_BLOCKSIZE - stream->wbuf_len, len);
                memcpy(stream->wbuf + stream->wbuf_len, data, l);
                stream->wbuf_len += l; data += l; len -= l;
                if (stream->wbuf_len == VTE_BOA_BLOCKSIZE) {
                        _vte_boa_write (stream->boa, ALIGN_BOA(stream->head), stream->wbuf);
                        stream->wbuf_len = 0;
                }
                stream->head += l;
        }
}

static void
_vte_file_stream_truncate (VteStream *astream, gsize offset)
{
	VteFileStream *stream = (VteFileStream *) astream;

        g_assert_cmpuint (offset, >=, stream->tail);
        g_assert_cmpuint (offset, <=, stream->head);

        if (offset < ALIGN_BOA(stream->head)) {
                /* Truncating goes back to the part that we've written to the
                 * file. For simplicity (since this is a rare event, only
                 * happens when the window size changes) go for the simplest
                 * local hack here that allows to leave the rest of the code
                 * intact, that is, read back the new partial last block to
                 * the write cache. */
                gsize offset_aligned = ALIGN_BOA(offset);
                if (G_UNLIKELY (!_vte_boa_read (stream->boa, offset_aligned, stream->wbuf))) {
                        /* what now? */
                        memset(stream->wbuf, 0, VTE_BOA_BLOCKSIZE);
                }

                if (stream->rbuf_offset >= offset_aligned) {
                        stream->rbuf_offset = 1;  /* Invalidate */
                }
        }
        stream->wbuf_len = MOD_BOA(offset);
	stream->head = offset;
}

static void
_vte_file_stream_advance_tail (VteStream *astream, gsize offset)
{
	VteFileStream *stream = (VteFileStream *) astream;

        g_assert_cmpuint (offset, >=, stream->tail);
        g_assert_cmpuint (offset, <=, stream->head);

        if (ALIGN_BOA(offset) > ALIGN_BOA(stream->tail))
                _vte_boa_advance_tail (stream->boa, ALIGN_BOA(offset));

        stream->tail = offset;
}

static gsize
_vte_file_stream_tail (VteStream *astream)
{
	VteFileStream *stream = (VteFileStream *) astream;

	return stream->tail;
}

static gsize
_vte_file_stream_head (VteStream *astream)
{
	VteFileStream *stream = (VteFileStream *) astream;

	return stream->head;
}

static void
_vte_file_stream_class_init (VteFileStreamClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = _vte_file_stream_finalize;

	klass->reset = _vte_file_stream_reset;
	klass->read = _vte_file_stream_read;
	klass->append = _vte_file_stream_append;
	klass->truncate = _vte_file_stream_truncate;
	klass->advance_tail = _vte_file_stream_advance_tail;
	klass->tail = _vte_file_stream_tail;
	klass->head = _vte_file_stream_head;
}

/******************************************************************************************/

#ifdef VTESTREAM_MAIN

/* Some helpers. Macros rather than functions to report useful line numbers on failure. */

/* Check for the file's exact contents */
#define assert_file(__fd, __contents) do { \
        char __buf[100]; \
        ssize_t __filesize = pread(__fd, __buf, 100, 0); \
        g_assert_cmpuint (__filesize, ==, strlen(__contents)); \
        g_assert (memcmp(__buf, __contents, strlen(__contents)) == 0); \
} while (0)

/* Check for the snake's state, tail, head and contents */
#define assert_snake(__snake, __state, __tail, __head, __contents) do { \
        char __buf[VTE_SNAKE_BLOCKSIZE]; \
        int __i; \
        g_assert_cmpuint (__snake->state, ==, __state); \
        g_assert_cmpuint (__snake->tail, ==, __tail); \
        g_assert_cmpuint (__snake->head, ==, __head); \
        g_assert_cmpuint (strlen(__contents), ==, __head - __tail); \
        for (__i = __tail; __i < __head; __i += VTE_SNAKE_BLOCKSIZE) { \
                g_assert (_vte_snake_read (__snake, __i, __buf)); \
                g_assert (memcmp(__buf, __contents + __i - __tail, VTE_SNAKE_BLOCKSIZE) == 0); \
        } \
} while (0)

/* Check for the boa's tail, head and contents */
#define assert_boa(__boa, __tail, __head, __contents) do { \
        char __buf[VTE_BOA_BLOCKSIZE]; \
        int __i; \
        g_assert_cmpuint (boa->tail, ==, __tail); \
        g_assert_cmpuint (boa->head, ==, __head); \
        g_assert_cmpuint (strlen(__contents), ==, __head - __tail); \
        for (__i = __tail; __i < __head; __i += VTE_BOA_BLOCKSIZE) { \
                g_assert (_vte_boa_read (boa, __i, __buf)); \
                g_assert (memcmp(__buf, __contents + __i - __tail, VTE_BOA_BLOCKSIZE) == 0); \
        } \
} while (0)

/* Check for the stream's tail, head and contents */
#define assert_stream(__astream, __tail, __head, __contents) do { \
        char __buf[100]; \
        g_assert_cmpuint (_vte_stream_tail (__astream), ==, __tail); \
        g_assert_cmpuint (_vte_stream_head (__astream), ==, __head); \
        g_assert_cmpuint (strlen(__contents), ==, __head - __tail); \
        g_assert (_vte_stream_read (__astream, __tail, __buf, __head - __tail)); \
        g_assert (memcmp(__buf, __contents, __head - __tail) == 0); \
} while (0)

/* Test the fake encryption/decryption and compression/decompression routines.
 * It usually doesn't make too much sense to test something that's part of the test infrastructure,
 * but if anything goes wrong we'd better catch it here rather than in the way more complicated tests. */
static void
test_fakes (void)
{
        char buf[100], buf2[100];
        VteBoa *boa = g_object_new (VTE_TYPE_BOA, NULL);

        /* Compress, but becomes bigger */
        strcpy(buf, "abcdef");
        g_assert_cmpuint(_vte_boa_compress (buf2, 100, buf, 6), ==, 7);
        g_assert(strncmp (buf2, "1abcdef", 7) == 0);

        /* Uncompress */
        strcpy(buf, "1abcdef");
        g_assert_cmpuint(_vte_boa_uncompress (buf2, 100, buf, 7), ==, 6);
        g_assert(strncmp (buf2, "abcdef", 6) == 0);

        /* Compress, becomes smaller */
        strcpy(buf, "www");
        g_assert_cmpuint(_vte_boa_compress (buf2, 100, buf, 3), ==, 2);
        g_assert(strncmp (buf2, "3w", 2) == 0);

        /* Uncompress */
        strcpy(buf, "3w");
        g_assert_cmpuint(_vte_boa_uncompress (buf2, 100, buf, 2), ==, 3);
        g_assert(strncmp (buf2, "www", 3) == 0);

        /* Compress, remains the same size */
        strcpy(buf, "zebraaa");
        g_assert_cmpuint(_vte_boa_compress (buf2, 100, buf, 7), ==, 7);
        g_assert(strncmp (buf2, "1zebr3a", 7) == 0);

        /* Uncompress */
        strcpy(buf, "1zebr3a");
        g_assert_cmpuint(_vte_boa_uncompress (buf2, 100, buf, 7), ==, 7);
        g_assert(strncmp (buf2, "zebraaa", 7) == 0);

        /* Trying to uncompress the original does *not* give back the same contents.
         * This will be important below. */
        strcpy(buf, "zebraaa");
        g_assert_cmpuint(_vte_boa_uncompress (buf2, 100, buf, 7), ==, 0);

        g_object_unref (boa);
}

#define snake_write(snake, offset, str) _vte_snake_write((snake), (offset), (str), strlen(str))

static void
test_snake (void)
{
        VteSnake *snake = g_object_new (VTE_TYPE_SNAKE, NULL);

        /* Test overwriting data */
        snake_write (snake, 0, "Antelope");
        assert_snake (snake, 1, 0, 8, "Antelope");

        snake_write (snake, 8, "Bobcat");
        assert_file (snake->fd, "AntelopeBobcat..");
        assert_snake (snake, 1, 0, 16, "AntelopeBobcat..");

        snake_write (snake, 8, "Camel");
        assert_file (snake->fd, "AntelopeCamel...");
        assert_snake (snake, 1, 0, 16, "AntelopeCamel...");

        snake_write (snake, 0, "Duck");
        assert_file (snake->fd, "Duck....Camel...");
        assert_snake (snake, 1, 0, 16, "Duck....Camel...");

        snake_write (snake, 16, "");
        assert_file (snake->fd, "Duck....Camel...........");
        assert_snake (snake, 1, 0, 24, "Duck....Camel...........");

        snake_write (snake, 24, "Ferret");
        assert_file (snake->fd, "Duck....Camel...........Ferret..");
        assert_snake (snake, 1, 0, 32, "Duck....Camel...........Ferret..");

        /* Reset */
        _vte_snake_reset (snake, 0);
        assert_snake (snake, 1, 0, 0, "");

        /* State 1 */
        snake_write (snake, 0, "Antelope");
        snake_write (snake, 8, "Bobcat");
        assert_file (snake->fd, "AntelopeBobcat..");
        assert_snake (snake, 1, 0, 16, "AntelopeBobcat..");

        /* Stay in state 1 */
        _vte_snake_advance_tail (snake, 8);
        snake_write (snake, 16, "Camel");
        assert_file (snake->fd, "........Bobcat..Camel...");
        assert_snake (snake, 1, 8, 24, "Bobcat..Camel...");

        /* State 1 -> 2 */
        _vte_snake_advance_tail (snake, 16);
        snake_write (snake, 24, "Duck");
        assert_file (snake->fd, "Duck............Camel...");
        assert_snake (snake, 2, 16, 32, "Camel...Duck....");

        /* Stay in state 2 */
        snake_write (snake, 32, "Elephant");
        assert_file (snake->fd, "Duck....ElephantCamel...");
        assert_snake (snake, 2, 16, 40, "Camel...Duck....Elephant");

        /* State 2 -> 3 */
        snake_write (snake, 40, "Ferret");
        assert_file (snake->fd, "Duck....ElephantCamel...Ferret..");
        assert_snake (snake, 3, 16, 48, "Camel...Duck....ElephantFerret..");

        /* State 3 -> 4 */
        _vte_snake_advance_tail (snake, 24);
        assert_file (snake->fd, "Duck....Elephant........Ferret..");
        assert_snake (snake, 4, 24, 48, "Duck....ElephantFerret..");

        /* Stay in state 4 */
        _vte_snake_advance_tail (snake, 32);
        assert_file (snake->fd, "........Elephant........Ferret..");
        assert_snake (snake, 4, 32, 48, "ElephantFerret..");

        /* State 4 -> 1 */
        _vte_snake_advance_tail (snake, 40);
        assert_file (snake->fd, "........................Ferret..");
        assert_snake (snake, 1, 40, 48, "Ferret..");

        /* State 1 -> 2 */
        snake_write (snake, 48, "Giraffe");
        assert_file (snake->fd, "Giraffe.................Ferret..");
        assert_snake (snake, 2, 40, 56, "Ferret..Giraffe.");

        /* Reset, back to state 1 */
        _vte_snake_reset (snake, 200);
        assert_snake (snake, 1, 200, 200, "");

        /* Stay in state 1 */
        snake_write (snake, 200, "Zebra");
        assert_file (snake->fd, "Zebra...");
        assert_snake (snake, 1, 200, 208, "Zebra...");

        g_object_unref (snake);
}

static void
test_boa (void)
{
        VteBoa *boa = g_object_new (VTE_TYPE_BOA, NULL);
        VteSnake *snake = (VteSnake *) &boa->parent;

        /* State 1 */
        _vte_boa_write (boa, 0, "axolotl");
        _vte_boa_write (boa, 7, "beeeeee");
        assert_file (snake->fd, "\007axolotl" "\0041b6e...");
        assert_snake (snake, 1, 0, 16, "\007axolotl" "\0041b6e...");
        assert_boa (boa, 0, 14, "axolotl" "beeeeee");

        /* Test overwrites */
        _vte_boa_write (boa, 7, "buffalo");
        assert_file (snake->fd, "\007axolotl" "\007buffalo");
        assert_snake (snake, 1, 0, 16, "\007axolotl" "\007buffalo");
        assert_boa (boa, 0, 14, "axolotl" "buffalo");

        _vte_boa_write (boa, 7, "beeeeee");
        assert_file (snake->fd, "\007axolotl" "\0041b6e...");
        assert_snake (snake, 1, 0, 16, "\007axolotl" "\0041b6e...");
        assert_boa (boa, 0, 14, "axolotl" "beeeeee");

        _vte_boa_write (boa, 0, "axolotl");
        assert_file (snake->fd, "\007axolotl" "\0041b6e...");
        assert_snake (snake, 1, 0, 16, "\007axolotl" "\0041b6e...");
        assert_boa (boa, 0, 14, "axolotl" "beeeeee");

        /* Stay in state 1 */
        _vte_boa_advance_tail (boa, 7);
        _vte_boa_write (boa, 14, "cheetah");
        assert_file (snake->fd, "........" "\0041b6e..." "\007cheetah");
        assert_snake (snake, 1, 8, 24, "\0041b6e..." "\007cheetah");
        assert_boa (boa, 7, 21, "beeeeee" "cheetah");

        /* State 1 -> 2 */
        _vte_boa_advance_tail (boa, 14);
        _vte_boa_write (boa, 21, "deeeeer");
        assert_file (snake->fd, "\0061d5e1r." "........" "\007cheetah");
        assert_snake (snake, 2, 16, 32, "\007cheetah" "\0061d5e1r.");
        assert_boa (boa, 14, 28, "cheetah" "deeeeer");

        /* Skip some state changes that we tested in test_snake() */

        /* Reset, back to state 1 */
        _vte_boa_reset (boa, 175);
        assert_snake (snake, 1, 200, 200, "");
        assert_boa (boa, 175, 175, "");

        /* Stay in state 1.
         * Test handling a string that compresses exactly to its original,
         * length, making sure that the uncompressed version is stored.
         * It was tested above that trying to decompress the uncompressed
         * version wouldn't work, so with this test we can be sure that we
         * don't try to decompress.
         */
        _vte_boa_write (boa, 175, "zebraaa");
        assert_file (snake->fd, "\007zebraaa");
        assert_snake (snake, 1, 200, 208, "\007zebraaa");
        assert_boa (boa, 175, 182, "zebraaa");

        g_object_unref (boa);
}

#define stream_append(as, str) _vte_stream_append((as), (str), strlen(str))

static void
test_stream (void)
{
        VteBoa *boa;
        VteSnake *snake;
        char buf[8];

        VteStream *astream = _vte_file_stream_new();
        VteFileStream *stream = (VteFileStream *) astream;
        _vte_file_stream_init (stream);
        boa = stream->boa;
        snake = (VteSnake *) &boa->parent;

        /* Append */
        stream_append (astream, "axolot");
        g_assert (snake->fd == -1);
        assert_boa (boa, 0, 0, "");
        assert_stream (astream, 0, 6, "axolot");

        stream_append (astream, "l");
        assert_file (snake->fd, "\007axolotl");
        assert_snake (snake, 1, 0, 8, "\007axolotl");
        assert_boa (boa, 0, 7, "axolotl");
        assert_stream (astream, 0, 7, "axolotl");

        stream_append (astream, "beeee");
        assert_file (snake->fd, "\007axolotl");
        assert_snake (snake, 1, 0, 8, "\007axolotl");
        assert_boa (boa, 0, 7, "axolotl");
        assert_stream (astream, 0, 12, "axolotl" "beeee");

        stream_append (astream, "es" "cat");
        assert_file (snake->fd, "\007axolotl" "\0061b5e1s.");
        assert_snake (snake, 1, 0, 16, "\007axolotl" "\0061b5e1s.");
        assert_boa (boa, 0, 14, "axolotl" "beeeees");
        assert_stream (astream, 0, 17, "axolotl" "beeeees" "cat");

        /* Truncate */
        _vte_stream_truncate (astream, 14);
        assert_file (snake->fd, "\007axolotl" "\0061b5e1s.");
        assert_snake (snake, 1, 0, 16, "\007axolotl" "\0061b5e1s.");
        assert_boa (boa, 0, 14, "axolotl" "beeeees");
        assert_stream (astream, 0, 14, "axolotl" "beeeees");

        _vte_stream_truncate (astream, 10);
        assert_file (snake->fd, "\007axolotl" "\0061b5e1s.");
        assert_snake (snake, 1, 0, 16, "\007axolotl" "\0061b5e1s.");
        assert_boa (boa, 0, 14, "axolotl" "beeeees");
        assert_stream (astream, 0, 10, "axolotl" "bee");

        /* Increase overwrite counter, overwrite with shorter block */
        stream_append (astream, "eeee" "cat");
        assert_file (snake->fd, "\007axolotl" "\0041b6e...");
        assert_snake (snake, 1, 0, 16, "\007axolotl" "\0041b6e...");
        assert_boa (boa, 0, 14, "axolotlbeeeeee");
        assert_stream (astream, 0, 17, "axolotl" "beeeeee" "cat");

        /* Test that the read cache is invalidated on truncate */
        _vte_stream_read (astream, 12, buf, 2);
        g_assert_cmpuint (stream->rbuf_offset, ==, 7);
        _vte_stream_truncate (astream, 13);
        g_assert_cmpuint (stream->rbuf_offset, ==, 1);
        stream_append (astream, "z" "cat");
        _vte_stream_read (astream, 12, buf, 2);
        g_assert_cmpuint (stream->rbuf_offset, ==, 7);
        buf[2] = '\0';
        g_assert_cmpstr (buf, ==, "ez");
        assert_file (snake->fd, "\007axolotl" "\0061b5e1z.");
        assert_snake (snake, 1, 0, 16, "\007axolotl" "\0061b5e1z.");
        assert_boa (boa, 0, 14, "axolotl" "beeeeez");
        assert_stream (astream, 0, 17, "axolotl" "beeeeez" "cat");

        /* Truncate again */
        _vte_stream_truncate (astream, 10);
        assert_file (snake->fd, "\007axolotl" "\0061b5e1z.");
        assert_snake (snake, 1, 0, 16, "\007axolotl" "\0061b5e1z.");
        assert_boa (boa, 0, 14, "axolotl" "beeeeez");
        assert_stream (astream, 0, 10, "axolotl" "bee");

        /* Advance_tail */
        _vte_stream_advance_tail (astream, 6);
        assert_file (snake->fd, "\007axolotl" "\0061b5e1z.");
        assert_snake (snake, 1, 0, 16, "\007axolotl" "\0061b5e1z.");
        assert_boa (boa, 0, 14, "axolotl" "beeeeez");
        assert_stream (astream, 6, 10, "l" "bee");

        _vte_stream_advance_tail (astream, 7);
        assert_file (snake->fd, "........" "\0061b5e1z.");
        assert_snake (snake, 1, 8, 16, "\0061b5e1z.");
        assert_boa (boa, 7, 14, "beeeeez");
        assert_stream (astream, 7, 10, "bee");

        /* Tail and head within the same block in the stream,
         * but not in underlying layers (due to a previous truncate).
         * Nothing special. */
        _vte_stream_advance_tail (astream, 9);
        assert_file (snake->fd, "........" "\0061b5e1z.");
        assert_snake (snake, 1, 8, 16, "\0061b5e1z.");
        assert_boa (boa, 7, 14, "beeeeez");
        assert_stream (astream, 9, 10, "e");

        /* Tail reaches head. Still nothing special. */
        _vte_stream_advance_tail (astream, 10);
        assert_file (snake->fd, "........" "\0061b5e1z.");
        assert_snake (snake, 1, 8, 16, "\0061b5e1z.");
        assert_boa (boa, 7, 14, "beeeeez");
        assert_stream (astream, 10, 10, "");

        /* Snake state 2 */
        stream_append (astream, "eeee" "catfish");
        _vte_stream_advance_tail (astream, 15);
        stream_append (astream, "dolphin" "echi");
        assert_file (snake->fd, "\007dolphin" "........" "\007catfish");
        assert_snake (snake, 2, 16, 32, "\007catfish" "\007dolphin");
        assert_boa (boa, 14, 28, "catfish" "dolphin");
        assert_stream (astream, 15, 32, "atfish" "dolphin" "echi");

        /* Tail and head within the same block.
         * The snake resets itself to state 1, ...
         * (Note: despite advance_tail, "ec" is still there in the write buffer) */
        _vte_stream_advance_tail (astream, 30);
        assert_snake (snake, 1, 32, 32, "");
        assert_boa (boa, 28, 28, "");
        assert_stream (astream, 30, 32, "hi");

        /* ... and the next write goes to beginning of the file */
        stream_append (astream, "dna");
        assert_file (snake->fd, "\007echidna");
        assert_snake (snake, 1, 32, 40, "\007echidna");
        assert_boa (boa, 28, 35, "echidna");
        assert_stream (astream, 30, 35, "hidna");

        /* Test a bit what happens when "accidentally" writing aligned blocks. */
        _vte_stream_advance_tail (astream, 35);
        stream_append (astream, "flicker" "grizzly");
        assert_file (snake->fd, "\007flicker" "\007grizzly");
        assert_snake (snake, 1, 40, 56, "\007flicker" "\007grizzly");
        assert_boa (boa, 35, 49, "flicker" "grizzly");
        assert_stream (astream, 35, 49, "flicker" "grizzly");

        stream_append (astream, "hamster");
        assert_file (snake->fd, "\007flicker" "\007grizzly" "\007hamster");
        assert_snake (snake, 1, 40, 64, "\007flicker" "\007grizzly" "\007hamster");
        assert_boa (boa, 35, 56, "flicker" "grizzly" "hamster");
        assert_stream (astream, 35, 56, "flicker" "grizzly" "hamster");

        _vte_stream_advance_tail (astream, 49);
        assert_file (snake->fd, "........" "........" "\007hamster");
        assert_snake (snake, 1, 56, 64, "\007hamster");
        assert_boa (boa, 49, 56, "hamster");
        assert_stream (astream, 49, 56, "hamster");

        /* State 2 */
        stream_append (astream, "ibexxxx");
        assert_file (snake->fd, "\0061ibe4x." "........" "\007hamster");
        assert_snake (snake, 2, 56, 72, "\007hamster" "\0061ibe4x.");
        assert_boa (boa, 49, 63, "hamster" "ibexxxx");
        assert_stream (astream, 49, 63, "hamster" "ibexxxx");

        stream_append (astream, "jjjjjjj");
        assert_file (snake->fd, "\0061ibe4x." "\0027j....." "\007hamster");
        assert_snake (snake, 2, 56, 80, "\007hamster" "\0061ibe4x." "\0027j.....");
        assert_boa (boa, 49, 70, "hamster" "ibexxxx" "jjjjjjj");
        assert_stream (astream, 49, 70, "hamster" "ibexxxx" "jjjjjjj");

        /* State 3 */
        stream_append (astream, "karakul");
        assert_file (snake->fd, "\0061ibe4x." "\0027j....." "\007hamster" "\007karakul");
        assert_snake (snake, 3, 56, 88, "\007hamster" "\0061ibe4x." "\0027j....." "\007karakul");
        assert_boa (boa, 49, 77, "hamster" "ibexxxx" "jjjjjjj" "karakul");
        assert_stream (astream, 49, 77, "hamster" "ibexxxx" "jjjjjjj" "karakul");

        /* State 4 */
        _vte_stream_advance_tail (astream, 56);
        assert_file (snake->fd, "\0061ibe4x." "\0027j....." "........" "\007karakul");
        assert_snake (snake, 4, 64, 88, "\0061ibe4x." "\0027j....." "\007karakul");
        assert_boa (boa, 56, 77, "ibexxxx" "jjjjjjj" "karakul");
        assert_stream (astream, 56, 77, "ibexxxx" "jjjjjjj" "karakul");

        stream_append (astream, "llllama");
        assert_file (snake->fd, "\0061ibe4x." "\0027j....." "........" "\007karakul" "\0064l1ama.");
        assert_snake (snake, 4, 64, 96, "\0061ibe4x." "\0027j....." "\007karakul" "\0064l1ama.");
        assert_boa (boa, 56, 84, "ibexxxx" "jjjjjjj" "karakul" "llllama");
        assert_stream (astream, 56, 84, "ibexxxx" "jjjjjjj" "karakul" "llllama");

        /* Explicit reset to the middle of a poor meerkat */
        _vte_stream_reset (astream, 88);
        stream_append (astream, "kat");
        /* Unused leading blocks are filled with dashes */
        assert_file (snake->fd, "\0064-1kat.");
        assert_snake (snake, 1, 96, 104, "\0064-1kat.");
        assert_boa (boa, 84, 91, "----kat");
        assert_stream (astream, 88, 91, "kat");

        /* Explicit reset to a block boundary */
        _vte_stream_reset (astream, 175);
        stream_append (astream, "zebraaa");
        assert_file (snake->fd, "\007zebraaa");
        assert_snake (snake, 1, 200, 208, "\007zebraaa");
        assert_boa (boa, 175, 182, "zebraaa");
        assert_stream (astream, 175, 182, "zebraaa");

        g_object_unref (astream);
}

int
main (int argc, char **argv)
{
        test_fakes();

        test_snake();
        test_boa();
        test_stream();

        printf("vtestream-file tests passed :)\n");
        return 0;
}

#endif /* VTESTREAM_MAIN */
