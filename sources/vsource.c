/*
 * Copyright (c) 2005-2020 Julien Nadeau Carriere (vedge@csoft.net)
 * Copyright (c) 2008 Antoine Levitt (smeuuh@gmail.com)
 * Copyright (c) 2008 Steven Herbst (herbst@mit.edu)
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
 * Independent voltage source
 */

#include <core/core.h>
#include "sources.h"

const ES_Port esVsourcePorts[] = {
	{ 0, "" },
	{ 1, "v+" },
	{ 2, "v-" },
	{ -1 },
};

/* Find the pairs connecting two contiguous ports, and its polarity. */
static int
FindContigPair(ES_Port *pA, ES_Port *pB, ES_Pair **Rdip, int *Rpol)
{
	ES_Component *com = pA->com;
	ES_Circuit *ckt = com->ckt;
	ES_Pair *dip;
	Uint i;

	COMPONENT_FOREACH_PAIR(dip, i, com) {
		ES_Node *node;
		ES_Branch *br;
		int pol;

		/*
		 * As a convention, the relative polarities of pairs is
		 * determined by the port number order.
		 */
		if (dip->p1 == pA && dip->p2->node >= 0) {
			node = ckt->nodes[dip->p2->node];
			pol = -1;
		} else if (dip->p2 == pA && dip->p1->node >= 0) {
			node = ckt->nodes[dip->p1->node];
			pol = +1;
		} else {
			continue;
		}

		NODE_FOREACH_BRANCH(br, node) {
			if (br->port == pB)
				break;
		}
		if (br != NULL) {
			*Rdip = dip;
			*Rpol = pol;
			break;
		}
	}
	return (i < com->npairs);
}

/*
 * Insert a new voltage source loop based on the ports currently in the
 * loop stack.
 */
static void
InsertVoltageLoop(ES_Vsource *vs)
{
	ES_Loop *lnew;
	Uint i;

	/* Create a new voltage source loop entry. */
	lnew = Malloc(sizeof(ES_Loop));
	lnew->name = 1+vs->nLoops++;
	lnew->pairs = Malloc((vs->nlstack-1)*sizeof(ES_Pair *));
	lnew->npairs = 0;
	lnew->origin = vs;

	for (i = 1; i < vs->nlstack; i++) {
		ES_Pair *pair = NULL;
		int pol = 0;

		if (!FindContigPair(vs->lstack[i], vs->lstack[i-1], &pair,&pol))
			continue;

		lnew->pairs[lnew->npairs++] = pair;

		pair->loops = Realloc(pair->loops,
		    (pair->nLoops+1)*sizeof(ES_Loop *));
		pair->loopPolarity = Realloc(pair->loopPolarity,
		    (pair->nLoops+1)*sizeof(int));
		pair->loops[pair->nLoops] = lnew;
		pair->loopPolarity[pair->nLoops] = pol;
		pair->nLoops++;
	}
	TAILQ_INSERT_TAIL(&vs->loops, lnew, loops);
}

/*
 * Isolate closed loops with respect to a voltage source.
 *
 * The resulting loop structure is an array of pairs whose polarities
 * with respect to the loop are recorded in a separate array.
 */
static void
FindLoops(ES_Vsource *vs, ES_Port *portCur)
{
	ES_Circuit *ckt = COMPONENT(vs)->ckt;
	ES_Node *nodeCur = ckt->nodes[portCur->node], *nodeNext;
	ES_Port *portNext;
	ES_Branch *br;
	Uint i;

	nodeCur->flags |= CKTNODE_EXAM;

	vs->lstack = Realloc(vs->lstack, (vs->nlstack+1)*sizeof(ES_Port *));
	vs->lstack[vs->nlstack++] = portCur;

	NODE_FOREACH_BRANCH(br, nodeCur) {
		if (br->port == portCur) {
			continue;
		}
		if (br->port->com == COMPONENT(vs) &&
		    br->port->n == 2 &&
		    vs->nlstack > 2) {
			vs->lstack = Realloc(vs->lstack,
			    (vs->nlstack+1)*sizeof(ES_Port *));
			vs->lstack[vs->nlstack++] = &COMPONENT(vs)->ports[2];
			InsertVoltageLoop(vs);
			vs->nlstack--;
			continue;
		}
		if (br->port->com == NULL) {
			continue;
		}
		COMPONENT_FOREACH_PORT(portNext, i, br->port->com) {
			nodeNext = ckt->nodes[portNext->node];
			if ((portNext == portCur) ||
			    (portNext->com == br->port->com &&
			     portNext->n == br->port->n) ||
			    (nodeNext->flags & CKTNODE_EXAM)) {
				continue;
			}
			FindLoops(vs, portNext);
		}
	}

	vs->nlstack--;
	nodeCur->flags &= ~(CKTNODE_EXAM);
}

/*
 * Compute all possible loops relative to the given voltage source. This
 * function only works with subclasses of Vsource.
 */
void
ES_VsourceFindLoops(ES_Vsource *vs)
{
	vs->lstack = Malloc(sizeof(ES_Port *));
	FindLoops(vs, PORT(vs,1));

	Free(vs->lstack);
	vs->lstack = NULL;
	vs->nlstack = 0;
}

static __inline__ void
Stamp(ES_Vsource *vs, ES_SimDC *dc)
{
	StampVoltageSource(vs->v,vs->s);
}

static int
DC_SimBegin(void *obj, ES_SimDC *dc)
{
	ES_Vsource *vs = obj;
	Uint k = PNODE(vs,1);
	Uint j = PNODE(vs,2);

	InitStampVoltageSource(k, j, vs->vIdx, vs->s, dc);
	
	Stamp(vs, dc);

	return (0);
}

static void
DC_StepBegin(void *obj, ES_SimDC *dc)
{
	ES_Vsource *vs = obj;

	Stamp(vs, dc);
}

static void
DC_StepIter(void *obj, ES_SimDC *dc)
{
	ES_Vsource *vs = obj;

	Stamp(vs, dc);
}

static void
Connected(AG_Event *event)
{
	ES_Vsource *vs = ES_VSOURCE_SELF();
	ES_Circuit *ckt = ES_CIRCUIT_PTR(1);

	vs->vIdx = ES_AddVoltageSource(ckt, vs);
}

static void
Disconnected(AG_Event *event)
{
	ES_Vsource *vs = ES_VSOURCE_SELF();
	ES_Circuit *ckt = ES_CIRCUIT_PTR(1);

	ES_DelVoltageSource(ckt, vs->vIdx);
}

static void
Init(void *p)
{
	ES_Vsource *vs = p;

	ES_InitPorts(vs, esVsourcePorts);
	vs->vIdx = -1;
	vs->v = 5;
	
	TAILQ_INIT(&vs->loops);
	vs->nLoops = 0;
	vs->lstack = NULL;
	vs->nlstack = 0;

	COMPONENT(vs)->dcSimBegin = DC_SimBegin;
	COMPONENT(vs)->dcStepBegin = DC_StepBegin;
	COMPONENT(vs)->dcStepIter = DC_StepIter;

	AG_SetEvent(vs, "circuit-connected", Connected, NULL);
	AG_SetEvent(vs, "circuit-disconnected", Disconnected, NULL);
	
	M_BindReal(vs, "v", &vs->v);
}

static void
FreeDataset(void *p)
{
	ES_Vsource *vs = p;

	ES_VsourceFreeLoops(vs);
}

void
ES_VsourceFreeLoops(ES_Vsource *vs)
{
	ES_Loop *loop, *nloop;

	for (loop = TAILQ_FIRST(&vs->loops);
	     loop != TAILQ_END(&vs->loops);
	     loop = nloop) {
		nloop = TAILQ_NEXT(loop, loops);
		Free(loop->pairs);
		Free(loop);
	}
	TAILQ_INIT(&vs->loops);
	vs->nLoops = 0;

	Free(vs->lstack);
	vs->lstack = NULL;
	vs->nlstack = 0;
}

static int
Export(void *p, enum circuit_format fmt, FILE *f)
{
	ES_Vsource *vs = p;

	if (PNODE(vs,1) == -1 ||
	    PNODE(vs,2) == -1)
		return (0);

	switch (fmt) {
	case CIRCUIT_SPICE3:
		fprintf(f, "%s %d %d %g\n", OBJECT(vs)->name,
		    PNODE(vs,1), PNODE(vs,2), vs->v);
		break;
	}
	return (0);
}

double
ES_VsourceVoltage(void *p, int p1, int p2)
{
	ES_Vsource *vs = p;

	return (p1 == 1 ? vs->v : -(vs->v));
}

#if 0
static void
PollLoops(AG_Event *event)
{
	char text[AG_TLIST_LABEL_MAX];
	ES_Vsource *vs = ES_VSOURCE_PTR(1);
	AG_Tlist *tl = AG_TLIST_SELF();
	AG_TlistItem *it;
	ES_Loop *l;
	Uint i;

	AG_TlistClear(tl);

	it = AG_TlistAdd(tl, NULL, "I(%d)=%.04fA", vs->vIdx,
	    ES_BranchCurrent(COMCIRCUIT(vs), vs->vIdx));
	it->depth = 0;

	TAILQ_FOREACH(l, &vs->loops, loops) {
		it = AG_TlistAdd(tl, NULL, "L%u", l->name);
		it->p1 = l;
		it->depth = 0;

		for (i = 0; i < l->npairs; i++) {
			ES_Pair *pair = l->pairs[i];

			Snprintf(text, sizeof(text), "%s:(%s,%s)",
			    OBJECT(pair->com)->name, pair->p1->name,
			    pair->p2->name);
			it = AG_TlistAddPtr(tl, NULL, text, pair);
			it->depth = 1;
		}
	}
	AG_TlistRestore(tl);
}
#endif

static void *
Edit(void *p)
{
	ES_Vsource *vs = p;
	AG_Box *box = AG_BoxNewVert(NULL, AG_BOX_EXPAND);
/*	AG_Tlist *tl; */

	M_NumericalNewReal(box, 0, "V", _("Voltage: "), &vs->v);
	
	if (COMCIRCUIT(vs) != NULL) {
		AG_SeparatorNewHoriz(box);
		AG_LabelNewPolled(box, AG_LABEL_HFILL,
		    _("Entry in matrix: %i"), &vs->vIdx);
#if 0
		AG_SeparatorNewHoriz(box);
		AG_LabelNew(box, 0, _("Voltage loops:"));
		tl = AG_TlistNewPolled(box, AG_TLIST_TREE|AG_TLIST_EXPAND,
		    PollLoops, "%p", vs);
		AG_TlistSizeHint(tl, "< I(X) = 0.00000A   >", 5);
#endif
	}
	return (box);
}

ES_ComponentClass esVsourceClass = {
	{
		"Edacious(Circuit:Component:Vsource)"
		"@sources",
		sizeof(ES_Vsource),
		{ 0,0 },
		Init,
		FreeDataset,
		NULL,		/* destroy */
		NULL,		/* load */
		NULL,		/* save */
		Edit
	},
	N_("Voltage source"),
	"V",
	"Generic|Sources",
	&esIconVsource,
	NULL,			/* draw */
	NULL,			/* instance_menu */
	NULL,			/* class_menu */
	Export,
	NULL			/* connect */
};
