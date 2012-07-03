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
/*** texturepool.h ***********************************************************/

#ifndef __TEXTUREPOOL_H__
#define __TEXTUREPOOL_H__

#include <SDL.h>
#ifdef CI_THREADS
#include <SDL_thread.h>
#endif
#include <GL/gl.h>

/* pool of GL texture ids */

struct _TPool
{
#ifdef CI_THREADS
    SDL_mutex *mutex;
#endif
    unsigned int count;         /* number of texture ids */
    unsigned int maxsize;       /* maximum pixel dimension */
    GLuint *tids;               /* the ids */
    char *status;               /* corresponding used/free status */
};

typedef struct _TPool TPool;

/* the methods */

TPool *tp_create(unsigned int, unsigned int); /* create new pool with some number of tids */
void tp_delete(TPool *);        /* delete a pool */
GLuint tp_get(TPool *);         /* get a texture id */
void tp_free(TPool *, GLuint);  /* free a texture id */

#endif /* __TEXTUREPOOL_H__ */

/*** EOF *********************************************************************/
