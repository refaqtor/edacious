/*	$Csoft: circuit.c,v 1.11 2005/09/29 00:22:33 vedge Exp $	*/

/*
 * Copyright (c) 2004, 2005 CubeSoft Communications Inc
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
#include <agar/gui.h>
#include <agar/vg.h>

#include "eda.h"
#include "spice.h"

const AG_Version esCircuitVer = {
	"agar-eda circuit",
	0, 0
};

const AG_ObjectOps esCircuitOps = {
	ES_CircuitInit,
	ES_CircuitReinit,
	ES_CircuitDestroy,
	ES_CircuitLoad,
	ES_CircuitSave,
	ES_CircuitEdit
};

static void init_node(ES_Node *, u_int);
static void init_ground(struct circuit *);

static void
ES_CircuitDetached(AG_Event *event)
{
	struct circuit *ckt = AG_SELF();

	if (ckt->sim != NULL) {
		sim_destroy(ckt->sim);
		ckt->sim = NULL;
	}
}

static void
ES_CircuitOpened(AG_Event *event)
{
	struct circuit *ckt = AG_SELF();
	ES_Component *com;

	AGOBJECT_FOREACH_CHILD(com, ckt, es_component) {
		if (!AGOBJECT_SUBCLASS(com, "component"))
			continue;

		AG_PostEvent(ckt, com, "circuit-shown", NULL);
		AG_ObjectPageIn(com, AG_OBJECT_DATA);
	}
}

static void
ES_CircuitClosed(AG_Event *event)
{
	struct circuit *ckt = AG_SELF();
	ES_Component *com;

	if (ckt->sim != NULL) {
		sim_destroy(ckt->sim);
		ckt->sim = NULL;
	}

	AGOBJECT_FOREACH_CHILD(com, ckt, es_component) {
		if (!AGOBJECT_SUBCLASS(com, "component"))
			continue;

		AG_PostEvent(ckt, com, "circuit-hidden", NULL);
		AG_ObjectPageOut(com, AG_OBJECT_DATA);
	}
}

/* Effect a change in the circuit topology.  */
void
ES_CircuitModified(struct circuit *ckt)
{
	ES_Component *com;
	ES_Vsource *vs;
	ES_Loop *loop;
	u_int i;

	/* Regenerate loop and pair information. */
	AGOBJECT_FOREACH_CHILD(com, ckt, es_component) {
		if (!AGOBJECT_SUBCLASS(com, "component") ||
		    com->flags & COMPONENT_FLOATING)
			continue;

		for (i = 0; i < com->npairs; i++)
			com->pairs[i].nloops = 0;
	}
	AGOBJECT_FOREACH_CHILD(vs, ckt, es_vsource) {
		if (!AGOBJECT_SUBCLASS(vs, "component.vsource") ||
		    COM(vs)->flags & COMPONENT_FLOATING ||
		    PORT(vs,1)->node == -1 ||
		    PORT(vs,2)->node == -1) {
			continue;
		}
		ES_VsourceFreeLoops(vs);
		ES_VsourceFindLoops(vs);
	}
#if 0
	AGOBJECT_FOREACH_CHILD(com, ckt, es_component)
		AG_PostEvent(ckt, com, "circuit-modified", NULL);
#endif
	if (ckt->loops == NULL) {
		ckt->loops = Malloc(sizeof(ES_Loop *), M_EDA);
	}
	ckt->l = 0;
	AGOBJECT_FOREACH_CHILD(vs, ckt, es_vsource) {
		if (!AGOBJECT_SUBCLASS(vs, "component.vsource") ||
		    COM(vs)->flags & COMPONENT_FLOATING) {
			continue;
		}
		TAILQ_FOREACH(loop, &vs->loops, loops) {
			ckt->loops = Realloc(ckt->loops,
			    (ckt->l+1)*sizeof(ES_Loop *));
			ckt->loops[ckt->l++] = loop;
			loop->name = ckt->l;
		}
	}

	if (ckt->sim != NULL &&
	    ckt->sim->ops->cktmod != NULL)
		ckt->sim->ops->cktmod(ckt->sim, ckt);
}

void
ES_CircuitInit(void *p, const char *name)
{
	struct circuit *ckt = p;
	VG_Style *vgs;
	VG *vg;

	AG_ObjectInit(ckt, "circuit", name, &esCircuitOps);
	ckt->descr[0] = '\0';
	ckt->sim = NULL;
	ckt->dis_nodes = 1;
	ckt->dis_node_names = 1;
	pthread_mutex_init(&ckt->lock, &agRecursiveMutexAttr);

	ckt->loops = NULL;
	ckt->vsrcs = NULL;
	ckt->m = 0;
	
	ckt->nodes = Malloc(sizeof(ES_Node *), M_EDA);
	ckt->n = 0;
	init_ground(ckt);

	vg = ckt->vg = VG_New(VG_VISGRID);
	strlcpy(vg->layers[0].name, _("Schematic"), sizeof(vg->layers[0].name));
	VG_Scale(vg, 11.0, 8.5, 32.0);
	VG_DefaultScale(vg, 2.0);
	VG_SetGridGap(vg, 0.25);
	vg->origin[0].x = 5.5;
	vg->origin[0].y = 4.25;
	VG_OriginRadius(vg, 2, 0.09375);
	VG_OriginColor(vg, 2, 255, 255, 165);

	vgs = VG_CreateStyle(vg, VG_TEXT_STYLE, "component-name");
	vgs->vg_text_st.size = 10;
	
	vgs = VG_CreateStyle(vg, VG_TEXT_STYLE, "node-name");
	vgs->vg_text_st.size = 8;
	
	AG_SetEvent(ckt, "edit-open", ES_CircuitOpened, NULL);
	AG_SetEvent(ckt, "edit-close", ES_CircuitClosed, NULL);
}

void
ES_CircuitReinit(void *p)
{
	struct circuit *ckt = p;
	ES_Component *com;
	u_int i;
	
	if (ckt->sim != NULL) {
		sim_destroy(ckt->sim);
		ckt->sim = NULL;
	}

	Free(ckt->loops, M_EDA);
	ckt->loops = NULL;
	ckt->l = 0;

	Free(ckt->vsrcs, M_EDA);
	ckt->vsrcs = NULL;
	ckt->m = 0;
	
	for (i = 0; i <= ckt->n; i++) {
		ES_CircuitDestroyNode(ckt->nodes[i]);
		Free(ckt->nodes[i], M_EDA);
	}
	ckt->nodes = Realloc(ckt->nodes, sizeof(ES_Node *));
	ckt->n = 0;
	init_ground(ckt);

	AGOBJECT_FOREACH_CHILD(com, ckt, es_component) {
		if (!AGOBJECT_SUBCLASS(com, "component") ||
		    com->flags & COMPONENT_FLOATING) {
			continue;
		}
		for (i = 1; i <= com->nports; i++) {
			ES_Port *port = &com->ports[i];

			port->node = -1;
			port->branch = NULL;
			port->selected = 0;
			port->u = 0;
		}
		com->block = NULL;
	}
	VG_Reinit(ckt->vg);
}

int
ES_CircuitLoad(void *p, AG_Netbuf *buf)
{
	struct circuit *ckt = p;
	Uint32 ncoms;
	u_int i, j, nnodes;

	if (AG_ReadVersion(buf, &esCircuitVer, NULL) != 0) {
		return (-1);
	}
	AG_CopyString(ckt->descr, buf, sizeof(ckt->descr));
	AG_ReadUint32(buf);					/* Pad */

	/* Load the circuit nodes (including ground). */
	nnodes = (u_int)AG_ReadUint32(buf);
	for (i = 0; i <= nnodes; i++) {
		int nbranches, flags;
		u_int name;

		flags = (int)AG_ReadUint32(buf);
		nbranches = (int)AG_ReadUint32(buf);
		if (i == 0) {
			name = 0;
			ES_CircuitDestroyNode(ckt->nodes[0]);
			Free(ckt->nodes[0], M_EDA);
			init_ground(ckt);
		} else {
			name = ES_CircuitAddNode(ckt, flags & ~(CKTNODE_EXAM));
		}
		for (j = 0; j < nbranches; j++) {
			char ppath[AG_OBJECT_PATH_MAX];
			ES_Component *pcom;
			ES_Branch *br;
			int pport;

			AG_CopyString(ppath, buf, sizeof(ppath));
			AG_ReadUint32(buf);			/* Pad */
			pport = (int)AG_ReadUint32(buf);
			if ((pcom = AG_ObjectFind(ppath)) == NULL) {
				AG_SetError("%s", AG_GetError());
				return (-1);
			}
			if (pport > pcom->nports) {
				AG_SetError("%s: port #%d > %d", ppath, pport,
				    pcom->nports);
				return (-1);
			}
			br = ES_CircuitAddBranch(ckt, name,
			    &pcom->ports[pport]);
			pcom->ports[pport].node = name;
			pcom->ports[pport].branch = br;
		}
	}

	/* Load the schematics. */
	if (VG_Load(ckt->vg, buf) == -1)
		return (-1);

	/* Load the component port assignments. */
	ncoms = AG_ReadUint32(buf);
	for (i = 0; i < ncoms; i++) {
		char name[AG_OBJECT_NAME_MAX];
		char path[AG_OBJECT_PATH_MAX];
		int j, nports;
		ES_Component *com;

		/* Lookup the component. */
		AG_CopyString(name, buf, sizeof(name));
		AGOBJECT_FOREACH_CHILD(com, ckt, es_component) {
			if (AGOBJECT_SUBCLASS(com, "component") &&
			    strcmp(AGOBJECT(com)->name, name) == 0)
				break;
		}
		if (com == NULL) { Fatal("unexisting component"); }
		
		/* Reassign the block pointer since the vg was reloaded. */
		AG_ObjectCopyName(com, path, sizeof(path));
		if ((com->block = VG_GetBlock(ckt->vg, path)) == NULL)
			Fatal("unexisting block");

		/* Load the port information. */
		com->nports = (int)AG_ReadUint32(buf);
		for (j = 1; j <= com->nports; j++) {
			ES_Port *port = &com->ports[j];
			VG_Block *block_save;
			ES_Branch *br;
			ES_Node *node;

			port->n = (int)AG_ReadUint32(buf);
			AG_CopyString(port->name, buf, sizeof(port->name));
			port->x = AG_ReadDouble(buf);
			port->y = AG_ReadDouble(buf);
			port->node = (int)AG_ReadUint32(buf);
			node = ckt->nodes[port->node];

			TAILQ_FOREACH(br, &node->branches, branches) {
				if (br->port->com == com &&
				    br->port->n == port->n)
					break;
			}
			if (br == NULL) { Fatal("Illegal branch"); }
			port->branch = br;
			port->selected = 0;
		}
	}
	ES_CircuitModified(ckt);
	return (0);
}

int
ES_CircuitSave(void *p, AG_Netbuf *buf)
{
	char path[AG_OBJECT_PATH_MAX];
	struct circuit *ckt = p;
	ES_Node *node;
	ES_Branch *br;
	ES_Component *com;
	off_t ncoms_offs;
	Uint32 ncoms = 0;
	u_int i;

	AG_WriteVersion(buf, &esCircuitVer);
	AG_WriteString(buf, ckt->descr);
	AG_WriteUint32(buf, 0);					/* Pad */
	AG_WriteUint32(buf, ckt->n);
	for (i = 0; i <= ckt->n; i++) {
		ES_Node *node = ckt->nodes[i];
		off_t nbranches_offs;
		Uint32 nbranches = 0;

		AG_WriteUint32(buf, (Uint32)node->flags);
		nbranches_offs = AG_NetbufTell(buf);
		AG_WriteUint32(buf, 0);
		TAILQ_FOREACH(br, &node->branches, branches) {
			if (br->port == NULL || br->port->com == NULL ||
			    br->port->com->flags & COMPONENT_FLOATING) {
				continue;
			}
			AG_ObjectCopyName(br->port->com, path, sizeof(path));
			AG_WriteString(buf, path);
			AG_WriteUint32(buf, 0);			/* Pad */
			AG_WriteUint32(buf, (Uint32)br->port->n);
			nbranches++;
		}
		AG_PwriteUint32(buf, nbranches, nbranches_offs);
	}
	
	/* Save the schematics. */
	VG_Save(ckt->vg, buf);
	
	/* Save the component information. */
	ncoms_offs = AG_NetbufTell(buf);
	AG_WriteUint32(buf, 0);
	AGOBJECT_FOREACH_CHILD(com, ckt, es_component) {
		if (!AGOBJECT_SUBCLASS(com, "component") ||
		    com->flags & COMPONENT_FLOATING) {
			continue;
		}
		AG_WriteString(buf, AGOBJECT(com)->name);
		AG_WriteUint32(buf, (Uint32)com->nports);
		for (i = 1; i <= com->nports; i++) {
			ES_Port *port = &com->ports[i];

			AG_WriteUint32(buf, (Uint32)port->n);
			AG_WriteString(buf, port->name);
			AG_WriteDouble(buf, port->x);
			AG_WriteDouble(buf, port->y);
#ifdef DEBUG
			if (port->node == -1) { Fatal("Illegal port"); }
#endif
			AG_WriteUint32(buf, (Uint32)port->node);
		}
		ncoms++;
	}
	AG_PwriteUint32(buf, ncoms, ncoms_offs);
	return (0);
}

static void
init_node(ES_Node *node, u_int flags)
{
	node->flags = flags;
	node->nbranches = 0;
	TAILQ_INIT(&node->branches);
}

static void
init_ground(struct circuit *ckt)
{
	ckt->nodes[0] = Malloc(sizeof(ES_Node), M_EDA);
	init_node(ckt->nodes[0], CKTNODE_REFERENCE);
}

int
ES_CircuitAddNode(struct circuit *ckt, u_int flags)
{
	ES_Node *node;
	u_int n = ++ckt->n;

	ckt->nodes = Realloc(ckt->nodes, (n+1)*sizeof(ES_Node *));
	ckt->nodes[n] = Malloc(sizeof(ES_Node), M_EDA);
	init_node(ckt->nodes[n], flags);
	return (n);
}

/* Evaluate whether node n is at ground potential. */
int
ES_NodeGrounded(struct circuit *ckt, u_int n)
{
	/* TODO check for shunts */
	return (n == 0 ? 1 : 0);
}

/*
 * Evaluate whether the given node is connected to the given voltage source.
 * If that is the case, return the polarity of the node with respect to
 * the source.
 */
int
ES_NodeVsource(struct circuit *ckt, u_int n, u_int m, int *sign)
{
	ES_Node *node = ckt->nodes[n];
	ES_Branch *br;
	ES_Vsource *vs;
	u_int i;

	TAILQ_FOREACH(br, &node->branches, branches) {
		if (br->port == NULL ||
		   (vs = VSOURCE(br->port->com)) == NULL) {
			continue;
		}
		if (COM(vs)->flags & COMPONENT_FLOATING ||
		   !AGOBJECT_SUBCLASS(vs, "component.vsource") ||
		   vs != ckt->vsrcs[m-1]) {
			continue;
		}
		if (br->port->n == 1) {
			*sign = +1;
		} else {
			*sign = -1;
		}
		return (1);
	}
	return (0);
}


void
ES_CircuitDestroyNode(ES_Node *node)
{
	ES_Branch *br, *nbr;

	for (br = TAILQ_FIRST(&node->branches);
	     br != TAILQ_END(&node->branches);
	     br = nbr) {
		nbr = TAILQ_NEXT(br, branches);
		Free(br, M_EDA);
	}
	TAILQ_INIT(&node->branches);
}

void
ES_CircuitDelNode(struct circuit *ckt, u_int n)
{
	ES_Node *node;
	ES_Branch *br;
	u_int i;

#ifdef DEBUG
	if (n == 0 || n > ckt->n) { Fatal("Illegal node"); }
#endif
	node = ckt->nodes[n];
	ES_CircuitDestroyNode(node);
	Free(node, M_EDA);

	if (n != ckt->n) {
		for (i = n; i <= ckt->n; i++) {
			TAILQ_FOREACH(br, &ckt->nodes[i]->branches, branches) {
				if (br->port != NULL && br->port->com != NULL)
					br->port->node = i-1;
			}
		}
		memmove(&ckt->nodes[n], &ckt->nodes[n+1],
		    (ckt->n - n) * sizeof(ES_Node *));
	}
	ckt->n--;
}

ES_Branch *
ES_CircuitAddBranch(struct circuit *ckt, u_int n, ES_Port *port)
{
	ES_Node *node = ckt->nodes[n];
	ES_Branch *br;

	br = Malloc(sizeof(ES_Branch), M_EDA);
	br->port = port;
	TAILQ_INSERT_TAIL(&node->branches, br, branches);
	node->nbranches++;
	return (br);
}

ES_Branch *
ES_CircuitGetBranch(struct circuit *ckt, u_int n, ES_Port *port)
{
	ES_Node *node = ckt->nodes[n];
	ES_Branch *br;

	TAILQ_FOREACH(br, &node->branches, branches) {
		if (br->port == port)
			break;
	}
	return (br);
}

void
ES_CircuitDelBranch(struct circuit *ckt, u_int n, ES_Branch *br)
{
	ES_Node *node = ckt->nodes[n];

	TAILQ_REMOVE(&node->branches, br, branches);
	Free(br, M_EDA);
#ifdef DEBUG
	if ((node->nbranches - 1) < 0) { Fatal("--nbranches < 0"); }
#endif
	node->nbranches--;
}

void
ES_CircuitFreeComponents(struct circuit *ckt)
{
	ES_Component *com, *ncom;

	AGOBJECT_FOREACH_CHILD(com, ckt, es_component) {
		if (!AGOBJECT_SUBCLASS(com, "component")) {
			continue;
		}
		AG_ObjectDetach(com);
		AG_ObjectDestroy(com);
		Free(com, M_OBJECT);
	}
}

void
ES_CircuitDestroy(void *p)
{
	struct circuit *ckt = p;
	
	VG_Destroy(ckt->vg);
	pthread_mutex_destroy(&ckt->lock);
}

/* Select the simulation mode. */
struct sim *
ES_CircuitSetSimMode(struct circuit *ckt, const struct sim_ops *sops)
{
	struct sim *sim;

	if (ckt->sim != NULL) {
		sim_destroy(ckt->sim);
		ckt->sim = NULL;
	}
	ckt->sim = sim = Malloc(sops->size, M_EDA);
	sim->ckt = ckt;
	sim->ops = sops;
	sim->running = 0;

	if (sim->ops->init != NULL) {
		sim->ops->init(sim);
	}
	if (sim->ops->edit != NULL &&
	   (sim->win = sim->ops->edit(sim, ckt)) != NULL) {
		AG_WindowSetCaption(sim->win, "%s: %s", AGOBJECT(ckt)->name,
		    _(sim->ops->name));
		AG_WindowSetPosition(sim->win, AG_WINDOW_LOWER_LEFT, 0);
		AG_WindowShow(sim->win);
	}
	return (sim);
}

#ifdef EDITION

static void
poll_loops(AG_Event *event)
{
	AG_Tlist *tl = AG_SELF();
	struct circuit *ckt = AG_PTR(1);
	int i;

	AG_TlistClear(tl);
	for (i = 0; i < ckt->l; i++) {
		char voltage[32];
		ES_Loop *loop = ckt->loops[i];
		ES_Vsource *vs = (ES_Vsource *)loop->origin;
		AG_TlistItem *it;

		AG_UnitFormat(vs->voltage, agEMFUnits, voltage,
		    sizeof(voltage));
		it = AG_TlistAdd(tl, NULL, "%s:L%u (%s)", AGOBJECT(vs)->name,
		    loop->name, voltage);
		it->p1 = loop;
	}
	AG_TlistRestore(tl);
}

static void
poll_nodes(AG_Event *event)
{
	AG_Tlist *tl = AG_SELF();
	struct circuit *ckt = AG_PTR(1);
	u_int i;

	AG_TlistClear(tl);
	for (i = 0; i <= ckt->n; i++) {
		ES_Node *node = ckt->nodes[i];
		ES_Branch *br;
		AG_TlistItem *it, *it2;

		it = AG_TlistAdd(tl, NULL, "n%u (0x%x, %d branches)", i,
		    node->flags, node->nbranches);
		it->p1 = node;
		it->depth = 0;
		TAILQ_FOREACH(br, &node->branches, branches) {
			if (br->port == NULL) {
				it = AG_TlistAdd(tl, NULL, "(null port)");
			} else {
				it = AG_TlistAdd(tl, NULL, "%s:%s(%d)",
				    (br->port!=NULL && br->port->com!=NULL) ?
				    AGOBJECT(br->port->com)->name : "(null)",
				    br->port->name, br->port->n);
			}
			it->p1 = br;
			it->depth = 1;
		}
	}
	AG_TlistRestore(tl);
}

static void
poll_sources(AG_Event *event)
{
	AG_Tlist *tl = AG_SELF();
	struct circuit *ckt = AG_PTR(1);
	int i;

	AG_TlistClear(tl);
	for (i = 0; i < ckt->m; i++) {
		ES_Vsource *vs = ckt->vsrcs[i];
		AG_TlistItem *it;

		it = AG_TlistAdd(tl, NULL, "%s", AGOBJECT(vs)->name);
		it->p1 = vs;
	}
	AG_TlistRestore(tl);
}

static void
revert_circuit(AG_Event *event)
{
	struct circuit *ckt = AG_PTR(1);

	if (AG_ObjectLoad(ckt) == 0) {
		AG_TextTmsg(AG_MSG_INFO, 1000,
		    _("Circuit `%s' reverted successfully."),
		    AGOBJECT(ckt)->name);
	} else {
		AG_TextMsg(AG_MSG_ERROR, "%s: %s", AGOBJECT(ckt)->name,
		    AG_GetError());
	}
}

static void
save_circuit(AG_Event *event)
{
	struct circuit *ckt = AG_PTR(1);

	if (AG_ObjectSave(agWorld) == 0 &&
	    AG_ObjectSave(ckt) == 0) {
		AG_TextTmsg(AG_MSG_INFO, 1250,
		    _("Circuit `%s' saved successfully."),
		    AGOBJECT(ckt)->name);
	} else {
		AG_TextMsg(AG_MSG_ERROR, "%s: %s", AGOBJECT(ckt)->name,
		    AG_GetError());
	}
}

static void
show_interconnects(AG_Event *event)
{
	AG_Window *pwin = AG_PTR(1);
	struct circuit *ckt = AG_PTR(2);
	AG_Window *win;
	AG_Notebook *nb;
	AG_NotebookTab *ntab;
	AG_Tlist *tl;
	
	if ((win = AG_WindowNewNamed(0, "%s-interconnects",
	    AGOBJECT(ckt)->name)) == NULL) {
		return;
	}
	AG_WindowSetCaption(win, _("%s: Connections"), AGOBJECT(ckt)->name);
	AG_WindowSetPosition(win, AG_WINDOW_UPPER_RIGHT, 0);
	
	nb = AG_NotebookNew(win, AG_NOTEBOOK_EXPAND);
	ntab = AG_NotebookAddTab(nb, _("Nodes"), AG_BOX_VERT);
	{
		tl = AG_TlistNew(ntab, AG_TLIST_POLL|AG_TLIST_TREE|
		                       AG_TLIST_EXPAND);
		AG_SetEvent(tl, "tlist-poll", poll_nodes, "%p", ckt);
	}
	
	ntab = AG_NotebookAddTab(nb, _("Loops"), AG_BOX_VERT);
	{
		tl = AG_TlistNew(ntab, AG_TLIST_POLL|AG_TLIST_EXPAND);
		AG_SetEvent(tl, "tlist-poll", poll_loops, "%p", ckt);
	}
	
	ntab = AG_NotebookAddTab(nb, _("Sources"), AG_BOX_VERT);
	{
		tl = AG_TlistNew(ntab, AG_TLIST_POLL|AG_TLIST_EXPAND);
		AG_SetEvent(tl, "tlist-poll", poll_sources, "%p", ckt);
	}

	AG_WindowAttach(pwin, win);
	AG_WindowShow(win);
}

static void
layout_settings(AG_Event *event)
{
	char path[AG_OBJECT_PATH_MAX];
	AG_Window *pwin = AG_PTR(1);
	struct circuit *ckt = AG_PTR(2);
	VG *vg = ckt->vg;
	AG_Window *win;
	AG_MFSpinbutton *mfsu;
	AG_FSpinbutton *fsu;
	AG_Textbox *tb;
	AG_Checkbox *cb;
	
	AG_ObjectCopyName(ckt, path, sizeof(path));
	if ((win = AG_WindowNewNamed(0, "settings-%s", path)) == NULL) {
		return;
	}
	AG_WindowSetCaption(win, _("Circuit layout: %s"), AGOBJECT(ckt)->name);
	AG_WindowSetPosition(win, AG_WINDOW_UPPER_RIGHT, 0);

	AG_LabelNew(win, AG_LABEL_POLLED, _("Loops: %u"), &ckt->l);
	AG_LabelNew(win, AG_LABEL_POLLED, _("Voltage sources: %u"), &ckt->m);
	AG_LabelNew(win, AG_LABEL_POLLED, _("Nodes: %u"), &ckt->n);

	tb = AG_TextboxNew(win, AG_WIDGET_HFILL, _("Description: "));
	AG_WidgetBind(tb, "string", AG_WIDGET_STRING, &ckt->descr,
	    sizeof(ckt->descr));

	mfsu = AG_MFSpinbuttonNew(win, AG_WIDGET_HFILL, NULL, "x",
	    _("Bounding box: "));
	AG_WidgetBind(mfsu, "xvalue", AG_WIDGET_DOUBLE, &vg->w);
	AG_WidgetBind(mfsu, "yvalue", AG_WIDGET_DOUBLE, &vg->h);
	AG_MFSpinbuttonSetMin(mfsu, 1.0);
	AG_MFSpinbuttonSetIncrement(mfsu, 0.1);
//	AG_SetEvent(mfsu, "mfspinbutton-changed", VG_GeoChangedEv, "%p", vg);

	fsu = AG_FSpinbuttonNew(win, AG_WIDGET_HFILL, NULL,
	    _("Grid spacing: "));
	AG_WidgetBind(fsu, "value", AG_WIDGET_DOUBLE, &vg->grid_gap);
	AG_FSpinbuttonSetMin(fsu, 0.0625);
	AG_FSpinbuttonSetIncrement(fsu, 0.0625);
//	AG_SetEvent(fsu, "fspinbutton-changed", VG_ChangedEv, "%p", vg);
	
	fsu = AG_FSpinbuttonNew(win, AG_WIDGET_HFILL, NULL,
	    _("Scaling factor: "));
	AG_WidgetBind(fsu, "value", AG_WIDGET_DOUBLE, &vg->scale);
	AG_FSpinbuttonSetMin(fsu, 0.1);
	AG_FSpinbuttonSetIncrement(fsu, 0.1);
//	AG_SetEvent(fsu, "fspinbutton-changed", VG_GeoChangedEv, "%p", vg);

	cb = AG_CheckboxNew(win, 0, _("Display node indicators"));
	AG_WidgetBind(cb, "state", AG_WIDGET_INT, &ckt->dis_nodes);

	cb = AG_CheckboxNew(win, 0, _("Display node names"));
	AG_WidgetBind(cb, "state", AG_WIDGET_INT, &ckt->dis_node_names);
	
#if 0
	AG_LabelNew(win, AG_LABEL_STATIC, _("Nodes:"));
	tl = AG_TlistNew(win, AG_TLIST_POLL|AG_TLIST_TREE);
	AGWIDGET(tl)->flags &= ~(AG_WIDGET_HFILL);
	AG_SetEvent(tl, "tlist-poll", poll_nodes, "%p", ckt);
#endif	

	AG_WindowAttach(pwin, win);
	AG_WindowShow(win);
}

static void
export_to_spice(AG_Event *event)
{
	char name[FILENAME_MAX];
	struct circuit *ckt = AG_PTR(1);

	strlcpy(name, AGOBJECT(ckt)->name, sizeof(name));
	strlcat(name, ".cir", sizeof(name));

	if (ES_CircuitExportSPICE3(ckt, name) == -1)
		AG_TextMsg(AG_MSG_ERROR, "%s: %s", AGOBJECT(ckt)->name,
		    AG_GetError());
}

#if 0
static void
double_click(AG_Event *event)
{
	struct circuit *ckt = AG_PTR(1);
	int button = AG_INT(2);
	int tx = AG_INT(3);
	int ty = AG_INT(4);
	int txoff = AG_INT(5);
	int tyoff = AG_INT(6);
	ES_Component *com;
	double x, y;

	VG_Vcoords2(ckt->vg, tx, ty, txoff, tyoff, &x, &y);
	AGOBJECT_FOREACH_CHILD(com, ckt, es_component) {
		VG_Rect rext;

		if (!AGOBJECT_SUBCLASS(com, "component"))
			continue;

		VG_BlockExtent(ckt->vg, com->block, &rext);
		if (x > rext.x && y > rext.y &&
		    x < rext.x+rext.w && y < rext.y+rext.h) {
#if 0
			com->selected = !com->selected;
#endif
			/* XXX configurable */
			switch (button) {
			case 1:
				if (AGOBJECT(com)->ops->edit != NULL) {
					AG_ObjMgrOpenData(com, 1);
				}
				break;
			case 2:
			case 3:
				com->selected = !com->selected;
				break;
			}
		}
	}
}
#endif

static void
select_sim(AG_Event *event)
{
	struct circuit *ckt = AG_PTR(1);
	struct sim_ops *sops = AG_PTR(2);
	AG_Window *pwin = AG_PTR(3);
	struct sim *sim;

	sim = ES_CircuitSetSimMode(ckt, sops);
	if (sim->win != NULL)
		AG_WindowAttach(pwin, sim->win);
}

static void
select_tool(AG_Event *event)
{
	VG_View *vgv = AG_PTR(1);
	VG_Tool *tool = AG_PTR(2);
	struct circuit *ckt = AG_PTR(3);

	VG_ViewSelectTool(vgv, tool, ckt);
}

static void
find_objs(AG_Tlist *tl, AG_Object *pob, int depth, void *ckt)
{
	AG_Object *cob;
	AG_TlistItem *it;
	SDL_Surface *icon;

	it = AG_TlistAdd(tl, AG_ObjectIcon(pob), "%s%s", pob->name,
	    (pob->flags & AG_OBJECT_DATA_RESIDENT) ? " (resident)" : "");
	it->depth = depth;
	it->class = "object";
	it->p1 = pob;

	if (!TAILQ_EMPTY(&pob->children)) {
		it->flags |= AG_TLIST_HAS_CHILDREN;
		if (pob == AGOBJECT(ckt))
			it->flags |= AG_TLIST_VISIBLE_CHILDREN;
	}
	if ((it->flags & AG_TLIST_HAS_CHILDREN) &&
	    AG_TlistVisibleChildren(tl, it)) {
		TAILQ_FOREACH(cob, &pob->children, cobjs)
			find_objs(tl, cob, depth+1, ckt);
	}
}

static void
poll_objs(AG_Event *event)
{
	AG_Tlist *tl = AG_SELF();
	AG_Object *pob = AG_PTR(1);
	AG_TlistItem *it;

	AG_TlistClear(tl);
	AG_LockLinkage();
	find_objs(tl, pob, 0, pob);
	AG_UnlockLinkage();
	AG_TlistRestore(tl);
}

#if 0
static void
create_view(AG_Event *event)
{
	extern VG_ToolOps vgScaleTool;
	VG_View *omv = AG_PTR(1);
	AG_Window *pwin = AG_PTR(2);
	struct circuit *ckt = AG_PTR(3);
	AG_Map *map = omv->map;
	VG_View *mv;
	AG_Window *win;

	win = AG_WindowNew(0);
	AG_WindowSetCaption(win, _("View of %s"), AGOBJECT(ckt)->name);
	{
		mv = VG_ViewNew(win, map,
		    AG_MAPVIEW_NO_BMPSCALE|AG_MAPVIEW_NO_NODESEL,
		    NULL, NULL);
		mv->cam = AG_MapAddCamera(map, _("View"));

		VG_ViewPrescale(mv, 8, 6);
		VG_ViewRegTool(mv, &vgScaleTool, ckt->vg);
		AG_WidgetFocus(mv);
	}
	AG_WindowAttach(pwin, win);
	AG_WindowShow(win);
}
#endif

static void
poll_component_ports(AG_Event *event)
{
	AG_Tlist *tl = AG_SELF();
	struct circuit *ckt = AG_PTR(1);
	ES_Component *com;
	int i;
	
	AG_TlistClear(tl);
	
	AGOBJECT_FOREACH_CHILD(com, ckt, es_component) {
		if (AGOBJECT_SUBCLASS(com, "component") && com->selected)
			break;
	}
	if (com == NULL)
		goto out;

	pthread_mutex_lock(&com->lock);
	for (i = 1; i <= com->nports; i++) {
		char text[AG_TLIST_LABEL_MAX];
		ES_Port *port = &com->ports[i];

		snprintf(text, sizeof(text), "%d (%s) (U=%.2f) -> n%d\n",
		    port->n, port->name, port->u, port->node);
		AG_TlistAddPtr(tl, AGICON(EDA_BRANCH_TO_COMPONENT_ICON),
		    text, port);
	}
	pthread_mutex_unlock(&com->lock);
out:
	AG_TlistRestore(tl);
}

static void
poll_component_pairs(AG_Event *event)
{
	AG_Tlist *tl = AG_SELF();
	struct circuit *ckt = AG_PTR(1);
	ES_Component *com;
	int i, j;
	
	AG_TlistClear(tl);
	
	AGOBJECT_FOREACH_CHILD(com, ckt, es_component) {
		if (AGOBJECT_SUBCLASS(com, "component") && com->selected)
			break;
	}
	if (com == NULL)
		goto out;

	pthread_mutex_lock(&com->lock);
	for (i = 0; i < com->npairs; i++) {
		char Rbuf[32], Cbuf[32], Lbuf[32];
		char text[AG_TLIST_LABEL_MAX];
		ES_Pair *pair = &com->pairs[i];
		AG_TlistItem *it;

		AG_UnitFormat(ES_PairResistance(pair), agResistanceUnits,
		    Rbuf, sizeof(Rbuf));
		AG_UnitFormat(ES_PairCapacitance(pair), agCapacitanceUnits,
		    Cbuf, sizeof(Cbuf));
		AG_UnitFormat(ES_PairInductance(pair), agInductanceUnits,
		    Lbuf, sizeof(Lbuf));

		snprintf(text, sizeof(text),
		    "%s:(%s,%s) - %s, %s, %s",
		    AGOBJECT(com)->name, pair->p1->name, pair->p2->name,
		    Rbuf, Cbuf, Lbuf);
		it = AG_TlistAddPtr(tl, AGICON(EDA_BRANCH_TO_COMPONENT_ICON),
		    text, pair);
		it->depth = 0;

		for (j = 0; j < pair->nloops; j++) {
			ES_Loop *dloop = pair->loops[j];
			int dpol = pair->lpols[j];

			snprintf(text, sizeof(text), "%s:L%u (%s)",
			    AGOBJECT(dloop->origin)->name,
			    dloop->name,
			    dpol == 1 ? "+" : "-");
			it = AG_TlistAddPtr(tl, AGICON(EDA_MESH_ICON), text,
			    &pair->loops[j]);
			it->depth = 1;
		}
	}
	pthread_mutex_unlock(&com->lock);
out:
	AG_TlistRestore(tl);
}

void *
ES_CircuitEdit(void *p)
{
	struct circuit *ckt = p;
	AG_Window *win;
	AG_Toolbar *toolbar;
	AG_Menu *menu;
	AG_MenuItem *pitem, *subitem;
	AG_HPane *pane;
	AG_HPaneDiv *div;
	VG_View *vgv;

	win = AG_WindowNew(0);
	AG_WindowSetCaption(win, "%s", AGOBJECT(ckt)->name);

	toolbar = Malloc(sizeof(AG_Toolbar), M_OBJECT);
	AG_ToolbarInit(toolbar, AG_TOOLBAR_VERT, 1, 0);
	vgv = Malloc(sizeof(VG_View), M_OBJECT);
	VG_ViewInit(vgv, ckt->vg, VG_VIEW_EXPAND);
	
	menu = AG_MenuNew(win, AG_MENU_HFILL);
	pitem = AG_MenuAddItem(menu, _("File"));
	{
		AG_MenuActionKb(pitem, _("Revert"), OBJLOAD_ICON,
		    SDLK_r, KMOD_CTRL, revert_circuit, "%p", ckt);
		AG_MenuActionKb(pitem, _("Save"), OBJSAVE_ICON,
		    SDLK_s, KMOD_CTRL, save_circuit, "%p", ckt);

		subitem = AG_MenuAction(pitem, _("Export circuit"),
		    OBJSAVE_ICON, NULL, NULL);
		AG_MenuAction(subitem, _("Export to SPICE3..."), EDA_SPICE_ICON,
		    export_to_spice, "%p", ckt);
	
		AG_MenuSeparator(pitem);

		AG_MenuActionKb(pitem, _("Close document"), CLOSE_ICON,
		    SDLK_w, KMOD_CTRL, AGWINCLOSE(win));
	}
	pitem = AG_MenuAddItem(menu, _("Edit"));
	{
		/* TODO */
		AG_MenuAction(pitem, _("Undo"), -1, NULL, NULL);
		AG_MenuAction(pitem, _("Redo"), -1, NULL, NULL);

		AG_MenuSeparator(pitem);

		AG_MenuAction(pitem, _("Layout preferences..."), SETTINGS_ICON,
		    layout_settings, "%p,%p", win, ckt);
	}
#if 0
	pitem = AG_MenuAddItem(menu, _("Drawing"));
	VG_GenericMenu(menu, pitem, ckt->vg, vgv);
#endif
	pitem = AG_MenuAddItem(menu, _("View"));
	{
		AG_MenuAction(pitem, _("Show interconnects..."), EDA_MESH_ICON,
		    show_interconnects, "%p,%p", win, ckt);

		AG_MenuSeparator(pitem);

		AG_MenuIntFlags(pitem, _("Show origin"), VGORIGIN_ICON,
		    &ckt->vg->flags, VG_VISORIGIN, 0);
		AG_MenuIntFlags(pitem, _("Show grid"), GRID_ICON,
		    &ckt->vg->flags, VG_VISGRID, 0);
		AG_MenuIntFlags(pitem, _("Show extents"), VGBLOCK_ICON,
		    &ckt->vg->flags, VG_VISBBOXES, 0);
		AG_MenuSeparator(pitem);
#if 0
		AG_MenuAction(pitem, _("Create view..."), NEW_VIEW_ICON,
		    create_view, "%p,%p,%p", vgv, win, ckt);
#endif
	}

	pitem = AG_MenuAddItem(menu, _("Simulation"));
	{
		extern const struct sim_ops *sim_ops[];
		const struct sim_ops **ops;

		for (ops = &sim_ops[0]; *ops != NULL; ops++) {
			AG_MenuTool(pitem, toolbar, _((*ops)->name),
			    (*ops)->icon, 0, 0,
			    select_sim, "%p,%p,%p", ckt, *ops, win);
		}
	}
	
	pane = AG_HPaneNew(win, AG_HPANE_EXPAND);
	div = AG_HPaneAddDiv(pane,
	    AG_BOX_VERT, AG_BOX_VFILL,
	    AG_BOX_HORIZ, AG_BOX_EXPAND);
	{
		AG_Notebook *nb;
		AG_NotebookTab *ntab;
		AG_Tlist *tl;
		AG_Box *box;

		nb = AG_NotebookNew(div->box1, AG_NOTEBOOK_EXPAND);
		ntab = AG_NotebookAddTab(nb, _("Models"), AG_BOX_VERT);
		{
			char tname[AG_OBJECT_TYPE_MAX];
			extern const struct eda_type eda_models[];
			const struct eda_type *ty;
			int i;

			tl = AG_TlistNew(ntab, AG_TLIST_EXPAND);
			AGWIDGET(tl)->flags &= ~(AG_WIDGET_FOCUSABLE);
			AG_SetEvent(tl, "tlist-dblclick", ES_ComponentInsert,
			    "%p,%p,%p", vgv, tl, ckt);

			AG_ButtonAct(ntab, AG_BUTTON_HFILL,
			    _("Insert component"),
			    ES_ComponentInsert, "%p,%p,%p", vgv, tl, ckt);

			for (ty = &eda_models[0]; ty->name != NULL; ty++) {
				for (i = 0; i < agnTypes; i++) {
					strlcpy(tname, "component.",
					    sizeof(tname));
					strlcat(tname, ty->name, sizeof(tname));
					if (strcmp(agTypes[i].type, tname) == 0)
						break;
				}
				if (i < agnTypes) {
					AG_ObjectType *ctype = &agTypes[i];
					ES_ComponentOps *cops =
					    (ES_ComponentOps *)ctype->ops;

					AG_TlistAddPtr(tl, NULL,
					    _(cops->name), ctype);
				}
			}
		}

		ntab = AG_NotebookAddTab(nb, _("Objects"), AG_BOX_VERT);
		{
			tl = AG_TlistNew(ntab, AG_TLIST_POLL|AG_TLIST_TREE|
			                       AG_TLIST_EXPAND);
			AG_SetEvent(tl, "tlist-poll", poll_objs, "%p", ckt);
			AGWIDGET(tl)->flags &= ~(AG_WIDGET_FOCUSABLE);
		}

		ntab = AG_NotebookAddTab(nb, _("Component"), AG_BOX_VERT);
		{
#if 0
			AG_FSpinbutton *fsb;

			label_static(ntab, _("Temperature:"));

			fsb = AG_FSpinbuttonNew(win, 0, "degC",
			    _("Reference: "));
			AG_WidgetBind(fsb, "value", AG_WIDGET_FLOAT,
			    &com->Tnom);
			AG_FSpinbuttonSetMin(fsb, 0.0);
	
			fsb = AG_FSpinbuttonNew(win, 0, "degC",
			    _("Instance: "));
			AG_WidgetBind(fsb, "value", AG_WIDGET_FLOAT,
			    &com->Tspec);
			AG_FSpinbuttonSetMin(fsb, 0.0);

			AG_SeparatorNew(ntab, SEPARATOR_HORIZ);
#endif
			AG_LabelNew(ntab, AG_LABEL_STATIC, _("Pinout:"));
			{
				tl = AG_TlistNew(ntab, AG_TLIST_POLL|
						       AG_TLIST_EXPAND);
				AG_SetEvent(tl, "tlist-poll",
				    poll_component_ports, "%p", ckt);
			}
			AG_LabelNew(ntab, AG_LABEL_STATIC, _("Port pairs:"));
			{
				tl = AG_TlistNew(ntab, AG_TLIST_POLL|
				                       AG_TLIST_HFILL);
				AG_SetEvent(tl, "tlist-poll",
				    poll_component_pairs, "%p", ckt);
			}
		}

		box = AG_BoxNew(div->box2, AG_BOX_VERT, AG_BOX_EXPAND);
		{
			AG_ObjectAttach(box, vgv);
			AG_WidgetFocus(vgv);
		}
		AG_ObjectAttach(div->box2, toolbar);
	}
	
	pitem = AG_MenuAddItem(menu, _("Components"));
	ES_ComponentRegMenu(menu, pitem, ckt);
	{
		extern VG_ToolOps esInscomOps;
		extern VG_ToolOps esSelcomOps;
		extern VG_ToolOps esConductorTool;
#if 0
		extern VG_ToolOps vgScaleTool;
		extern VG_ToolOps vgOriginTool;
		extern VG_ToolOps vgLineTool;
		extern VG_ToolOps vgTextTool;
#endif
		VG_Tool *tool;
			
#if 0
		VG_ViewRegTool(vgv, &vgScaleTool, ckt->vg);
		VG_ViewRegTool(vgv, &vgOriginTool, ckt->vg);
		VG_ViewRegTool(vgv, &vgLineTool, ckt->vg);
		VG_ViewRegTool(vgv, &vgTextTool, ckt->vg);
		AG_SetEvent(vgv, "mapview-dblclick", double_click, "%p", ckt);
#endif
		VG_ViewRegTool(vgv, &esInscomOps, ckt);

		tool = VG_ViewRegTool(vgv, &esSelcomOps, ckt);
		AG_MenuTool(pitem, toolbar, _("Select components"),
		    tool->ops->icon, 0, 0,
		    select_tool, "%p,%p,%p", vgv, tool, ckt);
		VG_ViewSetDefaultTool(vgv, tool);
		
		tool = VG_ViewRegTool(vgv, &esConductorTool, ckt);
		AG_MenuTool(pitem, toolbar, _("Insert conductor"),
		    tool->ops->icon, 0, 0,
		    select_tool, "%p,%p,%p", vgv, tool, ckt);
	}

	AG_WindowScale(win, -1, -1);
	AG_WindowSetGeometry(win,
	    agView->w/16, agView->h/16,
	    7*agView->w/8, 7*agView->h/8);
	
	return (win);
}
#endif /* EDITION */
