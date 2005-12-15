/*	$Csoft: spdt.c,v 1.6 2005/09/29 00:22:35 vedge Exp $	*/

/*
 * Copyright (c) 2004, 2005 CubeSoft Communications, Inc.
 * <http://www.winds-triton.com>
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

#include <agar/core.h>
#include <agar/vg.h>
#include <agar/gui.h>

#include "eda.h"
#include "spdt.h"

const AG_Version esSpdtVer = {
	"agar-eda spdt switch",
	0, 0
};

const ES_ComponentOps esSpdtOps = {
	{
		ES_SpdtInit,
		NULL,			/* reinit */
		ES_ComponentDestroy,
		ES_SpdtLoad,
		ES_SpdtSave,
		ES_ComponentEdit
	},
	N_("SPDT switch"),
	"Sw",
	ES_SpdtDraw,
	ES_SpdtEdit,
	NULL,			/* menu */
	NULL,			/* connect */
	NULL,			/* disconnect */
	ES_SpdtExport
};

const ES_Port esSpdtPinout[] = {
	{ 0, "",  0, +1.000 },
	{ 1, "A", 0,  0.000 },
	{ 2, "B", 2, -0.500 },
	{ 3, "C", 2, +0.500 },
	{ -1 },
};

void
ES_SpdtDraw(void *p, VG *vg)
{
	ES_Spdt *sw = p;

	VG_Begin(vg, VG_LINES);
	VG_Vertex2(vg, 0.000, 0.000);
	VG_Vertex2(vg, 0.400, 0.000);
	VG_Vertex2(vg, 1.500, -0.500);
	VG_Vertex2(vg, 2.000, -0.500);
	VG_Vertex2(vg, 1.600, +0.500);
	VG_Vertex2(vg, 2.000, +0.500);
	VG_End(vg);
	
	VG_Begin(vg, VG_CIRCLE);
	VG_Vertex2(vg, 0.525, 0.0000);
	VG_CircleRadius(vg, 0.125);
	VG_End(vg);
	VG_Begin(vg, VG_CIRCLE);
	VG_Vertex2(vg, 1.475, -0.500);
	VG_CircleRadius(vg, 0.125);
	VG_End(vg);
	VG_Begin(vg, VG_CIRCLE);
	VG_Vertex2(vg, 1.475, +0.500);
	VG_CircleRadius(vg, 0.125);
	VG_End(vg);

	VG_Begin(vg, VG_LINES);
	VG_Vertex2(vg, 0.65, 0.000);
	switch (sw->state) {
	case 1:
		VG_Vertex2(vg, 1.35, -0.500);
		break;
	case 2:
		VG_Vertex2(vg, 1.35, +0.500);
		break;
	}
	VG_End(vg);

	VG_Begin(vg, VG_TEXT);
	VG_SetStyle(vg, "component-name");
	VG_Vertex2(vg, 1.200, 0.000);
	VG_TextAlignment(vg, VG_ALIGN_MC);
	VG_Printf(vg, "%s", AGOBJECT(sw)->name);
	VG_End(vg);
}

void
ES_SpdtInit(void *p, const char *name)
{
	ES_Spdt *sw = p;

	ES_ComponentInit(sw, "spdt", name, &esSpdtOps, esSpdtPinout);
	sw->on_resistance = 1.0;
	sw->off_resistance = HUGE_VAL;
	sw->state = 1;
}

int
ES_SpdtLoad(void *p, AG_Netbuf *buf)
{
	ES_Spdt *sw = p;

	if (AG_ReadVersion(buf, &esSpdtVer, NULL) == -1 ||
	    ES_ComponentLoad(sw, buf) == -1)
		return (-1);

	sw->on_resistance = AG_ReadDouble(buf);
	sw->off_resistance = AG_ReadDouble(buf);
	sw->state = (int)AG_ReadUint8(buf);
	return (0);
}

int
ES_SpdtSave(void *p, AG_Netbuf *buf)
{
	ES_Spdt *sw = p;

	AG_WriteVersion(buf, &esSpdtVer);
	if (ES_ComponentSave(sw, buf) == -1)
		return (-1);

	AG_WriteDouble(buf, sw->on_resistance);
	AG_WriteDouble(buf, sw->off_resistance);
	AG_WriteUint8(buf, (Uint8)sw->state);
	return (0);
}

int
ES_SpdtExport(void *p, enum circuit_format fmt, FILE *f)
{
	ES_Spdt *sw = p;
	
	switch (fmt) {
	case CIRCUIT_SPICE3:
		if (PNODE(sw,1) != -1 &&
		    PNODE(sw,2) != -1) {
			fprintf(f, "R%s %d %d %g\n", AGOBJECT(sw)->name,
			    PNODE(sw,1), PNODE(sw,2),
			    ES_SpdtResistance(sw, PORT(sw,1), PORT(sw,2)));
		}
		if (PNODE(sw,1) != -1 &&
		    PNODE(sw,3) != -1) {
			fprintf(f, "R%s %d %d %g\n", AGOBJECT(sw)->name,
			    PNODE(sw,1), PNODE(sw,3),
			    ES_SpdtResistance(sw, PORT(sw,1), PORT(sw,3)));
		}
		if (PNODE(sw,2) != -1 &&
		    PNODE(sw,3) != -1) {
			fprintf(f, "%s %d %d %g\n", AGOBJECT(sw)->name,
			    PNODE(sw,2), PNODE(sw,3),
			    ES_SpdtResistance(sw, PORT(sw,2), PORT(sw,3)));
		}
		break;
	}
	return (0);
}

double
ES_SpdtResistance(void *p, ES_Port *p1, ES_Port *p2)
{
	ES_Spdt *sw = p;

	if (p1->n == 2 && p2->n == 3)
		return (sw->off_resistance);

	switch (p1->n) {
	case 1:
		switch (p2->n) {
		case 2:
			return (sw->state == 1 ? sw->on_resistance :
			                         sw->off_resistance);
		case 3:
			return (sw->state == 2 ? sw->on_resistance :
			                         sw->off_resistance);
		}
		break;
	case 2:
		return (sw->off_resistance);
	}
	return (-1);
}

#ifdef EDITION
static void
toggle_state(AG_Event *event)
{
	ES_Spdt *sw = AG_PTR(1);

	sw->state = (sw->state == 1) ? 2 : 1;
}

void *
ES_SpdtEdit(void *p)
{
	ES_Spdt *sw = p;
	AG_Window *win;
	AG_FSpinbutton *fsb;

	win = AG_WindowNew(0);

	fsb = AG_FSpinbuttonNew(win, 0, "ohms", _("ON resistance: "));
	AG_WidgetBind(fsb, "value", AG_WIDGET_DOUBLE, &sw->on_resistance);
	AG_FSpinbuttonSetMin(fsb, 1.0);
	
	fsb = AG_FSpinbuttonNew(win, 0, "ohms", _("OFF resistance: "));
	AG_WidgetBind(fsb, "value", AG_WIDGET_DOUBLE, &sw->off_resistance);
	AG_FSpinbuttonSetMin(fsb, 1.0);

	AG_ButtonAct(win, AG_BUTTON_EXPAND, _("Toggle state"), toggle_state,
	    "%p", sw);

	return (win);
}
#endif /* EDITION */
