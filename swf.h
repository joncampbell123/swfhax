
#ifndef __SWFHAX_FLASH_SWF_H
#define __SWFHAX_FLASH_SWF_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>

#include "rawint.h"
#include "sliding_window.h"

#include <zlib.h>		/* ZLIB decompression */
#include <lzma.h>		/* LZMA decompression */

/* Flash file format data types. Little endian. Bit fields are MSB first. */

typedef uint8_t		swf_UI8;	/* or UI8[n] for n-element array */
typedef uint16_t	swf_UI16;	/* or UI16[n] for n-element array */
typedef uint32_t	swf_UI32;	/* or UI32[n] for n-element array */
typedef uint64_t	swf_UI64;	/* or UI64[n] for n-element array */

typedef int8_t		swf_SI8;	/* or SI8[n] for n-element array */
typedef int16_t		swf_SI16;	/* or SI16[n] for n-element array */
typedef int32_t		swf_SI32;	/* or SI32[n] for n-element array */

typedef int32_t		swf_FIXED;	/* 32-bit 16.16 fixed point */
typedef int16_t		swf_FIXED8;	/* 16-bit 8.8 fixed point */

typedef uint32_t	swf_UFIXED;	/* 32-bit 16.16 fixed point unsigned */
typedef uint16_t	swf_UFIXED8;	/* 16-bit 8.8 fixed point unsigned */

typedef uint16_t	swf_FLOAT16;	/* 16-bit half-precision floating point, use accessor function */
typedef uint32_t	swf_FLOAT;	/* 32-bit single-precision floating point, use accessor function */
typedef uint64_t	swf_DOUBLE;	/* 64-bit double-precision floating point, use accessor function */

/* other conventions:
 *
 * SB[n]      n-bit value, MSB is sign-extended (ex: 4-bit 1100 would become 32-bit integer 11111111'11111111'11111111'11111100
 * UB[n]      n-bit value
 * FB[n]      n-bit fixed point value (?) */

/* Adobe Flash File Format Spec, v19 "Using Bit Values" */
typedef struct swf_RECT {
	unsigned char	Nbits;		/* number of bits in each field */
	swf_SI32	Xmin,Xmax,Ymin,Ymax;
} swf_RECT;

/* 20 twips per logical pixel, which at 100% scale, is equal to one screen pixel */
#define SWF_TWIPS_PER_PIXEL	(20)

/* Language Code */
typedef swf_UI8		swf_LanguageCode;

typedef struct swf_RGB {
	swf_UI8		Red,Green,Blue;
} __attribute__((packed)) swf_RGB;

typedef struct swf_RGBA {
	swf_UI8		Red,Green,Blue,Alpha;
} __attribute__((packed)) swf_RGBA;

typedef struct swf_ARGB {
	swf_UI8		Alpha,Red,Green,Blue;
} __attribute__((packed)) swf_ARGB;

typedef struct swf_MATRIX { /* NTS: Fields are OUT OF ORDER compared to actual layout on disk */
	unsigned int	HasScale:1;		/* UB[1] */
	unsigned int	HasRotate:1;		/* UB[1] */
	unsigned char	NScaleBits;		/* UB[5] */
	unsigned char	NRotateBits;		/* UB[5] */
	unsigned char	NTranslateBits;		/* UB[5] */
	swf_FIXED	ScaleX,ScaleY;		/* FB[NScaleBits] */
	swf_FIXED	RotateSkew0,RotateSkew1;/* FB[NRotateBits] */
	swf_FIXED	TranslateX,TranslateY;	/* SB[NTranslateBits] */
} swf_MATRIX;

typedef struct swf_CXFORM {
	unsigned int	HasAddTerms:1;		/* UB[1] */
	unsigned int	NasMultTerms:1;		/* UB[1] */
	unsigned int	Nbits:6;		/* UB[4] */
	swf_SI16	RedMultTerm;		/* SB[Nbits] if HasMultTerms */
	swf_SI16	GreenMultTerm;		/* SB[Nbits] if HasMultTerms */
	swf_SI16	BlueMultTerm;		/* SB[Nbits] if HasMultTerms */
	swf_SI16	RedAddTerm;		/* SB[Nbits] if HasAddTerms */
	swf_SI16	GreenAddTerm;		/* SB[Nbits] if HasAddTerms */
	swf_SI16	BlueAddTerm;		/* SB[Nbits] if HasAddTerms */
} swf_CXFORM;

typedef struct swf_CXFORMWITHALPHA {
	unsigned int	HasAddTerms:1;		/* UB[1] */
	unsigned int	NasMultTerms:1;		/* UB[1] */
	unsigned int	Nbits:6;		/* UB[4] */
	swf_SI16	RedMultTerm;		/* SB[Nbits] if HasMultTerms */
	swf_SI16	GreenMultTerm;		/* SB[Nbits] if HasMultTerms */
	swf_SI16	BlueMultTerm;		/* SB[Nbits] if HasMultTerms */
	swf_SI16	AlphaMultTerm;		/* SB[Nbits] if HasMultTerms */
	swf_SI16	RedAddTerm;		/* SB[Nbits] if HasAddTerms */
	swf_SI16	GreenAddTerm;		/* SB[Nbits] if HasAddTerms */
	swf_SI16	BlueAddTerm;		/* SB[Nbits] if HasAddTerms */
	swf_SI16	AlphaAddTerm;		/* SB[Nbits] if HasAddTerms */
} swf_CXFORMWITHALPHA;

typedef struct swf_RECORDHEADER {
	swf_UI16	TagCode;
	swf_UI32	Length;
} swf_RECORDHEADER;

typedef struct swf_tag {
	swf_RECORDHEADER	record;
	uint64_t		absolute_tag_offset;
	uint64_t		absolute_offset;
} swf_tag;

typedef struct swf_SoundStreamHead {
	unsigned int		PlaybackSoundRate:2;
	unsigned int		PlaybackSoundSize:1;
	unsigned int		PlaybackSoundType:1;
	unsigned int		LatencySeek_present:1;
	unsigned int		StreamSoundRate:2;
	unsigned int		StreamSoundSize:1;
	unsigned int		StreamSoundCompression:4;
	unsigned int		StreamSoundType:1;
	uint16_t		PlaybackSoundRate_value;
	uint8_t			PlaybackSoundSize_value;
	uint8_t			PlaybackSoundType_value;
	uint16_t		StreamSoundRate_value;
	uint8_t			StreamSoundSize_value;
	uint8_t			StreamSoundType_value;
	swf_UI16		StreamSoundSampleCount;
	swf_UI16		LatencySeek;
	const char*		StreamSoundCompression_str;
} swf_SoundStreamHead;

typedef struct swf_RemoveObject {
	swf_UI16		CharacterId;
	swf_UI16		Depth;
} swf_RemoveObject;

typedef struct swf_PlaceObject {
	swf_UI16		CharacterId;
	swf_UI16		Depth;
	swf_MATRIX		Matrix;
	swf_CXFORM		ColorTransform;
} swf_PlaceObject;

typedef struct swf_DefineSound {
	swf_UI16		SoundId;
	const char*		SoundFormat_str;
	unsigned int		SoundFormat:4;
	unsigned int		SoundRate:2;
	unsigned int		SoundSize:1;
	unsigned int		SoundType:1;
	uint16_t		SoundRate_value;
	uint8_t			SoundSize_value;
	uint8_t			SoundType_value;
	swf_UI32		SoundSampleCount;
} swf_DefineSound;

typedef struct swf_FileAttributes {
	unsigned int		UseDirectBlit:1;
	unsigned int		UseGPU:1;
	unsigned int		HasMetadata:1;
	unsigned int		ActionScript3:1;
	unsigned int		UseNetwork:1;
} swf_FileAttributes;

typedef struct swf_DefineFont {
	swf_UI16		FontID;
	swf_UI16		nGlyphs;	/* inferred from first offset entry */
	swf_UI16		_OffsetTableEntry0;
	swf_UI16*		OffsetTable;
	uint64_t		absoluteOffsetTable;
	/* TODO: Array of structures containing SHAPEs read from each offset representing the GlyphShapeTable */
} swf_DefineFont;

typedef struct swf_DefineShape {
	swf_UI16		ShapeId;
	swf_RECT		ShapeBounds;
	/* DefineShape4 */
	swf_RECT		EdgeBounds;
	unsigned int		UsesFillWindingRule:1;
	unsigned int		UsesNonScalingStrokes:1;
	unsigned int		UsesScalingStrokes:1;
} swf_DefineShape;

enum {
	swf_shrec_EndShapeRecord=0,
	swf_shrec_StyleChangeRecord,
	swf_shrec_StraightEdgeRecord,
	swf_shrec_CurvedEdgeRecord
};

typedef union swf_SHAPE_record {
	union { /* TypeFlag == 0 */
		struct {
			unsigned int	EndOfShape:5;
		} EndShapeRecord; /* EndOfShape == 0 */
		struct {
			unsigned int	StateNewStyles:1;
			unsigned int	StateLineStyle:1;
			unsigned int	StateFillStyle1:1;
			unsigned int	StateFillStyle0:1;
			unsigned int	StateMoveTo:1;
			unsigned int	MoveBits:5;	/* if StateMoveTo */
			swf_SI32	MoveDeltaX;
			swf_SI32	MoveDeltaY;
			swf_UI16	FillStyle0;
			swf_UI16	FillStyle1;
			swf_UI16	LineStyle;
			/* NTS: If StateNewStyles is set, it's the caller's responsibility to read
			 *      the new FILLSTYLEARRAY and LINESTYLEARRAY as well as the new
			 *      NumFillBits and NumLineBits fields. */
		} StyleChangeRecord;
	} style;
	union { /* TypeFlag == 1 */
		struct {
			unsigned int	StraightFlag:1;
			unsigned int	NumBits:5;	/* NTS: the actual bitfield is bits - 2, we add +2 on read */
			unsigned int	GeneralLineFlag:1;
			unsigned int	VertLineFlag:1;	/* if GeneralLineFlag == 0. if set, vertical line. if clear, horizontal */
			swf_SI32	DeltaX;
			swf_SI32	DeltaY;
		} StraightEdgeRecord; /* StraightFlag == 1 */
		struct {
			unsigned int	StraightFlag:1;
			unsigned int	NumBits:5;	/* NTS: the actual bitfield is bits - 2, we add +2 on read */
			swf_SI32	ControlDeltaX;
			swf_SI32	ControlDeltaY;
			swf_SI32	AnchorDeltaX;
			swf_SI32	AnchorDeltaY;
		} CurvedEdgeRecord; /* StraightFlag == 0 */
	} edge;
} swf_SHAPE_record;

typedef struct swf_SHAPE_entry {
	unsigned char		ShapeRecord_type;
	swf_SHAPE_record	shrec;
} swf_SHAPE_entry;

typedef struct swf_SHAPE_reading {
	unsigned int		NumFillBits:4;
	unsigned int		NumLineBits:4;
	unsigned int		ShapeRecordTypeFlag:1;
	const char*		ShapeRecord_str;
	unsigned char		ShapeRecord_type;
	swf_SHAPE_record	shrec;
} swf_SHAPE_reading;

typedef struct swf_FILEHEADER {
	swf_UI8		Signature[3];		/* "FWS" (SWF backwards).
						   First char may be "C" for zlib, or "Z" for LZMA compressed files */
	swf_UI8		Version;		/* version number (i.e. 0x06 for Flash 6) */
	swf_UI32	FileLength;		/* file length (if compressed, this length is the uncompressed size) */
/* Fields beyond this point and all tags are ZLIB/LZMA compressed */
	swf_RECT	FrameSize;		/* SWF frame size in twips */
	swf_UFIXED8	FrameRate;		/* Frame rate in 8.8 fixed point (though Flash seems to ignore the fractional part) */
	swf_UI16	FrameCount;		/* Total number of frames in the file */
} swf_FILEHEADER;

typedef struct swf_GRADRECORD {
	swf_UI8		Ratio;
	swf_RGBA	Color;
} swf_GRADRECORD;

typedef struct swf_GRADIENT {
	unsigned int	SpreadMode:2;
	unsigned int	InterpolationMode:2;
	unsigned int	NumGradients:4;
	swf_GRADRECORD	GradientRecords[16];
	/* DefineShape4 */
	swf_FIXED8	FocalPoint;
} swf_GRADIENT;

typedef union swf_FILLSTYLEARRAY_info {
	struct {
		swf_RGBA	Color;
	} solid;
	struct {
		swf_MATRIX	GradientMatrix;
		swf_GRADIENT	Gradient;
	} gradient;
	struct {
		swf_UI16	BitmapId;
		swf_MATRIX	BitmapMatrix;
	} bitmap;
} swf_FILLSTYLEARRAY_info;

typedef struct swf_FILLSTYLEARRAY_read {
	unsigned int			color_rgba:1;

	swf_UI16			FillStyleCount;

	swf_UI8				FillStyleType;
	const char*			FillStyleType_str;
	swf_FILLSTYLEARRAY_info		fsinfo;
} swf_FILLSTYLEARRAY_read;

typedef struct swf_LINESTYLEARRAY_read {
	unsigned int	color_rgba:1;

	swf_UI16	LineStyleCount;

	swf_UI16	Width;
	swf_RGBA	Color;

	/* DefineShape4 */
	unsigned int	StartCapStyle:2;
	unsigned int	JoinStyle:2;
	unsigned int	HasFillFlag:1;
	unsigned int	NoHScaleFlag:1;
	unsigned int	NoVScaleFlag:1;
	unsigned int	PixelHintingFlag:1;
	unsigned int	NoClose:1;
	unsigned int	EndCapStyle:2;
	swf_UI16	MiterLimitFactor;
	swf_FILLSTYLEARRAY_read FillType;
} swf_LINESTYLEARRAY_read;

typedef struct swf_FILLSTYLEARRAY_entry {
	swf_UI8				FillStyleType;
	swf_FILLSTYLEARRAY_info		fsinfo;
} swf_FILLSTYLEARRAY_entry;

typedef struct swf_LINESTYLEARRAY_entry {
	swf_UI16	Width;
	swf_RGBA	Color;
} swf_LINESTYLEARRAY_entry;

typedef struct swf_reader {
	uint64_t		raw_current_pos;	/* raw file I/O current position */
	uint64_t		current_pos;		/* decompressed file I/O position */
	int			file_fd;
	unsigned char		file_fd_owner;
	int			(*raw_seek)(void *io,uint64_t pos);
	int			(*raw_read)(void *io,void *buf,int len);
	int			(*seek)(void *io,uint64_t pos); /* may not work, if asked to seek backwards in compressed file */
	int			(*read)(void *io,void *buf,int len); /* read file through decompressor, or raw */
	swf_FILEHEADER		header;
	unsigned char		bit_buffer;
	unsigned char		bit_buffer_bits;
	unsigned int		header_end;
	uint64_t		read_stop;		/* if nonzero, all file I/O code stops reading at that decompressed offset */
	uint64_t		next_tag;
	unsigned char		zlib_eof;
	unsigned char		eof;

	/* decompression buffer */
	sliding_window_v4*	decom_buf;
	sliding_window_v4*	out_buf;

	/* ZLIB compression */
	z_stream		zlib;
	lzma_stream		lzma;
} swf_reader;
/* ^ NTS: External code must NOT use the raw_seek & raw_read functions. */

enum {
	swf_fillstyle_solid=0x00,

	swf_fillstyle_linear_gradient_fill=0x10,

	swf_fillstyle_radial_gradient_fill=0x12,

	swf_fillstyle_focal_radial_gradient_fill=0x13,	/* v8+ */

	swf_fillstyle_repeating_bitmap_fill=0x40,
	swf_fillstyle_clipped_bitmap_fill=0x41,
	swf_fillstyle_non_smoothed_repeating_bitmap_fill=0x42,
	swf_fillstyle_non_smoothed_clipped_bitmap_fill=0x43
};

/* SWF tags */
enum {
	swftag_End=0,			/* v1+ */
	swftag_ShowFrame=1,		/* v1+ */
	swftag_DefineShape=2,		/* v1+ */

	swftag_PlaceObject=4,		/* v1+ */
	swftag_RemoveObject=5,		/* v1+ */
	swftag_DefineBits=6,		/* v1+ */
	swftag_DefineButton=7,		/* v1+ */
	swftag_JPEGTables=8,		/* v1+ */
	swftag_SetBackgroundColor=9,	/* v1+ */
	swftag_DefineFont=10,		/* v1+ */
	swftag_DefineText=11,		/* v1+ */
	swftag_DoAction=12,		/* v1+ */
	swftag_DefineFontInfo=13,	/* v1+ */
	swftag_DefineSound=14,		/* v1+ */
	swftag_StartSound=15,		/* v1+ */

	swftag_DefineButtonSound=17,	/* v2+ */
	swftag_SoundStreamHead=18,	/* v1+ */
	swftag_SoundStreamBlock=19,	/* v1+ */
	swftag_DefineBitsLossless=20,	/* v2+ */
	swftag_DefineBitsJPEG2=21,	/* v2+ */
	swftag_DefineShape2=22,		/* v2+ */
	swftag_DefineButtonCxform=23,	/* v2+ */
	swftag_Protect=24,		/* v2+ */

	swftag_PlaceObject2=26,		/* v3+ */

	swftag_RemoveObject2=28,	/* v3+ */

	swftag_DefineShape3=32,		/* v3+ */
	swftag_DefineText2=33,		/* v3+ */
	swftag_DefineButton2=34,	/* v3+ */
	swftag_DefineBitsJPEG3=35,	/* v3+ */
	swftag_DefineBitsLossless2=36,	/* v3+ */
	swftag_DefineEditText=37,	/* v4+ */

	swftag_DefineSprite=39,		/* v3+ */

	swftag_FrameLabel=43,		/* v3+ */

	swftag_SoundStreamHead2=45,	/* v3+ */
	swftag_DefineMorphShape=46,	/* v3+ */

	swftag_DefineFont2=48,		/* v3+ */

	swftag_ExportAssets=56,		/* v5+ */
	swftag_ImportAssets=57,		/* v5+ */
	swftag_EnableDebugger=58,	/* v5 only */
	swftag_DoInitAction=59,		/* v6+ */
	swftag_DefineVideoStream=60,	/* v6+ */
	swftag_VideoFrame=61,		/* v6+ */
	swftag_DefineFontInfo2=62,	/* v6+ */

	swftag_EnableDebugger2=64,	/* v6+ */
	swftag_ScriptLimits=65,		/* v7+ */
	swftag_SetTabIndex=66,		/* v7+ */

	swftag_FileAttributes=69,	/* v8+ */
	swftag_PlaceObject3=70,		/* v8+ */
	swftag_ImportAssets2=71,	/* ?? */

	swftag_DefineFontAlignZones=73,	/* v8+ */
	swftag_CSMTextSettings=74,	/* v8+ */
	swftag_DefineFont3=75,		/* v8+ */
	swftag_SymbolClass=76,		/* v8+ ? */
	swftag_Metadata=77,		/* v1+ (but normally in recent Flash movies) */
	swftag_DefineScalingGrid=78,	/* ?? */

	swftag_DoABC=82,		/* v9+ */
	swftag_DefineShape4=83,		/* v8+ */
	swftag_DefineMorphShape2=84,	/* ?? */

	swftag_DefineSceneAndFrameLabelData=86, /* Adobe's documentation doesn't say what version this appeared */
	swftag_DefineBinaryData=87,
	swftag_DefineFontName=88,	/* v9+ */
	swftag_StartSound2=89,
	swftag_DefineBitsJPEG4=90,
	swftag_DefineFont4=91,

	swftag_EnableTelemetry=93
};

int swf_reader_stock_raw_seek(void *io,uint64_t pos);
const char *swf_TagCode_to_str(unsigned int tag);
int swf_reader_raw_seek(swf_reader *r,uint64_t pos);
int swf_reader_seek(swf_reader *r,uint64_t pos);
int swf_reader_read(swf_reader *r,void *buf,int len);
void swf_reader_read_bytealign(swf_reader *swf);
swf_UI8 swf_reader_read_UI8(swf_reader *swf);
swf_UI32 swf_reader_read_UB(swf_reader *swf,int bits);
swf_SI32 swf_reader_read_SB(swf_reader *swf,int bits);
void swf_reader_read_RECT(swf_reader *swf,swf_RECT *rect);
swf_UI16 swf_reader_read_UI16(swf_reader *swf);
swf_UI32 swf_reader_read_UI32(swf_reader *swf);
swf_SI8 swf_reader_read_SI8(swf_reader *swf);
swf_SI16 swf_reader_read_SI16(swf_reader *swf);
swf_SI32 swf_reader_read_SI32(swf_reader *swf);
int swf_reader_raw_read(swf_reader *r,void *buf,int len);
int swf_reader_uncompressed_seek(void *io,uint64_t pos);
int swf_reader_uncompressed_read(void *io,void *buf,int len);
int swf_reader_zlib_read(void *io,void *buf,int len);
int swf_reader_zlib_seek(void *io,uint64_t pos);
int swf_reader_init_zlib(swf_reader *swf);
int swf_reader_zlib_seek(void *io,uint64_t pos);
int swf_reader_zlib_read(void *io,void *buf,int len);
int swf_reader_stock_raw_read(void *io,void *buf,int len);
int swf_reader_init(swf_reader *r);
void swf_reader_close_file(swf_reader *r);
void swf_reader_free(swf_reader *r);
swf_reader *swf_reader_create();
swf_reader *swf_reader_destroy(swf_reader *r);
void swf_reader_assign_fd_ownership(swf_reader *r);
int swf_reader_read_record(swf_reader *r,swf_tag *tag);
int swf_reader_assign_fd(swf_reader *r,int fd);
int swf_reader_get_file_header(swf_reader *swf);
void swf_reader_read_RGB(swf_reader *swf,swf_RGB *rgb);
void swf_reader_read_RGBA(swf_reader *swf,swf_RGBA *rgb);
void swf_reader_read_RGB_as_RGBA(swf_reader *swf,swf_RGBA *rgb);
int swf_reader_read_DefineSound(swf_reader *swf,swf_DefineSound *h);
int swf_reader_read_SoundStreamHead(swf_reader *swf,swf_SoundStreamHead *h);
int swf_reader_read_FileAttributes(swf_reader *swf,swf_FileAttributes *a);
int swf_reader_read_DefineFont_step1(swf_reader *swf,swf_DefineFont *n);
void swf_DefineFont_init(swf_DefineFont *nfo);
void swf_DefineFont_free(swf_DefineFont *nfo);
int swf_reader_read_DefineFont_alloc_OffsetTable(swf_DefineFont *nfo);
int swf_reader_read_DefineFont_read_OffsetTable(swf_reader *swf,swf_DefineFont *nfo);
int swf_reader_read_SHAPE_beginning(swf_reader *swf,swf_SHAPE_reading *r);
int swf_reader_read_SHAPE_record(swf_reader *swf,swf_SHAPE_reading *r);
int swf_reader_read_DefineShape_header(swf_reader *swf,swf_DefineShape *r);
#define swf_reader_read_DefineShape2_header swf_reader_read_DefineShape_header
int swf_reader_read_DefineShape4_header(swf_reader *swf,swf_DefineShape *r);
int swf_reader_read_GRADIENT(swf_reader *swf,swf_GRADIENT *g,unsigned int rgba);
int swf_reader_read_MATRIX(swf_reader *swf,swf_MATRIX *m);
int swf_reader_read_LINESTYLE(swf_reader *swf,swf_LINESTYLEARRAY_read *nfo);
int swf_reader_read_FILLSTYLE(swf_reader *swf,swf_FILLSTYLEARRAY_read *nfo);
int swf_reader_read_FILLSTYLEARRAY_header(swf_reader *swf,swf_FILLSTYLEARRAY_read *nfo,unsigned int tag_code);
int swf_reader_read_LINESTYLEARRAY_header(swf_reader *swf,swf_LINESTYLEARRAY_read *nfo,unsigned int tag_code);
int swf_reader_read_LINESTYLE2(swf_reader *swf,swf_LINESTYLEARRAY_read *nfo);
int swf_reader_read_RemoveObject(swf_reader *swf,swf_RemoveObject *r);
int swf_reader_read_PlaceObject(swf_reader *swf,swf_PlaceObject *r);

extern const uint16_t swf_SoundRatesUB2[4];
extern const char *swf_SoundCompressionUB4[16];

#endif /* __SWFHAX_FLASH_SWF_H */

