/* NOTES:
 *
 *   I am able to test every version of the Flash format so far, because my copy of
 *   Adobe Flash CS3 still allows me to export to Flash 1 through 9 and still make
 *   ActionScript 2.0 type Flash.
 *
 *   Imagine my shock (oh yeah, sarcasm) when I checked out Adobe Flash CC (Creative
 *   Cloud) and, surprise surprise, you can only author Flash 10 and Flash 11 and
 *   ActionScript 3.0.
 *
 *   Do you guys see why I keep my older software around? This is why. Duh.
 */

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

static void dump_tag_raw(swf_reader *swf,swf_tag *tag) {
	unsigned char temp[16];
	int i,rd;

	if (swf_reader_seek(swf,tag->absolute_offset))
		return;

	rd = swf_reader_read(swf,temp,sizeof(temp));
	if (rd > 0) {
		printf("    RAW: ");
		for (i=0;i < rd;i++) printf("%02x ",temp[i]);
		for (;i < 16;i++) printf("   ");
		for (i=0;i < rd;i++) {
			if (temp[i] >= 0x20 && temp[i] < 0x7F)
				printf("%c",temp[i]);
			else
				printf(".");
		}
		printf("\n");
	}
}

static void dump_tag_raw_remainder(swf_reader *swf) {
	unsigned char temp[16];
	int i,rd;

	rd = swf_reader_read(swf,temp,sizeof(temp));
	if (rd > 0) {
		printf("    REMAINDER: ");
		for (i=0;i < rd;i++) printf("%02x ",temp[i]);
		for (;i < 16;i++) printf("   ");
		for (i=0;i < rd;i++) {
			if (temp[i] >= 0x20 && temp[i] < 0x7F)
				printf("%c",temp[i]);
			else
				printf(".");
		}
		printf("\n");
	}
}

static void dump_tag_raw_text(swf_reader *swf) {
	unsigned char tmp[81];
	int rd;

	printf("    RAW TEXT:\n");
	while ((rd=swf_reader_read(swf,tmp,80)) > 0) {
		tmp[rd] = 0;
		printf("        %s\n",tmp);
	}
}

static void dump_RGB(swf_RGB *rgb) {
	printf("RGB(%u, %u, %u)",rgb->Red,rgb->Green,rgb->Blue);
}

static int dump_FILLSTYLEARRAY(swf_reader *swf,unsigned int tag_code);
static int dump_LINESTYLEARRAY(swf_reader *swf,unsigned int tag_code);

static void dump_SHAPE(swf_reader *swf,unsigned int i,unsigned int tag_code) {
	swf_SHAPE_reading r;

	if (swf_reader_read_SHAPE_beginning(swf,&r) == 0) {
		printf("        SHAPE[%u]: NumFillBits=%u NumLineBits=%u\n",
				i,r.NumFillBits,r.NumLineBits);

		while (swf_reader_read_SHAPE_record(swf,&r) == 0) {
			printf("          %s TypeFlag=%u\n",
					r.ShapeRecord_str,
					r.ShapeRecordTypeFlag);

			if (r.ShapeRecord_type == swf_shrec_EndShapeRecord)
				break;
			else if (r.ShapeRecord_type == swf_shrec_StyleChangeRecord) {
				printf("            StateNewStyles: %u\n",
						r.shrec.style.StyleChangeRecord.StateNewStyles?1:0);
				printf("            StateLineStyle: %u\n",
						r.shrec.style.StyleChangeRecord.StateLineStyle?1:0);
				printf("            StateFillStyle1: %u\n",
						r.shrec.style.StyleChangeRecord.StateFillStyle1?1:0);
				printf("            StateFillStyle0: %u\n",
						r.shrec.style.StyleChangeRecord.StateFillStyle0?1:0);
				printf("            StateMoveTo: %u\n",
						r.shrec.style.StyleChangeRecord.StateMoveTo?1:0);
				if (r.shrec.style.StyleChangeRecord.StateMoveTo) {
					printf("            MoveBits: %u\n",
							r.shrec.style.StyleChangeRecord.MoveBits);
					printf("            Move DeltaX/DeltaY: %.3f, %.3f (%d, %d twips)\n",
							(double)r.shrec.style.StyleChangeRecord.MoveDeltaX /
							SWF_TWIPS_PER_PIXEL,
							(double)r.shrec.style.StyleChangeRecord.MoveDeltaY /
							SWF_TWIPS_PER_PIXEL,
							r.shrec.style.StyleChangeRecord.MoveDeltaX,
							r.shrec.style.StyleChangeRecord.MoveDeltaY);
				}
				if (r.shrec.style.StyleChangeRecord.StateFillStyle1)
					printf("            FillStyle1: %u\n",
							r.shrec.style.StyleChangeRecord.FillStyle1);
				if (r.shrec.style.StyleChangeRecord.StateFillStyle0)
					printf("            FillStyle0: %u\n",
							r.shrec.style.StyleChangeRecord.FillStyle0);
				if (r.shrec.style.StyleChangeRecord.StateLineStyle)
					printf("            LineStyle: %u\n",
							r.shrec.style.StyleChangeRecord.LineStyle);

				if (r.shrec.style.StyleChangeRecord.StateNewStyles) {
					printf("        Style Change...\n");
					if (dump_FILLSTYLEARRAY(swf,tag_code)) break;
					if (dump_LINESTYLEARRAY(swf,tag_code)) break;
					/* NumFillBits and NumLineBits follow */
					if (swf_reader_read_SHAPE_beginning(swf,&r)) break;
					printf("        SHAPE[%u]: NumFillBits=%u NumLineBits=%u\n",
						i,r.NumFillBits,r.NumLineBits);
				}
			}
			else if (r.ShapeRecord_type == swf_shrec_StraightEdgeRecord) {
				printf("            NumBits: %u\n",
						r.shrec.edge.StraightEdgeRecord.NumBits);
				printf("            GeneralLineFlag: %u\n",
						r.shrec.edge.StraightEdgeRecord.GeneralLineFlag);
				if (!r.shrec.edge.StraightEdgeRecord.GeneralLineFlag)
					printf("            VertLineFlag: %u\n",
							r.shrec.edge.StraightEdgeRecord.VertLineFlag);
				printf("            DeltaX/Y: %.3f, %.3f (or %d, %d)\n",
						(double)r.shrec.edge.StraightEdgeRecord.DeltaX /
						SWF_TWIPS_PER_PIXEL,
						(double)r.shrec.edge.StraightEdgeRecord.DeltaY /
						SWF_TWIPS_PER_PIXEL,
						r.shrec.edge.StraightEdgeRecord.DeltaX,
						r.shrec.edge.StraightEdgeRecord.DeltaY);
			}
			else if (r.ShapeRecord_type == swf_shrec_CurvedEdgeRecord) {
				printf("            NumBits: %u\n",
						r.shrec.edge.CurvedEdgeRecord.NumBits);
				printf("            Control DeltaX/Y: %.3f, %.3f (or %d, %d)\n",
						(double)r.shrec.edge.CurvedEdgeRecord.ControlDeltaX /
						SWF_TWIPS_PER_PIXEL,
						(double)r.shrec.edge.CurvedEdgeRecord.ControlDeltaY /
						SWF_TWIPS_PER_PIXEL,
						r.shrec.edge.CurvedEdgeRecord.ControlDeltaX,
						r.shrec.edge.CurvedEdgeRecord.ControlDeltaY);
				printf("            Anchor DeltaX/Y: %.3f, %.3f (or %d, %d)\n",
						(double)r.shrec.edge.CurvedEdgeRecord.AnchorDeltaX /
						SWF_TWIPS_PER_PIXEL,
						(double)r.shrec.edge.CurvedEdgeRecord.AnchorDeltaY /
						SWF_TWIPS_PER_PIXEL,
						r.shrec.edge.CurvedEdgeRecord.AnchorDeltaX,
						r.shrec.edge.CurvedEdgeRecord.AnchorDeltaY);
			}
		}
	}
}

static void dump_GRADIENT(swf_GRADIENT *g) {
	swf_GRADRECORD *r;
	unsigned int i;

	printf("          SpreadMode=%u InterpolationMode=%u NumGradients=%u FocalPoint=%.3f\n",
		g->SpreadMode,
		g->InterpolationMode,
		g->NumGradients,
		(double)g->FocalPoint / 256);
	for (i=0;i < g->NumGradients;i++) {
		r = &g->GradientRecords[i];

		printf("            Ratio=%.3f Color=RGBA %u / %u / %u / %u\n",
			(double)r->Ratio / 255,
			r->Color.Red,
			r->Color.Green,
			r->Color.Blue,
			r->Color.Alpha);
	}
}

static void dump_MATRIX(swf_MATRIX *m) {
	printf("HasScale=%u [%u-bit] HasRotate=%u [%u-bit] Translation=(%.3f,%.3f) [%u-bit]",
		m->HasScale,
		m->NScaleBits,
		m->HasRotate,
		m->NRotateBits,
		(double)m->TranslateX / SWF_TWIPS_PER_PIXEL,
		(double)m->TranslateY / SWF_TWIPS_PER_PIXEL,
		m->NTranslateBits);
	if (m->HasScale)
		printf(" Scale=(%.6f,%.6f)",
			(double)m->ScaleX / 0x10000,
			(double)m->ScaleY / 0x10000);
	if (m->HasRotate)
		printf(" Rotate=(skew %.6f,%.6f)",
			(double)m->RotateSkew0 / 0x10000,
			(double)m->RotateSkew1 / 0x10000);
}

static int dump_FILLSTYLE(swf_reader *swf,swf_FILLSTYLEARRAY_read *nfo,unsigned int i);
static int dump_FILLSTYLE_nr(swf_FILLSTYLEARRAY_read *nfo,unsigned int i);

static int dump_LINESTYLE2(swf_reader *swf,swf_LINESTYLEARRAY_read *nfo,unsigned int i) {
	if (swf_reader_read_LINESTYLE2(swf,nfo)) return -1;

	printf("      LineStyle2[%u]: Width=%.3f StartCapStyle=%u JoinStyle=%u HasFillFlag=%u NoHScaleFlag=%u\n"
               "          NoVScaleFlag=%u PixelHintingFlag=%u NoClose=%u EndCapStyle=%u MiterLimitFactor=%u\n",
	       i,(double)nfo->Width / SWF_TWIPS_PER_PIXEL,
	       nfo->StartCapStyle,
	       nfo->JoinStyle,
	       nfo->HasFillFlag,
	       nfo->NoHScaleFlag,
	       nfo->NoVScaleFlag,
	       nfo->PixelHintingFlag,
	       nfo->NoClose,
	       nfo->EndCapStyle,
	       nfo->MiterLimitFactor);
	if (nfo->HasFillFlag)
		dump_FILLSTYLE_nr(&nfo->FillType,i);
	else {
		printf("        Color: %s %u / %u / %u / %u\n",
			nfo->color_rgba ? "RGBA" : "RGB(A)",
			nfo->Color.Red,
			nfo->Color.Green,
			nfo->Color.Blue,
			nfo->Color.Alpha);
	}

	return 0;
}

static int dump_LINESTYLE(swf_reader *swf,swf_LINESTYLEARRAY_read *nfo,unsigned int i) {
	if (swf_reader_read_LINESTYLE(swf,nfo)) return -1;

	printf("      LineStyle[%u]: Width=%.3f\n",i,(double)nfo->Width / SWF_TWIPS_PER_PIXEL);
	printf("        Color: %s %u / %u / %u / %u\n",
		nfo->color_rgba ? "RGBA" : "RGB(A)",
		nfo->Color.Red,
		nfo->Color.Green,
		nfo->Color.Blue,
		nfo->Color.Alpha);

	return 0;
}

static int dump_FILLSTYLE(swf_reader *swf,swf_FILLSTYLEARRAY_read *nfo,unsigned int i) {
	if (swf_reader_read_FILLSTYLE(swf,nfo)) return -1;
	return dump_FILLSTYLE_nr(nfo,i);
}

static int dump_FILLSTYLE_nr(swf_FILLSTYLEARRAY_read *nfo,unsigned int i) {
	printf("      FillStyleType[%u]:    0x%02x %s\n",i,nfo->FillStyleType,nfo->FillStyleType_str);
	switch (nfo->FillStyleType) {
		case swf_fillstyle_solid:
			printf("        Color: %s %u / %u / %u / %u\n",
				nfo->color_rgba ? "RGBA" : "RGB(A)",
				nfo->fsinfo.solid.Color.Red,
				nfo->fsinfo.solid.Color.Green,
				nfo->fsinfo.solid.Color.Blue,
				nfo->fsinfo.solid.Color.Alpha);
			break;
		case swf_fillstyle_linear_gradient_fill:
		case swf_fillstyle_radial_gradient_fill:
		case swf_fillstyle_focal_radial_gradient_fill:
			/* NTS: Reminder: Flash gradients are based on a virtual coordinate system where
			 *      the -16384,-16384 to 16384,16384 represents -1.0 to 1.0 respectively,
			 *      and you scale it to fit an object by scaling it down from 16384 to the
			 *      width of the object. */
			printf("        GradientMatrix: ");
			dump_MATRIX(&nfo->fsinfo.gradient.GradientMatrix);
			printf("\n");
			printf("        Gradient:\n");
			dump_GRADIENT(&nfo->fsinfo.gradient.Gradient);
			break;
		case swf_fillstyle_repeating_bitmap_fill:
		case swf_fillstyle_clipped_bitmap_fill:
		case swf_fillstyle_non_smoothed_repeating_bitmap_fill:
		case swf_fillstyle_non_smoothed_clipped_bitmap_fill:
			printf("        BitmapId: %u\n",nfo->fsinfo.bitmap.BitmapId);
			printf("        BitmapMatrix: ");
			dump_MATRIX(&nfo->fsinfo.bitmap.BitmapMatrix);
			printf("\n");
			break;
		default:
			printf("!!!! FILLSTYLE NOT IMPLEMENTED\n");
			return -1;
	};

	return 0;
}

static int dump_FILLSTYLEARRAY(swf_reader *swf,unsigned int tag_code) {
	swf_FILLSTYLEARRAY_read nfo;
	unsigned int i;

	if (swf_reader_read_FILLSTYLEARRAY_header(swf,&nfo,tag_code)) {
		printf("    FILLSTYLEARRAY (unable to read)\n");
		return -1;
	}
	printf("    FILLSTYLEARRAY FillStyleCount=%u\n",nfo.FillStyleCount);
	for (i=0;i < nfo.FillStyleCount;i++) {
		if (dump_FILLSTYLE(swf,&nfo,i+1)) return -1;
	}

	return 0;
}

static int dump_LINESTYLEARRAY(swf_reader *swf,unsigned int tag_code) {
	swf_LINESTYLEARRAY_read nfo;
	unsigned int i;

	if (swf_reader_read_LINESTYLEARRAY_header(swf,&nfo,tag_code)) {
		printf("    LINESTYLEARRAY (unable to read)\n");
		return -1;
	}
	printf("    LINESTYLEARRAY LineStyleCount=%u\n",nfo.LineStyleCount);
	if (tag_code == swftag_DefineShape4) {
		for (i=0;i < nfo.LineStyleCount;i++) {
			if (dump_LINESTYLE2(swf,&nfo,i+1)) return -1;
		}
	}
	else {
		for (i=0;i < nfo.LineStyleCount;i++) {
			if (dump_LINESTYLE(swf,&nfo,i+1)) return -1;
		}
	}

	return 0;
}

int main(int argc,char **argv) {
	swf_reader *swf;
	swf_tag tag;
	int fd;

	if (argc < 2) {
		fprintf(stderr,"c4_swfdump <file>\n");
		return 1;
	}

	fd = open(argv[1],O_RDONLY);
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

	printf("SWF version %u (%c%c%c), file length %lu\n",
		swf->header.Version,
		swf->header.Signature[0],
		swf->header.Signature[1],
		swf->header.Signature[2],
		(unsigned long)swf->header.FileLength);
	printf("   FrameSize: Nbits=%u (%.3f,%.3f)-(%.3f,%.3f) or in twips (%u,%u)-(%u,%u)\n",
		swf->header.FrameSize.Nbits,
		(double)swf->header.FrameSize.Xmin / SWF_TWIPS_PER_PIXEL,
		(double)swf->header.FrameSize.Ymin / SWF_TWIPS_PER_PIXEL,
		(double)swf->header.FrameSize.Xmax / SWF_TWIPS_PER_PIXEL,
		(double)swf->header.FrameSize.Ymax / SWF_TWIPS_PER_PIXEL,
		swf->header.FrameSize.Xmin,
		swf->header.FrameSize.Ymin,
		swf->header.FrameSize.Xmax,
		swf->header.FrameSize.Ymax);
	printf("   Frame rate: %.3f    Frame count: %lu\n",
		(double)swf->header.FrameRate / 256,
		(unsigned long)swf->header.FrameCount);
	printf("   Header ends at offset %llu\n",
		(unsigned long long)swf->header_end);

	while (swf_reader_read_record(swf,&tag)) {
		printf("Tag %u (%s), length %lu at %llu (data %llu)\n",(unsigned int)tag.record.TagCode,
			swf_TagCode_to_str(tag.record.TagCode),
			(unsigned long)tag.record.Length,
			(unsigned long long)tag.absolute_tag_offset,
			(unsigned long long)tag.absolute_offset);

		/* set read stop, so we can read the bitfields */
		swf->read_stop = swf->next_tag;

		switch (tag.record.TagCode) {
			case swftag_SetBackgroundColor: {
				swf_RGB BackgroundColor;

				swf_reader_read_RGB(swf,&BackgroundColor);
				printf("    BackgroundColor:      ");
				dump_RGB(&BackgroundColor);
				printf("\n");
				dump_tag_raw_remainder(swf);
				} break;
			case swftag_DefineShape:
			case swftag_DefineShape2:
			case swftag_DefineShape3: {
				swf_DefineShape nfo;

				if (swf_reader_read_DefineShape_header(swf,&nfo) == 0) {
					printf("    ShapeId:              %u\n",nfo.ShapeId);
					printf("    ShapeBounds:          (%.3f,%.3f)-(%.3f,%.3f) or (%d,%d)-(%d,%d) twips\n",
						(double)nfo.ShapeBounds.Xmin / SWF_TWIPS_PER_PIXEL,
						(double)nfo.ShapeBounds.Ymin / SWF_TWIPS_PER_PIXEL,
						(double)nfo.ShapeBounds.Xmax / SWF_TWIPS_PER_PIXEL,
						(double)nfo.ShapeBounds.Ymax / SWF_TWIPS_PER_PIXEL,

						nfo.ShapeBounds.Xmin,
						nfo.ShapeBounds.Ymin,
						nfo.ShapeBounds.Xmax,
						nfo.ShapeBounds.Ymax);

					/* SHAPEWITHSTYLE follows, which means we read a FILLSTYLEARRAY followed by
					 * a LINESTYLEARRAY, followed by a normal SHAPE definition */
					if (dump_FILLSTYLEARRAY(swf,tag.record.TagCode) == 0) {
						if (dump_LINESTYLEARRAY(swf,tag.record.TagCode) == 0) {
							dump_SHAPE(swf,0,tag.record.TagCode);
						}
					}
				}

				dump_tag_raw_remainder(swf);
				} break;
			case swftag_PlaceObject: {
				swf_PlaceObject nfo;

				if (swf_reader_read_PlaceObject(swf,&nfo) == 0) {
					printf("    CharacterId:          %u\n",nfo.CharacterId);
					printf("    Depth:                %u\n",nfo.Depth);
					printf("    Matrix:               ");
					dump_MATRIX(&nfo.Matrix);
					printf("\n");
					/* TODO: CXFORM */
				}
				} break;
			case swftag_RemoveObject: {
				swf_RemoveObject nfo;

				if (swf_reader_read_RemoveObject(swf,&nfo) == 0) {
					printf("    CharacterId:          %u\n",nfo.CharacterId);
					printf("    Depth:                %u\n",nfo.Depth);
				}
				} break;
			case swftag_DefineShape4: {
				swf_DefineShape nfo;

				if (swf_reader_read_DefineShape4_header(swf,&nfo) == 0) {
					printf("    ShapeId:              %u\n",nfo.ShapeId);
					printf("    ShapeBounds:          (%.3f,%.3f)-(%.3f,%.3f) or (%d,%d)-(%d,%d) twips\n",
						(double)nfo.ShapeBounds.Xmin / SWF_TWIPS_PER_PIXEL,
						(double)nfo.ShapeBounds.Ymin / SWF_TWIPS_PER_PIXEL,
						(double)nfo.ShapeBounds.Xmax / SWF_TWIPS_PER_PIXEL,
						(double)nfo.ShapeBounds.Ymax / SWF_TWIPS_PER_PIXEL,

						nfo.ShapeBounds.Xmin,
						nfo.ShapeBounds.Ymin,
						nfo.ShapeBounds.Xmax,
						nfo.ShapeBounds.Ymax);
					printf("    EdgeBounds:           (%.3f,%.3f)-(%.3f,%.3f) or (%d,%d)-(%d,%d) twips\n",
						(double)nfo.EdgeBounds.Xmin / SWF_TWIPS_PER_PIXEL,
						(double)nfo.EdgeBounds.Ymin / SWF_TWIPS_PER_PIXEL,
						(double)nfo.EdgeBounds.Xmax / SWF_TWIPS_PER_PIXEL,
						(double)nfo.EdgeBounds.Ymax / SWF_TWIPS_PER_PIXEL,

						nfo.EdgeBounds.Xmin,
						nfo.EdgeBounds.Ymin,
						nfo.EdgeBounds.Xmax,
						nfo.EdgeBounds.Ymax);
					printf("    UsesFillWindingRule:  %u\n",
						nfo.UsesFillWindingRule);
					printf("    UsesNonScalingStrokes:%u\n",
						nfo.UsesNonScalingStrokes);
					printf("    UsesScalingStrokes:   %u\n",
						nfo.UsesScalingStrokes);

					/* SHAPEWITHSTYLE follows, which means we read a FILLSTYLEARRAY followed by
					 * a LINESTYLEARRAY, followed by a normal SHAPE definition */
					if (dump_FILLSTYLEARRAY(swf,tag.record.TagCode) == 0) {
						if (dump_LINESTYLEARRAY(swf,tag.record.TagCode) == 0) {
							dump_SHAPE(swf,0,tag.record.TagCode);
						}
					}
				}

				dump_tag_raw_remainder(swf);
				} break;
			case swftag_DefineSound: {
				swf_DefineSound nfo;

				if (swf_reader_read_DefineSound(swf,&nfo) == 0) {
					printf("    SoundId:              %u\n",nfo.SoundId);
					printf("    SoundFormat:          %u (%s)\n",nfo.SoundFormat,nfo.SoundFormat_str);
					printf("    SoundRate:            %u (%uHz)\n",nfo.SoundRate,nfo.SoundRate_value);
					printf("    SoundSize:            %u (%u bits)\n",nfo.SoundSize,nfo.SoundSize_value);
					printf("    SoundType:            %u (%u channels)\n",nfo.SoundType,nfo.SoundType_value);
					printf("    SoundSampleCount:     %u\n",nfo.SoundSampleCount);

					if (nfo.SoundFormat == 2/*MP3*/) {
						swf_UI16 t16;

						t16 = swf_reader_read_UI16(swf);
						printf("    SampleCount [MP3]:    %u\n",t16);
					}
				}

				dump_tag_raw_remainder(swf);
				} break;
			case swftag_DefineFont: {
				swf_DefineFont nfo;

				swf_DefineFont_init(&nfo);
				if (swf_reader_read_DefineFont_step1(swf,&nfo) == 0) {
					printf("    FontID:               %u\n",nfo.FontID);
					printf("    nGlyphs:              %u\n",nfo.nGlyphs);
				}
				if (nfo.nGlyphs != 0 && swf_reader_read_DefineFont_alloc_OffsetTable(&nfo) == 0 &&
					swf_reader_read_DefineFont_read_OffsetTable(swf,&nfo) == 0) {
					unsigned int i;
					uint64_t ofs;

					printf("    OffsetTable:\n");
					for (i=0;i < nfo.nGlyphs;i++) {
						ofs = (unsigned long long)nfo.absoluteOffsetTable + (unsigned long long)nfo.OffsetTable[i];
						printf("        %u from start of OffsetTable (%u into GlyphShapeTable or %llu)\n",
							(unsigned int)nfo.OffsetTable[i],
							(unsigned int)(nfo.OffsetTable[i] - nfo._OffsetTableEntry0),
							(unsigned long long)ofs);
					}

					for (i=0;i < nfo.nGlyphs;i++) {
						ofs = (unsigned long long)nfo.absoluteOffsetTable + (unsigned long long)nfo.OffsetTable[i];
						if (swf_reader_seek(swf,ofs) == 0)
							dump_SHAPE(swf,i,tag.record.TagCode);
						else
							fprintf(stderr,"!! Unable to seek to offset (likely reason: non-sequential offsets)\n");
					}
				}

				swf_DefineFont_free(&nfo);
				dump_tag_raw_remainder(swf);
				} break;
			case swftag_SoundStreamHead:
			case swftag_SoundStreamHead2: {
				swf_SoundStreamHead nfo;

				if (swf_reader_read_SoundStreamHead(swf,&nfo) == 0) {
					printf("    PlaybackSoundRate:    %u (%uHz)\n",nfo.PlaybackSoundRate,nfo.PlaybackSoundRate_value);
					printf("    PlaybackSoundSize:    %u (%u bits)\n",nfo.PlaybackSoundSize,nfo.PlaybackSoundSize_value);
					printf("    PlaybackSoundType:    %u (%u channels)\n",nfo.PlaybackSoundType,nfo.PlaybackSoundType_value);
					printf("    StreamSoundCompression: %u (%s)\n",nfo.StreamSoundCompression,nfo.StreamSoundCompression_str);
					printf("    StreamSoundRate:      %u (%uHz)\n",nfo.StreamSoundRate,nfo.StreamSoundRate_value);
					printf("    StreamSoundSize:      %u (%u bits)\n",nfo.StreamSoundSize,nfo.StreamSoundSize_value);
					printf("    StreamSoundType:      %u (%u channels)\n",nfo.StreamSoundType,nfo.StreamSoundType_value);
					printf("    StreamSoundSampleCount: %u\n",nfo.StreamSoundSampleCount);
					if (nfo.LatencySeek_present)
						printf("    LatencySeek:          %u\n",nfo.LatencySeek);
				}

				dump_tag_raw_remainder(swf);
				} break;
			case swftag_FileAttributes: {
				swf_FileAttributes nfo;

				if (swf_reader_read_FileAttributes(swf,&nfo) == 0) {
					printf("    UseDirectBlit:        %u\n",nfo.UseDirectBlit);
					printf("    UseGPU:               %u\n",nfo.UseGPU);
					printf("    HasMetadata:          %u\n",nfo.HasMetadata);
					printf("    ActionScript3:        %u\n",nfo.ActionScript3);
					printf("    UseNetwork:           %u\n",nfo.UseNetwork);
				}

				dump_tag_raw_remainder(swf);
				} break;
			case swftag_Metadata:
				dump_tag_raw_text(swf);
				break;
			default:
				dump_tag_raw(swf,&tag);
				break;
		}

		/* clear read stop */
		swf->read_stop = 0;
	}

	swf = swf_reader_destroy(swf);
	return 0;
}

