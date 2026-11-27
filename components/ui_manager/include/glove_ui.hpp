#pragma once

#include <array>
#include <cstdint>

#include "gesture_engine_types.hpp"
#include "battery_manager_types.hpp"

namespace GloveUI
{
    static constexpr uint16_t SCREEN_WIDTH   = 320;
    static constexpr uint16_t SCREEN_HEIGHT  = 240;
    static constexpr uint16_t ADC_MAX_MV     = 3300;
    static constexpr uint16_t DESIGN_CAP_MAH = 2000;

    enum class Screen : uint8_t
    {
        HOME     = 0,
        DEV      = 1,
        SETTINGS = 2,
    };

    enum class ConnStatus : uint8_t
    {
        CONNECTED    = 0,
        SEARCHING    = 1,
        DISCONNECTED = 2,
        UNKNOWN      = 3,
    };

    struct CommsStatus
    {
        ConnStatus r_to_recv = ConnStatus::DISCONNECTED;
        ConnStatus r_to_l    = ConnStatus::DISCONNECTED;
        ConnStatus l_to_recv = ConnStatus::UNKNOWN;

        bool operator==(const CommsStatus &o) const
        {
            return r_to_recv == o.r_to_recv &&
                   r_to_l    == o.r_to_l    &&
                   l_to_recv == o.l_to_recv;
        }
        bool operator!=(const CommsStatus &o) const { return !(*this == o); }
    };

    struct UIState
    {
        GestureEngine::InputMode active_mode     = GestureEngine::InputMode::UI;
        GestureEngine::InputMode pre_rest_mode   = GestureEngine::InputMode::UI;
        const char *mode_substate = "CURSOR READY";

        GestureEngine::Cursor cursor;

        CommsStatus comms;

        GestureEngine::GloveState right_glove;
        GestureEngine::GloveState left_glove;

        BatteryManager::BatteryDetails right_batt;
        BatteryManager::BatteryDetails left_batt;
    };

    struct UICommands
    {
        bool restart_comms_requested = false;
        bool brightness_changed      = false;
        uint8_t brightness_pct       = 100;
    };

    /**
     * @brief Builds the single-screen LVGL App Shell, registers the pointer
     *        input driver and crosshair cursor, and initializes the tick clock.
     *        Must be called after HalUI::init().
     *
     * @param initial_brightness_pct Initial brightness slider value (0-100%).
     */
    void init(uint8_t initial_brightness_pct = 50);

    /**
     * @brief Feeds cursor/mode state to LVGL, performs dirty-checked widget
     *        updates, advances lv_tick_inc(), runs lv_timer_handler(), and
     *        populates out_cmds with any 1-shot user actions.
     *
     * @param state    Latest snapshot assembled by disp_task.
     * @param out_cmds 1-shot commands emitted by LVGL callbacks.
     */
    void update(const UIState &state, UICommands &out_cmds);

    /**
     * @brief Returns the currently visible screen.
     */
    Screen active_screen();

    /**
     * @brief Programmatically switches the active screen.
     */
    void set_screen(Screen screen);
}