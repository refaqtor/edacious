/*	Public domain	*/

#ifndef _COMPONENT_VSOURCE_H_
#define _COMPONENT_VSOURCE_H_

#include <eda.h>

#include "begin_code.h"

typedef struct es_vsource {
	struct es_component com;
	M_Real voltage;			/* current voltage */
	TAILQ_HEAD(,es_loop) loops;	/* Forward loops */
	Uint nloops;
	ES_Port	**lstack;		/* Temporary loop stack */
	Uint nlstack;
} ES_Vsource;

#define VSOURCE(com) ((struct es_vsource *)(com))

__BEGIN_DECLS
extern ES_ComponentClass esVsourceClass;

void	 ES_VsourceFindLoops(ES_Vsource *);
void	 ES_VsourceFreeLoops(ES_Vsource *);
double	 ES_VsourceVoltage(void *, int, int);
int	 ES_VsourceName(void *);
__END_DECLS

#include "close_code.h"
#endif /* _COMPONENT_VSOURCE_H_ */
