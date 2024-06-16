#include <stdio.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "sdkconfig.h"

#include "driver/gpio.h"
#include "driver/ledc.h"

#include "updateOTA/updateOTA.h"
#include "pwmCON/pwmCON.h"

#include "esp_sleep.h"

#include <stdlib.h>


char *TAG = "ESP32 dev";
uint8_t ble_addr_type;

void ble_app_advertise(void);

// Write data to ESP32 defined as server
static int device_write(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{

    if(intit_partition_()){
        process_data((uint8_t*)ctxt->om->om_data, ctxt->om->om_len);
    } else {
        ESP_LOGE("OTA", "**intit ERR**");
    }
    
    return 0;
}

int setMode = 50;

static int MODEDATA(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    char *data = (char *)ctxt->om->om_data;

    setMode = atoi(data); 
    ESP_LOGI("data", "data : %s\n", data);
    memset(data, 0, ctxt->om->om_len); // Clear entire buffer length

    return 0;
}

void SetMode_watt() {
    printf("%d", setMode); // Print current setMode for debugging

    // Example loop logic based on setMode
    for(int duty = 0; duty <= 1024; duty += setMode) {
        pwm_con(duty);
        vTaskDelay(1);
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);

    for(int duty = 1024; duty >= 0; duty -= setMode) {
        pwm_con(duty);
        vTaskDelay(1);
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

// Read data from ESP32 defined as server
static int device_read(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    //os_mbuf_append(ctxt->om, "Data from the server", strlen("Data from the server"));
    return 0;
}

// Array of pointers to other service definitions
// UUID - Universal Unique Identifier
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID16_DECLARE(0x180),                 // Define UUID for device type
     .characteristics = (struct ble_gatt_chr_def[]){
         {.uuid = BLE_UUID16_DECLARE(0xFEF4),           // Define UUID for reading
          .flags = BLE_GATT_CHR_F_READ,
          .access_cb = device_read},
         {.uuid = BLE_UUID16_DECLARE(0xDEAD),        // Define UUID for writing
          .flags = BLE_GATT_CHR_F_WRITE,
          .access_cb = device_write},
          {.uuid = BLE_UUID16_DECLARE(0xFEAD),        // Define UUID for writing
          .flags = BLE_GATT_CHR_F_WRITE,
          .access_cb = MODEDATA},
         {0}}},
    {0}};

// BLE event handling
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {
    // Advertise if connected
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI("GAP", "BLE GAP EVENT CONNECT %s", event->connect.status == 0 ? "OK!" : "FAILED!");
        if (event->connect.status != 0)
        {
            ble_app_advertise();
        }
        break;
    // Advertise again after completion of the event
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI("GAP", "BLE GAP EVENT DISCONNECTED");
        //read_and_log_file("/littlefs/firmware.bin");
        //update_firmware();
        start_update_();
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI("GAP", "BLE GAP EVENT");
        ble_app_advertise();
        break;
    default:
        break;
    }
    return 0;
}

// Define the BLE connection
void ble_app_advertise(void)
{
    // GAP - device name definition
    struct ble_hs_adv_fields fields;
    const char *device_name;
    memset(&fields, 0, sizeof(fields));
    device_name = ble_svc_gap_device_name(); // Read the BLE device name
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    // GAP - device connectivity definition
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; // connectable or non-connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; // discoverable or non-discoverable
    adv_params.itvl_min = 0x100; // ช่วงเวลาโฆษณาขั้นต่ำ
    adv_params.itvl_max = 0x200; // ช่วงเวลาโฆษณาสูงสุด

    ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
}

// The application
void ble_app_on_sync(void)
{
    ble_hs_id_infer_auto(0, &ble_addr_type); // Determines the best address type automatically
    ESP_LOGI(TAG, "BLE synced");
    ble_app_advertise();                     // Define the BLE connection
}

// The infinite task
void host_task(void *param)
{
    nimble_port_run(); // This function will return only when nimble_port_stop() is executed
}

void app_main()
{
    nvs_flash_init();                           // 1 - Initialize NVS flash using
    // esp_nimble_hci_and_controller_init();      // 2 - Initialize ESP controller
    nimble_port_init();                        // 3 - Initialize the host stack
    ble_svc_gap_device_name_set("ESP32 dev"); // 4 - Initialize NimBLE configuration - server name
    ble_svc_gap_init();                        // 4 - Initialize NimBLE configuration - gap service
    ble_svc_gatt_init();                       // 4 - Initialize NimBLE configuration - gatt service
    ble_gatts_count_cfg(gatt_svcs);            // 4 - Initialize NimBLE configuration - config gatt services
    ble_gatts_add_svcs(gatt_svcs);             // 4 - Initialize NimBLE configuration - queues gatt services.
    ble_hs_cfg.sync_cb = ble_app_on_sync;      // 5 - Initialize application
    nimble_port_freertos_init(host_task);      // 6 - Run the thread

    // Configure GPIO pin as output
    //gpio_set_direction(LED, GPIO_MODE_OUTPUT);

    // while (1)
    // {
    //     // gpio_set_level(LED, 1);
    //     // vTaskDelay(100 / portTICK_PERIOD_MS);
    //     // gpio_set_level(LED, 0);
    //     // vTaskDelay(100 / portTICK_PERIOD_MS);
    //     setPWM();
    // }
    ESP_LOGI(TAG, "V1.0");
    check_partitions();
   
    intit_pin_pwm(8);

    while (1)
    {
      SetMode_watt();
    }
    
}