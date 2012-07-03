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
/*** cimagelist.h ************************************************************/

#ifndef __CIMAGELIST_H__
#define __CIMAGELIST_H__

#include <SDL.h>

#include "pqueue.h"
#include "cimage.h"
#include "texturepool.h"

/* LRU list of cimages structure */

struct _CIList
{
#ifdef CI_THREADS
    SDL_mutex *mutex;
#endif

    char *img_fmt;              /* format string for name -> JPEG filename */
    char *hir_fmt;              /* format string for name -> JPEG filename */
    CImage *hir;                /* which one is in hir tid */
    GLuint hir_tid;

    PQueue *pq_imgprep;         /* queue of image prep requests */
    PQueue *pq_hirprep;         /* queue of image prep requests */
    TPool *tpool;               /* GL tid pools */

    GLuint gl_list;             /* 1 QUAD */

    int rcount;
    CImage **list;              /* the list */
};

typedef struct _CIList CIList;

/* the methods */

CIList *cil_create(TPool *, char *, char *); /* create new list */
CImage *cil_ci_create(CIList *, char *); /* given sdb filename */

void cil_clear(CIList *);       /* clear list */
void cil_delete(CIList *);      /* clear and delete list */
void cil_wipe(CIList *);        /* mark list unused */
void cil_cleanup(CIList *);     /* remove images with refcount=0, unload unused textures */

void cil_q_imgprep(CIList *, CImage *, int); /* get image jpeg loaded, prep for texture load */
void *cil_primer_imgprep(void *);   /* thread that fulfills cil_img requests */

void cil_q_hirprep(CIList *, CImage *, int); /* get image jpeg loaded, prep for texture load */
void *cil_primer_hirprep(void *);   /* thread that fulfills cil_q_hir requests */

void cil_db_load_dir(CIList *, char *); /* create all cis, load all sdbs */
void cil_load_scidb(CIList *, char *); /* load from this one db file */
void cil_ci_imgprep(CIList *, CImage *, int); /* load and prep images to n levels */

/* CImage-specific ops: */
CImage *cil_ci_move(CIList *, CImage *, float *cx, float *cy, float *zoom,
                    float dx, float dy, float dz);
    /* updates cx cy and zoom by dx dy dz and returns passed or new CImage */

/* GL functions, only for use in mainline thread */
    /* render m to n levels, with crop box */
void cil_ci_render(CIList *, CImage *, int, int, float, float, float, float, float, float);

#endif /* __CIMAGELIST_H__ */

/*** EOF *********************************************************************/
