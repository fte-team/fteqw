#ifndef RENDERQUE_H
#define RENDERQUE_H

void RQ_AddDistReorder(void (*render) (void *, void *), void *data1, void *data2, float *pos);

void RQ_RenderDistAndClear(void);

typedef struct renderque_s
{
	struct renderque_s *next;
	void (*render) (void *data1, void *data2);
	void *data1;
	void *data2;
} renderque_t;

#endif
