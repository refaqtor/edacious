/*
 * Copyright (c) 2008 Antoine Levitt (smeuuh@gmail.com)
 * Copyright (c) 2008 Steven Herbst (herbst@mit.edu)
 * Copyright (c) 2009 Julien Nadeau (vedge@hypertriton.com)
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
 * Model for NMOS transistor
 */

#include <core/core.h>
#include "generic.h"

enum {
	PORT_G = 1,
	PORT_D = 2,
	PORT_S = 3
};
const ES_Port esNMOSPorts[] = {
	{ 0, "" },
	{ PORT_G, "G" },	/* Gate */
	{ PORT_D, "D" },	/* Drain */
	{ PORT_S, "S"},		/* Source */
	{ -1 },
};

static __inline__ M_Real
VdsPrevStep(ES_NMOS *u)
{
	return V_PREV_STEP(u,PORT_D,1) - V_PREV_STEP(u,PORT_S,1);
}
static __inline__ M_Real
VgsPrevStep(ES_NMOS *u)
{
	return V_PREV_STEP(u,PORT_G,1) - V_PREV_STEP(u,PORT_S,1);
}

static M_Real
vDS(ES_NMOS *u)
{
	return VPORT(u,PORT_D) - VPORT(u,PORT_S);
}
static M_Real
vGS(ES_NMOS *u)
{
	return VPORT(u,PORT_G) - VPORT(u,PORT_S);
}

static void
UpdateModel(ES_NMOS *u, M_Real vGS, M_Real vDS)
{
	M_Real I;

	if (vGS < u->Vt) {					/* Cutoff */
		I = 0;
		u->gm = 0;
		u->go = 0;
	} else if ((vGS-u->Vt) < vDS) {				/* Saturation */
		M_Real vSat = vGS-u->Vt;

		I = (u->K)/2.0*(vSat*vSat)*(1.0 + (vDS - vSat)/u->Va);
		u->gm = Sqrt(2.0*(u->K)*I);
		u->go = I/u->Va;
	} else {						/* Triode */
		I = (u->K)*((vGS - (u->Vt)) - vDS/2)*vDS;
		u->gm = (u->K)*vDS;
		u->go = (u->K)*((vGS - (u->Vt)) - vDS);
	}

	u->Ieq = I - u->gm*vGS - u->go*vDS;
}

static __inline__ void
Stamp(ES_NMOS *u, ES_SimDC *dc)
{
	StampVCCS(u->gm, u->s_vccs);
	StampConductance(u->go, u->s_conductance);
	StampCurrentSource(u->Ieq, u->s_current);
}

static int
DC_SimBegin(void *obj, ES_SimDC *dc)
{
	ES_NMOS *u = obj;
	Uint g = PNODE(u,PORT_G);
	Uint d = PNODE(u,PORT_D);
	Uint s = PNODE(u,PORT_S);

	InitStampVCCS(g,s,d,s, u->s_vccs, dc);
	InitStampConductance(d,s, u->s_conductance, dc);
	InitStampCurrentSource(s,d, u->s_current, dc);

	u->gm = 0.0;
	u->go = 1.0;
	u->Ieq = 0.0;
	Stamp(u,dc);

	return (0);
}

static void
DC_StepBegin(void *obj, ES_SimDC *dc)
{
	ES_NMOS *u = obj;

	UpdateModel(u,VgsPrevStep(u),VdsPrevStep(u));
	Stamp(u,dc);
}

static void
DC_StepIter(void *obj, ES_SimDC *dc)
{
        ES_NMOS *u = obj;

	UpdateModel(u,vGS(u),vDS(u));
	Stamp(u,dc);
}

static void
Init(void *p)
{
	ES_NMOS *u = p;

	ES_InitPorts(u, esNMOSPorts);
	u->Vt = 0.5;
	u->Va = 10;
	u->K = 1e-3;

	COMPONENT(u)->dcSimBegin = DC_SimBegin;
	COMPONENT(u)->dcStepBegin = DC_StepBegin;
	COMPONENT(u)->dcStepIter = DC_StepIter;

	M_BindReal(u, "Vt", &u->Vt);
	M_BindReal(u, "Va", &u->Va);
	M_BindReal(u, "K", &u->K);
}

static void *
Edit(void *p)
{
	ES_NMOS *u = p;
	AG_Box *box = AG_BoxNewVert(NULL, AG_BOX_EXPAND);

	M_NumericalNewRealPNZ(box, 0, "V", _("Threshold voltage: "), &u->Vt);
	M_NumericalNewRealPNZ(box, 0, "V", _("Early voltage: "), &u->Va);
	M_NumericalNewRealPNZ(box, 0, NULL, _("K: "), &u->K);

	return (box);
}

ES_ComponentClass esNMOSClass = {
	{
		"Edacious(Circuit:Component:NMOS)"
		"@generic",
		sizeof(ES_NMOS),
		{ 0,0 },
		Init,
		NULL,		/* reinit */
		NULL,		/* destroy */
		NULL,		/* load */
		NULL,		/* save */
		Edit		/* edit */
	},
	N_("Transistor (NMOS)"),
	"U",
	"Generic|Nonlinear",
	&esIconNMOS,
	NULL,			/* draw */
	NULL,			/* instance_menu */
	NULL,			/* class_menu */
	NULL,			/* export */
	NULL			/* connect */
};
