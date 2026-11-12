#ifndef LEAF_APP_CONFIG_H
#define LEAF_APP_CONFIG_H

#include <board.h>

/* Run msh> list_device on the board and replace this if the UART name differs. */
#define LEAF_UART_DEVICE_NAME        "uart2"
#define LEAF_UART_BAUD_RATE          1000000
#define LEAF_UART_READ_TIMEOUT_MS    15000

#define LEAF_I2C_BUS_NAME            "i2c1"
#define LEAF_ENVIRONMENT_PERIOD_MS   2000

/*
 * These thresholds are environmental warnings only. They do not diagnose
 * apple disease and must be presented separately from the image classifier.
 */
#define LEAF_TEMP_HIGH_C             32.0f
#define LEAF_HUMIDITY_HIGH_PCT       85.0f

/* Class 0 is healthy. Lower confidence is shown as uncertain, not diseased. */
#define LEAF_DISEASE_CONFIDENCE      0.60f

#define LEAF_LED_RED_PIN             GET_PIN(16, 7)
#define LEAF_LED_GREEN_PIN           GET_PIN(16, 6)
#define LEAF_LED_BLUE_PIN            GET_PIN(16, 5)
#define LEAF_LED_ON_LEVEL            PIN_LOW
#define LEAF_LED_OFF_LEVEL           PIN_HIGH

#define LEAF_MODEL_ENABLE            1
#define LEAF_MODEL_INPUT_SIZE        160
#define LEAF_MODEL_INPUT_PIXELS      (LEAF_MODEL_INPUT_SIZE * LEAF_MODEL_INPUT_SIZE)
#define LEAF_MODEL_FLOAT_VALUES      (LEAF_MODEL_INPUT_PIXELS * 3)
#define LEAF_IMAGE_SIZE              128
#define LEAF_IMAGE_BYTES             (LEAF_IMAGE_SIZE * LEAF_IMAGE_SIZE * 3)

#endif

