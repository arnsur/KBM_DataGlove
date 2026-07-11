#include "DisplayUI.h"
#include "Motion.h"
#include "Comms.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <lvgl.h>
#include "ui.h"

extern volatile int currentInputMode;
extern volatile int currentMouseMode;
extern volatile int shared_battery_l;
extern volatile int shared_battery_r;
extern volatile int rRecvConnStatus;
extern volatile int sensorValues[];
extern volatile int16_t cursor_x;
extern volatile int16_t cursor_y;
extern volatile bool lmbClicked;

Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);
const uint16_t SCREEN_WIDTH  = 320;
const uint16_t SCREEN_HEIGHT = 240;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[SCREEN_WIDTH * 10];

void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.drawRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
    lv_disp_flush_ready(disp_drv);
}

void my_glove_mouse_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data) {
    data->point.x = cursor_x;
    data->point.y = cursor_y;
    data->state = (lmbClicked && currentInputMode == 1) ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL; // Must be in UI mode (1) to send clicks to display
}

void guiTask(void *pvParameters) {
    uint32_t time_last = millis();
        while (1) {
            uint32_t time_now = millis();
            lv_tick_inc(time_now - time_last);
            time_last = time_now;

            //----------------------------------MODE LABELS----------------------------------------------
            if (currentInputMode == 0) {
                lv_label_set_text_fmt(ui_HomeModeLabel, "%d / %d", currentInputMode, currentMouseMode);\
                lv_label_set_text_fmt(ui_SettingsModeLabel, "%d / %d", currentInputMode, currentMouseMode);
                lv_label_set_text_fmt(ui_DevMiniModeLabelR, "%d / %d", currentInputMode, currentMouseMode);
                lv_label_set_text_fmt(ui_DevMiniModeLabelL, "%d / %d", currentInputMode, currentMouseMode);
            } else {
                lv_label_set_text_fmt(ui_HomeModeLabel, "%d", currentInputMode);
                lv_label_set_text_fmt(ui_SettingsModeLabel, "%d", currentInputMode);
                lv_label_set_text_fmt(ui_DevMiniModeLabelR, "%d", currentInputMode);
                lv_label_set_text_fmt(ui_DevMiniModeLabelL, "%d", currentInputMode);
            }

            //------------------------------------BATTERY LABELS---------------------------------------------
            lv_label_set_text_fmt(ui_HomeBatteryLevelLabelL, "%d%%", shared_battery_l);
            lv_label_set_text_fmt(ui_HomeBatteryLevelLabelR, "%d%%", shared_battery_r);

            lv_label_set_text_fmt(ui_SettingsBatteryLevelLabelL, "%d%%", shared_battery_l);
            lv_label_set_text_fmt(ui_SettingsBatteryLevelLabelR, "%d%%", shared_battery_r);

            lv_label_set_text_fmt(ui_RDevMiniBatteryLevelLabelL, "%d%%", shared_battery_l);
            lv_label_set_text_fmt(ui_RDevMiniBatteryLevelLabelR, "%d%%", shared_battery_r);

            lv_label_set_text_fmt(ui_LDevMiniBatteryLevelLabelL, "%d%%", shared_battery_l);
            lv_label_set_text_fmt(ui_LDevMiniBatteryLevelLabelR, "%d%%", shared_battery_r);

            if (shared_battery_l < 25) {
                lv_obj_set_style_text_color(ui_HomeBatteryLevelLabelL, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_SettingsBatteryLevelLabelL, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_RDevMiniBatteryLevelLabelL, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_LDevMiniBatteryLevelLabelL, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
            } else if (shared_battery_l < 50) {
                lv_obj_set_style_text_color(ui_HomeBatteryLevelLabelL, lv_color_hex(0xE88300), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_SettingsBatteryLevelLabelL, lv_color_hex(0xE88300), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_RDevMiniBatteryLevelLabelL, lv_color_hex(0xE88300), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_LDevMiniBatteryLevelLabelL, lv_color_hex(0xE88300), LV_PART_MAIN | LV_STATE_DEFAULT);
                
            } else if (shared_battery_l < 75) {
                lv_obj_set_style_text_color(ui_HomeBatteryLevelLabelL, lv_color_hex(0xE7D02D), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_SettingsBatteryLevelLabelL, lv_color_hex(0xE7D02D), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_RDevMiniBatteryLevelLabelL, lv_color_hex(0xE7D02D), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_LDevMiniBatteryLevelLabelL, lv_color_hex(0xE7D02D), LV_PART_MAIN | LV_STATE_DEFAULT);
            } else {
                lv_obj_set_style_text_color(ui_HomeBatteryLevelLabelL, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_SettingsBatteryLevelLabelL, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_RDevMiniBatteryLevelLabelL, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_LDevMiniBatteryLevelLabelL, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);
            }

            if (shared_battery_r < 25) {
                lv_obj_set_style_text_color(ui_HomeBatteryLevelLabelR, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_RDevMiniBatteryLevelLabelR, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_LDevMiniBatteryLevelLabelR, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
            } else if (shared_battery_r < 50) {
                lv_obj_set_style_text_color(ui_HomeBatteryLevelLabelR, lv_color_hex(0xE88300), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_RDevMiniBatteryLevelLabelR, lv_color_hex(0xE88300), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_LDevMiniBatteryLevelLabelR, lv_color_hex(0xE88300), LV_PART_MAIN | LV_STATE_DEFAULT);
            } else if (shared_battery_r < 75) {
                lv_obj_set_style_text_color(ui_HomeBatteryLevelLabelR, lv_color_hex(0xE7D02D), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_RDevMiniBatteryLevelLabelR, lv_color_hex(0xE7D02D), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_LDevMiniBatteryLevelLabelR, lv_color_hex(0xE7D02D), LV_PART_MAIN | LV_STATE_DEFAULT);
            } else {
                lv_obj_set_style_text_color(ui_HomeBatteryLevelLabelR, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_RDevMiniBatteryLevelLabelR, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_LDevMiniBatteryLevelLabelR, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);
            }

            //------------------------------------RECEIVER CONNECTION LABELS---------------------------------------------
            // Mini status bar does not get text changes so it stays condensed
            if (rRecvConnStatus == CONNECTED) {
                lv_label_set_text(ui_HomeRRecvConLabel, "R<>RECV:\nCONNECTED");
                lv_obj_set_style_text_color(ui_HomeRRecvConLabel, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);

                lv_label_set_text(ui_SettingsRRecvConLabel, "R<>RECV:\nCONNECTED");
                lv_obj_set_style_text_color(ui_SettingsRRecvConLabel, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);

                lv_obj_set_style_text_color(ui_DevRRecvLabelR, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_DevRRecvLabelL, lv_color_hex(0x31FF52), LV_PART_MAIN | LV_STATE_DEFAULT);
            } else if (rRecvConnStatus == DISCONNECTED) {
                lv_label_set_text(ui_HomeRRecvConLabel, "R<>RECV:\nDISCONNECTED");
                lv_obj_set_style_text_color(ui_HomeRRecvConLabel, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);

                lv_label_set_text(ui_SettingsRRecvConLabel, "R<>RECV:\nDISCONNECTED");
                lv_obj_set_style_text_color(ui_SettingsRRecvConLabel, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);

                lv_obj_set_style_text_color(ui_DevRRecvLabelR, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_DevRRecvLabelL, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
            } else if (rRecvConnStatus == SEARCHING) {
                lv_label_set_text(ui_HomeRRecvConLabel, "R<>RECV:\nSEARCHING");
                lv_obj_set_style_text_color(ui_HomeRRecvConLabel, lv_color_hex(0xFFDD00), LV_PART_MAIN | LV_STATE_DEFAULT);

                lv_label_set_text(ui_SettingsRRecvConLabel, "R<>RECV:\nSEARCHING");
                lv_obj_set_style_text_color(ui_SettingsRRecvConLabel, lv_color_hex(0xFFDD00), LV_PART_MAIN | LV_STATE_DEFAULT);

                lv_obj_set_style_text_color(ui_DevRRecvLabelR, lv_color_hex(0xFFDD00), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(ui_DevRRecvLabelL, lv_color_hex(0xFFDD00), LV_PART_MAIN | LV_STATE_DEFAULT);
            }

            //------------------------------------DEVELOPER SCREEN---------------------------------------------
            lv_label_set_text_fmt(ui_FSRReadingsLabelR, "%d (%d%%)\n%d (%d%%)\n%d (%d%%)\n%d (%d%%)\n%d (%d%%)",
                sensorValues[0], 100 *sensorValues[0]/4096,
                sensorValues[1], 100 *sensorValues[1]/4096,
                sensorValues[2], 100 *sensorValues[2]/4096,
                sensorValues[3], 100 *sensorValues[3]/4096,
                sensorValues[4], 100 *sensorValues[4]/4096);
            lv_bar_set_value(ui_FSR0BarR, sensorValues[0], LV_ANIM_OFF);
            lv_bar_set_value(ui_FSR1BarR, sensorValues[1], LV_ANIM_OFF);
            lv_bar_set_value(ui_FSR2BarR, sensorValues[2], LV_ANIM_OFF);
            lv_bar_set_value(ui_FSR3BarR, sensorValues[3], LV_ANIM_OFF);
            lv_bar_set_value(ui_FSR4BarR, sensorValues[4], LV_ANIM_OFF);

            // Uncomment when all flex sensors are wired and NUM_SENSORS = 12
            lv_label_set_text_fmt(ui_FlexReadingsLabelR, "%d (%d%%)\n%d (%d%%)\n%d (%d%%)\n%d (%d%%)\n%d (%d%%)\n%d (%d%%)"/*\n%d (%d%%)"*/,
                sensorValues[5], 100 *sensorValues[5]/4096,
                sensorValues[6], 100 *sensorValues[6]/4096,
                sensorValues[7], 100 *sensorValues[7]/4096,
                sensorValues[8], 100 *sensorValues[8]/4096,
                sensorValues[9], 100 *sensorValues[9]/4096,
                sensorValues[10], 100 *sensorValues[10]/4096);/*,
                sensorValues[11], 100 *sensorValues[11]/4096);*/
            
            lv_bar_set_value(ui_Flex5BarR, sensorValues[5], LV_ANIM_OFF);
            lv_bar_set_value(ui_Flex6BarR, sensorValues[6], LV_ANIM_OFF);
            lv_bar_set_value(ui_Flex7BarR, sensorValues[7], LV_ANIM_OFF);
            lv_bar_set_value(ui_Flex8BarR, sensorValues[8], LV_ANIM_OFF);
            lv_bar_set_value(ui_Flex9BarR, sensorValues[9], LV_ANIM_OFF);
            lv_bar_set_value(ui_Flex10BarR, sensorValues[10], LV_ANIM_OFF);
            // lv_bar_set_value(ui_Flex11Bar, sensorValues[11], LV_ANIM_OFF);

            lv_label_set_text_fmt(ui_IMUYawRollLabelR, "Yaw: %.2f°\nRoll: %.2f°", smoothedYaw, smoothedRoll);

            lv_timer_handler();
            vTaskDelay(pdMS_TO_TICKS(30));
    }
}

void initDisplay() {
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, LOW);
    
    setCpuFrequencyMhz(240);
    Serial.begin(115200);

    SPI.begin(TFT_CLK, -1, TFT_DIN, TFT_CS);
    tft.init(240, 320);
    tft.setRotation(1);
    tft.fillScreen(ST77XX_BLACK);

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_WIDTH * 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_glove_mouse_read; 
    lv_indev_t * mouse_indev = lv_indev_drv_register(&indev_drv);

    ui_init();
    lv_obj_clear_flag(lv_tabview_get_content(ui_DevTabViewR), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(lv_tabview_get_content(ui_DevTabViewL), LV_OBJ_FLAG_SCROLLABLE);

    lv_timer_handler();
    analogWrite(TFT_BL, 50);
}

void startGUITask() {
    lv_indev_t * mouse_indev = lv_indev_get_next(NULL);

    lv_obj_t * crosshair_obj = lv_label_create(lv_layer_sys()); 
    lv_label_set_text(crosshair_obj, LV_SYMBOL_PLUS); 
    lv_obj_set_style_text_color(crosshair_obj, lv_color_hex(0xFF0000), LV_PART_MAIN); 
    lv_indev_set_cursor(mouse_indev, crosshair_obj);
    
    lv_scr_load_anim(ui_HomeScreen, LV_SCR_LOAD_ANIM_NONE, 500, 0, false);
    lv_timer_handler();

    xTaskCreatePinnedToCore(guiTask, "GUI_Task", 10000, NULL, 1, NULL, 0);
}

extern "C" {
    void on_comms_search_click(lv_event_t * e) {
        Serial.println("UI: Reconnect button clicked. Waking up comms...");
        
        wakeUpComms(); 
    }
}