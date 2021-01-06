#include "wifi.h"

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>

static const char *TAG = "WiFi";

#define WIFI_CONNECTED_BIT BIT0
static EventGroupHandle_t wifi_event_group;

static void on_wifi_prov_event(void *arg, esp_event_base_t event_base,
                               int event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_PROV_START:
        ESP_LOGI(TAG, "Wi-Fi provisioning started.");
        break;
    case WIFI_PROV_CRED_RECV:
    {
        wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
        ESP_LOGI(TAG, "Received Wi-Fi credentials\n"
                      "\tSSID: %s\n"
                      "\tPASS: %s",
                 (const char *)wifi_sta_cfg->ssid,
                 (const char *)wifi_sta_cfg->password);
        break;
    }
    case WIFI_PROV_CRED_FAIL:
    {
        wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
        ESP_LOGE(TAG, "Wi-Fi provisioning failed. Reason:\n"
                      "\t%s\n"
                      "Please reset and try again.",
                 (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "Wi-Fi authentication failed." : "Wi-Fi access-point not found");
        break;
    }
    case WIFI_PROV_CRED_SUCCESS:
        ESP_LOGI(TAG, "Provisioning success.");
        break;
    case WIFI_PROV_END:
        wifi_prov_mgr_deinit();
        break;
    default:
        break;
    }
}

esp_err_t custome_prov_data_handler(uint32_t session_id,
                                    const uint8_t *inbuf, ssize_t inlen,
                                    uint8_t **outbuf, ssize_t *outlen,
                                    void *priv_data)
{
    if (inbuf)
        ESP_LOGI(TAG, "Data received: %.*s", inlen, (char *)inbuf);

    char response[] = "SUCCESS";
    *outbuf = (uint8_t *)strdup(response);
    if (*outbuf == NULL)
    {
        ESP_LOGE(TAG, "Out of Memory");
        return ESP_ERR_NO_MEM;
    }
    *outlen = strlen(response) + 1;

    return ESP_OK;
}

static void on_wifi_event(void *arg, esp_event_base_t event_base,
                          int event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "Wi-Fi disconnected. ");
        esp_wifi_connect();
        break;
    case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG, "Connecting to AP...");
        esp_wifi_connect();
        break;
    }
}

static void on_ip_event(void *arg, esp_event_base_t event_base,
                        int event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Connected with IP:" IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
}

esp_err_t wifi_start()
{

    wifi_event_group = xEventGroupCreate();

    // Register event handler for Wi-Fi, IP and Provisioning related events
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                                               ESP_EVENT_ANY_ID, &on_wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                                               IP_EVENT_STA_GOT_IP, &on_ip_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT,
                                               ESP_EVENT_ANY_ID, on_wifi_prov_event, NULL));

    // Initialize Wi-Fi with default configuration
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Initialize the provision manager
    wifi_prov_mgr_config_t prov_cfg = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM};
    ESP_ERROR_CHECK(wifi_prov_mgr_init(prov_cfg));

    // Grab the provisioning state
    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (provisioned)
    {
        ESP_LOGI(TAG, "Wi-Fi is alread provision. starting...");
        // shutdown wifi provision manager
        wifi_prov_mgr_deinit();
        // start the station
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
    }
    else
    {
        ESP_LOGI(TAG, "Wi-Fi is not provisioned, initiating the provisioning process.");

        wifi_prov_security_t prov_security = WIFI_PROV_SECURITY_1;
        const char *pass = "7788665";
        const char *service_key = NULL;
        const char *service_name = "motoilet";
        uint8_t custom_service_uuid[] = {
            0x5F, 0x89, 0x5D, 0x5A, 0x76, 0x92, 0x47, 0x4C,
            0xA1, 0xF4, 0x76, 0xB0, 0x66, 0x97, 0xAD, 0xF7};

        wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid);
        wifi_prov_mgr_endpoint_create("custom-data");

        // start provisioning service
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(prov_security, pass, service_name, service_key));
        wifi_prov_mgr_endpoint_register("custom-data", custome_prov_data_handler, NULL);
    }

    return ESP_OK;
}

void wait_for_wifi()
{
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
}