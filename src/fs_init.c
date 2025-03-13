#include <stdio.h>
#include <string.h>
#include <hardware/clocks.h>
#include <hardware/flash.h>
#include "blockdevice/flash.h"
#include "blockdevice/sd.h"
#include "filesystem/fat.h"
#include "filesystem/littlefs.h"
#include "filesystem/vfs.h"

bool fs_init() {
    printf("Initializing custom file system\n");

    #ifdef USE_FLASH_LFS
    // Mount littlefs on the on-board flash memory to '/flash'
    blockdevice_t *flash = blockdevice_flash_create(PICO_FLASH_SIZE_BYTES - PICO_FS_DEFAULT_SIZE, 0);
    filesystem_t *lfs = filesystem_littlefs_create(500, 16);

    int err = fs_mount("/", lfs, flash);
    if (err == -1) {
        err = fs_format(lfs, flash);
        if (err == -1) {
            printf("flash fs_format error: %s", strerror(errno));
            return false;
        }
        err = fs_mount("/", lfs, flash);
        if (err == -1) {
            printf("fs_mount error: %s", strerror(errno));
            return false;
        }
    }
    #endif

    #ifdef USE_SD_FAT
    // Mount FAT on the SD Card to '/sd'
    blockdevice_t *sd = blockdevice_sd_create(spi0,
                                              PICO_DEFAULT_SPI_TX_PIN,
                                              PICO_DEFAULT_SPI_RX_PIN,
                                              PICO_DEFAULT_SPI_SCK_PIN,
                                              PICO_DEFAULT_SPI_CSN_PIN,
                                              10 * MHZ,
                                              false);
    filesystem_t *fat = filesystem_fat_create();

    int err = fs_mount("/sd", fat, sd);
    if (err == -1) {
        err = fs_format(fat, sd);
        if (err == -1) {
            printf("sd fs_format error: %s", strerror(errno));
            return false;
        }
        err = fs_mount("/sd", fat, sd);
        if (err == -1) {
            printf("fs_mount error: %s", strerror(errno));
            return false;
        }
    }
    #endif

    return true;
}

