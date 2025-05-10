# Multi-threaded Batch Image Processor

Originally a course project in CSCI4060 Massively Parallel Programming

## Usage

Place images into "image_input" folder. If another folder is to be desired, change the INPUT_FOLDER value in main0.c. This also applies to "image_output" and thread count.

Compile using: gcc -std=c17 -Wall main0.c -o main0.o -lm

Run while listing all image file names to be used as arguments: ./main0.o examplefile1.png examplefile2.jpg examplefile3.jpg

Choose any image operation you would like using the terminal instructions: "gs", "sp", "hf", "vf", "rt".

Retyping the operation will deselect it. Note that greyscale and sepia are mutually exclusive, and selecting one deselects the other.

If Rotate is selected, type "90", "180", "270" or "-90" to proceed with rotation. Otherwise type anything else to cancel.

Type "confirm" to proceed.

Once done, the images will be in the output folder "image_output" if the defined variable was not changed.


## Operations

Greyscale: Turns an image black and white, reducing the channel count to 1, or 2 if an alpha channel (transparency) exists.

Sepia: Applies a sepia filter over an image.

Rotate: Rotates an image by specified either 90, 180, or 270 degrees.

Horizontal Flip: Horizontally mirrors an image.

Vertical Flip: Vertically mirrors an image.

## Changing Defined Variables

Some defined variables at the top of main0.c can be changed to support the user's needs.

"INPUT_FOLDER" can be changed to let users use a different folder to place their images.

"OUTPUT_FOLDER" can be changed to let users use a different folder to output their images.

"NUM_THREADS" can be changed to adjust the number of threads the image processor uses.


## Sample Images

Some sample images have been provided to let users try the processor.

large_image.jpg and large_image2.jpg are larger files that spotlight the benefits of parallelization, processing and outputting.

tighten_smug.png and rdj.png test png compatibility, and also rdj.png contains an alpha channel, so transparency can be tested.

man.jpg can be used to compare against the larger images due to its smaller file size.



