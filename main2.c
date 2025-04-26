#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------
#define BMP_TYPE 0x4D42
#define BMP_HEADER_SIZE 54       // pr la taille ds images
#define BMP_COLOR_TABLE_SIZE 1024 //colorsacle (gray)


#define OFFSET_WIDTH 18
#define OFFSET_HEIGHT 22
#define OFFSET_COLOR_DEPTH 28
#define OFFSET_IMAGE_SIZE 34
#define OFFSET_DATA_OFFSET 10

// -----------------------------------------------------------------------------
// Structures
// -----------------------------------------------------------------------------

//8-bit Grayscale BMP
typedef struct {
    unsigned char header[BMP_HEADER_SIZE];
    unsigned char colorTable[BMP_COLOR_TABLE_SIZE];
    unsigned char *data;      // Pixel data (1 byte per pixel)
    unsigned int width;
    unsigned int height;
    unsigned int colorDepth; // ~8
    unsigned int dataSize;
} t_bmp8;

//24-bit Color BMP
// BMP file header portion
typedef struct {
    uint16_t type; // ("BM")
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
} t_bmp_file_header;

// BMP info header portion (last 40 bytes of the 54)
typedef struct {
    uint32_t size; //(40 bytes)
    int32_t width; // in pixels
    int32_t height; // in pixels
    uint16_t planes; // Number of color planes (1)
    uint16_t bits; // (24)
    uint32_t compression; // Compression type (0 for uncompressed)
    uint32_t imagesize; // Image size (may be 0 for uncompressed images)
    int32_t xresolution;
    int32_t yresolution;
    uint32_t ncolors; // Number of colors (0 for default)
    uint32_t importantcolors;// Check
} t_bmp_info_header;

// Pixel structure for 24-bit
typedef struct {
    uint8_t blue; //(BGR)
    uint8_t green;
    uint8_t red;
} t_pixel; // 3 bytes

// for 24-bit BMP image
typedef struct {
    unsigned char header_bytes[BMP_HEADER_SIZE];
    int width;
    int height;
    int colorDepth; // (24)
    uint32_t dataOffset;
    t_pixel **data; //(height x width)
} t_bmp24;

//for YUV color space (used in color equalization)
typedef struct {
    double y;
    double u;
    double v;
} t_yuv;


// -----------------------------------------------------------------------------
// Utility Functions (Memory Allocation, Kernels)
// -----------------------------------------------------------------------------

// Allocate 3x3 float kernel
float **allocateKernel3x3(const float values[9]) {
    float **kernel = (float **)malloc(3 * sizeof(float *));
    if (!kernel) return NULL;
    for (int i = 0; i < 3; i++) {
        kernel[i] = (float *)malloc(3 * sizeof(float));
        if (!kernel[i]) {
            fprintf(stderr, "Error: Failed to allocate kernel row %d\n", i);
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

// Free kernel func
void freeKernel(float **kernel, int size) {
    if (!kernel) return;
    for (int i = 0; i < size; i++) {
        free(kernel[i]);
    }
    free(kernel);
}

// Allocate pixel data func for t_bmp24 (2D array)
t_pixel **bmp24_allocateDataPixels(int width, int height) {
    if (height <= 0 || width <= 0) {
        fprintf(stderr, "Error: Invalid dimensions for pixel allocation (%d x %d)\n", width, height);
        return NULL;
    }
    t_pixel **pixels = (t_pixel **)malloc(height * sizeof(t_pixel *));
    if (!pixels) {
        fprintf(stderr, "Error: Unable to allocate memory for pixel rows.\n");
        return NULL;
    }
    for (int i = 0; i < height; i++) {
        pixels[i] = (t_pixel *)malloc(width * sizeof(t_pixel));
        if (!pixels[i]) { //failsafe
            fprintf(stderr, "Error: Unable to allocate memory for pixel row %d.\n", i);
            for (int j = 0; j < i; j++) {
                free(pixels[j]);
            }
            free(pixels);
            return NULL;
        }
        // memset(pixels[i], 0, width * sizeof(t_pixel));
    }
    return pixels;
}

// Free pixel data func for t_bmp24
void bmp24_freeDataPixels(t_pixel **pixels, int height) {
    if (!pixels) return;
    for (int i = 0; i < height; i++) {
        free(pixels[i]);
    }
    free(pixels);
}

// -----------------------------------------------------------------------------
// BMP File I/O (Handling Padding)
// -----------------------------------------------------------------------------

// 8-bit I/O :


int bmp8_readPixelData(t_bmp8 *img, FILE *file) {
    size_t data_row_size = img->width; //( multiple of 4)
    size_t row_stride = (img->width + 3) & ~3; //round up
    size_t padding = row_stride - data_row_size;

    img->data = (unsigned char *)malloc(img->dataSize);
    if (!img->data) {
        fprintf(stderr, "Error: Could not allocate memory for 8-bit pixel data.\n");
        return 0; // failure
    }

    // saved bottom-up
    for (int i = img->height - 1; i >= 0; i--) {
        unsigned char *row_ptr = img->data + (i * img->width);
        if (fread(row_ptr, 1, data_row_size, file) != data_row_size) {
            fprintf(stderr, "Error reading pixel data row (i=%d).\n", i);
            free(img->data);
            img->data = NULL;
            return 0; // failure
        }
        if (padding > 0) {
            fseek(file, padding, SEEK_CUR);
        }
    }
    return 1; // success :D
}


int bmp8_writePixelData(t_bmp8 *img, FILE *file) {
    size_t data_row_size = img->width;
    size_t row_stride = (img->width + 3) & ~3;
    size_t padding = row_stride - data_row_size;
    unsigned char padding_bytes[3] = {0, 0, 0}; // Max : 3 bytes
    
    for (int i = img->height - 1; i >= 0; i--) {
        unsigned char *row_ptr = img->data + (i * img->width);
        if (fwrite(row_ptr, 1, data_row_size, file) != data_row_size) {
            fprintf(stderr, "Error writing pixel data row (i=%d).\n", i);
            return 0; // Failure
        }
        if (padding > 0) {
            if (fwrite(padding_bytes, 1, padding, file) != padding) {
                 fprintf(stderr, "Error writing padding for row (i=%d).\n", i);
                 return 0; // Failure
            }
        }
    }
    return 1; // Success
}



// 24-bit I/O :


int bmp24_readPixelData(t_bmp24 *img, FILE *file) { //(data: width * 3 bytes/pixel)
    size_t data_row_size = img->width * sizeof(t_pixel);
    // multiple of 4
    size_t row_stride = (data_row_size + 3) & ~3;
    size_t padding = row_stride - data_row_size;

    img->data = bmp24_allocateDataPixels(img->width, img->height);
     if (!img->data) {
        fprintf(stderr, "Error: Could not allocate memory for 24-bit pixel data.\n");
        return 0; // Failure
    }

  
    for (int i = img->height - 1; i >= 0; i--) // (BGR )
        if (fread(img->data[i], sizeof(t_pixel), img->width, file) != img->width) {
            fprintf(stderr, "Error reading pixel data row (i=%d).\n", i);
            bmp24_freeDataPixels(img->data, img->height);
            img->data = NULL;
            return 0; // Failure
        }
        if (padding > 0) {
            fseek(file, padding, SEEK_CUR);
        }
    }
    return 1; // Success
}


int bmp24_writePixelData(t_bmp24 *img, FILE *file) {
    size_t data_row_size = img->width * sizeof(t_pixel);
    size_t row_stride = (data_row_size + 3) & ~3;
    size_t padding = row_stride - data_row_size;
    unsigned char padding_bytes[3] = {0, 0, 0};
    
    for (int i = img->height - 1; i >= 0; i--) { //BGR
        if (fwrite(img->data[i], sizeof(t_pixel), img->width, file) != img->width) {
             fprintf(stderr, "Error writing pixel data row (i=%d).\n", i);
            return 0; // Failure
        }
        if (padding > 0) {
            if (fwrite(padding_bytes, 1, padding, file) != padding) {
                fprintf(stderr, "Error writing padding for row (i=%d).\n", i);
                return 0; // Failure
            }
        }
    }
    return 1; // Success
}

// ---------------------------------------------------------------------------
// 8-bit Image Loading, Saving, Freeing, Info
// -----------------------------------------------------------------------------
t_bmp8 *bmp8_loadImage(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return NULL;
    }

    t_bmp8 *img = (t_bmp8 *)malloc(sizeof(t_bmp8));
    if (!img) {
        fprintf(stderr, "Error: Cannot allocate memory for t_bmp8 structure.\n");
        fclose(file);
        return NULL;
    }
    img->data = NULL; // Initialize data pointer
    
    if (fread(img->header, 1, BMP_HEADER_SIZE, file) != BMP_HEADER_SIZE) {
        fprintf(stderr, "Error: Failed to read BMP header.\n");
        fclose(file);
        free(img);
        return NULL;
    }

    //check Bmp signat
    if (img->header[0] != 'B' || img->header[1] != 'M') {
        fprintf(stderr, "Error: Invalid BMP signature.\n");
        fclose(file);
        free(img);
        return NULL;
    }


    img->width = *(unsigned int *)&img->header[OFFSET_WIDTH];
    img->height = *(unsigned int *)&img->header[OFFSET_HEIGHT];
    img->colorDepth = *(unsigned short *)&img->header[OFFSET_COLOR_DEPTH]; // Note: short (2 bytes)
    img->dataSize = *(unsigned int *)&img->header[OFFSET_IMAGE_SIZE];
    uint32_t dataOffset = *(uint32_t *)&img->header[OFFSET_DATA_OFFSET];


    if (img->colorDepth != 8) {
        fprintf(stderr, "Error: Image is not 8-bit (color depth = %u).\n", img->colorDepth);
        fclose(file);
        free(img);
        return NULL;
    }

    if (img->dataSize == 0) {
        size_t row_stride = (img->width + 3) & ~3;
        img->dataSize = row_stride * img->height;
        // *(unsigned int*)&img->header[OFFSET_IMAGE_SIZE] = img->dataSize;
    }


    // Read color table 
    if (fread(img->colorTable, 1, BMP_COLOR_TABLE_SIZE, file) != BMP_COLOR_TABLE_SIZE) {
        fprintf(stderr, "Error: Failed to read BMP color table.\n");
        fclose(file);
        free(img);
        return NULL;
    }
    
    fseek(file, dataOffset, SEEK_SET);
    if (!bmp8_readPixelData(img, file)) {
        fprintf(stderr, "Error: Failed to read 8-bit pixel data.\n");
        fclose(file);
        // bmp8_readPixelData frees img->data on failure
        free(img);
        return NULL;
    }

    fclose(file);
    printf("Loaded 8-bit image: %u x %u\n", img->width, img->height);
    return img;
}

void bmp8_saveImage(const char *filename, t_bmp8 *img) {
    if (!img || !img->data) {
        fprintf(stderr, "Error: No 8-bit image data to save.\n");
        return;
    }
    FILE *file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Error: Cannot create file %s\n", filename);
        return;
    }

    uint32_t dataOffset = *(uint32_t *)&img->header[OFFSET_DATA_OFFSET];

    // Write header
    if (fwrite(img->header, 1, BMP_HEADER_SIZE, file) != BMP_HEADER_SIZE) {
        fprintf(stderr, "Error writing BMP header.\n");
        fclose(file);
        return;
    }
    // Write color table
    if (fwrite(img->colorTable, 1, BMP_COLOR_TABLE_SIZE, file) != BMP_COLOR_TABLE_SIZE) {
         fprintf(stderr, "Error writing BMP color table.\n");
        fclose(file);
        return;
    }
    
    fseek(file, dataOffset, SEEK_SET);
    
    if (!bmp8_writePixelData(img, file)) {
        fprintf(stderr, "Error writing 8-bit pixel data.\n");
    } else {
         printf("Saved 8-bit image successfully: %s\n", filename);
    }


    fclose(file);
}

void bmp8_free(t_bmp8 *img) {
    if (!img) return;
    free(img->data); // Free pixel data first
    free(img);       // Then free the structure
}

void bmp8_printInfo(t_bmp8 *img) {
    if (!img) {
        printf("No 8-bit image loaded.\n");
        return;
    }
    printf("--- 8-bit Image Info ---\n");
    printf("Width: %u pixels\n", img->width);
    printf("Height: %u pixels\n", img->height);
    printf("Color Depth: %u bits\n", img->colorDepth);
    printf("Data Size: %u bytes\n", img->dataSize);
    printf("Calculated Pixels: %u\n", img->width * img->height);
}

// -----------------------------------------------------------------------------
// 24-bit Image Loading, Saving, Freeing, Info
// -----------------------------------------------------------------------------
t_bmp24 *bmp24_loadImage(const char *filename) {
     FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return NULL;
    }

    t_bmp24 *img = (t_bmp24 *)malloc(sizeof(t_bmp24));
    if (!img) {
        fprintf(stderr, "Error: Cannot allocate memory for t_bmp24 structure.\n");
        fclose(file);
        return NULL;
    }
     img->data = NULL; // Initialize

    // Read the raw 54-byte header
    if (fread(img->header_bytes, 1, BMP_HEADER_SIZE, file) != BMP_HEADER_SIZE) {
        fprintf(stderr, "Error: Failed to read BMP header.\n");
        fclose(file);
        free(img);
        return NULL;
    }

    // Check BMP signat
    if (img->header_bytes[0] != 'B' || img->header_bytes[1] != 'M') {
        fprintf(stderr, "Error: Invalid BMP signature.\n");
        fclose(file);
        free(img);
        return NULL;
    }

    // Extract info
    img->width = *(int32_t *)&img->header_bytes[OFFSET_WIDTH];
    img->height = *(int32_t *)&img->header_bytes[OFFSET_HEIGHT];
    img->colorDepth = *(uint16_t *)&img->header_bytes[OFFSET_COLOR_DEPTH];
    img->dataOffset = *(uint32_t *)&img->header_bytes[OFFSET_DATA_OFFSET];
    uint32_t compression = *(uint32_t *)&img->header_bytes[30]; // Compression offset
    
    if (img->colorDepth != 24 || compression != 0) {
        fprintf(stderr, "Error: Image is not 24-bit uncompressed (depth=%d, compression=%u).\n", img->colorDepth, compression);
        fclose(file);
        free(img);
        return NULL;
    }
    if (img->width <= 0 || img->height <= 0) {
        fprintf(stderr, "Error: Invalid image dimensions (%d x %d).\n", img->width, img->height);
        fclose(file);
        free(img);
        return NULL;
    }

    
    fseek(file, img->dataOffset, SEEK_SET);
    if (!bmp24_readPixelData(img, file)) {
         fprintf(stderr, "Error: Failed to read 24-bit pixel data.\n");
        fclose(file);
        free(img);
        return NULL;
    }

    fclose(file);
    printf("Loaded 24-bit image: %d x %d\n", img->width, img->height);
    return img;
}

void bmp24_saveImage(const char *filename, t_bmp24 *img) {
    if (!img || !img->data) {
        fprintf(stderr, "Error: No 24-bit image data to save.\n");
        return;
    }
    FILE *file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Error: Cannot create file %s\n", filename);
        return;
    }

    if (fwrite(img->header_bytes, 1, BMP_HEADER_SIZE, file) != BMP_HEADER_SIZE) {
         fprintf(stderr, "Error writing BMP header.\n");
        fclose(file);
        return;
    }
    
    fseek(file, img->dataOffset, SEEK_SET);
    
    if (!bmp24_writePixelData(img, file)) {
         fprintf(stderr, "Error writing 24-bit pixel data.\n");
    } else {
        printf("Saved 24-bit image successfully: %s\n", filename);
    }

    fclose(file);
}

void bmp24_free(t_bmp24 *img) {
    if (!img) return;
    bmp24_freeDataPixels(img->data, img->height); // Free pixel data
    free(img);   // Free the structure
}

void bmp24_printInfo(t_bmp24 *img) {
    if (!img) {
        printf("No 24-bit image loaded.\n");
        return;
    }
     printf("--- 24-bit Image Info ---\n");
    printf("Width: %d pixels\n", img->width);
    printf("Height: %d pixels\n", img->height);
    printf("Color Depth: %d bits\n", img->colorDepth);
    printf("Data Offset: %u\n", img->dataOffset);


// -----------------------------------------------------------------------------
// Image Processing Functions (8-bit)
// -----------------------------------------------------------------------------

void bmp8_negative(t_bmp8 *img) {
    if (!img || !img->data) return;
    unsigned int num_pixels = img->width * img->height;
    for (unsigned int i = 0; i < num_pixels; i++) {
        img->data[i] = 255 - img->data[i];
    }
}

void bmp8_brightness(t_bmp8 *img, int value) {
    if (!img || !img->data) return;
    unsigned int num_pixels = img->width * img->height;
    for (unsigned int i = 0; i < num_pixels; i++) {
        int new_val = img->data[i] + value;
        if (new_val > 255) new_val = 255;
        if (new_val < 0) new_val = 0;
        img->data[i] = (unsigned char)new_val;
    }
}

void bmp8_threshold(t_bmp8 *img, int threshold) {
     if (!img || !img->data) return;
    unsigned int num_pixels = img->width * img->height;
     if (threshold < 0) threshold = 0;
     if (threshold > 255) threshold = 255;
    for (unsigned int i = 0; i < num_pixels; i++) {
        img->data[i] = (img->data[i] >= threshold) ? 255 : 0;
    }
}

// Apply convolution filter
void bmp8_applyFilter(t_bmp8 *img, float **kernel, int kernelSize) {
    if (!img || !img->data || !kernel) return;
    int offset = kernelSize / 2;
    if (offset <= 0) return; // Kernel too small

    unsigned int num_pixels = img->width * img->height;
    unsigned char *tempData = (unsigned char *)malloc(num_pixels * sizeof(unsigned char));
    if (!tempData) {
        fprintf(stderr, "Error: Failed to allocate temp buffer for 8-bit convolution.\n");
        return;
    }
    memcpy(tempData, img->data, num_pixels * sizeof(unsigned char)); // Copy original

    for (int y = offset; y < img->height - offset; y++) {
        for (int x = offset; x < img->width - offset; x++) {
            float sum = 0;
            for (int ky = -offset; ky <= offset; ky++) {
                for (int kx = -offset; kx <= offset; kx++) {
                    int currentY = y + ky;
                    int currentX = x + kx;
                    sum += tempData[currentY * img->width + currentX] * kernel[ky + offset][kx + offset];
                }
            }
            // Clamp result
            if (sum < 0) sum = 0;
            if (sum > 255) sum = 255;
            img->data[y * img->width + x] = (unsigned char)round(sum);
        }
    }

    free(tempData);
}

// -----------------------------------------------------------------------------
// Image Processing Functions (24-bit)
// -----------------------------------------------------------------------------

void bmp24_negative(t_bmp24 *img) {
    if (!img || !img->data) return;
    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++) {
            img->data[i][j].red   = 255 - img->data[i][j].red;
            img->data[i][j].green = 255 - img->data[i][j].green;
            img->data[i][j].blue  = 255 - img->data[i][j].blue;
        }
    }
}

void bmp24_grayscale(t_bmp24 *img) {
     if (!img || !img->data) return;
    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++) {
             uint8_t gray = (uint8_t)(0.299 * img->data[i][j].red +
                                     0.587 * img->data[i][j].green +
                                     0.114 * img->data[i][j].blue);
            // uint8_t gray = (img->data[i][j].red + img->data[i][j].green + img->data[i][j].blue) / 3;
            img->data[i][j].red = gray;
            img->data[i][j].green = gray;
            img->data[i][j].blue = gray;
        }
    }
}

void bmp24_brightness(t_bmp24 *img, int value) {
    if (!img || !img->data) return;
    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++) {
            int newR = img->data[i][j].red + value;
            int newG = img->data[i][j].green + value;
            int newB = img->data[i][j].blue + value;
            // Clamp values
            img->data[i][j].red   = (uint8_t)(fmax(0, fmin(255, newR)));
            img->data[i][j].green = (uint8_t)(fmax(0, fmin(255, newG)));
            img->data[i][j].blue  = (uint8_t)(fmax(0, fmin(255, newB)));
        }
    }
}

// Apply convolution : single 24-bit pixel
t_pixel bmp24_convolution(t_bmp24 *img, int y, int x, float **kernel, int kernelSize) {
    int offset = kernelSize / 2;
    double sumR = 0, sumG = 0, sumB = 0;

    for (int ky = -offset; ky <= offset; ky++) {
        for (int kx = -offset; kx <= offset; kx++) {
            int currentY = y + ky;
            int currentX = x + kx;
            if (currentY >= 0 && currentY < img->height && currentX >= 0 && currentX < img->width) {
                t_pixel p = img->data[currentY][currentX]; // Read BGR
                float k = kernel[ky + offset][kx + offset];
                sumB += p.blue * k;
                sumG += p.green * k;
                sumR += p.red * k;
            }
        }
    }
    t_pixel result;
    result.blue  = (uint8_t)(fmax(0, fmin(255, round(sumB))));
    result.green = (uint8_t)(fmax(0, fmin(255, round(sumG))));
    result.red   = (uint8_t)(fmax(0, fmin(255, round(sumR))));
    return result;
}


//convolution filter to 24-bit 
void bmp24_applyConvolutionFilter(t_bmp24 *img, float **kernel, int kernelSize) {
    if (!img || !img->data || !kernel) return;
    int offset = kernelSize / 2;
     if (offset <= 0 || img->height <= 2*offset || img->width <= 2*offset) {
         fprintf(stderr, "Error: Image too small for kernel or invalid kernel size.\n");
         return;
     }

    // Create a temporary copy
    t_pixel **tempData = bmp24_allocateDataPixels(img->width, img->height);
    if (!tempData) {
        fprintf(stderr, "Error: Failed to allocate temp buffer for 24-bit convolution.\n");
        return;
    }
    for (int i = 0; i < img->height; i++) {
        memcpy(tempData[i], img->data[i], img->width * sizeof(t_pixel));
    }

    // img->data
    for (int y = offset; y < img->height - offset; y++) {
        for (int x = offset; x < img->width - offset; x++) {
             img->data[y][x] = bmp24_convolution(img, y, x, kernel, kernelSize);
             // Correction: Need to read from the *copy* (tempData)
             // Recreate bmp24_convolution to accept the source buffer explicitly
             // Or simpler: just modify the original convolution to read from tempData

             double sumR = 0, sumG = 0, sumB = 0;
             for (int ky = -offset; ky <= offset; ky++) {
                 for (int kx = -offset; kx <= offset; kx++) {
                     t_pixel p = tempData[y + ky][x + kx]; // Read from copy
                     float k = kernel[ky + offset][kx + offset];
                     sumB += p.blue * k;
                     sumG += p.green * k;
                     sumR += p.red * k;
                 }
             }
             img->data[y][x].blue  = (uint8_t)(fmax(0, fmin(255, round(sumB))));
             img->data[y][x].green = (uint8_t)(fmax(0, fmin(255, round(sumG))));
             img->data[y][x].red   = (uint8_t)(fmax(0, fmin(255, round(sumR))));
        }
    }

    bmp24_freeDataPixels(tempData, img->height); // Free the temporary copy
}


void bmp24_boxBlur(t_bmp24 *img) {
    const float values[9] = { 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f };
    float **k = allocateKernel3x3(values); if(k) { bmp24_applyConvolutionFilter(img, k, 3); freeKernel(k, 3); }
}
void bmp24_gaussianBlur(t_bmp24 *img) {
    const float values[9] = { 1/16.0f, 2/16.0f, 1/16.0f, 2/16.0f, 4/16.0f, 2/16.0f, 1/16.0f, 2/16.0f, 1/16.0f };
    float **k = allocateKernel3x3(values); if(k) { bmp24_applyConvolutionFilter(img, k, 3); freeKernel(k, 3); }
}
void bmp24_outline(t_bmp24 *img) {
    const float values[9] = { -1, -1, -1, -1, 8, -1, -1, -1, -1 };
    float **k = allocateKernel3x3(values); if(k) { bmp24_applyConvolutionFilter(img, k, 3); freeKernel(k, 3); }
}
void bmp24_emboss(t_bmp24 *img) {
    const float values[9] = { -2, -1, 0, -1, 1, 1, 0, 1, 2 };
    float **k = allocateKernel3x3(values); if(k) { bmp24_applyConvolutionFilter(img, k, 3); freeKernel(k, 3); }
}
void bmp24_sharpen(t_bmp24 *img) {
    const float values[9] = { 0, -1, 0, -1, 5, -1, 0, -1, 0 };
    float **k = allocateKernel3x3(values); if(k) { bmp24_applyConvolutionFilter(img, k, 3); freeKernel(k, 3); }
}

// -----------------------------------------------------------------------------
// Histogram Equalization (Part 3)
// -----------------------------------------------------------------------------

//              8-bit Equalization 

// Compute histogram for 8-bit image
unsigned int *bmp8_computeHistogram(t_bmp8 *img) {
    if (!img || !img->data) return NULL;
    unsigned int *hist = (unsigned int *)calloc(256, sizeof(unsigned int)); // Initialize to 0
    if (!hist) {
        fprintf(stderr, "Error: Cannot allocate memory for histogram.\n");
        return NULL;
    }
    unsigned int num_pixels = img->width * img->height;
    for (unsigned int i = 0; i < num_pixels; i++) {
        hist[img->data[i]]++;
    }
    return hist;
}

// CDF from histogram
unsigned int *bmp8_computeCDF(unsigned int *hist) {
    if (!hist) return NULL;
    unsigned int *cdf = (unsigned int *)malloc(256 * sizeof(unsigned int));
    if (!cdf) {
        fprintf(stderr, "Error: Cannot allocate memory for CDF.\n");
        return NULL;
    }
    cdf[0] = hist[0];
    for (int i = 1; i < 256; i++) {
        cdf[i] = cdf[i - 1] + hist[i];
    }
    return cdf;
}

// Apply histogram equalization
void bmp8_equalize(t_bmp8 *img) {
    if (!img || !img->data) return;

    unsigned int *hist = bmp8_computeHistogram(img);
    if (!hist) return;
    unsigned int *cdf = bmp8_computeCDF(hist);
    if (!cdf) {
        free(hist);
        return;
    }

    unsigned int cdf_min = 0;
    for (int i = 0; i < 256; i++) {
        if (cdf[i] != 0) {
            cdf_min = cdf[i];
            break;
        }
    }

    unsigned int num_pixels = img->width * img->height;
    if (num_pixels - cdf_min == 0) {
        fprintf(stderr, "Warning: Cannot equalize image with uniform color or only one color level present.\n");
        free(hist);
        free(cdf);
        return; // No change
    }


    unsigned char hist_eq[256];
    double scale_factor = 255.0 / (num_pixels - cdf_min);
    for (int i = 0; i < 256; i++) {
         if (cdf[i] >= cdf_min) { // Apply formula
            hist_eq[i] = (unsigned char)round((double)(cdf[i] - cdf_min) * scale_factor);
         } else {
             hist_eq[i] = 0; // Map non-occurring low values to 0
         }
    }

    // Apply mapping
    for (unsigned int i = 0; i < num_pixels; i++) {
        img->data[i] = hist_eq[img->data[i]];
    }

    free(hist);
    free(cdf);
    printf("8-bit histogram equalization applied.\n");
}

// 24-bit Equalization  :


t_yuv rgb_to_yuv(t_pixel p) {
    t_yuv yuv;
    double r = p.red;
    double g = p.green;
    double b = p.blue;
    yuv.y = 0.299 * r + 0.587 * g + 0.114 * b;
    yuv.u = -0.14713 * r - 0.28886 * g + 0.436 * b;
    yuv.v = 0.615 * r - 0.51499 * g - 0.10001 * b;
    return yuv;
}


t_pixel yuv_to_rgb(t_yuv yuv) {
    t_pixel p;
    double y = yuv.y;
    double u = yuv.u;
    double v = yuv.v;
    double r_f = y + 1.13983 * v;
    double g_f = y - 0.39465 * u - 0.58060 * v;
    double b_f = y + 2.03211 * u;


    p.red   = (uint8_t)fmax(0, fmin(255, round(r_f)));
    p.green = (uint8_t)fmax(0, fmin(255, round(g_f)));
    p.blue  = (uint8_t)fmax(0, fmin(255, round(b_f)));
    return p;
}

void bmp24_equalize(t_bmp24 *img) {
    if (!img || !img->data) return;

    int width = img->width;
    int height = img->height;
    unsigned int num_pixels = width * height;

    t_yuv **yuv_data = (t_yuv **)malloc(height * sizeof(t_yuv *));
    unsigned int *y_hist = (unsigned int *)calloc(256, sizeof(unsigned int));
    if (!yuv_data || !y_hist) {
        fprintf(stderr, "Error: Failed to allocate memory for YUV data or Y histogram.\n");
        free(yuv_data); 
        free(y_hist);
        return;
    }

    for (int i = 0; i < height; i++) {
        yuv_data[i] = (t_yuv *)malloc(width * sizeof(t_yuv));
        if (!yuv_data[i]) {
             fprintf(stderr, "Error: Failed to allocate memory for YUV row %d.\n", i);
             for(int k=0; k<i; k++) free(yuv_data[k]);
             free(yuv_data);
             free(y_hist);
             return;
        }
        for (int j = 0; j < width; j++) {
            yuv_data[i][j] = rgb_to_yuv(img->data[i][j]);
            uint8_t y_clamped = (uint8_t)fmax(0, fmin(255, round(yuv_data[i][j].y)));
            y_hist[y_clamped]++;
        }
    }

    unsigned int *y_cdf = bmp8_computeCDF(y_hist); 
    if (!y_cdf) {
        fprintf(stderr, "Error: Failed to compute Y channel CDF.\n");
        for(int i=0; i<height; i++) free(yuv_data[i]);
        free(yuv_data);
        free(y_hist);
        return;
    }


    unsigned int cdf_min_y = 0;
    for (int i = 0; i < 256; i++) { if (y_cdf[i] != 0) { cdf_min_y = y_cdf[i]; break; } }

    if (num_pixels - cdf_min_y == 0) {
         fprintf(stderr, "Warning: Cannot equalize Y channel (uniform luminance).\n");
         for(int i=0; i<height; i++) free(yuv_data[i]);
         free(yuv_data);
         free(y_hist);
         free(y_cdf);
         return;
    }

    double scale_factor_y = 255.0 / (num_pixels - cdf_min_y);
    unsigned char y_map[256];
    for (int i = 0; i < 256; i++) {
        if (y_cdf[i] >= cdf_min_y) {
             y_map[i] = (unsigned char)round((double)(y_cdf[i] - cdf_min_y) * scale_factor_y);
        } else {
            y_map[i] = 0;
        }

    }

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
             uint8_t y_original_clamped = (uint8_t)fmax(0, fmin(255, round(yuv_data[i][j].y)));
             double y_new = y_map[y_original_clamped];

            t_yuv yuv_new = { y_new, yuv_data[i][j].u, yuv_data[i][j].v };

            // Convert back to RGB
            img->data[i][j] = yuv_to_rgb(yuv_new);
        }
    }

    for(int i=0; i<height; i++) free(yuv_data[i]);
    free(yuv_data);
    free(y_hist);
    free(y_cdf);

    printf("24-bit histogram equalization (Y channel) applied.\n");
}


// -----------------------------------------------------------------------------
// Main Menu and Program Flow
// -----------------------------------------------------------------------------

void printMainMenu() {
    printf("\n--- Image Processing Menu ---\n");
    printf("1. Load 8-bit Grayscale BMP\n");
    printf("2. Load 24-bit Color BMP\n");
    printf("3. Save Current Image\n");
    printf("4. Display Image Info\n");
    printf("--- Basic Operations ---\n");
    printf("5. Negative\n");
    printf("6. Adjust Brightness\n");
    printf("7. Threshold (8-bit only)\n");
    printf("8. Convert to Grayscale (24-bit only)\n");
    printf("--- Convolution Filters ---\n");
    printf("9. Box Blur (3x3)\n");
    printf("10. Gaussian Blur (3x3)\n");
    printf("11. Outline (3x3)\n");
    printf("12. Emboss (3x3)\n");
    printf("13. Sharpen (3x3)\n");
    printf("--- Histogram Equalization ---\n");
    printf("14. Equalize Histogram\n");
    printf("0. Quit\n");
    printf(">>> Enter your choice: ");
}

int main() {
    t_bmp8 *img8 = NULL;
    t_bmp24 *img24 = NULL;
    char filepath[256];
    int choice;
    int value; // brightness/threshold

    while (1) {
        printMainMenu();
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            choice = -1; // Invalid choice
        }
        getchar(); 

        // Image Management 
        if (choice == 1) { // Load 8-bit
            if (img8) { bmp8_free(img8); img8 = NULL; }
            if (img24) { bmp24_free(img24); img24 = NULL; }
            printf("Enter path for 8-bit BMP: ");
            fgets(filepath, sizeof(filepath), stdin);
            filepath[strcspn(filepath, "\n")] = 0; // Remove trailing newline
            img8 = bmp8_loadImage(filepath);
            if (!img8) printf("Failed to load 8-bit image.\n");
        } else if (choice == 2) { // Load 24-bit
             if (img8) { bmp8_free(img8); img8 = NULL; }
             if (img24) { bmp24_free(img24); img24 = NULL; }
            printf("Enter path for 24-bit BMP: ");
            fgets(filepath, sizeof(filepath), stdin);
            filepath[strcspn(filepath, "\n")] = 0;
            img24 = bmp24_loadImage(filepath);
             if (!img24) printf("Failed to load 24-bit image.\n");
        } else if (choice == 3) { // Save
            if (img8) {
                 printf("Enter path to save 8-bit BMP: ");
                 fgets(filepath, sizeof(filepath), stdin);
                 filepath[strcspn(filepath, "\n")] = 0;
                 bmp8_saveImage(filepath, img8);
            } else if (img24) {
                 printf("Enter path to save 24-bit BMP: ");
                 fgets(filepath, sizeof(filepath), stdin);
                 filepath[strcspn(filepath, "\n")] = 0;
                 bmp24_saveImage(filepath, img24);
            } else {
                printf("No image loaded to save.\n");
            }
        } else if (choice == 4) { // Info
            if (img8) bmp8_printInfo(img8);
            else if (img24) bmp24_printInfo(img24);
            else printf("No image loaded.\n");
        }
        // Basic Operations
        else if (choice == 5) { // Negative
            if (img8) { bmp8_negative(img8); printf("8-bit negative applied.\n"); }
            else if (img24) { bmp24_negative(img24); printf("24-bit negative applied.\n"); }
            else printf("No image loaded.\n");
        } else if (choice == 6) { // Brightness
             printf("Enter brightness adjustment value: ");
             if (scanf("%d", &value) == 1) {
                 getchar(); // consume newline
                 if (img8) { bmp8_brightness(img8, value); printf("8-bit brightness adjusted.\n"); }
                 else if (img24) { bmp24_brightness(img24, value); printf("24-bit brightness adjusted.\n"); }
                 else printf("No image loaded.\n");
             } else {
                 printf("Invalid input for brightness.\n");
                 while (getchar() != '\n'); // Clear buffer
             }
        } else if (choice == 7) { // Threshold (8-bit only)
            if (img8) {
                printf("Enter threshold value (0-255): ");
                if (scanf("%d", &value) == 1) {
                    getchar(); // consume newline
                    bmp8_threshold(img8, value);
                    printf("8-bit threshold applied.\n");
                } else {
                    printf("Invalid input for threshold.\n");
                    while (getchar() != '\n'); // Clear buffer
                }
            } else if (img24) {
                printf("Threshold is only applicable to 8-bit grayscale images.\n");
            } else {
                printf("No image loaded.\n");
            }
        } else if (choice == 8) { // Grayscale (24-bit only)
            if (img24) {
                bmp24_grayscale(img24);
                printf("Converted 24-bit image to grayscale.\n");
            } else if (img8) {
                printf("Image is already grayscale.\n");
            } else {
                printf("No image loaded.\n");
            }
        }
        // --- Convolution Filters ---
        else if (choice >= 9 && choice <= 13) { // Filters
            if (!img8 && !img24) {
                printf("No image loaded.\n");
            } else {
                float **kernel = NULL;
                const float k_box[9] = { 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f };
                const float k_gauss[9] = { 1/16.0f, 2/16.0f, 1/16.0f, 2/16.0f, 4/16.0f, 2/16.0f, 1/16.0f, 2/16.0f, 1/16.0f };
                const float k_outline[9] = { -1, -1, -1, -1, 8, -1, -1, -1, -1 };
                const float k_emboss[9] = { -2, -1, 0, -1, 1, 1, 0, 1, 2 };
                const float k_sharpen[9] = { 0, -1, 0, -1, 5, -1, 0, -1, 0 };

                const char *filter_name = "";
                if (choice == 9) { kernel = allocateKernel3x3(k_box); filter_name = "Box Blur"; }
                else if (choice == 10) { kernel = allocateKernel3x3(k_gauss); filter_name = "Gaussian Blur"; }
                else if (choice == 11) { kernel = allocateKernel3x3(k_outline); filter_name = "Outline"; }
                else if (choice == 12) { kernel = allocateKernel3x3(k_emboss); filter_name = "Emboss"; }
                else if (choice == 13) { kernel = allocateKernel3x3(k_sharpen); filter_name = "Sharpen"; }

                if (kernel) {
                    if (img8) bmp8_applyFilter(img8, kernel, 3);
                    else if (img24) bmp24_applyConvolutionFilter(img24, kernel, 3); //24-bit wrapper
                    printf("%s filter applied.\n", filter_name);
                    freeKernel(kernel, 3);
                } else {
                    printf("Failed to create kernel.\n");
                }
            }
        }
         // Histogram Equalization
         else if (choice == 14) { // Equalize
            if (img8) {
                bmp8_equalize(img8);
            } else if (img24) {
                bmp24_equalize(img24);
            } else {
                printf("No image loaded.\n");
            }
        }
        // Quit
        else if (choice == 0) {
            printf("Exiting...\n");
            break; // Exit while loop
        }
        //Invalid Choice
        else {
            printf("Invalid choice. Please try again.\n");
        }
    } // End while loop

    // Clean up before exiting
    if (img8) bmp8_free(img8);
    if (img24) bmp24_free(img24);

    return 0;
}
