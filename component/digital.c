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
#include "digital.h"

const AG_Version esDigitalVer = {
	"agar-eda digital",
	0, 0
};

void
ES_DigitalDraw(void *p, VG *vg)
{
	ES_Digital *dig = p;

	VG_Begin(vg, VG_LINES);
	VG_HLine(vg, 0.000, 0.15600, 0.000);
	VG_HLine(vg, 1.250, 1.09375, 0.000);
	VG_End(vg);
	VG_Begin(vg, VG_LINE_LOOP);
	VG_Vertex2(vg, 0.156, -0.240);
	VG_Vertex2(vg, 0.156,  0.240);
	VG_Vertex2(vg, 1.09375,  0.240);
	VG_Vertex2(vg, 1.09375, -0.240);
	VG_End(vg);

	VG_Begin(vg, VG_TEXT);
	VG_SetStyle(vg, "component-name");
	VG_Vertex2(vg, 0.625, 0.000);
	VG_TextAlignment(vg, VG_ALIGN_MC);
	VG_Printf(vg, "%s", AGOBJECT(dig)->name);
	VG_End(vg);
}

int
ES_DigitalLoad(void *p, AG_Netbuf *buf)
{
	ES_Digital *dig = p;

	if (AG_ReadVersion(buf, &esDigitalVer, NULL) == -1 ||
	    ES_ComponentLoad(dig, buf) == -1)
		return (-1);

	dig->Vdd = SC_ReadRange(buf);
	dig->Vi = SC_ReadRange(buf);
	dig->Tamb = SC_ReadRange(buf);
	dig->Idd = SC_ReadRange(buf);
	dig->Vol = SC_ReadRange(buf);
	dig->Voh = SC_ReadRange(buf);
	dig->Vil = SC_ReadRange(buf);
	dig->Vih = SC_ReadRange(buf);
	dig->VolUB = SC_ReadRange(buf);
	dig->VohUB = SC_ReadRange(buf);
	dig->VilUB = SC_ReadRange(buf);
	dig->VihUB = SC_ReadRange(buf);
	dig->Iol = SC_ReadRange(buf);
	dig->Ioh = SC_ReadRange(buf);
	dig->Iin = SC_ReadRange(buf);
	dig->Iozh = SC_ReadRange(buf);
	dig->Iozl = SC_ReadRange(buf);
	dig->Tthl = SC_ReadQTimeRange(buf);
	dig->Ttlh = SC_ReadQTimeRange(buf);
	return (0);
}

int
ES_DigitalSave(void *p, AG_Netbuf *buf)
{
	ES_Digital *dig = p;

	AG_WriteVersion(buf, &esDigitalVer);
	if (ES_ComponentSave(dig, buf) == -1)
		return (-1);

	SC_WriteRange(buf, dig->Vdd);
	SC_WriteRange(buf, dig->Vi);
	SC_WriteRange(buf, dig->Tamb);
	SC_WriteRange(buf, dig->Idd);
	SC_WriteRange(buf, dig->Vol);
	SC_WriteRange(buf, dig->Voh);
	SC_WriteRange(buf, dig->Vil);
	SC_WriteRange(buf, dig->Vih);
	SC_WriteRange(buf, dig->VolUB);
	SC_WriteRange(buf, dig->VohUB);
	SC_WriteRange(buf, dig->VilUB);
	SC_WriteRange(buf, dig->VihUB);
	SC_WriteRange(buf, dig->Iol);
	SC_WriteRange(buf, dig->Ioh);
	SC_WriteRange(buf, dig->Iin);
	SC_WriteRange(buf, dig->Iozh);
	SC_WriteRange(buf, dig->Iozl);
	SC_WriteQTimeRange(buf, dig->Tthl);
	SC_WriteQTimeRange(buf, dig->Ttlh);
	return (0);
}

/* Stamp the model conductances. */
static int
ES_DigitalLoadDC_G(void *p, SC_Matrix *G)
{
	ES_Digital *dig = p;
	ES_Node *n;
	int i;

	for (i = 0; i < COM(dig)->npairs; i++) {
		ES_Pair *pair = PAIR(dig,i);
		u_int k = pair->p1->node;
		u_int j = pair->p2->node;
		SC_Real g = vEnt(dig->G, i+1);

		if (g == 0.0) {
			continue;
		}
		if (g == HUGE_VAL ||
		    k == -1 || j == -1 || (k == 0 && j == 0)) {
			continue;
		}
		ES_ComponentLog(dig, "pair=(%s,%s) kj=%d,%d, g=%f (idx %d)",
		    pair->p1->name, pair->p2->name, k, j, g, i);

		if (k != 0) {
			mAdd(G, k, k, g);
		}
		if (j != 0) {
			mAdd(G, j, j, g);
		}
		if (k != 0 && j != 0) {
			mSub(G, k, j, g);
			mSub(G, j, k, g);
		}
	}
	return (0);
}

void
ES_DigitalSwitch(void *p, int port, int nstate)
{
}

void
ES_DigitalInit(void *p, const char *type, const char *name, const void *ops,
    const ES_Port *pinout)
{
	ES_Digital *dig = p;

	ES_ComponentInit(dig, type, name, ops, pinout);
	AG_ObjectSetOps(dig, ops);

	COM(dig)->loadDC_G = ES_DigitalLoadDC_G;

	dig->G = SC_VectorNew(COM(dig)->npairs);
	SC_VectorSetZero(dig->G);
}

#ifdef EDITION
static void
PollPairs(AG_Event *event)
{
	AG_Table *t = AG_SELF();
	ES_Digital *dig = AG_PTR(1);
	int i;

	AG_TableBegin(t);
	for (i = 0; i < COM(dig)->npairs; i++) {
		ES_Pair *pair = &COM(dig)->pairs[i];

		AG_TableAddRow(t, "%d:%s:%s:%f", i+1, pair->p1->name,
		    pair->p2->name, vEnt(dig->G, i+1));
	}
	AG_TableEnd(t);
}

void
ES_DigitalEdit(void *p, void *cont)
{
	ES_Digital *dig = p;
	AG_Table *tbl;

	tbl = AG_TableNewPolled(cont, AG_TABLE_EXPAND, PollPairs, "%p", dig);
	AG_TableAddCol(tbl, "n", "<88>", NULL);
	AG_TableAddCol(tbl, "p1", "<88>", NULL);
	AG_TableAddCol(tbl, "p2", "<88>", NULL);
	AG_TableAddCol(tbl, "G", NULL, NULL);
}
#endif /* EDITION */
