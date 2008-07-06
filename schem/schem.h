/*	Public domain	*/

#define ES_SCHEM_NAME_MAX 128

#include "begin_code.h"

/* Component schematic block */
typedef struct es_schem {
	struct ag_object _inherit;
	VG *vg;
} ES_Schem;

#define SCHEM(p) ((ES_Schem *)(p))

#include "schem_port.h"
#include "schem_wire.h"
#include "schem_block.h"

__BEGIN_DECLS
extern AG_ObjectClass esSchemClass;
#ifdef DEBUG
extern VG_ToolOps esSchemProximityTool;
#endif
extern VG_ToolOps esSchemSelectTool;
extern VG_ToolOps esSchemPointTool;
extern VG_ToolOps esSchemLineTool;
extern VG_ToolOps esSchemCircleTool;
extern VG_ToolOps esSchemTextTool;
extern VG_ToolOps esSchemPortTool;

ES_Schem *ES_SchemNew(void *);
void     *ES_SchemNearest(VG_View *, VG_Vector);
__END_DECLS

#include "close_code.h"