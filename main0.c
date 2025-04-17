#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image/stb_image_write.h"

#define INPUT_FOLDER "image_input/"
#define OUTPUT_FOLDER "image_output/"


//returns file extension
char* get_filename_ext(char* filename) {
    char* dot = strrchr(filename, '.');
    return (!dot || dot == filename) ? "" : dot + 1;
}

//Grayscale operation takes average of rgb into a single channel
void apply_grayscale(unsigned char* img, unsigned char* output_img, int width, int height, int channels, int output_channels) {
#pragma omp parallel for
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
    memcpy(img, flipped, width * height * channels);
    free(flipped);
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Error: Provide image filenames.\n");
        return 0;
    }

    int greyscale = 0, hflip = 0, vflip = 0, sepia = 0;
    char input[8];
    printf("Select operations (\"confirm\" to proceed):\nNote: Greyscale and Sepia are mutually exclusive.\nGreyscale: \"gs\", Sepia: \"sp\", Horizontal Flip: \"hf\", Vertical Flip: \"vf\" \n");
    
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
        else printf("Invalid operation.\n");
        printf("Chosen: gs(%d), sp(%d), hf(%d), vf(%d)\n", greyscale, sepia, hflip, vflip);
    }

    //set threads
    int num_threads = 32;
    omp_set_num_threads(num_threads);

    //image parallelization timing

    double input_start = omp_get_wtime();

    //each image is an iteration
#pragma omp parallel for
    for (int i = 1; i < argc; i++) {
        int threadId = omp_get_thread_num();
        
        //get path for given image
        char path[256];
        snprintf(path, sizeof(path), "%s%s", INPUT_FOLDER, argv[i]);


       
        //load image
        int width, height, channels;
        unsigned char* img = stbi_load(path, &width, &height, &channels, 0);
        if (!img) {
            printf("(%d): Failed to load %s\n", threadId, argv[i]);
            continue;
        }

        unsigned char* output_img = img;
        int output_channels = channels;
        size_t img_size = width * height * channels;


        //Processing Timing
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



        //Processing Timer End
        end = omp_get_wtime();
        printf("(%d): Processing took %f seconds\n", omp_get_thread_num(), end - start);

        char out_path[256];
        snprintf(out_path, sizeof(out_path), "%s%s", OUTPUT_FOLDER, argv[i]);
        char* ext = get_filename_ext(argv[i]);
        
        //Writing to correct filetype (PNG and JPG supported)
        printf("(%d): Writing to (%s)\n", omp_get_thread_num(), out_path);
        if (strcmp(ext, "png") == 0) {
            stbi_write_png(out_path, width, height, output_channels, output_img, width * output_channels);
        }
        else if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0) {
            stbi_write_jpg(out_path, width, height, output_channels, output_img, 100);
        }

        stbi_image_free(img);
        if (output_img != img) free(output_img);
    }

    //Mark completion time
    double input_end = omp_get_wtime();

    printf("Completed all images in %f seconds using %d threads\n", input_end - input_start, num_threads);
    return 0;
}