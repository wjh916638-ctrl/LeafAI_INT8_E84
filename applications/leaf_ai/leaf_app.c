#include <board.h>
#include <math.h>
#include <rtdevice.h>
#include <rtthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aht10.h"
#include "leaf_app.h"
#include "leaf_app_config.h"
#include "leaf_protocol.h"
#include "leaf_ui.h"
#include "model.h"

#define LEAF_ENV_THREAD_STACK       4096

#if defined(__GNUC__)
#define LEAF_SHARED_MEM             __attribute__((section(".m55_ml_buffers"), aligned(16)))
#else
#define LEAF_SHARED_MEM
#endif

static uint8_t g_rgb[LEAF_IMAGE_BYTES];
static float g_model_input[LEAF_MODEL_FLOAT_VALUES] LEAF_SHARED_MEM;

static struct rt_mutex g_snapshot_lock;
static leaf_app_snapshot_t g_snapshot;

static const char *const g_class_names[] =
{
    "healthy",
    "multiple_diseases",
    "rust",
    "scab",
};

static const float g_mean[3] = {0.485f, 0.456f, 0.406f};
static const float g_std[3] = {0.229f, 0.224f, 0.225f};

static uint16_t get_u16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t get_u32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

uint16_t leaf_get_u16_le(const uint8_t *data)
{
    return get_u16(data);
}

uint32_t leaf_get_u32_le(const uint8_t *data)
{
    return get_u32(data);
}

static void put_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

uint32_t leaf_crc32(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t index;
    uint32_t bit;

    for (index = 0; index < length; index++)
    {
        crc ^= data[index];
        for (bit = 0; bit < 8; bit++)
        {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

static void snapshot_copy(leaf_app_snapshot_t *destination)
{
    rt_mutex_take(&g_snapshot_lock, RT_WAITING_FOREVER);
    *destination = g_snapshot;
    rt_mutex_release(&g_snapshot_lock);
}

void leaf_app_get_snapshot(leaf_app_snapshot_t *snapshot)
{
    if (snapshot != RT_NULL)
    {
        snapshot_copy(snapshot);
    }
}

const char *leaf_app_class_name(uint8_t class_id)
{
    if (class_id < (sizeof(g_class_names) / sizeof(g_class_names[0])))
    {
        return g_class_names[class_id];
    }
    return "unknown";
}

static void update_leds(const leaf_app_snapshot_t *snapshot)
{
    rt_base_t red_level = LEAF_LED_OFF_LEVEL;
    rt_base_t green_level = LEAF_LED_OFF_LEVEL;
    rt_base_t blue_level = LEAF_LED_OFF_LEVEL;

    if (!snapshot->sensor_valid)
    {
        blue_level = LEAF_LED_ON_LEVEL;
    }
    else if (snapshot->environment_alarm || snapshot->disease_alarm)
    {
        red_level = LEAF_LED_ON_LEVEL;
    }
    else
    {
        green_level = LEAF_LED_ON_LEVEL;
    }

    rt_pin_write(LEAF_LED_RED_PIN, red_level);
    rt_pin_write(LEAF_LED_GREEN_PIN, green_level);
    rt_pin_write(LEAF_LED_BLUE_PIN, blue_level);
}

static void update_environment(float temperature_c, float humidity_pct, bool valid)
{
    leaf_app_snapshot_t copy;

    rt_mutex_take(&g_snapshot_lock, RT_WAITING_FOREVER);
    g_snapshot.sensor_valid = valid;
    g_snapshot.temperature_c = temperature_c;
    g_snapshot.humidity_pct = humidity_pct;
    g_snapshot.environment_alarm = valid &&
        ((temperature_c > LEAF_TEMP_HIGH_C) ||
         (humidity_pct > LEAF_HUMIDITY_HIGH_PCT));
    copy = g_snapshot;
    rt_mutex_release(&g_snapshot_lock);

    update_leds(&copy);
}

static void update_result(uint8_t class_id, float confidence, uint32_t inference_ms)
{
    leaf_app_snapshot_t copy;

    rt_mutex_take(&g_snapshot_lock, RT_WAITING_FOREVER);
    g_snapshot.result_valid = true;
    g_snapshot.class_id = class_id;
    g_snapshot.confidence = confidence;
    g_snapshot.inference_ms = inference_ms;
    g_snapshot.disease_alarm = (class_id != 0) &&
        (confidence >= LEAF_DISEASE_CONFIDENCE);
    copy = g_snapshot;
    rt_mutex_release(&g_snapshot_lock);

    update_leds(&copy);
}

static int wait_for_magic(rt_device_t device, uint32_t timeout_ms)
{
    uint8_t window[4] = {0};
    uint8_t byte;
    uint8_t samples[4] = {0};
    uint32_t received = 0;
    rt_tick_t deadline = rt_tick_get() + rt_tick_from_millisecond(timeout_ms);

    while (1)
    {
        rt_ssize_t read_size = rt_device_read(device, 0, &byte, 1);
        if (read_size == 1)
        {
            if (received < sizeof(samples))
            {
                samples[received] = byte;
            }
            received++;
            window[0] = window[1];
            window[1] = window[2];
            window[2] = window[3];
            window[3] = byte;
            if ((window[0] == LEAF_MAGIC_0) &&
                (window[1] == LEAF_MAGIC_1) &&
                (window[2] == LEAF_MAGIC_2) &&
                (window[3] == LEAF_MAGIC_3))
            {
                rt_kprintf("leaf: image magic found\r\n");
                return 0;
            }
        }
        else if ((rt_int32_t)(rt_tick_get() - deadline) >= 0)
        {
            rt_kprintf("leaf: header timeout bytes=%lu first=%02x,%02x,%02x,%02x\r\n",
                       (unsigned long)received,
                       samples[0], samples[1], samples[2], samples[3]);
            return -RT_ETIMEOUT;
        }
        else
        {
            rt_thread_mdelay(1);
        }
    }
}

static int read_exact(rt_device_t device, uint8_t *destination, uint32_t length)
{
    uint32_t received = 0;
    rt_tick_t deadline = rt_tick_get() + rt_tick_from_millisecond(LEAF_UART_READ_TIMEOUT_MS);

    while (received < length)
    {
        rt_ssize_t read_size = rt_device_read(device, 0, destination + received, length - received);
        if (read_size > 0)
        {
            received += (uint32_t)read_size;
        }
        else if ((rt_int32_t)(rt_tick_get() - deadline) >= 0)
        {
            rt_kprintf("leaf: read timeout received=%lu expected=%lu\r\n",
                       (unsigned long)received, (unsigned long)length);
            return -RT_ETIMEOUT;
        }
        else
        {
            rt_thread_mdelay(1);
        }
    }
    return RT_EOK;
}

static int write_all(rt_device_t device, const uint8_t *source, uint32_t length)
{
    uint32_t written = 0;
    while (written < length)
    {
        rt_ssize_t write_size = rt_device_write(device, 0, source + written, length - written);
        if (write_size <= 0)
        {
            return -RT_EIO;
        }
        written += (uint32_t)write_size;
    }
    return RT_EOK;
}

static int send_result(rt_device_t device, uint32_t sequence, const char *json)
{
    uint8_t header[LEAF_HEADER_SIZE];
    uint32_t payload_length = (uint32_t)strlen(json);
    uint32_t copy;
    int result;

    header[0] = LEAF_MAGIC_0;
    header[1] = LEAF_MAGIC_1;
    header[2] = LEAF_MAGIC_2;
    header[3] = LEAF_MAGIC_3;
    header[4] = LEAF_PROTOCOL_VERSION;
    header[5] = LEAF_TYPE_RESULT_JSON;
    header[6] = 0;
    header[7] = 0;
    put_u16(&header[8], 0);
    put_u16(&header[10], 0);
    put_u32(&header[12], sequence);
    put_u32(&header[16], payload_length);
    put_u32(&header[20], leaf_crc32((const uint8_t *)json, payload_length));

    for (copy = 0; copy < 2u; copy++)
    {
        result = write_all(device, header, sizeof(header));
        if (result == RT_EOK)
        {
            result = write_all(device, (const uint8_t *)json, payload_length);
        }
        if (result != RT_EOK)
        {
            return result;
        }
    }
    return result;
}

static void rgb_to_model_input(void)
{
    uint32_t channel;
    uint32_t y;
    uint32_t x;

    for (channel = 0; channel < 3; channel++)
    {
        for (y = 0; y < LEAF_MODEL_INPUT_SIZE; y++)
        {
            for (x = 0; x < LEAF_MODEL_INPUT_SIZE; x++)
            {
                float source_x =
                    (float)x * (LEAF_IMAGE_SIZE - 1u) /
                    (LEAF_MODEL_INPUT_SIZE - 1u);
                float source_y =
                    (float)y * (LEAF_IMAGE_SIZE - 1u) /
                    (LEAF_MODEL_INPUT_SIZE - 1u);
                uint32_t x0 = (uint32_t)source_x;
                uint32_t y0 = (uint32_t)source_y;
                uint32_t x1 = (x0 + 1u < LEAF_IMAGE_SIZE) ? x0 + 1u : x0;
                uint32_t y1 = (y0 + 1u < LEAF_IMAGE_SIZE) ? y0 + 1u : y0;
                float fx = source_x - (float)x0;
                float fy = source_y - (float)y0;
                uint32_t top_left =
                    (y0 * LEAF_IMAGE_SIZE + x0) * 3u + channel;
                uint32_t top_right =
                    (y0 * LEAF_IMAGE_SIZE + x1) * 3u + channel;
                uint32_t bottom_left =
                    (y1 * LEAF_IMAGE_SIZE + x0) * 3u + channel;
                uint32_t bottom_right =
                    (y1 * LEAF_IMAGE_SIZE + x1) * 3u + channel;
                float top = (float)g_rgb[top_left] +
                    ((float)g_rgb[top_right] - (float)g_rgb[top_left]) * fx;
                float bottom = (float)g_rgb[bottom_left] +
                    ((float)g_rgb[bottom_right] - (float)g_rgb[bottom_left]) * fx;
                float pixel_rgb = (top + (bottom - top) * fy) / 255.0f;
                uint32_t model_index =
                    (y * LEAF_MODEL_INPUT_SIZE + x) * 3u + channel;
                g_model_input[model_index] =
                    (pixel_rgb - g_mean[channel]) / g_std[channel];
            }
        }
    }
}

static void softmax4(const float *logits, float *probabilities)
{
    float maximum = logits[0];
    float sum = 0.0f;
    uint32_t index;

    for (index = 1; index < 4; index++)
    {
        if (logits[index] > maximum)
        {
            maximum = logits[index];
        }
    }
    for (index = 0; index < 4; index++)
    {
        probabilities[index] = expf(logits[index] - maximum);
        sum += probabilities[index];
    }
    for (index = 0; index < 4; index++)
    {
        probabilities[index] /= sum;
    }
}

static void send_error(rt_device_t device, uint32_t sequence, const char *message)
{
    char json[160];
    rt_snprintf(json, sizeof(json),
                "{\"status\":\"error\",\"seq\":%lu,\"message\":\"%s\"}",
                (unsigned long)sequence, message);
    (void)send_result(device, sequence, json);
}

static void infer_and_reply(rt_device_t device, uint32_t sequence)
{
    float logits[4] = {0};
    float probabilities[4] = {0};
    uint8_t class_id = 0;
    uint32_t index;
    rt_tick_t started;
    uint32_t inference_ms;
    char json[384];
    leaf_app_snapshot_t snapshot;
    int confidence_milli;
    int temperature_centi;
    int humidity_centi;
    int compute_result;

    rt_kprintf("leaf: preprocessing seq=%lu\r\n", (unsigned long)sequence);
    rgb_to_model_input();
    started = rt_tick_get();
    rt_kprintf("leaf: IMAI_compute begin\r\n");
    compute_result = IMAI_compute(g_model_input, logits);
    inference_ms = (uint32_t)(((uint64_t)(rt_tick_get() - started) * 1000u +
        RT_TICK_PER_SECOND - 1u) / RT_TICK_PER_SECOND);
    rt_kprintf("leaf: IMAI_compute end rc=%d elapsed_ms=%lu\r\n",
               compute_result, (unsigned long)inference_ms);
    if (compute_result != 0)
    {
        send_error(device, sequence, "IMAI_compute_failed");
        return;
    }

    softmax4(logits, probabilities);
    for (index = 1; index < 4; index++)
    {
        if (probabilities[index] > probabilities[class_id])
        {
            class_id = (uint8_t)index;
        }
    }

    update_result(class_id, probabilities[class_id], inference_ms);
    snapshot_copy(&snapshot);

    confidence_milli = (int)lroundf(probabilities[class_id] * 1000.0f);
    temperature_centi = snapshot.sensor_valid ?
        (int)lroundf(snapshot.temperature_c * 100.0f) : -1;
    humidity_centi = snapshot.sensor_valid ?
        (int)lroundf(snapshot.humidity_pct * 100.0f) : -1;

    rt_snprintf(json, sizeof(json),
        "{\"status\":\"ok\",\"seq\":%lu,\"class_id\":%u,\"class_name\":\"%s\","
        "\"confidence_milli\":%d,\"probabilities_milli\":[%d,%d,%d,%d],"
        "\"inference_ms\":%lu,\"temperature_centi\":%d,\"humidity_centi\":%d,"
        "\"environment_alarm\":%s}",
        (unsigned long)sequence,
        (unsigned)class_id,
        leaf_app_class_name(class_id),
        confidence_milli,
        (int)lroundf(probabilities[0] * 1000.0f),
        (int)lroundf(probabilities[1] * 1000.0f),
        (int)lroundf(probabilities[2] * 1000.0f),
        (int)lroundf(probabilities[3] * 1000.0f),
        (unsigned long)inference_ms,
        temperature_centi,
        humidity_centi,
        snapshot.environment_alarm ? "true" : "false");

    if (send_result(device, sequence, json) != RT_EOK)
    {
        rt_kprintf("leaf: result send failed\n");
    }
}

static int leaf_stream(int argc, char **argv)
{
    static int model_initialized;
    static int model_init_result;
    rt_device_t device = rt_console_get_device();
    uint8_t header[20];
    uint8_t magic;
    uint32_t sequence;
    uint32_t payload_length;
    uint32_t expected_crc;
    int frame_count = (argc > 1) ? atoi(argv[1]) : 1;
    int frame_index;

    if (device == RT_NULL)
    {
        rt_kprintf("leaf: console device is unavailable\n");
        return -RT_ERROR;
    }
    if (frame_count < 1)
    {
        frame_count = 1;
    }

#if LEAF_MODEL_ENABLE
    if (!model_initialized)
    {
        rt_kprintf("leaf: initializing model\r\n");
        model_init_result = IMAI_init();
        model_initialized = 1;
    }
    if (model_init_result != 0)
    {
        rt_kprintf("leaf: IMAI_init failed (%d)\n", model_init_result);
        return -RT_ERROR;
    }
#endif

    rt_kprintf("LEAF_READY %d %lu\r\n", frame_count, (unsigned long)sizeof(g_rgb));
    for (frame_index = 0; frame_index < frame_count; frame_index++)
    {
        if (wait_for_magic(device, LEAF_UART_READ_TIMEOUT_MS) != 0)
        {
            rt_kprintf("leaf: image header timeout\n");
            return -RT_ETIMEOUT;
        }
        if (read_exact(device, &magic, 1) != RT_EOK)
        {
            return -RT_ETIMEOUT;
        }
        header[0] = magic;
        if (read_exact(device, &header[1], sizeof(header) - 1u) != RT_EOK)
        {
            return -RT_ETIMEOUT;
        }

        if (header[0] != LEAF_PROTOCOL_VERSION ||
            header[1] != LEAF_TYPE_IMAGE_RGB888 ||
            get_u16(&header[4]) != LEAF_IMAGE_SIZE ||
            get_u16(&header[6]) != LEAF_IMAGE_SIZE ||
            header[2] != 3 ||
            get_u32(&header[12]) != LEAF_IMAGE_BYTES)
        {
            send_error(device, get_u32(&header[8]), "bad_image_header");
            continue;
        }

        sequence = get_u32(&header[8]);
        payload_length = get_u32(&header[12]);
        expected_crc = get_u32(&header[16]);

        if (payload_length != LEAF_IMAGE_BYTES ||
            read_exact(device, g_rgb, sizeof(g_rgb)) != RT_EOK)
        {
            send_error(device, sequence, "image_receive_timeout");
            continue;
        }

        if (leaf_crc32(g_rgb, sizeof(g_rgb)) != expected_crc)
        {
            send_error(device, sequence, "crc_mismatch");
            continue;
        }

#if LEAF_MODEL_ENABLE
        infer_and_reply(device, sequence);
#else
        send_error(device, sequence, "model_disabled");
#endif
    }
    rt_kprintf("LEAF_DONE\r\n");
    return RT_EOK;
}
MSH_CMD_EXPORT(leaf_stream, receive RGB image and run local leaf model);

static int leaf_status(int argc, char **argv)
{
    leaf_app_snapshot_t snapshot;
    (void)argc;
    (void)argv;

    snapshot_copy(&snapshot);
    rt_kprintf("LEAF_STATUS sensor_valid=%d temp_centi=%d humidity_centi=%d "
               "result_valid=%d class_id=%u class=%s confidence_milli=%d "
               "inference_ms=%lu environment_alarm=%d disease_alarm=%d\r\n",
               snapshot.sensor_valid,
               snapshot.sensor_valid ? (int)lroundf(snapshot.temperature_c * 100.0f) : -1,
               snapshot.sensor_valid ? (int)lroundf(snapshot.humidity_pct * 100.0f) : -1,
               snapshot.result_valid,
               (unsigned)snapshot.class_id,
               leaf_app_class_name(snapshot.class_id),
               snapshot.result_valid ?
                   (int)lroundf(snapshot.confidence * 1000.0f) : -1,
               (unsigned long)snapshot.inference_ms,
               snapshot.environment_alarm,
               snapshot.disease_alarm);
    return RT_EOK;
}
MSH_CMD_EXPORT(leaf_status, show environmental and leaf classification status);

static void environment_thread_entry(void *parameter)
{
    aht10_device_t sensor = RT_NULL;
    (void)parameter;

    while (1)
    {
        if (sensor == RT_NULL)
        {
            sensor = aht10_init(LEAF_I2C_BUS_NAME);
        }

        if (sensor != RT_NULL)
        {
            float temperature = aht10_read_temperature(sensor);
            float humidity = aht10_read_humidity(sensor);
            bool valid = isfinite(temperature) && isfinite(humidity) &&
                temperature > -50.0f && temperature < 100.0f &&
                humidity >= 0.0f && humidity <= 100.0f;
            update_environment(temperature, humidity, valid);
        }
        else
        {
            update_environment(0.0f, 0.0f, false);
        }

        rt_thread_mdelay(sensor == RT_NULL ?
            5000 : LEAF_ENVIRONMENT_PERIOD_MS);
    }
}

static int leaf_app_init(void)
{
    rt_thread_t thread;

    rt_memset(&g_snapshot, 0, sizeof(g_snapshot));
    rt_mutex_init(&g_snapshot_lock, "leaf_lock", RT_IPC_FLAG_PRIO);

    rt_pin_mode(LEAF_LED_RED_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(LEAF_LED_GREEN_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(LEAF_LED_BLUE_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(LEAF_LED_RED_PIN, LEAF_LED_OFF_LEVEL);
    rt_pin_write(LEAF_LED_GREEN_PIN, LEAF_LED_OFF_LEVEL);
    rt_pin_write(LEAF_LED_BLUE_PIN, LEAF_LED_ON_LEVEL);

    thread = rt_thread_create("leaf_env", environment_thread_entry, RT_NULL,
        LEAF_ENV_THREAD_STACK, 20, 10);
    if (thread == RT_NULL)
    {
        return -RT_ENOMEM;
    }
    rt_thread_startup(thread);

    return RT_EOK;
}
INIT_APP_EXPORT(leaf_app_init);

