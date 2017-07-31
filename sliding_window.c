/* TODO:
 *    - Linux 64-bit compile test               [ ] Run
 *    - Windows 32-bit compile test             [ ] Run
 *    - Windows 64-bit compile test             [ ] Run
 *    - Mac OS X 32-bit compile test            [ ] Run
 *    - Mac OS X 64-bit compile test            [ ] Run
 */

/* Sliding window generic buffer management.
 * Useful for stream parsing functions like AC-3 or MPEG audio decoding
 * in an efficient manner.
 *
 * Impact Studio Pro utilities - Text processors
 * (C) 2008-2010 Impact Studio Pro ALL RIGHTS RESERVED.
 * Written by Jonathan Campbell
 *
 */

#define _POSIX_C_SOURCE 200112L

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include "sliding_window.h"

/* FIXME: Used ONCE in this code, replace with open coding and remove this */
/* macro to detect pointer overflow cases.
 * code can use this to increase security by detecting whether the sum of
 * a memory location and a length would overflow the datatype and wrap
 * back around from the beginning of memory, potentially referring to
 * or corrupting unrelated data there. */
static inline size_t _size_t_overflow(const size_t a,const size_t b) { return ((~((size_t)0)) - a) < b; }
#define size_t_overflow(a,b) _size_t_overflow((const size_t)(a),(const size_t)(b))

static size_t SYSTEM_PAGE_SIZE = 0;

static void SYSTEM_PAGE_SIZE_init(void) {
    if (SYSTEM_PAGE_SIZE == 0)
        SYSTEM_PAGE_SIZE = sysconf(_SC_PAGESIZE);
}

sliding_window_v4 *sliding_window_v4_set_custom_buffer(sliding_window_v4 *sw,size_t size,const unsigned char *data) {
	/* sanity check: the buffer pointer is NULL */
	assert(sw->buffer == NULL);
	/* sanity check: caller isn't trying to alloc a buffer on top of a buffer or mmap() window, it must be a null window.
	 * if it is a buffer, then it must be a buffer we don't own (meaning: previously called with set_custom_buffer) */
	assert(sw->type == SLIDING_WINDOW_V4_TYPE_NULL || (sw->type == SLIDING_WINDOW_V4_TYPE_BUFFER && !sw->u.buffer.lib_owner));
	/* OK, zero the struct and prepare it for buffer management */
	memset(sw,0,sizeof(*sw));
	if (size < 2) size = 2;
	sw->buffer = (unsigned char*)data;
	sw->type = SLIDING_WINDOW_V4_TYPE_BUFFER;
	sw->u.buffer.lib_owner = 0; /* this library does not own the buffer */
	sw->fence = sw->buffer + size;
	sw->data = sw->end = sw->buffer;
	return sw;
}

sliding_window_v4 *sliding_window_v4_alloc_buffer(sliding_window_v4 *sw,size_t size) {
	/* sanity check: the buffer pointer is NULL */
	assert(sw->buffer == NULL);
	/* sanity check: caller isn't trying to alloc a buffer on top of a buffer or mmap() window, it must be a null window */
	assert(sw->type == SLIDING_WINDOW_V4_TYPE_NULL);
	/* OK, zero the struct and prepare it for buffer management */
	memset(sw,0,sizeof(*sw));
	if (size < 2) size = 2;
	sw->buffer = (unsigned char*)malloc(size);
	if (!sw->buffer) return NULL;
	sw->type = SLIDING_WINDOW_V4_TYPE_BUFFER;
	sw->u.buffer.lib_owner = 1; /* this library owns the buffer */
	sw->fence = sw->buffer + size;
	sw->data = sw->end = sw->buffer;
	return sw;
}

sliding_window_v4* sliding_window_v4_create_null() {
	sliding_window_v4 *sw = (sliding_window_v4*)malloc(sizeof(sliding_window_v4));
	if (!sw) return NULL;
	memset(sw,0,sizeof(*sw));
	return sw;
}

/* [doc] sliding_window_create
 *
 * Allocate and create a sliding window
 *
 * Parameters:
 *
 *    size = allocation length
 *
 * Return value:
 *
 *    allocated sliding window
 * 
 */
sliding_window_v4* sliding_window_v4_create_buffer(size_t size) {
	sliding_window_v4 *sw = sliding_window_v4_create_null();
	if (!sw) return NULL;
	if (sliding_window_v4_alloc_buffer(sw,size) == NULL) {
		free(sw);
		return NULL;
	}
	return sw;
}

/* [doc] sliding_window_destroy
 *
 * Destroy and free a sliding window
 *
 * Parameters:
 *
 *    sw = sliding window
 *
 * Return value:
 *
 *    NULL
 * 
 */
sliding_window_v4* sliding_window_v4_destroy(sliding_window_v4 *sw) {
	if (sw) {
		sliding_window_v4_free(sw);
		memset(sw,0,sizeof(*sw));
		free(sw);
	}
	return NULL;
}

int sliding_window_v4_resize_buffer(sliding_window_v4 *sw,size_t new_size) {
	unsigned char *np;

	assert(sw->type == SLIDING_WINDOW_V4_TYPE_BUFFER);

	if (new_size < 2) new_size = 2;

	/* don't resize if we're resizing to the same size buffer */
	if (sw->buffer+new_size == sw->fence) return 1;

	/* check for pointer location overflow */
	if (size_t_overflow(sw->buffer,new_size)) return 0;

	/* don't allow resizing if doing so would leave the data & end pointers outside the buffer (violating window sanity) */
	if (sw->data > (sw->buffer+new_size)) return 0;
	if (sw->end > (sw->buffer+new_size)) return 0;

	/* carry out the resize. manage pointers so that if realloc() fails we can gracefully return the struct unmodified */
	if ((np = (unsigned char*)realloc(sw->buffer,new_size)) == NULL) return 0;

	/* throw away the old pointer and update the structure */
	{
		size_t old_do = (size_t)(sw->data - sw->buffer);
		size_t old_eo = (size_t)(sw->end - sw->buffer);
		sw->buffer = np;
		sw->fence = np + new_size;
		sw->data = np + old_do;
		sw->end = np + old_eo;
	}

	return 1;
}

/* [doc] sliding_window_resize
 *
 * Resize a sliding window's allocation length
 *
 * Parameters:
 *
 *    sw = sliding window
 *    new_size = new allocation length
 *
 * Return value:
 *
 *    1 if successful
 *    0 if not
 * 
 */
int sliding_window_v4_resize(sliding_window_v4 *sw,size_t new_size) {
	if (sw == NULL)
		return 0;
	else if (sw->type == SLIDING_WINDOW_V4_TYPE_BUFFER)
		return sliding_window_v4_resize_buffer(sw,new_size);

	return 0;
}

/* [doc] sliding_window_data_advance
 *
 * Advance the data pointer by the specified number of bytes
 *    (sw->data += bytes)
 *
 * Parameters:
 *
 *    sw = sliding window
 *    bytes = amount of data
 *
 * Return value:
 *
 *    bytes advanced. this will be less than the caller requested if the
 *    function knows that the data pointer would have surpassed the end
 *    pointer, creating an invalid condition
 * 
 */
size_t sliding_window_v4_data_advance(sliding_window_v4 *sw,size_t bytes) {
	if (sw->buffer == NULL)
		return 0;

	/* don't let the caller advance too far and violate the pointer rule specified in the header file */
	if ((sw->data+bytes) > sw->end)
		bytes = (size_t)(sw->end - sw->data);

	sw->data += bytes;
	return bytes;
}

/* [doc] sliding_window_wrote
 *
 * Advance the end pointer, usually after adding new data to sw->end
 *
 * Parameters:
 *
 *    sw = sliding window
 *    bytes = amount of data written
 *
 * Return value:
 *
 *    amount of data acknowledged. this can be less if the function knows that
 *    actually writing that much data can overrun the window and possibly
 *    corrupt memory. it is recommended that your code abort() or check the
 *    return value in an assert() to make sure you do not overrun the window.
 * 
 */
size_t sliding_window_v4_wrote(sliding_window_v4 *sw,size_t bytes) {
	if (sw->buffer == NULL)
		return 0;

	/* to clarify: the caller loads data into memory at sw->end and uses this function to
	 * tell us HOW much data it wrote so the sliding window can follow along */
	if ((sw->end+bytes) > sw->fence)
		bytes = (size_t)(sw->fence - sw->end);

	sw->end += bytes;
	return bytes;
}

/* [doc] sliding_window_flush
 *
 * Flush the window, discarding old data and moving the valid data region back
 * to the beginning of the window. This also makes more room for the addition
 * of new data. If the valid data is already at the beginning of the window,
 * the function will do nothing.
 *
 * Parameters:
 *
 *    sw = sliding window
 *
 * Return value:
 *
 *    number of bytes prior to data pointer discarded (for your accounting)
 *
 * Warning:
 *
 *    Do not over-use this function. Do not flush after every operation. The
 *    flush operation involves copying data which when overused will cause your
 *    program's performance to suffer terribly. Instead, try to develop your
 *    program such that this function is called only when you are low on data
 *    or at least halfway through the window.
 * 
 */
size_t sliding_window_v4_flush(sliding_window_v4 *sw) {
	size_t valid_data,ret=0;

    SYSTEM_PAGE_SIZE_init();

	/* copy the valid data left back to the front of the buffer, reset data/end pointers. */
	/* NOTICE: used properly this allows fast reading and parsing of streams. over-used, and
	 * performance will suffer horribly. call this function only when you need to. */

	/* if already there, do nothing */
	if (sw->data == sw->buffer) return 0;

	if (sw->type == SLIDING_WINDOW_V4_TYPE_BUFFER) {
		ret = (size_t)(sw->data - sw->buffer);
		valid_data = sliding_window_v4_data_available(sw);
		if (valid_data > 0) memmove(sw->buffer,sw->data,valid_data);
		sw->data = sw->buffer;
		sw->end = sw->data + valid_data;
	}

	return ret;
}

/* [doc] sliding_window_lazy_flush
 *
 * Lazily flush the window. If the data pointer is more than halfway across the
 * window, it will call sliding_window_flush. Else, it will do nothing.
 * See the Warnings for sliding_window_flush() on why such a function is useful
 *
 * Parameters:
 *
 *    sw = sliding window
 *
 * Return value:
 *
 *    1 if successful
 *
 */
size_t sliding_window_v4_lazy_flush(sliding_window_v4 *sw) {
	/* lazy flush: call sliding_window_flush() only if more than half the entire buffer has
	 * been consumed. a caller that wants a generally-optimal streaming buffer policy would
	 * call this instead of duplicating code to check and call all over the place. */
	size_t threshhold = ((size_t)(sw->fence - sw->buffer)) >> 1;
	if ((sw->data+threshhold) >= sw->end && sliding_window_v4_data_offset(sw) >= (threshhold/2))
		return sliding_window_v4_flush(sw);

	return 0;
}

/* [doc] sliding_window_refill_from_socket
 *
 * Read data from a socket and add it to the sliding window. This is intended
 * for programs that need to read and buffer streaming media over a network
 * connection.
 *
 * Parameters:
 *
 *    sw = sliding window
 *    fd = (Linux/unix) file descriptor of socket
 *    fd = (Windows) WinSock handle of socket
 *    max = maximum data to add from socket. if 0, then function will use the allocation length as maximum
 *
 * Return value:
 *
 *    > 0 if data was added to the window
 *    0 if no data was added
 *    -1 if the connection was lost
 *
 */
ssize_t sliding_window_v4_refill_from_socket(sliding_window_v4 *sw,int fd,size_t max) {
	size_t cw;
	ssize_t rd;
	if (sw->type != SLIDING_WINDOW_V4_TYPE_BUFFER) return (ssize_t)(-1); /* <- TODO */
	/* NTS: As of 2013/02/07 we no longer lazy-flush for the caller */
	if (fd < 0) return (ssize_t)(-1);
	cw = sliding_window_v4_can_write(sw);
	if (max == 0) max = sliding_window_v4_alloc_length(sw);
	if (max > cw) max = cw;
	if (max == 0) return 0;
	rd = recv(fd,sw->end,max,MSG_DONTWAIT);
	if (rd == 0 || (rd < 0 && errno == EPIPE)) { /* whoops. disconnect */ return (ssize_t)(-1); }
	if (rd < 0) return 0;
	if ((size_t)rd > max) rd = max;
	sw->end += rd;
	return rd;
}

/* [doc] sliding_window_empty_to_socket
 *
 * Take data from the valid data region and send out the socket, if possible.
 * What the function actually sends out the socket is marked off by advancing
 * the data pointer to the next data to send. This is intended for programs
 * that need to stream data out over a network connection.
 *
 * Parameters:
 *
 *    sw = sliding window
 *    fd = (Linux/unix) file descriptor of socket
 *    fd = (Windows) WinSock handle of socket
 *    max = maximum data to send from socket. if 0, then function will use the allocation length as maximum
 *
 * Return value:
 *
 *    > 0 if data was sent
 *    0 if no data was sent
 *    -1 if the connection was lost
 *
 */
ssize_t sliding_window_v4_empty_to_socket(sliding_window_v4 *sw,int fd,size_t max) {
	size_t cr = sliding_window_v4_data_available(sw);
	ssize_t wd;
	if (sw->type != SLIDING_WINDOW_V4_TYPE_BUFFER) return (ssize_t)(-1); /* <- TODO */
	if (fd < 0) return 0;
	if (max == 0) max = cr;
	else if (cr > max) cr = max;
	if (cr == 0) return 0;
	wd = send(fd,sw->data,cr,MSG_DONTWAIT|MSG_NOSIGNAL);
	if (wd == 0 || (wd < 0 && errno == EPIPE)) { /* whoops. disconnect */ return (ssize_t)(-1); }
	if (wd < 0) return 0;
	if ((size_t)wd > cr) wd = cr;
	sw->data += wd;
	return wd;
}

/* [doc] sliding_window_refill_from_fd
 *
 * Read data from a file and add it to the sliding window. This is intended
 * for programs that need to parse data streams from a file.
 *
 * Parameters:
 *
 *    sw = sliding window
 *    fd = file descriptor
 *    max = maximum data to add from file. if 0, then function will use the allocation length as maximum
 *
 * Return value:
 *
 *    > 0 if data was added to the window
 *    0 if no data was added
 *    -1 if file error
 *
 */
ssize_t sliding_window_v4_refill_from_fd(sliding_window_v4 *sw,int fd,size_t max) {
	size_t cw;
	ssize_t rd;
	if (sw->type != SLIDING_WINDOW_V4_TYPE_BUFFER) return (ssize_t)(-1); /* <- TODO */
	/* NTS: As of 2013/02/07 we no longer lazy-flush for the caller */
	if (fd < 0) return 0;
	cw = sliding_window_v4_can_write(sw);
	if (max == 0) max = sliding_window_v4_alloc_length(sw);
	if (max > cw) max = cw;
	if (max == 0) return 0;
	rd = read(fd,sw->end,max);
	if (rd == 0 || (rd < 0 && errno == EPIPE)) { /* whoops. disconnect */ return (ssize_t)(-1); }
	if (rd < 0) return 0;
	if ((size_t)rd > max) rd = max;
	sw->end += rd;
	return rd;
}

/* [doc] sliding_window_empty_to_fd
 *
 * Take data from the valid data region and write it to the file, if possible.
 * What the function actually writes is marked off by advancing the data
 * pointer to the next data to send.
 *
 * Parameters:
 *
 *    sw = sliding window
 *    fd = file descriptor
 *    max = maximum data to write to file. if 0, then function will use the allocation length as maximum
 *
 * Return value:
 *
 *    > 0 if data was written
 *    0 if no data was written
 *    -1 if file error
 *
 */
ssize_t sliding_window_v4_empty_to_fd(sliding_window_v4 *sw,int fd,size_t max) {
	size_t cr = sliding_window_v4_data_available(sw);
	ssize_t wd;
	if (sw->type != SLIDING_WINDOW_V4_TYPE_BUFFER) return (ssize_t)(-1); /* <- TODO */
	if (fd < 0) return 0;
	if (max == 0) max = cr;
	else if (cr > max) cr = max;
	if (cr == 0) return 0;
	wd = write(fd,sw->data,cr);
	if (wd == 0 || (wd < 0 && errno == EPIPE)) { /* whoops. disconnect */ return (ssize_t)(-1); }
	if (wd < 0) return 0;
	if ((size_t)wd > cr) wd = cr;
	sw->data += wd;
	return wd;
}

void sliding_window_v4_free_buffer(sliding_window_v4 *sw) {
	if (sw->buffer != NULL) {
		if (sw->u.buffer.lib_owner) {
			free(sw->buffer); /* we own the buffer, we free it */
			sw->buffer = NULL;
		}
		else if (sw->u.buffer.owner_free_buffer != NULL) {
			/* the buffer is owned by someone else, and that someone else
			 * gave us a callback to free it by. */
			sw->u.buffer.owner_free_buffer(sw);
			sw->buffer = NULL;
		}
		else {
			/* no way to free the buffer. hope the owner intended it that
			 * way, or else a memory leak may happen */
		}
	}
}

void sliding_window_v4_free(sliding_window_v4 *sw) {
	if (sw->type == SLIDING_WINDOW_V4_TYPE_BUFFER)
		sliding_window_v4_free_buffer(sw);

	sw->buffer = sw->fence = sw->data = sw->end = NULL;
}

int sliding_window_v4_is_sane(sliding_window_v4 *sw) {
	if (sw == NULL) return 0;

	/* if any of the pointers are NULL, */
	if (sw->buffer == NULL || sw->data == NULL || sw->fence == NULL || sw->end == NULL) {
		/* if it's a memory buffer: NO, the pointers must not be null */
		if (sw->type == SLIDING_WINDOW_V4_TYPE_BUFFER)
			return 0;

		/* if it's not, then for the window to be valid all four pointers must be NULL.
		 * We enforce consistency here: either they are all non-NULL or they are all NULL. */
		if (sw->buffer != NULL || sw->data != NULL || sw->fence != NULL || sw->end != NULL)
			return 0;

		/* it's valid, then */
		return 1;
	}

	return	(sw->buffer <= sw->data) &&
		(sw->data <= sw->end) &&
		(sw->end <= sw->fence) &&
		(sw->buffer != sw->fence);
}

int sliding_window_v4_safe_strtoui(sliding_window_v4 *w,unsigned int *ret,int base) {
	unsigned char *scan,digit;

	if (sliding_window_v4_data_available(w) == 0) return 0;

	*ret = 0;
	scan = w->data;
	if (base == 0) {
		/* assume decimal */
		base = 10;

		/* starts with 0, then it's octal */
		if (*(w->data) == '0') {
			base = 8;
			scan++;
		}
		else {
			if (sliding_window_v4_data_available(w) < 2) return 0;
			if (!memcmp(w->data,"0x",2)) {
				scan += 2;
				base = 16;
			}
		}
	}

	do {
		if (scan >= w->end) return 0;

		if (*scan >= '0' && *scan <= '9')
			digit = *scan - '0';
		else if (*scan >= 'a' && *scan <= 'f')
			digit = *scan + 10 - 'a';
		else if (*scan >= 'A' && *scan <= 'F')
			digit = *scan + 10 - 'A';
		else
			break;

		if (digit >= (unsigned char)base)
			break;

		/* TODO: Overflow detect */
		*ret = (*ret * (unsigned int)base) + (unsigned int)digit;
		scan++;
	} while (1);

	w->data = scan;
	return 1;
}

