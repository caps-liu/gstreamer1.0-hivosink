/*
* Hisi Vo Context
*/

#ifndef __GST_HISI_VO_CONTEXT_H__
#define __GST_HISI_VO_CONTEXT_H__

typedef struct _HisiVideoOutputContext HisiVideoOutputContext;

struct _HisiVideoOutputContext {
	int (*open)();
	int (*close)();
	int (*reset)();
	int (*render)(int width, int height, unsigned long addr);
};

extern HisiVideoOutputContext *hisi_vo_context_get();

#endif /*__GST_HISI_VO_CONTEXT_H__*/




