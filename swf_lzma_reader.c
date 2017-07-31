
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>

#include "sliding_window.h"
#include "swf.h"
#include "swf_lzma_reader.h"

int swf_reader_init_lzma(swf_reader *swf) {
	lzma_end(&swf->lzma);
	memset(&swf->lzma,0,sizeof(swf->lzma));
	if (lzma_alone_decoder(&swf->lzma,UINT64_MAX) != LZMA_OK) {
		fprintf(stderr,"LZMA failed to init\n");
		return -1;
	}

	swf->eof = 0;
	swf->zlib_eof = 0;
	swf->seek = swf_reader_lzma_seek;
	swf->read = swf_reader_lzma_read;
	swf->current_pos = swf->raw_current_pos;
	swf_reader_raw_seek(swf,8); /* put raw pointer back to just after the header */
	sliding_window_v4_empty(swf->decom_buf); /* reset buffer */
	sliding_window_v4_empty(swf->out_buf);
	return 0;
}

int swf_reader_lzma_seek(void *io,uint64_t pos) {
	swf_reader *r = (swf_reader*)io;
	unsigned char temp[512];
	int rd;

	/* if seeking backward we only allow going back to the header */
	if (pos < r->current_pos) {
		if (pos > 8) {
			fprintf(stderr,"LZMA: Seeking backwards beyond the initial header not supported\n");
			return -1;
		}
		else {
			/* reset LZMA decoder */
			if (swf_reader_init_lzma(r)) {
				fprintf(stderr,"Failed to reset LZMA decoder\n");
				return -1;
			}

			r->current_pos = pos;
		}
	}

	/* read until the position */
	while (r->current_pos < pos) {
		uint64_t go = pos - r->current_pos;

		if (go > (uint64_t)sizeof(temp)) go = (uint64_t)sizeof(temp);
		rd = swf_reader_lzma_read(r,temp,(size_t)go);
		if (rd <= 0) return -1;
	}

	return 0;
}

int swf_reader_lzma_read(void *io,void *buf,int len) {
	unsigned char *cbuf = (unsigned char*)buf;
	swf_reader *r = (swf_reader*)io;
	int rd,accum=0;
	size_t cc;

	/* pass back the first 8 bytes uncompressed */
	while (len > 0 && r->current_pos < 8) {
		if (swf_reader_raw_seek((swf_reader*)io,r->current_pos)) return -1;
		cc = (size_t)((uint64_t)8 - r->current_pos);
		if (cc > (size_t)len) cc = (size_t)len;
		r->current_pos = r->raw_current_pos;
		rd = swf_reader_raw_read((swf_reader*)io,cbuf,(int)cc);

		/* need to patch first byte at offset 0 to "FWS" instead of "CWS" */
		if (r->current_pos == 0 && rd > 0) cbuf[0] = 'F';

		r->current_pos = r->raw_current_pos;
		if (rd < (int)cc) return rd;
		accum += rd;
		cbuf += rd;
		len -= rd;
	}

	while (len > 0) {
		assert(sliding_window_v4_is_sane(r->decom_buf));
		assert(sliding_window_v4_is_sane(r->out_buf));
		sliding_window_v4_lazy_flush(r->out_buf);
		sliding_window_v4_lazy_flush(r->decom_buf);
		if ((cc=sliding_window_v4_data_available(r->out_buf)) > 0) {
			if (cc > (size_t)len) cc = (size_t)len;
			memcpy(cbuf,r->out_buf->data,cc);
			r->out_buf->data += cc;
			r->current_pos += cc;
			accum += cc;
			cbuf += cc;
			len -= cc;

			assert(sliding_window_v4_is_sane(r->out_buf));
		}
		else {
			if (r->zlib_eof) {
				r->eof = 1;
				break;
			}

			assert(sliding_window_v4_is_sane(r->decom_buf));

			/* and then we need to translate Adobe's LZMA header format to the one
			 * the LZMA library expects... because apparently Adobe wanted to make
			 * an incompatible variant or something? */
			if (r->raw_current_pos == 8) {
				/* LZMA header format:
				 *
				 * bytes 0-4: LZMA properties
				 * bytes 5-12: Uncompressed length
				 * bytes 13+: compressed data
				 *
				 * Adobe SWF format:
				 *
				 * bytes 0-3: ZWS header
				 * bytes 4-7: Uncompressed length
				 * bytes 8-11: Compressed length
				 * bytes 12-16: LZMA properties
				 * bytes 17+: compressed data */
				sliding_window_v4_empty(r->decom_buf);
				assert(r->decom_buf->data == r->decom_buf->buffer);
				if (swf_reader_raw_seek(r,0)) return -1;
				if ((cc=sliding_window_v4_can_write(r->decom_buf)) >= 16) {
					rd = swf_reader_raw_read((swf_reader*)io,r->decom_buf->end,cc - 16);
					if (rd > 0) r->decom_buf->end += rd;
					assert(sliding_window_v4_is_sane(r->decom_buf));
					sliding_window_v4_lazy_flush(r->decom_buf);

					/* sigh... alright, now we need to modify the buffer to apply this transformation */
					/* SWF 12-16 LZMA props -> LZMA 0-4 props */
					memmove(r->decom_buf->data+4,r->decom_buf->data+12,5);
					/* dummy uncompressed length LZMA 5-12 */
					memset(r->decom_buf->data+4+5,0xFF,8);
					/* move data ptr forward 4 bytes. our storing LZMA (5) + len (8)
					 * becomes 5+8 = 13 bytes to compressed data */
					r->decom_buf->data += 4;
				}
				else {
					fprintf(stderr,"LZMA: Unable to head check\n");
					return -1;
				}
			}
			else {
				if ((cc=sliding_window_v4_can_write(r->decom_buf)) > 0) {
					rd = swf_reader_raw_read((swf_reader*)io,r->decom_buf->end,cc);
					if (rd > 0) r->decom_buf->end += rd;
					assert(sliding_window_v4_is_sane(r->decom_buf));
					sliding_window_v4_lazy_flush(r->decom_buf);
				}
			}

			r->lzma.next_in = r->decom_buf->data;
			r->lzma.avail_in = sliding_window_v4_data_available(r->decom_buf);
			r->lzma.next_out = r->out_buf->end;
			r->lzma.avail_out = sliding_window_v4_can_write(r->out_buf);
			rd = lzma_code(&r->lzma,LZMA_RUN);
			if (rd == LZMA_STREAM_END) {
				r->zlib_eof = 1;
			}
			else if (rd != LZMA_OK) {
				fprintf(stderr,"LZMA error %d\n",rd);
				r->zlib_eof = 1;
				break;
			}
			r->decom_buf->data = (unsigned char*)r->lzma.next_in;
			r->out_buf->end = r->lzma.next_out;
			assert(sliding_window_v4_is_sane(r->decom_buf));
		}
	}

	return accum;
}

