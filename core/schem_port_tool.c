/*
 * Copyright (c) 2008-2009 Hypertriton, Inc. <http://hypertriton.com/>
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
 * Port edition tool.
 */

#include "core.h"
#include <agar/gui/primitive.h>
#include <agar/core/limits.h>

typedef struct es_schem_select_tool {
	VG_Tool _inherit;
	int insPin;
} ES_SchemPortTool;

static void
Init(void *p)
{
	ES_SchemPortTool *t = p;

	t->insPin = 0;
}

static int
MouseButtonDown(void *p, VG_Vector vPos, int button)
{
	ES_SchemPortTool *t = p;
	ES_Component *com = VGTOOL(t)->p;
	VG_View *vv = VGTOOL(t)->vgv;
	VG_Point *vp;
	ES_SchemPort *sp;

	if (button != AG_MOUSE_LEFT) {
		return (0);
	}
	if ((vp = VG_NearestPoint(vv, vPos, NULL)) != NULL) {
		sp = ES_SchemPortNew(vp, NULL);
	} else {
		sp = ES_SchemPortNew(vv->vg->root, NULL);
		VG_Translate(sp, vPos);
	}
	Strlcpy(sp->name, com->ports[t->insPin+1].name, sizeof(sp->name));
	return (1);
}

static int
MouseMotion(void *p, VG_Vector vPos, VG_Vector vRel, int buttons)
{
	ES_SchemPortTool *t = p;
	VG_View *vv = VGTOOL(t)->vgv;
	VG_Point *vp;

	if ((vp = VG_HighlightNearestPoint(vv, vPos, NULL)) != NULL) {
		VG_Status(vv, _("Create Pin #%d on Point%u"),
		    t->insPin+1, VGNODE(vp)->handle);
	} else {
		VG_Status(vv, _("Create Pin #%d at %.2f,%.2f"),
		    t->insPin+1, vPos.x, vPos.y);
	}
	return (0);
}

static int
KeyDown(void *p, int ksym, int kmod, Uint32 unicode)
{
	ES_SchemPortTool *t = p;
	VG_View *vv = VGTOOL(t)->vgv;
	VG_Node *vn;
	Uint nDel = 0;

	if (ksym == AG_KEY_DELETE || ksym == AG_KEY_BACKSPACE) {
del:
		TAILQ_FOREACH(vn, &vv->vg->nodes, list) {
			if (!VG_NodeIsClass(vn, "SchemPort") ||
			    !(vn->flags & VG_NODE_SELECTED)) {
				continue;
			}
			VG_ClearEditAreas(vv);
			if (VG_Delete(vn) == -1) {
				vn->flags &= ~(VG_NODE_SELECTED);
				VG_StatusS(vv, AG_GetError());
			} else {
				nDel++;
			}
			goto del;
		}
		VG_Status(vv, _("Deleted %u ports"), nDel);
		return (1);
	}
	return (0);
}

static void *
Edit(void *p, VG_View *vv)
{
	ES_SchemPortTool *t = p;
	AG_Box *box = AG_BoxNewVert(NULL, AG_BOX_EXPAND);
	ES_Component *com = VGTOOL(t)->p;
	ES_Port *port;
	AG_Radio *rad;
	int i;

	AG_LabelNew(box, 0, _("Class: %s"), OBJECT(com)->cls->name);

	rad = AG_RadioNewInt(box, AG_RADIO_EXPAND, NULL, &t->insPin);
	COMPONENT_FOREACH_PORT(port, i, com) {
		AG_RadioAddItem(rad, "%d (%s)", i, port->name);
	}
	AG_WidgetSetFocusable(rad, 0);
	return (box);
}

static void
PostDraw(void *p, VG_View *vv)
{
	ES_SchemPortTool *t = p;
	VG_Point *pNear;
	VG_Vector v = VGTOOL(t)->vCursor;
	AG_Color c;
	int x, y;

	if ((pNear = VG_NearestPoint(vv, v, NULL))) {
		v = VG_Pos(pNear);
	} else {
		v = VGTOOL(t)->vCursor;
	}
	VG_GetViewCoords(vv, v, &x,&y);
	c = VG_MapColorRGB(vv->vg->selectionColor);
	AG_DrawCircle(vv, x,y, 3, &c);
	AG_DrawCircle(vv, x,y, 4, &c);
	AG_DrawCircle(vv, x,y, 5, &c);
}

VG_ToolOps esSchemPortTool = {
	N_("Port Editor"),
	N_("Create connection points for the schematic."),
	&esIconPortEditor,
	sizeof(ES_SchemPortTool),
	VG_NOSNAP,
	Init,
	NULL,			/* destroy */
	Edit,
	NULL,			/* predraw */
	PostDraw,
	NULL,			/* selected */
	NULL,			/* deselected */
	MouseMotion,
	MouseButtonDown,
	NULL,			/* mousebuttonup */
	KeyDown,
	NULL			/* keyup */
};
