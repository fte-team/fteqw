//a qvm compatable malloc/free interface

//This is seperate from qvm_api.c because this has a chunk of memory that simply isn't needed in all plugins.

#include "plugin.h"

struct memhead_s
{
	int size;
	int isfree;
	struct memhead_s *next;
	struct memhead_s *prev;
};

#ifndef MEMSIZE
#define MEMSIZE 1024*64 //64kb
#endif

static struct memhead_s *head;
static char memory[MEMSIZE];

//we create two dummies at the start and end
//these will never be freed
//we then have dynamic allocation in the middle.
//sizes include the headers

void *malloc(int size)
{
	struct memhead_s *lasthead;

	if (size <= 0)
		return NULL;

	size = ((size+4) & ~3) + sizeof(struct memhead_s);	//round up
	if (!head)
	{	//first call
		struct memhead_s *last;
		struct memhead_s *middle;
		struct memhead_s *first;

		first = (struct memhead_s*)memory;
		last= (struct memhead_s*)((char*)memory - sizeof(struct memhead_s));
		first->size = last->size = sizeof(struct memhead_s);
		first->isfree = last->isfree = false;

		middle = (struct memhead_s*)((char*)first+first->size);
		middle->size = sizeof(memory) - sizeof(struct memhead_s)*3;
		middle->isfree = true;

		last->next = first;
		last->prev = middle;
		first->next = middle;
		first->prev = last;
		middle->next = last;
		middle->prev = first;

		head = middle;
	}
	lasthead = head;

	do
	{
		if (head->isfree)
			if (head->size >= size)
			{
				struct memhead_s *split;
				if (head->size > size + sizeof(struct memhead_s)+1)
				{	//split
					split = (struct memhead_s*)((char*)head + size);
					split->size = head->size - size;
					head->size = size;
					split->next = head->next;
					split->prev = head;
					head->next = split;
					split->next->prev = split;
					split->isfree = true;
					head->isfree = false;
				}
				else
				{	//no point in splitting
					head->isfree = false;
				}
				split = head;
				head = head->next;
				return (char*)split + sizeof(struct memhead_s);
			}
		head = head->next;
	} while (lasthead != head);

	Sys_Errorf("VM Out of memory on allocation of %i bytes\n", size);

	return NULL;
}

static struct memhead_s *mergeblock(struct memhead_s *b1, struct memhead_s *b2)
{
	//b1 and b2 must be in logical order

	b1->next = b2->next;
	b2->next->prev = b1;
	b1->size += b2->size;

	return b1;
}

void free(void *mem)
{	//the foot hopefully isn't going to be freed
	struct memhead_s *block;
	block = (struct memhead_s*)((char*)mem - sizeof(struct memhead_s));

	if (block->isfree)
		Sys_Error("(plugin) Double free\n");
	block->isfree = true;

	if (block->prev->isfree)
	{	//merge previous with this
		block = mergeblock(block->prev, block);
	}
	if (block->next)
	{	//merge next with this
		block = mergeblock(block, block->next);
	}

	head = (struct memhead_s*)memory;
}
