# CSCI-4060-Massively Parallel Programming



# USAGE:

Place images into "image_input" folder.

Compile using: gcc -std=c17 -Wall main0.c -o main0.o -lm

Run while listing all image file names to be used as arguments: ./main0.o examplefile1.png examplefile2.jpg examplefile3.jpg

Choose any image operation you would like using the terminal instructions: "gs", "sp", "hf", "vf".

Retyping the operation will deselect it. Note that greyscale and sepia are mutually exclusive.


# Features Implemented:

Supports OpenMP parallelization.
