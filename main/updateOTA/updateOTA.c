#include "updateOTA.h"

const esp_partition_t *update_partition = NULL;
bool ota_started = false;
esp_ota_handle_t ota_handle;
char * TAG_ = "OTA";
int segment = 0;


void check_partitions()
{
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    if (running_partition != NULL)
    {
        ESP_LOGI("Partition Info", "Running partition: %s", running_partition->label);
    }
    else
    {
        ESP_LOGE("Partition Info", "Failed to get running partition");
    }

    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while (it != NULL)
    {
        const esp_partition_t *part = esp_partition_get(it);
        ESP_LOGI("Partition Info", "Found partition: %s of size %lu", part->label, (unsigned long)part->size);
        it = esp_partition_next(it);
    }
    esp_partition_iterator_release(it);
}

// Function to update firmware
bool intit_partition_()
{
    const esp_partition_t *ota_partition = NULL;
    if (segment == 0) {
        // Begin OTA update
        const esp_partition_t *running_partition = esp_ota_get_running_partition();
        const char *factory_label = "factory";
        const char *ota_0_label = "ota_0";
        const char *ota_1_label = "ota_1";

        if (strcmp(running_partition->label, factory_label) == 0) {
            ota_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
        }
        if(strcmp(running_partition->label, ota_0_label) == 0){
            ota_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
        }
        if(strcmp(running_partition->label, ota_1_label) == 0){
            ota_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
        }

        if (!ota_partition) {
            ESP_LOGE(TAG_, "Failed to find OTA partition");
            return false;
        }

        esp_err_t err = esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_, "esp_ota_begin failed, error=%d", err);
            return false;
        }

        ESP_LOGI(TAG_, "OTA update started...");
    }
    return true;
}

// Function to finalize and boot to new firmware
void start_update_()
{
    esp_err_t err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_, "esp_ota_end failed, error=%d", err);
        return;
    }

    const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
    err = esp_ota_set_boot_partition(ota_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_, "esp_ota_set_boot_partition failed, error=%d", err);
        return;
    }

    ESP_LOGI(TAG_, "OTA update completed, rebooting...");
    esp_restart();
}


size_t data_len = 0;

int process_data(uint8_t *data, size_t len) {

    printf("firmware size : %zu ", data_len);
    for (int i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
    
    if (segment > 0) {
        // Print data in hexadecimal format
        data_len+=len;
        printf("Data in hexadecimal %zu bytes / %d ", data_len, segment);
        for (int i = 0; i < len; i++) {
            printf("%02X ", data[i]);
        }
        printf("\n");

            // Write data to OTA partition
            esp_err_t err = esp_ota_write(ota_handle, data, len);
            if (err != ESP_OK) {
                ESP_LOGE(TAG_, "esp_ota_write failed, error=%d", err);
                esp_ota_end(ota_handle);
                return -1;
            }
    }
    segment += 1;
    return 0;
}