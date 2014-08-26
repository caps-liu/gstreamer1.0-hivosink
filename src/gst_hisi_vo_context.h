/*
* Hisi Vo Context
*/

#ifndef __GST_HISI_VO_CONTEXT_H__
#define __GST_HISI_VO_CONTEXT_H__

#include <hi_type.h>
#include <hi_unf_vo.h>
#include <hi_unf_disp.h>
#include <hi_unf_common.h>

typedef struct _HisiVideoOutputContext HisiVideoOutputContext;

struct _HisiVideoOutputContext {
	int (*open)();
	int (*close)();
	int (*reset)();
	int (*render)(int width, int height, unsigned long addr);
	HI_HANDLE (*get_window_handle)();
	
};

extern HisiVideoOutputContext *hisi_vo_context_get();

#endif /*__GST_HISI_VO_CONTEXT_H__*/




