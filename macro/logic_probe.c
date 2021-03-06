/*
 * Copyright (c) 2006-2009 Julien Nadeau (vedge@hypertriton.com)
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
 * Ideal logic probe.
 */

#include <core/core.h>
#include "macro.h"

const ES_Port esLogicProbePorts[] = {
	{ 0, "" },
	{ 1, "A" },
	{ -1 },
};

static void
Draw(void *p, VG *vg)
{
	ES_LogicProbe *r = p;
	VG_Polygon *vp;
	
	vp = VG_PolygonNew(vg->root);
	if (r->state) {
		VG_SetColorRGB(vp, 200,0,0);
	} else {
		VG_SetColorRGB(vp, 0,0,0);
	}
	VG_PolygonVertex(vp, VG_PointNew(vp, VGVECTOR(0.156, -0.125)));
	VG_PolygonVertex(vp, VG_PointNew(vp, VGVECTOR(0.156,  0.125)));
	VG_PolygonVertex(vp, VG_PointNew(vp, VGVECTOR(0.500,  0.125)));
	VG_PolygonVertex(vp, VG_PointNew(vp, VGVECTOR(0.500, -0.125)));
}

static void
DC_StepEnd(void *obj, ES_SimDC *dc)
{
	ES_LogicProbe *lp = obj;
	M_Real v1 = ES_NodeVoltage(COMPONENT(lp)->ckt,PNODE(lp,1));
	M_Real v2 = ES_NodeVoltage(COMPONENT(lp)->ckt,PNODE(lp,2));

	lp->state = ((v1 - v2) >= lp->Vhigh);
}

static void
Init(void *p)
{
	ES_LogicProbe *lp = p;

	ES_InitPorts(lp, esLogicProbePorts);
	lp->Vhigh = 5.0;
	lp->state = 0;
	COMPONENT(lp)->dcStepEnd = DC_StepEnd;

	M_BindReal(lp, "Vhigh", &lp->Vhigh);
	AG_BindInt(lp, "state", &lp->state);
}

static void *
Edit(void *p)
{
	ES_LogicProbe *r = p;
	AG_Box *box = AG_BoxNewVert(NULL, AG_BOX_EXPAND);

	M_NumericalNewRealPNZ(box, 0, "V", _("HIGH voltage: "), &r->Vhigh);
	return (box);
}

ES_ComponentClass esLogicProbeClass = {
	{
		"Edacious(Circuit:Component:LogicProbe)"
		"@macro",
		sizeof(ES_LogicProbe),
		{ 0,0 },
		Init,
		NULL,		/* reinit */
		NULL,		/* destroy */
		NULL,		/* load */
		NULL,		/* save */
		Edit
	},
	N_("Logic Probe"),
	"LPROBE",
	"Generic|Digital|Probes",
	&esIconLogicProbe,
	Draw,
	NULL,			/* instance_menu */
	NULL,			/* class_menu */
	NULL,			/* export */
	NULL			/* connect */
};
