#ifndef FSUTILS_H
#define FSUTILS_H

/*
 * Saves the last image captured on the on-board flash memory, overwritting the
 * last saved image.
 * 
 * @param image Void pointer to the first element of the image array 
*/
void save_image_flash(void *image);

/*
 * Saves an image on the SD card, by creating a new unique name for it.
 
 * @param image Void pointer to the first element of the image array 
*/
void save_image_sd(void *image);

#endif