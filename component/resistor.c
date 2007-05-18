/*	$Csoft: resistor.c,v 1.5 2005/09/27 03:34:09 vedge Exp $	*/

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
#include "resistor.h"

const ES_ComponentOps esResistorOps = {
	{
		"ES_Component:ES_Resistor",
		sizeof(ES_Resistor),
		{ 0,0 },
		ES_ResistorInit,
		NULL,			/* reinit */
		ES_ComponentDestroy,
		ES_ResistorLoad,
		ES_ResistorSave,
		ES_ComponentEdit
	},
	N_("Resistor"),
	"R",
	ES_ResistorDraw,
	ES_ResistorEdit,
	NULL,			/* instance_menu */
	NULL,			/* class_menu */
	ES_ResistorExport,
	NULL			/* connect */
};

const ES_Port esResistorPinout[] = {
	{ 0, "",  0.000, 0.625 },
	{ 1, "A", 0.000, 0.000 },
	{ 2, "B", 1.250, 0.000 },
	{ -1 },
};

int esResistorDispVal = 0;
enum {
	RESISTOR_BOX_STYLE,
	RESISTOR_US_STYLE
} esResistorStyle = 0;

void
ES_ResistorDraw(void *p, VG *vg)
{
	ES_Resistor *r = p;

	switch (esResistorStyle) {
	case RESISTOR_BOX_STYLE:
		VG_Begin(vg, VG_LINES);
		VG_Vertex2(vg, 0.000, 0.000);
		VG_Vertex2(vg, 0.156, 0.000);
		VG_Vertex2(vg, 1.250, 0.000);
		VG_Vertex2(vg, 1.09375, 0.000);
		VG_End(vg);
		VG_Begin(vg, VG_LINE_LOOP);
		VG_Vertex2(vg, 0.156, -0.240);
		VG_Vertex2(vg, 0.156,  0.240);
		VG_Vertex2(vg, 1.09375,  0.240);
		VG_Vertex2(vg, 1.09375, -0.240);
		VG_End(vg);
		break;
	case RESISTOR_US_STYLE:
		VG_Begin(vg, VG_LINE_STRIP);
		VG_Vertex2(vg, 0.000, 0.125);
		VG_Vertex2(vg, 0.125, 0.000);
		VG_Vertex2(vg, 0.250, 0.125);
		VG_Vertex2(vg, 0.375, 0.000);
		VG_Vertex2(vg, 0.500, 0.125);
		VG_Vertex2(vg, 0.625, 0.000);
		VG_Vertex2(vg, 0.750, 0.125);
		VG_Vertex2(vg, 0.875, 0.000);
		VG_Vertex2(vg, 1.000, 0.000);
		VG_End(vg);
		break;
	}

	VG_Begin(vg, VG_TEXT);
	VG_SetStyle(vg, "component-name");
	VG_Vertex2(vg, 0.625, 0.000);
	VG_TextAlignment(vg, VG_ALIGN_MC);

	if (esResistorDispVal) {
		VG_Printf(vg, "%s (%.2f\xce\xa9)", AGOBJECT(r)->name,
		    r->resistance);
	} else {
		VG_Printf(vg, "%s", AGOBJECT(r)->name);
	}
	VG_End(vg);
}

int
ES_ResistorLoad(void *p, AG_Netbuf *buf)
{
	ES_Resistor *r = p;

	if (AG_ReadObjectVersion(buf, r, NULL) == -1 ||
	    ES_ComponentLoad(r, buf) == -1)
		return (-1);

	r->resistance = AG_ReadDouble(buf);
	r->tolerance = (int)AG_ReadUint32(buf);
	r->power_rating = AG_ReadDouble(buf);
	r->Tc1 = AG_ReadFloat(buf);
	r->Tc2 = AG_ReadFloat(buf);
	return (0);
}

int
ES_ResistorSave(void *p, AG_Netbuf *buf)
{
	ES_Resistor *r = p;

	AG_WriteObjectVersion(buf, r);
	if (ES_ComponentSave(r, buf) == -1)
		return (-1);

	AG_WriteDouble(buf, r->resistance);
	AG_WriteUint32(buf, (Uint32)r->tolerance);
	AG_WriteDouble(buf, r->power_rating);
	AG_WriteFloat(buf, r->Tc1);
	AG_WriteFloat(buf, r->Tc2);
	return (0);
}

int
ES_ResistorExport(void *p, enum circuit_format fmt, FILE *f)
{
	ES_Resistor *r = p;

	if (PNODE(r,1) == -1 ||
	    PNODE(r,2) == -1)
		return (0);
	
	switch (fmt) {
	case CIRCUIT_SPICE3:
		fprintf(f, "%s %d %d %g\n", AGOBJECT(r)->name,
		    PNODE(r,1), PNODE(r,2), r->resistance);
		break;
	}
	return (0);
}

SC_Real
ES_ResistorResistance(void *p, ES_Port *p1, ES_Port *p2)
{
	ES_Resistor *r = p;
	SC_Real deltaT = COM_T0 - COM(r)->Tspec;

	return (r->resistance * (1.0 + r->Tc1*deltaT + r->Tc2*deltaT*deltaT));
}

/*
 * Load the element into the conductance matrix. All conductances between
 * (k,j) are added to (k,k) and (j,j), and subtracted from (k,j) and (j,k).
 *
 *   |  Vk    Vj  | RHS
 *   |------------|-----
 * k |  Gkj  -Gkj |
 * j | -Gkj   Gkj |
 *   |------------|-----
 */
static int
ES_ResistorLoadDC_G(void *p, SC_Matrix *G)
{
	ES_Resistor *r = p;
	ES_Node *n;
	u_int k = PNODE(r,1);
	u_int j = PNODE(r,2);
	SC_Real g;

	if (r->resistance == 0.0 || k == -1 || j == -1 || (k == 0 && j == 0)) {
		AG_SetError("null resistance");
		return (-1);
	}
	g = 1.0/r->resistance;
	if (k != 0) {
		G->mat[k][k] += g;
	}
	if (j != 0) {
		G->mat[j][j] += g;
	}
	if (k != 0 && j != 0) {
		G->mat[k][j] -= g;
		G->mat[j][k] -= g;
	}
	return (0);
}

static int
ES_ResistorLoadSP(void *p, SC_Matrix *S, SC_Matrix *N)
{
	ES_Resistor *res = p;
	u_int k = PNODE(res,1);
	u_int j = PNODE(res,2);
	SC_Real r, z, f;
	
	if (res->resistance == 0.0 ||
	    k == -1 || j == -1 || (k == 0 && j == 0)) {
		AG_SetError("null resistance");
		return (-1);
	}

	r = res->resistance;
	z = r/COM_Z0;
	S->mat[k][k] += z/(z+2);
	S->mat[j][j] += z/(z+2);
	S->mat[k][j] -= z/(z+2);
	S->mat[j][k] -= z/(z+2);

	f = COM(res)->Tspec*4.0*r*COM_Z0 /
	    ((2.0*COM_Z0+r)*(2.0*COM_Z0+r)) / COM_T0;
	N->mat[k][k] += f;
	N->mat[j][j] += f;
	N->mat[k][j] -= f;
	N->mat[j][k] -= f;
	return (0);
}

void
ES_ResistorInit(void *p, const char *name)
{
	ES_Resistor *r = p;

	ES_ComponentInit(r, name, &esResistorOps, esResistorPinout);
	r->resistance = 1;
	r->power_rating = HUGE_VAL;
	r->tolerance = 0;
	COM(r)->loadDC_G = ES_ResistorLoadDC_G;
	COM(r)->loadSP= ES_ResistorLoadSP;
}

#ifdef EDITION
void *
ES_ResistorEdit(void *p)
{
	ES_Resistor *r = p;
	AG_Window *win;
	AG_Spinbutton *sb;
	AG_FSpinbutton *fsb;

	win = AG_WindowNew(0);

	fsb = AG_FSpinbuttonNew(win, 0, "ohms", _("Resistance: "));
	AG_WidgetBind(fsb, "value", AG_WIDGET_DOUBLE, &r->resistance);
	AG_FSpinbuttonSetMin(fsb, 1.0);
	
	sb = AG_SpinbuttonNew(win, 0, _("Tolerance (%): "));
	AG_WidgetBind(sb, "value", AG_WIDGET_INT, &r->tolerance);
	AG_SpinbuttonSetRange(sb, 1, 100);

	fsb = AG_FSpinbuttonNew(win, 0, "W", _("Power rating: "));
	AG_WidgetBind(fsb, "value", AG_WIDGET_DOUBLE, &r->power_rating);
	AG_FSpinbuttonSetMin(fsb, 0);
	
	fsb = AG_FSpinbuttonNew(win, 0, "mohms/degC", _("Temp. coefficient: "));
	AG_WidgetBind(fsb, "value", AG_WIDGET_FLOAT, &r->Tc1);
	fsb = AG_FSpinbuttonNew(win, 0, "mohms/degC^2",
	    _("Temp. coefficient: "));
	AG_WidgetBind(fsb, "value", AG_WIDGET_FLOAT, &r->Tc2);

	return (win);
}
#endif /* EDITION */
