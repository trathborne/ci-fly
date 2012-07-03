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
/*** texturepool.c ***********************************************************/

#include <stdlib.h>
#include <GL/glu.h>
#include "texturepool.h"

TPool *
tp_create(unsigned int texcount, unsigned int maxsize)
{
    GLenum error;

    TPool *newp = (TPool *) calloc(1, sizeof(TPool));

#ifdef CI_THREADS
    newp->mutex = SDL_CreateMutex();
#endif

    newp->count = texcount;
    newp->maxsize = maxsize;
    newp->status = (char *) calloc(texcount, sizeof(char));

    newp->tids = (GLuint *) calloc(texcount, sizeof(GLuint));

    error = glGetError();
    if (error != GL_NO_ERROR)
        printf("Error: %s\n", gluErrorString(error));

    glGenTextures(texcount, newp->tids);

    return newp;
}

void
tp_delete(TPool * tp)
{
#ifdef CI_THREADS
    SDL_mutexP(tp->mutex);
#endif
    free(tp->tids);
    free(tp->status);
#ifdef CI_THREADS
    SDL_DestroyMutex(tp->mutex);
#endif
    free(tp);
    return;
}

GLuint
tp_get(TPool * tp)
{
    GLuint i = 0;
    GLuint found = 0;

#ifdef CI_THREADS
    SDL_mutexP(tp->mutex);
#endif

    while (found == 0 && i < tp->count) {
        if ((tp->status[i] == 0) && (tp->tids[i] != 0)) {
            tp->status[i] = 1;
            found = tp->tids[i];
        } else {
            i++;
        }
    }

    // printf("Using tid #%d: %d\n", i, found);

#ifdef CI_THREADS
    SDL_mutexV(tp->mutex);
#endif

    return found;
}

void
tp_free(TPool * tp, GLuint tn)
{
    GLuint i = 0;

#ifdef CI_THREADS
    SDL_mutexP(tp->mutex);
#endif

    while (i < tp->count) {
        if (tp->tids[i] == tn) {
            tp->status[i] = 0;
            i = tp->count;
        }
        i++;
    }

#ifdef CI_THREADS
    SDL_mutexV(tp->mutex);
#endif

    return;
}

/*** EOF *********************************************************************/
