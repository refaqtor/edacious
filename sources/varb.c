/*
 * Copyright (c) 2008 Antoine Levitt (smeuuh@gmail.com)
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
 * Independent voltage source with voltage defined using arbitrary expression.
 */

#include <core/core.h>
#include <core/interpreteur.h>

#include "sources.h"

#include <agar/config/ag_threads.h>

PARAM params[] = { {"pi" , M_PI},
		   {"t"  , 0.0}
};
const ES_Port esVArbPorts[] = {
	{  0, "" },
	{  1, "v+" },
	{  2, "v-" },
	{ -1 },
};

static __inline__ void 
Stamp(ES_VArb *va, ES_SimDC *dc)
{
	StampVoltageSource(ESVSOURCE(va)->v, ESVSOURCE(va)->s);
}

static int
DC_SimBegin(void *obj, ES_SimDC *dc)
{
	ES_VArb *va = obj;
	ES_Vsource *vs = ESVSOURCE(va);

	Uint k = PNODE(va,1);
	Uint j = PNODE(va,2);

	/* Calculate initial voltage */
	params[1].Value = 0.0;
	Calculer(va->exp, params, (sizeof(params)/sizeof(params[0])),
	    &(vs->v));

	InitStampVoltageSource(k,j, vs->vIdx, vs->s, dc);

	Stamp(va,dc);

	va->vPrev = vs->v;

	return (0);
}

static void
DC_StepBegin(void *obj, ES_SimDC *dc)
{
	ES_VArb *va = obj;
	ES_Vsource *vs = ESVSOURCE(va);
	M_Real res;
	int ret;

	params[1].Value = dc->Telapsed;
	InterpreteurReset();
	ret = Calculer(va->exp, params, (sizeof(params)/sizeof(params[0])),
	    &res);
	if (ret != EVALUER_SUCCESS) {
		printf("Error !");
	}
	vs->v = res; 

	if (M_Fabs(vs->v-va->vPrev) > 0.5)
		dc->inputStep=1;

	va->vPrev=vs->v;

	Stamp(va,dc);
}

static void
DC_StepIter(void *obj, ES_SimDC *dc)
{
	ES_VArb *va = obj;
	Stamp(va, dc);
}

static void
Init(void *p)
{
	ES_VArb *va = p;

	ES_InitPorts(va, esVArbPorts);

	Strlcpy(va->exp, "sin(2*pi*t)", sizeof(va->exp));
	va->flags = 0;
	InterpreteurInit();
	
	COMPONENT(va)->dcSimBegin = DC_SimBegin;
	COMPONENT(va)->dcStepBegin = DC_StepBegin;
	COMPONENT(va)->dcStepIter = DC_StepIter;

	AG_BindString(va, "expr", va->exp, sizeof(va->exp));
}

static void
UpdatePlot(AG_Event *event)
{
	M_Plot *pl = AG_PTR(1);
	ES_VArb *va = AG_PTR(2);
	M_Real i, v = 0.0;
	int ret;

	M_PlotClear(pl);
	pl->flags &= ~(ES_VARB_ERROR);
	for (i = 0.0; i < 1.5; i += 0.01) {
		params[1].Value = i;
		InterpreteurReset();
		ret = Calculer(va->exp, params,
		    (sizeof(params)/sizeof(params[0])), &v);
		if (ret != EVALUER_SUCCESS) {
			va->flags |= ES_VARB_ERROR;
			return;
		}
		M_PlotReal(pl, v);
	}
}

static void *
Edit(void *p)
{
	ES_VArb *va = p;
	AG_Box *box = AG_BoxNewVert(NULL, AG_BOX_EXPAND);
	M_Plotter *ptr;
	M_Plot *pl;
	AG_Textbox *tb;

	AG_LabelNewPolledMT(box, 0, &OBJECT(va)->lock,
	    _("Effective voltage: %[R]v"), &ESVSOURCE(va)->v);
	
	tb = AG_TextboxNewS(box, 0, "v(t) = ");
	AG_TextboxBindASCII(tb, va->exp, sizeof(va->exp));
	AG_SeparatorNewHoriz(box);

	ptr = M_PlotterNew(box, M_PLOTTER_EXPAND);
	M_PlotterSizeHint(ptr, 100, 50);
	pl = M_PlotNew(ptr, M_PLOT_LINEAR);
	M_PlotSetLabel(pl, "v(t)");
	M_PlotSetScale(pl, 0.0, 16.0);

	AG_SetEvent(tb, "textbox-return", UpdatePlot, "%p,%p", pl, va);
	AG_PostEvent(tb, "textbox-return", NULL);
	return (box);
}

ES_ComponentClass esVArbClass = {
	{
		"Edacious(Circuit:Component:Vsource:VArb)"
		"@sources",
		sizeof(ES_VArb),
		{ 0,0 },
		Init,
		NULL,		/* reinit */
		NULL,		/* destroy */
		NULL,		/* load */
		NULL,		/* save */
		Edit
	},
	N_("Voltage source (expression)"),
	"Varb",
	"Generic|Sources",
	&esIconVarb,
	NULL,			/* draw */
	NULL,			/* instance_menu */
	NULL,			/* class_menu */
	NULL,			/* export */
	NULL			/* connect */
};
