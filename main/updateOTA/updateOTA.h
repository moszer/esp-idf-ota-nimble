#ifndef UPDATE_OTA_H
#define UPDATE_OTA_H

#include <string.h>
#include "esp_partition.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_event.h"

void check_partitions();
bool intit_partition_();
void start_update_();
int process_data(uint8_t *data, size_t len);

#endif // UPDATE_OTA_H
