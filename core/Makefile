TOP=..
include ${TOP}/Makefile.config

LIB=		es_core
LIB_CURRENT=	1
LIB_REVISION=	0
LIB_AGE=	0
LIB_INSTALL=	Yes
LIB_SHARED=	Yes
LIB_GUID=	"0c09acdb-225a-4b7f-a8f9-edc0045ae18d"
LIB_LINKS=	ag_gui ag_core_static \
		pthreads SDL opengl freetype

SRCS=	core.c \
	circuit.c \
	circuit_edit.c \
	component.c \
	component_edit.c \
	component_library.c \
	component_insert_tool.c \
	dc.c \
	interpreteur.c \
	scope.c \
	sim.c \
	spice.c \
	wire.c \
	wire_tool.c \
	schem.c \
	schem_edit.c \
	schem_library.c \
	schem_block.c \
	schem_port.c \
	schem_port_tool.c \
	schem_select_tool.c \
	schem_wire.c \
	integration.c \
	tools.c \
	layout.c \
	layout_edit.c \
	layout_select_tool.c \
	layout_node.c \
	layout_node_tool.c \
	layout_trace.c \
	layout_trace_tool.c \
	layout_block.c \
	package.c \
	package_library.c \
	package_insert_tool.c

MAN3=	ES_Circuit.3

CFLAGS+=${AGAR_MATH_CFLAGS} ${AGAR_DEV_CFLAGS} ${AGAR_VG_CFLAGS} ${AGAR_CFLAGS}
LIBS=	${AGAR_MATH_LIBS} ${AGAR_DEV_LIBS} ${AGAR_VG_LIBS} ${AGAR_LIBS}

include ${TOP}/mk/build.lib.mk
include ${TOP}/mk/build.man.mk
