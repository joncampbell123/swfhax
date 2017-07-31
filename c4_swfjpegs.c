
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>

#include "swf.h"

#include <zlib.h>

static const char*			swf_file = NULL;

static unsigned char*			JPEGTables = NULL;
static size_t				JPEGTables_size = 0;

static void do_definebits_jpeg3(swf_reader *swf,swf_UI16 id,swf_UI32 len) {
	sliding_window_v4 *buf;
	FILE *fp=NULL;
	int jpeg=0;
	int rd;

	if (len < 8) return;
	buf = sliding_window_v4_create_buffer(32768);
	if (buf == NULL) return;
	sliding_window_v4_empty(buf);
	rd = sliding_window_v4_can_write(buf);
	if ((swf_UI32)rd > len) rd = len;
	rd = swf_reader_read(swf,buf->data,rd);
	if (rd < 8) goto end;
	buf->end += rd;
	len -= rd;

	/* it can be JPEG, PNG, or GIF. We have to look at the first 8 bytes */
	{
		char tmp[100];

		if (!memcmp(buf->data,"\x89\x50\x4E\x47\x0D\x0A\x1A\x0A",8))
			sprintf(tmp,"swf.jpeg.%u.png",id);
		else if (!memcmp(buf->data,"GIF89a",6))
			sprintf(tmp,"swf.jpeg.%u.gif",id);
		else {
			sprintf(tmp,"swf.jpeg.%u.jpg",id);
			jpeg = 1;
		}

		fp = fopen(tmp,"wb");
		if (fp == NULL) {
			buf = sliding_window_v4_destroy(buf);
			return;
		}
	}

	/* Macromedia Flash prior to 8 stuck in an extra SOI/EOI for some reason. Discard */
	if (!memcmp(buf->data,"\xFF\xD8\xFF\xD9",4)) buf->data += 4;
	if (!memcmp(buf->data,"\xFF\xD9\xFF\xD8",4)) buf->data += 4;

	/* write out the rest of the buffer */
	rd = (int)sliding_window_v4_data_available(buf);
	if (rd > 0) fwrite(buf->data,(size_t)rd,1,fp);

	/* and then copy the rest of the file */
	do {
		sliding_window_v4_empty(buf);
		rd = sliding_window_v4_can_write(buf);
		if ((swf_UI32)rd > len) rd = len;
		rd = swf_reader_read(swf,buf->data,rd);
		if (rd == 0) break;
		fwrite(buf->data,(size_t)rd,1,fp);
		len -= rd;
	} while (1);

	sliding_window_v4_empty(buf);
	if (fp) fclose(fp); fp = NULL;

	/* and then... the JPEG alpha channel */
	if (jpeg) {
		unsigned char out[4096];
		char tmp[100];
		z_stream z;

		sprintf(tmp,"swf.jpeg.%u.jpg.alpha.data",id);
		fp = fopen(tmp,"wb");
		if (fp == NULL) goto end;

		rd = sliding_window_v4_can_write(buf);
		rd = swf_reader_read(swf,buf->end,rd);
		if (rd > 0) buf->end += rd;
		memset(&z,0,sizeof(z));
		z.next_in = buf->data;
		z.avail_in = sliding_window_v4_data_available(buf);
		z.next_out = out;
		z.avail_out = sizeof(out);
		if (inflateInit(&z) == Z_OK) {
			int eof=0;

			do {
				sliding_window_v4_lazy_flush(buf);
				rd = sliding_window_v4_can_write(buf);
				if (rd > 0) {
					rd = swf_reader_read(swf,buf->end,rd);
					if (rd > 0) buf->end += rd;
					else eof=1;
				}

				z.next_in = buf->data;
				z.avail_in = sliding_window_v4_data_available(buf);
				z.next_out = out;
				z.avail_out = sizeof(out);
				rd = inflate(&z,Z_NO_FLUSH);
				if (rd == Z_STREAM_END) eof=1;
				buf->data = z.next_in;
				if (z.next_out == out && eof) break;
				fwrite(out,(size_t)(z.next_out-out),1,fp);
				if (rd != Z_OK) break;
			} while (1);

			inflateEnd(&z);
		}
	}

end:	buf = sliding_window_v4_destroy(buf);
	if (fp) fclose(fp); fp = NULL;
}

static void do_definebits_jpeg2(swf_reader *swf,swf_UI16 id) {
	sliding_window_v4 *buf;
	FILE *fp=NULL;
	int rd;

	buf = sliding_window_v4_create_buffer(32768);
	if (buf == NULL) return;
	sliding_window_v4_empty(buf);
	rd = swf_reader_read(swf,buf->data,sliding_window_v4_can_write(buf));
	if (rd < 8) goto end;
	buf->end += rd;

	/* it can be JPEG, PNG, or GIF. We have to look at the first 8 bytes */
	{
		char tmp[100];

		if (!memcmp(buf->data,"\x89\x50\x4E\x47\x0D\x0A\x1A\x0A",8))
			sprintf(tmp,"swf.jpeg.%u.png",id);
		else if (!memcmp(buf->data,"GIF89a",6))
			sprintf(tmp,"swf.jpeg.%u.gif",id);
		else
			sprintf(tmp,"swf.jpeg.%u.jpg",id);

		fp = fopen(tmp,"wb");
		if (fp == NULL) {
			buf = sliding_window_v4_destroy(buf);
			return;
		}
	}

	/* Macromedia Flash prior to 8 stuck in an extra SOI/EOI for some reason. Discard */
	if (!memcmp(buf->data,"\xFF\xD8\xFF\xD9",4)) buf->data += 4;
	if (!memcmp(buf->data,"\xFF\xD9\xFF\xD8",4)) buf->data += 4;

	/* write out the rest of the buffer */
	rd = (int)sliding_window_v4_data_available(buf);
	if (rd > 0) fwrite(buf->data,(size_t)rd,1,fp);

	/* and then copy the rest of the file */
	do {
		sliding_window_v4_empty(buf);
		rd = swf_reader_read(swf,buf->data,sliding_window_v4_can_write(buf));
		if (rd == 0) break;
		fwrite(buf->data,(size_t)rd,1,fp);
	} while (1);

end:	buf = sliding_window_v4_destroy(buf);
	if (fp) fclose(fp);
}

static void do_definebits(swf_reader *swf,swf_UI16 id) {
	sliding_window_v4 *buf;
	unsigned char *s;
	FILE *fp;
	int rd;

	if (JPEGTables == NULL || JPEGTables_size == 0) {
		/* Flash 8+ compatibility where JPEGTables tags are zero length */
		do_definebits_jpeg2(swf,id);
		return;
	}

	buf = sliding_window_v4_create_buffer(32768);
	if (buf == NULL) return;

	{
		char tmp[100];

		sprintf(tmp,"swf.jpeg.%u.jpg",id);
		fp = fopen(tmp,"wb");
		if (fp == NULL) {
			buf = sliding_window_v4_destroy(buf);
			return;
		}
	}

	sliding_window_v4_empty(buf);
	rd = swf_reader_read(swf,buf->data,sliding_window_v4_can_write(buf));
	if (rd < 8) goto end;
	buf->end += rd;

	/* OK. We have to do two things here:
	 *
	 *    1) Skip the extra 0xFF 0xD9 0xFF 0xD8 that Macromedia Flash prior to version 8
	 *       inserted into the stream for some reason
	 *    2) Scan and copy up to the Start of Frame marker.
	 *       At that point, before continuing to copy the stream, we need to take the
	 *       JPEGTables and splice it into the stream to make a valid whole JPEG file */
	if (!memcmp(buf->data,"\xFF\xD8\xFF\xD9",4)) buf->data += 4;
	else if (!memcmp(buf->data,"\xFF\xD9\xFF\xD8",4)) buf->data += 4;
	s = buf->data;
	while ((s+2) <= buf->end) {
		if (*s == 0xFF) {
			if (s[1] == 0xC0 || s[1] == 0xC2) {
				/* Stop here. It's the Start of Frame */
				break;
			}
			else if (s[1] == 0xE0) { /* APP0 marker. speed scan by reading length field */
				unsigned int l = __be_u16(s+2);
				s += 2 + l; if (s > buf->end) s = buf->end;
			}
			else {
				s += 2;
			}
		}
		else {
			s++;
		}
	}

	/* write what we found */
	fwrite(buf->data,(size_t)(s-buf->data),1,fp);
	buf->data = s;

	/* now splice in the JPEGTables. Note the JPEGTables might have SOI and EOI
	 * around the quant tables, so we have to check for and remove those */
	{
		unsigned char *s = JPEGTables;
		unsigned char *e = JPEGTables + JPEGTables_size;

		if (s[0] == 0xFF && s[1] == 0xD8) s += 2;
		if ((s+2) <= e && e[-2] == 0xFF && e[-1] == 0xD9) e -= 2;
		if (s < e) fwrite(s,(size_t)(e-s),1,fp);
	}

	/* write out the rest of the buffer */
	rd = (int)sliding_window_v4_data_available(buf);
	if (rd > 0) fwrite(buf->data,(size_t)rd,1,fp);

	/* and then copy the rest of the file */
	do {
		sliding_window_v4_empty(buf);
		rd = swf_reader_read(swf,buf->data,sliding_window_v4_can_write(buf));
		if (rd == 0) break;
		fwrite(buf->data,(size_t)rd,1,fp);
	} while (1);

end:	buf = sliding_window_v4_destroy(buf);
	fclose(fp);
}

static void help() {
	fprintf(stderr,"c4_swfjpegs [options] <file>\n");
}

static int parse_argv(int argc,char **argv) {
	int i,swi=0;
	char *a;

	for (i=1;i < argc;) {
		a = argv[i++];

		if (*a == '-') {
			do { a++; } while (*a == '-');

			if (!strcmp(a,"h") || !strcmp(a,"help")) {
				help();
				return 1;
			}
			else {
				fprintf(stderr,"Unknown switch %s\n",a);
				return 1;
			}
		}
		else {
			switch (swi++) {
				case 0:
					swf_file = a;
					break;
				default:
					fprintf(stderr,"Unexpected non-switch %s\n",a);
					return 1;
			}
		}
	}

	if (swf_file == NULL) {
		fprintf(stderr,"You must specify a SWF file to dump\n");
		return 1;
	}

	return 0;
}

int main(int argc,char **argv) {
	swf_reader *swf;
	swf_tag tag;
	int fd;

	if (parse_argv(argc,argv))
		return 1;

	fd = open(swf_file,O_RDONLY);
	if (fd < 0) {
		fprintf(stderr,"Unable to open file %s\n",argv[1]);
		return 1;
	}
	if ((swf=swf_reader_create()) == NULL) {
		fprintf(stderr,"Cannot create SWF reader\n");
		return 1;
	}
	if (swf_reader_assign_fd(swf,fd)) {
		fprintf(stderr,"Cannot assign fd\n");
		return 1;
	}
	swf_reader_assign_fd_ownership(swf);
	if (swf_reader_get_file_header(swf)) {
		fprintf(stderr,"Unable to read file header.\n");
		return 1;
	}

	while (swf_reader_read_record(swf,&tag)) {
		/* set read stop, so we can read the bitfields */
		swf->read_stop = swf->next_tag;

		switch (tag.record.TagCode) {
			case swftag_JPEGTables:
				/* NTS: Flash 8 and later will apparently emit Flash movies with
				 *      JPEGTables tags of zero length, followed by DefineBits
				 *      tags that contain a whole JPEG. Ooookay then... */
				if (JPEGTables == NULL) {
					JPEGTables_size = tag.record.Length;
					if (JPEGTables_size != 0) {
						JPEGTables = malloc(JPEGTables_size);
						if (JPEGTables) swf_reader_read(swf,JPEGTables,JPEGTables_size);
					}
				}
				else {
					fprintf(stderr,"WARNING: JPEGTables already defined\n");
				}

				break;
			case swftag_DefineBits: {
				swf_UI16 CharacterID;

				CharacterID = swf_reader_read_UI16(swf);
				do_definebits(swf,CharacterID);
				} break;
			case swftag_DefineBitsJPEG2: {
				swf_UI16 CharacterID;

				CharacterID = swf_reader_read_UI16(swf);
				do_definebits_jpeg2(swf,CharacterID);
				} break;
			case swftag_DefineBitsJPEG3: {
				swf_UI32 AlphaDataOffset;
				swf_UI16 CharacterID;

				CharacterID = swf_reader_read_UI16(swf);
				AlphaDataOffset = swf_reader_read_UI32(swf);
				do_definebits_jpeg3(swf,CharacterID,AlphaDataOffset);
				} break;
		}

		assert(swf->current_pos <= swf->read_stop);

		/* clear read stop */
		swf->read_stop = 0;
	}

	swf = swf_reader_destroy(swf);
	return 0;
}

