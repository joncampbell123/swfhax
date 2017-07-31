
/* NOTES:
 *
 *    For the most part, you could treat Flash shapes as a vector drawing
 *    that encloses a filled area. But----and this is what will trip up
 *    the unaware---Flash has some rules of it's own regarding how shapes
 *    are closed and how you fill them!
 *
 *    The best example of a SWF that might trip you up, is a Flash movie
 *    with a shape like this:
 *
 *    +-------------------+
 *    |                   |
 *    |                   |
 *    |                   |
 *    |             +-----+------+
 *    |             |     |      |
 *    +-------------+-----+      |
 *                  |            |
 *                  +------------+
 *
 *    The naive fool would think that three closed regions are encoded
 *    into the SWF: One for the larger rectangle interior, one for the
 *    intersection, and one for the smaller rectangle's interior.
 *
 *    Nope! I made just such a diagram in Adobe Flash CS3 and this is
 *    how the SWF is encoded!
 *
 *    11-----------------10
 *    |                   |
 *    |                   |
 *    |         C         |
 *    |             4----3/9-----8
 *    |             |  A  |      |
 *    12---------1/5/13---2  B   |
 *                  |            |
 *                  6------------7
 *
 *    In other words:
 *
 *    Starts at point #1, draws to the right to point #2.
 *    Then draws upward to point #3, then to the left to point #4.
 *    Then downward to point #5, which is the same as point #1 (closing region A) and
 *    using FillStyle0 according to each line filling in the region on the left edge.
 *
 *    From #5, draw downward to point #6, then to the right to point #7.
 *    Then upward to point #8. Then to the left, point #9 which is the same as point #3.
 *    Note that this doesn't DIRECTLY close region B, but it gives the Flash player
 *    enough information to implicitly close region B (see how they save bytes??).
 *
 *    From #9, draw upward to point #10, then to the left for point #11, then downward
 *    to point #12, and finally close the region at point #13, noting again that
 *    the vertices originally drawn for region A implicitly close region C by sharing
 *    line segments.
 *
 *    To clarify, this is what is explicitly drawn for region B.
 *
 *          +--------+
 *                   |
 *    +       B      |
 *    |              |
 *    +--------------+
 *
 *    This is apparently implied by FillStyle1 from the coordinates of Region A
 *
 *    +.....+........+
 *    .  A  |        .
 *    +-----+  (B)   .
 *    .              .
 *    +..............+
 *
 *    Because the B region lies on the right edge of the two implied segments
 *    that initially formed Region A, FillStyle1 from those lines is used to
 *    color in Region B (while FillStyle0 is used for the left edge forming Region A).
 *
 *    So you see, you can't just parse through the shape record and blindly feed it
 *    to OpenGL, you've gotta do some additional post processing to the data to
 *    correctly fill in the regions.
 *
 *    Same applies to region C. This is what is explicitly drawn for region C.
 *
 *    +----------------------+
 *    |                      |
 *    |            C         |
 *    |                      |
 *    |                      +
 *    |
 *    +----------------+
 *
 *    This is implied by the line segments already present. Match them up and
 *    you'll know how to close the region properly.
 *
 *    +......................+
 *    .                      .
 *    .        (C)           .
 *    .                      .
 *    .                +-----+.....+
 *    .                |  A  .     .
 *    .................+.....+     .
 *                     .       (B) .
 *                     .............
 *
 *    Understanding this is the key to understanding why the Flash SWF format
 *    would bother defining fillstyles for each edge, and how to use and apply
 *    the FillStyle0 and FillStyle1 indices in the shape definition. */

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

static int				outline_only = 0;
static int				outline_colormap = 0;
static const char*			swf_file = NULL;

static uint32_t outcmap[8] = {
	0xFF0000,	/* red */
	0xFF7F00,	/* orange */
	0xFFFF00,	/* yellow */
	0x7FFF00,	/* yellow-green */
	0x00FF00,	/* green */
	0x00FFFF,	/* cyan */
	0x0000FF,	/* blue */
	0xFF00FF	/* purple */
};

struct swf_point {
	int		x,y;
	int		x2,y2;
} swf_point;

/* outline-only (debugging) dump */
static void do_dump_shape_outline_only(swf_reader *swf,swf_DefineShape *nfo,unsigned int tag_code,FILE *fp) {
#define MAX_POINTS		65536
	struct swf_point *pointxy = NULL;
	swf_FILLSTYLEARRAY_read fsnfo;
	swf_LINESTYLEARRAY_read lsnfo;
	unsigned int padx=0,pady=0;
	unsigned int cmapi=0;
	unsigned int dotn=1;
	swf_SHAPE_reading r;
	unsigned int op=0;
	int pos_x,pos_y;
	unsigned int i;

	if (outline_only && outline_colormap/*TODO*/)
		padx = pady = 400;

	if (swf_reader_read_FILLSTYLEARRAY_header(swf,&fsnfo,tag_code)) return;
	for (i=0;i < fsnfo.FillStyleCount;i++) {
		if (swf_reader_read_FILLSTYLE(swf,&fsnfo)) return;
	}

	if (swf_reader_read_LINESTYLEARRAY_header(swf,&lsnfo,tag_code)) return;
	if (tag_code == swftag_DefineShape4) {
		for (i=0;i < lsnfo.LineStyleCount;i++) {
			if (swf_reader_read_LINESTYLE2(swf,&lsnfo)) return;
		}
	}
	else {
		for (i=0;i < lsnfo.LineStyleCount;i++) {
			if (swf_reader_read_LINESTYLE(swf,&lsnfo)) return;
		}
	}

	pointxy = (struct swf_point*)malloc(sizeof(struct swf_point) * MAX_POINTS);
	assert(pointxy != NULL);

	fprintf(fp,"<?xml version=\"1.0\" standalone=\"no\"?>\n");
	fprintf(fp,"<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");
	fprintf(fp,"<svg width=\"%upx\" height=\"%upx\" viewBox=\"0 0 %u %u\" xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">\n",
		((nfo->ShapeBounds.Xmax - nfo->ShapeBounds.Xmin) + (padx*2)) / SWF_TWIPS_PER_PIXEL,
		((nfo->ShapeBounds.Ymax - nfo->ShapeBounds.Ymin) + (pady*2)) / SWF_TWIPS_PER_PIXEL,
		(nfo->ShapeBounds.Xmax - nfo->ShapeBounds.Xmin) + (padx*2),
		(nfo->ShapeBounds.Ymax - nfo->ShapeBounds.Ymin) + (pady*2));
	fprintf(fp,"  <style type=\"text/css\"><![CDATA[\n");
	fprintf(fp,"   .blackpen { fill:none; stroke:black; stroke-width:20 }\n");
	fprintf(fp,"  ]]></style>\n");
#if 0
	if (!outline_colormap) {
		fprintf(fp,"  <rect x=\"%u\" y=\"%u\" width=\"%u\" height=\"%u\" fill=\"none\" stroke=\"blue\" stroke-width=\"20\" /><!-- DEBUG BORDERLINE -->\n",
			padx,pady,
			nfo->ShapeBounds.Xmax - nfo->ShapeBounds.Xmin,
			nfo->ShapeBounds.Ymax - nfo->ShapeBounds.Ymin);
	}
#endif

	pos_x = pos_y = 0;
	if (swf_reader_read_SHAPE_beginning(swf,&r) == 0) {
		while (swf_reader_read_SHAPE_record(swf,&r) == 0) {
			if (r.ShapeRecord_type == swf_shrec_EndShapeRecord)
				break;

			if (r.ShapeRecord_type == swf_shrec_StraightEdgeRecord ||
				r.ShapeRecord_type == swf_shrec_CurvedEdgeRecord) {
				if (op && outline_colormap) {
					fprintf(fp,"\" />\n");
					op = 0;
				}

				if (dotn <= MAX_POINTS) {
					assert(dotn >= 1);
					pointxy[dotn-1].x = pos_x;
					pointxy[dotn-1].y = pos_y;
				}

				if (!op) {
					if (outline_colormap) {
						fprintf(fp,"  <path stroke=\"#%06x\" ",outcmap[cmapi]);
						fprintf(fp,"onmouseover=\"evt.target.parentNode.getElementById('segment%u').setAttribute('opacity','1.0')\" ",dotn);
						fprintf(fp,"onmouseout=\"evt.target.parentNode.getElementById('segment%u').setAttribute('opacity','0')\" ",dotn);
						fprintf(fp,"stroke-width=\"20\" ");
						fprintf(fp,"fill=\"none\" d=\"M %u %u ",padx + pos_x - nfo->ShapeBounds.Xmin,pady + pos_y - nfo->ShapeBounds.Ymin);

						if ((++cmapi) >= 8) cmapi = 0;
					}
					else {
						fprintf(fp,"  <path class=\"blackpen\" d=\"");
					}

					op = 1;
				}
			}
			else {
				if (op && !outline_colormap) {
					fprintf(fp,"\" />\n");
					op = 0;
				}
			}

			if (r.ShapeRecord_type == swf_shrec_StraightEdgeRecord) {
				pos_x += r.shrec.edge.StraightEdgeRecord.DeltaX;
				pos_y += r.shrec.edge.StraightEdgeRecord.DeltaY;
				fprintf(fp,"L %d %d ",padx + pos_x - nfo->ShapeBounds.Xmin,pady + pos_y - nfo->ShapeBounds.Ymin);
			}
			else if (r.ShapeRecord_type == swf_shrec_CurvedEdgeRecord) {
				pos_x += r.shrec.edge.CurvedEdgeRecord.ControlDeltaX;
				pos_y += r.shrec.edge.CurvedEdgeRecord.ControlDeltaY;
				fprintf(fp,"Q %d %d ",padx + pos_x - nfo->ShapeBounds.Xmin,pady + pos_y - nfo->ShapeBounds.Ymin);
				pos_x += r.shrec.edge.CurvedEdgeRecord.AnchorDeltaX;
				pos_y += r.shrec.edge.CurvedEdgeRecord.AnchorDeltaY;
				fprintf(fp,"%d %d ",padx + pos_x - nfo->ShapeBounds.Xmin,pady + pos_y - nfo->ShapeBounds.Ymin);
			}
			else if (r.ShapeRecord_type == swf_shrec_StyleChangeRecord) {
				if (r.shrec.style.StyleChangeRecord.StateNewStyles) {
#if 0
					fprintf(stderr,"NewStyles in ShapeID %u\n",nfo->ShapeId);
#endif
					if (swf_reader_read_FILLSTYLEARRAY_header(swf,&fsnfo,tag_code)) return;
					for (i=0;i < fsnfo.FillStyleCount;i++) {
						if (swf_reader_read_FILLSTYLE(swf,&fsnfo)) return;
					}

					if (swf_reader_read_LINESTYLEARRAY_header(swf,&lsnfo,tag_code)) return;
					if (tag_code == swftag_DefineShape4) {
						for (i=0;i < lsnfo.LineStyleCount;i++) {
							if (swf_reader_read_LINESTYLE2(swf,&lsnfo)) return;
						}
					}
					else {
						for (i=0;i < lsnfo.LineStyleCount;i++) {
							if (swf_reader_read_LINESTYLE(swf,&lsnfo)) return;
						}
					}

					if (swf_reader_read_SHAPE_beginning(swf,&r)) break;
				}

				if (!op && !outline_colormap) {
					fprintf(fp,"  <path class=\"blackpen\" d=\"");
					op = 1;
				}

				if (r.shrec.style.StyleChangeRecord.StateMoveTo) {
					pos_x = r.shrec.style.StyleChangeRecord.MoveDeltaX;
					pos_y = r.shrec.style.StyleChangeRecord.MoveDeltaY;
				}

				if (op) fprintf(fp,"M %d %d ",padx + pos_x - nfo->ShapeBounds.Xmin,pady + pos_y - nfo->ShapeBounds.Ymin);
			}

			if (r.ShapeRecord_type == swf_shrec_StraightEdgeRecord ||
				r.ShapeRecord_type == swf_shrec_CurvedEdgeRecord) {
				if (op && outline_colormap) {
					fprintf(fp,"\" />\n");
					op = 0;
				}

				if (dotn <= MAX_POINTS) {
					assert(dotn >= 1);
					pointxy[dotn-1].x2 = pos_x;
					pointxy[dotn-1].y2 = pos_y;
					dotn++;
				}
			}
		}

		if (op) {
			fprintf(fp,"\" />\n");
			op = 0;
		}
	}

	if (outline_colormap) { /* TODO: If it's own option... */
		cmapi = 0;
		for (i=1;i < dotn;i++) {
			unsigned int fsz;

			fsz = 280;
			if (i >= 10) fsz = (fsz * 2) / 3;
			else if (i >= 100) fsz = (fsz * 1) / 3;
			else if (i >= 1000) fsz = (fsz * 1) / 5;

			fprintf(fp,"  <g id=\"segment%u\" opacity=\"0\">\n",i);

			fprintf(fp,"    <circle r=\"140px\" fill=\"#%06x\" cx=\"%d\" cy=\"%d\" stroke=\"none\" />\n",
				outcmap[cmapi],padx + pointxy[i-1].x - nfo->ShapeBounds.Xmin,pady + pointxy[i-1].y - nfo->ShapeBounds.Ymin);
			fprintf(fp,"    <text style=\"font-size: %upx; text-anchor:middle; dominant-baseline:middle;\" fill=\"black\" x=\"%d\" y=\"%d\">%u</text>\n",
				fsz,padx + pointxy[i-1].x - nfo->ShapeBounds.Xmin,pady + pointxy[i-1].y - nfo->ShapeBounds.Ymin,i);

			fprintf(fp,"    <circle r=\"140px\" fill=\"none\" stroke=\"#%06x\" stroke-width=\"20\" cx=\"%d\" cy=\"%d\" />\n",
				outcmap[cmapi],padx + pointxy[i-1].x2 - nfo->ShapeBounds.Xmin,pady + pointxy[i-1].y2 - nfo->ShapeBounds.Ymin);

			fprintf(fp,"  </g>\n");
			if ((++cmapi) >= 8) cmapi = 0;
		}
	}

	fprintf(fp,"</svg>\n");
	free(pointxy);
#undef MAX_POINTS
}

typedef struct swf_vertex {
	unsigned char			type; /* shaperecord type, either line, curve, or end record */
	swf_SI32			x,y,cx,cy; /* endpoint in next record */
	swf_UI16			LineStyle;
	swf_UI16			FillStyle[2];
	ssize_t				same_as;	/* if not -1, points to vertex that is in the same place */
	unsigned int			fillclose0:1,fillclose1:1;
} swf_vertex;

static void do_dump_shape(swf_reader *swf,swf_DefineShape *nfo,unsigned int tag_code,FILE *fp) {
#define MAX_VERTICES 65536
	swf_FILLSTYLEARRAY_entry *fstable=NULL;
	swf_LINESTYLEARRAY_entry *lstable=NULL;
	swf_vertex *vertices=NULL,*cv,*nv,*ev,vstate;
	size_t fstable_base=0,lstable_base=0;
	size_t fstable_len=0,lstable_len=0;
	swf_FILLSTYLEARRAY_read fsnfo;
	swf_LINESTYLEARRAY_read lsnfo;
	swf_SHAPE_reading r;
	int vertex_alloc=0;
	int pos_x,pos_y;
	unsigned int i;

	vertices = (swf_vertex*)malloc(sizeof(swf_vertex) * MAX_VERTICES);
	if (vertices == NULL) goto end;
	vertex_alloc = 0;

	memset(&vstate,0,sizeof(vstate));
	vstate.same_as = -1;

	if (swf_reader_read_FILLSTYLEARRAY_header(swf,&fsnfo,tag_code)) goto end;
	fstable_len = fsnfo.FillStyleCount;
	if (fsnfo.FillStyleCount != 0) {
		fstable = (swf_FILLSTYLEARRAY_entry*)malloc(sizeof(swf_FILLSTYLEARRAY_entry) * fsnfo.FillStyleCount);
		assert(fstable != NULL);
		for (i=0;i < fsnfo.FillStyleCount;i++) {
			if (swf_reader_read_FILLSTYLE(swf,&fsnfo)) goto end;
			fstable[i].FillStyleType = fsnfo.FillStyleType;
			fstable[i].fsinfo = fsnfo.fsinfo;
		}
	}

	if (swf_reader_read_LINESTYLEARRAY_header(swf,&lsnfo,tag_code)) goto end;
	lstable_len = lsnfo.LineStyleCount;
	if (lsnfo.LineStyleCount != 0) {
		lstable = (swf_LINESTYLEARRAY_entry*)malloc(sizeof(swf_LINESTYLEARRAY_entry) * lsnfo.LineStyleCount);
		assert(lstable != NULL);

		if (tag_code == swftag_DefineShape4) {
			for (i=0;i < lsnfo.LineStyleCount;i++) {
				if (swf_reader_read_LINESTYLE2(swf,&lsnfo)) goto end;
				lstable[i].Width = lsnfo.Width;
				lstable[i].Color = lsnfo.Color;
				/* ^TODO: Also Flash 8 additional fields... */
			}
		}
		else {
			for (i=0;i < lsnfo.LineStyleCount;i++) {
				if (swf_reader_read_LINESTYLE(swf,&lsnfo)) goto end;
				lstable[i].Width = lsnfo.Width;
				lstable[i].Color = lsnfo.Color;
			}
		}
	}

	fprintf(fp,"<?xml version=\"1.0\" standalone=\"no\"?>\n");
	fprintf(fp,"<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");
	fprintf(fp,"<svg width=\"%upx\" height=\"%upx\" viewBox=\"0 0 %u %u\" xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">\n",
		(nfo->ShapeBounds.Xmax - nfo->ShapeBounds.Xmin) / SWF_TWIPS_PER_PIXEL,
		(nfo->ShapeBounds.Ymax - nfo->ShapeBounds.Ymin) / SWF_TWIPS_PER_PIXEL,
		nfo->ShapeBounds.Xmax - nfo->ShapeBounds.Xmin,
		nfo->ShapeBounds.Ymax - nfo->ShapeBounds.Ymin);
#if 0
	fprintf(fp,"  <rect x=\"0\" y=\"0\" width=\"%u\" height=\"%u\" fill=\"none\" stroke=\"blue\" stroke-width=\"20\" /><!-- DEBUG BORDERLINE -->\n",
		nfo->ShapeBounds.Xmax - nfo->ShapeBounds.Xmin,
		nfo->ShapeBounds.Ymax - nfo->ShapeBounds.Ymin);
#endif

	pos_x = pos_y = 0;
	if (swf_reader_read_SHAPE_beginning(swf,&r) == 0) {
		while (swf_reader_read_SHAPE_record(swf,&r) == 0) {
			if (r.ShapeRecord_type == swf_shrec_EndShapeRecord) {
				if (vertex_alloc >= MAX_VERTICES) {
					fprintf(stderr,"Uh oh! Too many vertices!\n");
					break;
				}

				cv = vertices + (vertex_alloc++);
				vstate.type = swf_shrec_EndShapeRecord;
				vstate.x = pos_x;
				vstate.y = pos_y;
				*cv = vstate;
				break;
			}

			if (r.ShapeRecord_type == swf_shrec_StraightEdgeRecord ||
				r.ShapeRecord_type == swf_shrec_CurvedEdgeRecord) {
			}

			if (r.ShapeRecord_type == swf_shrec_StraightEdgeRecord) {
				if (vertex_alloc >= MAX_VERTICES) {
					fprintf(stderr,"Uh oh! Too many vertices!\n");
					break;
				}

				cv = vertices + (vertex_alloc++);
				vstate.type = r.ShapeRecord_type;
				vstate.x = pos_x;
				vstate.y = pos_y;
				*cv = vstate;

				pos_x += r.shrec.edge.StraightEdgeRecord.DeltaX;
				pos_y += r.shrec.edge.StraightEdgeRecord.DeltaY;
			}
			else if (r.ShapeRecord_type == swf_shrec_CurvedEdgeRecord) {
				if (vertex_alloc >= MAX_VERTICES) {
					fprintf(stderr,"Uh oh! Too many vertices!\n");
					break;
				}

				cv = vertices + (vertex_alloc++);
				vstate.type = r.ShapeRecord_type;
				vstate.x = pos_x;
				vstate.y = pos_y;

				pos_x += r.shrec.edge.CurvedEdgeRecord.ControlDeltaX;
				pos_y += r.shrec.edge.CurvedEdgeRecord.ControlDeltaY;

				vstate.cx = pos_x;
				vstate.cy = pos_y;

				pos_x += r.shrec.edge.CurvedEdgeRecord.AnchorDeltaX;
				pos_y += r.shrec.edge.CurvedEdgeRecord.AnchorDeltaY;

				*cv = vstate;
			}
			else if (r.ShapeRecord_type == swf_shrec_StyleChangeRecord) {
				if (r.shrec.style.StyleChangeRecord.StateNewStyles) {
					void *np;

#if 0
					fprintf(stderr,"NewStyles in ShapeID %u\n",nfo->ShapeId);
#endif

					if (swf_reader_read_FILLSTYLEARRAY_header(swf,&fsnfo,tag_code)) goto end;
					if (fsnfo.FillStyleCount != 0) {
						fstable_base = fstable_len;
						np = (void*)realloc((void*)fstable,(fstable_len + fsnfo.FillStyleCount) * sizeof(swf_FILLSTYLEARRAY_entry));
						assert(np != NULL);
						fstable = (swf_FILLSTYLEARRAY_entry*)np;
						for (i=0;i < fsnfo.FillStyleCount;i++) {
							if (swf_reader_read_FILLSTYLE(swf,&fsnfo)) goto end;
							fstable[i+fstable_len].FillStyleType = fsnfo.FillStyleType;
							fstable[i+fstable_len].fsinfo = fsnfo.fsinfo;
						}
						fstable_len += fsnfo.FillStyleCount;
					}

					if (swf_reader_read_LINESTYLEARRAY_header(swf,&lsnfo,tag_code)) goto end;
					if (lsnfo.LineStyleCount != 0) {
						lstable_base = lstable_len;
						np = (void*)realloc((void*)lstable,(lstable_len + lsnfo.LineStyleCount) * sizeof(swf_LINESTYLEARRAY_entry));
						assert(np != NULL);
						lstable = (swf_LINESTYLEARRAY_entry*)np;
						if (tag_code == swftag_DefineShape4) {
							for (i=0;i < lsnfo.LineStyleCount;i++) {
								if (swf_reader_read_LINESTYLE2(swf,&lsnfo)) goto end;
								lstable[i+lstable_len].Width = lsnfo.Width;
								lstable[i+lstable_len].Color = lsnfo.Color;
								/* ^TODO: Also Flash 8 additional fields... */
							}
						}
						else {
							for (i=0;i < lsnfo.LineStyleCount;i++) {
								if (swf_reader_read_LINESTYLE(swf,&lsnfo)) goto end;
								lstable[i+lstable_len].Width = lsnfo.Width;
								lstable[i+lstable_len].Color = lsnfo.Color;
							}
						}

						lstable_len += lsnfo.LineStyleCount;
					}

					if (swf_reader_read_SHAPE_beginning(swf,&r)) goto end;
				}
				if (r.shrec.style.StyleChangeRecord.StateLineStyle)
					vstate.LineStyle = r.shrec.style.StyleChangeRecord.LineStyle + lstable_base;
				if (r.shrec.style.StyleChangeRecord.StateFillStyle1)
					vstate.FillStyle[1] = r.shrec.style.StyleChangeRecord.FillStyle1 + fstable_base;
				if (r.shrec.style.StyleChangeRecord.StateFillStyle0)
					vstate.FillStyle[0] = r.shrec.style.StyleChangeRecord.FillStyle0 + fstable_base;
				if (r.shrec.style.StyleChangeRecord.StateMoveTo) {
					if (vertex_alloc >= MAX_VERTICES) {
						fprintf(stderr,"Uh oh! Too many vertices!\n");
						break;
					}

					cv = vertices + (vertex_alloc++);
					vstate.type = swf_shrec_EndShapeRecord;
					vstate.x = pos_x;
					vstate.y = pos_y;
					*cv = vstate;

					pos_x = r.shrec.style.StyleChangeRecord.MoveDeltaX;
					pos_y = r.shrec.style.StyleChangeRecord.MoveDeltaY;
				}
			}
		}
	}

	/* first pass: filled regions, look for duplicated vertices */
	fprintf(fp,"<!-- fill -->\n");
	for (i=0;i < (unsigned int)vertex_alloc;) {
		unsigned int scan;

		cv = vertices + i;
#if 0
		fprintf(fp,"<!-- vertex %u fs0/1=%u/%u %.3f,%.3f type=%u -->\n",i,cv->FillStyle[0],cv->FillStyle[1],
			(double)cv->x / SWF_TWIPS_PER_PIXEL,
			(double)cv->y / SWF_TWIPS_PER_PIXEL,
			cv->type);
#endif
		if (cv->same_as != -1) {
			/* this vertex has already been done */
		}
		else {
			unsigned int mrk = i;

			scan = i+1;
			while (1) {
				if ((size_t)scan >= (size_t)vertex_alloc) scan = 0;
				if (scan == i) break;
				nv = vertices + scan;
				if (nv->same_as != -1) {
				}
				else if (cv->x == nv->x && cv->y == nv->y) {
					nv->same_as = mrk;
					mrk = scan;
					fprintf(fp,"<!-- vertex %u, same place as %u -->\n",
						(unsigned int)scan,(unsigned int)nv->same_as);
				}
				scan++;
			}
		}

		i++; /* next vertex */
	}

	/* first pass: filled regions, trace out regions including implicit closure */
	for (i=0;i < (unsigned int)vertex_alloc;) {
		swf_FILLSTYLEARRAY_entry *fs;
		unsigned int start,end = 0;
		double sum = 0;
		int side;

		start = i;
		fprintf(fp,"<!-- begin scan at vertex %u -->\n",start);
		cv = vertices + (i++);
		nv = vertices + i;
		if (cv->type == swf_shrec_EndShapeRecord) continue;

		/* check: clockwise, or counter-clockwise? */
		while (1) {
			if (cv != (vertices + start) && cv->same_as != -1) {
				end = (unsigned int)(cv - vertices); /* NTS: C++ pointer math, memory addr difference divided by sizeof() */
				break; /* we hit the end vertex */
			}
			if (nv == NULL) break;
			sum += (nv->x - cv->x) * (nv->y + cv->y);

			if ((++i) >= (unsigned int)vertex_alloc) {
				cv = nv;
				nv = NULL;
			}
			else {
				cv = nv;
				nv = vertices + i;
			}
		}
		if (end <= start) continue;

		ev = vertices + end;
		cv = vertices + start;
		side = (sum > 0) ? 0 : 1;

		/* if it's "closed" implicitly then we need to continue scanning */
		/* NTS: "cv" is only set correctly to the ending vertex if end is set */
		fprintf(fp,"<!-- closed region vertex %u to %u sum=%.3f (side=%u) fs0/1=%u/%u -->\n",start,end,sum,side,cv->FillStyle[0],cv->FillStyle[1]);

		if (cv->FillStyle[side] > fstable_len) fprintf(stderr,"WARNING: linestyle out of range\n");
		if (cv->FillStyle[side] == 0) continue;
		fs = fstable + cv->FillStyle[side] - 1;

		if ((unsigned int)ev->same_as != start)
			fprintf(fp,"<!-- ...but region is closed implicitly, need more tracing from %u to %u -->\n",(unsigned int)ev->same_as,start);

		fprintf(fp,"  <path stroke=\"none\" fill=\"");
		if (fs->FillStyleType == swf_fillstyle_solid) {
			fprintf(fp,"rgba(%u,%u,%u,%u)",
					fs->fsinfo.solid.Color.Red,
					fs->fsinfo.solid.Color.Green,
					fs->fsinfo.solid.Color.Blue,
					fs->fsinfo.solid.Color.Alpha);
		}
		else {
			fprintf(fp,"none");
		}
		fprintf(fp,"\" d=\"");
		for (i=start;(i+1) <= end;i++) {
			cv = vertices + i;
			if (side) cv->fillclose1=1;
			else cv->fillclose0=1;
			nv = cv + 1;

			if (i == start)
				fprintf(fp,"M %d %d ",
					cv->x - nfo->ShapeBounds.Xmin,cv->y - nfo->ShapeBounds.Ymin);

			if (cv->type == swf_shrec_StraightEdgeRecord) {
				fprintf(fp,"L %d %d ",nv->x - nfo->ShapeBounds.Xmin,nv->y - nfo->ShapeBounds.Ymin);
			}
			else if (cv->type == swf_shrec_CurvedEdgeRecord) {
				fprintf(fp,"Q %d %d ",cv->cx - nfo->ShapeBounds.Xmin,cv->cy - nfo->ShapeBounds.Ymin);
				fprintf(fp,"%d %d ",nv->x - nfo->ShapeBounds.Xmin,nv->y - nfo->ShapeBounds.Ymin);
			}
		}

		/* if closed implicitly, then search for nearby vertices and edges that have NOT yet
		 * been used to fill a region */
		if ((unsigned int)ev->same_as != start && ev->same_as != -1) {
			cv = vertices + ev->same_as;
			/* TODO */
		}

		fprintf(fp,"\" />\n");

		i = end;
	}

	/* second pass: outlines */
	fprintf(fp,"<!-- outline -->\n");
	for (i=0;(i+1) < (unsigned int)vertex_alloc;) {
		swf_LINESTYLEARRAY_entry *ls;

		cv = vertices + (i++);
		nv = vertices + i;

		if (cv->LineStyle > lstable_len) fprintf(stderr,"WARNING: linestyle out of range\n");
		if (cv->LineStyle == 0 || cv->LineStyle > lstable_len) continue;
		if (cv->type == swf_shrec_EndShapeRecord) continue;
		ls = lstable + cv->LineStyle - 1;
		assert(lstable != NULL);

		fprintf(fp,"<!-- linestyle %u -->\n",cv->LineStyle);
		fprintf(fp,"  <path fill=\"none\" stroke=\"rgba(%u,%u,%u,%u)\" stroke-width=\"%u\" d=\"",
			ls->Color.Red,ls->Color.Green,
			ls->Color.Blue,ls->Color.Alpha,
			ls->Width);
		fprintf(fp,"M %d %d ",cv->x - nfo->ShapeBounds.Xmin,cv->y - nfo->ShapeBounds.Ymin);

		do {
			if (cv->type == swf_shrec_StraightEdgeRecord)
				fprintf(fp,"L %d %d ",nv->x - nfo->ShapeBounds.Xmin,nv->y - nfo->ShapeBounds.Ymin);
			else
				fprintf(fp,"Q %d %d %d %d ",cv->cx - nfo->ShapeBounds.Xmin,cv->cy - nfo->ShapeBounds.Ymin,
					nv->x - nfo->ShapeBounds.Xmin,nv->y - nfo->ShapeBounds.Ymin);

			cv = nv;
			if (cv->type == swf_shrec_EndShapeRecord) break;
			if (i >= MAX_VERTICES) break;
			nv = vertices + (i+1);
			if (cv->LineStyle != nv->LineStyle) break;
			i++;
		} while (1);

		fprintf(fp,"\" />\n");
	}

	fprintf(fp,"</svg>\n");

end:	if (lstable) free(lstable);
	if (fstable) free(fstable);
	if (vertices) free(vertices);
}

static void dump_shape_to_svg(swf_reader *swf,swf_DefineShape *nfo,unsigned int tag_code) {
	FILE *fp;

	{
		char tmp[256];

		sprintf(tmp,"swf.shape.%u.svg",nfo->ShapeId);
		fp = fopen(tmp,"w");
		if (fp == NULL) return;
	}

	if (outline_only)
		do_dump_shape_outline_only(swf,nfo,tag_code,fp);
	else
		do_dump_shape(swf,nfo,tag_code,fp);

	fclose(fp);
}

void do_dump_font_to_svg(swf_reader *swf,FILE *fp) {
	unsigned int Xmin,Xmax,Ymin,Ymax;
	swf_SHAPE_reading r;
	unsigned int op=0;
	int pos_x,pos_y;

	Xmin = -20 * 50;
	Ymin = -20 * 50;
	Xmax =  20 * 50;
	Ymax =  20 * 50;

	fprintf(fp,"<?xml version=\"1.0\" standalone=\"no\"?>\n");
	fprintf(fp,"<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");
	fprintf(fp,"<svg width=\"%upx\" height=\"%upx\" viewBox=\"0 0 %u %u\" xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">\n",
		(Xmax - Xmin) / SWF_TWIPS_PER_PIXEL,
		(Ymax - Ymin) / SWF_TWIPS_PER_PIXEL,
		Xmax - Xmin,
		Ymax - Ymin);
	fprintf(fp,"  <style type=\"text/css\"><![CDATA[\n");
	if (outline_only)
		fprintf(fp,"   .blackpen { fill:none; stroke:black; stroke-width:20 }\n");
	else
		fprintf(fp,"   .blackpen { fill:black; stroke:none; }\n");

	fprintf(fp,"  ]]></style>\n");
#if 0
	fprintf(fp,"  <rect x=\"0\" y=\"0\" width=\"%u\" height=\"%u\" fill=\"none\" stroke=\"blue\" stroke-width=\"20\" /><!-- DEBUG BORDERLINE -->\n",
		Xmax - Xmin,
		Ymax - Ymin);
#endif

	pos_x = pos_y = 0;
	if (swf_reader_read_SHAPE_beginning(swf,&r) == 0) {
		while (swf_reader_read_SHAPE_record(swf,&r) == 0) {
			if (r.ShapeRecord_type == swf_shrec_EndShapeRecord)
				break;

			if (!op) {
				fprintf(fp,"  <path class=\"blackpen\" d=\"");
				op = 1;
			}

			if (r.ShapeRecord_type == swf_shrec_StraightEdgeRecord) {
				pos_x += r.shrec.edge.StraightEdgeRecord.DeltaX;
				pos_y += r.shrec.edge.StraightEdgeRecord.DeltaY;
				fprintf(fp,"L %d %d ",pos_x - Xmin,pos_y - Ymin);
			}
			else if (r.ShapeRecord_type == swf_shrec_CurvedEdgeRecord) {
				pos_x += r.shrec.edge.CurvedEdgeRecord.ControlDeltaX;
				pos_y += r.shrec.edge.CurvedEdgeRecord.ControlDeltaY;
				fprintf(fp,"Q %d %d ",pos_x - Xmin,pos_y - Ymin);
				pos_x += r.shrec.edge.CurvedEdgeRecord.AnchorDeltaX;
				pos_y += r.shrec.edge.CurvedEdgeRecord.AnchorDeltaY;
				fprintf(fp,"%d %d ",pos_x - Xmin,pos_y - Ymin);
			}
			else if (r.ShapeRecord_type == swf_shrec_StyleChangeRecord) {
				if (r.shrec.style.StyleChangeRecord.StateNewStyles) {
#if 0
					fprintf(stderr,"NewStyles in Font??\n");
#endif
					break;
				}

				if (r.shrec.style.StyleChangeRecord.StateMoveTo) {
					pos_x = r.shrec.style.StyleChangeRecord.MoveDeltaX;
					pos_y = r.shrec.style.StyleChangeRecord.MoveDeltaY;
				}

				fprintf(fp,"M %d %d ",pos_x - Xmin,pos_y - Ymin);
			}
		}

		if (op) {
			fprintf(fp,"\" />\n");
			op = 0;
		}
	}

	fprintf(fp,"</svg>\n");
}

void dump_font_to_svg(swf_reader *swf,swf_DefineFont *nfo) {
	unsigned int i;
	uint64_t ofs;
	FILE *fp;

	for (i=0;i < nfo->nGlyphs;i++) {
		ofs = (unsigned long long)nfo->absoluteOffsetTable + (unsigned long long)nfo->OffsetTable[i];
		if (swf_reader_seek(swf,ofs) == 0) {
			char tmp[256];

			sprintf(tmp,"swf.font.%u.glyph.%u.svg",nfo->FontID,i);
			fp = fopen(tmp,"w");
			if (fp) do_dump_font_to_svg(swf,fp);
			fclose(fp);
		}
		else {
			fprintf(stderr,"!! Unable to seek to offset (likely reason: non-sequential offsets)\n");
		}
	}
}

static void help() {
	fprintf(stderr,"c4_swfdump [options] <file>\n");
	fprintf(stderr,"    --dbg-outline            Debugging function: render shape outlines only\n");
	fprintf(stderr,"    --dbg-outline-colormap   With --dbg-outline, render each segment with a different color\n");
}

static int parse_argv(int argc,char **argv) {
	int i,swi=0;
	char *a;

	for (i=1;i < argc;) {
		a = argv[i++];

		if (*a == '-') {
			do { a++; } while (*a == '-');

			if (!strcmp(a,"dbg-outline")) {
				outline_only = 1;
			}
			else if (!strcmp(a,"dbg-outline-colormap")) {
				outline_colormap = 1;
			}
			else if (!strcmp(a,"h") || !strcmp(a,"help")) {
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
			case swftag_DefineShape:
			case swftag_DefineShape2:
			case swftag_DefineShape3: {
				swf_DefineShape nfo;

				if (swf_reader_read_DefineShape_header(swf,&nfo) == 0)
					dump_shape_to_svg(swf,&nfo,tag.record.TagCode);

				} break;
			case swftag_DefineShape4: {
				swf_DefineShape nfo;

				if (swf_reader_read_DefineShape4_header(swf,&nfo) == 0)
					dump_shape_to_svg(swf,&nfo,tag.record.TagCode);

				} break;
			case swftag_DefineFont: {
				swf_DefineFont nfo;

				swf_DefineFont_init(&nfo);
				if (swf_reader_read_DefineFont_step1(swf,&nfo) == 0 && nfo.nGlyphs != 0 &&
					swf_reader_read_DefineFont_alloc_OffsetTable(&nfo) == 0 &&
					swf_reader_read_DefineFont_read_OffsetTable(swf,&nfo) == 0)
					dump_font_to_svg(swf,&nfo);

				swf_DefineFont_free(&nfo);
				} break;
		}

		/* clear read stop */
		swf->read_stop = 0;
	}

	swf = swf_reader_destroy(swf);
	return 0;
}

