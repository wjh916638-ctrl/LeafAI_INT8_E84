#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <lv_rt_thread_conf.h>
#include "vg_lite.h"
#include "vg_lite_platform.h"
#include "lv_port_disp.h"
#include "leaf_ai/leaf_ui.h"

static void configure_console_baud(void)
{
    rt_device_t console = rt_console_get_device();
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;

    if (console != RT_NULL)
    {
        /* Keep compatibility with the existing host-side image uploader. */
        config.baud_rate = 1000000;
        rt_device_control(console, RT_DEVICE_CTRL_CONFIG, &config);
    }
}

void lv_user_gui_init(void)
{
    leaf_ui_create();
}

int main(void)
{
    /* Keep the LVGL worker alive while UART commands run local inference. */
    rt_kprintf("Apple leaf monitor starting\r\n");
    configure_console_baud();
    lvgl_thread_init();

    while (1)
    {
        rt_thread_mdelay(1000);
    }
    return 0;
}
