//this is to render transparent things in a distance oriented order

#include "quakedef.h"
#include "renderque.h"

#define NUMGRADUATIONS 0x400
static renderque_t *freerque;
static renderque_t *activerque;
static renderque_t *initialque;

static renderque_t *distlastarque[NUMGRADUATIONS];
static renderque_t *distrque[NUMGRADUATIONS];

int rqmaxgrad, rqmingrad;

int rquesize = 0x2000;

void RQ_AddDistReorder(void (*render) (void *, void *), void *data1, void *data2, float *pos)
{
	int dist;
	vec3_t delta;
	renderque_t *rq;
	if (!freerque)
	{
		render(data1, data2);
		return;
	}

	VectorSubtract(pos, r_refdef.vieworg, delta);
	dist = Length(delta)/4;

	if (dist > rqmaxgrad)
	{
		if (dist >= NUMGRADUATIONS)
			dist = NUMGRADUATIONS-1;
		rqmaxgrad = dist;
	}
	if (dist < rqmingrad)
	{
		if (dist < 0)	//hmm... value wrapped? shouldn't happen
			dist = 0;
		rqmingrad = dist;
	}

	rq = freerque;
	freerque = freerque->next;
	rq->next = NULL;
	if (distlastarque[dist])
		distlastarque[dist]->next = rq;
	distlastarque[dist] = rq;

	rq->render = render;
	rq->data1 = data1;
	rq->data2 = data2;

	if (!distrque[dist])
		distrque[dist] = rq;
}

void RQ_RenderDistAndClear(void)
{
	int i;
	renderque_t *rq;
	for (i = rqmaxgrad; i>=rqmingrad; i--)
//	for (i = rqmingrad; i<=rqmaxgrad; i++)
	{
		for (rq = distrque[i]; rq; rq=rq->next)	
		{
			rq->render(rq->data1, rq->data2);
		}
		if (distlastarque[i])
		{
			distlastarque[i]->next = freerque;
			freerque = distrque[i];
			distrque[i] = NULL;
			distlastarque[i] = NULL;
		}
	}
	rqmaxgrad=0;
	rqmingrad = NUMGRADUATIONS-1;
}

void RQ_Init(void)
{
	int		i;

	if (initialque)
		return;

	initialque = (renderque_t *) Hunk_AllocName (rquesize * sizeof(renderque_t), "renderque");
			
	
	freerque = &initialque[0];
	activerque = NULL;

	for (i=0 ;i<rquesize-1 ; i++)
		initialque[i].next = &initialque[i+1];
	initialque[rquesize-1].next = NULL;
}
