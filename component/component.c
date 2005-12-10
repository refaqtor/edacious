/*	$Csoft: component.c,v 1.7 2005/09/27 03:34:08 vedge Exp $	*/

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

const AG_Version esComponentVer = {
	"agar-eda component",
	0, 0
};

void
ES_ComponentDestroy(void *p)
{
	ES_Component *com = p;
	int i;

	pthread_mutex_destroy(&com->lock);
	for (i = 0; i < com->npairs; i++) {
		ES_Pair *dip = &com->pairs[i];

		Free(dip->loops, M_EDA);
		Free(dip->lpols, M_EDA);
	}
	Free(com->pairs, M_EDA);
}

void
ES_ComponentSelect(ES_Component *com)
{
	com->selected = 1;
//	AG_ObjMgrOpenData(com, 1);
}

void
ES_ComponentUnselect(ES_Component *com)
{
	com->selected = 0;
//	AG_ObjMgrCloseData(com);
}
		
void
ES_ComponentUnselectAll(ES_Circuit *ckt)
{
	ES_Component *com;

	AGOBJECT_FOREACH_CHILD(com, ckt, es_component) {
		if (!AGOBJECT_SUBCLASS(com, "component")) {
			continue;
		}
		ES_ComponentUnselect(com);
	}
}

static void
ES_ComponentRenamed(AG_Event *event)
{
	ES_Component *com = AG_SELF();

	if (com->block == NULL)
		return;

	AG_ObjectCopyName(com, com->block->name, sizeof(com->block->name));
}

static void
ES_ComponentAttached(AG_Event *event)
{
	char blkname[VG_BLOCK_NAME_MAX];
	ES_Component *com = AG_SELF();
	ES_Circuit *ckt = AG_SENDER();
	VG *vg = ckt->vg;
	VG_Block *block;

	if (!AGOBJECT_SUBCLASS(ckt, "circuit")) {
		return;
	}
	com->ckt = ckt;
	
	AG_ObjectCopyName(com, blkname, sizeof(blkname));
#ifdef DEBUG
	if (com->block != NULL) { Fatal("Block exists"); }
	if (VG_GetBlock(vg, blkname) != NULL) { Fatal("Duplicate block"); }
#endif
	com->block = VG_BeginBlock(vg, blkname, 0);
	VG_Origin(vg, 0, com->ports[0].x, com->ports[0].y);
	VG_EndBlock(vg);

	if (AGOBJECT_SUBCLASS(com, "component.vsource")) {
		ckt->vsrcs = Realloc(ckt->vsrcs,
		    (ckt->m+1)*sizeof(ES_Vsource *));
		ckt->vsrcs[ckt->m] = (ES_Vsource *)com;
		ckt->m++;
	}
}

static void
ES_ComponentDetached(AG_Event *event)
{
	ES_Component *com = AG_SELF();
	ES_Circuit *ckt = AG_SENDER();
	u_int i, j;

	if (!AGOBJECT_SUBCLASS(ckt, "circuit"))
		return;
	
	if (AGOBJECT_SUBCLASS(com, "component.vsource")) {
		for (i = 0; i < ckt->m; i++) {
			if (ckt->vsrcs[i] == (ES_Vsource *)com) {
				if (i < --ckt->m) {
					for (; i < ckt->m; i++)
						ckt->vsrcs[i] = ckt->vsrcs[i+1];
				}
				break;
			}
		}
	}

	for (i = 0; i <= ckt->n; i++) {
		ES_Node *node = ckt->nodes[i];
		ES_Branch *br, *nbr;

		for (br = TAILQ_FIRST(&node->branches);
		     br != TAILQ_END(&node->branches);
		     br = nbr) {
			nbr = TAILQ_NEXT(br, branches);
			if (br->port != NULL && br->port->com == com) {
				ES_CircuitDelBranch(ckt, i, br);
			}
		}
	}

del_nodes:
	/* XXX widely inefficient */
	for (i = 1; i <= ckt->n; i++) {
		ES_Node *node = ckt->nodes[i];
		ES_Branch *br;

		if (node->nbranches == 0) {
			ES_CircuitDelNode(ckt, i);
			goto del_nodes;
		}
	}
	
	for (i = 1; i <= com->nports; i++) {
		ES_Port *port = &com->ports[i];

		port->branch = NULL;
		port->node = -1;
		port->selected = 0;
	}

	for (i = 0; i < com->npairs; i++)
		com->pairs[i].nloops = 0;

	if (com->block != NULL) {
		VG_DestroyBlock(ckt->vg, com->block);
		com->block = NULL;
	}
	com->ckt = NULL;
}

static void
ES_ComponentShown(AG_Event *event)
{
	ES_Component *com = AG_SELF();

	AG_AddTimeout(com, &com->redraw_to, 50);
}

static void
ES_ComponentHidden(AG_Event *event)
{
	ES_Component *com = AG_SELF();

	AG_LockTimeouts(com);
	if (AG_TimeoutIsScheduled(com, &com->redraw_to)) {
		AG_DelTimeout(com, &com->redraw_to);
	}
	AG_UnlockTimeouts(com);
}

/* Redraw a component schematic block. */
static Uint32
ES_ComponentRedraw(void *p, Uint32 ival, void *arg)
{
	ES_Component *com = p;
	VG_Block *block_save;
	VG_Element *element_save;
	VG *vg = com->ckt->vg;
	VG_Element *vge;
	int i;
	
	if ((AGOBJECT(com)->flags & AG_OBJECT_DATA_RESIDENT) == 0)
		return (ival);

#ifdef DEBUG
	if (com->block == NULL) { Fatal("Missing block"); }
#endif

	pthread_mutex_lock(&vg->lock);
	block_save = vg->cur_block;
	element_save = vg->cur_vge;
	VG_SelectBlock(vg, com->block);
	VG_ClearBlock(vg, com->block);

	if (com->ops->draw != NULL)
		com->ops->draw(com, vg);

	/* Indicate the nodes associated with connection points. */
	for (i = 1; i <= com->nports; i++) {
		ES_Port *port = &com->ports[i];
		float rho, theta;
		float x, y;

		VG_Car2Pol(vg, port->x, port->y, &rho, &theta);
		theta -= com->block->theta;
		VG_Pol2Car(vg, rho, theta, &x, &y);

		if (com->ckt->dis_nodes) {
			VG_Begin(vg, VG_CIRCLE);
			VG_Vertex2(vg, x, y);
			if (port->selected) {
				VG_Color3(vg, 255, 255, 165);
				VG_CircleRadius(vg, 0.1600);
			} else {
				VG_Color3(vg, 0, 150, 150);
				VG_CircleRadius(vg, 0.0625);
			}
			VG_End(vg);
		}
		if (com->ckt->dis_node_names &&
		    port->node >= 0) {
			VG_Begin(vg, VG_TEXT);
			VG_Vertex2(vg, x, y);
			VG_SetStyle(vg, "node-name");
			VG_Color3(vg, 0, 200, 100);
			VG_TextAlignment(vg, VG_ALIGN_BR);
			if (port->node == 0) {
				VG_Printf(vg, "gnd");
			} else {
				VG_Printf(vg, "n%d", port->node);
			}
			VG_End(vg);
		}
	}

	if (com->selected) {
		TAILQ_FOREACH(vge, &com->block->vges, vgbmbs) {
			VG_Select(vg, vge);
			/* TODO blend */
			VG_Color3(vg, 240, 240, 50);
			VG_End(vg);
		}
	} else if (com->highlighted) {
		TAILQ_FOREACH(vge, &com->block->vges, vgbmbs) {
			VG_Select(vg, vge);
			/* TODO blend */
			VG_Color3(vg, 180, 180, 120);
			VG_End(vg);
		}
	}

	if (com->block->theta != 0) {
		VG_RotateBlock(vg, com->block, com->block->theta);
	}
	VG_Select(vg, element_save);
	VG_SelectBlock(vg, block_save);
	pthread_mutex_unlock(&vg->lock);
	return (ival);
}

void
ES_ComponentInit(void *obj, const char *tname, const char *name,
    const void *ops, const ES_Port *pinout)
{
	char cname[AG_OBJECT_NAME_MAX];
	ES_Component *com = obj;
	const ES_Port *pinoutp;
	ES_Pair *dip;
	int i, j, k;

	strlcpy(cname, "component.", sizeof(cname));
	strlcat(cname, tname, sizeof(cname));
	AG_ObjectInit(com, cname, name, ops);
	com->ops = ops;
	com->ckt = NULL;
	com->block = NULL;
	com->nports = 0;
	com->selected = 0;
	com->highlighted = 0;
	com->Tnom = 27+273.15;
	com->Tspec = 27+273.15;
	pthread_mutex_init(&com->lock, &agRecursiveMutexAttr);

	/* Assign the origin and default pinout. */
	com->ports[0].x = pinout[0].x;
	com->ports[0].y = pinout[0].y;
	for (i = 1, pinoutp = &pinout[1];
	     i < COMPONENT_MAX_PORTS && pinoutp->n >= 0;
	     i++, pinoutp++) {
		ES_Port *port = &com->ports[i];

		strlcpy(port->name, pinoutp->name, sizeof(port->name));
		port->n = i;
		port->x = pinoutp->x;
		port->y = pinoutp->y;
		port->com = com;
		port->node = -1;
		port->branch = NULL;
		port->u = 0;
		port->selected = 0;
		com->nports++;
	}

	/* Find the port pairs . */
	com->pairs = Malloc(sizeof(ES_Pair), M_EDA);
	com->npairs = 0;
	for (i = 1; i <= com->nports; i++) {
		for (j = 1; j <= com->nports; j++) {
			if (i == j)
				continue;
			
			/* Skip redundant pairs . */
			for (k = 0; k < com->npairs; k++) {
				ES_Pair *dip = &com->pairs[k];

				if (dip->p2 == &com->ports[i] &&
				    dip->p1 == &com->ports[j]) {
					break;
				}
			}
			if (k < com->npairs)
				continue;

			com->pairs = Realloc(com->pairs,
			    (com->npairs+1)*sizeof(ES_Pair));
			dip = &com->pairs[com->npairs++];
			dip->com = com;
			dip->p1 = &com->ports[i];
			dip->p2 = &com->ports[j];
			dip->loops = Malloc(sizeof(ES_Loop *), M_EDA);
			dip->lpols = Malloc(sizeof(int), M_EDA);
			dip->nloops = 0;
		}
	}

	AG_SetEvent(com, "attached", ES_ComponentAttached, NULL);
	AG_SetEvent(com, "detached", ES_ComponentDetached, NULL);
	AG_SetEvent(com, "renamed", ES_ComponentRenamed, NULL);
	AG_SetEvent(com, "circuit-shown", ES_ComponentShown, NULL);
	AG_SetEvent(com, "circuit-hidden", ES_ComponentHidden, NULL);
	AG_SetTimeout(&com->redraw_to, ES_ComponentRedraw, NULL,
	    AG_TIMEOUT_LOADABLE);
}

int
ES_ComponentLoad(void *p, AG_Netbuf *buf)
{
	ES_Component *com = p;
	float Tnom, Tspec;

	if (AG_ReadVersion(buf, &esComponentVer, NULL) == -1)
		return (-1);

	com->flags = (u_int)AG_ReadUint32(buf);
	com->Tnom = AG_ReadFloat(buf);
	com->Tspec = AG_ReadFloat(buf);
	return (0);
}

int
ES_ComponentSave(void *p, AG_Netbuf *buf)
{
	ES_Component *com = p;

	AG_WriteVersion(buf, &esComponentVer);
	AG_WriteUint32(buf, com->flags);
	AG_WriteFloat(buf, com->Tnom);
	AG_WriteFloat(buf, com->Tspec);
	return (0);
}

/* Return the effective DC resistance of a pair. */
double
ES_PairResistance(ES_Pair *dip)
{
#ifdef DEBUG
	if (dip->p1 == dip->p2) { Fatal("p1 == p2"); }
#endif
	if (dip->com->ops->resistance == NULL) {
		return (0);
	}
	if (dip->p1->n < dip->p2->n) {
		return (dip->com->ops->resistance(dip->com, dip->p1, dip->p2));
	} else {
		return (dip->com->ops->resistance(dip->com, dip->p2, dip->p1));
	}
}

/* Return the effective capacitance of a pair. */
double
ES_PairCapacitance(ES_Pair *dip)
{
#ifdef DEBUG
	if (dip->p1 == dip->p2)
		fatal("p1 == p2");
#endif
	if (dip->com->ops->capacitance == NULL) {
		return (0);
	}
	if (dip->p1->n < dip->p2->n) {
		return (dip->com->ops->capacitance(dip->com, dip->p1, dip->p2));
	} else {
		return (dip->com->ops->capacitance(dip->com, dip->p2, dip->p1));
	}
}

/* Return the effective inductance of a pair. */
double
ES_PairInductance(ES_Pair *dip)
{
#ifdef DEBUG
	if (dip->p1 == dip->p2)
		fatal("p1 == p2");
#endif
	if (dip->com->ops->inductance == NULL) {
		return (0);
	}
	if (dip->p1->n < dip->p2->n) {
		return (dip->com->ops->inductance(dip->com, dip->p1, dip->p2));
	} else {
		return (dip->com->ops->inductance(dip->com, dip->p2, dip->p1));
	}
}

/*
 * Check whether a given pair is part of a given loop, and return
 * its polarity with respect to the loop direction.
 */
int
ES_PairIsInLoop(ES_Pair *dip, ES_Loop *loop, int *sign)
{
	unsigned int i;

	for (i = 0; i < dip->nloops; i++) {
		if (dip->loops[i] == loop) {
			*sign = dip->lpols[i];
			return (1);
		}
	}
	return (0);
}

#ifdef EDITION
void *
ES_ComponentEdit(void *p)
{
	ES_Component *com = p;
	AG_Window *win;

	if (com->ops->edit == NULL) {
		return (NULL);
	}
	win = com->ops->edit(com);
	AG_WindowSetPosition(win, AG_WINDOW_MIDDLE_LEFT, 0);
	AG_WindowSetCaption(win, "%s: %s", com->ops->name, AGOBJECT(com)->name);
	AG_WindowSetCloseAction(win, AG_WINDOW_DETACH);
	AG_WindowShow(win);
	return (win);
}

/* Insert a new, floating component into a circuit. */
void
ES_ComponentInsert(AG_Event *event)
{
	extern ES_Component *esInscomCur;
	char name[AG_OBJECT_NAME_MAX];
	VG_View *vgv = AG_PTR(1);
	AG_Tlist *tl = AG_PTR(2);
	ES_Circuit *ckt = AG_PTR(3);
	AG_TlistItem *it;
	AG_ObjectType *comtype;
	ES_ComponentOps *comops;
	ES_Component *com;
	VG_Tool *t;
	int n = 1;

	if ((it = AG_TlistSelectedItem(tl)) == NULL) {
		AG_TextMsg(AG_MSG_ERROR, _("No component type is selected."));
		return;
	}
	comtype = it->p1;
	comops = (ES_ComponentOps *)comtype->ops;
tryname:
	snprintf(name, sizeof(name), "%s%d", comops->pfx, n++);
	AGOBJECT_FOREACH_CHILD(com, ckt, es_component) {
		if (strcmp(AGOBJECT(com)->name, name) == 0)
			break;
	}
	if (com != NULL)
		goto tryname;

	com = Malloc(comtype->size, M_OBJECT);
	comtype->ops->init(com, name);
	com->flags |= COMPONENT_FLOATING;
	AG_ObjectAttach(ckt, com);
	AG_ObjectUnlinkDatafiles(com);
	AG_ObjectPageIn(com, AG_OBJECT_DATA);
//	AG_ObjectSave(com);
	AG_PostEvent(ckt, com, "circuit-shown", NULL);

	if ((t = VG_ViewFindTool(vgv, "Insert component")) != NULL) {
		VG_ViewSelectTool(vgv, t, ckt);
		esInscomCur = com;
		esInscomCur->selected = 1;
	}
	AG_WidgetFocus(vgv);
	AG_WindowFocus(AG_WidgetParentWindow(vgv));
}

/*
 * Evaluate whether the given point collides with a port in the schematic
 * (that does not belong to the given component if specified).
 */
ES_Port *
ES_ComponentPortOverlap(ES_Circuit *ckt, ES_Component *ncom, float x,
    float y)
{
	ES_Component *ocom;
	int i;
	
	/* XXX use bounding boxes */
	AGOBJECT_FOREACH_CHILD(ocom, ckt, es_component) {
		if (!AGOBJECT_SUBCLASS(ocom, "component") ||
		    ocom == ncom ||
		    ocom->flags & COMPONENT_FLOATING) {
			continue;
		}
		for (i = 1; i <= ocom->nports; i++) {
			ES_Port *oport = &ocom->ports[i];

			if (fabs(x-ocom->block->pos.x - oport->x) < 0.25 &&
			    fabs(y-ocom->block->pos.y - oport->y) < 0.25)
				return (oport);
		}
	}
	return (NULL);
}

/* Evaluate whether the given port is connected to the reference point. */
int
ES_PortIsGrounded(ES_Port *port)
{
	ES_Circuit *ckt = port->com->ckt;

	return (ES_NodeGrounded(ckt, port->node));
}

/* Connect a floating component to the circuit. */
void
ES_ComponentConnect(ES_Circuit *ckt, ES_Component *com, VG_Vtx *vtx)
{
	ES_Port *oport;
	ES_Branch *br;
	int i;

	for (i = 1; i <= com->nports; i++) {
		ES_Port *port = &com->ports[i];

		oport = ES_ComponentPortOverlap(ckt, com,
		    vtx->x + port->x,
		    vtx->y + port->y);
		if (com->ops->connect == NULL ||
		    com->ops->connect(com, port, oport) == -1) {
			if (oport != NULL) {
#ifdef DEBUG
				if (port->node > 0)
					fatal("spurious node");
#endif
				port->node = oport->node;
				port->branch = ES_CircuitAddBranch(ckt,
				    oport->node, port);
			} else {
				port->node = ES_CircuitAddNode(ckt, 0);
				port->branch = ES_CircuitAddBranch(ckt,
				    port->node, port);
			}
		}
		port->selected = 0;
	}
	com->flags &= ~(COMPONENT_FLOATING);

	ES_CircuitModified(ckt);
}

/*
 * Highlight the connection points of the given component with respect
 * to other components in the circuit and return the number of matches.
 */
int
ES_ComponentHighlightPorts(ES_Circuit *ckt, ES_Component *com)
{
	ES_Port *oport;
	int i, nconn = 0;

	for (i = 1; i <= com->nports; i++) {
		ES_Port *nport = &com->ports[i];

		oport = ES_ComponentPortOverlap(ckt, com,
		    com->block->pos.x + nport->x,
		    com->block->pos.y + nport->y);
		if (oport != NULL) {
			nport->selected = 1;
			nconn++;
		} else {
			nport->selected = 0;
		}
	}
	return (nconn);
}

void
ES_ComponentRegMenu(AG_Menu *m, AG_MenuItem *pitem, ES_Circuit *ckt)
{
	/* Nothing yet */
}
#endif /* EDITION */
