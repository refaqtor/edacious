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

/* Select overlapping components. */
static int
SelcomButtondown(void *p, float x, float y, int b)
{
	VG_Tool *t = p;
	ES_Circuit *ckt = t->p;
	VG *vg = ckt->vg;
	ES_Component *com;
	int multi = SDL_GetModState() & KMOD_CTRL;
	AG_Window *pwin;

	if (b != SDL_BUTTON_LEFT)
		return (0);

	if (!multi) {
		ES_ComponentUnselectAll(ckt);
	}
	AGOBJECT_FOREACH_CHILD(com, ckt, es_component) {
		VG_Rect rext;

		if (!AGOBJECT_SUBCLASS(com, "component") ||
		    com->flags & COMPONENT_FLOATING) {
			continue;
		}
		VG_BlockExtent(vg, com->block, &rext);
		if (x > rext.x && y > rext.y &&
		    x < rext.x+rext.w && y < rext.y+rext.h) {
			if (multi) {
				if (com->selected) {
					ES_ComponentUnselect(com);
				} else {
					ES_ComponentSelect(com);
				}
			} else {
				ES_ComponentSelect(com);
				break;
			}
		}
	}
	if ((pwin = AG_WidgetParentWindow(t->vgv)) != NULL) {
		agView->focus_win = pwin;
		AG_WidgetFocus(t->vgv);
	}
	return (1);
}

/* Select overlapping components. */
static int
SelcomLeftButton(VG_Tool *t, int button, int state, float x, float y, void *arg)
{
	ES_Circuit *ckt = t->p;
	VG *vg = ckt->vg;
	ES_Component *com;
	int multi = SDL_GetModState() & KMOD_CTRL;

	if (button != SDL_BUTTON_LEFT || !state)
		return (0);

	if (!multi) {
		ES_ComponentUnselectAll(ckt);
	}
	AGOBJECT_FOREACH_CHILD(com, ckt, es_component) {
		VG_Rect rext;

		if (!AGOBJECT_SUBCLASS(com, "component") ||
		    com->flags & COMPONENT_FLOATING) {
			continue;
		}
		VG_BlockExtent(vg, com->block, &rext);
		if (x > rext.x && y > rext.y &&
		    x < rext.x+rext.w && y < rext.y+rext.h) {
			if (multi) {
				if (com->selected) {
					ES_ComponentUnselect(com);
				} else {
					ES_ComponentSelect(com);
				}
			} else {
				ES_ComponentSelect(com);
				break;
			}
		}
	}
	return (1);
}

/* Highlight any component underneath the cursor. */
static int
SelcomMousemotion(void *p, float x, float y, float xrel, float yrel, int b)
{
	VG_Tool *t = p;
	ES_Circuit *ckt = t->p;
	VG *vg = ckt->vg;
	ES_Component *com;
	VG_Rect rext;

	vg->origin[1].x = x;
	vg->origin[1].y = y;
	AGOBJECT_FOREACH_CHILD(com, ckt, es_component) {
		if (!AGOBJECT_SUBCLASS(com, "component") ||
		    com->flags & COMPONENT_FLOATING) {
			continue;
		}
		VG_BlockExtent(vg, com->block, &rext);
		if (x > rext.x &&
		    y > rext.y &&
		    x < rext.x+rext.w &&
		    y < rext.y+rext.h) {
			com->highlighted = 1;
		} else {
			com->highlighted = 0;
		}
	}
	return (0);
}

static void
SelcomInit(void *p)
{
	VG_Tool *t = p;

	VG_ToolBindMouseButton(t, SDL_BUTTON_LEFT, SelcomLeftButton, NULL);
}

VG_ToolOps esSelcomOps = {
	N_("Select component"),
	N_("Select one or more component in the circuit."),
	EDA_COMPONENT_ICON,
	sizeof(VG_Tool),
	VG_NOSNAP,
	SelcomInit,
	NULL,			/* destroy */
	NULL,			/* edit */
	SelcomMousemotion,
	SelcomButtondown,
	NULL,			/* mousebuttonup */
	NULL,			/* keydown */
	NULL			/* keyup */
};
