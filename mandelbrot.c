/* Source file for ulam_spiral.c, to generate an ulam spiral as a .png image from a sequence of integers
    Author: Max Croucher
    Email: mpccroucher@gmail.com
    July 2025
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <png.h>
#include <glob.h>
#include <pthread.h>
#include <sys/stat.h>

#define IMAGE_SIZE 256
#define IMAGE_BIT_DEPTH 8
#define IMAGE_BYTES (((IMAGE_SIZE*IMAGE_BIT_DEPTH)+7)/8) // guarantees rounding up if IMAGE_SIZE is not divisible by 8

#define MAX_ITERATIONS 256
#define MANDELBROT_BOUND 2
#define MAX_DEPTH 58 // enables a 58-bit index and a 6-bit depth (6 bits are required to encode 58) as a single 64-bit value

#define MIN_X -2.0
#define MIN_Y -1.25
#define BASE_RANGE_X 2.5
#define BASE_RANGE_Y 2.5

char* dirname = "map";
unsigned int max_zoom;

struct shared_data {
    unsigned int current_depth;
    unsigned int current_row;
    pthread_mutex_t mutex;
    pthread_cond_t timereport;
};


static int mandelbrot_point(long double c_r, long double c_i) {
    /* determine the number of iterations required for z = z^2 + c to diverge, where z,c are complex */
    long double z_r = 0;
    long double z_i = 0;
    for (unsigned int i = 0; i < MAX_ITERATIONS; i++) {
        long double x_r = pow(z_r, 2) - pow(z_i, 2);
        long double x_i = 2 * z_r * z_i;
        z_r = x_r + c_r;
        z_i = x_i + c_i;
        if (-2 > z_r || z_r > 2 || -2 > z_i || z_i > 2) return 0xFF - i;
    }
    return 0;
    // return ((int)(x * 10) % 2 ^ (int)(y * 10) % 2);
}


static png_byte* mandelbrot_row(png_structp png_ptr, long double start_x, long double y, long double range) {
    /* render a slice of the mandelbrot set between (start_x, y) and (start_x+range, y) */
    png_byte* row = png_calloc(png_ptr, IMAGE_BYTES);
    for (unsigned int x = 0; x < IMAGE_SIZE; x++) {
        row[x] = mandelbrot_point(start_x+(range/IMAGE_SIZE*x), y);
    }
    return row;
}


static void render_tile(char* filename, long double start_x, long double start_y, long double range_x, long double range_y) {
    /* save a square image IMAGE_SIZE pixels in width. The coordinates for the image are given by start_x and start_y */
     // Open and safety check the png file
    FILE *png_file = fopen(filename, "wb");
    if (!png_file) { // opening file failed
        fprintf(stderr, "Error: Unable to create file %s\n", filename);
        return;
    }
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) { // creating write struct failed
        fclose(png_file);
        fprintf(stderr, "Error: Unable to initialise PNG image %s\n", filename);
        return;
    }
    png_infop png_info = png_create_info_struct(png_ptr);
    if (png_info == NULL || setjmp(png_jmpbuf(png_ptr))) { // creating info struct failed
        png_destroy_write_struct(&png_ptr, &png_info);
        fclose(png_file);
        fprintf(stderr, "Error: Unable to initialise PNG metadata %s\n", filename);
        return;
    }
    // Set header
    png_set_IHDR(
        png_ptr,
        png_info,
        IMAGE_SIZE,
        IMAGE_SIZE,
        IMAGE_BIT_DEPTH,
        PNG_COLOR_TYPE_GRAY,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );

    // create and fill buffers to store bitmap data
    png_byte **row_pointers = png_malloc(png_ptr, IMAGE_SIZE * sizeof(png_byte *));
    for (unsigned int y = 0; y < IMAGE_SIZE; y++) {
        row_pointers[y] = mandelbrot_row(png_ptr, start_x, start_y+(range_y/IMAGE_SIZE*y), range_x);
    }
    png_init_io(png_ptr, png_file);
    png_set_rows(png_ptr, png_info, row_pointers);

    // write png
    png_write_png(png_ptr, png_info, PNG_TRANSFORM_IDENTITY, NULL);

    // free arrays and clean up
    for (unsigned int y = 0; y < IMAGE_SIZE; y++) {
        png_free(png_ptr, row_pointers[y]);
    }
    png_free(png_ptr, row_pointers);
    fclose(png_file);
}


static int rm_recurse(char* current_dir) {
    /* recursively remove png files and directories from the current directory. Exits with failure if a directory contains non-png files */
              
    // remove existing pngs
    glob_t glob_struct;
    char glob_str[256];
    sprintf(glob_str, "%s/*.png", current_dir);
    int code = glob(glob_str, GLOB_ERR, NULL, &glob_struct);
    if (code) {
        if (code == GLOB_NOMATCH) {
            goto remove_directories;
        }
        fprintf(stderr, "Error: Unable to initialise directory\n");
        exit(EXIT_FAILURE);
    }
    while(*glob_struct.gl_pathv) {
        int code = remove(*glob_struct.gl_pathv);
        if (code) return code;
        glob_struct.gl_pathv++;
    }
remove_directories:
    // remove existing directories
    sprintf(glob_str, "%s/*", current_dir);
    code = glob(glob_str, GLOB_ERR, NULL, &glob_struct);
    if (code) {
        if (code == GLOB_NOMATCH) {
            return 0;
        }
        fprintf(stderr, "Error: Unable to initialise directory\n");
        exit(EXIT_FAILURE);
    }
    struct stat stats;
    while(*glob_struct.gl_pathv) {
        int result = stat(*glob_struct.gl_pathv, &stats);
        if (result == 0 && S_ISDIR(stats.st_mode)) { // is a directory
            rm_recurse(*glob_struct.gl_pathv);
            int code = remove(*glob_struct.gl_pathv);
            if (code) return code;
        }
        glob_struct.gl_pathv++;
    }
    return 0;
}


static void init_dir(void) {
    /* initialise the dirname/z/y/x.png directory structure, and remove existing files */
    mkdir(dirname, 0755);
    int code = rm_recurse(dirname);
    if (code) {
        fprintf(stderr, "Unable to clean map directory\n");
        exit(EXIT_FAILURE);
    }
    for (unsigned int z = 0; z <= max_zoom; z++) {
        char z_dir_name[256];
        sprintf(z_dir_name, "%s/%d", dirname, z);
        mkdir(z_dir_name, 0755);
    }
}


static void generate_tile_row(unsigned int z, unsigned int x) {
    /* generate tiles along a given y axis and at the desired zoom level */
    char y_dir_name[256];
    sprintf(y_dir_name, "%s/%d/%d", dirname, z, x);
    mkdir(y_dir_name, 0755);
    long double range_x = BASE_RANGE_X / pow(2, z);
    long double start_x = MIN_X + range_x * x;
    for (unsigned int y = 0; y < pow(2, z); y++) {
        char tile_name[256];
        sprintf(tile_name, "%s/%d/%d/%d.png", dirname, z, x, y);
        long double range_y = BASE_RANGE_Y / pow(2, z);
        long double start_y = MIN_Y + range_y * y;
        render_tile(tile_name, start_x, start_y, range_x, range_y);
    }
}


static void* tile_worker(void* shared_data_ptr) {
    /* Asynchronous worker to to generate tiles, where a job is the generation of every tile along
    a given row at a given depth */
    struct shared_data* data = (struct shared_data*)shared_data_ptr;
  
    while (1) {
        pthread_mutex_lock(&data->mutex);

        if (data->current_depth > max_zoom) { // exit thread
            pthread_mutex_unlock(&data->mutex);
            return NULL;
        }
        unsigned int z = data->current_depth;
        unsigned int x = data->current_row;
        data->current_row++;
        if (data->current_row >= pow(2, data->current_depth)) {
            data->current_row = 0;
            data->current_depth++;
        }
        if (data->current_depth > max_zoom && data->current_row) { // exit thread
            pthread_mutex_unlock(&data->mutex);
            return NULL;
        }

        pthread_mutex_unlock(&data->mutex);
        
        // do the work
        if (x == 0) printf("Generating level %d\n", z);
        generate_tile_row(z, x);
    }
}


static void worker_dispatch(unsigned int num_workers) {
    /* Asynchronously generates the tilemap by dispatching the generation of rows to different worker threads. */
    
    // initialise mutex and shared data
    struct shared_data data = {
        .current_depth = 0,
        .current_row = 0
    };
    pthread_mutex_init(&data.mutex, NULL);
    pthread_t workers[num_workers];

    // initialise workers
    for (unsigned int i = 0; i < num_workers; i++) {
        pthread_create(&workers[i], NULL, tile_worker, &data);
    }

    // Wait for workers to exit
    for (int w = 0; w < num_workers; w++) {
        pthread_join(workers[w], NULL);
    }
    pthread_mutex_destroy(&data.mutex);
}


int main(int argc, char *argv[]) {
    if (argc != 3) {fprintf(stderr, "Usage: %s <zoom-levels> <max-threads>\n", argv[0]); exit(EXIT_FAILURE);}
    max_zoom = atoi(argv[1]);
    if (max_zoom > MAX_DEPTH) {fprintf(stderr, "Error: Zoom level must be at most %d\n", MAX_DEPTH); exit(EXIT_FAILURE);}
    unsigned int num_threads = atoi(argv[2]);
    init_dir();
    printf("Generating tile maps with zoom level %d using %d threads. Target resolution: %.0fx%.0f pixels\n", max_zoom, num_threads, IMAGE_SIZE * pow(2, max_zoom), IMAGE_SIZE * pow(2, max_zoom));
    worker_dispatch(num_threads);
    return EXIT_SUCCESS;
}