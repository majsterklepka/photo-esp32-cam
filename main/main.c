/**
 * @file main.c
 *
 * 	 @date wto, 1 mar 2022, 15:09:28 CET 
 *    @license GNU GPLv3
 *     @author Paweł Sobótka <48721262935pl@gmail.com>
 *  @copyright © Paweł Sobótka, 2017-2022, all rights reserved
 *        URL: https://github.com/majsterklepka/photo-esp23-cam
 *    Company: mgr inż. Paweł Sobótka, self-employed, individual creator 
 *    Address: POLAND, masovian, Szydłowiec, 26-500
 * NIP(taxid): 799-169-51-12
 *
 * This file is part of camera-sd-card.
 *
 *   camera-sd-card is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   camera-sd-card is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with camera-sd-card.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


// =============================== SETUP ======================================

// 1. Board setup (Uncomment):
// #define BOARD_WROVER_KIT
#define BOARD_ESP32CAM_AITHINKER

/**
 * 2. Kconfig setup
 *
 * If you have a Kconfig file, copy the content from
 *  https://github.com/espressif/esp32-camera/blob/master/Kconfig into it.
 * In case you haven't, copy and paste this Kconfig file inside the src directory.
 * This Kconfig file has definitions that allows more control over the camera and
 * how it will be initialized.
 */

/**
 * 3. Enable PSRAM on sdkconfig:
 *
 * CONFIG_ESP32_SPIRAM_SUPPORT=y
 *
 * More info on
 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html#config-esp32-spiram-support
 */

// ================================ CODE ======================================

#include <esp_event_loop.h>
#include <esp_log.h>
#include "esp_sleep.h"
#include <esp_system.h>
#include <nvs_flash.h>
#include "nvs.h"
#include <sys/param.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

#include "driver/gpio.h"

#include "esp_camera.h"

//#define USE_SPI_MODE

#define FLASH_GPIO	4
//#define FLASH_ON // uncomment for set flash led on

// WROVER-KIT PIN Map
#ifdef BOARD_WROVER_KIT

#define CAM_PIN_PWDN -1  //power down is not used
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 21
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 19
#define CAM_PIN_D2 18
#define CAM_PIN_D1 5
#define CAM_PIN_D0 4
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

#endif

// ESP32Cam (AiThinker) PIN Map
#ifdef BOARD_ESP32CAM_AITHINKER

#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

#endif


#ifdef USE_SPI_MODE
// Pin mapping when using SPI mode.
// With this mapping, SD card can be used both in SPI and 1-line SD mode.
// Note that a pull-up on CS line is required in SD mode.
#define PIN_NUM_MISO 2
#define PIN_NUM_MOSI 15
#define PIN_NUM_CLK  14
#define PIN_NUM_CS   13
#endif //USE_SPI_MODE



static const char *TAG = "take_picture";
static const char *TAG1 = "SD.card";
static const char *TAG2 = "CAMERA";

static camera_config_t camera_config =
{
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sscb_sda = CAM_PIN_SIOD,
    .pin_sscb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_UXGA,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 12, //0-63 lower number means higher quality
    .fb_count = 1       //if more than one, i2s runs in continuous mode. Use only with JPEG
};

static esp_err_t init_camera()
{
    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
        {
            ESP_LOGE(TAG2, "Camera Init Failed");
            return err;
        }

    return ESP_OK;
}

static esp_err_t init_sd_card()
{
    ESP_LOGI(TAG1, "Initializing SD card");

#ifndef USE_SPI_MODE
    ESP_LOGI(TAG1, "Using SDMMC peripheral");
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // To use 1-line SD mode, uncomment the following line:
    // slot_config.width = 1;
    // GPIOs 15, 2, 4, 12, 13 should have external 10k pull-ups.
    // Internal pull-ups are not sufficient. However, enabling internal pull-ups
    // does make a difference some boards, so we do that here.
    gpio_set_pull_mode(15, GPIO_PULLUP_ONLY);   // CMD, needed in 4- and 1- line modes
    gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);    // D0, needed in 4- and 1-line modes
    gpio_set_pull_mode(4, GPIO_PULLUP_ONLY);    // D1, needed in 4-line mode only
    gpio_set_pull_mode(12, GPIO_PULLUP_ONLY);   // D2, needed in 4-line mode only
    gpio_set_pull_mode(13, GPIO_PULLUP_ONLY);   // D3, needed in 4- and 1-line modes

#else
    ESP_LOGI(TAG1, "Using SPI peripheral");
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
    slot_config.gpio_miso = PIN_NUM_MISO;
    slot_config.gpio_mosi = PIN_NUM_MOSI;
    slot_config.gpio_sck  = PIN_NUM_CLK;
    slot_config.gpio_cs   = PIN_NUM_CS;
    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
#endif //USE_SPI_MODE

    sdmmc_card_t* card;

    esp_vfs_fat_sdmmc_mount_config_t mount_config =
    {
        .format_if_mount_failed = false,
        .max_files = 3,
        .allocation_unit_size = 16 * 512
    };

    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK)
        {
            if (ret == ESP_FAIL)
                {
                    ESP_LOGE(TAG1, "Failed to mount filesystem. "
                             "If you want the card to be formatted, set format_if_mount_failed = true.");
                }
            else
                {
                    ESP_LOGE(TAG1, "Failed to initialize the card (%s). "
                             "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
                }
            return ESP_FAIL;
        }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    return ESP_OK;
}

static esp_err_t save_pic(camera_fb_t *pic)
{

    init_sd_card();

    struct stat sb;

    ESP_LOGI(TAG1, "Opening file");
    FILE *index = fopen("/sdcard/index.dat", "a");
    if (index == NULL)
        {

            ESP_LOGE(TAG1, "Failed to open file index.dat for writing");
            return ESP_FAIL;

        }
    else
        {
            stat("/sdcard/index.dat", &sb);
        }

    long dat = sb.st_size + 1;

    char k = '*';
    char path[40];
    sprintf(path, "/sdcard/pict%03ld.jpg", dat);
    ESP_LOGI(TAG1, "name of file %s", path);

    FILE* f = fopen(path, "w");

    if (f == NULL)
        {
            ESP_LOGE(TAG1, "Failed to open file for writing");
            return ESP_FAIL;
        }
    fwrite(pic->buf, sizeof(uint8_t), pic->len, f);
    fclose(f);
    ESP_LOGI(TAG1, "File written");

    fprintf(index, "%c", k);
    fclose(index);
    ESP_LOGI(TAG1, "index updated");


    return ESP_OK;
}



void app_main()
{

    init_camera();
    int k = 0;

    camera_fb_t *pic;


    while(k < 5)
        {
#ifdef FLASH_ON
            gpio_pad_select_gpio(FLASH_GPIO);// for flash led
            gpio_set_direction(FLASH_GPIO, GPIO_MODE_OUTPUT);

            gpio_set_level(FLASH_GPIO, 1); //flash led
#endif
            ESP_LOGI(TAG, "Taking picture...");
            pic = esp_camera_fb_get();//get framebuffer

            // use pic->buf to access the image
            ESP_LOGI(TAG, "Picture taken! Its size was: %zu bytes", pic->len);

#ifdef FLASH_ON
            gpio_set_level(FLASH_GPIO, 0);// down led
#endif


            if ( save_pic(pic) == ESP_OK)
                {
                    ESP_LOGI(TAG1, "Picture saved!");
                    esp_vfs_fat_sdmmc_unmount();//umount sd card
                    ESP_LOGI(TAG1, "Card unmounted");
                }

            k++; // increment numer of pictures taken at once run

            esp_camera_fb_return(pic); //return pointer

            vTaskDelay(500/portTICK_PERIOD_MS);
        }

    esp_deep_sleep_start(); //support for power saving


}

