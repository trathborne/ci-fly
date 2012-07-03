/*

    This file is part of
    Continuous Imaging 'fly'
    Copyright (C) 2008-2012  David Lowy & Tom Rathborne

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/
/*** pqueue.h ****************************************************************/

#ifndef __PQUEUE_H__
#define __PQUEUE_H__

#include <SDL.h>
#ifdef CI_THREADS
#include <SDL_thread.h>
#endif

/* a priority queue is a head node with data=NULL, following nodes are real */

/* things of same priority are FILO */

/* the pqueue + node structure */

struct _PQueue
{
#ifdef CI_THREADS
    SDL_mutex *mutex;           /* only used in head - :| */
    SDL_sem *semaphore;         /* only used in head - :| */
#endif
    int priority;               /* lower is higher priority */
    void *data;                 /* the enqueued data */
    struct _PQueue *next;       /* embedded linked list */
};

typedef struct _PQueue PQueue;

/* the methods */

PQueue *pq_create();            /* create a new empty queue */
void pq_delete(PQueue *);       /* delete an existing queue */
void pq_enqueue(PQueue *, int, void *); /* enqueue, priority and data */
void *pq_dequeue(PQueue *, int *); /* get next */
void pq_clear(PQueue *);        /* empty queue */

#endif /* __PQUEUE_H__ */

/*** EOF *********************************************************************/
