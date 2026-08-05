#include "hal_display.hpp"
#include "board_config.hpp"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <lvgl.h>
#include "ui/ui.h"
#include <stdio.h>

namespace HalDisplay
{
    const uint16_t SCREEN_WIDTH = 320;
    const uint16_t SCREEN_HEIGHT = 240;

    static esp_lcd_panel_handle_t panel_handle = NULL;
    static esp_lcd_panel_io_handle_t io_handle = NULL;
    static lv_disp_draw_buf_t disp_buf;
    static lv_color_t buf[SCREEN_WIDTH * 20];

    static lv_disp_drv_t disp_drv;

    static int16_t local_cursor_x = 160;
    static int16_t local_cursor_y = 120;
    static bool local_click = false;
    static int local_input_mode = 1;

    static bool on_color_trans_done(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
        lv_disp_drv_t *driver = (lv_disp_drv_t *)user_ctx;
        lv_disp_flush_ready(driver);
        return false;
    }

    // --- LVGL Callbacks ---
    static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
        // Account for Endianness and swap the high and low color bytes
        uint32_t size = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);
        for (uint32_t i = 0; i < size; i++) {
            color_p[i].full = (color_p[i].full >> 8) | (color_p[i].full << 8);
        }

        esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_p);
    }

    static void indev_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
        data->point.x = local_cursor_x;
        data->point.y = local_cursor_y;
        data->state = (local_click && local_input_mode == 1) ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL; 
    }

    void set_brightness(uint32_t brightness) // Brightness range: 0 - 4096
    {
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, brightness));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
    }

    void init()
    {
        spi_bus_config_t spi_bus_config = {};
        spi_bus_config.mosi_io_num = TFT_DIN;
        spi_bus_config.miso_io_num = -1;
        spi_bus_config.sclk_io_num = TFT_CLK;
        spi_bus_config.quadwp_io_num = -1;
        spi_bus_config.quadhd_io_num = -1;
        spi_bus_config.max_transfer_sz = SCREEN_WIDTH * 20 * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &spi_bus_config, SPI_DMA_CH_AUTO));

        esp_lcd_panel_io_spi_config_t lcd_io_config = {};
        lcd_io_config.cs_gpio_num = TFT_CS;
        lcd_io_config.dc_gpio_num = TFT_DC;
        lcd_io_config.spi_mode = 0;
        lcd_io_config.pclk_hz = 26 * 1000 * 1000;// 26 MHz because of pin matrix
        lcd_io_config.trans_queue_depth = 10,
        lcd_io_config.on_color_trans_done = on_color_trans_done;
        lcd_io_config.user_ctx = &disp_drv;
        lcd_io_config.lcd_cmd_bits = 8;
        lcd_io_config.lcd_param_bits = 8;
        lcd_io_config.cs_ena_pretrans = 0;
        lcd_io_config.cs_ena_posttrans = 0;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &lcd_io_config, &io_handle));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = TFT_RST;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
        
        esp_lcd_panel_reset(panel_handle);
        esp_lcd_panel_init(panel_handle);
        esp_lcd_panel_invert_color(panel_handle, true);

        esp_lcd_panel_disp_on_off(panel_handle, true);
        
        // Landscape
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, false);


        ledc_timer_config_t disp_bltimer_config = {};
        disp_bltimer_config.speed_mode = LEDC_LOW_SPEED_MODE;
        disp_bltimer_config.timer_num = LEDC_TIMER_0;
        disp_bltimer_config.freq_hz = 5000;
        disp_bltimer_config.duty_resolution = LEDC_TIMER_12_BIT;
        disp_bltimer_config.clk_cfg = LEDC_AUTO_CLK;
        ESP_ERROR_CHECK(ledc_timer_config(&disp_bltimer_config));

        ledc_channel_config_t disp_bl_channel_config = {};
        disp_bl_channel_config.gpio_num = TFT_BL;
        disp_bl_channel_config.speed_mode = LEDC_LOW_SPEED_MODE;
        disp_bl_channel_config.channel = LEDC_CHANNEL_0;
        disp_bl_channel_config.timer_sel = LEDC_TIMER_0;
        ESP_ERROR_CHECK(ledc_channel_config(&disp_bl_channel_config));

        set_brightness(2048); // 50% brightness on startup

        lv_init();
        lv_disp_draw_buf_init(&disp_buf, buf, NULL, SCREEN_WIDTH * 20);

        lv_disp_drv_init(&disp_drv);
        disp_drv.hor_res = SCREEN_WIDTH;
        disp_drv.ver_res = SCREEN_HEIGHT;
        disp_drv.flush_cb = disp_flush;
        disp_drv.draw_buf = &disp_buf;
        lv_disp_drv_register(&disp_drv);

        static lv_indev_drv_t indev_drv;
        lv_indev_drv_init(&indev_drv);
        indev_drv.type = LV_INDEV_TYPE_POINTER;
        indev_drv.read_cb = indev_read;
        lv_indev_t *mouse_indev = lv_indev_drv_register(&indev_drv);

        lv_obj_t *crosshair_obj = lv_label_create(lv_layer_sys()); 
        lv_label_set_text(crosshair_obj, LV_SYMBOL_PLUS); 
        lv_obj_set_style_text_color(crosshair_obj, lv_color_hex(0xFF0000), LV_PART_MAIN); 
        lv_indev_set_cursor(mouse_indev, crosshair_obj);

        ui_init();
        lv_scr_load_anim(ui_HomeScreen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    }

    void update_display(UIState &currentUIState)
    {
        static uint32_t last_tick = 0;
        uint32_t current_tick = esp_timer_get_time() / 1000;
        lv_tick_inc(current_tick - last_tick);
        last_tick = current_tick;

        local_cursor_x = currentUIState.uiCursorX;
        local_cursor_y = currentUIState.uiCursorY;
        local_click = currentUIState.hasClicked;
        local_input_mode = currentUIState.inputMode;

        //----------------------------------MODE LABELS----------------------------------------------
        if (currentUIState.inputMode == 0) {
            lv_label_set_text_fmt(ui_HomeModeLabel, "%d / %d", currentUIState.inputMode, currentUIState.mouseMode);
            lv_label_set_text_fmt(ui_SettingsModeLabel, "%d / %d", currentUIState.inputMode, currentUIState.mouseMode);
            lv_label_set_text_fmt(ui_DevMiniModeLabelR, "%d / %d", currentUIState.inputMode, currentUIState.mouseMode);
            lv_label_set_text_fmt(ui_DevMiniModeLabelL, "%d / %d", currentUIState.inputMode, currentUIState.mouseMode);
        } else {
            lv_label_set_text_fmt(ui_HomeModeLabel, "%d", currentUIState.inputMode);
            lv_label_set_text_fmt(ui_SettingsModeLabel, "%d", currentUIState.inputMode);
            lv_label_set_text_fmt(ui_DevMiniModeLabelR, "%d", currentUIState.inputMode);
            lv_label_set_text_fmt(ui_DevMiniModeLabelL, "%d", currentUIState.inputMode);
        }

        //------------------------------------BATTERY LABELS---------------------------------------------
        int battery_r = currentUIState.batteryPct;
        int battery_l = 33; // Placeholder

        lv_label_set_text_fmt(ui_HomeBatteryLevelLabelL, "%d%%", battery_l);
        lv_label_set_text_fmt(ui_HomeBatteryLevelLabelR, "%d%%", battery_r);

        lv_label_set_text_fmt(ui_SettingsBatteryLevelLabelL, "%d%%", battery_l);
        lv_label_set_text_fmt(ui_SettingsBatteryLevelLabelR, "%d%%", battery_r);

        lv_label_set_text_fmt(ui_RDevMiniBatteryLevelLabelL, "%d%%", battery_l);
        lv_label_set_text_fmt(ui_RDevMiniBatteryLevelLabelR, "%d%%", battery_r);

        lv_label_set_text_fmt(ui_LDevMiniBatteryLevelLabelL, "%d%%", battery_l);
        lv_label_set_text_fmt(ui_LDevMiniBatteryLevelLabelR, "%d%%", battery_r);

        if (battery_l < 25) {
            lv_obj_set_style_text_color(ui_HomeBatteryLevelLabelL, lv_color_hex(0xFF0000), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_SettingsBatteryLevelLabelL, lv_color_hex(0xFF0000), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_RDevMiniBatteryLevelLabelL, lv_color_hex(0xFF0000), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_LDevMiniBatteryLevelLabelL, lv_color_hex(0xFF0000), LV_PART_MAIN);
        } else if (battery_l < 50) {
            lv_obj_set_style_text_color(ui_HomeBatteryLevelLabelL, lv_color_hex(0xE88300), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_SettingsBatteryLevelLabelL, lv_color_hex(0xE88300), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_RDevMiniBatteryLevelLabelL, lv_color_hex(0xE88300), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_LDevMiniBatteryLevelLabelL, lv_color_hex(0xE88300), LV_PART_MAIN);
            
        } else if (battery_l < 75) {
            lv_obj_set_style_text_color(ui_HomeBatteryLevelLabelL, lv_color_hex(0xE7D02D), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_SettingsBatteryLevelLabelL, lv_color_hex(0xE7D02D), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_RDevMiniBatteryLevelLabelL, lv_color_hex(0xE7D02D), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_LDevMiniBatteryLevelLabelL, lv_color_hex(0xE7D02D), LV_PART_MAIN);
        } else {
            lv_obj_set_style_text_color(ui_HomeBatteryLevelLabelL, lv_color_hex(0x31FF52), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_SettingsBatteryLevelLabelL, lv_color_hex(0x31FF52), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_RDevMiniBatteryLevelLabelL, lv_color_hex(0x31FF52), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_LDevMiniBatteryLevelLabelL, lv_color_hex(0x31FF52), LV_PART_MAIN);
        }

        if (battery_r < 25) {
            lv_obj_set_style_text_color(ui_HomeBatteryLevelLabelR, lv_color_hex(0xFF0000), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_SettingsBatteryLevelLabelR, lv_color_hex(0xFF0000), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_RDevMiniBatteryLevelLabelR, lv_color_hex(0xFF0000), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_LDevMiniBatteryLevelLabelR, lv_color_hex(0xFF0000), LV_PART_MAIN);
        } else if (battery_r < 50) {
            lv_obj_set_style_text_color(ui_HomeBatteryLevelLabelR, lv_color_hex(0xE88300), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_SettingsBatteryLevelLabelR, lv_color_hex(0xE88300), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_RDevMiniBatteryLevelLabelR, lv_color_hex(0xE88300), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_LDevMiniBatteryLevelLabelR, lv_color_hex(0xE88300), LV_PART_MAIN);
        } else if (battery_r < 75) {
            lv_obj_set_style_text_color(ui_HomeBatteryLevelLabelR, lv_color_hex(0xE7D02D), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_SettingsBatteryLevelLabelR, lv_color_hex(0xE7D02D), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_RDevMiniBatteryLevelLabelR, lv_color_hex(0xE7D02D), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_LDevMiniBatteryLevelLabelR, lv_color_hex(0xE7D02D), LV_PART_MAIN);
        } else {
            lv_obj_set_style_text_color(ui_HomeBatteryLevelLabelR, lv_color_hex(0x31FF52), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_SettingsBatteryLevelLabelR, lv_color_hex(0x31FF52), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_RDevMiniBatteryLevelLabelR, lv_color_hex(0x31FF52), LV_PART_MAIN);
            lv_obj_set_style_text_color(ui_LDevMiniBatteryLevelLabelR, lv_color_hex(0x31FF52), LV_PART_MAIN);
        }

        //------------------------------------PEER CONNECTION LABELS---------------------------------------------
        // Mini status bar does not get text changes so it stays condensed
        // if (rToRecvConnStatus == CONNECTED) {
        //     lv_label_set_text(ui_HomeRRecvConLabel, "R<>RECV:\nCONNECTED");
        //     lv_obj_set_style_text_color(ui_HomeRRecvConLabel, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);

        //     lv_label_set_text(ui_SettingsRRecvConLabel, "R<>RECV:\nCONNECTED");
        //     lv_obj_set_style_text_color(ui_SettingsRRecvConLabel, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);

        //     lv_obj_set_style_text_color(ui_DevRRecvLabelR, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);
        //     lv_obj_set_style_text_color(ui_DevRRecvLabelL, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);
        // } else if (rToRecvConnStatus == DISCONNECTED) {
        //     lv_label_set_text(ui_HomeRRecvConLabel, "R<>RECV:\nDISCONNECTED");
        //     lv_obj_set_style_text_color(ui_HomeRRecvConLabel, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);

        //     lv_label_set_text(ui_SettingsRRecvConLabel, "R<>RECV:\nDISCONNECTED");
        //     lv_obj_set_style_text_color(ui_SettingsRRecvConLabel, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);

        //     lv_obj_set_style_text_color(ui_DevRRecvLabelR, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
        //     lv_obj_set_style_text_color(ui_DevRRecvLabelL, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
        // } else if (rToRecvConnStatus == SEARCHING) {
        //     lv_label_set_text(ui_HomeRRecvConLabel, "R<>RECV:\nSEARCHING");
        //     lv_obj_set_style_text_color(ui_HomeRRecvConLabel, lv_color_hex(0xFFDD00), LV_PART_MAIN | LV_STATE_DEFAULT);

        //     lv_label_set_text(ui_SettingsRRecvConLabel, "R<>RECV:\nSEARCHING");
        //     lv_obj_set_style_text_color(ui_SettingsRRecvConLabel, lv_color_hex(0xFFDD00), LV_PART_MAIN | LV_STATE_DEFAULT);

        //     lv_obj_set_style_text_color(ui_DevRRecvLabelR, lv_color_hex(0xFFDD00), LV_PART_MAIN | LV_STATE_DEFAULT);
        //     lv_obj_set_style_text_color(ui_DevRRecvLabelL, lv_color_hex(0xFFDD00), LV_PART_MAIN | LV_STATE_DEFAULT);
        // }

        //     if (lToRConnStatus == CONNECTED) {
        //     lv_label_set_text(ui_HomeLRConLabel, "L<-R:\nCONNECTED");
        //     lv_obj_set_style_text_color(ui_HomeLRConLabel, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);

        //     lv_label_set_text(ui_SettingsLRConLabel, "L<-R:\nCONNECTED");
        //     lv_obj_set_style_text_color(ui_SettingsLRConLabel, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);

        //     lv_obj_set_style_text_color(ui_DevLRLabelR, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);
        //     lv_obj_set_style_text_color(ui_DevLRLabelL, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);
        // } else if (lToRConnStatus == DISCONNECTED) {
        //     lv_label_set_text(ui_HomeLRConLabel, "L<-R:\nDISCONNECTED");
        //     lv_obj_set_style_text_color(ui_HomeLRConLabel, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);

        //     lv_label_set_text(ui_SettingsLRConLabel, "L<-R:\nDISCONNECTED");
        //     lv_obj_set_style_text_color(ui_SettingsLRConLabel, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);

        //     lv_obj_set_style_text_color(ui_DevLRLabelR, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
        //     lv_obj_set_style_text_color(ui_DevLRLabelL, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
        // } else if (lToRConnStatus == SEARCHING) {
        //     lv_label_set_text(ui_HomeLRConLabel, "L<-R:\nSEARCHING");
        //     lv_obj_set_style_text_color(ui_HomeLRConLabel, lv_color_hex(0xFFDD00), LV_PART_MAIN | LV_STATE_DEFAULT);

        //     lv_label_set_text(ui_SettingsLRConLabel, "L<-R:\nSEARCHING");
        //     lv_obj_set_style_text_color(ui_SettingsLRConLabel, lv_color_hex(0xFFDD00), LV_PART_MAIN | LV_STATE_DEFAULT);

        //     lv_obj_set_style_text_color(ui_DevLRLabelR, lv_color_hex(0xFFDD00), LV_PART_MAIN | LV_STATE_DEFAULT);
        //     lv_obj_set_style_text_color(ui_DevLRLabelL, lv_color_hex(0xFFDD00), LV_PART_MAIN | LV_STATE_DEFAULT);
        // }

        //------------------------------------DEVELOPER SCREEN---------------------------------------------
        lv_label_set_text_fmt(ui_FSRReadingsLabelR, "%d (%d%%)\n%d (%d%%)\n%d (%d%%)\n%d (%d%%)\n%d (%d%%)",
            currentUIState.muxValues[0], 100 * currentUIState.muxValues[0]/4096,
            currentUIState.muxValues[1], 100 * currentUIState.muxValues[1]/4096,
            currentUIState.muxValues[2], 100 * currentUIState.muxValues[2]/4096,
            currentUIState.muxValues[3], 100 * currentUIState.muxValues[3]/4096,
            currentUIState.muxValues[4], 100 * currentUIState.muxValues[4]/4096);
        lv_bar_set_value(ui_FSR0BarR, currentUIState.muxValues[0], LV_ANIM_OFF);
        lv_bar_set_value(ui_FSR1BarR, currentUIState.muxValues[1], LV_ANIM_OFF);
        lv_bar_set_value(ui_FSR2BarR, currentUIState.muxValues[2], LV_ANIM_OFF);
        lv_bar_set_value(ui_FSR3BarR, currentUIState.muxValues[3], LV_ANIM_OFF);
        lv_bar_set_value(ui_FSR4BarR, currentUIState.muxValues[4], LV_ANIM_OFF);

        lv_label_set_text_fmt(ui_FlexReadingsLabelR, "%d (%d%%)\n%d (%d%%)\n%d (%d%%)\n%d (%d%%)\n%d (%d%%)\n%d (%d%%)\n%d (%d%%)",
            currentUIState.muxValues[5], 100 * currentUIState.muxValues[5]/4096,
            currentUIState.muxValues[6], 100 * currentUIState.muxValues[6]/4096,
            currentUIState.muxValues[7], 100 * currentUIState.muxValues[7]/4096,
            currentUIState.muxValues[8], 100 * currentUIState.muxValues[8]/4096,
            currentUIState.muxValues[9], 100 * currentUIState.muxValues[9]/4096,
            currentUIState.muxValues[10], 100 * currentUIState.muxValues[10]/4096,
            currentUIState.muxValues[11], 100 * currentUIState.muxValues[11]/4096);
        
        lv_bar_set_value(ui_Flex5BarR, currentUIState.muxValues[5], LV_ANIM_OFF);
        lv_bar_set_value(ui_Flex6BarR, currentUIState.muxValues[6], LV_ANIM_OFF);
        lv_bar_set_value(ui_Flex7BarR, currentUIState.muxValues[7], LV_ANIM_OFF);
        lv_bar_set_value(ui_Flex8BarR, currentUIState.muxValues[8], LV_ANIM_OFF);
        lv_bar_set_value(ui_Flex9BarR, currentUIState.muxValues[9], LV_ANIM_OFF);
        lv_bar_set_value(ui_Flex10BarR, currentUIState.muxValues[10], LV_ANIM_OFF);
        lv_bar_set_value(ui_Flex11BarR, currentUIState.muxValues[11], LV_ANIM_OFF);

        {
            char imu_buf[64];
            snprintf(imu_buf, sizeof(imu_buf), "Yaw: %.2f°\nRoll: %.2f°", currentUIState.yaw, currentUIState.roll);
            lv_label_set_text(ui_IMUYawRollLabelR, imu_buf);
        }

        // if (lastLeftTelemetryRecvTime != 0 && (millis() - lastLeftTelemetryRecvTime > 5000)) {
        //     lToRecvConnStatus = UNKNOWN;
        // }
        // if (newLeftTelemetryAvailable) {
        //     int leftSensorValues[11];
        //     memcpy(leftSensorValues, (void*) leftTelemetryData.sensorData, sizeof(leftTelemetryData.sensorData));
        //     lToRecvConnStatus = leftTelemetryData.connectionStatus;

        //     lv_label_set_text_fmt(ui_FSRReadingsLabelL, "%d (%d%%)",
        //         leftSensorValues[0], 100 * leftSensorValues[0]/4096);
        //     lv_bar_set_value(ui_FSR0BarL, leftSensorValues[0], LV_ANIM_OFF);

        //     newLeftTelemetryAvailable = false;
        // }

        // if (lToRecvConnStatus == CONNECTED) {
        //     lv_label_set_text(ui_HomeLRecvConLabel, "L<>RECV:\nCONNECTED");
        //     lv_obj_set_style_text_color(ui_HomeLRecvConLabel, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);

        //     lv_label_set_text(ui_SettingsLRecvConLabel, "L<>RECV:\nCONNECTED");
        //     lv_obj_set_style_text_color(ui_SettingsLRecvConLabel, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);

        //     lv_obj_set_style_text_color(ui_DevLRecvLabelR, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);
        //     lv_obj_set_style_text_color(ui_DevLRecvLabelL, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);
        // } else if (lToRecvConnStatus == DISCONNECTED) {
        //     lv_label_set_text(ui_HomeLRecvConLabel, "L<>RECV:\nDISCONNECTED");
        //     lv_obj_set_style_text_color(ui_HomeLRecvConLabel, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);

        //     lv_label_set_text(ui_SettingsLRecvConLabel, "L<>RECV:\nDISCONNECTED");
        //     lv_obj_set_style_text_color(ui_SettingsLRecvConLabel, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);

        //     lv_obj_set_style_text_color(ui_DevLRecvLabelR, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
        //     lv_obj_set_style_text_color(ui_DevLRecvLabelL, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
        // } else if (lToRecvConnStatus == SEARCHING) {
        //     lv_label_set_text(ui_HomeLRecvConLabel, "L<>RECV:\nSEARCHING");
        //     lv_obj_set_style_text_color(ui_HomeLRecvConLabel, lv_color_hex(0xFFDD00), LV_PART_MAIN | LV_STATE_DEFAULT);

        //     lv_label_set_text(ui_SettingsLRecvConLabel, "L<>RECV:\nSEARCHING");
        //     lv_obj_set_style_text_color(ui_SettingsLRecvConLabel, lv_color_hex(0xFFDD00), LV_PART_MAIN | LV_STATE_DEFAULT);

        //     lv_obj_set_style_text_color(ui_DevLRecvLabelR, lv_color_hex(0xFFDD00), LV_PART_MAIN | LV_STATE_DEFAULT);
        //     lv_obj_set_style_text_color(ui_DevLRecvLabelL, lv_color_hex(0xFFDD00), LV_PART_MAIN | LV_STATE_DEFAULT);
        // } else if (lToRecvConnStatus == UNKNOWN) {
        //     lv_label_set_text(ui_HomeLRecvConLabel, "L<>RECV:\nUNKNOWN");
        //     lv_obj_set_style_text_color(ui_HomeLRecvConLabel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);

        //     lv_label_set_text(ui_SettingsLRecvConLabel, "L<>RECV:\nUNKNOWN");
        //     lv_obj_set_style_text_color(ui_SettingsLRecvConLabel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);

        //     lv_obj_set_style_text_color(ui_DevLRecvLabelR, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        //     lv_obj_set_style_text_color(ui_DevLRecvLabelL, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        // }

        lv_timer_handler();
    }

    void sleep()
    {
        if (io_handle == NULL) return;
        
        printf("HalDisplay::sleep: disabling display and backlight\n");
        set_brightness(0);
        esp_lcd_panel_disp_on_off(panel_handle, false);

        uint8_t display_brightness = 0;
        ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io_handle, 0x51, &display_brightness, 1));
        vTaskDelay(pdMS_TO_TICKS(10));

        esp_lcd_panel_io_tx_param(io_handle, 0x10, NULL, 0);
        vTaskDelay(pdMS_TO_TICKS(10));

        ESP_ERROR_CHECK(ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0));
        gpio_set_direction(TFT_BL, GPIO_MODE_OUTPUT);
        gpio_set_level(TFT_BL, 0);
        gpio_hold_en(TFT_BL);
        printf("HalDisplay::sleep: TFT_BL set low and held\n");
    }
}

extern "C" {
    void on_comms_search_click(lv_event_t * e) {
        printf("UI EVENT: Reconnect button clicked!\n");
    }
}
