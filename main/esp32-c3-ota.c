#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "updateOTA/updateOTA.h" //ota update controller
#include "pwmCON/pwmCON.h" //pwm controller
#include "esp_rom_sys.h"
#include "esp_sleep.h"

#include "esp_system.h"
#include "esp_task_wdt.h"


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

int setMode = 0;

static int MODEDATA(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    char *data = (char *)ctxt->om->om_data;

    int inputValue = atoi(data); 
    setMode = (inputValue * 1024) / 100;  // Mapping input value to range 0-1024
    pwm_con(setMode);
    ESP_LOGI("data", "data : %s, mapped setMode : %d\n", data, setMode);
    memset(data, 0, ctxt->om->om_len); // Clear entire buffer length

    return 0;
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
        ble_app_advertise();
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

#define IO_MUX_GPIO7_REG         0x60009020
#define GPIO_ENABLE_W1TS_REG     0x60004024
#define GPIO_OUT_W1TS_REG        0x60004008
#define GPIO_OUT_W1TC_REG        0x6000400C
#define GPIO7_MASK               (1 << 7)

void init_gpio7() {
    const char *tag = "GPIO_INIT";
    const char *prefix = "Address: ";
    ESP_LOGI(tag, "%s%p %p %p %p", prefix, (void *)IO_MUX_GPIO7_REG, (void *)GPIO_ENABLE_W1TS_REG, (void *)GPIO_OUT_W1TS_REG, (void *)GPIO_OUT_W1TC_REG);

    *(volatile uint32_t*)IO_MUX_GPIO7_REG = (1 << 12);
    *(volatile uint32_t*)GPIO_ENABLE_W1TS_REG = GPIO7_MASK;
}

//convert hz to milliseconds
float frequency_milli(float Hz) {
    return ((1.0f / Hz) * 1000.0f);
}

//controller dutycircle
int duty = 0;
void DUTY_CONTROLLER(){
    while (1) {
        for(float duty_=-100; duty_<=100; duty_+=0.1){
            duty = duty_;
            //printf("PWM UPPER...: %f\n", duty_);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        for(float duty_=100; duty_>=-100; duty_-=0.1){
            duty = duty_;
            //printf("PWM LOWER...: %f\n", duty_);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
}

#define frequency_hz 5000

//controller pwm
void PWM_CONTROLLER() {
    while (1)
    {
        int max_time = (int)round(frequency_milli(frequency_hz) * 1000);
        int time_h = ((max_time / 2) + duty);
        int time_l = ((max_time / 2) - duty);
        *(volatile uint32_t*)GPIO_OUT_W1TS_REG = GPIO7_MASK;
        esp_rom_delay_us(time_h);
        *(volatile uint32_t*)GPIO_OUT_W1TC_REG = GPIO7_MASK; 
        esp_rom_delay_us(time_l);
    }
}

#define DR_REG_GPIO_BASE              0x60004000
#define GPIO_ENABLE_REG_OFFSET        0x00000020
#define GPIO_ENABLE_W1TC_REG_OFFSET   0x00000028
#define GPIO_IN_REG_OFFSET            0x0000003C
#define GPIO_INPUT_IO_3               3  

#define IO_MUX_GPIO3_REG_             0x60009010 //MUX_GPIO3
#define FUN_PU_                       (1 << 8)   //ENABLE PULL-UP

#define REG_WRITE(addr, val)           (*((volatile uint32_t *)(addr))) = (val)
#define REG_READ(addr)                 (*((volatile uint32_t *)(addr)))
#define SET_PERI_REG_MASKS(reg, mask)  (*(volatile uint32_t *)(reg) |= (mask))

void intit_gpio3(){
    SET_PERI_REG_MASKS(IO_MUX_GPIO3_REG_, FUN_PU_);
    REG_WRITE(DR_REG_GPIO_BASE + GPIO_ENABLE_W1TC_REG_OFFSET, (1 << GPIO_INPUT_IO_3));
}

static const char *sleep_core = "SLEEP_CORE";
esp_err_t err;
int level = 0;  // Initialize level

void SLEEP_CONTROLLER() {
    while (1) {
        if (level == 0) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);

            ESP_LOGI(sleep_core, "Entering light sleep. Press GPIO3 to wake up.");

            // Enable GPIO wakeup on GPIO3, interrupt on high level
            esp_err_t err = esp_sleep_enable_gpio_wakeup();
            if (err != ESP_OK) {
                ESP_LOGE(sleep_core, "Failed to enable GPIO wakeup: %d", err);
            }

            err = gpio_wakeup_enable(GPIO_INPUT_IO_3, GPIO_INTR_HIGH_LEVEL);
            if (err != ESP_OK) {
                ESP_LOGE(sleep_core, "Failed to enable wakeup on GPIO3: %d", err);
            }

            // Enter light sleep
            err = esp_light_sleep_start();
            if (err != ESP_OK) {
                ESP_LOGE(sleep_core, "Failed to enter light sleep: %d", err);
            }

            // Waking up from sleep
            ESP_LOGI(sleep_core, "Woke up!");
        }
    }
}

void SENSOR_CONTROLLER(){
    ESP_LOGI(TAG, "DR_REG_GPIO_BASE: %p, GPIO_ENABLE_W1TC_REG: %p, ADD_GPIO3: %p", 
             (void*)DR_REG_GPIO_BASE, 
             (void*)GPIO_ENABLE_W1TC_REG_OFFSET, 
             (void*)(1 << 8));

    while (1)
    {   
        uint32_t gpio_in = REG_READ(DR_REG_GPIO_BASE + GPIO_IN_REG_OFFSET);
        level = (gpio_in >> GPIO_INPUT_IO_3) & 0x1;
        //ESP_LOGI(TAG, "GPIO3 is HIGH: %d", level);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void app_main()
{   
    ESP_LOGI(TAG, "THIS FIRMWARE HAVE LISENCES PODX INDUSRTRY V1.0\n");
    ESP_LOGI(TAG, "BETA VERSION\n");

    nvs_flash_init();                             // 1 - Initialize NVS flash using
    // esp_nimble_hci_and_controller_init();      // 2 - Initialize ESP controller
    nimble_port_init();                           // 3 - Initialize the host stack
    ble_svc_gap_device_name_set("ESP32 dev");     // 4 - Initialize NimBLE configuration - server name
    ble_svc_gap_init();                           // 4 - Initialize NimBLE configuration - gap service
    ble_svc_gatt_init();                          // 4 - Initialize NimBLE configuration - gatt service
    ble_gatts_count_cfg(gatt_svcs);               // 4 - Initialize NimBLE configuration - config gatt services
    ble_gatts_add_svcs(gatt_svcs);                // 4 - Initialize NimBLE configuration - queues gatt services.
    ble_hs_cfg.sync_cb = ble_app_on_sync;         // 5 - Initialize application
    nimble_port_freertos_init(host_task);         // 6 - Run the thread

    check_partitions();

    vTaskDelay(pdMS_TO_TICKS(5000)); //delay of feed watch dog

    init_gpio7(); //intit gpio7 for test
    intit_pin_pwm(2); //intit pwm gpio2 for test
    intit_gpio3(); //intit gpio3 for test

    xTaskCreate(DUTY_CONTROLLER, "DUTY_CONTROLLER", 2048, NULL, 1, NULL); //pwm with squarewave generater (DUTY_CONTROLLER)
    xTaskCreate(PWM_CONTROLLER, "PWM_CONTROLLER", 2048, NULL, 1, NULL); //pwm with squarewave generater (PWM_CONTROLLER)

    xTaskCreate(SLEEP_CONTROLLER, "SLEEP_CONTROLLER", 2048, NULL, 1, NULL); //sleep conytroller for test
    xTaskCreate(SENSOR_CONTROLLER, "SENSOR_CONTROLLER", 2048, NULL, 1, NULL); //read sensor airflow

    printf("UART OPENNING\n");

    esp_task_wdt_add(NULL);

    //feed to watch dog when mcu is glitch
    while (1)
    {
        ESP_LOGI(TAG, "Main loop running, feeding the watchdog...");
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1000)); //delay of feed watch dog
    }
}