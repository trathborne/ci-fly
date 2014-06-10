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
/*** fly.c *******************************************************************/

/* System libraries */

#include <assert.h>

#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <getopt.h>

extern char *optarg;
extern int optind;
extern int optopt;
extern int opterr;
extern int optreset;

/* Graphics libraries */

#include <GL/glu.h>
#include <SDL.h>
#include <jpeglib.h>

/* CI_THREADS makes all this stuff threadsafe,
    but we're only using one thread because HIR is disabled */
#undef CI_THREADS

/* CI libraries */

#include "texturepool.h"

#include "cimage.h"
#include "cimagelist.h"

extern GLfloat sib_x[8];
extern GLfloat sib_y[8];

/* OpenGL context settings */

#define CI_BPC 8
#define CI_BITS 24
#define CI_ENV_MODE GL_REPLACE

/* Geometry */

#define CI_ASPECT (800.0/600.0) /* XXX - aspect of tiles */
#define CI_SCALE 2.0 /* debugging to expose cropped tiles - 2.0 is final value */
#define CI_CLIP_GROW 1.0 /* debugging to fade in tiles earlier - 1.0 is final value */

/* Timing */

#define CI_FPS 60.0 /* Target FPS */
#define CI_ZOOM_INCREMENT 0.0005 /* Zoom rate increment */
#define CI_MAG_RECURSE (2.0/3.0) /* when to start fading in subtiles */
#define CI_MAG_DENOM (1/(1-CI_MAG_RECURSE))

/* Initialize SDL and OpenGL */

SDL_Surface *
init_sdl_gl(unsigned int bpc, unsigned int bits, int w, int h)
{
    GLuint gl_error;
    SDL_Surface *screen;

    /* Init SDL */

    SDL_Init(SDL_INIT_VIDEO);
    SDL_WM_SetCaption("Continuous Imaging 'fly' (c)2008-2012 David Lowy & Tom Rathborne", "fly");

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, bpc);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, bpc);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, bpc);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, bits);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    if (w > 0 && h > 0) {
        screen = SDL_SetVideoMode(w, h, 0, SDL_OPENGL);
    } else {
        screen = SDL_SetVideoMode(0, 0, 0, SDL_OPENGL|SDL_FULLSCREEN);
    }

    if (!screen) {
        fprintf(stderr, "Couldn't setup SDL GL mode: %s\n", SDL_GetError());
        SDL_Quit();
    }

#ifndef CI_LINUX
    SDL_ShowCursor(SDL_ENABLE);
#endif
    SDL_WM_GrabInput(SDL_GRAB_ON);

    /* Init GL */

    glViewport(0, 0, screen->w, screen->h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, CI_ENV_MODE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        printf("GL Init Error: %s\n", gluErrorString(gl_error));
        SDL_Quit();
        return NULL;
    }

    return screen;
}

/* write current screen contents to a JPEG file */

static JSAMPROW *jpeg_row;
static unsigned char *jpeg_image;

void
jpeg_alloc (int w, int h)
{
    jpeg_row = malloc(w * sizeof(JSAMPROW));
    jpeg_image = malloc (w * h * 3);
    glReadBuffer(GL_BACK); // alters glReadPixels behaviour
}

void jpeg_free ()
{
    free(jpeg_row);
    free(jpeg_image);
}

void
write_jpeg_screenshot (SDL_Surface *screen, int num, char *ss_fmt)
{
    FILE *file;
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    static char filename[256]; // XXX - buffer overflow if another part of this program is evil

    int row, row_stride;

    glReadPixels(0, 0, screen->w, screen->h, GL_RGB, GL_UNSIGNED_BYTE, (GLvoid *)jpeg_image);

    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_compress(&cinfo);

    sprintf(filename, ss_fmt, num);

    if ((file = fopen(filename, "wb")) == NULL) {
        fprintf(stderr, "Can't open %s\n", filename);
        exit(1);
    }

    jpeg_stdio_dest(&cinfo, file);

    cinfo.image_width = screen->w; /* image width and height, in pixels */
    cinfo.image_height = screen->h;
    cinfo.input_components = 3;       /* # of color components per pixel */
    cinfo.in_color_space = JCS_RGB;   /* colorspace of input image */
    jpeg_set_defaults(&cinfo);
    jpeg_start_compress(&cinfo, TRUE);

    row_stride = screen->w * 3; /* JSAMPLEs per row in image_buffer */

    for (row = 0; row < screen->h; row++) {
        jpeg_row[screen->h-row-1] = &jpeg_image[row * row_stride];
    }

    (void) jpeg_write_scanlines(&cinfo, jpeg_row, screen->h);

    jpeg_finish_compress(&cinfo);
    fclose(file);
    jpeg_destroy_compress(&cinfo);

    return;
}

/* Main loop - draw and zoom until done */

unsigned long int
fly(SDL_Surface *screen, CIList *cil, unsigned long int do_jpeg)
{
    SDL_Event event;

    // CI structure pointers
    CImage *ci, *sci;

    // counters
    unsigned long int fcount = 0;

    // flags
    int done = 0,
        fly = 1,
        mouse = 1,
        debug = 0;

    // timing
    int ms_fstart, ms_used, zframes;
    double ms_ftime = 1000.0 / CI_FPS;

    // geometry
    int width = screen->w;
    int height = screen->h;
    double clip_aspect = CI_ASPECT / ((double)width/(double)height);
    double rel_aspect = ((double)width/(double)height) / CI_ASPECT;

    // location and zoom centre
    double mag = CI_MAG_RECURSE / 2.0;
    double cx = 0.0;
    double cy = 0.0;

    // clicked pixels
    int zpx, zpy;
    // pixel motion
    int mpx, mpy; // pixels

    // zoom speed & target
    double zspeed = CI_ZOOM_INCREMENT;
    double ztx = 0.0;
    double zty = 0.0;

    // log zoom
    double log_sx, log_2; // tile size constant
    double scale; // calculated from mag (0..1)

    // other stuff, could be mysterious
    int ox, oy, draw_levels, i, tx, ty;
    double tlx, tly, brx, bry, sub_alpha, ms_tmp;

    /* Run! */
    ci = cil->list[rand() % cil->rcount]; // start on random image
    log_sx = log(ci->sx);
    log_2 = log(2.0);

    glEnable(GL_POINT_SMOOTH);

    ms_fstart = SDL_GetTicks();

    while (!done) {
        /* Render ************************************************************/
        sub_alpha = (mag > CI_MAG_RECURSE)
            ? (exp( (mag - CI_MAG_RECURSE) * CI_MAG_DENOM * log_2 ) - 1.0) // XXX sqrt optional
            : 0.0;
        draw_levels = (mag > CI_MAG_RECURSE) ? 2 : 1;

        /* Reset OpenGL */
        // glClear(GL_COLOR_BUFFER_BIT); /* clear screen ... or not */
        glLoadIdentity();   /* always in modelview mode */
        scale = exp(log_sx * mag) * CI_SCALE;
        glScalef(scale, scale / clip_aspect, 1.0);
        glTranslatef(-0.5 - cx, -0.5 - cy, 0.0);
        glPushMatrix(); // store this position so we can draw a mouse cursor

        /* this loop starts with tx = ty = 0, and sci = ci;
            subsequent passes have tx, ty, sci values of neighbours
        */
        sci = ci;
        ox = 0;
        oy = 0;

        tx = 0;
        ty = 0;

        for (i = 0; i <= 8; i++) {
            /* calculate crop from mag, cx, cy */
            tlx = 0.5 - ox + cx - CI_CLIP_GROW / scale;
            tly = 0.5 - oy + cy - CI_CLIP_GROW * clip_aspect/scale;
            brx = 0.5 - ox + cx + CI_CLIP_GROW / scale;
            bry = 0.5 - oy + cy + CI_CLIP_GROW * clip_aspect/scale;

            if ((brx > 0.0) && (bry > 0.0) && (tlx < 1.0) && (tly < 1.0)) { // VISIBLE
                // determine which subtiles need to be drawn
                tlx = CI_CLAMP(tlx, 0.000001, 0.999999);
                tly = CI_CLAMP(tly, 0.000001, 0.999999);
                brx = CI_CLAMP(brx, 0.000001, 0.999999);
                bry = CI_CLAMP(bry, 0.000001, 0.999999);

                cil_ci_render(cil, sci, 1, draw_levels, 1.0, sub_alpha, tlx, tly, brx, bry);
            }


            // set values for next pass through this loop
            if (i < 8) {
                sci = ci->sibs[i];
                ox = sib_x[i];
                oy = sib_y[i];

                glTranslatef(ox - tx, oy - ty, 0);
                tx = ox;
                ty = oy;
            }
        }

        if (fly && do_jpeg) {
            write_jpeg_screenshot(screen, do_jpeg, "fly-%08ld.jpg");
            do_jpeg++;
        }

        glPopMatrix();
        glDisable(GL_TEXTURE_2D);

        if(debug) { // draw target
            glColor4f(1,0.75,0.25,0.5);
            glPointSize(13.0);
            glBegin(GL_POINTS);
                glVertex2f(ztx + 0.5, zty + 0.5);
            glEnd();
            glColor4f(0,0,0,0.75);
            glPointSize(3.0);
            glBegin(GL_POINTS);
                glVertex2f(ztx + 0.5, zty + 0.5);
            glEnd();
        }

        // draw mouse cursor
        SDL_PumpEvents();
        SDL_GetMouseState(&zpx, &zpy);

        // tlx and tly are being misused.
        tlx = cx + ( 2.0 * (((double)zpx / (double)width) - 0.5) / (scale));
        tly = cy - ( 2.0 * (((double)zpy / (double)height) - 0.5) / (scale * rel_aspect));

        glColor4f(1,1,1,1);
        glPointSize(11.0);
        glBegin(GL_POINTS);
            glVertex2f(tlx + 0.5, tly + 0.5);
        glEnd();
        glColor4f(0,0,0,1);
        glPointSize(5.0);
        glBegin(GL_POINTS);
            glVertex2f(tlx + 0.5, tly + 0.5);
        glEnd();

        glEnable(GL_TEXTURE_2D);

        // timing and double buffer swap

        ms_tmp = SDL_GetTicks();
        ms_used = ms_tmp - ms_fstart - 1;
        if(ms_used < ms_ftime && !do_jpeg) SDL_Delay(3 + ms_ftime - ms_used); // get in next vblank

        SDL_GL_SwapBuffers();

        ms_tmp = SDL_GetTicks();
        ms_used = ms_tmp - ms_fstart - 1;
        ms_fstart = ms_tmp;

        fcount++;

        /* Render done *******************************************************/

        /* Handle events *****************************************************/

        // reset motion delta
        mpx = 0;
        mpy = 0;

        if (!mouse) { // reset click point unless we're following the mouse
            zpx = width/2;
            zpy = height/2;
        }

        if (fly == 2) fly = 0; // fly = 2 is frame step

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_MOUSEMOTION) {

                if (event.motion.state & SDL_BUTTON_MMASK) {
                    mpx += event.motion.xrel;
                    mpy += event.motion.yrel;
                }

                if (event.motion.state & SDL_BUTTON_LMASK) {

                    fly = 1;
                    zpx = event.motion.x;
                    zpy = event.motion.y;
                    if(zspeed <= 0) zspeed = CI_ZOOM_INCREMENT;

                } else if (event.motion.state & SDL_BUTTON_RMASK) {

                    fly = 1;
                    zpx = event.motion.x;
                    zpy = event.motion.y;
                    if(zspeed >= 0) zspeed = -CI_ZOOM_INCREMENT;

                }

            } else if (event.type == SDL_MOUSEBUTTONDOWN) {

                if (event.button.button == 1) { /* left */

                    fly = 1;
                    zpx = event.button.x;
                    zpy = event.button.y;
                    if(zspeed <= 0) zspeed = CI_ZOOM_INCREMENT;

                } else if (event.button.button == 3) { /* right */

                    fly = 1;
                    zpx = event.button.x;
                    zpy = event.button.y;
                    if(zspeed >= 0) zspeed = -CI_ZOOM_INCREMENT;

                } else if (event.button.button == 4) { /* wheel up */

                    if((fly == 0) && (zspeed < 0)) zspeed = 0;
                    zspeed += CI_ZOOM_INCREMENT;
                    fly = 1;

                } else if (event.button.button == 5) { /* wheel down */

                    if((fly == 0) && (zspeed > 0)) zspeed = 0;
                    zspeed -= CI_ZOOM_INCREMENT;
                    fly = 1;

                }

            } else if (event.type == SDL_KEYDOWN) {

                if (event.key.keysym.sym == SDLK_SPACE) {

                    fly = !fly;

                } else if (event.key.keysym.sym == SDLK_PERIOD) {

                    fly = 2; // draw next frame then stop

                } else if (event.key.keysym.sym == SDLK_m) {

                    mouse = !mouse;

                } else if (event.key.keysym.sym == SDLK_d) {

                    debug = !debug;

                } else if (event.key.keysym.sym == SDLK_BACKSPACE) {

                    fly = 0;
                    cx = 0.0;
                    cy = 0.0;
                    mag = 0.0;
                    zpx = width/2;
                    zpy = height/2;

                } else if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q) {
                    done = 1;
                }

            } else if (event.type == SDL_QUIT) {
                done = 1;
            }
        }
        /* Handle events done ************************************************/

        if (fly) { /* Calculate new coordinates */

            zframes = do_jpeg
                ? 1 // we're not keeping track of time
                : ceil(2.0 * (double)(ms_used) / ms_ftime);

            if (mpx || mpx) { // drag-motion
                cx -= ((double)mpx / (double)width) / (scale);
                ztx -= ((double)mpx / (double)width) / (scale);
                cy += ((double)mpy / (double)height) / (scale * rel_aspect);
                zty += ((double)mpy / (double)height) / (scale * rel_aspect);

            } else if ((zpx != width/2) || (zpy != height/2)) { // mouse click
                /* Project clicked pixel zpx,zpy -> ztx,zty */
                ztx = cx + ( 2.0 * (((double)zpx / (double)width) - 0.5) / (scale));
                zty = cy - ( 2.0 * (((double)zpy / (double)height) - 0.5) / (scale * rel_aspect));
            }

            // zoom according to mouseclick or pending zoom
            cx += (ztx - cx) / ((12.0 * (mouse + 1)) * (abs(zspeed) + 1.0) * (2.0-mag));
            cy += (zty - cy) / ((12.0 * (mouse + 1)) * (abs(zspeed) + 1.0) * (2.0-mag));
            mag += (zspeed * zframes) / 2.0;

            // [maybe] change position
            ci = ci_zoom_to(ci, &cx, &cy, &ztx, &zty, &mag);
        }
    }

    return fcount;
}

int
main(int argc, char *argv[])
{
    SDL_Surface *screen;

    TPool *tpool;
    CIList *cil;
    CImage *ci;

    int i, ch;
    unsigned int rw, rh; // requested window size
    unsigned long int fcount, do_jpeg;
    double ticks_start, load_fade, aspect;

    /* Copyright notice */
    printf("\n  Continuous Imaging 'fly'\n  Copyright (C) 2008-2012  David Lowy & Tom Rathborne\n\n");

    /* Get options --jpeg / -j and --geometry / -g */
    static struct option longopts[] = {
        { "jpeg",       no_argument,        NULL, 'j' },
        { "geometry",   required_argument,  NULL, 'g' },
        { NULL,         0,                  NULL, 0 }
    };

    /* Defaults */
    do_jpeg = 0;
    rw = 0;
    rh = 0;

    while ((ch = getopt_long(argc, argv, "jg:", longopts, NULL)) != -1)
        switch(ch) {
            case 'j':
                do_jpeg = 1;
                break;
            case 'g':
                if (sscanf(optarg, "%ux%u", &rw, &rh) != 2) {
                    printf("Unhandled geometry '%s': format is WxH\n", optarg);
                    rw = 0;
                    rh = 0;
                }
                break;
            default:
                printf("Unrecognized option %c ignored\n", ch);
        }

    argc -= optind;
    argv += optind;

    /* Get SDL+GL screen */
    screen = init_sdl_gl(CI_BPC, CI_BITS, rw, rh);
    if(!screen) {
        fprintf(stderr, "Failed to init SDL GL context.\n");
        return (0);
    }

#ifndef CI_LINUX
    SDL_ShowCursor(SDL_DISABLE);
#endif
    SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);

    /* Set up JPEG if necessary */
    if (do_jpeg) jpeg_alloc(screen->w, screen->h);

    aspect = CI_ASPECT / ((double)screen->w / (double)screen->h);

    /* time entire program */
    ticks_start = SDL_GetTicks();

    /* Init texture pool */
    tpool = tp_create(65535, 800); // This was supposed to keep fewer textures in VRAM, but it just fills VRAM.

    /* create CI List */
    cil = cil_create(tpool, "thumb/%s.jpg", "image/%s.jpg");

    /* Load all dbs, make links */
    cil_load_scidb(cil, "cidb.db");

    /* Load image data and textures - and draw 'em! */
    for(i = 0; i < cil->rcount; i++) {
        ci = cil->list[i];
        assert(ci);
        assert(ci->subs);

        cil_ci_imgprep(cil, ci, 0); /* load images to N levels */
        ci_load_tx(ci, 0); /* load textures to N level */

        // Render
        load_fade = sqrt(1.0 - ((double) i / (double) cil->rcount));
        glLoadIdentity();
        glScalef(load_fade, load_fade / aspect, 1.0);
        // glRotatef(360.0 * ((double) i / (double) cil->rcount), 0.0, 0.0, 1.0);
        glTranslatef(-0.5, -0.5, 0.0);

        glClear(GL_COLOR_BUFFER_BIT);
        glColor4f(1.0, 1.0, 1.0, sqrt(load_fade));
        glBindTexture(GL_TEXTURE_2D, ci->tid);
        glCallList(cil->gl_list);
        SDL_GL_SwapBuffers();
    }

    for(i = 0; i < cil->rcount; i++) {
        ci = cil->list[i];
        assert(ci);
        assert(ci->subs);
        ci_mklist(ci, cil->gl_list);
    }

    printf("Startup took %.2fs.\n", (SDL_GetTicks() - ticks_start) / 1000.0);

    /* Main loop */
    ticks_start = SDL_GetTicks();

    SDL_WarpMouse(screen->w/2, screen->h/2);
    SDL_EventState(SDL_MOUSEMOTION, SDL_ENABLE);
    fcount = fly(screen, cil, do_jpeg);

    printf("Average FPS: %.2f\n", fcount * 1000.0 / (SDL_GetTicks() - ticks_start));

    printf("Cleaning up ...\n");

    cil_cleanup(cil);

    /* cleanup */
    cil_delete(cil);

    /* drop our data */
    tp_delete(tpool);

    /* JPEG cleanup */
    if (do_jpeg) jpeg_free();

    SDL_Quit();

    return (0);
}

/*********************************************************************** EOF */
