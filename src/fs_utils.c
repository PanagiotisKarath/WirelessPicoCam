#include <stdio.h>
#include <stdbool.h>
#include "filesystem/vfs.h"
#include <errno.h>
#include <string.h>
#include "camera.h"

extern uint8_t image_buf[324*324];
extern struct arducam_config config;

void save_image_flash(void *image) {
    printf("Writing to flash\n"); // DEBUG
    FILE *picture_file = fopen("/image.pgm", "w");
    if (picture_file == NULL) {
        printf("ERROR: could not open file (%s)", strerror(errno));
    }
    // Magic number
    fprintf(picture_file, "P5\n");
    // Dimensions
    fprintf(picture_file, "324 324\n");
    // Max value 
    fprintf(picture_file, "255\n");
    // Write image data
    size_t written = fwrite(image, sizeof(uint8_t), 104976 * sizeof(uint8_t), picture_file);
    int err = fclose(picture_file);
    if (err == -1) {
        printf("ERROR: could not close file (%s)", strerror(errno));
    }
}

// MYTODO: SAVE A LOT OF FILES WITH DIFFERENT NAMES
void save_image_sd(void *image) {
    printf("Writing to SD card\n"); // DEBUG
    FILE *picture_file = fopen("/sd/image.pgm", "w");
    if (picture_file == NULL) {
        printf("ERROR: could not open file (%s)", strerror(errno));
    }
    // Magic number
    fprintf(picture_file, "P5\n");
    // Dimensions
    fprintf(picture_file, "324 324\n");
    // Max value 
    fprintf(picture_file, "255\n");
    // Write image data
    size_t written = fwrite(image, sizeof(uint8_t), 104976 * sizeof(uint8_t), picture_file);
    int err = fclose(picture_file);
    if (err == -1) {
        printf("ERROR: could not close file (%s)", strerror(errno));
    }
}