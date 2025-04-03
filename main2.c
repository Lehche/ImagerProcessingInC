#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Useful constants for BMP format
#define BMP_TYPE 0x4D42          // 'BM'
#define HEADER_SIZE 14           // BMP file header size
#define INFO_SIZE 40             // BMP info header size
#define BITMAP_OFFSET 54         // Pixel data offset for 24-bit BMP (header+info)
#define DEFAULT_DEPTH 24         // 24-bit image

// -----------------------------------------------------------------------------
// Structures for 24-bit BMP Images
// -----------------------------------------------------------------------------

// BMP file header
typedef struct {
    uint16_t type;       // Signature ("BM")
    uint32_t size;       // File size in bytes
    uint16_t reserved1;  // Reserved (0)
    uint16_t reserved2;  // Reserved (0)
    uint32_t offset;     // Offset to pixel data
} t_bmp_header;

// BMP info header
typedef struct {
    uint32_t size;           // Size of this header (40 bytes)
    int32_t width;           // Image width in pixels
    int32_t height;          // Image height in pixels
    uint16_t planes;         // Number of color planes (must be 1)
    uint16_t bits;           // Bits per pixel (should be 24)
    uint32_t compression;    // Compression type (0 for uncompressed)
    uint32_t imagesize;      // Image size (may be 0 for uncompressed images)
    int32_t xresolution;     // Horizontal resolution (pixels per meter)
    int32_t yresolution;     // Vertical resolution (pixels per meter)
    uint32_t ncolors;        // Number of colors (0 for default)
    uint32_t importantcolors;// Important colors (0 = all)
} t_bmp_info;

// Pixel structure (store as RGB)
typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} t_pixel;

// Structure representing a 24-bit BMP image
typedef struct {
    t_bmp_header header;
    t_bmp_info header_info;
    int width;
    int height;
    int colorDepth;
    t_pixel **data;  // 2D array of pixels
} t_bmp24;

// -----------------------------------------------------------------------------
// Low-level File I/O Helpers
// -----------------------------------------------------------------------------

void file_rawRead(uint32_t position, void *buffer, uint32_t size, size_t n, FILE *file) {
    fseek(file, position, SEEK_SET);
    fread(buffer, size, n, file);
}

void file_rawWrite(uint32_t position, void *buffer, uint32_t size, size_t n, FILE *file) {
    fseek(file, position, SEEK_SET);
    fwrite(buffer, size, n, file);
}

// -----------------------------------------------------------------------------
// Memory Allocation / Deallocation Functions for 24-bit Image Data
// -----------------------------------------------------------------------------

// Dynamically allocate a 2D array (matrix) of t_pixel of size height x width
t_pixel **bmp24_allocateDataPixels(int width, int height) {
    t_pixel **pixels = (t_pixel **)malloc(height * sizeof(t_pixel *));
    if (!pixels) {
        fprintf(stderr, "Error: Unable to allocate memory for pixel rows.\n");
        return NULL;
    }
    for (int i = 0; i < height; i++) {
        pixels[i] = (t_pixel *)malloc(width * sizeof(t_pixel));
        if (!pixels[i]) {
            fprintf(stderr, "Error: Unable to allocate memory for pixel row %d.\n", i);
            for (int j = 0; j < i; j++) {
                free(pixels[j]);
            }
            free(pixels);
            return NULL;
        }
    }
    return pixels;
}

// Free the 2D pixel array
void bmp24_freeDataPixels(t_pixel **pixels, int height) {
    if (!pixels) return;
    for (int i = 0; i < height; i++) {
        free(pixels[i]);
    }
    free(pixels);
}

// Allocate a t_bmp24 image and its pixel data
t_bmp24 *bmp24_allocate(int width, int height, int colorDepth) {
    t_bmp24 *img = (t_bmp24 *)malloc(sizeof(t_bmp24));
    if (!img) {
        fprintf(stderr, "Error: Unable to allocate memory for t_bmp24 structure.\n");
        return NULL;
    }
    img->width = width;
    img->height = height;
    img->colorDepth = colorDepth;
    img->data = bmp24_allocateDataPixels(width, height);
    if (!img->data) {
        free(img);
        return NULL;
    }
    return img;
}

// Free a t_bmp24 image and its pixel data
void bmp24_free(t_bmp24 *img) {
    if (!img) return;
    bmp24_freeDataPixels(img->data, img->height);
    free(img);
}

// -----------------------------------------------------------------------------
// Functions to Read/Write Pixel Data (24-bit)
// -----------------------------------------------------------------------------

// Read pixel data from the file into the t_bmp24 structure.
// Note: BMP stores rows bottom-up and each pixel is stored in BGR order.
void bmp24_readPixelData(t_bmp24 *img, FILE *file) {
    int width = img->width;
    int height = img->height;
    // For each row (BMP rows are stored from bottom to top)
    for (int i = height - 1; i >= 0; i--) {
        for (int j = 0; j < width; j++) {
            uint8_t colors[3];
            fread(colors, sizeof(uint8_t), 3, file);
            // Convert BGR (file order) to RGB (our internal order)
            img->data[i][j].red   = colors[2];
            img->data[i][j].green = colors[1];
            img->data[i][j].blue  = colors[0];
        }
    }
}

// Write pixel data from the t_bmp24 structure to the file.
void bmp24_writePixelData(t_bmp24 *img, FILE *file) {
    int width = img->width;
    int height = img->height;
    // Write rows from bottom to top
    for (int i = height - 1; i >= 0; i--) {
        for (int j = 0; j < width; j++) {
            uint8_t colors[3];
            // Convert RGB to BGR for file writing
            colors[0] = img->data[i][j].blue;
            colors[1] = img->data[i][j].green;
            colors[2] = img->data[i][j].red;
            fwrite(colors, sizeof(uint8_t), 3, file);
        }
    }
}

// -----------------------------------------------------------------------------
// Loading and Saving 24-bit BMP Images
// -----------------------------------------------------------------------------

// Load a 24-bit BMP image from file
t_bmp24 *bmp24_loadImage(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open file %s.\n", filename);
        return NULL;
    }

    // Read BMP header
    t_bmp_header header;
    file_rawRead(0, &header, sizeof(t_bmp_header), 1, file);
    if (header.type != BMP_TYPE) {
        fprintf(stderr, "Error: Not a valid BMP file.\n");
        fclose(file);
        return NULL;
    }

    // Read BMP info header
    t_bmp_info info;
    file_rawRead(HEADER_SIZE, &info, sizeof(t_bmp_info), 1, file);

    // Check that the image is 24-bit
    if (info.bits != DEFAULT_DEPTH) {
        fprintf(stderr, "Error: Only 24-bit BMP images are supported.\n");
        fclose(file);
        return NULL;
    }

    // Allocate image structure
    t_bmp24 *img = bmp24_allocate(info.width, info.height, info.bits);
    if (!img) {
        fclose(file);
        return NULL;
    }

    // Save header information into our image structure
    img->header = header;
    img->header_info = info;
    img->width = info.width;
    img->height = info.height;
    img->colorDepth = info.bits;

    // Move file cursor to the pixel data offset and read pixel data
    fseek(file, header.offset, SEEK_SET);
    bmp24_readPixelData(img, file);

    fclose(file);
    return img;
}

// Save a 24-bit BMP image to file
void bmp24_saveImage(t_bmp24 *img, const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Error: Could not create file %s.\n", filename);
        return;
    }

    // Write header and info header
    file_rawWrite(0, &img->header, sizeof(t_bmp_header), 1, file);
    file_rawWrite(HEADER_SIZE, &img->header_info, sizeof(t_bmp_info), 1, file);

    // Move file cursor to pixel data offset and write pixel data
    fseek(file, img->header.offset, SEEK_SET);
    bmp24_writePixelData(img, file);

    fclose(file);
}

// -----------------------------------------------------------------------------
// Basic 24-bit Image Processing Functions
// -----------------------------------------------------------------------------

// Invert colors (negative)
void bmp24_negative(t_bmp24 *img) {
    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++) {
            img->data[i][j].red   = 255 - img->data[i][j].red;
            img->data[i][j].green = 255 - img->data[i][j].green;
            img->data[i][j].blue  = 255 - img->data[i][j].blue;
        }
    }
}

// Convert image to grayscale
void bmp24_grayscale(t_bmp24 *img) {
    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++) {
            uint8_t avg = (img->data[i][j].red + img->data[i][j].green + img->data[i][j].blue) / 3;
            img->data[i][j].red   = avg;
            img->data[i][j].green = avg;
            img->data[i][j].blue  = avg;
        }
    }
}

// Adjust brightness by adding value (can be negative) to each color channel
void bmp24_brightness(t_bmp24 *img, int value) {
    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++) {
            int newR = img->data[i][j].red + value;
            int newG = img->data[i][j].green + value;
            int newB = img->data[i][j].blue + value;
            if (newR > 255) newR = 255; else if (newR < 0) newR = 0;
            if (newG > 255) newG = 255; else if (newG < 0) newG = 0;
            if (newB > 255) newB = 255; else if (newB < 0) newB = 0;
            img->data[i][j].red   = (uint8_t)newR;
            img->data[i][j].green = (uint8_t)newG;
            img->data[i][j].blue  = (uint8_t)newB;
        }
    }
}

// -----------------------------------------------------------------------------
// Convolution Filtering (24-bit)
// -----------------------------------------------------------------------------

// Apply convolution for a single pixel at (x, y) using the kernel.
// Assumes (x, y) is not on the border.
t_pixel bmp24_convolution(t_bmp24 *img, int x, int y, float **kernel, int kernelSize) {
    int offset = kernelSize / 2;
    float sumR = 0, sumG = 0, sumB = 0;

    for (int i = -offset; i <= offset; i++) {
        for (int j = -offset; j <= offset; j++) {
            int curX = x + i;
            int curY = y + j;
            t_pixel p = img->data[curX][curY];
            float k = kernel[i + offset][j + offset];
            sumR += p.red * k;
            sumG += p.green * k;
            sumB += p.blue * k;
        }
    }
    // Clamp values to [0, 255]
    if (sumR < 0) sumR = 0; if (sumR > 255) sumR = 255;
    if (sumG < 0) sumG = 0; if (sumG > 255) sumG = 255;
    if (sumB < 0) sumB = 0; if (sumB > 255) sumB = 255;
    
    t_pixel result;
    result.red   = (uint8_t)sumR;
    result.green = (uint8_t)sumG;
    result.blue  = (uint8_t)sumB;
    return result;
}

// Helper: Apply a convolution filter to the entire image.
// Note: Border pixels (offset rows/cols) are left unchanged.
void bmp24_applyConvolutionFilter(t_bmp24 *img, float **kernel, int kernelSize) {
    int offset = kernelSize / 2;
    t_pixel **newData = bmp24_allocateDataPixels(img->width, img->height);
    if (!newData) return;

    // Copy original image to newData (so borders remain unchanged)
    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++) {
            newData[i][j] = img->data[i][j];
        }
    }
    // Apply convolution for inner pixels only
    for (int i = offset; i < img->height - offset; i++) {
        for (int j = offset; j < img->width - offset; j++) {
            newData[i][j] = bmp24_convolution(img, i, j, kernel, kernelSize);
        }
    }
    // Copy newData back into img->data
    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++) {
            img->data[i][j] = newData[i][j];
        }
    }
    bmp24_freeDataPixels(newData, img->height);
}

// -----------------------------------------------------------------------------
// Helper Functions for 3x3 Kernels
// -----------------------------------------------------------------------------

// Allocate a 3x3 kernel and initialize it with the given values array (9 elements)
float **allocateKernel3x3(const float values[9]) {
    float **kernel = (float **)malloc(3 * sizeof(float *));
    if (!kernel) return NULL;
    for (int i = 0; i < 3; i++) {
        kernel[i] = (float *)malloc(3 * sizeof(float));
        if (!kernel[i]) {
            for (int j = 0; j < i; j++) free(kernel[j]);
            free(kernel);
            return NULL;
        }
        for (int j = 0; j < 3; j++) {
            kernel[i][j] = values[i * 3 + j];
        }
    }
    return kernel;
}

void freeKernel(float **kernel, int size) {
    for (int i = 0; i < size; i++) {
        free(kernel[i]);
    }
    free(kernel);
}

// -----------------------------------------------------------------------------
// Specific Convolution Filters
// -----------------------------------------------------------------------------

void bmp24_boxBlur(t_bmp24 *img) {
    const float values[9] = {
        1/9.0f, 1/9.0f, 1/9.0f,
        1/9.0f, 1/9.0f, 1/9.0f,
        1/9.0f, 1/9.0f, 1/9.0f
    };
    float **kernel = allocateKernel3x3(values);
    if (kernel) {
        bmp24_applyConvolutionFilter(img, kernel, 3);
        freeKernel(kernel, 3);
    }
}

void bmp24_gaussianBlur(t_bmp24 *img) {
    const float values[9] = {
        1/16.0f, 2/16.0f, 1/16.0f,
        2/16.0f, 4/16.0f, 2/16.0f,
        1/16.0f, 2/16.0f, 1/16.0f
    };
    float **kernel = allocateKernel3x3(values);
    if (kernel) {
        bmp24_applyConvolutionFilter(img, kernel, 3);
        freeKernel(kernel, 3);
    }
}

void bmp24_outline(t_bmp24 *img) {
    const float values[9] = {
         -1, -1, -1,
         -1,  8, -1,
         -1, -1, -1
    };
    float **kernel = allocateKernel3x3(values);
    if (kernel) {
        bmp24_applyConvolutionFilter(img, kernel, 3);
        freeKernel(kernel, 3);
    }
}

void bmp24_emboss(t_bmp24 *img) {
    const float values[9] = {
         -2, -1,  0,
         -1,  1,  1,
          0,  1,  2
    };
    float **kernel = allocateKernel3x3(values);
    if (kernel) {
        bmp24_applyConvolutionFilter(img, kernel, 3);
        freeKernel(kernel, 3);
    }
}

void bmp24_sharpen(t_bmp24 *img) {
    const float values[9] = {
         0, -1,  0,
        -1,  5, -1,
         0, -1,  0
    };
    float **kernel = allocateKernel3x3(values);
    if (kernel) {
        bmp24_applyConvolutionFilter(img, kernel, 3);
        freeKernel(kernel, 3);
    }
}

// -----------------------------------------------------------------------------
// Simple Command-line Menu for Testing
// -----------------------------------------------------------------------------

void printMenu24() {
    printf("\n24-bit Image Processing Menu:\n");
    printf("1. Load an image\n");
    printf("2. Save image\n");
    printf("3. Apply Negative\n");
    printf("4. Convert to Grayscale\n");
    printf("5. Adjust Brightness\n");
    printf("6. Box Blur\n");
    printf("7. Gaussian Blur\n");
    printf("8. Outline\n");
    printf("9. Emboss\n");
    printf("10. Sharpen\n");
    printf("11. Quit\n");
    printf(">>> Your choice: ");
}

int main() {
    t_bmp24 *img = NULL;
    char filepath[256];
    int choice;
    int brightnessValue;
    
    while (1) {
        printMenu24();
        if (scanf("%d", &choice) != 1) break;
        getchar(); // consume newline
        
        switch (choice) {
            case 1:
                printf("Enter file path to load (24-bit BMP): ");
                fgets(filepath, sizeof(filepath), stdin);
                filepath[strcspn(filepath, "\n")] = 0;
                img = bmp24_loadImage(filepath);
                if (img)
                    printf("Image loaded successfully: %d x %d, %d-bit\n", img->width, img->height, img->colorDepth);
                break;
            case 2:
                if (!img) {
                    printf("No image loaded.\n");
                    break;
                }
                printf("Enter file path to save: ");
                fgets(filepath, sizeof(filepath), stdin);
                filepath[strcspn(filepath, "\n")] = 0;
                bmp24_saveImage(img, filepath);
                printf("Image saved successfully.\n");
                break;
            case 3:
                if (!img) {
                    printf("No image loaded.\n");
                    break;
                }
                bmp24_negative(img);
                printf("Negative filter applied.\n");
                break;
            case 4:
                if (!img) {
                    printf("No image loaded.\n");
                    break;
                }
                bmp24_grayscale(img);
                printf("Grayscale filter applied.\n");
                break;
            case 5:
                if (!img) {
                    printf("No image loaded.\n");
                    break;
                }
                printf("Enter brightness adjustment value (can be negative): ");
                scanf("%d", &brightnessValue);
                getchar(); // consume newline
                bmp24_brightness(img, brightnessValue);
                printf("Brightness adjusted by %d.\n", brightnessValue);
                break;
            case 6:
                if (!img) {
                    printf("No image loaded.\n");
                    break;
                }
                bmp24_boxBlur(img);
                printf("Box Blur filter applied.\n");
                break;
            case 7:
                if (!img) {
                    printf("No image loaded.\n");
                    break;
                }
                bmp24_gaussianBlur(img);
                printf("Gaussian Blur filter applied.\n");
                break;
            case 8:
                if (!img) {
                    printf("No image loaded.\n");
                    break;
                }
                bmp24_outline(img);
                printf("Outline filter applied.\n");
                break;
            case 9:
                if (!img) {
                    printf("No image loaded.\n");
                    break;
                }
                bmp24_emboss(img);
                printf("Emboss filter applied.\n");
                break;
            case 10:
                if (!img) {
                    printf("No image loaded.\n");
                    break;
                }
                bmp24_sharpen(img);
                printf("Sharpen filter applied.\n");
                break;
            case 11:
                if (img) {
                    bmp24_free(img);
                }
                printf("Exiting...\n");
                return 0;
            default:
                printf("Invalid choice. Try again.\n");
        }
    }
    
    if (img) {
        bmp24_free(img);
    }
    return 0;
}
