/*
 * Copyright (c) 2003-2008 Hypertriton, Inc. <http://hypertriton.com/>
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
 * Main user interface code for Edacious.
 */

#include <agar/core.h>
#include <agar/gui.h>
#include <agar/dev.h>
#include <agar/vg.h>

#include <core/core.h>
#include <generic/generic.h>
#include <macro/macro.h>
#include <sources/sources.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <config/enable_nls.h>
#include <config/have_getopt.h>
#include <config/have_agar_dev.h>
#include <agar/config/have_opengl.h>

AG_Menu *appMenu = NULL;
static void *objFocus = NULL;
static AG_Mutex objLock;

static void
SaveAndClose(AG_Object *obj, AG_Window *win)
{
	AG_ViewDetach(win);
	AG_ObjectPageOut(obj);
}

static void
SaveChangesReturn(AG_Event *event)
{
	AG_Window *win = AG_PTR(1);
	AG_Object *obj = AG_PTR(2);
	int doSave = AG_INT(3);

	AG_PostEvent(NULL, obj, "edit-close", NULL);

	if (doSave) {
		SaveAndClose(obj, win);
	} else {
		AG_ViewDetach(win);
	}
}

static void
SaveChangesDlg(AG_Event *event)
{
	AG_Window *win = AG_SELF();
	AG_Object *obj = AG_PTR(1);

	if (!AG_ObjectChanged(obj)) {
		SaveAndClose(obj, win);
	} else {
		AG_Button *bOpts[3];
		AG_Window *wDlg;

		wDlg = AG_TextPromptOptions(bOpts, 3,
		    _("Save changes to %s?"), OBJECT(obj)->name);
		AG_WindowAttach(win, wDlg);
		
		AG_ButtonText(bOpts[0], _("Save"));
		AG_SetEvent(bOpts[0], "button-pushed", SaveChangesReturn,
		    "%p,%p,%i", win, obj, 1);
		AG_WidgetFocus(bOpts[0]);
		AG_ButtonText(bOpts[1], _("Discard"));
		AG_SetEvent(bOpts[1], "button-pushed", SaveChangesReturn,
		    "%p,%p,%i", win, obj, 0);
		AG_ButtonText(bOpts[2], _("Cancel"));
		AG_SetEvent(bOpts[2], "button-pushed", AGWINDETACH(wDlg));
	}
}

static void
WindowGainedFocus(AG_Event *event)
{
	AG_Object *obj = AG_PTR(1);
	const char **s;

	AG_MutexLock(&objLock);
	objFocus = NULL;
	for (s = &esEditableClasses[0]; *s != NULL; s++) {
		if (AG_ObjectIsClass(obj, *s)) {
			objFocus = obj;
			break;
		}
	}
	AG_MutexUnlock(&objLock);
}

static void
WindowLostFocus(AG_Event *event)
{
	AG_MutexLock(&objLock);
	objFocus = NULL;
	AG_MutexUnlock(&objLock);
}

static __inline__ char *
ShortFilename(char *name)
{
	char *s;

	if ((s = strrchr(name, PATHSEP)) != NULL && s[1] != '\0') {
		return (&s[1]);
	} else {
		return (name);
	}
}

/* Invoked by libcore to set up an edition window for an object. */
static AG_Window *
ObjectOpenHandler(void *p)
{
	AG_Object *obj = p;
	AG_Window *win;

	win = obj->cls->edit(obj);
	AG_SetEvent(win, "window-close", SaveChangesDlg, "%p", obj);
	AG_AddEvent(win, "window-gainfocus", WindowGainedFocus, "%p", obj);
	AG_AddEvent(win, "window-lostfocus", WindowLostFocus, "%p", obj);
	AG_AddEvent(win, "window-hidden", WindowLostFocus, "%p", obj);
	AG_SetPointer(win, "object", obj);
	if (obj->archivePath != NULL) {
		AG_WindowSetCaption(win, "%s", ShortFilename(obj->archivePath));
	} else {
		AG_WindowSetCaption(win, _("Untitled"));
	}
	AG_WindowShow(win);
	return (win);
}

/* Invoked by libcore to close edition windows associated with an object. */
static void
ObjectCloseHandler(void *p)
{
	AG_Window *win;
	void *wObj;

	TAILQ_FOREACH(win, &agView->windows, windows) {
		if (AG_GetProp(p, "object", AG_PROP_POINTER, (void *)&wObj) &&
		    wObj == p)
			AG_ViewDetach(win);
	}
}

static void
NewObject(AG_Event *event)
{
	AG_ObjectClass *cls = AG_PTR(1);
	AG_Object *obj;

	obj = AG_ObjectNew(&esVfsRoot, NULL, cls);
	ES_OpenObject(obj);
	AG_PostEvent(NULL, obj, "edit-open", NULL);
}

#if 0
static void
EditDevice(AG_Event *event)
{
	AG_Object *dev = AG_PTR(1);

	ES_OpenObject(dev);
}
#endif

static void
SetArchivePath(void *obj, const char *path)
{
	const char *c;

	AG_ObjectSetArchivePath(obj, path);

	if ((c = strrchr(path, PATHSEP)) != NULL && c[1] != '\0') {
		AG_ObjectSetName(obj, "%s", &c[1]);
	} else {
		AG_ObjectSetName(obj, "%s", path);
	}
}

/* Load an object file from native Edacious format. */
static void
OpenNativeObject(AG_Event *event)
{
	AG_ObjectClass *cls = AG_PTR(1);
	char *path = AG_STRING(2);
	AG_Object *obj;

	obj = AG_ObjectNew(&esVfsRoot, NULL, cls);
	if (AG_ObjectLoadFromFile(obj, path) == -1) {
		AG_TextMsg(AG_MSG_ERROR, "%s: %s", ShortFilename(path),
		    AG_GetError());
#if 0
		AG_ObjectDetach(obj);		/* XXX */
		AG_ObjectDestroy(obj);
#endif
		return;
	}
	SetArchivePath(obj, path);
	ES_OpenObject(obj);
}

static void
OpenDlg(AG_Event *event)
{
	AG_Window *win;
	AG_FileDlg *fd;
	AG_FileType *ft;
	AG_Pane *hPane;

	win = AG_WindowNew(0);
	AG_WindowSetCaption(win, _("Open..."));

	fd = AG_FileDlgNewMRU(win, "edacious.mru.circuits",
	    AG_FILEDLG_LOAD|AG_FILEDLG_EXPAND|AG_FILEDLG_CLOSEWIN);
	AG_FileDlgSetOptionContainer(fd, AG_BoxNewVert(win, AG_BOX_HFILL));

	AG_FileDlgAddType(fd, _("Edacious circuit model"), "*.ecm",
	    OpenNativeObject, "%p", &esCircuitClass);
	AG_FileDlgAddType(fd, _("Edacious component schematic"), "*.eschem",
	    OpenNativeObject, "%p", &esSchemClass);

	AG_WindowShow(win);
}

/* Save an object file in native Edacious format. */
static void
SaveNativeObject(AG_Event *event)
{
	AG_Object *obj = AG_PTR(1);
	char *path = AG_STRING(2);
	AG_Window *wEdit;

	if (AG_ObjectSaveToFile(obj, path) == -1) {
		AG_TextMsgFromError();
	}
	SetArchivePath(obj, path);

	if ((wEdit = AG_WindowFindFocused()) != NULL)
		AG_WindowSetCaption(wEdit, "%s", ShortFilename(path));
}

static void
SaveCircuitToGerber(AG_Event *event)
{
//	ES_Circuit *ckt = AG_PTR(1);
//	char *path = AG_STRING(2);

	AG_TextMsg(AG_MSG_ERROR, "Export to Gerber not implemented yet");
}

static void
SaveCircuitToXGerber(AG_Event *event)
{
//	ES_Circuit *ckt = AG_PTR(1);
//	char *path = AG_STRING(2);

	AG_TextMsg(AG_MSG_ERROR, "Export to XGerber not implemented yet");
}

static void
SaveCircuitToSPICE3(AG_Event *event)
{
	ES_Circuit *ckt = AG_PTR(1);
	char *path = AG_STRING(2);
	
	if (ES_CircuitExportSPICE3(ckt, path) == -1)
		AG_TextMsg(AG_MSG_ERROR, "%s: %s", OBJECT(ckt)->name,
		    AG_GetError());
}

static void
SaveCircuitToPDF(AG_Event *event)
{
//	ES_Circuit *ckt = AG_PTR(1);
//	char *path = AG_STRING(2);

	/* TODO */
	AG_TextMsg(AG_MSG_ERROR, "Export to PDF not implemented yet");
}

static void
SaveSchem(AG_Event *event)
{
//	ES_Circuit *ckt = AG_PTR(1);
//	char *path = AG_STRING(2);

	/* TODO */
	AG_TextMsg(AG_MSG_ERROR, "Export to PDF not implemented yet");
}

static void
SaveSchemToPDF(AG_Event *event)
{
//	ES_Schem *scm = AG_PTR(1);
//	char *path = AG_STRING(2);

	/* TODO */
	AG_TextMsg(AG_MSG_ERROR, "Export to PDF not implemented yet");
}

static void
SaveAsDlg(AG_Event *event)
{
	AG_Object *obj = AG_PTR(1);
	AG_Window *win;
	AG_FileDlg *fd;
	AG_FileType *ft;

	win = AG_WindowNew(0);
	AG_WindowSetCaption(win, _("Save %s as..."), obj->name);

	fd = AG_FileDlgNewMRU(win, "edacious.mru.circuits",
	    AG_FILEDLG_SAVE|AG_FILEDLG_CLOSEWIN|AG_FILEDLG_EXPAND);
	AG_FileDlgSetOptionContainer(fd, AG_BoxNewVert(win, AG_BOX_HFILL));

	if (AG_ObjectIsClass(obj, "ES_Circuit:*")) {
		AG_FileDlgAddType(fd, _("Edacious Circuit Model"),
		    "*.ecm",
		    SaveNativeObject, "%p", obj);
		ft = AG_FileDlgAddType(fd, _("SPICE3 netlist"),
		    "*.cir",
		    SaveCircuitToSPICE3, "%p", obj);
		AG_FileDlgAddType(fd, _("Portable Document Format"),
		    "*.pdf",
		    SaveCircuitToPDF, "%p", obj);
		/* ... */
	} else if (AG_ObjectIsClass(obj, "ES_Layout:*")) {
		AG_FileDlgAddType(fd, _("Edacious Circuit Layout"),
		    "*.ecl",
		    SaveNativeObject, "%p", obj);
		AG_FileDlgAddType(fd, _("Gerber (RS-274D)"),
		    "*.gbr,*.phd,*.spl,*.art",
		    SaveCircuitToGerber, "%p", obj);
		AG_FileDlgAddType(fd, _("Extended Gerber (RS-274X)"),
		    "*.gbl,*.gtl,*.gbs,*.gts,*.gbo,*.gto",
		    SaveCircuitToXGerber, "%p", obj);
	} else if (AG_ObjectIsClass(obj, "ES_Schem:*")) {
		AG_FileDlgAddType(fd, _("Edacious Component Schematic"),
		    "*.eschem",
		    SaveNativeObject, "%p", obj);
		AG_FileDlgAddType(fd, _("Portable Document Format"),
		    "*.pdf",
		    SaveSchemToPDF, "%p", obj);
	}
	AG_WindowShow(win);
}

static void
Save(AG_Event *event)
{
	AG_Object *obj = AG_PTR(1);

	if (OBJECT(obj)->archivePath == NULL) {
		SaveAsDlg(event);
		return;
	}
	if (AG_ObjectSave(obj) == -1) {
		AG_TextMsg(AG_MSG_ERROR, _("Error saving object: %s"),
		    AG_GetError());
	} else {
		AG_TextInfo(_("Saved object %s successfully"),
		    OBJECT(obj)->name);
	}
}

static void
ConfirmQuit(AG_Event *event)
{
	SDL_Event nev;

	nev.type = SDL_USEREVENT;
	SDL_PushEvent(&nev);
}

static void
AbortQuit(AG_Event *event)
{
	AG_Window *win = AG_PTR(1);

	agTerminating = 0;
	AG_ViewDetach(win);
}

static void
Quit(AG_Event *event)
{
	AG_Object *obj;
	AG_Window *win;
	AG_Box *box;

	if (agTerminating) {
		ConfirmQuit(NULL);
	}
	agTerminating = 1;

	OBJECT_FOREACH_CHILD(obj, &esVfsRoot, ag_object) {
		if (AG_ObjectChanged(obj))
			break;
	}
	if (obj == NULL) {
		ConfirmQuit(NULL);
	} else {
		if ((win = AG_WindowNewNamed(AG_WINDOW_MODAL|AG_WINDOW_NOTITLE|
		    AG_WINDOW_NORESIZE, "QuitCallback")) == NULL) {
			return;
		}
		AG_WindowSetCaption(win, _("Exit application?"));
		AG_WindowSetPosition(win, AG_WINDOW_CENTER, 0);
		AG_WindowSetSpacing(win, 8);
		AG_LabelNewStaticString(win, 0,
		    _("There is at least one object with unsaved changes.  "
	              "Exit application?"));
		box = AG_BoxNewHoriz(win, AG_BOX_HOMOGENOUS|AG_VBOX_HFILL);
		AG_BoxSetSpacing(box, 0);
		AG_BoxSetPadding(box, 0);
		AG_ButtonNewFn(box, 0, _("Discard changes"),
		    ConfirmQuit, NULL);
		AG_WidgetFocus(AG_ButtonNewFn(box, 0, _("Cancel"),
		    AbortQuit, "%p", win));
		AG_WindowShow(win);
	}
}

static void
FileMenu(AG_Event *event)
{
	AG_MenuItem *m = AG_SENDER();
	AG_MenuItem *node, *node2;

	AG_MenuActionKb(m, _("New circuit..."), esIconCircuit.s,
	    SDLK_n, KMOD_CTRL,
	    NewObject, "%p", &esCircuitClass);
	AG_MenuAction(m, _("New component schematic..."), esIconComponent.s,
	    NewObject, "%p", &esSchemClass);

	AG_MenuActionKb(m, _("Open..."), agIconLoad.s,
	    SDLK_o, KMOD_CTRL,
	    OpenDlg, NULL);

	AG_MutexLock(&objLock);
	if (objFocus == NULL) { AG_MenuDisable(m); }

	AG_MenuActionKb(m, _("Save"), agIconSave.s,
	    SDLK_s, KMOD_CTRL,
	    Save, "%p", objFocus);
	AG_MenuAction(m, _("Save as..."), agIconSave.s,
	    SaveAsDlg, "%p", objFocus);
	
	if (objFocus == NULL) { AG_MenuEnable(m); }
	AG_MutexUnlock(&objLock);
	
	AG_MenuSeparator(m);

#if 0
	node = AG_MenuNode(m, _("Devices"), NULL);
	{
		ES_Device *dev;

		AG_MenuAction(node, _("Microcontroller..."), agIconDoc.s,
		    NewObject, "%p", &esMCU);
		AG_MenuAction(node, _("Oscilloscope..."), agIconDoc.s,
		    NewObject, "%p", &esOscilloscopeClass);
		AG_MenuAction(node, _("Audio input..."), agIconDoc.s,
		    NewObject, "%p", &esAudioInputClass);
		AG_MenuAction(node, _("Audio output..."), agIconDoc.s,
		    NewObject, "%p", &esAudioOutputClass);
		AG_MenuAction(node, _("Digital input..."), agIconDoc.s,
		    NewObject, "%p", &esDigitalInputClass);
		AG_MenuAction(node, _("Digital output..."), agIconDoc.s,
		    NewObject, "%p", &esDigitalOutputClass);

		AG_MenuSeparator(node);

		OBJECT_FOREACH_CLASS(dev, &esVfsRoot, es_device,
		    "ES_Device:*") {
			AG_MenuAction(node, OBJECT(dev)->name, agIconGear.s,
			    EditDevice, "%p", dev);
		}
	}
#endif
	AG_MenuSeparator(m);
	AG_MenuActionKb(m, _("Quit"), agIconClose.s,
	    SDLK_q, KMOD_CTRL,
	    Quit, NULL);
}

static void
Undo(AG_Event *event)
{
	/* TODO */
	printf("undo!\n");
}

static void
Redo(AG_Event *event)
{
	/* TODO */
	printf("redo!\n");
}

static void
EditMenu(AG_Event *event)
{
	AG_MenuItem *m = AG_SENDER();
	
	AG_MutexLock(&objLock);
	if (objFocus == NULL) { AG_MenuDisable(m); }
	AG_MenuActionKb(m, _("Undo"), NULL, SDLK_z, KMOD_CTRL,
	    Undo, "%p", objFocus);
	AG_MenuActionKb(m, _("Redo"), NULL, SDLK_r, KMOD_CTRL,
	    Redo, "%p", objFocus);
	if (objFocus == NULL) { AG_MenuEnable(m); }
	AG_MutexUnlock(&objLock);
}

int
main(int argc, char *argv[])
{
	Uint videoFlags = AG_VIDEO_OPENGL_OR_SDL|AG_VIDEO_RESIZABLE;
	int c, i, fps = -1;
	char *s;

#ifdef ENABLE_NLS
	bindtextdomain("edacious", LOCALEDIR);
	bind_textdomain_codeset("edacious", "UTF-8");
	textdomain("edacious");
#endif
	if (AG_InitCore("edacious", 0) == -1) {
		fprintf(stderr, "%s\n", AG_GetError());
		return (1);
	}
	AG_TextParseFontSpec("_agFontVera:12");
#ifdef HAVE_GETOPT
	while ((c = getopt(argc, argv, "?vdt:r:T:t:gG")) != -1) {
		extern char *optarg;

		switch (c) {
		case 'v':
			exit(0);
		case 'd':
			agDebugLvl = 2;
			break;
		case 'r':
			fps = atoi(optarg);
			break;
		case 'T':
			AG_SetString(agConfig, "font-path", "%s", optarg);
			break;
		case 't':
			AG_TextParseFontSpec(optarg);
			break;
#ifdef HAVE_OPENGL
		case 'g':
			fprintf(stderr, "Forcing GL mode\n");
			videoFlags &= ~(AG_VIDEO_OPENGL_OR_SDL);
			videoFlags |= AG_VIDEO_OPENGL;
			break;
		case 'G':
			fprintf(stderr, "Forcing SDL mode\n");
			videoFlags &= ~(AG_VIDEO_OPENGL_OR_SDL);
			break;
#endif
		case '?':
		default:
			printf("Usage: %s [-vd] [-r fps] "
			       "[-T font-path] [-t fontspec]", agProgName);
#ifdef HAVE_OPENGL
			printf(" [-gG]");
#endif
			printf("\n");
			exit(0);
		}
	}
#endif /* HAVE_GETOPT */

	/* Setup the display. */
	if (AG_InitVideo(800, 600, 32, videoFlags) == -1) {
		fprintf(stderr, "%s\n", AG_GetError());
		return (-1);
	}
	AG_SetRefreshRate(fps);
	AG_BindGlobalKey(SDLK_ESCAPE, KMOD_NONE, AG_Quit);
	AG_BindGlobalKey(SDLK_F8, KMOD_NONE, AG_ViewCapture);

	/* Initialize the Edacious libraries. */
	ES_CoreInit();
	ES_GenericInit();
	ES_MacroInit();
	ES_SourcesInit();
	
	/* Configure our editor handlers. */
	ES_SetObjectOpenHandler(ObjectOpenHandler);
	ES_SetObjectCloseHandler(ObjectCloseHandler);

	/* Initialize our object management lock. */
	AG_MutexInit(&objLock);

	/* Create the application menu. */ 
	appMenu = AG_MenuNewGlobal(0);
	AG_MenuDynamicItem(appMenu->root, _("File"), NULL, FileMenu, NULL);
	AG_MenuDynamicItem(appMenu->root, _("Edit"), NULL, EditMenu, NULL);

#if defined(HAVE_AGAR_DEV) && defined(DEBUG)
	DEV_InitSubsystem(0);
	DEV_ToolMenu(AG_MenuNode(appMenu->root, _("Debug"), NULL));
#endif

#ifdef HAVE_GETOPT
	for (i = optind; i < argc; i++) {
#else
	for (i = 1; i < argc; i++) {
#endif
		AG_ObjectClass *cls = &esCircuitClass;
		ES_Circuit *ckt;
		char *ext;

		if ((ext = strrchr(argv[i], '.')) != NULL) {
			if (strcasecmp(ext, ".ecm") == 0) {
				cls = &esCircuitClass;
			} else if (strcasecmp(ext, ".eschem") == 0) {
				cls = &esSchemClass;
			}
		}

		ckt = AG_ObjectNew(&esVfsRoot, NULL, cls);
		if (AG_ObjectLoadFromFile(ckt, argv[i]) == 0) {
			AG_ObjectSetArchivePath(ckt, argv[i]);
			ES_OpenObject(ckt);
		} else {
			AG_TextMsgFromError();
			AG_ObjectDetach(ckt);
			AG_ObjectDestroy(ckt);
		}
	}

	AG_EventLoop();
	AG_ObjectDestroy(&esVfsRoot);
	AG_Destroy();
	return (0);
}