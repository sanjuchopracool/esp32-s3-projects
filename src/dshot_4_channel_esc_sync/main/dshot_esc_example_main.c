/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "dshot_esc_encoder.h"
#include "driver/gpio.h"

#if CONFIG_IDF_TARGET_ESP32H2
#define DSHOT_ESC_RESOLUTION_HZ 32000000 // 32MHz resolution, DSHot protocol needs a relative high resolution
#else
#define DSHOT_ESC_RESOLUTION_HZ 40000000 // 40MHz resolution, DSHot protocol needs a relative high resolution
#endif
#define SPEED_CHANGE_GPIO_NUM      4

static const char *TAG = "example";

#define NUM_DSHOT_ESC_CHANNELS      4
void app_main(void)
{
    ESP_LOGI(TAG, "Create RMT TX channel");
    rmt_channel_handle_t dshot_esc_channels[NUM_DSHOT_ESC_CHANNELS] = {NULL};
    int dshot_esc_gpio_numbers[NUM_DSHOT_ESC_CHANNELS] = {1, 2, 42, 41};

    for (int i = 0; i < NUM_DSHOT_ESC_CHANNELS; ++i) {
        rmt_tx_channel_config_t tx_chan_configs = {
            .clk_src = RMT_CLK_SRC_DEFAULT, // select a clock that can provide needed resolution
            .gpio_num = dshot_esc_gpio_numbers[i],
            .mem_block_symbols = 48,
            .resolution_hz = DSHOT_ESC_RESOLUTION_HZ,
            .trans_queue_depth = 10, // set the number of transactions that can be pending in the background
        };
        ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_configs, &dshot_esc_channels[i]));
        ESP_LOGI(TAG, "Created RMT TX channel %d", i);
    }

    ESP_LOGI(TAG, "Install Dshot ESC encoder");
    rmt_encoder_handle_t dshot_encoders[NUM_DSHOT_ESC_CHANNELS] = {NULL};
    for (int i = 0; i < NUM_DSHOT_ESC_CHANNELS; ++i) {
        dshot_esc_encoder_config_t encoder_config = {
        .resolution = DSHOT_ESC_RESOLUTION_HZ,
        .baud_rate = 300000, // DSHOT300 protocol
        .post_delay_us = 50, // extra delay between each frame
        };
        ESP_ERROR_CHECK(rmt_new_dshot_esc_encoder(&encoder_config, &dshot_encoders[i]));
    }

    ESP_LOGI(TAG, "Enable RMT TX channel");
    for (int i = 0; i < NUM_DSHOT_ESC_CHANNELS; ++i) {
        ESP_ERROR_CHECK(rmt_enable(dshot_esc_channels[i]));
        ESP_LOGI(TAG, "Enabled RMT TX channel %d", i);
    }

    // install sync manager
    // rmt_sync_manager_handle_t synchro = NULL;
    // rmt_sync_manager_config_t synchro_config = {
    //     .tx_channel_array = dshot_esc_channels,
    //     .array_size = sizeof(dshot_esc_channels) / sizeof(dshot_esc_channels[0]),
    // };
    // ESP_ERROR_CHECK(rmt_new_sync_manager(&synchro_config, &synchro));

    rmt_transmit_config_t tx_config = {
        .loop_count = -1, // infinite loop
    };
    dshot_esc_throttle_t throttle = {
        .throttle = 0,
        .telemetry_req = false, // telemetry is not supported in this example
    };

    ESP_LOGI(TAG, "Start ESC by sending zero throttle for a while...");
    for (int i = 0; i < NUM_DSHOT_ESC_CHANNELS; ++i) {
        ESP_ERROR_CHECK(rmt_transmit(dshot_esc_channels[i], dshot_encoders[i], &throttle, sizeof(throttle), &tx_config));
    }
    vTaskDelay(pdMS_TO_TICKS(5000));

    ESP_LOGI(TAG, "Increase throttle, no telemetry");
    uint16_t thro = 50;
    gpio_set_direction(SPEED_CHANGE_GPIO_NUM, GPIO_MODE_OUTPUT);
    gpio_set_level(SPEED_CHANGE_GPIO_NUM, 1);         // Turn the LED on
    while(1) {
        throttle.throttle = thro;
        // ESP_LOGI(TAG, "Throttle %d", thro);
        gpio_set_level(SPEED_CHANGE_GPIO_NUM, 0);
        // loop takes around 105 micro second with 240 MHz
        for (int i = 0; i < NUM_DSHOT_ESC_CHANNELS; ++i) {
            throttle.throttle = thro + 10*i;
            ESP_ERROR_CHECK(rmt_transmit(dshot_esc_channels[i], dshot_encoders[i], &throttle, sizeof(throttle), &tx_config));
            // the previous loop transfer is till undergoing, we need to stop it and restart,
            // so that the new throttle can be updated on the output
            ESP_ERROR_CHECK(rmt_disable(dshot_esc_channels[i]));
            ESP_ERROR_CHECK(rmt_enable(dshot_esc_channels[i]));
        }

        gpio_set_level(SPEED_CHANGE_GPIO_NUM, 1);         // Turn the LED on
        // ESP_LOGI(TAG, "Throttle %d", thro);
        vTaskDelay(pdMS_TO_TICKS(1));
        thro += 1;
        if (thro > 400) {
            thro = 50;
        }
    }

}
