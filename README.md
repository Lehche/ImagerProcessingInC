# ImagerProcessingInC

This project is a command-line tool written in C for basic image processing operations on BMP (Bitmap) files. It supports uncompressed 8-bit grayscale BMP images (with a standard 256-entry RGBA color table) and uncompressed 24-bit color (RGB) BMP images. The tool provides a menu-driven interface for loading, processing, and saving images.

### Compilation

Open a terminal or command prompt, navigate to the directory containing the source code file (e.g., bmp_processor.c), and compile using GCC with the following command:

gcc -o image_processor bmp_processor.c -lm -std=c99 -Wall -Wextra


### Execution

After successful compilation, run the program from the terminal:
./image_processor

### Implemented Features

The program supports the following features, accessible via a numerical menu:

1- Load 8-bit Grayscale BMP: Loads an 8-bit BMP file. Assumes a standard 256-entry, 4-bytes-per-entry color table.

2- Load 24-bit Color BMP: Loads a 24-bit (RGB) BMP file.

3- bave Current Image: Saves the currently loaded and processed image to a new BMP file. The format (8-bit or 24-bit) matches the loaded image.

4- Display Image Info: Shows metadata of the loaded image (width, height, color depth).

5- Negative:
  For 8-bit images: pixel_value = 255 - pixel_value.
  For 24-bit images: Applies negative to R, G, and B channels independently.
  
6- Adjust Brightness:
  For 8-bit images: Adds a user-specified value to each pixel, clamping results to [0, 255].
  For 24-bit images: Adds a user-specified value to R, G, and B channels, clamping results.
  
7- Threshold (8-bit only): Converts an 8-bit image to binary (black and white) based on a user-specified threshold value. Pixels >= threshold become 255; others become 0.

8- Convert to Grayscale (24-bit only): Converts a 24-bit color image to grayscale using the luminosity method (Gray = 0.299*R + 0.587*G + 0.114*B). The image remains 24-bit, but R, G, and B channels will have identical grayscale values.

9- Box Blur (3x3): Applies a simple averaging blur.

10- Gaussian Blur (3x3): Applies a blur using a Gaussian kernel.

11- Outline (3x3): Applies an edge detection filter to highlight outlines.

12- Emboss (3x3): Applies an emboss filter to give a raised/lowered relief effect.

13- Sharpen (3x3): Applies a sharpening filter to enhance edges.



