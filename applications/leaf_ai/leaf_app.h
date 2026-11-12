#ifndef LEAF_APP_H
#define LEAF_APP_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    bool sensor_valid;
    float temperature_c;
    float humidity_pct;
    bool environment_alarm;

    bool result_valid;
    uint8_t class_id;
    float confidence;
    uint32_t inference_ms;
    bool disease_alarm;
} leaf_app_snapshot_t;

void leaf_app_get_snapshot(leaf_app_snapshot_t *snapshot);
const char *leaf_app_class_name(uint8_t class_id);

#endif

