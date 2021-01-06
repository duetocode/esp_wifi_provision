#include <stdio.h>
#include <string.h>

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>

#include "wifi/wifi.h"

static const char *TAG = "APP";


void app_main(void)
{
    // Initialize NVS
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());

    // TCP/IP Stack initialization
    ESP_ERROR_CHECK(esp_netif_init());

    // Event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Start the wifi
    ESP_ERROR_CHECK(wifi_start());

    // Wait for the Wi-Fi
    wait_for_wifi();

    while (1)
    {
        ESP_LOGI(TAG, "Hello, World!");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}