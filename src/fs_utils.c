#include <stdio.h>
#include <stdbool.h>
#include "filesystem/vfs.h"
#include <errno.h>
#include <string.h>
#include "camera.h"

extern uint8_t image_buf[324*324];
extern struct arducam_config config;

/* 
 * Opens "config.txt" file, gets the number written in it, and increments
 * the value in the file.
*/
int return_next_image_index() {
    int image_number, new_number;
    FILE *config_file = fopen("/sd/config.txt", "r");
    if (config_file == NULL) {
        printf("ERROR: could not open config file for reading (%s)\n", strerror(errno));
        return 0;
    }
    fscanf(config_file, "%d", &image_number);
    fclose(config_file);

    config_file = fopen("/sd/config.txt", "w");
    if (config_file == NULL) {
        printf("ERROR: could not open config file for writing (%s)\n", strerror(errno));
        return 0;
    }
    new_number = image_number + 1;
    fprintf(config_file, "%d", new_number);
    fclose(config_file);
    return image_number;
}

void save_image_flash(void *image) {
    FILE *picture_file = fopen("/image.pgm", "w");
    if (picture_file == NULL) {
        printf("ERROR: could not open image file (%s)", strerror(errno));
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

void save_image_sd(void *image) {
    printf("Writing to SD card\n"); // DEBUG
    int image_number = return_next_image_index();
    char filename[50];
    snprintf(filename, sizeof(filename), "/sd/image%d.pgm", image_number);

    FILE *picture_file = fopen(filename, "w");
    if (picture_file == NULL) {
        printf("ERROR: could not open image file (%s)", strerror(errno));
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