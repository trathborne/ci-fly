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
/*** cimagelist.c ************************************************************/

#include <assert.h>
#include <stdlib.h>

#ifdef CI_MACOSX
#include <errno.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <math.h>

#include <db_185.h>
#include <GL/glu.h>

#include "cimagelist.h"
#include "cimage.h"
#include "texturepool.h"

/* API functions */

CIList *
cil_create(TPool *tpool, char *img_fmt, char *hir_fmt)
{
    CIList *cil = (CIList *) calloc(1, sizeof(CIList));

#ifdef CI_THREADS
    cil->mutex = SDL_CreateMutex();
#endif

    cil->pq_imgprep = pq_create();
    cil->pq_hirprep = pq_create();

    cil->tpool = tpool;
    cil->hir_tid = tp_get(tpool);

    cil->img_fmt = (char *) malloc(strlen(img_fmt) + 1);
    strcpy(cil->img_fmt, img_fmt);
    cil->hir_fmt = (char *) malloc(strlen(hir_fmt) + 1);
    strcpy(cil->hir_fmt, hir_fmt);

    // cil->hash = calloc(CIL_HASH_BUCKETS, sizeof(CImage *));

    cil->gl_list = glGenLists(1);
    glNewList(cil->gl_list, GL_COMPILE);
        glBegin(GL_QUADS);
            glTexCoord2f(0, 0);
            glVertex2f(0, 0);
            glTexCoord2f(0, 1);
            glVertex2f(0, 1);
            glTexCoord2f(1, 1);
            glVertex2f(1, 1);
            glTexCoord2f(1, 0);
            glVertex2f(1, 0);
        glEnd();
    glEndList();

    return cil;
}

void
cil_load_scidb(CIList * cil, char *sdbname)
{
    DB *sdb;
    DBT key, value;

    CImage *ci, *sci, **nsubs;
    short rcount, kv, size, x, y, i, *pt;
    char *name;

    memset(&key, 0, sizeof(key));
    memset(&value, 0, sizeof(value));

    sdb = (DB *) dbopen(sdbname, O_RDONLY, 0600, DB_HASH, NULL);

    if(!sdb) {
        printf("cidb '%s' could not be opened: ", sdbname);
#ifdef CI_MACOSX
        if (errno == EFTYPE) {
            printf("EFTYPE: Incorrect file format\n");
        } else if (errno == EINVAL) {
            printf("EINVAL: Invalid argument\n");
        } else {
            printf("Unrecognized errno: %d\n", errno);
        }
#endif
        exit(1);
    }

    key.size = sizeof(short);
    key.data = &kv;
    kv = 0;

    (sdb->get)(sdb, &key, &value, 0);
    assert(value.size = 2 * sizeof(short));
    assert(value.data);
    pt = value.data;

    rcount = *pt; // first short is rcount
    cil->rcount = rcount;
    pt++;
    size = *pt;     // second short is max data size

    cil->list = malloc(sizeof(CImage *) * rcount);

    for( i = 0; i < rcount; i++) {
        kv = i+1;
        (sdb->get)(sdb, &key, &value, 0);
        pt = value.data;
        // printf("XL index %d = 2 + %d * %d\n", pt[0], pt[1]);
        name = (char *)(pt+2+pt[0]*pt[1]);
        ci = ci_create(name);
        assert(ci);
        cil->list[i] = ci;
        // printf("XL #%d got length %d name %s\n", i+1, (int) value.size, name);
    }

    for( i = 0; i < rcount; i++) {
        ci = cil->list[i];
        kv = i+1;
        (sdb->get)(sdb, &key, &value, 0);
        // printf("YL #%d got length %d\n", i+1, (int) value.size);
        pt = value.data;
        ci->sx = pt[0]; pt++;
        ci->sy = pt[0]; pt++;
        nsubs = (CImage **) malloc(ci->sx * ci->sy * sizeof(CImage *));

        for (x = 0; x < ci->sx; x++) {
            for (y = 0; y < ci->sy; y++) {

                sci = cil->list[pt[ x + (ci->sx * y) ]-1];
                assert(sci);
                nsubs[ x + (ci->sx * (ci->sy-y-1)) ] = sci;

                if( !sci->parent
                    || sci->parent == sci
                    || ((  (sci->px == 0)
                        || (sci->py == 0)
                        || (sci->px == ( sci->parent->sx - 1 ) )
                        || (sci->py == ( sci->parent->sy - 1 ) ))
                        && (x > 0)
                        && (y > 0)
                        && (x < (ci->sx - 1) )
                        && (y < (ci->sy - 1) )
                    )
                ) {
                    sci->parent = ci;
                    sci->px = x;
                    sci->py = ci->sy - y - 1;
                }
            }
        }

        ci->subs = nsubs;
    }

    for( i = 0; i < rcount; i++) {
        ci = cil->list[i];
        if (!ci->parent) {
            //printf("Eek, CI %s has no parent!\n", ci->name);
            ci->parent = cil->list[rand() % cil->rcount];
            ci->px = ci->sx/2;
            ci->py = ci->sy/2;
        }
        ci_set_siblings(ci);
    }

    return;
}

void
cil_delete(CIList * cil)
{
#ifdef CI_THREADS
    SDL_mutexP(cil->mutex);
#endif

    pq_delete(cil->pq_imgprep);
    if(cil->img_fmt) free(cil->img_fmt);
    cil->img_fmt = 0;
    if(cil->hir_fmt) free(cil->hir_fmt);
    cil->hir_fmt = 0;
    cil_clear(cil);
    free(cil->list);
    glDeleteLists(cil->gl_list, 1);

#ifdef CI_THREADS
    SDL_mutexV(cil->mutex);
    SDL_DestroyMutex(cil->mutex);
#endif

    free(cil);

    return;
}

void
cil_clear(CIList * cil)
{
    int i;
    CImage *ci;

#ifdef CI_THREADS
    if (cil->img_fmt != 0)
        SDL_mutexP(cil->mutex);
#endif

    for( i = 0; i < cil->rcount; i++) {
        ci = cil->list[i];
        ci_delete(ci);
        cil->list[i] = 0;
    }

#ifdef CI_THREADS
    if (cil->img_fmt != 0)
        SDL_mutexV(cil->mutex);
#endif

    return;
}

inline void
cil_q_imgprep(CIList * cil, CImage * ci, int priority)
{
    if (!ci) {
        pq_enqueue(cil->pq_imgprep, 0, 0);
        return;
    }

    if (ci->inq)
        return;

    ci->inq = 1;
    pq_enqueue(cil->pq_imgprep, priority, (void *) ci);

    return;
}

inline void
cil_q_hirprep(CIList * cil, CImage * ci, int priority)
{
    if (!ci) {
        pq_enqueue(cil->pq_hirprep, 0, 0);
        return;
    }

    if (ci->inq)
        return;

    ci->inq = 1;
    pq_enqueue(cil->pq_hirprep, priority, (void *) ci);

    return;
}

void
cil_cleanup(CIList * cil)
{

    /*
     * CImage *ciln, *prev = 0, *nuke = 0;
     * int i = 0;
     *
     * printf("Cleaning up.\n");
     *
     */

    /* expire old cimages from cache and/or memory */

#ifdef CI_THREADS
    SDL_mutexP(cil->mutex);
#endif

    /* FIXME - this needs to do something totally different */
#if 0

    tln = tl->head;

    while (tln) {
        i++;
        if (i > tl->mthresh) {
            // printf("discarding %d %s %d", i, tln->name, (int) tln->next);
            if (tln->tex) {     /* free it */
                printf("Memory drop!\n");
                if (prev)
                    prev->next = tln->next;
                nuke = tln;
                tln = tln->next;

                if (nuke->tex != (Texture *) - 1) /* -1 means primer saw it */
                    tx_delete(nuke->tex);
                free(nuke->name);
                free(nuke);
            } else {            /* cancel load */
                printf("Load cancelled.\n");
                tln->cancel = 1;
                prev = tln;
                tln = tln->next;
            }
        } else if (i > tl->gthresh && tln->tex && tln->tex->loaded) {
            printf("Unloading GL %d %s %ld\n", i, tln->name, (long int) tln->next);
            tx_unload(tln->tex);

            prev = tln;
            tln = tln->next;
        } else {
            // printf("keeping %d %s %d\n", i, tln->name, (int) tln->next);
            prev = tln;
            tln = tln->next;
        }
    }

#endif

#ifdef CI_THREADS
    SDL_mutexV(cil->mutex);
#endif

    return;
}

void *
cil_primer_imgprep(void *cilp)
{
    CIList *cil = (CIList *) cilp;
    CImage *cipq;
    int priority;

    while ((cipq = (CImage *) pq_dequeue(cil->pq_imgprep, &priority))) {
        if (!ci_is_tx_ready(cipq)) {
            // printf("_IMGPREP: Loading Image %s\n", cipq->name);
            ci_do_img(cipq, cil->img_fmt);
            ci_do_prep(cipq, cil->tpool);
            // assert(ci_is_tx_ready(cipq));
        } else {
            printf("_IMGPREP: Skipping Image %s\n", cipq->name);
        }
        cipq->inq = 0;

        SDL_Delay(1);
    }

    printf("_IMGPREP: DONE\n");

    return (0);
}

void
cil_ci_imgprep(CIList *cil, CImage *ci, int levels)
{
    CImage *sci;
    int x, y;

    ci_do_img(ci, cil->img_fmt);
    ci_do_prep(ci, cil->tpool);
    // assert(ci_is_tx_ready(ci));

    if (levels > 0) {
        levels--;
        for (x = 0; x < ci->sx; x++) {
            for (y = 0; y < ci->sy; y++) {

                if ( (sci = ci->subs[x + (ci->sx * y)]) && !(sci->idata)) {
                    cil_ci_imgprep(cil, sci, levels);
                } else {
                    assert(0);
                }
            }
        }
    }
}

void *
cil_primer_hirprep(void *cilp)
{                               /* separate thread please */
    CIList *cil = (CIList *) cilp;
    CImage *cipq;
    int priority;

    while ((cipq = (CImage *) pq_dequeue(cil->pq_hirprep, &priority))) {
        ci_do_img(cipq, cil->hir_fmt);
        cipq->inq = 0;
        // printf("_HIRPREP: Loaded Image %s\n", cipq->name);
        SDL_Delay(1);
    }

    printf("_HIRPREP: DONE\n");

    return (0);
}

/* render images m to n levels starting at alpha a, zoom z, cx, cy, tp_levels, tps, &tx_loads, &img_queues */
void
cil_ci_render(CIList * cil, CImage * ci, int lm, int ln, float subalpha, float alphafact,
              float tlx, float tly, float brx, float bry)
{
    int x, y, cx, cy, minx, maxx, miny, maxy, skipb;
    float newalpha;
    // GLenum error;
    CImage *sci;

    // assert(lm >= 0);
    // assert(ln >= 0);

    if (lm == 0) {
        glBindTexture(GL_TEXTURE_2D, ci->tid);
        glCallList(cil->gl_list);
    }

    if (lm > 1)
        lm--;

    if (ln > 0) {
        ln--;

        glPushMatrix();

            glScalef(1.0 / ((float) ci->sx), 1.0 / ((float) ci->sy), 1.0);
            glColor4f(1.0, 1.0, 1.0, subalpha);

            glCallList(ci->gl_list);

            if (ln > 0) {
                newalpha = subalpha * alphafact;

                /* pre-GL cropping */
                minx = floor(tlx * ci->sx);
                maxx = ceil(brx * ci->sx) - 1;
                miny = floor(tly * ci->sy);
                maxy = ceil(bry * ci->sy) - 1;

                skipb = ( ( (maxx-minx) * (maxy-miny) ) > 12 );

                tlx = (tlx - ((float)minx/(float)ci->sx)) * (float)ci->sx;
                tly = (tly - ((float)miny/(float)ci->sy)) * (float)ci->sy;
                brx = (brx - ((float)maxx/(float)ci->sx)) * (float)ci->sx;
                bry = (bry - ((float)maxy/(float)ci->sy)) * (float)ci->sy;

                cx = minx;
                cy = miny;
                glTranslatef((float) cx, (float) cy, 0.0);

                for (x = minx; x <= maxx; x++) {
                    for (y = miny; y <= maxy; y++) {
                        if ( !(skipb && ((x == minx) || (x == maxx) || (y == miny) || (y == maxy)))
                            && (sci = ci->subs[x + (ci->sx * y)])) {

                            glTranslatef((float) (x - cx), (float) (y - cy), 0.0);
                            cx = x;
                            cy = y;

                            // restore alpha which child will have presumably overwritten
                            if (ln > 1) glColor4f(1.0, 1.0, 1.0, subalpha);

                            cil_ci_render(cil, sci, (lm < 1) ? 1 : lm, ln,
                                newalpha, alphafact,
                                x == minx ? tlx : 0.0,
                                y == miny ? tly : 0.0,
                                x == maxx ? brx : 1.0,
                                y == maxy ? bry : 1.0);
                        }
                    }
                }
            }

        glPopMatrix();
    }
}

/*** EOF *********************************************************************/
