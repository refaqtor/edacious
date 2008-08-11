/*
 * Copyright (c) 2008 
 *
 * Antoine Levitt (smeuuh@gmail.com)
 * Steven Herbst (herbst@mit.edu)
 *
 * Hypertriton, Inc. <http://hypertriton.com/>
 *
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
 * Independent square-wave voltage source.
 */

#include <core/core.h>
#include "sources.h"

const ES_Port esVSquarePorts[] = {
	{  0, "" },
	{  1, "v+" },
	{  2, "v-" },
	{ -1 },
};

static void
Stamp(ES_VSquare *vsq, ES_SimDC *dc)
{

	StampVoltageSource(VSOURCE(vsq)->v, VSOURCE(vsq)->s);
}

static int
DC_SimBegin(void *obj, ES_SimDC *dc)
{
	ES_VSquare *vsq = obj;
	ES_Vsource *vs = VSOURCE(vsq);

	Uint k = PNODE(vsq,1);
	Uint j = PNODE(vsq,2);

	vs->v = 0.0;

	InitStampVoltageSource(k,j, vs->vIdx, vs->s, dc);

	Stamp(vsq,dc);

	vsq->vPrev = vs->v;

	return (0);
}

static void
DC_StepBegin(void *obj, ES_SimDC *dc)
{
	ES_Vsource *vs = obj;
	ES_VSquare *vsq = obj;

#define my_modulus(x, y) (x - M_Floor(x/y) * y)
	if (my_modulus(dc->Telapsed, (vsq->tL + vsq->tH)) < vsq->tL)
		vs->v = vsq->vL;
	else
		vs->v = vsq->vH;
#undef my_modulus
	
	if (M_Fabs(vs->v - vsq->vPrev) > 0.5)
		dc->inputStep=1;

	vsq->vPrev = vs->v;

	Stamp(vsq,dc);
}

static void
DC_StepIter(void *obj, ES_SimDC *dc)
{
	ES_VSquare *vsq = obj;
	Stamp(vsq,dc);
}


static void
Init(void *p)
{
	ES_VSquare *vs = p;

	ES_InitPorts(vs, esVSquarePorts);
	vs->vH = 5.0;
	vs->vL = 0.0;
	vs->tH = 0.5;
	vs->tL = 0.5;
	COMPONENT(vs)->dcSimBegin = DC_SimBegin;
	COMPONENT(vs)->dcStepBegin = DC_StepBegin;
	COMPONENT(vs)->dcStepIter = DC_StepIter;
}

static int
Load(void *p, AG_DataSource *buf, const AG_Version *ver)
{
	ES_VSquare *vs = p;

	vs->vH = M_ReadReal(buf);
	vs->vL = M_ReadReal(buf);
	vs->tH = M_ReadReal(buf);
	vs->tL = M_ReadReal(buf);
	return (0);
}

static int
Save(void *p, AG_DataSource *buf)
{
	ES_VSquare *vs = p;

	M_WriteReal(buf, vs->vH);
	M_WriteReal(buf, vs->vL);
	M_WriteReal(buf, vs->tH);
	M_WriteReal(buf, vs->tL);
	return (0);
}

static void *
Edit(void *p)
{
	ES_VSquare *vs = p;
	AG_Box *box = AG_BoxNewVert(NULL, AG_BOX_EXPAND);

	M_NumericalNewReal(box, 0, "V", _("HIGH voltage: "), &vs->vH);
	M_NumericalNewReal(box, 0, "V", _("LOW voltage: "), &vs->vL);
	M_NumericalNewReal(box, 0, "s", _("HIGH duration: "), &vs->tH);
	M_NumericalNewReal(box, 0, "s", _("LOW duration: "), &vs->tL);
	return (box);
}

ES_ComponentClass esVSquareClass = {
	{
		"ES_Component:ES_Vsource:ES_VSquare",
		sizeof(ES_VSquare),
		{ 0,0 },
		Init,
		NULL,		/* free_dataset */
		NULL,		/* destroy */
		Load,
		Save,
		Edit
	},
	N_("Voltage source (square)"),
	"Vsq",
	"Sources/Vsquare.eschem",
	"Generic|Sources",
	&esIconVsquare,
	NULL,			/* draw */
	NULL,			/* instance_menu */
	NULL,			/* class_menu */
	NULL,			/* export */
	NULL			/* connect */
};
