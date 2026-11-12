#include <lvgl.h>
#include <stdio.h>

#include "leaf_app.h"
#include "leaf_app_config.h"
#include "leaf_ui.h"

static lv_obj_t *g_temperature_value;
static lv_obj_t *g_humidity_value;
static lv_obj_t *g_result_value;
static lv_obj_t *g_result_detail;
static lv_obj_t *g_footer;
static bool g_ready;

static void leaf_ui_refresh_timer_cb(lv_timer_t *timer)
{
    leaf_app_snapshot_t snapshot;

    (void)timer;
    leaf_app_get_snapshot(&snapshot);
    leaf_ui_refresh(&snapshot);
}

static const lv_font_t *font_title(void)
{
#if LV_FONT_MONTSERRAT_28
    return &lv_font_montserrat_28;
#else
    return LV_FONT_DEFAULT;
#endif
}

static const lv_font_t *font_value(void)
{
#if LV_FONT_MONTSERRAT_48
    return &lv_font_montserrat_48;
#elif LV_FONT_MONTSERRAT_28
    return &lv_font_montserrat_28;
#else
    return LV_FONT_DEFAULT;
#endif
}

static lv_obj_t *create_card(lv_obj_t *parent, lv_coord_t width)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, width, lv_pct(100));
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xE2E8EE), 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return card;
}

static lv_obj_t *create_label(lv_obj_t *parent, const char *text,
    const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

void leaf_ui_create(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_t *root;
    lv_obj_t *row;
    lv_obj_t *temperature_card;
    lv_obj_t *humidity_card;
    lv_obj_t *result_card;
    leaf_app_snapshot_t snapshot;

    lv_obj_set_style_bg_color(screen, lv_color_hex(0xF3F6F8), 0);

    root = lv_obj_create(screen);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, lv_pct(94), lv_pct(92));
    lv_obj_center(root);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(root, 10, 0);

    create_label(root, "Apple Leaf Monitor", font_title(), 0x16212B);

    row = lv_obj_create(root);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, lv_pct(100), lv_pct(28));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 10, 0);

    temperature_card = create_card(row, lv_pct(49));
    create_label(temperature_card, "TEMPERATURE", LV_FONT_DEFAULT, 0x64717D);
    g_temperature_value = create_label(temperature_card, "--", font_value(), 0x16212B);

    humidity_card = create_card(row, lv_pct(49));
    create_label(humidity_card, "HUMIDITY", LV_FONT_DEFAULT, 0x64717D);
    g_humidity_value = create_label(humidity_card, "--", font_value(), 0x16212B);

    result_card = create_card(root, lv_pct(100));
    lv_obj_set_flex_grow(result_card, 1);
    lv_obj_set_style_bg_color(result_card, lv_color_hex(0xFFFFFF), 0);
    create_label(result_card, "LEAF RESULT", LV_FONT_DEFAULT, 0x64717D);
    g_result_value = create_label(result_card, "WAITING", font_title(), 0x246B9A);
    g_result_detail = create_label(result_card, "Capture from the PC camera", LV_FONT_DEFAULT, 0x64717D);

    g_footer = create_label(root,
        "PC camera -> UART -> E84 local inference", LV_FONT_DEFAULT, 0x64717D);
    lv_obj_set_width(g_footer, lv_pct(100));
    lv_obj_set_style_text_align(g_footer, LV_TEXT_ALIGN_CENTER, 0);

    g_ready = true;
    leaf_app_get_snapshot(&snapshot);
    leaf_ui_refresh(&snapshot);
    lv_timer_create(leaf_ui_refresh_timer_cb, 250, NULL);
}

void leaf_ui_refresh(const leaf_app_snapshot_t *snapshot)
{
    if (!g_ready || snapshot == NULL)
    {
        return;
    }

    if (snapshot->sensor_valid)
    {
        int temperature_tenths =
            (int)(snapshot->temperature_c * 10.0f + 0.5f);
        int humidity_tenths =
            (int)(snapshot->humidity_pct * 10.0f + 0.5f);
        lv_label_set_text_fmt(g_temperature_value, "%d.%d C",
            temperature_tenths / 10, temperature_tenths % 10);
        lv_label_set_text_fmt(g_humidity_value, "%d.%d %%",
            humidity_tenths / 10, humidity_tenths % 10);
    }
    else
    {
        lv_label_set_text(g_temperature_value, "--");
        lv_label_set_text(g_humidity_value, "--");
    }

    if (snapshot->result_valid)
    {
        const char *name = leaf_app_class_name(snapshot->class_id);
        unsigned confidence_tenths = (unsigned)(snapshot->confidence * 1000.0f + 0.5f);
        lv_obj_set_style_text_color(g_result_value,
            lv_color_hex(snapshot->disease_alarm ? 0xB42318 : 0x1E7A46), 0);
        lv_label_set_text(g_result_value, name);
        lv_label_set_text_fmt(g_result_detail,
            "%u.%u%% confidence | %lu ms on board%s",
            confidence_tenths / 10u,
            confidence_tenths % 10u,
            (unsigned long)snapshot->inference_ms,
            snapshot->confidence < LEAF_DISEASE_CONFIDENCE &&
                snapshot->class_id != 0 ? " | LOW CONFIDENCE" : "");
    }

    if (!snapshot->sensor_valid)
    {
        lv_label_set_text(g_footer, "AHT20 unavailable | check i2c1 and package");
        lv_obj_set_style_text_color(g_footer, lv_color_hex(0xB42318), 0);
    }
    else if (snapshot->environment_alarm)
    {
        lv_label_set_text(g_footer, "Environment attention | not a disease diagnosis");
        lv_obj_set_style_text_color(g_footer, lv_color_hex(0xB42318), 0);
    }
    else
    {
        lv_label_set_text(g_footer,
            "PC camera -> UART -> E84 local inference");
        lv_obj_set_style_text_color(g_footer, lv_color_hex(0x64717D), 0);
    }

}

