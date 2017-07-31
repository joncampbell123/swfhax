
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
#include "swf_zlib_reader.h"
#include "swf_lzma_reader.h"

const uint16_t swf_SoundRatesUB2[4] = {
	5512, 11025, 22050, 44100
};

const char *swf_SoundCompressionUB4[16] = {
	"Uncompressed, native endian",		/* index=0, swf v1+ */
	"ADPCM",				/* index=1, swf v1+ */
	"MP3",					/* index=2, swf v4+ */
	"Uncompressed, little endian",		/* index=3, swf v4+ */
	"Nellymoser 16 kHz",			/* index=4, swf v10+ */
	"Nellymoser 8 kHz",			/* index=5, swf v10+ */
	"Nellymoser",				/* index=6, swf v6+ */
	"",					/* 7 */
	"",					/* 8 */
	"",					/* 9 */
	"",					/* 10 */
	"Speex",				/* index=11, swf v10+ */
	"",					/* 12 */
	"",					/* 13 */
	"",					/* 14 */
	""					/* 15 */
};

int swf_reader_stock_raw_seek(void *io,uint64_t pos) {
	swf_reader *swf = (swf_reader*)io;

	if (swf == NULL) return -1;
	if (swf->file_fd < 0) return -1;
	if (lseek(swf->file_fd,(off_t)pos,SEEK_SET) != (off_t)pos) return -1;
	swf->raw_current_pos = pos;
	return 0;
}

const char *swf_TagCode_to_str(unsigned int tag) {
	switch (tag) {
		case /*0*/ swftag_End:			return "End";
		case /*1*/ swftag_ShowFrame:		return "ShowFrame";
		case /*2*/ swftag_DefineShape:		return "DefineShape";

		case /*4*/ swftag_PlaceObject:		return "PlaceObject";
		case /*5*/ swftag_RemoveObject:		return "RemoveObject";
		case /*6*/ swftag_DefineBits:		return "DefineBits";
		case /*7*/ swftag_DefineButton:		return "DefineButton";
		case /*8*/ swftag_JPEGTables:		return "JPEGTables";
		case /*9*/ swftag_SetBackgroundColor:	return "SetBackgroundColor";
		case /*10*/swftag_DefineFont:		return "DefineFont";
		case /*11*/swftag_DefineText:		return "DefineText";
		case /*12*/swftag_DoAction:		return "DoAction";
		case /*13*/swftag_DefineFontInfo:	return "DefineFontInfo";
		case /*14*/swftag_DefineSound:		return "DefineSound";
		case /*15*/swftag_StartSound:		return "StartSound";

		case /*17*/swftag_DefineButtonSound:	return "DefineButtonSound";
		case /*18*/swftag_SoundStreamHead:	return "SoundStreamHead";
		case /*19*/swftag_SoundStreamBlock:	return "SoundStreamBlock";
		case /*20*/swftag_DefineBitsLossless:	return "DefineBitsLossless";
		case /*21*/swftag_DefineBitsJPEG2:	return "DefineBitsJPEG2";
		case /*22*/swftag_DefineShape2:		return "DefineShape2";
		case /*23*/swftag_DefineButtonCxform:	return "DefineButtonCxform";
		case /*24*/swftag_Protect:		return "Protect";

		case /*26*/swftag_PlaceObject2:		return "PlaceObject2";

		case /*28*/swftag_RemoveObject2:	return "RemoveObject2";

		case /*32*/swftag_DefineShape3:		return "DefineShape3";
		case /*33*/swftag_DefineText2:		return "DefineText2";
		case /*34*/swftag_DefineButton2:	return "DefineButton2";
		case /*35*/swftag_DefineBitsJPEG3:	return "DefineBitsJPEG3";
		case /*36*/swftag_DefineBitsLossless2:	return "DefineBitsLossless2";
		case /*37*/swftag_DefineEditText:	return "DefineEditText";

		case /*39*/swftag_DefineSprite:		return "DefineSprite";

		case /*43*/swftag_FrameLabel:		return "FrameLabel";

		case /*45*/swftag_SoundStreamHead2:	return "SoundStreamHead2";
		case /*46*/swftag_DefineMorphShape:	return "DefineMorphShape";

		case /*48*/swftag_DefineFont2:		return "DefineFont2";

		case /*56*/swftag_ExportAssets:		return "ExportAssets";
		case /*57*/swftag_ImportAssets:		return "ImportAssets";
		case /*58*/swftag_EnableDebugger:	return "EnableDebugger";
		case /*59*/swftag_DoInitAction:		return "DoInitAction";
		case /*60*/swftag_DefineVideoStream:	return "DefineVideoStream";
		case /*61*/swftag_VideoFrame:		return "VideoFrame";
		case /*62*/swftag_DefineFontInfo2:	return "DefineFontInfo2";

		case /*64*/swftag_EnableDebugger2:	return "EnableDebugger2";
		case /*65*/swftag_ScriptLimits:		return "ScriptLimits";
		case /*66*/swftag_SetTabIndex:		return "SetTabIndex";

		case /*69*/swftag_FileAttributes:	return "FileAttributes";
		case /*70*/swftag_PlaceObject3:		return "PlaceObject3";

		case /*73*/swftag_DefineFontAlignZones:	return "DefineFontAlignZones";
		case /*74*/swftag_CSMTextSettings:	return "CSMTextSettings";
		case /*75*/swftag_DefineFont3:		return "DefineFont3";
		case /*76*/swftag_SymbolClass:		return "SymbolClass";
		case /*77*/swftag_Metadata:		return "Metadata";

		case /*82*/swftag_DoABC:		return "DoABC";
		case /*83*/swftag_DefineShape4:		return "DefineShape4";

		case /*86*/swftag_DefineSceneAndFrameLabelData: return "DefineSceneAndFrameLabelData";
		case /*87*/swftag_DefineBinaryData:	return "DefineBinaryData";
		case /*88*/swftag_DefineFontName:	return "DefineFontName";
	};

	return "?";
}

int swf_reader_raw_seek(swf_reader *r,uint64_t pos) {
	if (r == NULL) return -1;
	if (r->raw_seek == NULL) return -1;
	return r->raw_seek(r,pos);
}

int swf_reader_seek(swf_reader *r,uint64_t pos) {
	if (r == NULL) return -1;
	if (r->seek == NULL) return -1;
	r->eof = 0;
	return r->seek(r,pos);
}

int swf_reader_read(swf_reader *r,void *buf,int len) {
	int rd;

	if (r == NULL) return -1;
	if (r->read == NULL) return -1;
	if (r->read_stop != (uint64_t)0ULL) {
		if (r->current_pos >= r->read_stop) return 0;
		if ((r->current_pos+(uint64_t)len) > r->read_stop)
			len = (int)(r->read_stop - r->current_pos);
	}

	rd = r->read(r,buf,len);
	if (rd < len) r->eof = 1;
	return rd;
}

void swf_reader_read_bytealign(swf_reader *swf) {
	swf->bit_buffer_bits = 0;
}

swf_UI8 swf_reader_read_UI8(swf_reader *swf) {
	unsigned char c = 0x00;
	swf_reader_read_bytealign(swf);
	swf_reader_read(swf,&c,1);
	return (swf_UI8)c;
}

void swf_reader_read_RGB(swf_reader *swf,swf_RGB *rgb) {
	rgb->Red = swf_reader_read_UI8(swf);
	rgb->Green = swf_reader_read_UI8(swf);
	rgb->Blue = swf_reader_read_UI8(swf);
}

void swf_reader_read_RGBA(swf_reader *swf,swf_RGBA *rgb) {
	rgb->Red = swf_reader_read_UI8(swf);
	rgb->Green = swf_reader_read_UI8(swf);
	rgb->Blue = swf_reader_read_UI8(swf);
	rgb->Alpha = swf_reader_read_UI8(swf);
}

void swf_reader_read_RGB_as_RGBA(swf_reader *swf,swf_RGBA *rgb) {
	rgb->Red = swf_reader_read_UI8(swf);
	rgb->Green = swf_reader_read_UI8(swf);
	rgb->Blue = swf_reader_read_UI8(swf);
	rgb->Alpha = 0xFF;
}

swf_UI32 swf_reader_read_UB(swf_reader *swf,int bits) {
	swf_UI32 r = 0;
	int avail;

	if (bits <= 0 || bits > 32) return 0;

	while (bits > 0) {
		if (swf->bit_buffer_bits == 0) {
			swf->bit_buffer = swf_reader_read_UI8(swf);
			swf->bit_buffer_bits = 8;
		}

		avail = swf->bit_buffer_bits;
		if (avail > bits) avail = bits;
		swf->bit_buffer_bits -= avail;
		r <<= (swf_UI32)avail;
		r |= (((swf_UI32)swf->bit_buffer >> (swf_UI32)swf->bit_buffer_bits)) &
			(((swf_UI32)1 << (swf_UI32)avail) - 1);
		bits -= avail;
	}

	return r;
}

swf_SI32 swf_reader_read_SB(swf_reader *swf,int bits) {
	swf_UI32 r = 0;

	/* FIXME: Do we sign-extend if the field is only 1 bit long and bit is set? */
	if (bits <= 0 || bits > 32) return 0;
	r = swf_reader_read_UB(swf,bits);
	if (r & ((swf_UI32)1 << ((swf_UI32)(bits-1))))
		r |= (swf_UI32)0xFFFFFFFF << bits;

	return r;
}

void swf_reader_read_RECT(swf_reader *swf,swf_RECT *rect) {
	swf_reader_read_bytealign(swf);				/* "The RECT record must be byte aligned" */
	rect->Nbits = swf_reader_read_UB(swf,5);		/* UB[5] */
	rect->Xmin = swf_reader_read_SB(swf,rect->Nbits);	/* SB[Nbits] */
	rect->Xmax = swf_reader_read_SB(swf,rect->Nbits);	/* SB[Nbits] */
	rect->Ymin = swf_reader_read_SB(swf,rect->Nbits);	/* SB[Nbits] */
	rect->Ymax = swf_reader_read_SB(swf,rect->Nbits);	/* SB[Nbits] */
}

swf_UI16 swf_reader_read_UI16(swf_reader *swf) {
	unsigned char buf[2] = {0};
	swf_reader_read_bytealign(swf);
	swf_reader_read(swf,buf,2);
	return (swf_UI16)__le_u16(buf);
}

swf_UI32 swf_reader_read_UI32(swf_reader *swf) {
	unsigned char buf[4] = {0};
	swf_reader_read_bytealign(swf);
	swf_reader_read(swf,buf,4);
	return (swf_UI32)__le_u32(buf);
}

swf_SI8 swf_reader_read_SI8(swf_reader *swf) {
	signed char c = 0x00;
	swf_reader_read_bytealign(swf);
	swf_reader_read(swf,&c,1);
	return (swf_SI8)c;
}

swf_SI16 swf_reader_read_SI16(swf_reader *swf) {
	unsigned char buf[2] = {0};
	swf_reader_read_bytealign(swf);
	swf_reader_read(swf,buf,2);
	return (swf_SI16)__le_s16(buf);
}

swf_SI32 swf_reader_read_SI32(swf_reader *swf) {
	unsigned char buf[4] = {0};
	swf_reader_read_bytealign(swf);
	swf_reader_read(swf,buf,4);
	return (swf_SI32)__le_s32(buf);
}

int swf_reader_raw_read(swf_reader *r,void *buf,int len) {
	if (r == NULL) return -1;
	if (r->raw_read == NULL) return -1;
	return r->raw_read(r,buf,len);
}

/* ===== uncompressed file reader ===== */
int swf_reader_uncompressed_seek(void *io,uint64_t pos) {
	swf_reader *r = (swf_reader*)io;
	int rd;

	rd = swf_reader_raw_seek((swf_reader*)io,pos);
	r->current_pos = r->raw_current_pos;
	return rd;
}

int swf_reader_uncompressed_read(void *io,void *buf,int len) {
	swf_reader *r = (swf_reader*)io;
	int rd;

	rd = swf_reader_raw_read((swf_reader*)io,buf,len);
	r->current_pos = r->raw_current_pos;
	return rd;
}
/* ==================================== */

int swf_reader_stock_raw_read(void *io,void *buf,int len) {
	swf_reader *swf = (swf_reader*)io;
	int rd;

	if (swf == NULL) return -1;
	if (swf->file_fd < 0) return -1;
	if (len <= 0) return 0;
	rd = read(swf->file_fd,buf,len);
	if (rd < 0) return -1;
	swf->raw_current_pos += (uint64_t)rd;
	return rd;
}

int swf_reader_init(swf_reader *r) {
	memset(r,0,sizeof(*r));
	r->file_fd = -1;
	return 0;
}

void swf_reader_close_file(swf_reader *r) {
	if (r->zlib.next_in != NULL) {
		inflateEnd(&r->zlib);
		memset(&r->zlib,0,sizeof(r->zlib));
	}
	if (r->lzma.next_in != NULL) {
		lzma_end(&r->lzma);
		memset(&r->lzma,0,sizeof(r->lzma));
	}
	r->decom_buf = sliding_window_v4_destroy(r->decom_buf);
	r->out_buf = sliding_window_v4_destroy(r->out_buf);
	if (r->file_fd >= 0) {
		if (r->file_fd_owner) close(r->file_fd);
		r->file_fd = -1;
	}
	r->file_fd_owner = 0;
}

void swf_reader_free(swf_reader *r) {
	swf_reader_close_file(r);
}

swf_reader *swf_reader_create() {
	swf_reader *r = (swf_reader*)malloc(sizeof(swf_reader));
	if (r == NULL) return NULL;
	if (swf_reader_init(r)) {
		free(r);
		return NULL;
	}
	return r;
}

swf_reader *swf_reader_destroy(swf_reader *r) {
	if (r != NULL) {
		swf_reader_free(r);
		free(r);
	}

	return NULL;
}

void swf_reader_assign_fd_ownership(swf_reader *r) {
	if (r->file_fd >= 0) r->file_fd_owner = 1;
}

int swf_reader_read_record(swf_reader *r,swf_tag *tag) {
	swf_UI16 tmp;

	if (r->current_pos > r->next_tag) {
		if (swf_reader_seek(r,r->next_tag)) {
			fprintf(stderr,"SWF I/O error: file pointer past next tag and cannot seek to it\n");
			return 0;
		}
	}

	if (!r->eof && r->current_pos < r->next_tag) {
		if (swf_reader_seek(r,r->next_tag)) {
			r->eof = 1;
			return 0;
		}
	}
	if (r->eof) return 0;

	tag->absolute_tag_offset = r->current_pos;
	tmp = swf_reader_read_UI16(r);
	tag->record.TagCode = tmp >> 6;
	tag->record.Length = tmp & 0x3F;
	if (tag->record.Length == 0x3F) tag->record.Length = swf_reader_read_UI32(r);
	tag->absolute_offset = r->current_pos;
	r->next_tag = tag->absolute_offset + tag->record.Length;
	if (r->eof) return 0;
	return 1;
}

int swf_reader_assign_fd(swf_reader *r,int fd) {
	if (fd != r->file_fd) {
		swf_reader_close_file(r);
		r->file_fd = fd;
		r->file_fd_owner = 0;
	}

	r->raw_seek = swf_reader_stock_raw_seek;
	r->raw_read = swf_reader_stock_raw_read;
	r->seek = NULL;
	r->read = NULL;
	return 0;
}

int swf_reader_get_file_header(swf_reader *swf) {
	unsigned char tmp[16];

	if (swf == NULL) return -1;
	if (swf_reader_raw_seek(swf,0)) return -1;
	if (swf_reader_raw_read(swf,tmp,4) < 4) return -1;

	/* should be "FWS" (SWF backwards) or "CWS" or "ZWS" */
	if (tmp[1] != 'W' || tmp[2] != 'S') return -1;

	swf->seek = swf_reader_uncompressed_seek;
	swf->read = swf_reader_uncompressed_read;
	swf->current_pos = swf->raw_current_pos;

	memcpy(swf->header.Signature,tmp,3);
	swf->header.Version = tmp[3];
	swf->header.FileLength = swf_reader_read_UI32(swf);

	/* check compression */
	if (tmp[0] == 'F') {
	}
	else if (tmp[0] == 'C') {
		swf->decom_buf = sliding_window_v4_create_buffer(4096);
		if (swf->decom_buf == NULL) {
			fprintf(stderr,"Unable to init decompression buffer\n");
			return -1;
		}
		swf->out_buf = sliding_window_v4_create_buffer(16384);
		if (swf->out_buf == NULL) {
			fprintf(stderr,"Unable to init decompression buffer\n");
			return -1;
		}

		/* Init zlib decoder. Also assigns zlib reader to read/seek. */
		if (swf_reader_init_zlib(swf)) {
			fprintf(stderr,"Failed to initialize ZLIB stream decompressor\n");
			return -1;
		}
	}
	else if (tmp[0] == 'Z') {
		swf->decom_buf = sliding_window_v4_create_buffer(4096);
		if (swf->decom_buf == NULL) {
			fprintf(stderr,"Unable to init decompression buffer\n");
			return -1;
		}
		swf->out_buf = sliding_window_v4_create_buffer(16384);
		if (swf->out_buf == NULL) {
			fprintf(stderr,"Unable to init decompression buffer\n");
			return -1;
		}

		/* Init LZMA decoder. Also assigns zlib reader to read/seek. */
		if (swf_reader_init_lzma(swf)) {
			fprintf(stderr,"Failed to initialize LZMA stream decompressor\n");
			return -1;
		}
	}

	/* check version */
	if (tmp[3] == 0x00 || tmp[3] > 20) { /* Adobe's up to version 20 */
		fprintf(stderr,"Unsupported SWF version %u\n",tmp[3]);
		return -1;
	}

#if ENABLE_debug
	/* test code by re-reading header through decompressor.
	 * the decompressor is supposed to pass through the first 8 bytes unmodified */
	if (swf_reader_seek(swf,0)) {
		fprintf(stderr,"Failed to seek to 0\n");
		return -1;
	}
	if ((size_t)swf_reader_read(swf,tmp,sizeof(tmp)) != sizeof(tmp)) {
		fprintf(stderr,"Failed to re-read header\n");
		return -1;
	}
	if (swf_reader_seek(swf,8)) {
		fprintf(stderr,"Failed to seek to 8\n");
		return -1;
	}

	/* NTS: The ZLIB reader changes the "CWS" signature to "FWS" on read */
	if (memcmp(swf->header.Signature+1,tmp+1,2) != 0 || swf->header.Version != tmp[3] ||
		swf->header.FileLength != __le_u32(tmp+4)) {
		fprintf(stderr,"Re-read doesn't match\n");
		return -1;
	}
#endif

	swf_reader_read_RECT(swf,&swf->header.FrameSize);
	swf->header.FrameRate = (swf_UFIXED8)swf_reader_read_UI16(swf);
	swf->header.FrameCount = swf_reader_read_UI16(swf);
	swf->header_end = swf->current_pos;
	swf->next_tag = swf->current_pos;
	if (swf->eof) fprintf(stderr,"WARNING: Just read header, already an eof\n");
	return 0;
}

int swf_reader_read_DefineSound(swf_reader *swf,swf_DefineSound *h) {
	h->SoundId = swf_reader_read_UI16(swf);
	h->SoundFormat = swf_reader_read_UB(swf,4);
	h->SoundFormat_str = swf_SoundCompressionUB4[h->SoundFormat];
	h->SoundRate = swf_reader_read_UB(swf,2);
	h->SoundRate_value = swf_SoundRatesUB2[h->SoundRate];
	h->SoundSize = swf_reader_read_UB(swf,1);
	h->SoundSize_value = h->SoundSize ? 16 : 8;
	h->SoundType = swf_reader_read_UB(swf,1);
	h->SoundType_value = h->SoundSize ? 2 : 1;
	h->SoundSampleCount = swf_reader_read_UI32(swf);
	return 0;
}

int swf_reader_read_SoundStreamHead(swf_reader *swf,swf_SoundStreamHead *h) {
	unsigned int Reserved;

	swf_reader_read_bytealign(swf);
	h->StreamSoundCompression_str = NULL;
	Reserved = swf_reader_read_UB(swf,4);
	if (Reserved != 0) fprintf(stderr,"SWF SoundStreamHead reserved field nonzero\n");
	h->PlaybackSoundRate = swf_reader_read_UB(swf,2);
	h->PlaybackSoundRate_value = swf_SoundRatesUB2[h->PlaybackSoundRate];
	h->PlaybackSoundSize = swf_reader_read_UB(swf,1);
	h->PlaybackSoundSize_value = h->PlaybackSoundSize ? 16 : 8;
	h->PlaybackSoundType = swf_reader_read_UB(swf,1);
	h->PlaybackSoundType_value = h->PlaybackSoundType ? 2 : 1;
	h->StreamSoundCompression = swf_reader_read_UB(swf,4);
	h->StreamSoundCompression_str = swf_SoundCompressionUB4[h->StreamSoundCompression];
	h->StreamSoundRate = swf_reader_read_UB(swf,2);
	h->StreamSoundRate_value = swf_SoundRatesUB2[h->StreamSoundRate];
	h->StreamSoundSize = swf_reader_read_UB(swf,1);
	h->StreamSoundSize_value = h->StreamSoundSize ? 16 : 8;
	h->StreamSoundType = swf_reader_read_UB(swf,1);
	h->StreamSoundType_value = h->StreamSoundType ? 2 : 1;
	h->StreamSoundSampleCount = swf_reader_read_UI16(swf);
	if (h->StreamSoundCompression == 2) {
		h->LatencySeek_present = 1;
		h->LatencySeek = swf_reader_read_UI16(swf);
	}
	else {
		h->LatencySeek_present = 0;
		h->LatencySeek = 0;
	}

	return 0;
}

int swf_reader_read_FileAttributes(swf_reader *swf,swf_FileAttributes *a) {
	unsigned int __attribute__((unused)) Reserved;

	Reserved = swf_reader_read_UB(swf,1);
	a->UseDirectBlit = swf_reader_read_UB(swf,1);
	a->UseGPU = swf_reader_read_UB(swf,1);
	a->HasMetadata = swf_reader_read_UB(swf,1);
	a->ActionScript3 = swf_reader_read_UB(swf,1);
	Reserved = swf_reader_read_UB(swf,2);
	a->UseNetwork = swf_reader_read_UB(swf,1);
	Reserved = swf_reader_read_UB(swf,24);
	return 0;
}

void swf_DefineFont_init(swf_DefineFont *nfo) {
	memset(nfo,0,sizeof(*nfo));
}

void swf_DefineFont_free(swf_DefineFont *nfo) {
	if (nfo->OffsetTable != NULL) free(nfo->OffsetTable);
	memset(nfo,0,sizeof(*nfo));
}

int swf_reader_read_DefineFont_step1(swf_reader *swf,swf_DefineFont *n) {
	n->FontID = swf_reader_read_UI16(swf);
	n->absoluteOffsetTable = swf->current_pos;
	n->_OffsetTableEntry0 = swf_reader_read_UI16(swf);

	/* "Because the GlyphShapeTable immediately follows the OffsetTable, the number of
	 * entries in each table (the number of glyphs in the font) can be inferred by
	 * dividing the first entry in the OffsetTable by two". Ooookay then.. */
	n->nGlyphs = n->_OffsetTableEntry0 >> 1;
	return 0;
}

int swf_reader_read_DefineFont_alloc_OffsetTable(swf_DefineFont *nfo) {
	if (nfo->OffsetTable != NULL || nfo->nGlyphs == 0) return 0;
	nfo->OffsetTable = (swf_UI16*)malloc(sizeof(swf_UI16) * nfo->nGlyphs);
	if (nfo->OffsetTable == NULL) return -1;
	return 0;
}

int swf_reader_read_DefineFont_read_OffsetTable(swf_reader *swf,swf_DefineFont *nfo) {
	unsigned int i;

	if (nfo->OffsetTable == NULL) return (nfo->nGlyphs == 0 ? 0 : -1);
	nfo->OffsetTable[0] = nfo->_OffsetTableEntry0; /* NTS: Remember? We had to read the first one to infer nGlyphs */
	for (i=1;i < nfo->nGlyphs;i++) nfo->OffsetTable[i] = swf_reader_read_UI16(swf);
	return 0;
}

int swf_reader_read_PlaceObject(swf_reader *swf,swf_PlaceObject *r) {
	r->CharacterId = swf_reader_read_UI16(swf);
	r->Depth = swf_reader_read_UI16(swf);
	swf_reader_read_MATRIX(swf,&r->Matrix);
	/* TODO: If there is room yet in the tag, read a CXFORM into &r->ColorTransform */
	return 0;
}

int swf_reader_read_RemoveObject(swf_reader *swf,swf_RemoveObject *r) {
	r->CharacterId = swf_reader_read_UI16(swf);
	r->Depth = swf_reader_read_UI16(swf);
	return 0;
}

int swf_reader_read_DefineShape_header(swf_reader *swf,swf_DefineShape *r) {
	r->ShapeId = swf_reader_read_UI16(swf);
	swf_reader_read_RECT(swf,&r->ShapeBounds);
	return 0;
}

int swf_reader_read_DefineShape4_header(swf_reader *swf,swf_DefineShape *r) {
	r->ShapeId = swf_reader_read_UI16(swf);
	swf_reader_read_RECT(swf,&r->ShapeBounds);
	swf_reader_read_RECT(swf,&r->EdgeBounds);
	/* Reserved */swf_reader_read_UB(swf,5);
	r->UsesFillWindingRule = swf_reader_read_UB(swf,1);
	r->UsesNonScalingStrokes = swf_reader_read_UB(swf,1);
	r->UsesScalingStrokes = swf_reader_read_UB(swf,1);
	return 0;
}

int swf_reader_read_SHAPE_beginning(swf_reader *swf,swf_SHAPE_reading *r) {
	swf_reader_read_bytealign(swf);
	r->NumFillBits = swf_reader_read_UB(swf,4);
	r->NumLineBits = swf_reader_read_UB(swf,4);
	return 0;
}

int swf_reader_read_SHAPE_record(swf_reader *swf,swf_SHAPE_reading *r) {
	unsigned int tmp;

	r->ShapeRecord_str = "EndShapeRecord";
	r->ShapeRecord_type = swf_shrec_EndShapeRecord;
	r->ShapeRecordTypeFlag = swf_reader_read_UB(swf,1);
	if (r->ShapeRecordTypeFlag == 0) {
		r->shrec.style.StyleChangeRecord.StateNewStyles = swf_reader_read_UB(swf,1);
		r->shrec.style.StyleChangeRecord.StateLineStyle = swf_reader_read_UB(swf,1);
		r->shrec.style.StyleChangeRecord.StateFillStyle1 = swf_reader_read_UB(swf,1);
		r->shrec.style.StyleChangeRecord.StateFillStyle0 = swf_reader_read_UB(swf,1);
		r->shrec.style.StyleChangeRecord.StateMoveTo = swf_reader_read_UB(swf,1);
		/* EndShapeRecord TypeFlag == 0, followed by 5 bits of 0 */
		if (r->shrec.style.StyleChangeRecord.StateNewStyles == 0 &&
			r->shrec.style.StyleChangeRecord.StateLineStyle == 0 &&
			r->shrec.style.StyleChangeRecord.StateFillStyle1 == 0 &&
			r->shrec.style.StyleChangeRecord.StateFillStyle0 == 0 &&
			r->shrec.style.StyleChangeRecord.StateMoveTo == 0) {
			r->shrec.style.EndShapeRecord.EndOfShape = 0;
			return 0;
		}

		/* StyleChangeRecord TypeFlag == 0, 5 bits at least 1 is nonzero */
		r->ShapeRecord_str = "StyleChangeRecord";
		r->ShapeRecord_type = swf_shrec_StyleChangeRecord;
		if (r->shrec.style.StyleChangeRecord.StateMoveTo) {
			r->shrec.style.StyleChangeRecord.MoveBits = swf_reader_read_UB(swf,5);
			r->shrec.style.StyleChangeRecord.MoveDeltaX = swf_reader_read_SB(swf,r->shrec.style.StyleChangeRecord.MoveBits);
			r->shrec.style.StyleChangeRecord.MoveDeltaY = swf_reader_read_SB(swf,r->shrec.style.StyleChangeRecord.MoveBits);
		}
		else {
			r->shrec.style.StyleChangeRecord.MoveBits = 0;
			r->shrec.style.StyleChangeRecord.MoveDeltaX = 0;
			r->shrec.style.StyleChangeRecord.MoveDeltaY = 0;
		}
		if (r->shrec.style.StyleChangeRecord.StateFillStyle0)
			r->shrec.style.StyleChangeRecord.FillStyle0 = swf_reader_read_UB(swf,r->NumFillBits);
		if (r->shrec.style.StyleChangeRecord.StateFillStyle1)
			r->shrec.style.StyleChangeRecord.FillStyle1 = swf_reader_read_UB(swf,r->NumFillBits);
		if (r->shrec.style.StyleChangeRecord.StateLineStyle)
			r->shrec.style.StyleChangeRecord.LineStyle = swf_reader_read_UB(swf,r->NumLineBits);
	}
	else {
		tmp = swf_reader_read_UB(swf,1);
		if (tmp) { /* StraightEdgeRecord TypeFlag == 1, StraightFlag == 1, 5 bits */
			r->ShapeRecord_str = "StraightEdgeRecord";
			r->ShapeRecord_type = swf_shrec_StraightEdgeRecord;
			r->shrec.edge.StraightEdgeRecord.StraightFlag = 1;
			r->shrec.edge.StraightEdgeRecord.NumBits = swf_reader_read_UB(swf,4) + 2;
			r->shrec.edge.StraightEdgeRecord.GeneralLineFlag = swf_reader_read_UB(swf,1);
			if (r->shrec.edge.StraightEdgeRecord.GeneralLineFlag == 0)
				r->shrec.edge.StraightEdgeRecord.VertLineFlag = swf_reader_read_UB(swf,1);
			if (r->shrec.edge.StraightEdgeRecord.GeneralLineFlag || r->shrec.edge.StraightEdgeRecord.VertLineFlag == 0)
				r->shrec.edge.StraightEdgeRecord.DeltaX = swf_reader_read_SB(swf,r->shrec.edge.StraightEdgeRecord.NumBits);
			else
				r->shrec.edge.StraightEdgeRecord.DeltaX = 0;
			if (r->shrec.edge.StraightEdgeRecord.GeneralLineFlag || r->shrec.edge.StraightEdgeRecord.VertLineFlag == 1)
				r->shrec.edge.StraightEdgeRecord.DeltaY = swf_reader_read_SB(swf,r->shrec.edge.StraightEdgeRecord.NumBits);
			else
				r->shrec.edge.StraightEdgeRecord.DeltaY = 0;
		}
		else {
			r->ShapeRecord_str = "CurvedEdgeRecord";
			r->ShapeRecord_type = swf_shrec_CurvedEdgeRecord;
			r->shrec.edge.CurvedEdgeRecord.StraightFlag = 0;
			r->shrec.edge.CurvedEdgeRecord.NumBits = swf_reader_read_UB(swf,4) + 2;
			r->shrec.edge.CurvedEdgeRecord.ControlDeltaX = swf_reader_read_SB(swf,r->shrec.edge.CurvedEdgeRecord.NumBits);
			r->shrec.edge.CurvedEdgeRecord.ControlDeltaY = swf_reader_read_SB(swf,r->shrec.edge.CurvedEdgeRecord.NumBits);
			r->shrec.edge.CurvedEdgeRecord.AnchorDeltaX = swf_reader_read_SB(swf,r->shrec.edge.CurvedEdgeRecord.NumBits);
			r->shrec.edge.CurvedEdgeRecord.AnchorDeltaY = swf_reader_read_SB(swf,r->shrec.edge.CurvedEdgeRecord.NumBits);
		}
	}

	return 0;
}

int swf_reader_read_FOCALGRADIENT(swf_reader *swf,swf_GRADIENT *g,unsigned int rgba) {
	unsigned int i;

	swf_reader_read_bytealign(swf); /* NTS: Adobe documentation does not mention byte alignment requirement */
	g->SpreadMode = swf_reader_read_UB(swf,2);
	g->InterpolationMode = swf_reader_read_UB(swf,2);
	g->NumGradients = swf_reader_read_UB(swf,4);
	for (i=0;i < g->NumGradients;i++) {
		g->GradientRecords[i].Ratio = swf_reader_read_UI8(swf);
		if (rgba)	swf_reader_read_RGBA(swf,&g->GradientRecords[i].Color);
		else		swf_reader_read_RGB_as_RGBA(swf,&g->GradientRecords[i].Color);
	}
	g->FocalPoint = (swf_FIXED8)swf_reader_read_SI16(swf);;
	return 0;
}

int swf_reader_read_GRADIENT(swf_reader *swf,swf_GRADIENT *g,unsigned int rgba) {
	unsigned int i;

	swf_reader_read_bytealign(swf); /* NTS: Adobe documentation does not mention byte alignment requirement */
	g->SpreadMode = swf_reader_read_UB(swf,2);
	g->InterpolationMode = swf_reader_read_UB(swf,2);
	g->NumGradients = swf_reader_read_UB(swf,4);
	for (i=0;i < g->NumGradients;i++) {
		g->GradientRecords[i].Ratio = swf_reader_read_UI8(swf);
		if (rgba)	swf_reader_read_RGBA(swf,&g->GradientRecords[i].Color);
		else		swf_reader_read_RGB_as_RGBA(swf,&g->GradientRecords[i].Color);
	}
	g->FocalPoint = 0;
	return 0;
}

int swf_reader_read_MATRIX(swf_reader *swf,swf_MATRIX *m) {
	swf_reader_read_bytealign(swf);
	m->HasScale = swf_reader_read_UB(swf,1);
	if (m->HasScale) {
		m->NScaleBits = swf_reader_read_UB(swf,5);
		m->ScaleX = swf_reader_read_SB(swf,m->NScaleBits);
		m->ScaleY = swf_reader_read_SB(swf,m->NScaleBits);
	}
	else {
		m->ScaleX = m->ScaleY = 0x10000;
		m->NScaleBits = 0;
	}
	m->HasRotate = swf_reader_read_UB(swf,1);
	if (m->HasRotate) {
		m->NRotateBits = swf_reader_read_UB(swf,5);
		m->RotateSkew0 = swf_reader_read_SB(swf,m->NRotateBits);
		m->RotateSkew1 = swf_reader_read_SB(swf,m->NRotateBits);
	}
	else {
		m->NRotateBits = 0;
		m->RotateSkew0 = 0;
		m->RotateSkew1 = 0;
	}
	m->NTranslateBits = swf_reader_read_UB(swf,5);
	m->TranslateX = swf_reader_read_SB(swf,m->NTranslateBits);
	m->TranslateY = swf_reader_read_SB(swf,m->NTranslateBits);
	return 0;
}

int swf_reader_read_FILLSTYLEARRAY_header(swf_reader *swf,swf_FILLSTYLEARRAY_read *nfo,unsigned int tag_code) {
	nfo->color_rgba = (tag_code == swftag_DefineShape3 || tag_code == swftag_DefineShape4);
	nfo->FillStyleCount = swf_reader_read_UI8(swf);
	if (nfo->FillStyleCount == 0xFF) nfo->FillStyleCount = swf_reader_read_UI16(swf);
	return 0;
}

int swf_reader_read_LINESTYLE(swf_reader *swf,swf_LINESTYLEARRAY_read *nfo) {
	nfo->Width = swf_reader_read_UI16(swf);
	if (nfo->color_rgba) swf_reader_read_RGBA(swf,&nfo->Color);
	else swf_reader_read_RGB_as_RGBA(swf,&nfo->Color);
	return 0;
}

int swf_reader_read_LINESTYLE2(swf_reader *swf,swf_LINESTYLEARRAY_read *nfo) {
	nfo->Width = swf_reader_read_UI16(swf);
	nfo->StartCapStyle = swf_reader_read_UB(swf,2);
	nfo->JoinStyle = swf_reader_read_UB(swf,2);
	nfo->HasFillFlag = swf_reader_read_UB(swf,1);
	nfo->NoHScaleFlag = swf_reader_read_UB(swf,1);
	nfo->NoVScaleFlag = swf_reader_read_UB(swf,1);
	nfo->PixelHintingFlag = swf_reader_read_UB(swf,1);
	/* Reserved */swf_reader_read_UB(swf,5);
	nfo->NoClose = swf_reader_read_UB(swf,1);
	nfo->EndCapStyle = swf_reader_read_UB(swf,2);
	if (nfo->JoinStyle == 2) nfo->MiterLimitFactor = swf_reader_read_UI16(swf);
	else nfo->MiterLimitFactor = 0;
	if (nfo->HasFillFlag)
		return swf_reader_read_FILLSTYLE(swf,&nfo->FillType);
	else
		swf_reader_read_RGBA(swf,&nfo->Color);

	return 0;
}

int swf_reader_read_FILLSTYLE(swf_reader *swf,swf_FILLSTYLEARRAY_read *nfo) {
	nfo->FillStyleType_str = "";
	nfo->FillStyleType = swf_reader_read_UI8(swf);
	switch (nfo->FillStyleType) {
		case swf_fillstyle_solid:
			nfo->FillStyleType_str = "solid fill";
			if (nfo->color_rgba) swf_reader_read_RGBA(swf,&nfo->fsinfo.solid.Color);
			else swf_reader_read_RGB_as_RGBA(swf,&nfo->fsinfo.solid.Color);
			break;
		case swf_fillstyle_linear_gradient_fill:
			nfo->FillStyleType_str = "linear gradient fill";
			swf_reader_read_MATRIX(swf,&nfo->fsinfo.gradient.GradientMatrix);
			swf_reader_read_GRADIENT(swf,&nfo->fsinfo.gradient.Gradient,nfo->color_rgba);
			break;
		case swf_fillstyle_radial_gradient_fill:
			nfo->FillStyleType_str = "radial gradient fill";
			swf_reader_read_MATRIX(swf,&nfo->fsinfo.gradient.GradientMatrix);
			swf_reader_read_GRADIENT(swf,&nfo->fsinfo.gradient.Gradient,nfo->color_rgba);
			break;
		case swf_fillstyle_focal_radial_gradient_fill:
			nfo->FillStyleType_str = "focal radial gradient fill";
			swf_reader_read_MATRIX(swf,&nfo->fsinfo.gradient.GradientMatrix);
			swf_reader_read_FOCALGRADIENT(swf,&nfo->fsinfo.gradient.Gradient,nfo->color_rgba);
			break;
		case swf_fillstyle_repeating_bitmap_fill:
			nfo->FillStyleType_str = "repeating bitmap fill";
			nfo->fsinfo.bitmap.BitmapId = swf_reader_read_UI16(swf);
			swf_reader_read_MATRIX(swf,&nfo->fsinfo.bitmap.BitmapMatrix);
			break;
		case swf_fillstyle_clipped_bitmap_fill:
			nfo->FillStyleType_str = "clipped bitmap fill";
			nfo->fsinfo.bitmap.BitmapId = swf_reader_read_UI16(swf);
			swf_reader_read_MATRIX(swf,&nfo->fsinfo.bitmap.BitmapMatrix);
			break;
		case swf_fillstyle_non_smoothed_repeating_bitmap_fill:
			nfo->FillStyleType_str = "non-smoothed repeating bitmap fill";
			nfo->fsinfo.bitmap.BitmapId = swf_reader_read_UI16(swf);
			swf_reader_read_MATRIX(swf,&nfo->fsinfo.bitmap.BitmapMatrix);
			break;
		case swf_fillstyle_non_smoothed_clipped_bitmap_fill:
			nfo->FillStyleType_str = "non-smoothed clipped bitmap fill";
			nfo->fsinfo.bitmap.BitmapId = swf_reader_read_UI16(swf);
			swf_reader_read_MATRIX(swf,&nfo->fsinfo.bitmap.BitmapMatrix);
			break;
		default:
			return -1;
	};

	return 0;
}

int swf_reader_read_LINESTYLEARRAY_header(swf_reader *swf,swf_LINESTYLEARRAY_read *nfo,unsigned int tag_code) {
	nfo->color_rgba = (tag_code == swftag_DefineShape3 || tag_code == swftag_DefineShape4);
	nfo->LineStyleCount = swf_reader_read_UI8(swf);
	if (nfo->LineStyleCount == 0xFF) nfo->LineStyleCount = swf_reader_read_UI16(swf);
	return 0;
}

