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
/*** cimage.c ****************************************************************/

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <GL/glu.h>

#include "cimage.h"

#include <jpeglib.h>

#define CI_MIN_FILTER GL_LINEAR
#define CI_MAG_FILTER GL_LINEAR

/* offsets of siblings */

                  // L=0   R=1  B=2   T=3
GLfloat sib_x[8] = { 0.0,  0.0, 1.0, -1.0, 1.0, -1.0,  1.0, -1.0 };
GLfloat sib_y[8] = { 1.0, -1.0, 0.0,  0.0, 1.0, -1.0, -1.0,  1.0 };

#define SIB_L 0
#define SIB_R 1
#define SIB_B 2
#define SIB_T 3

/* API functions */

CImage *
ci_create (char *name)
{
    CImage *ci;

    ci = (CImage *) calloc(1, sizeof(CImage));

    /* explicit defaults despite use of calloc */
    ci->inq = 0;

    ci->name = malloc(strlen(name) + 1);
    strncpy(ci->name, name, strlen(name) + 1);

    ci->parent = 0;
    ci->px = 0;
    ci->py = 0;

    ci->sx = 0;
    ci->sy = 0;
    ci->subs = 0;

    ci->channels = 0;
    ci->iw = 0;
    ci->ih = 0;
    ci->idata = 0;

    ci->tloaded = 0;
    ci->tw = 0;
    ci->th = 0;
    ci->tdata = 0;
    ci->tp = 0;
    ci->tid = 0;

    ci->next = 0;

    return ci;
}

void
ci_delete(CImage * ci)
{
    if (ci->name) free (ci->name);
    if (ci->subs) free (ci->subs);
    if (ci->gl_list) glDeleteLists(ci->gl_list, 1);
    free (ci);
    /* FIXME - drop data, clean up texture stuff */
}

int
ci_is_img_loaded(CImage * ci)
{
    return (ci->idata ? 1 : 0);
}

inline int
ci_is_tx_ready(CImage * ci)
{
    return (ci->tdata ? 1 : 0);
}

inline int
ci_is_tx_loaded(CImage * ci)
{
    return (ci->tloaded ? 1 : 0);
}

void
ci_do_img(CImage * ci, char *img_fmt)
{                               /* load jpeg data using fmt */
    FILE *file;
    struct jpeg_decompress_struct decomp;
    struct jpeg_error_mgr jperrs;
    char *img_fname;
    unsigned char *data;

    // if(ci->idata) return;
    if (ci->idata) free(ci->idata);
    ci->idata = 0;

    img_fname = malloc(strlen(ci->name) + strlen(img_fmt) + 1);
    sprintf(img_fname, img_fmt, ci->name);

    int i, rowlength;
    unsigned char *row;

    // printf("Reading %s\n", img_fname);
    file = fopen(img_fname, "r");
    if (!file) {
        printf("ERROR: %s\n", strerror(errno));
        assert(file);
    }

    decomp.err = jpeg_std_error(&jperrs);
    jpeg_create_decompress(&decomp);
    jpeg_stdio_src(&decomp, file);
    jpeg_read_header(&decomp, TRUE);

    ci->channels = decomp.num_components;
    // printf("JPEG channels: %d\n", decomp.num_components);
    ci->iw = decomp.image_width;
    ci->ih = decomp.image_height;

    rowlength = ci->iw * ci->channels;
    data = calloc(1, rowlength * ci->ih);

    jpeg_start_decompress(&decomp);

    /* store in reverse order since OpenGL uses mathie-coords */
    for (i = ci->ih - 1; i >= 0; i--) {
        row = data + i * rowlength;
        //assert(
            jpeg_read_scanlines(&decomp, &row, 1);
        //== 1);
    }

    jpeg_finish_decompress(&decomp);
    jpeg_destroy_decompress(&decomp);

    fclose(file);

    ci->idata = data;           /* ATOMIC transaction */

    if (ci->channels == 1) {    /* FIXME - small race condition */
        ci_torgb(ci);
    }

    return;
}

void
ci_torgb(CImage * ci)
{
    char pixel;
    unsigned char *olddata, *newdata, *ptr;
    int i, i3, j, oldrowlength, newrowlength;

    if (ci->channels == 3)
        return;

    // implicit else

    oldrowlength = ci->iw;
    newrowlength = ci->iw * 3;

    olddata = ci->idata;
    newdata = calloc(1, ci->iw * ci->ih * 3);

    for (i = 0; i < ci->iw; i++) {
        i3 = i * 3;
        for (j = 0; j < ci->ih; j++) {
            pixel = *(olddata + (j * oldrowlength) + i);

            ptr = (unsigned char *) (newdata + (j * newrowlength) + i3);
            *ptr = pixel;
            ptr++;
            *ptr = pixel;
            ptr++;
            *ptr = pixel;
        }
    }

    free(ci->idata);
    ci->idata = newdata;
    ci->channels = 3;

    return;
}

void
ci_do_prep(CImage * ci, TPool * tp)
{                               /* scale image to target size, allocate texture id */
    if (!ci_is_img_loaded(ci)) {
        assert(0);
        return;
    }

    if(ci->tdata) return;

    int maxsize = 0;

    if (ci->tp) {
        if (ci->tp->maxsize < tp->maxsize) {
            tp_free(ci->tp, ci->tid);
            ci->tp = tp;
            ci->tid = tp_get(ci->tp);
        }
    } else {
        ci->tp = tp;
        ci->tid = tp_get(ci->tp);
    }

    maxsize = ci->tp->maxsize;

    if (maxsize) {
        if (ci->iw > maxsize || ci->ih > maxsize) {
            if (ci->iw > ci->ih) {
                ci->tw = maxsize;
                ci->th = ((double) ci->ih / (double) ci->iw) * maxsize;
            } else {
                ci->th = maxsize;
                ci->tw = ((double) ci->iw / (double) ci->ih) * maxsize;
            }
        } else {
            ci->tw = ci->iw;
            ci->th = ci->ih;
        }

        /* FIXME - scale this data! */
        ci->tdata = ci->idata;  /* ATOMIC */
    } else {
        ci->tw = ci->iw;
        ci->th = ci->ih;
        ci->tdata = ci->idata;  /* ATOMIC */
    }

    // printf("Prepared image %s, tid=%d, maxsize=%d.\n", ci->name, ci->tid, maxsize);
}

inline int
ci_load_tx(CImage * ci, int levels)
{                               /* very quick - load texture */
    CImage *sci;
    // GLenum error;
    int x, y;

    if (ci_is_tx_ready(ci) && !ci_is_tx_loaded(ci)) {

        // assert(ci->tid);

        if (ci->channels != 3) {
            return 0;
        }

        //printf("Loading MX tx %s tid %d (%d channels) from 0x%lX\n", ci->name, ci->tid, ci->channels, (long int) ci->tdata);

        glBindTexture(GL_TEXTURE_2D, ci->tid);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, CI_MAG_FILTER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, CI_MIN_FILTER);
        glTexImage2D(GL_TEXTURE_2D, 0, ci->channels, ci->tw, ci->th, 0,
                     ci->channels == 3 ? GL_RGB : GL_LUMINANCE, GL_UNSIGNED_BYTE, ci->tdata);

/*
        error = glGetError();
        if (error != GL_NO_ERROR) {
            printf("Texture Load Error: %s\n", gluErrorString(error));
            return 0;
        }
*/
        ci->tloaded = 1;
    } else {
        assert(0); /* should never be called when not needed */
    }

    if (levels > 0) {
        levels--;
        for (x = 0; x < ci->sx; x++) {
            for (y = 0; y < ci->sy; y++) {

                if ( (sci = ci->subs[x + (ci->sx * y)])
                        && !ci_is_tx_loaded(sci)
                        && ci_is_tx_ready(sci) ) {
                    ci_load_tx(sci, levels);
                }
            }
        }
    }

    return 1;
}

inline int
ci_load_hir(CImage * ci, GLuint tid)
{                               /* very quick - load texture */

    glBindTexture(GL_TEXTURE_2D, tid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, CI_MAG_FILTER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, CI_MIN_FILTER);
    glTexImage2D(GL_TEXTURE_2D, 0, ci->channels, ci->iw, ci->ih, 0,
                 ci->channels == 3 ? GL_RGB : GL_LUMINANCE, GL_UNSIGNED_BYTE, ci->idata);

    // printf("Loaded HIR %d x %d\n", ci->iw, ci->ih);
    return 1;
}

void
ci_mklist(CImage *ci, GLuint gl_list)
{
    int x, y;
    CImage *sci;

    ci->gl_list = glGenLists(1);

    glNewList(ci->gl_list, GL_COMPILE);
        glPushMatrix();

        for (x = 0; x < ci->sx; x++) {
            for (y = 0; y < ci->sy; y++) {
                sci = ci->subs[ x + (ci->sx * ( (x % 2) ? (ci->sy - 1 - y) : y) ) ];

                glBindTexture(GL_TEXTURE_2D, sci->tid);
                glCallList(gl_list);

                if( y == (ci->sy - 1)) { // end of column
                    glTranslatef(1.0, 0.0, 0.0);
                } else {
                    glTranslatef(0.0, (x % 2) ? -1.0 : 1.0, 0.0);
                }

            }
        }

        glPopMatrix();
    glEndList();
}

inline void
ci_set_siblings(CImage *ci)
{
    int i, x, y;
    CImage *pci = ci->parent;

    for ( i = 0; i < 8; i++ ) { // 8 siblings
        x = ci->px + sib_x[i];
        y = ci->py + sib_y[i];

        if ((x >= 0) && (x < ci->sx) && (y >= 0) && (y < ci->sy) ) { // simple case
            ci->sibs[i] = pci->subs[x + (ci->sx * y)];
        } else {
            // printf("FIXME Sibling %d,%d\n", x, y); // TODO: handle this XXX
            ci->sibs[i] = ci;
        }
    }

    return;
}

inline void
ci_set_parent(CImage *ci, int x, int y)
{
    CImage *nci = ci->subs[x + (ci->sx * y)];

    nci->parent = ci;
    nci->px = x;
    nci->py = y;

    ci_set_siblings(nci);
}

/* updates cx cy and zoom by dx dy dz and returns passed or new CImage */
inline CImage *
ci_zoom_to(CImage * ci, double *cx, double *cy, double *ztx, double *zty, double *mag)
{
    CImage *nci;
    int x, y;

    // pick sibling if cx,cy outside of ci - 4 cases
    while (*cx < -0.5) {
        (*cx) += 1.0;
        (*ztx) += 1.0;

        if (ci->px == 0) { // left edge
            nci = ci->parent->sibs[SIB_L];
            ci_set_parent(nci, nci->sx - 1, ci->py); // reparent so siblingness is symmetric
            ci = nci->subs[ (nci->sx - 1) + (nci->sx * ci->py)];
        } else {
            ci_set_parent(ci->parent, ci->px - 1, ci->py); // reparent so siblingness is symmetric
            ci = ci->sibs[SIB_L];
        }
    }

    while (*cx > 0.5) {
        (*cx) -= 1.0;
        (*ztx) -= 1.0;

        if (ci->px == (ci->parent->sx - 1)) { // right edge
            nci = ci->parent->sibs[SIB_R];
            ci_set_parent(nci, 0, ci->py); // reparent so siblingness is symmetric
            ci = nci->subs[nci->sx * ci->py];
        } else {
            ci_set_parent(ci->parent, ci->px - 1, ci->py); // reparent so siblingness is symmetric
            ci = ci->sibs[SIB_R];
        }
    }

    while (*cy < -0.5) {
        (*cy) += 1.0;
        (*zty) += 1.0;

        if (ci->py == 0) { // top edge
            nci = ci->parent->subs[SIB_T];
            ci_set_parent(nci, ci->px, nci->sy - 1); // reparent so siblingness is symmetric
            ci = nci->subs[ ci->px + (nci->sx * (nci->sy - 1))];
        } else {
            ci_set_parent(ci->parent, ci->px, ci->py - 1); // reparent so siblingness is symmetric
            ci = ci->sibs[SIB_T];
        }
    }

    while (*cy > 0.5) {
        (*cy) -= 1.0;
        (*zty) -= 1.0;

        if (ci->py == (ci->parent->sy - 1)) { // bottom edge
            nci = ci->parent->subs[SIB_B];
            ci_set_parent(nci, ci->px, 0); // reparent so siblingness is symmetric
            ci = nci->subs[ ci->px ];
        } else {
            ci_set_parent(ci->parent, ci->px, 0); // reparent so siblingness is symmetric
            ci = ci->sibs[SIB_B];
        }
    }

    // process zooms
    if ((*mag) > 1.0) {
        // zoom on subtile
        x = floor((*cx + 0.5) * ci->sx);
        y = floor((*cy + 0.5) * ci->sy);
        x = CI_CLAMP(x, 0, ci->sx);
        y = CI_CLAMP(y, 0, ci->sy);
        nci = ci->subs[x + (ci->sx * y)];
        // assert(nci);
        ci_set_parent(ci, x, y);

        // recalculate centre
        (*cx) = fmod(((*cx) - 0.5)*ci->sx, 1.0) + 0.5;
        (*cy) = fmod(((*cy) - 0.5)*ci->sy, 1.0) + 0.5;
        (*ztx) = fmod(((*ztx) - 0.5)*ci->sx, 1.0) + 0.5;
        (*zty) = fmod(((*zty) - 0.5)*ci->sy, 1.0) + 0.5;

        // recalculate zoom
        (*mag) -= 1.0;

        // return subtile
        ci = nci;

    } else if ((*mag) < 0.0) {
        // assert(ci->parent); /* TODO - handle case where CI has no parent! */

        // get parent
        nci = ci->parent;
        // assert(nci);
        // assert( nci->subs[ci->px + (nci->sx * ci->py)] == ci );

        // recalculate centre
        (*cx) = (((*cx) + 0.5 + (double)ci->px) / (double)(nci->sx)) - 0.5;
        (*cy) = (((*cy) + 0.5 + (double)ci->py) / (double)(nci->sy)) - 0.5;
        (*ztx) = (((*ztx) + 0.5 + (double)ci->px) / (double)(nci->sx)) - 0.5;
        (*zty) = (((*zty) + 0.5 + (double)ci->py) / (double)(nci->sy)) - 0.5;

        // recalculate mag
        (*mag) += 1.0;

        // return parent
        ci = nci;
    }

    return ci;
}

/*** EOF *********************************************************************/
