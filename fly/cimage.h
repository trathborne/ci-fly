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
/*** cimage.h ****************************************************************/

#ifndef __CIMAGE_H__
#define __CIMAGE_H__

#include <GL/gl.h>

#include "texturepool.h"

#define CI_CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

struct _CImage
{
    int inq;                    /* flag - in a queue? */
    char *name;                 /* string name of this object */

    /* parent data */
    struct _CImage *parent;     /* last known parent */
    int px, py;                 /* location in l.k. parent */
    struct _CImage *sibs[8];    /* siblings */

    /* tile data */
    int sx, sy;                 /* Dimensions read from that file */
    struct _CImage **subs;      /* sub-cimages, malloc(sizeof(CImage*) * sx*sy) */
    GLuint gl_list;             /* list that shows all children */

    /* image data */
    unsigned int channels;      /* 1 = gray, 3 = RGB */
    int iw, ih;                 /* width and height in pixels */
    unsigned char *idata;       /* image data */

    /* texture data */
    int tloaded;                /* flag - is texture loaded */
    int tw, th;                 /* scaled width and height in pixels */
    unsigned char *tdata;       /* scaled data */
    TPool *tp;                  /* loaded texturepool object, 0 if unloaded */
    GLuint tid;                 /* loaded texture id, 0 if unloaded */

    struct _CImage *next;       /* forward link in CImageList */
};

typedef struct _CImage CImage;

CImage *ci_create(char *);
void ci_delete(CImage *);

void ci_do_img(CImage *, char *); /* load jpeg data using fmt */
void ci_do_prep(CImage *, TPool *); /* prep image for tx load */

int ci_is_tx_ready(CImage *);
int ci_is_tx_loaded(CImage *);

void ci_torgb(CImage *);

CImage *ci_zoom_to(CImage *, double *, double *, double *, double *, double *);
void ci_set_siblings(CImage *);
void ci_set_parent(CImage *, int, int);

/* GL functions, only for use in mainline thread */
int ci_load_tx(CImage *, int);       /* very quick - load textures ... optional recursion */
int ci_load_hir(CImage *, GLuint);       /* very quick - load textures ... optional recursion */
void ci_mklist(CImage *, GLuint); // make gl list of subs

# endif /* __CIMAGE_H __ */

/*** EOF *********************************************************************/
