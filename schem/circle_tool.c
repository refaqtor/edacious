/*
 * Copyright (c) 2008 Hypertriton, Inc. <http://hypertriton.com/>
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
 * Circle tool.
 */

#include <eda.h>

typedef struct es_schem_circle_tool {
	VG_Tool _inherit;
	VG_Circle *vcCur;
} ES_SchemCircleTool;

static void
Init(void *p)
{
	ES_SchemCircleTool *t = p;

	t->vcCur = NULL;
}

static __inline__ void
AdjustRadius(VG_Circle *vc, VG_Vector vPos)
{
	vc->r = VG_Distance(vPos, VG_PointPos(vc->p));
}

static int
MouseButtonDown(void *p, VG_Vector vPos, int button)
{
	ES_SchemCircleTool *t = p;
	ES_Schem *scm = VGTOOL(t)->p;
	VG_View *vv = VGTOOL(t)->vgv;
	VG *vg = scm->vg;
	VG_Point *pCenter;

	switch (button) {
	case SDL_BUTTON_LEFT:
		if (t->vcCur == NULL) {
			if (!(pCenter = VG_SchemFindPoint(vv, vPos, NULL))) {
				pCenter = VG_PointNew(vg->root, vPos);
			}
			t->vcCur = VG_CircleNew(vg->root, pCenter, 1.0f);
			AdjustRadius(t->vcCur, vPos);
		} else {
			AdjustRadius(t->vcCur, vPos);
			t->vcCur = NULL;
		}
		return (1);
	case SDL_BUTTON_MIDDLE:
	case SDL_BUTTON_RIGHT:
		if (t->vcCur != NULL) {
			VG_Delete(t->vcCur);
			t->vcCur = NULL;
		}
		return (1);
	default:
		return (0);
	}
}

static int
MouseMotion(void *p, VG_Vector vPos, VG_Vector vRel, int buttons)
{
	ES_SchemCircleTool *t = p;
	ES_Schem *scm = VGTOOL(t)->p;
	VG_View *vv = VGTOOL(t)->vgv;
	VG_Point *pEx;
	
	if (t->vcCur != NULL) {
		AdjustRadius(t->vcCur, vPos);
		VG_Status(vv, _("Set radius: %.2f"), t->vcCur->r);
	} else {
		if ((pEx = VG_SchemFindPoint(vv, vPos, NULL))) {
			VG_Status(vv, _("Use Point%u as center"),
			    VGNODE(pEx)->handle);
		} else {
			VG_Status(vv, _("Circle center at %.2f,%.2f"),
			    vPos.x, vPos.y);
		}
	}
	return (0);
}

static void
PostDraw(void *p, VG_View *vv)
{
	VG_Tool *t = p;
	int x, y;

	VG_GetViewCoords(vv, t->vCursor, &x,&y);
	AG_DrawCircle(vv, x,y, 3, VG_MapColorRGB(vv->vg->selectionColor));
}

VG_ToolOps esSchemCircleTool = {
	N_("Circle"),
	N_("Insert circles in the component schematic."),
	&vgIconCircle,
	sizeof(ES_SchemCircleTool),
	0,
	Init,
	NULL,			/* destroy */
	NULL,			/* edit */
	NULL,			/* predraw */
	PostDraw,
	MouseMotion,
	MouseButtonDown,
	NULL,			/* mousebuttonup */
	NULL,			/* keydown */
	NULL			/* keyup */
};