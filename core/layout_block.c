/*
 * Copyright (c) 2009 Hypertriton, Inc. <http://hypertriton.com/>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Layout block entity. Blocks represent groups of layout elements which
 * remain rigid across transformations (e.g., device packages).
 */

#include "core.h"
#include <agar/gui/primitive.h>

static void
Init(void *p)
{
	ES_LayoutBlock *lb = p;

	lb->name[0] = '\0';
	lb->com = NULL;
}

static int
Load(void *p, AG_DataSource *ds, const AG_Version *ver)
{
	ES_LayoutBlock *lb = p;

	(void)AG_ReadUint32(ds);				/* flags */
	AG_CopyString(lb->name, ds, sizeof(lb->name));
	return (0);
}

static void
Save(void *p, AG_DataSource *ds)
{
	ES_LayoutBlock *lb = p;

	AG_WriteUint32(ds, 0);					/* flags */
	AG_WriteString(ds, lb->name);
}

static void
GetNodeExtent(VG_Node *vn, VG_View *vv, VG_Vector *aBlk, VG_Vector *bBlk)
{
	VG_Vector a, b;
	VG_Node *vnChld;

	if (vn->ops->extent != NULL) {
		vn->ops->extent(vn, vv, &a, &b);
	}
	if (a.x < aBlk->x) { aBlk->x = a.x; }
	if (a.y < aBlk->y) { aBlk->y = a.y; }
	if (b.x > bBlk->x) { bBlk->x = b.x; }
	if (b.y > bBlk->y) { bBlk->y = b.y; }

	VG_FOREACH_CHLD(vnChld, vn, vg_node)
		GetNodeExtent(vnChld, vv, aBlk, bBlk);
}

static void
Extent(void *p, VG_View *vv, VG_Vector *a, VG_Vector *b)
{
	ES_LayoutBlock *lb = p;
	VG_Vector vPos = VG_Pos(lb);
	VG_Node *vnChld;

	a->x = vPos.x;
	a->y = vPos.y;
	b->x = vPos.x;
	b->y = vPos.y;

	VG_FOREACH_CHLD(vnChld, lb, vg_node)
		GetNodeExtent(vnChld, vv, a, b);
}

static void
Draw(void *p, VG_View *vv)
{
	ES_LayoutBlock *lb = p;
	AG_Rect rDraw;
	VG_Vector a, b;
	AG_Color c;

	if (lb->com->flags & (ES_COMPONENT_SELECTED|ES_COMPONENT_HIGHLIGHTED)) {
		Extent(lb, vv, &a, &b);
		a.x -= vv->wPixel*4;
		a.y -= vv->wPixel*4;
		b.x += vv->wPixel*4;
		b.y += vv->wPixel*4;
		VG_GetViewCoords(vv, a, &rDraw.x, &rDraw.y);
		rDraw.w = (b.x - a.x)*vv->scale;
		rDraw.h = (b.y - a.y)*vv->scale;
		if (lb->com->flags & ES_COMPONENT_SELECTED) {
			AG_ColorRGBA_8(&c, 0,255,0,64),
			AG_DrawRectBlended(vv, &rDraw, &c,
			    AG_ALPHA_SRC,
			    AG_ALPHA_ONE_MINUS_SRC);
		}
		if (lb->com->flags & ES_COMPONENT_HIGHLIGHTED) {
			AG_Color c;

			c = VG_MapColorRGB(VGNODE(lb)->color);
			AG_DrawRectOutline(vv, &rDraw, &c);
		}
	}
}

static float
PointProximity(void *p, VG_View *vv, VG_Vector *vPt)
{
	ES_LayoutBlock *lb = p;
	float x = vPt->x;
	float y = vPt->y;
	VG_Vector a, b, c, d;
	float len;

	Extent(lb, vv, &a, &c);
	a.x -= vv->wPixel*4;
	a.y -= vv->wPixel*4;
	c.x += vv->wPixel*4;
	c.y += vv->wPixel*4;
	b.x = c.x;
	b.y = a.y;
	d.x = a.x;
	d.y = c.y;

	if (x >= a.x && x <= c.x &&
	    y >= a.y && y <= c.y)
		return (0.0f);
	
	if (y < a.y) {
		if (x < a.x) {
			len = VG_Distance(a, *vPt);	*vPt = a;
			return (len);
		} else if (x > c.x) { 
			len = VG_Distance(b, *vPt);	*vPt = b;
			return (len);
		} else {
			return VG_PointLineDistance(a,b, vPt);
		}
	} else if (y > c.y) {
		if (x > c.x) {
			len = VG_Distance(c, *vPt);	*vPt = c;
			return (len);
		} else if (x < a.x) {
			len = VG_Distance(d, *vPt);	*vPt = d;
			return (len);
		} else {
			return VG_PointLineDistance(d,c, vPt);
		}
	} else {
		if (x < a.x) {
			return VG_PointLineDistance(a,d, vPt);
		} else if (x > c.x) {
			return VG_PointLineDistance(b,c, vPt);
		}
	}
	return (HUGE_VAL);
}

static void
Move(void *p, VG_Vector vPos, VG_Vector vRel)
{
	ES_LayoutBlock *lb = p;
	VG_Matrix T;

	T = VGNODE(lb)->T;
	VG_LoadIdentity(lb);
	VG_Translate(lb, vRel);
	VG_MultMatrix(&VGNODE(lb)->T, &T);
}

VG_NodeOps esLayoutBlockOps = {
	N_("LayoutBlock"),
	&esIconComponent,
	sizeof(ES_LayoutBlock),
	Init,
	NULL,			/* destroy */
	Load,
	Save,
	Draw,
	Extent,
	PointProximity,
	NULL,			/* lineProximity */
	NULL,			/* delete */
	Move,
	NULL			/* edit */
};
