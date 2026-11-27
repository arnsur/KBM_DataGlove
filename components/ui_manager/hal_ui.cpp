#include "driver/spi_master.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/ledc.h"
#include <lvgl.h>

#include "board_config.hpp"
#include "hal_ui.hpp"


namespace HalUI
{
    static esp_lcd_panel_io_handle_t io_handle = nullptr;
    static lv_disp_drv_t disp_drv;
    static lv_disp_draw_buf_t disp_buf;
    static esp_lcd_panel_handle_t panel_handle = nullptr;
    static lv_color_t buf[SCREEN_WIDTH * 20];
    static uint32_t BL_MIN_BRIGHTNESS = 600;

    static bool on_color_trans_done(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
        lv_disp_drv_t *driver = (lv_disp_drv_t *)user_ctx;
        lv_disp_flush_ready(driver);
        return false;
    }

    static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
        esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_p);
    }

    void init()
    {
        // SPI bus config
        spi_bus_config_t bus_cfg = {};
        bus_cfg.mosi_io_num = BoardConfig::TFT_MOSI;
        bus_cfg.miso_io_num = -1;
        bus_cfg.sclk_io_num = BoardConfig::TFT_CLK;
        bus_cfg.quadwp_io_num = -1;
        bus_cfg.quadhd_io_num = -1;
        bus_cfg.max_transfer_sz = SCREEN_WIDTH * 20 * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

        // Panel io config
        esp_lcd_panel_io_spi_config_t io_cfg = {};
        io_cfg.dc_gpio_num = BoardConfig::TFT_DC;
        io_cfg.cs_gpio_num = BoardConfig::TFT_CS;
        io_cfg.pclk_hz = 15'000'000;
        io_cfg.trans_queue_depth = 10;
        io_cfg.on_color_trans_done = on_color_trans_done;
        io_cfg.user_ctx = &disp_drv;
        io_cfg.lcd_cmd_bits = 8;
        io_cfg.lcd_param_bits = 8;
        io_cfg.cs_ena_pretrans = 0;
        io_cfg.cs_ena_posttrans = 0;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &io_handle));

        // Device config
        esp_lcd_panel_dev_config_t dev_cfg = {};
        dev_cfg.reset_gpio_num = BoardConfig::TFT_RST;
        dev_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        dev_cfg.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &dev_cfg, &panel_handle));

        esp_lcd_panel_reset(panel_handle);
        esp_lcd_panel_init(panel_handle);
        esp_lcd_panel_invert_color(panel_handle, true);
        esp_lcd_panel_disp_on_off(panel_handle, true);

        // Landscape
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, false);

        // Backlight timer config
        ledc_timer_config_t bl_timer_cfg = {};
        bl_timer_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
        bl_timer_cfg.duty_resolution = LEDC_TIMER_12_BIT;
        bl_timer_cfg.timer_num = LEDC_TIMER_0;
        bl_timer_cfg.freq_hz = 5000;
        ESP_ERROR_CHECK(ledc_timer_config(&bl_timer_cfg));
        
        // Backlight channel config
        ledc_channel_config_t bl_channel_cfg = {};
        bl_channel_cfg.gpio_num = BoardConfig::TFT_BL;
        bl_channel_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
        bl_channel_cfg.channel = LEDC_CHANNEL_0;
        bl_channel_cfg.timer_sel = LEDC_TIMER_0;
        ESP_ERROR_CHECK(ledc_channel_config(&bl_channel_cfg));

        set_brightness_pct(50);

        lv_init();
        lv_disp_draw_buf_init(&disp_buf, buf, NULL, SCREEN_WIDTH * 20);

        lv_disp_drv_init(&disp_drv);
        disp_drv.hor_res = SCREEN_WIDTH;
        disp_drv.ver_res = SCREEN_HEIGHT;
        disp_drv.flush_cb = disp_flush;
        disp_drv.draw_buf = &disp_buf;
        lv_disp_drv_register(&disp_drv);
    }

    void set_brightness_pct(uint8_t pct)
    {
        if (pct > 100) pct = 100;
        uint32_t duty = (((4095 - BL_MIN_BRIGHTNESS) * pct) / 100) + BL_MIN_BRIGHTNESS;
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
    }
}