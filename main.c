#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define BMP_TYPE 0x4D42             
#define BMP_HEADER_SIZE 54          
#define BMP_COLOR_TABLE_SIZE 1024   
#define OFFSET_WIDTH 18         
#define OFFSET_HEIGHT 22        
#define OFFSET_COLOR_DEPTH 28   
#define OFFSET_IMAGE_SIZE 34    
#define OFFSET_DATA_OFFSET 10   

typedef struct {
    unsigned char header[BMP_HEADER_SIZE];           
    unsigned char colorTable[BMP_COLOR_TABLE_SIZE];  
    unsigned char *data;                             
    unsigned int width;                              
    unsigned int height;                             
    unsigned int colorDepth;                         
    unsigned int dataSize;                           
} t_bmp8;


typedef struct {
    uint16_t type;         
    uint32_t size;         
    uint16_t reserved1;    
    uint16_t reserved2;     
    uint32_t offset;        
} t_bmp_file_header;


typedef struct {
    uint32_t size;            
    int32_t  width;           
    int32_t  height;          
    uint16_t planes;          
    uint16_t bits;            
    uint32_t compression;     
    uint32_t imagesize;      
    int32_t  xresolution;     
    int32_t  yresolution;     
    uint32_t ncolors;         
    uint32_t importantcolors;
} t_bmp_info_header;



typedef struct {
    uint8_t blue;   
    uint8_t green;  
    uint8_t red;   
} t_pixel; 


typedef struct {
    unsigned char header_bytes[BMP_HEADER_SIZE]; 
    int width;                                   
    int height;                                 
    int colorDepth;                            
    uint32_t dataOffset;                      
    t_pixel **data;                           
} t_bmp24;



typedef struct {
    double y;
    double u;
    double v; 
} t_yuv;


float **allocateKernel3x3(const float values[9]) {
    // Allocate memory for 3 rows (array of float pointers)
    float **kernel = (float **)malloc(3 * sizeof(float *));
    if (!kernel) return NULL; // Allocation failed

    // Allocate memory for each row (3 floats per row)
    for (int i = 0; i < 3; i++) {
        kernel[i] = (float *)malloc(3 * sizeof(float));
        if (!kernel[i]) { // Allocation for a row failed
            fprintf(stderr, "Error: Failed to allocate kernel row %d\n", i);
            // Free previously allocated rows
            for (int j = 0; j < i; j++) free(kernel[j]);
            free(kernel); // Free the array of row pointers
            return NULL;
        }
        // Initialize kernel values
        for (int j = 0; j < 3; j++) {
            kernel[i][j] = values[i * 3 + j];
        }
    }
    return kernel;
}


void freeKernel(float **kernel, int size) {
    if (!kernel) return; // Nothing to free
    // Free each row
    for (int i = 0; i < size; i++) {
        free(kernel[i]);
    }
    // Free the array of row pointers
    free(kernel);
}

t_pixel **bmp24_allocateDataPixels(int width, int height) {
    // Validate dimensions
    if (height <= 0 || width <= 0) {
        fprintf(stderr, "Error: Invalid dimensions for pixel allocation (%d x %d)\n", width, height);
        return NULL;
    }
    // Allocate memory for rows
    t_pixel **pixels = (t_pixel **)malloc(height * sizeof(t_pixel *));
    if (!pixels) {
        fprintf(stderr, "Error: Unable to allocate memory for pixel rows.\n");
        return NULL;
    }
    // Allocate memory for each row
    for (int i = 0; i < height; i++) {
        pixels[i] = (t_pixel *)malloc(width * sizeof(t_pixel));
        if (!pixels[i]) { // Allocation for a row failed
            fprintf(stderr, "Error: Unable to allocate memory for pixel row %d.\n", i);
            // Free previously allocated rows
            for (int j = 0; j < i; j++) {
                free(pixels[j]);
            }
            free(pixels); // Free the array of row pointers
            return NULL;
        }
    }
    return pixels;
}


void bmp24_freeDataPixels(t_pixel **pixels, int height) {
    if (!pixels) return; // Nothing to free
    // Free each row
    for (int i = 0; i < height; i++) {
        free(pixels[i]);
    }
    // Free the array of row pointers
    free(pixels);
}

int bmp8_readPixelData(t_bmp8 *img, FILE *file) {
    // Size of actual pixel data in a row (width * 1 byte/pixel)
    size_t data_row_size = img->width;
    // BMP rows are padded to be a multiple of 4 bytes. Calculate stride.
    size_t row_stride = (img->width + 3) & ~3; // (width * bytes_per_pixel + 3) / 4 * 4
    // Calculate padding bytes per row
    size_t padding = row_stride - data_row_size;

    // Allocate memory for the entire pixel data (flat array)
    img->data = (unsigned char *)malloc(img->dataSize);
    if (!img->data) {
        fprintf(stderr, "Error: Could not allocate memory for 8-bit pixel data.\n");
        return 0; // Failure
    }

    // Read pixel data row by row, from bottom to top (as stored in BMP) and store it in memory top to bottom for easier access.
    for (int i = img->height - 1; i >= 0; i--) {
        // Pointer to the start of the current row in memory
        unsigned char *row_ptr = img->data + (i * img->width);
        // Read the actual pixel data for the row
        if (fread(row_ptr, 1, data_row_size, file) != data_row_size) {
            fprintf(stderr, "Error reading pixel data row (i=%d).\n", i);
            free(img->data);
            img->data = NULL;
            return 0; // Failure
        }
        // Skip padding bytes in the file
        if (padding > 0) {
            fseek(file, padding, SEEK_CUR);
        }
    }
    return 1; // Success
}

int bmp8_writePixelData(t_bmp8 *img, FILE *file) {
    // Size of actual pixel data in a row
    size_t data_row_size = img->width;
    // BMP rows are padded to be a multiple of 4 bytes
    size_t row_stride = (img->width + 3) & ~3;
    // Padding bytes per row
    size_t padding = row_stride - data_row_size;
    unsigned char padding_bytes[3] = {0, 0, 0}; // Buffer for padding bytes 

    // Write pixel data row by row, from bottom to top
    for (int i = img->height - 1; i >= 0; i--) {
        // Pointer to the start of the current row in memory (stored top-to-bottom)
        unsigned char *row_ptr = img->data + (i * img->width);
        // Write the actual pixel data for the row
        if (fwrite(row_ptr, 1, data_row_size, file) != data_row_size) {
            fprintf(stderr, "Error writing pixel data row (i=%d).\n", i);
            return 0; // Failure
        }
        // Write padding bytes if any
        if (padding > 0) {
            if (fwrite(padding_bytes, 1, padding, file) != padding) {
                 fprintf(stderr, "Error writing padding for row (i=%d).\n", i);
                 return 0; // Failure
            }
        }
    }
    return 1; // Success
}

int bmp24_readPixelData(t_bmp24 *img, FILE *file) {
    // Size of actual pixel data in a row (width * 3 bytes/pixel)
    size_t data_row_size = img->width * sizeof(t_pixel);
    // BMP rows are padded to be a multiple of 4 bytes
    size_t row_stride = (data_row_size + 3) & ~3;
    // Padding bytes per row
    size_t padding = row_stride - data_row_size;

    // Allocate memory for pixel data (2D array of t_pixel)
    img->data = bmp24_allocateDataPixels(img->width, img->height);
     if (!img->data) {
        fprintf(stderr, "Error: Could not allocate memory for 24-bit pixel data.\n");
        return 0; // Failure
    }

    // Read pixel data row by row, from bottom to top (as stored in BMP) and store it in memory top to bottom for easier access.
    for (int i = img->height - 1; i >= 0; i--) {
        // Read the actual pixel data for the row (img->width t_pixel structures)
        if (fread(img->data[i], sizeof(t_pixel), img->width, file) != img->width) {
            fprintf(stderr, "Error reading pixel data row (i=%d).\n", i);
            bmp24_freeDataPixels(img->data, img->height);
            img->data = NULL;
            return 0; // Failure
        }
        // Skip padding bytes in the file
        if (padding > 0) {
            fseek(file, padding, SEEK_CUR);
        }
    }
    return 1; // Success
}

int bmp24_writePixelData(t_bmp24 *img, FILE *file) {
    // Size of actual pixel data in a row
    size_t data_row_size = img->width * sizeof(t_pixel);
    // BMP rows are padded to be a multiple of 4 bytes
    size_t row_stride = (data_row_size + 3) & ~3;
    // Padding bytes per row
    size_t padding = row_stride - data_row_size;
    unsigned char padding_bytes[3] = {0, 0, 0}; // Buffer for padding 

    // Write pixel data row by row, from bottom to top
    for (int i = img->height - 1; i >= 0; i--) {
        // Write the actual pixel data for the row
        if (fwrite(img->data[i], sizeof(t_pixel), img->width, file) != img->width) {
             fprintf(stderr, "Error writing pixel data row (i=%d).\n", i);
            return 0; // Failure
        }
        // Write padding bytes if any
        if (padding > 0) {
            if (fwrite(padding_bytes, 1, padding, file) != padding) {
                fprintf(stderr, "Error writing padding for row (i=%d).\n", i);
                return 0; // Failure
            }
        }
    }
    return 1; // Success
}

t_bmp8 *bmp8_loadImage(const char *filename) {
    FILE *file = fopen(filename, "rb"); // Open in binary read mode
    if (!file) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return NULL;
    }

    // Allocate memory for the image structure
    t_bmp8 *img = (t_bmp8 *)malloc(sizeof(t_bmp8));
    if (!img) {
        fprintf(stderr, "Error: Cannot allocate memory for t_bmp8 structure.\n");
        fclose(file);
        return NULL;
    }
    img->data = NULL; // Initialize data pointer

    // Read the BMP header (54 bytes)
    if (fread(img->header, 1, BMP_HEADER_SIZE, file) != BMP_HEADER_SIZE) {
        fprintf(stderr, "Error: Failed to read BMP header.\n");
        fclose(file);
        free(img);
        return NULL;
    }

    // Check BMP signature ('BM')
    if (img->header[0] != 'B' || img->header[1] != 'M') {
        fprintf(stderr, "Error: Invalid BMP signature.\n");
        fclose(file);
        free(img);
        return NULL;
    }

    // Extract image metadata from the header using defined offsets
    // This relies on the system's endianness matching the field's endianness (little-endian for BMP) and correct alignment.
    img->width = *(unsigned int *)&img->header[OFFSET_WIDTH];
    img->height = *(unsigned int *)&img->header[OFFSET_HEIGHT];
    img->colorDepth = *(unsigned short *)&img->header[OFFSET_COLOR_DEPTH];
    img->dataSize = *(unsigned int *)&img->header[OFFSET_IMAGE_SIZE];
    uint32_t dataOffset = *(uint32_t *)&img->header[OFFSET_DATA_OFFSET];

    // Validate image properties for 8-bit
    if (img->colorDepth != 8) {
        fprintf(stderr, "Error: Image is not 8-bit (color depth = %u).\n", img->colorDepth);
        fclose(file);
        free(img);
        return NULL;
    }
    // If dataSize in header is 0 (allowed for uncompressed), calculate it
    if (img->dataSize == 0) {
        size_t row_stride = (img->width + 3) & ~3; // Padded row size
        img->dataSize = row_stride * img->height;
    }

    // Read the color table (palette) for 8-bit images
    if (fread(img->colorTable, 1, BMP_COLOR_TABLE_SIZE, file) != BMP_COLOR_TABLE_SIZE) {
        fprintf(stderr, "Error: Failed to read BMP color table.\n");
        fclose(file);
        free(img);
        return NULL;
    }

    // Seek to the start of pixel data
    fseek(file, dataOffset, SEEK_SET);
    // Read the pixel data
    if (!bmp8_readPixelData(img, file)) {
        fprintf(stderr, "Error: Failed to read 8-bit pixel data.\n");
        // bmp8_readPixelData frees img->data on failure if it was allocated
        fclose(file);
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
    FILE *file = fopen(filename, "wb"); // Open in binary write mode
    if (!file) {
        fprintf(stderr, "Error: Cannot create file %s\n", filename);
        return;
    }

    // Get the data offset from the stored header
    uint32_t dataOffset = *(uint32_t *)&img->header[OFFSET_DATA_OFFSET];

    // Write the BMP header
    if (fwrite(img->header, 1, BMP_HEADER_SIZE, file) != BMP_HEADER_SIZE) {
        fprintf(stderr, "Error writing BMP header.\n");
        fclose(file);
        return;
    }
    // Write the color table
    if (fwrite(img->colorTable, 1, BMP_COLOR_TABLE_SIZE, file) != BMP_COLOR_TABLE_SIZE) {
         fprintf(stderr, "Error writing BMP color table.\n");
        fclose(file);
        return;
    }

    // Seek to where pixel data should start (important if header/color table size != dataOffset)
    // For standard 8-bit BMP, dataOffset = BMP_HEADER_SIZE + BMP_COLOR_TABLE_SIZE
    fseek(file, dataOffset, SEEK_SET);

    // Write the pixel data
    if (!bmp8_writePixelData(img, file)) {
        fprintf(stderr, "Error writing 8-bit pixel data.\n");
    } else {
         printf("Saved 8-bit image successfully: %s\n", filename);
    }

    fclose(file);
}

void bmp8_free(t_bmp8 *img) {
    if (!img) return;
    free(img->data); // Free pixel data
    free(img);       // Free the structure itself
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
    printf("Data Size (from header/calculated): %u bytes\n", img->dataSize);
    printf("Calculated Pixels (width*height): %u\n", img->width * img->height);
}

t_bmp24 *bmp24_loadImage(const char *filename) {
     FILE *file = fopen(filename, "rb"); // Open in binary read mode
    if (!file) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return NULL;
    }

    // Allocate memory for the image structure
    t_bmp24 *img = (t_bmp24 *)malloc(sizeof(t_bmp24));
    if (!img) {
        fprintf(stderr, "Error: Cannot allocate memory for t_bmp24 structure.\n");
        fclose(file);
        return NULL;
    }
     img->data = NULL; // Initialize data pointer

    // Read the BMP header (54 bytes)
    if (fread(img->header_bytes, 1, BMP_HEADER_SIZE, file) != BMP_HEADER_SIZE) {
        fprintf(stderr, "Error: Failed to read BMP header.\n");
        fclose(file);
        free(img);
        return NULL;
    }

    // Check BMP signature ('BM')
    if (img->header_bytes[0] != 'B' || img->header_bytes[1] != 'M') {
        fprintf(stderr, "Error: Invalid BMP signature.\n");
        fclose(file);
        free(img);
        return NULL;
    }

    // Extract image metadata from the header using defined offsets
    img->width = *(int32_t *)&img->header_bytes[OFFSET_WIDTH];
    img->height = *(int32_t *)&img->header_bytes[OFFSET_HEIGHT];
    img->colorDepth = *(uint16_t *)&img->header_bytes[OFFSET_COLOR_DEPTH];
    img->dataOffset = *(uint32_t *)&img->header_bytes[OFFSET_DATA_OFFSET];
    // Compression method is at offset 30 in the DIB header (which starts at byte 14 of file)
    uint32_t compression = *(uint32_t *)&img->header_bytes[30];

    // Validate image properties for 24-bit uncompressed
    if (img->colorDepth != 24 || compression != 0) { // 0 for BI_RGB (no compression)
        fprintf(stderr, "Error: Image is not 24-bit uncompressed (depth=%d, compression=%u).\n", img->colorDepth, compression);
        fclose(file);
        free(img);
        return NULL;
    }
    // Validate dimensions (height can be negative for top-down BMPs, but this code handles positive height)
    if (img->width <= 0 || img->height <= 0) {
        fprintf(stderr, "Error: Invalid image dimensions (%d x %d).\n", img->width, img->height);
        fclose(file);
        free(img);
        return NULL;
    }

    // Seek to the start of pixel data
    fseek(file, img->dataOffset, SEEK_SET);
    // Read the pixel data
    if (!bmp24_readPixelData(img, file)) {
         fprintf(stderr, "Error: Failed to read 24-bit pixel data.\n");
         // bmp24_readPixelData handles freeing img->data on its own failure path
        fclose(file);
        free(img); // Free the main struct
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
    FILE *file = fopen(filename, "wb"); // Open in binary write mode
    if (!file) {
        fprintf(stderr, "Error: Cannot create file %s\n", filename);
        return;
    }

    // Write the BMP header
    if (fwrite(img->header_bytes, 1, BMP_HEADER_SIZE, file) != BMP_HEADER_SIZE) {
         fprintf(stderr, "Error writing BMP header.\n");
        fclose(file);
        return;
    }

    // Seek to where pixel data should start (as specified in header_bytes)
    fseek(file, img->dataOffset, SEEK_SET);

    // Write the pixel data
    if (!bmp24_writePixelData(img, file)) {
         fprintf(stderr, "Error writing 24-bit pixel data.\n");
    } else {
        printf("Saved 24-bit image successfully: %s\n", filename);
    }

    fclose(file);
}

void bmp24_free(t_bmp24 *img) {
    if (!img) return;
    bmp24_freeDataPixels(img->data, img->height); // Free the 2D pixel array
    free(img);                                   // Free the structure itself
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
    printf("Data Offset: %u\n", img->dataOffset); // Offset to pixel data from start of file
}

void bmp8_negative(t_bmp8 *img) {
    if (!img || !img->data) return; // Check for valid image
    unsigned int num_pixels = img->width * img->height;
    for (unsigned int i = 0; i < num_pixels; i++) {
        img->data[i] = 255 - img->data[i]; // Invert pixel value
    }
}

void bmp8_brightness(t_bmp8 *img, int value) {
    if (!img || !img->data) return; // Check for valid image
    unsigned int num_pixels = img->width * img->height;
    for (unsigned int i = 0; i < num_pixels; i++) {
        int new_val = img->data[i] + value;
        // Clamp the value to the 0-255 range
        if (new_val > 255) new_val = 255;
        if (new_val < 0) new_val = 0;
        img->data[i] = (unsigned char)new_val;
    }
}

void bmp8_threshold(t_bmp8 *img, int threshold_val) { // Renamed parameter to avoid conflict
     if (!img || !img->data) return; // Check for valid image
    unsigned int num_pixels = img->width * img->height;
     // Clamp threshold value to 0-255
     if (threshold_val < 0) threshold_val = 0;
     if (threshold_val > 255) threshold_val = 255;
    for (unsigned int i = 0; i < num_pixels; i++) {
        img->data[i] = (img->data[i] >= threshold_val) ? 255 : 0;
    }
}

void bmp8_applyFilter(t_bmp8 *img, float **kernel, int kernelSize) {
    if (!img || !img->data || !kernel) return; // Check for valid inputs
    int offset = kernelSize / 2; // e.g., for 3x3 kernel, offset is 1
    if (offset <= 0) return; // Kernel too small or invalid

    unsigned int num_pixels = img->width * img->height;
    // Create a temporary buffer to store original pixel data for convolution
    unsigned char *tempData = (unsigned char *)malloc(num_pixels * sizeof(unsigned char));
    if (!tempData) {
        fprintf(stderr, "Error: Failed to allocate temp buffer for 8-bit convolution.\n");
        return;
    }
    memcpy(tempData, img->data, num_pixels * sizeof(unsigned char));

    // Iterate over pixels, avoiding borders where kernel would go out of bounds
    for (int y = offset; y < img->height - offset; y++) {
        for (int x = offset; x < img->width - offset; x++) {
            float sum = 0;
            // Apply kernel
            for (int ky = -offset; ky <= offset; ky++) {
                for (int kx = -offset; kx <= offset; kx++) {
                    int currentY = y + ky;
                    int currentX = x + kx;
                    // Access pixel from tempData (original image)
                    sum += tempData[currentY * img->width + currentX] * kernel[ky + offset][kx + offset];
                }
            }
            // Clamp result to 0-255 range
            if (sum < 0) sum = 0;
            if (sum > 255) sum = 255;
            // Update pixel in the original image data
            img->data[y * img->width + x] = (unsigned char)round(sum);
        }
    }

    free(tempData); // Free the temporary buffer
}

void bmp24_negative(t_bmp24 *img) {
    if (!img || !img->data) return; // Check for valid image
    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++) {
            img->data[i][j].red   = 255 - img->data[i][j].red;
            img->data[i][j].green = 255 - img->data[i][j].green;
            img->data[i][j].blue  = 255 - img->data[i][j].blue;
        }
    }
}

void bmp24_grayscale(t_bmp24 *img) {
     if (!img || !img->data) return; // Check for valid image
    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++) {
             // Calculate grayscale value using standard luminance formula
             uint8_t gray = (uint8_t)(0.299 * img->data[i][j].red +
                                     0.587 * img->data[i][j].green +
                                     0.114 * img->data[i][j].blue);
            // Set R, G, and B components to the same gray value
            img->data[i][j].red = gray;
            img->data[i][j].green = gray;
            img->data[i][j].blue = gray;
        }
    }
}

void bmp24_brightness(t_bmp24 *img, int value) {
    if (!img || !img->data) return; // Check for valid image
    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++) {
            // Adjust each color component
            int newR = img->data[i][j].red + value;
            int newG = img->data[i][j].green + value;
            int newB = img->data[i][j].blue + value;
            // Clamp results to 0-255 range
            img->data[i][j].red   = (uint8_t)(fmax(0, fmin(255, newR)));
            img->data[i][j].green = (uint8_t)(fmax(0, fmin(255, newG)));
            img->data[i][j].blue  = (uint8_t)(fmax(0, fmin(255, newB)));
        }
    }
}

t_pixel bmp24_convolution(t_bmp24 *img, int y, int x, float **kernel, int kernelSize) {
    int offset = kernelSize / 2;
    double sumR = 0, sumG = 0, sumB = 0;

    // Apply kernel to the neighborhood
    for (int ky = -offset; ky <= offset; ky++) {
        for (int kx = -offset; kx <= offset; kx++) {
            int currentY = y + ky;
            int currentX = x + kx;
            // Check bounds (though bmp24_applyConvolutionFilter should ensure this for center pixel)
            if (currentY >= 0 && currentY < img->height && currentX >= 0 && currentX < img->width) {
                // Get pixel from the original image data (passed as img)
                t_pixel p = img->data[currentY][currentX]; // BGR order
                float k_val = kernel[ky + offset][kx + offset]; // Kernel value
                // Accumulate weighted sums for each color channel
                sumB += p.blue * k_val;
                sumG += p.green * k_val;
                sumR += p.red * k_val;
            }
        }
    }
    t_pixel result;
    // Clamp results to 0-255 and round
    result.blue  = (uint8_t)(fmax(0, fmin(255, round(sumB))));
    result.green = (uint8_t)(fmax(0, fmin(255, round(sumG))));
    result.red   = (uint8_t)(fmax(0, fmin(255, round(sumR))));
    return result;
}

void bmp24_applyConvolutionFilter(t_bmp24 *img, float **kernel, int kernelSize) {
    if (!img || !img->data || !kernel) return; // Check for valid inputs
    int offset = kernelSize / 2; // e.g., for 3x3 kernel, offset is 1
     // Ensure image is large enough for the kernel to operate without always being on the border
     if (offset <= 0 || img->height <= 2*offset || img->width <= 2*offset) {
         fprintf(stderr, "Error: Image too small for kernel or invalid kernel size.\n");
         return;
     }

    // Allocate a temporary buffer for the original pixel data
    t_pixel **tempData = bmp24_allocateDataPixels(img->width, img->height);
    if (!tempData) {
        fprintf(stderr, "Error: Failed to allocate temp buffer for 24-bit convolution.\n");
        return;
    }
    // Copy current image data to tempData
    for (int i = 0; i < img->height; i++) {
        memcpy(tempData[i], img->data[i], img->width * sizeof(t_pixel));
    }

    // Iterate over pixels, avoiding borders where kernel would go out of bounds
    for (int y = offset; y < img->height - offset; y++) {
        for (int x = offset; x < img->width - offset; x++) {
             // The original bmp24_convolution call was redundant here, directly compute:
             double sumR = 0, sumG = 0, sumB = 0;
             // Apply kernel using data from tempData
             for (int ky = -offset; ky <= offset; ky++) {
                 for (int kx = -offset; kx <= offset; kx++) {
                     t_pixel p = tempData[y + ky][x + kx]; // Read from temp (original) data
                     float k_val = kernel[ky + offset][kx + offset];
                     sumB += p.blue * k_val;
                     sumG += p.green * k_val;
                     sumR += p.red * k_val;
                 }
             }
             // Update pixel in the original image data
             img->data[y][x].blue  = (uint8_t)(fmax(0, fmin(255, round(sumB))));
             img->data[y][x].green = (uint8_t)(fmax(0, fmin(255, round(sumG))));
             img->data[y][x].red   = (uint8_t)(fmax(0, fmin(255, round(sumR))));
        }
    }

    bmp24_freeDataPixels(tempData, img->height); // Free the temporary buffer
}

// Predefined filter application functions for 24-bit images
// Applies a 3x3 Box Blur filter. 
void bmp24_boxBlur(t_bmp24 *img) {
    const float values[9] = { 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f };
    float **k = allocateKernel3x3(values); if(k) { bmp24_applyConvolutionFilter(img, k, 3); freeKernel(k, 3); }
}
// Applies a 3x3 Gaussian Blur filter.
void bmp24_gaussianBlur(t_bmp24 *img) {
    const float values[9] = { 1/16.0f, 2/16.0f, 1/16.0f, 2/16.0f, 4/16.0f, 2/16.0f, 1/16.0f, 2/16.0f, 1/16.0f };
    float **k = allocateKernel3x3(values); if(k) { bmp24_applyConvolutionFilter(img, k, 3); freeKernel(k, 3); }
}
// Applies a 3x3 Outline (edge detection) filter. 
void bmp24_outline(t_bmp24 *img) {
    const float values[9] = { -1, -1, -1, -1, 8, -1, -1, -1, -1 };
    float **k = allocateKernel3x3(values); if(k) { bmp24_applyConvolutionFilter(img, k, 3); freeKernel(k, 3); }
}
// Applies a 3x3 Emboss filter. 
void bmp24_emboss(t_bmp24 *img) {
    const float values[9] = { -2, -1, 0, -1, 1, 1, 0, 1, 2 };
    float **k = allocateKernel3x3(values); if(k) { bmp24_applyConvolutionFilter(img, k, 3); freeKernel(k, 3); }
}
// Applies a 3x3 Sharpen filter.
void bmp24_sharpen(t_bmp24 *img) {
    const float values[9] = { 0, -1, 0, -1, 5, -1, 0, -1, 0 };
    float **k = allocateKernel3x3(values); if(k) { bmp24_applyConvolutionFilter(img, k, 3); freeKernel(k, 3); }
}

unsigned int *bmp8_computeHistogram(t_bmp8 *img) {
    if (!img || !img->data) return NULL; // Check valid image
    // Allocate memory for histogram (256 intensity levels) and initialize to zero
    unsigned int *hist = (unsigned int *)calloc(256, sizeof(unsigned int));
    if (!hist) {
        fprintf(stderr, "Error: Cannot allocate memory for histogram.\n");
        return NULL;
    }
    unsigned int num_pixels = img->width * img->height;
    // Populate histogram
    for (unsigned int i = 0; i < num_pixels; i++) {
        hist[img->data[i]]++; // Increment count for the pixel's intensity value
    }
    return hist;
}

unsigned int *bmp8_computeCDF(unsigned int *hist) {
    if (!hist) return NULL; // Check valid histogram
    // Allocate memory for CDF
    unsigned int *cdf = (unsigned int *)malloc(256 * sizeof(unsigned int));
    if (!cdf) {
        fprintf(stderr, "Error: Cannot allocate memory for CDF.\n");
        return NULL;
    }
    // Calculate CDF
    cdf[0] = hist[0]; // CDF of first level is its histogram value
    for (int i = 1; i < 256; i++) {
        cdf[i] = cdf[i - 1] + hist[i]; 
    }
    return cdf;
}

void bmp8_equalize(t_bmp8 *img) {
    if (!img || !img->data) return; // Check valid image

    // Compute histogram and CDF
    unsigned int *hist = bmp8_computeHistogram(img);
    if (!hist) return;
    unsigned int *cdf = bmp8_computeCDF(hist);
    if (!cdf) {
        free(hist);
        return;
    }

    // Find the minimum non-zero CDF value (cdf_min)
    unsigned int cdf_min = 0;
    for (int i = 0; i < 256; i++) {
        if (cdf[i] != 0) {
            cdf_min = cdf[i];
            break;
        }
    }

    unsigned int num_pixels = img->width * img->height;

    // Denominator for equalization formula. Avoid division by zero.
    if (num_pixels - cdf_min == 0) {
        fprintf(stderr, "Warning: Cannot equalize image (num_pixels - cdf_min is zero). This might happen with uniform images.\n");
        free(hist);
        free(cdf);
        return;
    }

    // Create the equalized histogram mapping table
    unsigned char hist_eq[256];
    // Scale factor for mapping CDF values to 0-255 range
    double scale_factor = 255.0 / (num_pixels - cdf_min);
    for (int i = 0; i < 256; i++) {
         if (cdf[i] >= cdf_min) { // Apply formula only if cdf[i] is not part of the flat start
            hist_eq[i] = (unsigned char)round((double)(cdf[i] - cdf_min) * scale_factor);
         } else { // For initial zero-count intensity levels, map to 0
             hist_eq[i] = 0;
         }
    }

    // Apply the equalization map to the image pixels
    for (unsigned int i = 0; i < num_pixels; i++) {
        img->data[i] = hist_eq[img->data[i]];
    }

    // Free allocated memory
    free(hist);
    free(cdf);
    printf("8-bit histogram equalization applied.\n");
}

t_yuv rgb_to_yuv(t_pixel p) {
    t_yuv yuv;
    double r = p.red;
    double g = p.green;
    double b = p.blue;
    // Standard conversion formulas (BT.601)
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

    // Clamp values to 0-255 range and convert to uint8_t
    p.red   = (uint8_t)fmax(0, fmin(255, round(r_f)));
    p.green = (uint8_t)fmax(0, fmin(255, round(g_f)));
    p.blue  = (uint8_t)fmax(0, fmin(255, round(b_f)));
    return p;
}

void bmp24_equalize(t_bmp24 *img) {
    if (!img || !img->data) return; // Check valid image

    int width = img->width;
    int height = img->height;
    unsigned int num_pixels = width * height;

    // Allocate memory for YUV data and Y-channel histogram
    t_yuv **yuv_data = (t_yuv **)malloc(height * sizeof(t_yuv *));
    unsigned int *y_hist = (unsigned int *)calloc(256, sizeof(unsigned int)); // Histogram for Y channel
    if (!yuv_data || !y_hist) {
        fprintf(stderr, "Error: Failed to allocate memory for YUV data or Y histogram.\n");
        // Free partially allocated yuv_data if y_hist allocation failed
        if (yuv_data) {
             for(int k=0; k<height; k++) if(yuv_data[k]) free(yuv_data[k]); // Check if rows were allocated
        }
        free(yuv_data);
        free(y_hist);
        return;
    }

    // Convert RGB to YUV and compute Y-channel histogram
    for (int i = 0; i < height; i++) {
        yuv_data[i] = (t_yuv *)malloc(width * sizeof(t_yuv));
        if (!yuv_data[i]) { // Failed to allocate a row for YUV data
             fprintf(stderr, "Error: Failed to allocate memory for YUV row %d.\n", i);
             for(int k=0; k<i; k++) free(yuv_data[k]); // Free previously allocated rows
             free(yuv_data);
             free(y_hist);
             return;
        }
        for (int j = 0; j < width; j++) {
            yuv_data[i][j] = rgb_to_yuv(img->data[i][j]);
            // Clamp Y value to 0-255 for histogram indexing
            uint8_t y_clamped = (uint8_t)fmax(0, fmin(255, round(yuv_data[i][j].y)));
            y_hist[y_clamped]++;
        }
    }

    // Compute CDF for the Y channel
    unsigned int *y_cdf = bmp8_computeCDF(y_hist); // Re-use 8-bit CDF function
    if (!y_cdf) {
        fprintf(stderr, "Error: Failed to compute Y channel CDF.\n");
        for(int i=0; i<height; i++) free(yuv_data[i]);
        free(yuv_data);
        free(y_hist);
        return;
    }

    // Find minimum non-zero CDF value for Y channel
    unsigned int cdf_min_y = 0;
    for (int i = 0; i < 256; i++) { if (y_cdf[i] != 0) { cdf_min_y = y_cdf[i]; break; } }

    // Denominator for Y-channel equalization. Avoid division by zero.
    if (num_pixels - cdf_min_y == 0) {
         fprintf(stderr, "Warning: Cannot equalize Y channel (num_pixels - cdf_min_y is zero).\n");
         for(int i=0; i<height; i++) free(yuv_data[i]);
         free(yuv_data);
         free(y_hist);
         free(y_cdf);
         return;
    }

    // Create the equalization map for Y channel
    double scale_factor_y = 255.0 / (num_pixels - cdf_min_y);
    unsigned char y_map[256]; // Map for Y values
    for (int i = 0; i < 256; i++) {
        if (y_cdf[i] >= cdf_min_y) { // Apply formula
             y_map[i] = (unsigned char)round((double)(y_cdf[i] - cdf_min_y) * scale_factor_y);
        } else { // Map initial zero-count Y levels to 0
            y_map[i] = 0;
        }
    }

    // Apply equalization to Y channel and convert back to RGB
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
             // Get original Y value (clamped) to look up in the map
             uint8_t y_original_clamped = (uint8_t)fmax(0, fmin(255, round(yuv_data[i][j].y)));
             double y_new = y_map[y_original_clamped]; // New, equalized Y value
            // Create new YUV pixel with equalized Y and original U, V
            t_yuv yuv_new = { y_new, yuv_data[i][j].u, yuv_data[i][j].v };
            // Convert back to RGB and update image data
            img->data[i][j] = yuv_to_rgb(yuv_new);
        }
    }

    // Free allocated memory
    for(int i=0; i<height; i++) free(yuv_data[i]);
    free(yuv_data);
    free(y_hist);
    free(y_cdf);

    printf("24-bit histogram equalization (Y channel) applied.\n");
}

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
    printf("--- Convolution Filters (3x3) ---\n");
    printf("9. Box Blur\n");
    printf("10. Gaussian Blur\n");
    printf("11. Outline\n");
    printf("12. Emboss\n");
    printf("13. Sharpen\n");
    printf("--- Histogram Equalization ---\n");
    printf("14. Equalize Histogram\n");
    printf("0. Quit\n");
    printf(">>> Enter your choice: ");
}

int main() {
    t_bmp8 *img8 = NULL;    // Pointer to an 8-bit image structure
    t_bmp24 *img24 = NULL;  // Pointer to a 24-bit image structure
    char filepath[256];     // Buffer for file paths
    int choice;             // User's menu choice
    int value;              // Integer value for operations like brightness, threshold

    // Main menu loop
    while (1) {
        printMainMenu();
        // Read user choice, with basic input error checking
        if (scanf("%d", &choice) != 1) {
            // Clear invalid input from buffer
            while (getchar() != '\n');
            choice = -1; // Set to invalid choice
        }
        getchar(); // Consume the newline character after scanf

        // --- Load Operations ---
        if (choice == 1) { // Load 8-bit BMP
            if (img8) { bmp8_free(img8); img8 = NULL; }    // Free previous 8-bit image
            if (img24) { bmp24_free(img24); img24 = NULL; } // Free previous 24-bit image
            printf("Enter path for 8-bit BMP: ");
            fgets(filepath, sizeof(filepath), stdin);
            filepath[strcspn(filepath, "\n")] = 0; // Remove trailing newline
            img8 = bmp8_loadImage(filepath);
            if (!img8) printf("Failed to load 8-bit image.\n");
        } else if (choice == 2) { // Load 24-bit BMP
             if (img8) { bmp8_free(img8); img8 = NULL; }
             if (img24) { bmp24_free(img24); img24 = NULL; }
            printf("Enter path for 24-bit BMP: ");
            fgets(filepath, sizeof(filepath), stdin);
            filepath[strcspn(filepath, "\n")] = 0; // Remove trailing newline
            img24 = bmp24_loadImage(filepath);
             if (!img24) printf("Failed to load 24-bit image.\n");
        }
        // --- Save Operation ---
        else if (choice == 3) { // Save current image
            if (img8) { // If 8-bit image is loaded
                 printf("Enter path to save 8-bit BMP: ");
                 fgets(filepath, sizeof(filepath), stdin);
                 filepath[strcspn(filepath, "\n")] = 0;
                 bmp8_saveImage(filepath, img8);
            } else if (img24) { // If 24-bit image is loaded
                 printf("Enter path to save 24-bit BMP: ");
                 fgets(filepath, sizeof(filepath), stdin);
                 filepath[strcspn(filepath, "\n")] = 0;
                 bmp24_saveImage(filepath, img24);
            } else {
                printf("No image loaded to save.\n");
            }
        }
        // --- Display Info ---
        else if (choice == 4) { // Display image info
            if (img8) bmp8_printInfo(img8);
            else if (img24) bmp24_printInfo(img24);
            else printf("No image loaded.\n");
        }
        // --- Basic Image Operations ---
        else if (choice == 5) { // Negative
            if (img8) { bmp8_negative(img8); printf("8-bit negative applied.\n"); }
            else if (img24) { bmp24_negative(img24); printf("24-bit negative applied.\n"); }
            else printf("No image loaded.\n");
        } else if (choice == 6) { // Adjust Brightness
             printf("Enter brightness adjustment value: ");
             if (scanf("%d", &value) == 1) { // Read brightness value
                 getchar(); // Consume newline
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
                if (scanf("%d", &value) == 1) { // Read threshold value
                    getchar(); // Consume newline
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
        } else if (choice == 8) { // Convert to Grayscale (24-bit only)
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
        else if (choice >= 9 && choice <= 13) {
            if (!img8 && !img24) { // Check if any image is loaded
                printf("No image loaded.\n");
            } else {
                float **kernel = NULL; // Kernel to be applied
                // Predefined 3x3 kernels
                const float k_box[9] = { 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f };
                const float k_gauss[9] = { 1/16.0f, 2/16.0f, 1/16.0f, 2/16.0f, 4/16.0f, 2/16.0f, 1/16.0f, 2/16.0f, 1/16.0f };
                const float k_outline[9] = { -1, -1, -1, -1, 8, -1, -1, -1, -1 };
                const float k_emboss[9] = { -2, -1, 0, -1, 1, 1, 0, 1, 2 };
                const float k_sharpen[9] = { 0, -1, 0, -1, 5, -1, 0, -1, 0 };

                const char *filter_name = ""; // Name of the filter for output message
                // Allocate the appropriate kernel based on user choice
                if (choice == 9) { kernel = allocateKernel3x3(k_box); filter_name = "Box Blur"; }
                else if (choice == 10) { kernel = allocateKernel3x3(k_gauss); filter_name = "Gaussian Blur"; }
                else if (choice == 11) { kernel = allocateKernel3x3(k_outline); filter_name = "Outline"; }
                else if (choice == 12) { kernel = allocateKernel3x3(k_emboss); filter_name = "Emboss"; }
                else if (choice == 13) { kernel = allocateKernel3x3(k_sharpen); filter_name = "Sharpen"; }

                if (kernel) { // If kernel allocation was successful
                    if (img8) bmp8_applyFilter(img8, kernel, 3);         // Apply to 8-bit image
                    else if (img24) bmp24_applyConvolutionFilter(img24, kernel, 3); // Apply to 24-bit image
                    printf("%s filter applied.\n", filter_name);
                    freeKernel(kernel, 3); // Free the allocated kernel
                } else {
                    printf("Failed to create kernel.\n");
                }
            }
        }
        // --- Histogram Equalization ---
         else if (choice == 14) { // Equalize Histogram
            if (img8) {
                bmp8_equalize(img8); // Equalize 8-bit image
            } else if (img24) {
                bmp24_equalize(img24); // Equalize 24-bit image (Y channel)
            } else {
                printf("No image loaded.\n");
            }
        }
        // --- Quit ---
        else if (choice == 0) {
            printf("Exiting...\n");
            break; // Exit the main loop
        }
        // --- Invalid Choice ---
        else {
            printf("Invalid choice. Please try again.\n");
        }
    }

    // Free any loaded image data before exiting
    if (img8) bmp8_free(img8);
    if (img24) bmp24_free(img24);

    return 0; // Successful execution
}
