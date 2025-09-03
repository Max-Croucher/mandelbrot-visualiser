/* Source file for graphics.c, controlling gameboy graphics
  Author: Max Croucher
  Email: mpccroucher@gmail.com
  May 2025
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <GL/freeglut.h>
#include <math.h>

typedef enum {
    MOUSE_NONE=0,
    MOUSE_SLIDER_REAL,
    MOUSE_SLIDER_CPLX,
    MOUSE_RECT_DRAG
} mose_state_t;

// openGL things
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 800
#define UI_CLOSENESS 0.015
int do_redraw_set = 1;
mose_state_t hover_slider = MOUSE_NONE;
mose_state_t clicked_slider = MOUSE_NONE;

// julia set things
#define NUM_ITERATIONS 64
#define MOTION_DIVIDER 8
#define VALUE_BOUND_SQUARED 16
GLubyte texture_large[SCREEN_HEIGHT][SCREEN_WIDTH][3];
GLubyte texture_small[SCREEN_HEIGHT/MOTION_DIVIDER][SCREEN_WIDTH/MOTION_DIVIDER][3];

#define BASE_MIN_X -2.0
#define BASE_MAX_X 2.0
#define BASE_MIN_Y -2.0
#define BASE_MAX_Y 2.0
#define ZOOM_RECORD_LIMIT 8
long double min_x = BASE_MIN_X;
long double max_x = BASE_MAX_X;
long double min_y = BASE_MIN_Y;
long double max_y = BASE_MAX_Y;
long double zoom_history[ZOOM_RECORD_LIMIT][4] = {0};
unsigned int num_records = 0;

// slider things
#define SLIDER_MIN_X 0.025
#define SLIDER_MAX_X 0.8
#define SLIDER_REAL_Y 0.075
#define SLIDER_CPLX_Y 0.035
#define SLIDER_VALUE_MIN -1
#define SLIDER_VALUE_MAX 1
long double julia_seed_real = 0.35;
long double julia_seed_cplx = -0.5;
int mouse_pointer_x = 0;
int mouse_pointer_y = 0;
int clicked = 0;
int rect_start_x = 0;
int rect_start_y = 0;
int rect_end_x = 0;
int rect_end_y = 0;


double num_julia_iterations(long double x, long double y) {
    /* compute the number of iterations required for the quadratic relation z:= z^2+c
    to become unbounded (or -1 if the relation remains bounded), where z_0 = x+yi
    and c is julia_seed_real+julia_seed_cplx(i). Unbounded is defined as when the square
    of the modulus of z exceeds VALUE_BOUND_SQUARED */
        for (unsigned int i = 0; i < NUM_ITERATIONS; i++) {
        long double next_x = x*x-y*y;
        long double next_y = 2*x*y;
        x = next_x + julia_seed_real;
        y = next_y + julia_seed_cplx;
        long double modulus_square = powl(x,2) + powl(y,2);
        double scaled_result = i + 1 - logl(logl(modulus_square))/logl(2);
        if (modulus_square > VALUE_BOUND_SQUARED) return scaled_result < 0 ? 0 : scaled_result;
    }
    return -1;
}


void hue_to_rgb(double hue, unsigned char* r, unsigned char* g, unsigned char* b) {
    /* take a hue from the HSV colour model in the range [0,1) and compute the corresponding
    RGB colour, assuming saturation and brightness are at maximum */
    hue = fmodl(hue, 1);
    unsigned int phase = hue * 6;
    double delta = hue * 6 - phase; 
    switch (phase)
    {
    case 0: // red-yellow
        *r = 255;
        *g = delta * 256;
        *b = 0;
        break;
    case 1: // yellow-green
        *r = (1-delta) * 256;
        *g = 255;
        *b = 0;
        break;
    case 2: // green-cyan
        *r = 0;
        *g = 255;
        *b = delta * 256;
        break;
    case 3: //cyan-blue
        *r = 0;
        *g = (1-delta) * 256;
        *b = 255;
        break;
    case 4: //blue-magenta
        *r = delta * 256;
        *g = 0;
        *b = 255;
        break;
    case 5: //magenta-red
        *r = 255;
        *g = 0;
        *b = (1-delta) * 256;
        break;
    default:
        *r = 0;
        *g = 0;
        *b = 0;
    }
}


void draw_fractal(unsigned int width, unsigned int height, GLubyte texture[width][height][3]) {
    /* called at the start of each frame to draw the fractal */
    for (unsigned int u = 0; u < width; u++) {
        for (unsigned int v = 0; v < height; v++) {
            long double x = (long double)u / (width-1) * (max_x - min_x) + min_x;
            long double y = (long double)v / (height-1) * (max_y - min_y) + min_y;
            double required_iterations = num_julia_iterations(x, y);
            if (required_iterations == -1) {
                texture[v][u][0]=0;
                texture[v][u][1]=0;
                texture[v][u][2]=0;
            } else {
                hue_to_rgb(((double)required_iterations) / NUM_ITERATIONS, &texture[v][u][0], &texture[v][u][1], &texture[v][u][2]);
            }
        } 
    }
}


static void draw_text(float x, float y, void *font, const char* string) {
    /* draw a string on the screen */  
  glColor3f(0, 0, 0); 
  glRasterPos2f(x, y);
  glutBitmapString(font, (const unsigned char*)string);
  glColor3f(1, 1, 1);
}


void reshape_window_free(int w, int h) {
    /* Ensure window is accurately scaled */
    glViewport(0,0,(GLsizei)w, (GLsizei)h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0,1,0,1,-1.0,1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}


void key_pressed(unsigned char key, int x, int y) {
    /* handle keys being pressed */
    printf("key %c down\n", key);
    switch (key)
    {
    case 'q':
        glutLeaveMainLoop();
        break;
    case 'e':
        if (num_records) {
            num_records--;
            min_x = zoom_history[num_records][0];
            max_x = zoom_history[num_records][1];
            min_y = zoom_history[num_records][2];
            max_y = zoom_history[num_records][3];
            do_redraw_set = 1;
        }
        break;
    default:
        break;
    }
}


void key_released(unsigned char key, int x, int y) {
    /* handle keys being released */
    printf("key %c up\n", key);
}


void mouse_event(int button, int state, int x, int y) {
    /* handle the mouse being clicked */
    if (button == GLUT_LEFT_BUTTON  && state == GLUT_DOWN) {
        if (hover_slider == MOUSE_SLIDER_REAL || hover_slider == MOUSE_SLIDER_CPLX) {
            clicked_slider = hover_slider;
        } else if (hover_slider == MOUSE_NONE) {
            clicked_slider = MOUSE_RECT_DRAG;
            rect_start_x = x;
            rect_start_y = SCREEN_HEIGHT - y - 1;
            rect_end_x = x;
            rect_end_y = SCREEN_HEIGHT - y - 1;
        }
    } else if (button == GLUT_LEFT_BUTTON  && state == GLUT_UP) {  
        if (clicked_slider == MOUSE_RECT_DRAG && rect_start_x != rect_end_x && rect_start_y != rect_end_y) {
            long double x_range = max_x - min_x;
            long double y_range = max_y - min_y;
            if (rect_end_x < rect_start_x) {
                int tmp = rect_end_x;
                rect_end_x = rect_start_x;
                rect_start_x = tmp;
            }
            if (rect_end_y < rect_start_y) {
                int tmp = rect_end_y;
                rect_end_y = rect_start_y;
                rect_start_y = tmp;
            }

            if (num_records == ZOOM_RECORD_LIMIT) {
                // copy backwards
                memmove(zoom_history, &zoom_history[1], sizeof(long double)*4*(ZOOM_RECORD_LIMIT-1));
                num_records--;
            }

            zoom_history[num_records][0] = max_x;
            zoom_history[num_records][1] = min_x;
            zoom_history[num_records][2] = max_y;
            zoom_history[num_records][3] = min_y;
            num_records++;
            max_x = (double)rect_start_x/SCREEN_WIDTH*x_range+min_x;
            min_x = (double)rect_end_x/SCREEN_WIDTH*x_range+min_x;
            max_y = (double)rect_start_y/SCREEN_WIDTH*y_range+min_y;
            min_y = (double)rect_end_y/SCREEN_WIDTH*y_range+min_y;
        } 
        clicked_slider = MOUSE_NONE;
        do_redraw_set = 1;
    }
    glutPostRedisplay();
}


void mouse_move(int x, int y) {
    /* passive callback for mouse pos */
	mouse_pointer_x = x;
	mouse_pointer_y = y;

    double real_slider_pos = ((julia_seed_real - SLIDER_VALUE_MIN) / (SLIDER_VALUE_MAX - SLIDER_VALUE_MIN) + SLIDER_MIN_X) * (SLIDER_MAX_X - SLIDER_MIN_X);
    double cplx_slider_pos = ((julia_seed_cplx - SLIDER_VALUE_MIN) / (SLIDER_VALUE_MAX - SLIDER_VALUE_MIN) + SLIDER_MIN_X) * (SLIDER_MAX_X - SLIDER_MIN_X);
    double screen_slider_pos = (double)mouse_pointer_x / SCREEN_WIDTH;
    if (clicked_slider == MOUSE_RECT_DRAG) {
        rect_end_x = x;
        rect_end_y = SCREEN_HEIGHT - y - 1;
    } else if (clicked_slider == MOUSE_SLIDER_REAL) {
        // currently clicking and dragging the real slider
        julia_seed_real = (screen_slider_pos / (SLIDER_MAX_X - SLIDER_MIN_X) - SLIDER_MIN_X) * (SLIDER_VALUE_MAX - SLIDER_VALUE_MIN) + SLIDER_VALUE_MIN;
        do_redraw_set = 1;
    } else if (clicked_slider == MOUSE_SLIDER_CPLX) {
        // currently clicking and dragging the cplx slider
        julia_seed_cplx = (screen_slider_pos / (SLIDER_MAX_X - SLIDER_MIN_X) - SLIDER_MIN_X) * (SLIDER_VALUE_MAX - SLIDER_VALUE_MIN) + SLIDER_VALUE_MIN;
        do_redraw_set = 1;
    } else if ((fabsl(real_slider_pos - screen_slider_pos) < UI_CLOSENESS) && (fabsl(SLIDER_REAL_Y - 1+(double)mouse_pointer_y / SCREEN_HEIGHT) < UI_CLOSENESS)) {
        // currently hovering over the real slider
        hover_slider = MOUSE_SLIDER_REAL;
        glutSetCursor(GLUT_CURSOR_INFO);
    } else if ((fabsl(cplx_slider_pos - screen_slider_pos) < UI_CLOSENESS) && (fabsl(SLIDER_CPLX_Y - 1+(double)mouse_pointer_y / SCREEN_HEIGHT) < UI_CLOSENESS)) {
        // currently hovering over the cplx slider
        hover_slider = MOUSE_SLIDER_CPLX;
        glutSetCursor(GLUT_CURSOR_INFO);
    } else {
        hover_slider = MOUSE_NONE;
        glutSetCursor(GLUT_CURSOR_CROSSHAIR);
    }
    glutPostRedisplay();
}


void gl_bordered_rectangle(double start_x, double start_y, double end_x, double end_y, double thickness, double fillcolour[3], double bordercolour[3]) {
    /* draw a bordered rectangle with the given coordinates, thickness and colours */
    glColor3f(bordercolour[0], bordercolour[1], bordercolour[2]);
    glBegin(GL_POLYGON);
        glVertex2f(start_x, start_y);
        glVertex2f(start_x, end_y  );
        glVertex2f(end_x,   end_y  );
        glVertex2f(end_x,   start_y);
    glEnd();
    glColor3f(fillcolour[0], fillcolour[1], fillcolour[2]);
    glBegin(GL_POLYGON);
        glVertex2f(start_x+thickness, start_y+thickness);
        glVertex2f(start_x+thickness, end_y-thickness  );
        glVertex2f(end_x-thickness,   end_y-thickness  );
        glVertex2f(end_x-thickness,   start_y+thickness);
    glEnd();
    glColor3f(1,1,1);
}


void gl_hollow_rectangle(double start_x, double start_y, double end_x, double end_y, double thickness, double bordercolour[3]) {
    /* draw a bordered rectangle with the given coordinates, thickness and colours */
    glColor3f(bordercolour[0], bordercolour[1], bordercolour[2]);
    glBegin(GL_LINE_LOOP);
        glVertex2f(start_x, start_y);
        glVertex2f(start_x, end_y  );
        glVertex2f(end_x,   end_y  );
        glVertex2f(end_x,   start_y);
    glEnd();
    glColor3f(1,1,1);
}


void gl_bordered_rhombus(double center_x, double center_y, double width, double height, double thickness, double fillcolour[3], double bordercolour[3]) {
    /* draw a bordered rhombus with the given center, dimensions, thickness and colours */
    glColor3f(bordercolour[0], bordercolour[1], bordercolour[2]);
    glBegin(GL_POLYGON);
        glVertex2f(center_x+width, center_y       );
        glVertex2f(center_x,       center_y+height);
        glVertex2f(center_x-width, center_y       );
        glVertex2f(center_x,       center_y-height);
    glEnd();
    glColor3f(fillcolour[0], fillcolour[1], fillcolour[2]);
    glBegin(GL_POLYGON);
        glVertex2f(center_x+width-thickness, center_y                 );
        glVertex2f(center_x,                 center_y+height-thickness);
        glVertex2f(center_x-width+thickness, center_y                 );
        glVertex2f(center_x,                 center_y-height+thickness);
    glEnd();
    glColor3f(1,1,1);
}


void gl_tick(void) {
    /* update graphics. This function is called at the start of every frame */
    glClear(GL_COLOR_BUFFER_BIT);
    glPushMatrix();
    glEnable(GL_TEXTURE_2D);
    if (clicked_slider == MOUSE_NONE || clicked_slider == MOUSE_RECT_DRAG) {
        if (do_redraw_set) draw_fractal(SCREEN_WIDTH, SCREEN_HEIGHT, texture_large);
        glTexImage2D(GL_TEXTURE_2D,0,3,SCREEN_WIDTH,SCREEN_HEIGHT,0,GL_RGB, GL_UNSIGNED_BYTE, texture_large);
    } else {
        if (do_redraw_set) draw_fractal(SCREEN_WIDTH/MOTION_DIVIDER, SCREEN_HEIGHT/MOTION_DIVIDER, texture_small);
        glTexImage2D(GL_TEXTURE_2D,0,3,SCREEN_WIDTH/MOTION_DIVIDER,SCREEN_HEIGHT/MOTION_DIVIDER,0,GL_RGB, GL_UNSIGNED_BYTE, texture_small);
    }
    do_redraw_set = 0;
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBegin(GL_POLYGON);
        glTexCoord2f(0,0); glVertex2f(0,0);
        glTexCoord2f(0,1); glVertex2f(0,1);
        glTexCoord2f(1,1); glVertex2f(1,1);
        glTexCoord2f(1,0); glVertex2f(1,0);
    glEnd();
	glDisable(GL_TEXTURE_2D);

    double bordercolour[3] = {1,1,1};
    double fillcolour[3] = {0,0,0};
    double fillcolourselected[3] = {0.3,0.3,0.3};
    gl_bordered_rectangle(SLIDER_MIN_X, SLIDER_REAL_Y-0.005, SLIDER_MAX_X, SLIDER_REAL_Y+0.005, 0.002, hover_slider==MOUSE_SLIDER_REAL ? fillcolourselected : fillcolour, bordercolour);
    gl_bordered_rectangle(SLIDER_MIN_X, SLIDER_CPLX_Y-0.005, SLIDER_MAX_X, SLIDER_CPLX_Y+0.005, 0.002, hover_slider==MOUSE_SLIDER_CPLX ? fillcolourselected : fillcolour, bordercolour);

    double real_slider_pos = ((julia_seed_real - SLIDER_VALUE_MIN) / (SLIDER_VALUE_MAX - SLIDER_VALUE_MIN) + SLIDER_MIN_X) * (SLIDER_MAX_X - SLIDER_MIN_X);
    double cplx_slider_pos = ((julia_seed_cplx - SLIDER_VALUE_MIN) / (SLIDER_VALUE_MAX - SLIDER_VALUE_MIN) + SLIDER_MIN_X) * (SLIDER_MAX_X - SLIDER_MIN_X);

    gl_bordered_rhombus(real_slider_pos, SLIDER_REAL_Y, 0.01, 0.01, 0.003, hover_slider==MOUSE_SLIDER_REAL ? fillcolourselected : fillcolour, bordercolour);
    gl_bordered_rhombus(cplx_slider_pos, SLIDER_CPLX_Y, 0.01, 0.01, 0.003, hover_slider==MOUSE_SLIDER_CPLX ? fillcolourselected : fillcolour, bordercolour);

    char text[16];
    sprintf(text, "%.6Lf", julia_seed_real);
    draw_text(SLIDER_MAX_X+0.02, SLIDER_REAL_Y-0.01, GLUT_BITMAP_TIMES_ROMAN_24, text);
    sprintf(text, "%.6Lf", julia_seed_cplx);
    draw_text(SLIDER_MAX_X+0.02, SLIDER_CPLX_Y-0.01, GLUT_BITMAP_TIMES_ROMAN_24, text);

    if (clicked_slider == MOUSE_RECT_DRAG) {
        gl_hollow_rectangle((double)rect_start_x/SCREEN_WIDTH, (double)rect_start_y/SCREEN_HEIGHT, (double)rect_end_x/SCREEN_WIDTH, (double)rect_end_y/SCREEN_HEIGHT, 0.002, bordercolour);
    }

    glPopMatrix();
    glutSwapBuffers();
}


void init_window(int *argc, char *argv[]) {
    /* Main init procedure for graphics */
    glutInit(argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB);
    glutInitWindowSize(SCREEN_WIDTH,SCREEN_HEIGHT);
    glutInitWindowPosition(100,20);
    glutCreateWindow("Julia Set Visualiser");
    glClearColor(0.0,0.0,0.0,0.0);
    glShadeModel(GL_FLAT);
    glutDisplayFunc(gl_tick);
    glutReshapeFunc(reshape_window_free);
    glutKeyboardFunc(key_pressed);
    glutKeyboardUpFunc(key_released);
    glutMouseFunc(mouse_event);
	glutMotionFunc(mouse_move);
	glutPassiveMotionFunc(mouse_move);
    draw_fractal(SCREEN_WIDTH, SCREEN_HEIGHT, texture_large);
    draw_fractal(SCREEN_WIDTH/MOTION_DIVIDER, SCREEN_HEIGHT/MOTION_DIVIDER, texture_small);}


int main(int argc, char *argv[]) {
    init_window(&argc, argv);
    glutMainLoop();
}