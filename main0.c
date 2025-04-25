#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image/stb_image_write.h"

//folder locations
#define INPUT_FOLDER "image_input/"
#define OUTPUT_FOLDER "image_output/"

//thread count
#define NUM_THREADS 16


//1 thread results in serialization
//n threads where n is the number of images results in each image being worked on but without processing speedups until some threads finish their image while others are still working.
//>n threads results in right away processing speedups for as many extra threads exist.


//returns file extension by finding last "." in a string
char* get_filename_ext(char* filename) {
    char* dot = strrchr(filename, '.');
    return (!dot || dot == filename) ? "" : dot + 1;
}

//Grayscale operation takes average of rgb values into a single channel
void apply_grayscale(unsigned char* img, unsigned char* output_img, int width, int height, int channels, int output_channels) {
#pragma omp taskloop
    for (size_t i = 0; i < width * height; ++i) {
        unsigned char* p = img + i * channels;
        unsigned char* pg = output_img + i * output_channels;
        pg[0] = (p[0] + p[1] + p[2]) / 3;
        if (channels == 4) pg[1] = p[3];
    }
}

//Apply sepia coefficients to rgb values
void apply_sepia(unsigned char* img, unsigned char* output_img, int width, int height, int channels) {
#pragma omp taskloop
    for (size_t i = 0; i < width * height; ++i) {
        unsigned char* p = img + i * channels;
        unsigned char* pg = output_img + i * channels;
        pg[0] = (uint8_t)fmin(0.393 * p[0] + 0.769 * p[1] + 0.189 * p[2], 255.0);
        pg[1] = (uint8_t)fmin(0.349 * p[0] + 0.686 * p[1] + 0.168 * p[2], 255.0);
        pg[2] = (uint8_t)fmin(0.272 * p[0] + 0.534 * p[1] + 0.131 * p[2], 255.0);
        if (channels == 4) pg[3] = p[3];
    }
}

//Iterate through each row and swap left and right values until meeting in the middle
void apply_hflip(unsigned char* img, int width, int height, int channels) {
#pragma omp taskloop
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width / 2; x++) {
            int left = (y * width + x) * channels;
            int right = (y * width + (width - x - 1)) * channels;
            //go through existing channels
            for (int c = 0; c < channels; c++) {
                unsigned char tmp = img[left + c];
                img[left + c] = img[right + c];
                img[right + c] = tmp;
            }
        }
    }
}

//Iterate through each row and swap left and right values until meeting in the middle
void apply_vflip(unsigned char* img, int width, int height, int channels) {
    unsigned char* flipped = malloc(width * height * channels);
#pragma omp taskloop
    for (int y = 0; y < height; y++) {
        int src_y = height - y - 1;
        memcpy(flipped + y * width * channels, img + src_y * width * channels, width * channels);
    }
    //move flipped to img
    memcpy(img, flipped, width * height * channels);
    free(flipped);
}

//Rotates image using transpose and flipping depending on rotation (90: t, hflip; 180: hflip, vflip; 270: t, vflip;)
void apply_rotate(unsigned char* img, int width, int height, int channels, int rotation) {

    switch (rotation) {
    case 180: //180 reuses flip functions
        apply_hflip(img, width, height, channels);
        apply_vflip(img, width, height, channels);
        break;
    
    case 90: //90 & 270 rotation requires transposing the image
    case 270: {
        
        unsigned char* transposed = malloc(width * height * channels);
        if (!transposed) {
            fprintf(stderr, "Memory allocation failed\n");
            return;
        }
        // Tile based transpose
        const int TILE_SIZE = 64;
#pragma omp taskloop
        for (int tile_y = 0; tile_y < height; tile_y += TILE_SIZE) {
            for (int tile_x = 0; tile_x < width; tile_x += TILE_SIZE) {
                //Process each tile
                int y_end = (tile_y + TILE_SIZE < height) ? tile_y + TILE_SIZE : height;
                int x_end = (tile_x + TILE_SIZE < width) ? tile_x + TILE_SIZE : width;

                for (int y = tile_y; y < y_end; y++) {
                    for (int x = tile_x; x < x_end; x++) {
                        //get source and destination positions
                        int src_idx = channels * (y * width + x);
                        int dst_idx = channels * (x * height + y);

                        //copy all channels
                        for (int c = 0; c < channels; c++) {
                            transposed[dst_idx + c] = img[src_idx + c];
                        }
                    }
                }
            }
        }

        // Apply flip based on rotation direction
        if (rotation == 90) {
            apply_hflip(transposed, height, width, channels);
        }
        else {
            apply_vflip(transposed, height, width, channels);
        }

        memcpy(img, transposed, width * height * channels);
        free(transposed);
        break;
    }

    default:
        fprintf(stderr, "ERROR: Invalid rotation value (%d). Valid values are 90, 180, 270\n", rotation);
        return;
    }
}


int main(int argc, char* argv[]) {
    //Input Validation: filenames are provided.
    if (argc < 2) {
        printf("Error: Provide image filenames.\n");
        return 0;
    }
    
    //input sequence
    char input[16];
    printf("Select operations (\"confirm\" to proceed):\nNote: Greyscale and Sepia are mutually exclusive.\nNote: Using rotate asks you to type a multiple of 90 degrees. Anything else cancels.\n"); 
    printf("Greyscale: \"gs\"\nSepia: \"sp\"\nHorizontal Flip: \"hf\"\nVertical Flip: \"vf\"\n");
    printf("Rotate n*90 degrees: \"rt\" then \"90\", \"180\", or \"270\"\n");
    
    //Operation bools
    int greyscale = 0, hflip = 0, vflip = 0, sepia = 0, rotate = 0, rotation = 0;

    //While input not "confirm", modify operation values
    while (scanf("%s", input) && strcmp(input, "confirm") != 0) {
        if (strcmp(input, "gs") == 0) {
            greyscale = !greyscale;
            if (greyscale) sepia = 0;
        }
        else if (strcmp(input, "sp") == 0) {
            sepia = !sepia;
            if (sepia) greyscale = 0;
        }
        else if (strcmp(input, "hf") == 0) hflip = !hflip;
        else if (strcmp(input, "vf") == 0) vflip = !vflip;
        //rotation sequence
        else if (strcmp(input, "rt") == 0) {
            printf("Choose Available Rotation: (90), (180), (270)\n");
            rotate = !rotate;
            scanf("%s", input);
            if (strcmp(input, "90") == 0) {
                rotation = 90;
                printf("Rotation: (%d) \n", rotation);
            } 
            else if (strcmp(input, "180") == 0) {
                rotation = 180;
                printf("Rotation: (%d) \n", rotation);
            }
            else if (strcmp(input, "270") == 0) {
                rotation = 270;
                printf("Rotation: (%d) \n", rotation);
            }
            else {
                //cancels rotation
                rotate = !rotate;
                rotation = 0;
                printf("Rotation Cancelled\n");

            }

        }
        else printf("Invalid operation.\n");
        printf("Chosen: gs(%d), sp(%d), hf(%d), vf(%d), rt(%d):%d\n", greyscale, sepia, hflip, vflip, rotate, rotation);
    }

    //start total timer after input
    double input_start = omp_get_wtime();




    //each image is an iteration, if an image or pointer is unavailable, proceed to next image
    omp_set_num_threads(NUM_THREADS);
#pragma omp parallel for
    for (int i = 1; i < argc; i++) {
        int threadId = omp_get_thread_num();
        
        //get path for given image
        char path[256];
        snprintf(path, sizeof(path), "%s%s", INPUT_FOLDER, argv[i]);
       
        //load image
        int width, height, channels;
        printf("(%d): loading (%s)...\n", omp_get_thread_num(), argv[i]);
        unsigned char* img = stbi_load(path, &width, &height, &channels, 0);
        if (!img) {
            printf("(%d): Failed to load %s\n", threadId, argv[i]);
            continue;
        }
        size_t img_size = width * height * channels;
        //create output image variables
        unsigned char* output_img = img;
        int output_channels = channels;


        //Start Processing
        printf("(%d): \tLOADED (%s), processing...\n", omp_get_thread_num(), argv[i]);
        double start; double end;
        start = omp_get_wtime();

        //operations (greyscale & sepia mutually exclusive)
        if (greyscale) {
            output_channels = (channels == 4) ? 2 : 1;
            unsigned char* gray_img = malloc(width * height * output_channels);
            apply_grayscale(img, gray_img, width, height, channels, output_channels);
            output_img = gray_img;
        }
        else if (sepia) {
            unsigned char* sepia_img = malloc(img_size);
            apply_sepia(img, sepia_img, width, height, channels);
            output_img = sepia_img;
        }

        if (hflip) apply_hflip(output_img, width, height, output_channels);
        if (vflip) apply_vflip(output_img, width, height, output_channels);
        if (rotate) {
            apply_rotate(output_img, width, height, output_channels, rotation);
            //swaps the height and width for rotation
            if (rotation == 90 || rotation == 270) {
                int temp = width;
                width = height;
                height = temp;
            }
        }

        //Processing Timer End
        end = omp_get_wtime();



        //get output file path
        char out_path[256];
        snprintf(out_path, sizeof(out_path), "%s%s", OUTPUT_FOLDER, argv[i]);
        char* ext = get_filename_ext(argv[i]);
        
        //Writing to correct filetype (PNG and JPG supported)
        printf("(%d): \t\tPROCESSED in %f seconds, Writing: (%s)...\n", omp_get_thread_num(), end - start, argv[i]);
        if (strcmp(ext, "png") == 0) {
            stbi_write_png(out_path, width, height, output_channels, output_img, width * output_channels);
        }
        else if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0) {
            stbi_write_jpg(out_path, width, height, output_channels, output_img, 100);
        }
        printf("(%d): \t\t\tWRITTEN: (%s)\n", omp_get_thread_num(), out_path);

        stbi_image_free(img);
        if (output_img != img) free(output_img);
    }

    //Mark completion time
    double input_end = omp_get_wtime();

    printf("Completed all images in %f seconds using %d threads\n", input_end - input_start, NUM_THREADS);
    return 0;
}