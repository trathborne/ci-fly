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
/*** pqueue.c ****************************************************************/

#include <stdlib.h>
#include "pqueue.h"

PQueue *
pq_create()
{
    PQueue *newq = (PQueue *) calloc(1, sizeof(PQueue));

#ifdef CI_THREADS
    newq->mutex = SDL_CreateMutex();
    newq->semaphore = SDL_CreateSemaphore(0);
#endif

    return newq;
}

void
pq_delete(PQueue * pq)
{
#ifdef CI_THREADS
    SDL_mutexP(pq->mutex);
#endif
    pq->priority = 1;           /* hint pq_clear to not lock */
    pq_clear(pq);
#ifdef CI_THREADS
    SDL_DestroyMutex(pq->mutex);
    SDL_DestroySemaphore(pq->semaphore);
#endif
    free(pq);
    return;
}

void
pq_clear(PQueue * pq)
{
    PQueue *foo, *bar;

#ifdef CI_THREADS
    if (pq->priority == 0)
        SDL_mutexP(pq->mutex);
#endif

    foo = pq->next;

    while (foo) {
#ifdef CI_THREADS
        SDL_SemWait(pq->semaphore);
#endif
        bar = foo->next;
        // free(foo->data);
        free(foo);
        foo = bar;
    }

    pq->next = 0;

#ifdef CI_THREADS
    if (pq->priority == 0)
        SDL_mutexV(pq->mutex);
#endif

    return;
}

inline void
pq_enqueue(PQueue * pq, int priority, void *data)
{
    PQueue *temp = pq;
    PQueue *newq = (PQueue *) calloc(1, sizeof(PQueue));

    newq->data = data;
    newq->priority = priority;

#ifdef CI_THREADS
    SDL_mutexP(pq->mutex);
#endif

    while (temp->next && temp->next->priority < priority)
        temp = temp->next;

    newq->next = temp->next;
    temp->next = newq;

#ifdef CI_THREADS
    SDL_mutexV(pq->mutex);
    SDL_SemPost(pq->semaphore);
#endif

    return;
}

void *
pq_dequeue(PQueue * pq, int *priority)
{
    void *data = 0;
    PQueue *temp;

#ifdef CI_THREADS
    SDL_SemWait(pq->semaphore);
    SDL_mutexP(pq->mutex);
#endif

    if (pq->next) {
        data = pq->next->data;
        *priority = pq->next->priority;
        temp = pq->next;
        pq->next = temp->next;
        free(temp);
    } else {
        *priority = 0;
    }

#ifdef CI_THREADS
    SDL_mutexV(pq->mutex);
#endif

    return data;
}

/*** EOF *********************************************************************/
