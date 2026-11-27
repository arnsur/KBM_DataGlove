#include "glove_ui.hpp"

#include "esp_timer.h"
#include <lvgl.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace GloveUI
{
    namespace
    {
        // =====================================================================
        // 1. DESIGN TOKENS, FONTS & COLOR PALETTE (MATCHES 320x240 SIMULATOR)
        // =====================================================================

        // Safe font fallbacks so this compiles with any lv_conf.h / sdkconfig setup.
        // Enable LV_FONT_MONTSERRAT_10 / 12 / 14 / 20 in menuconfig for best results.
#if defined(LV_FONT_MONTSERRAT_10) && LV_FONT_MONTSERRAT_10
        constexpr const lv_font_t *FONT_XS = &lv_font_montserrat_10;
#else
        constexpr const lv_font_t *FONT_XS = LV_FONT_DEFAULT;
#endif

#if defined(LV_FONT_MONTSERRAT_12) && LV_FONT_MONTSERRAT_12
        constexpr const lv_font_t *FONT_SM = &lv_font_montserrat_12;
#else
        constexpr const lv_font_t *FONT_SM = LV_FONT_DEFAULT;
#endif

#if defined(LV_FONT_MONTSERRAT_14) && LV_FONT_MONTSERRAT_14
        constexpr const lv_font_t *FONT_MD = &lv_font_montserrat_14;
#else
        constexpr const lv_font_t *FONT_MD = LV_FONT_DEFAULT;
#endif

#if defined(LV_FONT_MONTSERRAT_20) && LV_FONT_MONTSERRAT_20
        constexpr const lv_font_t *FONT_LG = &lv_font_montserrat_20;
#elif defined(LV_FONT_MONTSERRAT_18) && LV_FONT_MONTSERRAT_18
        constexpr const lv_font_t *FONT_LG = &lv_font_montserrat_18;
#elif defined(LV_FONT_MONTSERRAT_16) && LV_FONT_MONTSERRAT_16
        constexpr const lv_font_t *FONT_LG = &lv_font_montserrat_16;
#else
        constexpr const lv_font_t *FONT_LG = LV_FONT_DEFAULT;
#endif

        // Core dark surfaces & borders
        constexpr uint32_t COL_BG_ROOT      = 0x090B10;
        constexpr uint32_t COL_BG_BAR       = 0x111620;
        constexpr uint32_t COL_BG_RAIL      = 0x0E131B;
        constexpr uint32_t COL_BG_RAIL_UI   = 0x0D1816;
        constexpr uint32_t COL_BG_CARD      = 0x111620;
        constexpr uint32_t COL_BG_CARD_DARK = 0x0B101B;
        constexpr uint32_t COL_BG_PILL      = 0x171E2C;
        constexpr uint32_t COL_BG_MODAL     = 0x111827;
        constexpr uint32_t COL_BG_TRACK     = 0x1A2232;
        constexpr uint32_t COL_BORDER       = 0x222B3A;
        constexpr uint32_t COL_BORDER_SUB   = 0x1D2636;
        constexpr uint32_t COL_BORDER_PILL  = 0x283347;

        // Text hierarchy
        constexpr uint32_t COL_TEXT_PRIMARY = 0xF1F5F9;
        constexpr uint32_t COL_TEXT_SECOND  = 0xCBD5E1;
        constexpr uint32_t COL_TEXT_MUTED   = 0x94A3B8;
        constexpr uint32_t COL_TEXT_DIM     = 0x64748B;

        // Semantic accents
        constexpr uint32_t COL_SKY          = 0x38BDF8;
        constexpr uint32_t COL_SKY_BG       = 0x0E2A3D;
        constexpr uint32_t COL_EMERALD      = 0x34D399;
        constexpr uint32_t COL_EMERALD_BG   = 0x0E2B22;
        constexpr uint32_t COL_GREEN        = 0x22C55E;
        constexpr uint32_t COL_VIOLET       = 0xA78BFA;
        constexpr uint32_t COL_VIOLET_BG    = 0x21193B;
        constexpr uint32_t COL_AMBER        = 0xF59E0B;
        constexpr uint32_t COL_AMBER_LIGHT  = 0xFCD34D;
        constexpr uint32_t COL_AMBER_BG     = 0x2E220E;
        constexpr uint32_t COL_ROSE         = 0xFB7185;
        constexpr uint32_t COL_RED          = 0xEF4444;
        constexpr uint32_t COL_RED_BG       = 0x2F1218;
        constexpr uint32_t COL_SLATE_BG     = 0x1E293B;

        constexpr uint8_t DEV_TEXT_DIVIDER  = 3; // 30 Hz bars / ~10 Hz numeric text

        // =====================================================================
        // 2. MODE METADATA TABLE
        // =====================================================================

        struct ModeVisuals
        {
            const char *short_name;
            const char *hero_title;
            const char *default_substate;
            const char *symbol;
            uint32_t    accent_hex;
            uint32_t    bg_tint_hex;
            uint32_t    card_bg_hex;
        };

        constexpr ModeVisuals MODE_VIS[4] = {
            // 0: MOUSE
            {"MOUSE", "MOUSE",    "CURSOR READY",            LV_SYMBOL_GPS,      COL_SKY,     COL_SKY_BG,     0x111620},
            // 1: UI
            {"UI",    "UI MODE",  "DISPLAY CONTROL ACTIVE",  LV_SYMBOL_EDIT,     COL_EMERALD, COL_EMERALD_BG, 0x101918},
            // 2: KEYBOARD
            {"KEYBD", "KEYBOARD", "LOWERCASE - READY",       LV_SYMBOL_KEYBOARD, COL_VIOLET,  COL_VIOLET_BG,  0x131422},
            // 3: REST
            {"REST",  "RESTING",  "INPUTS IGNORED - WAKEUP", LV_SYMBOL_PAUSE,    COL_AMBER,   COL_AMBER_BG,   0x0E121A},
        };

        inline uint8_t mode_idx(GestureEngine::InputMode m)
        {
            uint8_t idx = static_cast<uint8_t>(m);
            return idx < 4 ? idx : 0;
        }

        // =====================================================================
        // 3. WIDGET REGISTRY, POINTER DRIVER & RUNTIME UI STATE
        // =====================================================================

        struct SensorRowWidgets
        {
            lv_obj_t *bar = nullptr;
            lv_obj_t *val = nullptr;
        };

        struct CommsRowWidgets
        {
            lv_obj_t *badge_box = nullptr;
            lv_obj_t *badge_lbl = nullptr;
            lv_obj_t *sub_lbl   = nullptr;
        };

        struct BattCardWidgets
        {
            lv_obj_t *card    = nullptr;
            lv_obj_t *dot     = nullptr; // Optional (used on Left Glove)
            lv_obj_t *pct_lbl = nullptr;
            lv_obj_t *chev    = nullptr;
            lv_obj_t *bar     = nullptr;
            lv_obj_t *sub_lbl = nullptr;
            lv_obj_t *mah_lbl = nullptr;
        };

        struct Widgets
        {
            // Top Status Bar
            lv_obj_t *mode_pill       = nullptr;
            lv_obj_t *mode_pill_lbl   = nullptr;
            lv_obj_t *dot_r_recv      = nullptr;
            lv_obj_t *dot_r_l         = nullptr;
            lv_obj_t *dot_l_recv      = nullptr;
            lv_obj_t *dot_l_recv_q    = nullptr;
            lv_obj_t *batt_r_dot      = nullptr;
            lv_obj_t *batt_r_pct      = nullptr;
            lv_obj_t *batt_l_dot      = nullptr;
            lv_obj_t *batt_l_pct      = nullptr;

            // Left Nav Rail
            lv_obj_t *nav_rail        = nullptr;
            lv_obj_t *nav_btns[3]     = {nullptr, nullptr, nullptr};

            // Screen Containers
            lv_obj_t *screens[3]      = {nullptr, nullptr, nullptr};

            // Home Screen (Mode HUD)
            lv_obj_t *hero_card       = nullptr;
            lv_obj_t *hero_id_pill    = nullptr;
            lv_obj_t *hero_id_lbl     = nullptr;
            lv_obj_t *hero_icon_box   = nullptr;
            lv_obj_t *hero_icon_lbl   = nullptr;
            lv_obj_t *hero_title_lbl  = nullptr;
            lv_obj_t *hero_resume_lbl = nullptr;
            lv_obj_t *cycle_box[3]    = {nullptr, nullptr, nullptr};
            lv_obj_t *cycle_lbl[3]    = {nullptr, nullptr, nullptr};
            lv_obj_t *ctx_dot         = nullptr;
            lv_obj_t *ctx_lbl         = nullptr;

            // Developer Telemetry Screen
            lv_obj_t *dev_tab[2]          = {nullptr, nullptr};
            lv_obj_t *dev_tab_lbl[2]      = {nullptr, nullptr};
            lv_obj_t *dev_tab_dot[2]      = {nullptr, nullptr};
            lv_obj_t *dev_hdr_fsr         = nullptr;
            lv_obj_t *dev_hdr_flex        = nullptr;
            lv_obj_t *dev_hdr_imu         = nullptr;
            SensorRowWidgets sensors[10];
            lv_obj_t *imu_quat_lbl[4]     = {nullptr, nullptr, nullptr, nullptr};
            lv_obj_t *imu_euler_lbl[3]    = {nullptr, nullptr, nullptr};
            lv_obj_t *dev_offline_overlay = nullptr;
            lv_obj_t *dev_offline_icon    = nullptr;
            lv_obj_t *dev_offline_title   = nullptr;
            lv_obj_t *dev_offline_sub     = nullptr;

            // Settings Screen
            lv_obj_t *bright_slider       = nullptr;
            lv_obj_t *bright_val_lbl      = nullptr;
            lv_obj_t *set_comms_summary   = nullptr;

            // Comms Manager Modal (on lv_layer_top)
            lv_obj_t *modal_comms         = nullptr;
            lv_obj_t *comms_hdr_dot       = nullptr;
            CommsRowWidgets comms_r_recv;
            CommsRowWidgets comms_r_l;
            CommsRowWidgets comms_l_recv;
            lv_obj_t *btn_restart_comms   = nullptr;
            lv_obj_t *lbl_restart_comms   = nullptr;

            // Battery & Power Modal (on lv_layer_top)
            lv_obj_t *modal_batt          = nullptr;
            lv_obj_t *batt_view_overview  = nullptr;
            lv_obj_t *batt_view_detail    = nullptr;
            BattCardWidgets batt_card[2]; // 0 = Right (G1), 1 = Left (G2)
            lv_obj_t *detail_title        = nullptr;
            lv_obj_t *detail_state_pill   = nullptr;
            lv_obj_t *detail_state_lbl    = nullptr;
            lv_obj_t *detail_bar          = nullptr;
            lv_obj_t *detail_soc          = nullptr;
            lv_obj_t *detail_cap          = nullptr;
            lv_obj_t *detail_cap_sub      = nullptr;
            lv_obj_t *detail_curr         = nullptr;
            lv_obj_t *detail_curr_sub     = nullptr;
            lv_obj_t *detail_soh          = nullptr;
            lv_obj_t *detail_soh_sub      = nullptr;
        };

        Widgets g_ui = {};

        // LVGL pointer driver & tick timing state
        lv_indev_drv_t        g_indev_drv      = {};
        GestureEngine::Cursor g_cursor         = {};
        bool                  g_ui_mode_active = false;
        uint32_t              g_last_tick_ms   = 0;

        // Internal interactive UI state
        bool     g_initialized        = false;
        Screen   g_active_screen      = Screen::HOME;
        uint8_t  g_active_dev_glove   = 0;  // 0 = Right (G1), 1 = Left (G2)
        int8_t   g_batt_detail_glove  = -1; // -1 = overview, 0 = Right, 1 = Left
        uint8_t  g_brightness_pct     = 50; // 0..100 user-facing slider value
        uint32_t g_dev_frame_counter  = 0;
        bool     g_force_dev_text     = true;
        bool     g_force_batt_refresh = true;

        // Pending 1-shot commands set by LVGL event callbacks
        bool     g_pending_restart_comms = false;
        bool     g_pending_brightness    = false;

        // Cached snapshot for dirty-checking
        bool        g_has_prev          = false;
        GestureEngine::InputMode   g_prev_mode         = GestureEngine::InputMode::MOUSE;
        GestureEngine::InputMode   g_prev_pre_rest     = GestureEngine::InputMode::MOUSE;
        char        g_prev_substate[48] = {0};
        CommsStatus g_prev_comms        = {};
        int16_t     g_prev_r_pct10      = -1;
        int16_t     g_prev_l_pct10      = -1;
        uint16_t    g_prev_r_mah        = 0xFFFF;
        uint16_t    g_prev_l_mah        = 0xFFFF;
        int16_t     g_prev_r_ma         = 0x7FFF;
        int16_t     g_prev_l_ma         = 0x7FFF;

        // =====================================================================
        // 4. LVGL POINTER INPUT CALLBACK
        // =====================================================================

        void indev_read(lv_indev_drv_t *, lv_indev_data_t *data)
        {
            data->point.x = std::clamp<int16_t>(g_cursor.x, 0, static_cast<int16_t>(SCREEN_WIDTH - 1));
            data->point.y = std::clamp<int16_t>(g_cursor.y, 0, static_cast<int16_t>(SCREEN_HEIGHT - 1));
            data->state   = (g_cursor.is_pressed && g_ui_mode_active)
                                ? LV_INDEV_STATE_PR
                                : LV_INDEV_STATE_REL;
        }

        // =====================================================================
        // 5. SHARED HELPER FUNCTIONS & COMPONENT FACTORIES
        // =====================================================================

        inline lv_color_t hex(uint32_t c) { return lv_color_hex(c); }

        inline float clampf(float v, float lo, float hi)
        {
            return v < lo ? lo : (v > hi ? hi : v);
        }

        inline uint32_t battery_color_hex(float pct)
        {
            if (pct < 20.0f) return COL_RED;
            if (pct < 45.0f) return COL_AMBER;
            return COL_EMERALD;
        }

        inline ConnStatus effective_l_to_recv(const CommsStatus &c)
        {
            return (c.r_to_l == ConnStatus::CONNECTED) ? c.l_to_recv : ConnStatus::UNKNOWN;
        }

        // Removes default LVGL padding/border/scroll so coordinates match exact pixels
        void reset_container(lv_obj_t *obj)
        {
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_pad_all(obj, 0, 0);
            lv_obj_set_style_border_width(obj, 0, 0);
            lv_obj_set_style_radius(obj, 0, 0);
            lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
            lv_obj_set_style_outline_width(obj, 0, 0);
            lv_obj_set_style_shadow_width(obj, 0, 0);
        }

        lv_obj_t *create_card(lv_obj_t *parent, lv_coord_t w, lv_coord_t h,
                              uint32_t bg = COL_BG_CARD, uint32_t border = COL_BORDER,
                              lv_coord_t radius = 6)
        {
            lv_obj_t *card = lv_obj_create(parent);
            reset_container(card);
            lv_obj_set_size(card, w, h);
            lv_obj_set_style_bg_color(card, hex(bg), 0);
            lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(card, hex(border), 0);
            lv_obj_set_style_border_width(card, 1, 0);
            lv_obj_set_style_radius(card, radius, 0);
            return card;
        }

        lv_obj_t *create_label(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color_hex)
        {
            lv_obj_t *lbl = lv_label_create(parent);
            lv_label_set_text(lbl, text);
            lv_obj_set_style_text_font(lbl, font, 0);
            lv_obj_set_style_text_color(lbl, hex(color_hex), 0);
            return lbl;
        }

        lv_obj_t *create_dot(lv_obj_t *parent, lv_coord_t size, uint32_t color_hex)
        {
            lv_obj_t *dot = lv_obj_create(parent);
            reset_container(dot);
            lv_obj_set_size(dot, size, size);
            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(dot, hex(color_hex), 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
            return dot;
        }

        // Reusable Comms Manager row
        CommsRowWidgets create_comms_row(lv_obj_t *parent, const char *title, uint32_t title_col, const char *subtitle)
        {
            CommsRowWidgets w = {};
            lv_obj_t *row = create_card(parent, 270, 34, COL_BG_CARD_DARK, 0x1E293B, 6);
            lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_hor(row, 8, 0);

            lv_obj_t *left = lv_obj_create(row);
            reset_container(left);
            lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_style_pad_row(left, 1, 0);

            create_label(left, title, FONT_XS, title_col);
            w.sub_lbl = create_label(left, subtitle, FONT_XS, COL_TEXT_DIM);

            w.badge_box = create_card(row, LV_SIZE_CONTENT, 18, COL_EMERALD_BG, COL_EMERALD, 4);
            lv_obj_set_style_pad_hor(w.badge_box, 6, 0);
            lv_obj_set_flex_flow(w.badge_box, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(w.badge_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            w.badge_lbl = create_label(w.badge_box, "CONNECTED", FONT_XS, COL_EMERALD);

            return w;
        }

        // Reusable Dual-Glove Battery Overview Card
        BattCardWidgets create_batt_card(lv_obj_t *parent, const char *title, uint32_t title_col,
                                         bool with_dot, lv_event_cb_t click_cb, void *user_data)
        {
            BattCardWidgets w = {};
            w.card = create_card(parent, 270, 74, COL_BG_CARD_DARK, 0x1E293B, 6);
            lv_obj_add_flag(w.card, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_pad_all(w.card, 8, 0);
            lv_obj_set_flex_flow(w.card, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(w.card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
            lv_obj_add_event_cb(w.card, click_cb, LV_EVENT_CLICKED, user_data);

            // Top Row: Title + Percentage + Chevron
            lv_obj_t *top = lv_obj_create(w.card);
            reset_container(top);
            lv_obj_set_size(top, LV_PCT(100), LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            lv_obj_t *title_wrap = lv_obj_create(top);
            reset_container(title_wrap);
            lv_obj_set_size(title_wrap, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(title_wrap, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(title_wrap, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(title_wrap, 5, 0);

            if (with_dot)
            {
                w.dot = create_dot(title_wrap, 6, COL_GREEN);
            }
            create_label(title_wrap, title, FONT_XS, title_col);

            lv_obj_t *right_wrap = lv_obj_create(top);
            reset_container(right_wrap);
            lv_obj_set_size(right_wrap, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(right_wrap, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(right_wrap, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(right_wrap, 5, 0);

            w.pct_lbl = create_label(right_wrap, "100.0%", FONT_SM, COL_EMERALD);
            w.chev    = create_label(right_wrap, LV_SYMBOL_RIGHT, FONT_XS, COL_TEXT_DIM);

            // Middle: Progress Bar
            w.bar = lv_bar_create(w.card);
            lv_obj_set_size(w.bar, LV_PCT(100), 7);
            lv_bar_set_range(w.bar, 0, 100);
            lv_bar_set_value(w.bar, 100, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(w.bar, hex(COL_BG_TRACK), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(w.bar, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_radius(w.bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
            lv_obj_set_style_bg_color(w.bar, hex(COL_EMERALD), LV_PART_INDICATOR);
            lv_obj_set_style_radius(w.bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);

            // Bottom Row: Subtitle + mAh
            lv_obj_t *bot = lv_obj_create(w.card);
            reset_container(bot);
            lv_obj_set_size(bot, LV_PCT(100), LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(bot, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(bot, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            w.sub_lbl = create_label(bot, "MAX17260 - TAP FOR DETAILS", FONT_XS, COL_TEXT_MUTED);
            w.mah_lbl = create_label(bot, "2000 / 2000 mAh", FONT_XS, COL_TEXT_SECOND);

            return w;
        }

        // Reusable 2x2 Detailed Battery Metric Cell
        void create_detail_cell(lv_obj_t *parent, const char *title, const char *init_val,
                                const char *init_sub, lv_obj_t **out_val, lv_obj_t **out_sub)
        {
            lv_obj_t *cell = create_card(parent, 132, 62, COL_BG_CARD_DARK, 0x1E293B, 6);
            lv_obj_set_style_pad_hor(cell, 8, 0);
            lv_obj_set_style_pad_ver(cell, 5, 0);
            lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

            create_label(cell, title, FONT_XS, COL_TEXT_MUTED);
            *out_val = create_label(cell, init_val, FONT_MD, COL_TEXT_PRIMARY);
            lv_obj_t *sub = create_label(cell, init_sub, FONT_XS, COL_TEXT_DIM);
            if (out_sub != nullptr)
            {
                *out_sub = sub;
            }
        }

        // =====================================================================
        // 6. FORWARD DECLARATIONS FOR INTERNAL REFRESH HELPERS
        // =====================================================================

        void refresh_nav_styles();
        void refresh_dev_glove_theme();
        void refresh_dev_offline_overlay(ConnStatus r_to_l);
        void open_comms_modal();
        void close_comms_modal();
        void open_batt_modal();
        void close_batt_modal();
        void show_batt_overview();
        void show_batt_detail(uint8_t glove_idx);

        // =====================================================================
        // 7. LVGL EVENT CALLBACKS
        // =====================================================================

        void on_nav_click(lv_event_t *e)
        {
            auto target = static_cast<Screen>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
            set_screen(target);
        }

        void on_open_comms_click(lv_event_t *)  { open_comms_modal(); }
        void on_close_comms_click(lv_event_t *) { close_comms_modal(); }

        void on_comms_backdrop_click(lv_event_t *e)
        {
            if (lv_event_get_target(e) == g_ui.modal_comms)
            {
                close_comms_modal();
            }
        }

        void on_restart_comms_click(lv_event_t *)
        {
            g_pending_restart_comms = true;
        }

        void on_open_batt_click(lv_event_t *)  { open_batt_modal(); }
        void on_close_batt_click(lv_event_t *) { close_batt_modal(); }

        void on_batt_backdrop_click(lv_event_t *e)
        {
            if (lv_event_get_target(e) == g_ui.modal_batt)
            {
                close_batt_modal();
            }
        }

        void on_batt_card_click(lv_event_t *e)
        {
            uint8_t glove_idx = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
            if (glove_idx == 1 && g_prev_comms.r_to_l != ConnStatus::CONNECTED)
            {
                return; // Left glove offline
            }
            show_batt_detail(glove_idx);
        }

        void on_batt_back_click(lv_event_t *) { show_batt_overview(); }

        void on_dev_tab_click(lv_event_t *e)
        {
            uint8_t glove_idx = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
            if (glove_idx > 1) return;
            g_active_dev_glove = glove_idx;
            g_force_dev_text = true;
            refresh_dev_glove_theme();
            refresh_dev_offline_overlay(g_prev_comms.r_to_l);
        }

        void on_brightness_changed(lv_event_t *e)
        {
            lv_obj_t *slider = lv_event_get_target(e);
            int32_t val = lv_slider_get_value(slider);
            if (val < 0) val = 0;
            if (val > 100) val = 100;

            g_brightness_pct = static_cast<uint8_t>(val);
            lv_label_set_text_fmt(g_ui.bright_val_lbl, "%u%%", static_cast<unsigned>(g_brightness_pct));
            g_pending_brightness = true;
        }

        // =====================================================================
        // 8. MODAL & NAVIGATION VISIBILITY HELPERS
        // =====================================================================

        void open_comms_modal()
        {
            close_batt_modal();
            lv_obj_clear_flag(g_ui.modal_comms, LV_OBJ_FLAG_HIDDEN);
        }

        void close_comms_modal()
        {
            lv_obj_add_flag(g_ui.modal_comms, LV_OBJ_FLAG_HIDDEN);
        }

        void show_batt_overview()
        {
            g_batt_detail_glove = -1;
            lv_obj_add_flag(g_ui.batt_view_detail, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.batt_view_overview, LV_OBJ_FLAG_HIDDEN);
        }

        void show_batt_detail(uint8_t glove_idx)
        {
            g_batt_detail_glove = static_cast<int8_t>(glove_idx);
            g_force_batt_refresh = true;
            lv_obj_add_flag(g_ui.batt_view_overview, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.batt_view_detail, LV_OBJ_FLAG_HIDDEN);
        }

        void open_batt_modal()
        {
            close_comms_modal();
            show_batt_overview();
            g_force_batt_refresh = true;
            lv_obj_clear_flag(g_ui.modal_batt, LV_OBJ_FLAG_HIDDEN);
        }

        void close_batt_modal()
        {
            lv_obj_add_flag(g_ui.modal_batt, LV_OBJ_FLAG_HIDDEN);
            g_batt_detail_glove = -1;
        }

        void refresh_nav_styles()
        {
            const bool is_ui_mode = (g_prev_mode == GestureEngine::InputMode::UI);

            // Highlight left rail when in UI mode, gently dim in other modes
            lv_obj_set_style_bg_color(g_ui.nav_rail, hex(is_ui_mode ? COL_BG_RAIL_UI : COL_BG_RAIL), 0);
            lv_obj_set_style_border_color(g_ui.nav_rail, hex(is_ui_mode ? COL_EMERALD : COL_BORDER), 0);
            lv_obj_set_style_opa(g_ui.nav_rail, is_ui_mode ? LV_OPA_COVER : LV_OPA_70, 0);

            for (uint8_t i = 0; i < 3; i++)
            {
                const bool active = (static_cast<uint8_t>(g_active_screen) == i);
                lv_obj_t *btn = g_ui.nav_btns[i];
                lv_obj_t *lbl = lv_obj_get_child(btn, 0);

                if (active)
                {
                    uint32_t acc = is_ui_mode ? COL_EMERALD : COL_SKY;
                    uint32_t bg  = is_ui_mode ? COL_EMERALD_BG : COL_SKY_BG;
                    lv_obj_set_style_bg_color(btn, hex(bg), 0);
                    lv_obj_set_style_border_color(btn, hex(acc), 0);
                    lv_obj_set_style_text_color(lbl, hex(acc), 0);
                }
                else
                {
                    lv_obj_set_style_bg_color(btn, hex(0x141B26), 0);
                    lv_obj_set_style_border_color(btn, hex(COL_BORDER), 0);
                    lv_obj_set_style_text_color(lbl, hex(COL_TEXT_MUTED), 0);
                }
            }
        }

        void refresh_dev_glove_theme()
        {
            const bool is_r = (g_active_dev_glove == 0);
            const uint32_t acc = is_r ? COL_SKY : COL_VIOLET;

            // Right tab (index 0)
            lv_obj_set_style_bg_color(g_ui.dev_tab[0], hex(is_r ? COL_SKY_BG : COL_BG_CARD), 0);
            lv_obj_set_style_border_color(g_ui.dev_tab[0], hex(is_r ? COL_SKY : COL_BG_CARD), 0);
            lv_obj_set_style_text_color(g_ui.dev_tab_lbl[0], hex(is_r ? COL_SKY : COL_TEXT_MUTED), 0);

            // Left tab (index 1)
            lv_obj_set_style_bg_color(g_ui.dev_tab[1], hex(!is_r ? COL_VIOLET_BG : COL_BG_CARD), 0);
            lv_obj_set_style_border_color(g_ui.dev_tab[1], hex(!is_r ? COL_VIOLET : COL_BG_CARD), 0);
            lv_obj_set_style_text_color(g_ui.dev_tab_lbl[1], hex(!is_r ? COL_VIOLET : COL_TEXT_MUTED), 0);

            // Headers, bars & Euler values
            lv_obj_set_style_text_color(g_ui.dev_hdr_fsr, hex(acc), 0);
            lv_obj_set_style_text_color(g_ui.dev_hdr_flex, hex(acc), 0);
            lv_obj_set_style_text_color(g_ui.dev_hdr_imu, hex(acc), 0);

            for (int i = 0; i < 10; i++)
            {
                lv_obj_set_style_bg_color(g_ui.sensors[i].bar, hex(acc), LV_PART_INDICATOR);
            }
            for (int i = 0; i < 3; i++)
            {
                lv_obj_set_style_text_color(g_ui.imu_euler_lbl[i], hex(acc), 0);
            }
        }

        void refresh_dev_offline_overlay(ConnStatus r_to_l)
        {
            const bool show = (g_active_dev_glove == 1 && r_to_l != ConnStatus::CONNECTED);
            if (!show)
            {
                lv_obj_add_flag(g_ui.dev_offline_overlay, LV_OBJ_FLAG_HIDDEN);
                return;
            }

            lv_obj_clear_flag(g_ui.dev_offline_overlay, LV_OBJ_FLAG_HIDDEN);
            if (r_to_l == ConnStatus::SEARCHING)
            {
                lv_obj_set_style_border_color(g_ui.dev_offline_overlay, hex(COL_AMBER), 0);
                lv_obj_set_style_text_color(g_ui.dev_offline_icon, hex(COL_AMBER_LIGHT), 0);
                lv_label_set_text(g_ui.dev_offline_title, "SEARCHING FOR LEFT GLOVE");
                lv_obj_set_style_text_color(g_ui.dev_offline_title, hex(COL_AMBER_LIGHT), 0);
                lv_label_set_text(g_ui.dev_offline_sub, "R <> L HANDSHAKE IN PROGRESS");
            }
            else
            {
                lv_obj_set_style_border_color(g_ui.dev_offline_overlay, hex(COL_RED), 0);
                lv_obj_set_style_text_color(g_ui.dev_offline_icon, hex(COL_ROSE), 0);
                lv_label_set_text(g_ui.dev_offline_title, "LEFT GLOVE OFFLINE");
                lv_obj_set_style_text_color(g_ui.dev_offline_title, hex(COL_ROSE), 0);
                lv_label_set_text(g_ui.dev_offline_sub, "R <> L DISCONNECTED");
            }
        }

        // =====================================================================
        // 9. UI BUILDERS (CALLED ONCE IN GloveUI::init)
        // =====================================================================

        void build_top_bar(lv_obj_t *root)
        {
            lv_obj_t *bar = lv_obj_create(root);
            reset_container(bar);
            lv_obj_set_pos(bar, 0, 0);
            lv_obj_set_size(bar, SCREEN_WIDTH, 24);
            lv_obj_set_style_bg_color(bar, hex(COL_BG_BAR), 0);
            lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(bar, hex(COL_BORDER), 0);
            lv_obj_set_style_border_width(bar, 1, 0);
            lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
            lv_obj_set_style_pad_hor(bar, 5, 0);
            lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            // Left: Active Mode Pill
            g_ui.mode_pill = create_card(bar, LV_SIZE_CONTENT, 16, COL_SKY_BG, COL_SKY, 4);
            lv_obj_set_style_pad_hor(g_ui.mode_pill, 5, 0);
            lv_obj_set_flex_flow(g_ui.mode_pill, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(g_ui.mode_pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            g_ui.mode_pill_lbl = create_label(g_ui.mode_pill, "M0: MOUSE", FONT_XS, COL_SKY);

            // Center: Clickable 3-Link Comms Pill
            lv_obj_t *comms_pill = create_card(bar, LV_SIZE_CONTENT, 18, COL_BG_PILL, COL_BORDER_PILL, 4);
            lv_obj_add_flag(comms_pill, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_pad_hor(comms_pill, 5, 0);
            lv_obj_set_flex_flow(comms_pill, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(comms_pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(comms_pill, 4, 0);
            lv_obj_add_event_cb(comms_pill, on_open_comms_click, LV_EVENT_CLICKED, nullptr);

            create_label(comms_pill, "R-X", FONT_XS, COL_TEXT_SECOND);
            g_ui.dot_r_recv = create_dot(comms_pill, 6, COL_RED);

            create_label(comms_pill, "R-L", FONT_XS, COL_TEXT_SECOND);
            g_ui.dot_r_l = create_dot(comms_pill, 6, COL_RED);

            create_label(comms_pill, "L-X", FONT_XS, COL_TEXT_SECOND);
            g_ui.dot_l_recv = create_dot(comms_pill, 9, COL_TEXT_DIM);
            lv_obj_set_flex_flow(g_ui.dot_l_recv, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(g_ui.dot_l_recv, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            g_ui.dot_l_recv_q = create_label(g_ui.dot_l_recv, "?", FONT_XS, COL_TEXT_PRIMARY);

            // Right: Clickable Dual-Glove Battery Pill
            lv_obj_t *batt_pill = create_card(bar, LV_SIZE_CONTENT, 18, COL_BG_PILL, COL_BORDER_PILL, 4);
            lv_obj_add_flag(batt_pill, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_pad_hor(batt_pill, 5, 0);
            lv_obj_set_flex_flow(batt_pill, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(batt_pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(batt_pill, 3, 0);
            lv_obj_add_event_cb(batt_pill, on_open_batt_click, LV_EVENT_CLICKED, nullptr);

            g_ui.batt_r_dot = create_dot(batt_pill, 5, COL_EMERALD);
            create_label(batt_pill, "R", FONT_XS, COL_SKY);
            g_ui.batt_r_pct = create_label(batt_pill, "--%", FONT_XS, COL_TEXT_PRIMARY);

            lv_obj_t *sep = lv_obj_create(batt_pill);
            reset_container(sep);
            lv_obj_set_size(sep, 1, 9);
            lv_obj_set_style_bg_color(sep, hex(0x2A3548), 0);
            lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);

            g_ui.batt_l_dot = create_dot(batt_pill, 5, COL_RED);
            create_label(batt_pill, "L", FONT_XS, COL_VIOLET);
            g_ui.batt_l_pct = create_label(batt_pill, "--", FONT_XS, COL_TEXT_DIM);
        }

        void build_nav_rail(lv_obj_t *root)
        {
            g_ui.nav_rail = lv_obj_create(root);
            reset_container(g_ui.nav_rail);
            lv_obj_set_pos(g_ui.nav_rail, 0, 24);
            lv_obj_set_size(g_ui.nav_rail, 44, 216);
            lv_obj_set_style_bg_color(g_ui.nav_rail, hex(COL_BG_RAIL), 0);
            lv_obj_set_style_bg_opa(g_ui.nav_rail, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(g_ui.nav_rail, hex(COL_BORDER), 0);
            lv_obj_set_style_border_width(g_ui.nav_rail, 1, 0);
            lv_obj_set_style_border_side(g_ui.nav_rail, LV_BORDER_SIDE_RIGHT, 0);
            lv_obj_set_style_pad_ver(g_ui.nav_rail, 4, 0);
            lv_obj_set_flex_flow(g_ui.nav_rail, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(g_ui.nav_rail, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_row(g_ui.nav_rail, 4, 0);

            const char *icons[3] = {LV_SYMBOL_HOME, LV_SYMBOL_LIST, LV_SYMBOL_SETTINGS};
            for (uintptr_t i = 0; i < 3; i++)
            {
                lv_obj_t *btn = create_card(g_ui.nav_rail, 36, 36, 0x141B26, COL_BORDER, 6);
                lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                create_label(btn, icons[i], FONT_MD, COL_TEXT_MUTED);
                lv_obj_add_event_cb(btn, on_nav_click, LV_EVENT_CLICKED, reinterpret_cast<void *>(i));
                g_ui.nav_btns[i] = btn;
            }
        }

        void build_screen_home(lv_obj_t *content)
        {
            lv_obj_t *scr = lv_obj_create(content);
            reset_container(scr);
            lv_obj_set_size(scr, 264, 204);
            lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
            lv_obj_set_style_pad_row(scr, 6, 0);
            g_ui.screens[static_cast<uint8_t>(Screen::HOME)] = scr;

            // Mode Hero Card (264x166)
            g_ui.hero_card = create_card(scr, 264, 166, COL_BG_CARD, COL_SKY, 7);
            lv_obj_set_style_pad_all(g_ui.hero_card, 10, 0);
            lv_obj_set_flex_flow(g_ui.hero_card, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(g_ui.hero_card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

            // Hero Top: Mode ID Pill
            g_ui.hero_id_pill = create_card(g_ui.hero_card, LV_SIZE_CONTENT, 18, COL_SKY_BG, COL_SKY, 4);
            lv_obj_set_style_pad_hor(g_ui.hero_id_pill, 6, 0);
            lv_obj_set_flex_flow(g_ui.hero_id_pill, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(g_ui.hero_id_pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            g_ui.hero_id_lbl = create_label(g_ui.hero_id_pill, "MODE 0", FONT_XS, COL_SKY);

            // Hero Center: Icon Box + Large Title (+ Resume hint in REST)
            lv_obj_t *mid = lv_obj_create(g_ui.hero_card);
            reset_container(mid);
            lv_obj_set_size(mid, LV_PCT(100), LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(mid, 12, 0);

            g_ui.hero_icon_box = create_card(mid, 48, 48, COL_SKY_BG, COL_SKY, 9);
            lv_obj_set_flex_flow(g_ui.hero_icon_box, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(g_ui.hero_icon_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            g_ui.hero_icon_lbl = create_label(g_ui.hero_icon_box, LV_SYMBOL_GPS, FONT_LG, COL_SKY);

            lv_obj_t *title_col = lv_obj_create(mid);
            reset_container(title_col);
            lv_obj_set_size(title_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(title_col, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_style_pad_row(title_col, 4, 0);

            g_ui.hero_title_lbl  = create_label(title_col, "MOUSE", FONT_LG, COL_TEXT_PRIMARY);
            g_ui.hero_resume_lbl = create_label(title_col, "RESUME -> MOUSE", FONT_XS, COL_AMBER_LIGHT);
            lv_obj_add_flag(g_ui.hero_resume_lbl, LV_OBJ_FLAG_HIDDEN);

            // Hero Bottom: 3-Step Cycle Track ([ MOUSE ] [ UI ] [ KEYBD ])
            lv_obj_t *track = create_card(g_ui.hero_card, 244, 26, 0x0A0E16, COL_BORDER_SUB, 5);
            lv_obj_set_style_pad_all(track, 2, 0);
            lv_obj_set_flex_flow(track, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(track, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(track, 2, 0);

            const char *step_names[3] = {"MOUSE", "UI", "KEYBD"};
            for (int i = 0; i < 3; i++)
            {
                g_ui.cycle_box[i] = create_card(track, 78, 20, 0x0A0E16, 0x0A0E16, 3);
                lv_obj_set_flex_flow(g_ui.cycle_box[i], LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(g_ui.cycle_box[i], LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                g_ui.cycle_lbl[i] = create_label(g_ui.cycle_box[i], step_names[i], FONT_XS, COL_TEXT_DIM);
            }

            // Contextual Sub-State Strip (264x32)
            lv_obj_t *ctx = create_card(scr, 264, 32, COL_BG_CARD, COL_BORDER, 6);
            lv_obj_set_style_pad_hor(ctx, 8, 0);
            lv_obj_set_flex_flow(ctx, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(ctx, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            lv_obj_t *ctx_left = lv_obj_create(ctx);
            reset_container(ctx_left);
            lv_obj_set_size(ctx_left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(ctx_left, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(ctx_left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(ctx_left, 5, 0);

            g_ui.ctx_dot = create_dot(ctx_left, 5, COL_SKY);
            create_label(ctx_left, "STATE", FONT_XS, COL_TEXT_MUTED);

            g_ui.ctx_lbl = create_label(ctx, "CURSOR READY", FONT_XS, COL_SKY);
            lv_label_set_long_mode(g_ui.ctx_lbl, LV_LABEL_LONG_DOT);
            lv_obj_set_style_text_align(g_ui.ctx_lbl, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_set_width(g_ui.ctx_lbl, 188);
        }

        void build_screen_dev(lv_obj_t *content)
        {
            lv_obj_t *scr = lv_obj_create(content);
            reset_container(scr);
            lv_obj_set_size(scr, 264, 204);
            lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
            lv_obj_set_style_pad_row(scr, 4, 0);
            lv_obj_add_flag(scr, LV_OBJ_FLAG_HIDDEN);
            g_ui.screens[static_cast<uint8_t>(Screen::DEV)] = scr;

            // Top Segmented Switcher Bar (264x24)
            lv_obj_t *tabs = create_card(scr, 264, 24, COL_BG_CARD, COL_BORDER, 5);
            lv_obj_set_style_pad_all(tabs, 2, 0);
            lv_obj_set_flex_flow(tabs, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(tabs, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            const char *tab_titles[2] = {"R - GLOVE 1", "L - GLOVE 2"};
            for (uintptr_t i = 0; i < 2; i++)
            {
                lv_obj_t *t = create_card(tabs, 128, 18, COL_BG_CARD, COL_BG_CARD, 3);
                lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_set_flex_flow(t, LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(t, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                lv_obj_set_style_pad_column(t, 5, 0);
                lv_obj_add_event_cb(t, on_dev_tab_click, LV_EVENT_CLICKED, reinterpret_cast<void *>(i));

                g_ui.dev_tab_dot[i] = create_dot(t, 5, i == 0 ? COL_GREEN : COL_RED);
                g_ui.dev_tab_lbl[i] = create_label(t, tab_titles[i], FONT_XS, COL_TEXT_MUTED);
                g_ui.dev_tab[i] = t;
            }

            // Telemetry Body (264x176)
            lv_obj_t *body = lv_obj_create(scr);
            reset_container(body);
            lv_obj_set_size(body, 264, 176);

            // Analog Sensor Columns Container (264x112 at 0,0)
            lv_obj_t *analog_row = lv_obj_create(body);
            reset_container(analog_row);
            lv_obj_set_pos(analog_row, 0, 0);
            lv_obj_set_size(analog_row, 264, 112);
            lv_obj_set_flex_flow(analog_row, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(analog_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

            lv_obj_t *cols[2] = {nullptr, nullptr};
            const char *col_titles[2] = {"FSR 0-4", "FLEX 5-9"};

            for (int c = 0; c < 2; c++)
            {
                lv_obj_t *col = create_card(analog_row, 130, 112, COL_BG_CARD, COL_BORDER, 5);
                lv_obj_set_style_pad_hor(col, 5, 0);
                lv_obj_set_style_pad_ver(col, 4, 0);
                lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
                lv_obj_set_flex_align(col, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

                lv_obj_t *hdr = lv_obj_create(col);
                reset_container(hdr);
                lv_obj_set_size(hdr, LV_PCT(100), 13);
                lv_obj_set_style_border_color(hdr, hex(COL_BORDER_SUB), 0);
                lv_obj_set_style_border_width(hdr, 1, 0);
                lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, 0);
                lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

                lv_obj_t *h_lbl = create_label(hdr, col_titles[c], FONT_XS, COL_SKY);
                create_label(hdr, "mV", FONT_XS, COL_TEXT_DIM);

                if (c == 0) g_ui.dev_hdr_fsr = h_lbl;
                else        g_ui.dev_hdr_flex = h_lbl;
                cols[c] = col;
            }

            // Build all 10 sensor rows in a single loop (0..4 FSR, 5..9 Flex)
            for (int i = 0; i < 10; i++)
            {
                lv_obj_t *parent_col = (i < 5) ? cols[0] : cols[1];

                lv_obj_t *row = lv_obj_create(parent_col);
                reset_container(row);
                lv_obj_set_size(row, LV_PCT(100), 16);
                lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                lv_obj_set_style_pad_column(row, 4, 0);

                char idx_str[4];
                std::snprintf(idx_str, sizeof(idx_str), "%d", i);
                lv_obj_t *idx_lbl = create_label(row, idx_str, FONT_XS, COL_TEXT_MUTED);
                lv_obj_set_width(idx_lbl, 9);

                lv_obj_t *bar = lv_bar_create(row);
                lv_obj_set_size(bar, 72, 6);
                lv_bar_set_range(bar, 0, ADC_MAX_MV);
                lv_bar_set_value(bar, 0, LV_ANIM_OFF);
                lv_obj_set_style_bg_color(bar, hex(0x0B0F17), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
                lv_obj_set_style_border_color(bar, hex(COL_BORDER_SUB), LV_PART_MAIN);
                lv_obj_set_style_border_width(bar, 1, LV_PART_MAIN);
                lv_obj_set_style_radius(bar, 2, LV_PART_MAIN);
                lv_obj_set_style_bg_color(bar, hex(COL_SKY), LV_PART_INDICATOR);
                lv_obj_set_style_radius(bar, 1, LV_PART_INDICATOR);

                lv_obj_t *val = create_label(row, "0", FONT_XS, COL_TEXT_SECOND);
                lv_obj_set_width(val, 29);
                lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_RIGHT, 0);

                g_ui.sensors[i].bar = bar;
                g_ui.sensors[i].val = val;
            }

            // Bottom IMU Panel (264x60 at 0,116)
            lv_obj_t *imu = create_card(body, 264, 60, COL_BG_CARD, COL_BORDER, 5);
            lv_obj_set_pos(imu, 0, 116);
            lv_obj_set_style_pad_hor(imu, 6, 0);
            lv_obj_set_style_pad_ver(imu, 4, 0);
            lv_obj_set_flex_flow(imu, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(imu, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

            lv_obj_t *imu_hdr = lv_obj_create(imu);
            reset_container(imu_hdr);
            lv_obj_set_size(imu_hdr, LV_PCT(100), LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(imu_hdr, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(imu_hdr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            g_ui.dev_hdr_imu = create_label(imu_hdr, "BNO086 IMU", FONT_XS, COL_SKY);
            create_label(imu_hdr, "QUAT & Z-Y-X EULER", FONT_XS, COL_TEXT_DIM);

            // Quaternion Row (4 boxes)
            lv_obj_t *q_row = lv_obj_create(imu);
            reset_container(q_row);
            lv_obj_set_size(q_row, LV_PCT(100), 17);
            lv_obj_set_flex_flow(q_row, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(q_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            const char *q_keys[4] = {"W", "X", "Y", "Z"};
            for (int i = 0; i < 4; i++)
            {
                lv_obj_t *cell = create_card(q_row, 60, 17, 0x0B0F17, 0x1B2331, 3);
                lv_obj_set_style_pad_hor(cell, 3, 0);
                lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                create_label(cell, q_keys[i], FONT_XS, COL_TEXT_DIM);
                g_ui.imu_quat_lbl[i] = create_label(cell, i == 0 ? "1.00" : "0.00", FONT_XS, COL_TEXT_SECOND);
            }

            // Z-Y-X Euler Row (3 boxes)
            lv_obj_t *e_row = lv_obj_create(imu);
            reset_container(e_row);
            lv_obj_set_size(e_row, LV_PCT(100), 17);
            lv_obj_set_flex_flow(e_row, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(e_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            const char *e_keys[3] = {"YAW", "PIT", "ROL"};
            for (int i = 0; i < 3; i++)
            {
                lv_obj_t *cell = create_card(e_row, 81, 17, 0x0B0F17, 0x1B2331, 3);
                lv_obj_set_style_pad_hor(cell, 4, 0);
                lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                create_label(cell, e_keys[i], FONT_XS, COL_TEXT_DIM);
                g_ui.imu_euler_lbl[i] = create_label(cell, "0.0°", FONT_XS, COL_SKY);
            }

            // Left Glove Offline / Searching Overlay (covers telemetry body)
            g_ui.dev_offline_overlay = create_card(body, 264, 176, COL_BG_ROOT, COL_RED, 5);
            lv_obj_set_pos(g_ui.dev_offline_overlay, 0, 0);
            lv_obj_set_style_bg_opa(g_ui.dev_offline_overlay, LV_OPA_90, 0);
            lv_obj_set_flex_flow(g_ui.dev_offline_overlay, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(g_ui.dev_offline_overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_row(g_ui.dev_offline_overlay, 7, 0);
            lv_obj_add_flag(g_ui.dev_offline_overlay, LV_OBJ_FLAG_HIDDEN);

            g_ui.dev_offline_icon  = create_label(g_ui.dev_offline_overlay, LV_SYMBOL_WARNING, FONT_LG, COL_ROSE);
            g_ui.dev_offline_title = create_label(g_ui.dev_offline_overlay, "LEFT GLOVE OFFLINE", FONT_SM, COL_ROSE);
            g_ui.dev_offline_sub   = create_label(g_ui.dev_offline_overlay, "R <> L DISCONNECTED", FONT_XS, COL_TEXT_MUTED);

            lv_obj_t *btn_open = create_card(g_ui.dev_offline_overlay, 136, 26, 0x182132, COL_SKY, 5);
            lv_obj_add_flag(btn_open, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_flex_flow(btn_open, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(btn_open, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            create_label(btn_open, "COMMS MANAGER ->", FONT_XS, COL_SKY);
            lv_obj_add_event_cb(btn_open, on_open_comms_click, LV_EVENT_CLICKED, nullptr);
        }

        void build_screen_settings(lv_obj_t *content)
        {
            lv_obj_t *scr = lv_obj_create(content);
            reset_container(scr);
            lv_obj_set_size(scr, 264, 204);
            lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
            lv_obj_set_style_pad_row(scr, 8, 0);
            lv_obj_add_flag(scr, LV_OBJ_FLAG_HIDDEN);
            g_ui.screens[static_cast<uint8_t>(Screen::SETTINGS)] = scr;

            // 1. Display Brightness Card (0..100%)
            lv_obj_t *bright_card = create_card(scr, 264, 64, COL_BG_CARD, COL_BORDER, 6);
            lv_obj_set_style_pad_hor(bright_card, 10, 0);
            lv_obj_set_style_pad_ver(bright_card, 9, 0);
            lv_obj_set_flex_flow(bright_card, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(bright_card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

            lv_obj_t *b_hdr = lv_obj_create(bright_card);
            reset_container(b_hdr);
            lv_obj_set_size(b_hdr, LV_PCT(100), LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(b_hdr, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(b_hdr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            create_label(b_hdr, "DISPLAY BRIGHTNESS", FONT_XS, COL_TEXT_SECOND);
            g_ui.bright_val_lbl = create_label(b_hdr, "50%", FONT_XS, COL_SKY);

            g_ui.bright_slider = lv_slider_create(bright_card);
            lv_obj_set_size(g_ui.bright_slider, 240, 8);
            lv_slider_set_range(g_ui.bright_slider, 0, 100);
            lv_slider_set_value(g_ui.bright_slider, g_brightness_pct, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(g_ui.bright_slider, hex(0x1E293B), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(g_ui.bright_slider, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_bg_color(g_ui.bright_slider, hex(COL_SKY), LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(g_ui.bright_slider, hex(COL_SKY), LV_PART_KNOB);
            lv_obj_set_style_border_color(g_ui.bright_slider, hex(COL_BG_ROOT), LV_PART_KNOB);
            lv_obj_set_style_border_width(g_ui.bright_slider, 2, LV_PART_KNOB);
            lv_obj_set_style_pad_all(g_ui.bright_slider, 4, LV_PART_KNOB);
            lv_obj_add_event_cb(g_ui.bright_slider, on_brightness_changed, LV_EVENT_VALUE_CHANGED, nullptr);

            // 2. Comms Manager Quick Access Card
            lv_obj_t *comms_card = create_card(scr, 264, 56, COL_BG_CARD, COL_BORDER, 6);
            lv_obj_add_flag(comms_card, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_pad_hor(comms_card, 10, 0);
            lv_obj_set_flex_flow(comms_card, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(comms_card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_add_event_cb(comms_card, on_open_comms_click, LV_EVENT_CLICKED, nullptr);

            lv_obj_t *c_left = lv_obj_create(comms_card);
            reset_container(c_left);
            lv_obj_set_size(c_left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(c_left, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_style_pad_row(c_left, 3, 0);

            create_label(c_left, "COMMS MANAGER", FONT_XS, COL_TEXT_PRIMARY);
            g_ui.set_comms_summary = create_label(c_left, "LINK FAULT - TAP TO MANAGE", FONT_XS, COL_ROSE);

            lv_obj_t *arrow_box = create_card(comms_card, 28, 24, COL_SKY_BG, COL_SKY, 4);
            lv_obj_set_flex_flow(arrow_box, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(arrow_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            create_label(arrow_box, LV_SYMBOL_RIGHT, FONT_XS, COL_SKY);

            // 3. Desktop Companion Note Card
            lv_obj_t *note = create_card(scr, 264, 68, 0x0D111A, 0x1C2433, 6);
            lv_obj_set_style_pad_all(note, 9, 0);
            lv_obj_set_flex_flow(note, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(note, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
            lv_obj_set_style_pad_row(note, 4, 0);

            create_label(note, "DESKTOP COMPANION", FONT_XS, COL_TEXT_MUTED);
            lv_obj_t *desc = create_label(note, "Cursor speed & advanced tuning\nare configured in the host app.", FONT_XS, COL_TEXT_DIM);
            lv_obj_set_width(desc, 244);
        }

        void build_comms_modal()
        {
            lv_obj_t *backdrop = lv_obj_create(lv_layer_top());
            reset_container(backdrop);
            lv_obj_set_size(backdrop, SCREEN_WIDTH, SCREEN_HEIGHT);
            lv_obj_set_style_bg_color(backdrop, hex(0x05070B), 0);
            lv_obj_set_style_bg_opa(backdrop, LV_OPA_80, 0);
            lv_obj_add_flag(backdrop, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(backdrop, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_event_cb(backdrop, on_comms_backdrop_click, LV_EVENT_CLICKED, nullptr);
            g_ui.modal_comms = backdrop;

            lv_obj_t *panel = create_card(backdrop, 288, 208, COL_BG_MODAL, 0x1F2937, 8);
            lv_obj_center(panel);
            lv_obj_set_style_pad_all(panel, 9, 0);
            lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

            // Header
            lv_obj_t *hdr = lv_obj_create(panel);
            reset_container(hdr);
            lv_obj_set_size(hdr, LV_PCT(100), 26);
            lv_obj_set_style_border_color(hdr, hex(0x1F2937), 0);
            lv_obj_set_style_border_width(hdr, 1, 0);
            lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, 0);
            lv_obj_set_style_pad_bottom(hdr, 4, 0);
            lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            lv_obj_t *hdr_l = lv_obj_create(hdr);
            reset_container(hdr_l);
            lv_obj_set_size(hdr_l, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(hdr_l, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(hdr_l, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(hdr_l, 6, 0);

            g_ui.comms_hdr_dot = create_dot(hdr_l, 7, COL_RED);
            create_label(hdr_l, "COMMS MANAGER", FONT_SM, COL_TEXT_PRIMARY);

            lv_obj_t *close_btn = create_card(hdr, 22, 22, COL_BG_TRACK, 0x2C384F, 4);
            lv_obj_add_flag(close_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_flex_flow(close_btn, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(close_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            create_label(close_btn, LV_SYMBOL_CLOSE, FONT_XS, COL_TEXT_SECOND);
            lv_obj_add_event_cb(close_btn, on_close_comms_click, LV_EVENT_CLICKED, nullptr);

            // 3 Link Status Rows
            lv_obj_t *rows = lv_obj_create(panel);
            reset_container(rows);
            lv_obj_set_size(rows, LV_PCT(100), 114);
            lv_obj_set_flex_flow(rows, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(rows, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

            g_ui.comms_r_recv = create_comms_row(rows, "R <> RECV", COL_SKY,         "Right (G1) <> Receiver");
            g_ui.comms_r_l    = create_comms_row(rows, "R <> L",    COL_TEXT_SECOND, "Right (G1) <> Left (G2)");
            g_ui.comms_l_recv = create_comms_row(rows, "L <> RECV", COL_VIOLET,      "Reported via R <> L");

            // Full-width RESTART COMMS Button
            g_ui.btn_restart_comms = create_card(panel, 270, 30, COL_SKY_BG, COL_SKY, 6);
            lv_obj_add_flag(g_ui.btn_restart_comms, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_flex_flow(g_ui.btn_restart_comms, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(g_ui.btn_restart_comms, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            g_ui.lbl_restart_comms = create_label(g_ui.btn_restart_comms, LV_SYMBOL_REFRESH "  RESTART COMMS", FONT_XS, COL_SKY);
            lv_obj_add_event_cb(g_ui.btn_restart_comms, on_restart_comms_click, LV_EVENT_CLICKED, nullptr);
        }

        void build_battery_modal()
        {
            lv_obj_t *backdrop = lv_obj_create(lv_layer_top());
            reset_container(backdrop);
            lv_obj_set_size(backdrop, SCREEN_WIDTH, SCREEN_HEIGHT);
            lv_obj_set_style_bg_color(backdrop, hex(0x05070B), 0);
            lv_obj_set_style_bg_opa(backdrop, LV_OPA_80, 0);
            lv_obj_add_flag(backdrop, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(backdrop, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_event_cb(backdrop, on_batt_backdrop_click, LV_EVENT_CLICKED, nullptr);
            g_ui.modal_batt = backdrop;

            lv_obj_t *panel = create_card(backdrop, 288, 208, COL_BG_MODAL, 0x1F2937, 8);
            lv_obj_center(panel);
            lv_obj_set_style_pad_all(panel, 9, 0);

            // -----------------------------------------------------------------
            // VIEW A: Dual-Glove Battery Overview
            // -----------------------------------------------------------------
            lv_obj_t *ov = lv_obj_create(panel);
            reset_container(ov);
            lv_obj_set_size(ov, 270, 190);
            lv_obj_set_flex_flow(ov, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(ov, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
            g_ui.batt_view_overview = ov;

            // Overview Header
            lv_obj_t *ov_hdr = lv_obj_create(ov);
            reset_container(ov_hdr);
            lv_obj_set_size(ov_hdr, LV_PCT(100), 26);
            lv_obj_set_style_border_color(ov_hdr, hex(0x1F2937), 0);
            lv_obj_set_style_border_width(ov_hdr, 1, 0);
            lv_obj_set_style_border_side(ov_hdr, LV_BORDER_SIDE_BOTTOM, 0);
            lv_obj_set_style_pad_bottom(ov_hdr, 4, 0);
            lv_obj_set_flex_flow(ov_hdr, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(ov_hdr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            lv_obj_t *ov_hl = lv_obj_create(ov_hdr);
            reset_container(ov_hl);
            lv_obj_set_size(ov_hl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(ov_hl, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(ov_hl, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(ov_hl, 6, 0);
            create_dot(ov_hl, 7, COL_EMERALD);
            create_label(ov_hl, "BATTERY & POWER", FONT_SM, COL_TEXT_PRIMARY);

            lv_obj_t *ov_close = create_card(ov_hdr, 22, 22, COL_BG_TRACK, 0x2C384F, 4);
            lv_obj_add_flag(ov_close, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_flex_flow(ov_close, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(ov_close, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            create_label(ov_close, LV_SYMBOL_CLOSE, FONT_XS, COL_TEXT_SECOND);
            lv_obj_add_event_cb(ov_close, on_close_batt_click, LV_EVENT_CLICKED, nullptr);

            // Right & Left Battery Cards
            g_ui.batt_card[0] = create_batt_card(ov, "RIGHT GLOVE (G1 - HOST)", COL_SKY,
                                                 false, on_batt_card_click, reinterpret_cast<void *>(0));
            g_ui.batt_card[1] = create_batt_card(ov, "LEFT GLOVE (G2)", COL_VIOLET,
                                                 true,  on_batt_card_click, reinterpret_cast<void *>(1));

            // -----------------------------------------------------------------
            // VIEW B: Single-Glove Detailed Metrics (SOC, CAP, CURRENT, SOH)
            // -----------------------------------------------------------------
            lv_obj_t *dt = lv_obj_create(panel);
            reset_container(dt);
            lv_obj_set_size(dt, 270, 190);
            lv_obj_set_flex_flow(dt, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(dt, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
            lv_obj_add_flag(dt, LV_OBJ_FLAG_HIDDEN);
            g_ui.batt_view_detail = dt;

            // Detail Header
            lv_obj_t *dt_hdr = lv_obj_create(dt);
            reset_container(dt_hdr);
            lv_obj_set_size(dt_hdr, LV_PCT(100), 26);
            lv_obj_set_style_border_color(dt_hdr, hex(0x1F2937), 0);
            lv_obj_set_style_border_width(dt_hdr, 1, 0);
            lv_obj_set_style_border_side(dt_hdr, LV_BORDER_SIDE_BOTTOM, 0);
            lv_obj_set_style_pad_bottom(dt_hdr, 4, 0);
            lv_obj_set_flex_flow(dt_hdr, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(dt_hdr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            lv_obj_t *dt_hl = lv_obj_create(dt_hdr);
            reset_container(dt_hl);
            lv_obj_set_size(dt_hl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(dt_hl, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(dt_hl, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(dt_hl, 6, 0);

            lv_obj_t *back_btn = create_card(dt_hl, 22, 22, COL_BG_TRACK, 0x2C384F, 4);
            lv_obj_add_flag(back_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_flex_flow(back_btn, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(back_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            create_label(back_btn, LV_SYMBOL_LEFT, FONT_XS, COL_TEXT_PRIMARY);
            lv_obj_add_event_cb(back_btn, on_batt_back_click, LV_EVENT_CLICKED, nullptr);

            g_ui.detail_title = create_label(dt_hl, "RIGHT GLOVE (G1)", FONT_XS, COL_SKY);

            lv_obj_t *dt_hr = lv_obj_create(dt_hdr);
            reset_container(dt_hr);
            lv_obj_set_size(dt_hr, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(dt_hr, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(dt_hr, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(dt_hr, 5, 0);

            g_ui.detail_state_pill = create_card(dt_hr, LV_SIZE_CONTENT, 18, COL_SLATE_BG, COL_TEXT_DIM, 4);
            lv_obj_set_style_pad_hor(g_ui.detail_state_pill, 5, 0);
            lv_obj_set_flex_flow(g_ui.detail_state_pill, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(g_ui.detail_state_pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            g_ui.detail_state_lbl = create_label(g_ui.detail_state_pill, "DISCHG", FONT_XS, COL_TEXT_SECOND);

            lv_obj_t *dt_close = create_card(dt_hr, 22, 22, COL_BG_TRACK, 0x2C384F, 4);
            lv_obj_add_flag(dt_close, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_flex_flow(dt_close, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(dt_close, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            create_label(dt_close, LV_SYMBOL_CLOSE, FONT_XS, COL_TEXT_SECOND);
            lv_obj_add_event_cb(dt_close, on_close_batt_click, LV_EVENT_CLICKED, nullptr);

            // Compact Full-Width SOC Bar
            g_ui.detail_bar = lv_bar_create(dt);
            lv_obj_set_size(g_ui.detail_bar, 270, 6);
            lv_bar_set_range(g_ui.detail_bar, 0, 100);
            lv_bar_set_value(g_ui.detail_bar, 100, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(g_ui.detail_bar, hex(COL_BG_TRACK), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(g_ui.detail_bar, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_radius(g_ui.detail_bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
            lv_obj_set_style_bg_color(g_ui.detail_bar, hex(COL_EMERALD), LV_PART_INDICATOR);
            lv_obj_set_style_radius(g_ui.detail_bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);

            // 2x2 Metric Grid (2 rows of 2 cards)
            lv_obj_t *row1 = lv_obj_create(dt);
            reset_container(row1);
            lv_obj_set_size(row1, 270, 62);
            lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            create_detail_cell(row1, "SOC",      "100.0%",   "MAX17260 RepSOC", &g_ui.detail_soc, nullptr);
            create_detail_cell(row1, "CAPACITY", "2000 mAh", "of 2000 mAh nom.", &g_ui.detail_cap, &g_ui.detail_cap_sub);

            lv_obj_t *row2 = lv_obj_create(dt);
            reset_container(row2);
            lv_obj_set_size(row2, 270, 62);
            lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(row2, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            create_detail_cell(row2, "CHARGE CURRENT", "0 mA", "DISCHARGING", &g_ui.detail_curr, &g_ui.detail_curr_sub);
            create_detail_cell(row2, "SOH",            "--%",  "PLACEHOLDER", &g_ui.detail_soh,  &g_ui.detail_soh_sub);
        }

        // =====================================================================
        // 10. DIRTY-CHECKED UPDATE ROUTINES
        // =====================================================================

        void apply_top_dot(lv_obj_t *dot, ConnStatus s, lv_obj_t *q_lbl = nullptr)
        {
            if (q_lbl != nullptr)
            {
                if (s == ConnStatus::UNKNOWN)
                {
                    lv_obj_set_size(dot, 9, 9);
                    lv_obj_set_style_bg_color(dot, hex(COL_TEXT_DIM), 0);
                    lv_obj_clear_flag(q_lbl, LV_OBJ_FLAG_HIDDEN);
                    return;
                }
                lv_obj_set_size(dot, 6, 6);
                lv_obj_add_flag(q_lbl, LV_OBJ_FLAG_HIDDEN);
            }

            uint32_t col = COL_RED;
            if (s == ConnStatus::CONNECTED)      col = COL_GREEN;
            else if (s == ConnStatus::SEARCHING) col = COL_AMBER;
            lv_obj_set_style_bg_color(dot, hex(col), 0);
        }

        void apply_comms_badge(CommsRowWidgets &w, ConnStatus s)
        {
            switch (s)
            {
                case ConnStatus::CONNECTED:
                    lv_label_set_text(w.badge_lbl, "CONNECTED");
                    lv_obj_set_style_text_color(w.badge_lbl, hex(COL_EMERALD), 0);
                    lv_obj_set_style_bg_color(w.badge_box, hex(COL_EMERALD_BG), 0);
                    lv_obj_set_style_border_color(w.badge_box, hex(COL_EMERALD), 0);
                    break;
                case ConnStatus::SEARCHING:
                    lv_label_set_text(w.badge_lbl, "SEARCHING...");
                    lv_obj_set_style_text_color(w.badge_lbl, hex(COL_AMBER_LIGHT), 0);
                    lv_obj_set_style_bg_color(w.badge_box, hex(COL_AMBER_BG), 0);
                    lv_obj_set_style_border_color(w.badge_box, hex(COL_AMBER), 0);
                    break;
                case ConnStatus::UNKNOWN:
                    lv_label_set_text(w.badge_lbl, "UNKNOWN");
                    lv_obj_set_style_text_color(w.badge_lbl, hex(COL_TEXT_SECOND), 0);
                    lv_obj_set_style_bg_color(w.badge_box, hex(COL_SLATE_BG), 0);
                    lv_obj_set_style_border_color(w.badge_box, hex(COL_TEXT_DIM), 0);
                    break;
                case ConnStatus::DISCONNECTED:
                default:
                    lv_label_set_text(w.badge_lbl, "DISCONNECTED");
                    lv_obj_set_style_text_color(w.badge_lbl, hex(COL_ROSE), 0);
                    lv_obj_set_style_bg_color(w.badge_box, hex(COL_RED_BG), 0);
                    lv_obj_set_style_border_color(w.badge_box, hex(COL_RED), 0);
                    break;
            }
        }

        void update_mode_section(const UIState &state)
        {
            const uint8_t m = mode_idx(state.active_mode);
            const uint8_t pre = mode_idx(state.pre_rest_mode);
            const ModeVisuals &vis = MODE_VIS[m];
            const bool is_rest = (state.active_mode == GestureEngine::InputMode::REST);
            const char *sub_txt = (state.mode_substate != nullptr && state.mode_substate[0] != '\0')
                                      ? state.mode_substate
                                      : vis.default_substate;

            const bool mode_changed = !g_has_prev ||
                                      state.active_mode != g_prev_mode ||
                                      state.pre_rest_mode != g_prev_pre_rest;
            const bool sub_changed  = !g_has_prev || (std::strncmp(g_prev_substate, sub_txt, sizeof(g_prev_substate)) != 0);

            if (!mode_changed && !sub_changed) return;

            g_prev_mode     = state.active_mode;
            g_prev_pre_rest = state.pre_rest_mode;
            std::snprintf(g_prev_substate, sizeof(g_prev_substate), "%s", sub_txt);

            if (mode_changed)
            {
                // Top bar pill
                lv_label_set_text_fmt(g_ui.mode_pill_lbl, "M%u: %s", static_cast<unsigned>(m), vis.short_name);
                lv_obj_set_style_text_color(g_ui.mode_pill_lbl, hex(vis.accent_hex), 0);
                lv_obj_set_style_bg_color(g_ui.mode_pill, hex(vis.bg_tint_hex), 0);
                lv_obj_set_style_border_color(g_ui.mode_pill, hex(vis.accent_hex), 0);

                // Home Hero Card
                lv_obj_set_style_bg_color(g_ui.hero_card, hex(vis.card_bg_hex), 0);
                lv_obj_set_style_border_color(g_ui.hero_card, hex(is_rest ? COL_TEXT_DIM : vis.accent_hex), 0);
                lv_obj_set_style_opa(g_ui.hero_card, is_rest ? LV_OPA_80 : LV_OPA_COVER, 0);

                lv_label_set_text_fmt(g_ui.hero_id_lbl, "MODE %u", static_cast<unsigned>(m));
                lv_obj_set_style_text_color(g_ui.hero_id_lbl, hex(is_rest ? COL_TEXT_SECOND : vis.accent_hex), 0);
                lv_obj_set_style_bg_color(g_ui.hero_id_pill, hex(is_rest ? COL_SLATE_BG : vis.bg_tint_hex), 0);
                lv_obj_set_style_border_color(g_ui.hero_id_pill, hex(is_rest ? COL_TEXT_DIM : vis.accent_hex), 0);

                lv_label_set_text(g_ui.hero_icon_lbl, vis.symbol);
                lv_obj_set_style_text_color(g_ui.hero_icon_lbl, hex(vis.accent_hex), 0);
                lv_obj_set_style_bg_color(g_ui.hero_icon_box, hex(is_rest ? COL_SLATE_BG : vis.bg_tint_hex), 0);
                lv_obj_set_style_border_color(g_ui.hero_icon_box, hex(is_rest ? COL_TEXT_DIM : vis.accent_hex), 0);

                lv_label_set_text(g_ui.hero_title_lbl, vis.hero_title);
                lv_obj_set_style_text_color(g_ui.hero_title_lbl, hex(is_rest ? COL_TEXT_SECOND : COL_TEXT_PRIMARY), 0);

                if (is_rest)
                {
                    lv_label_set_text_fmt(g_ui.hero_resume_lbl, "RESUME -> %s", MODE_VIS[pre].short_name);
                    lv_obj_clear_flag(g_ui.hero_resume_lbl, LV_OBJ_FLAG_HIDDEN);
                }
                else
                {
                    lv_obj_add_flag(g_ui.hero_resume_lbl, LV_OBJ_FLAG_HIDDEN);
                }

                // 3-Step Cycle Track
                for (uint8_t i = 0; i < 3; i++)
                {
                    if (!is_rest && i == m)
                    {
                        lv_obj_set_style_bg_color(g_ui.cycle_box[i], hex(vis.bg_tint_hex), 0);
                        lv_obj_set_style_border_color(g_ui.cycle_box[i], hex(vis.accent_hex), 0);
                        lv_obj_set_style_text_color(g_ui.cycle_lbl[i], hex(vis.accent_hex), 0);
                    }
                    else if (is_rest && i == pre)
                    {
                        lv_obj_set_style_bg_color(g_ui.cycle_box[i], hex(COL_AMBER_BG), 0);
                        lv_obj_set_style_border_color(g_ui.cycle_box[i], hex(COL_AMBER), 0);
                        lv_obj_set_style_text_color(g_ui.cycle_lbl[i], hex(COL_AMBER_LIGHT), 0);
                    }
                    else
                    {
                        lv_obj_set_style_bg_color(g_ui.cycle_box[i], hex(0x0A0E16), 0);
                        lv_obj_set_style_border_color(g_ui.cycle_box[i], hex(0x0A0E16), 0);
                        lv_obj_set_style_text_color(g_ui.cycle_lbl[i], hex(COL_TEXT_DIM), 0);
                    }
                }

                lv_obj_set_style_bg_color(g_ui.ctx_dot, hex(vis.accent_hex), 0);
                lv_obj_set_style_text_color(g_ui.ctx_lbl, hex(is_rest ? COL_AMBER_LIGHT : vis.accent_hex), 0);

                refresh_nav_styles();
            }

            if (sub_changed)
            {
                lv_label_set_text(g_ui.ctx_lbl, g_prev_substate);
            }
        }

        void update_comms_section(const CommsStatus &comms)
        {
            if (g_has_prev && comms == g_prev_comms) return;
            g_prev_comms = comms;
            g_force_batt_refresh = true; // Left battery display depends on r_to_l

            const ConnStatus l_eff = effective_l_to_recv(comms);

            // Top bar dots
            apply_top_dot(g_ui.dot_r_recv, comms.r_to_recv);
            apply_top_dot(g_ui.dot_r_l,    comms.r_to_l);
            apply_top_dot(g_ui.dot_l_recv, l_eff, g_ui.dot_l_recv_q);

            // Comms Manager modal badges
            apply_comms_badge(g_ui.comms_r_recv, comms.r_to_recv);
            apply_comms_badge(g_ui.comms_r_l,    comms.r_to_l);
            apply_comms_badge(g_ui.comms_l_recv, l_eff);

            lv_label_set_text(g_ui.comms_l_recv.sub_lbl,
                              (l_eff == ConnStatus::UNKNOWN) ? "R <> L lost (unavailable)" : "Reported via R <> L");

            const bool all_ok  = (comms.r_to_recv == ConnStatus::CONNECTED &&
                                  comms.r_to_l    == ConnStatus::CONNECTED &&
                                  l_eff           == ConnStatus::CONNECTED);
            const bool any_src = (comms.r_to_recv == ConnStatus::SEARCHING ||
                                  comms.r_to_l    == ConnStatus::SEARCHING ||
                                  l_eff           == ConnStatus::SEARCHING);

            if (all_ok)
            {
                lv_obj_set_style_bg_color(g_ui.comms_hdr_dot, hex(COL_GREEN), 0);
                lv_label_set_text(g_ui.set_comms_summary, "ALL 3 LINKS CONNECTED");
                lv_obj_set_style_text_color(g_ui.set_comms_summary, hex(COL_EMERALD), 0);
            }
            else if (any_src)
            {
                lv_obj_set_style_bg_color(g_ui.comms_hdr_dot, hex(COL_AMBER), 0);
                lv_label_set_text(g_ui.set_comms_summary, "SEARCHING FOR LINKS...");
                lv_obj_set_style_text_color(g_ui.set_comms_summary, hex(COL_AMBER_LIGHT), 0);
            }
            else
            {
                lv_obj_set_style_bg_color(g_ui.comms_hdr_dot, hex(COL_RED), 0);
                lv_label_set_text(g_ui.set_comms_summary, "LINK FAULT - TAP TO MANAGE");
                lv_obj_set_style_text_color(g_ui.set_comms_summary, hex(COL_ROSE), 0);
            }

            // Restart Comms button visual feedback while searching
            if (any_src)
            {
                lv_obj_set_style_bg_color(g_ui.btn_restart_comms, hex(COL_AMBER_BG), 0);
                lv_obj_set_style_border_color(g_ui.btn_restart_comms, hex(COL_AMBER), 0);
                lv_obj_set_style_text_color(g_ui.lbl_restart_comms, hex(COL_AMBER_LIGHT), 0);
                lv_label_set_text(g_ui.lbl_restart_comms, LV_SYMBOL_REFRESH "  SEARCHING...");
            }
            else
            {
                lv_obj_set_style_bg_color(g_ui.btn_restart_comms, hex(COL_SKY_BG), 0);
                lv_obj_set_style_border_color(g_ui.btn_restart_comms, hex(COL_SKY), 0);
                lv_obj_set_style_text_color(g_ui.lbl_restart_comms, hex(COL_SKY), 0);
                lv_label_set_text(g_ui.lbl_restart_comms, LV_SYMBOL_REFRESH "  RESTART COMMS");
            }

            // Dev screen Left Glove tab dot & overlay
            uint32_t g2_dot_col = COL_RED;
            if (comms.r_to_l == ConnStatus::CONNECTED)      g2_dot_col = COL_GREEN;
            else if (comms.r_to_l == ConnStatus::SEARCHING) g2_dot_col = COL_AMBER;
            lv_obj_set_style_bg_color(g_ui.dev_tab_dot[1], hex(g2_dot_col), 0);
            refresh_dev_offline_overlay(comms.r_to_l);
        }

        void update_battery_section(const UIState &state)
        {
            const float r_pct = clampf(state.right_batt.pct, 0.0f, 100.0f);
            const float l_pct = clampf(state.left_batt.pct,  0.0f, 100.0f);
            const int16_t r_pct10 = static_cast<int16_t>(std::lround(r_pct * 10.0f));
            const int16_t l_pct10 = static_cast<int16_t>(std::lround(l_pct * 10.0f));

            const bool changed = !g_has_prev || g_force_batt_refresh ||
                                 r_pct10 != g_prev_r_pct10 ||
                                 l_pct10 != g_prev_l_pct10 ||
                                 state.right_batt.capacity_mah != g_prev_r_mah ||
                                 state.left_batt.capacity_mah  != g_prev_l_mah ||
                                 state.right_batt.i_chrg_ma    != g_prev_r_ma  ||
                                 state.left_batt.i_chrg_ma     != g_prev_l_ma;

            if (!changed) return;

            g_force_batt_refresh = false;
            g_prev_r_pct10 = r_pct10;
            g_prev_l_pct10 = l_pct10;
            g_prev_r_mah   = state.right_batt.capacity_mah;
            g_prev_l_mah   = state.left_batt.capacity_mah;
            g_prev_r_ma    = state.right_batt.i_chrg_ma;
            g_prev_l_ma    = state.left_batt.i_chrg_ma;

            const ConnStatus r_l = state.comms.r_to_l;
            const uint32_t c_r   = battery_color_hex(r_pct);
            const uint32_t c_l   = battery_color_hex(l_pct);
            char buf[32];

            // 1. Top Status Bar (Both Gloves)
            lv_obj_set_style_bg_color(g_ui.batt_r_dot, hex(c_r), 0);
            lv_label_set_text_fmt(g_ui.batt_r_pct, "%d%%", static_cast<int>(std::lround(r_pct)));

            if (r_l == ConnStatus::CONNECTED)
            {
                lv_obj_set_style_bg_color(g_ui.batt_l_dot, hex(c_l), 0);
                lv_label_set_text_fmt(g_ui.batt_l_pct, "%d%%", static_cast<int>(std::lround(l_pct)));
                lv_obj_set_style_text_color(g_ui.batt_l_pct, hex(COL_TEXT_PRIMARY), 0);
            }
            else if (r_l == ConnStatus::SEARCHING)
            {
                lv_obj_set_style_bg_color(g_ui.batt_l_dot, hex(COL_AMBER), 0);
                lv_label_set_text(g_ui.batt_l_pct, "..%");
                lv_obj_set_style_text_color(g_ui.batt_l_pct, hex(COL_AMBER_LIGHT), 0);
            }
            else
            {
                lv_obj_set_style_bg_color(g_ui.batt_l_dot, hex(COL_RED), 0);
                lv_label_set_text(g_ui.batt_l_pct, "--");
                lv_obj_set_style_text_color(g_ui.batt_l_pct, hex(COL_TEXT_DIM), 0);
            }

            // 2. Battery Modal Overview — Right Glove (G1)
            std::snprintf(buf, sizeof(buf), "%.1f%%", r_pct);
            lv_label_set_text(g_ui.batt_card[0].pct_lbl, buf);
            lv_obj_set_style_text_color(g_ui.batt_card[0].pct_lbl, hex(c_r), 0);
            lv_bar_set_value(g_ui.batt_card[0].bar, static_cast<int32_t>(std::lround(r_pct)), LV_ANIM_OFF);
            lv_obj_set_style_bg_color(g_ui.batt_card[0].bar, hex(c_r), LV_PART_INDICATOR);
            lv_label_set_text(g_ui.batt_card[0].sub_lbl,
                              (state.right_batt.i_chrg_ma > 0) ? "CHARGING - TAP FOR DETAILS" : "MAX17260 - TAP FOR DETAILS");
            lv_label_set_text_fmt(g_ui.batt_card[0].mah_lbl, "%u / %u mAh",
                                  static_cast<unsigned>(state.right_batt.capacity_mah),
                                  static_cast<unsigned>(DESIGN_CAP_MAH));

            // 3. Battery Modal Overview — Left Glove (G2)
            if (r_l == ConnStatus::CONNECTED)
            {
                lv_obj_set_style_opa(g_ui.batt_card[1].card, LV_OPA_COVER, 0);
                lv_obj_set_style_bg_color(g_ui.batt_card[1].dot, hex(COL_GREEN), 0);
                std::snprintf(buf, sizeof(buf), "%.1f%%", l_pct);
                lv_label_set_text(g_ui.batt_card[1].pct_lbl, buf);
                lv_obj_set_style_text_color(g_ui.batt_card[1].pct_lbl, hex(c_l), 0);
                lv_obj_clear_flag(g_ui.batt_card[1].chev, LV_OBJ_FLAG_HIDDEN);
                lv_bar_set_value(g_ui.batt_card[1].bar, static_cast<int32_t>(std::lround(l_pct)), LV_ANIM_OFF);
                lv_obj_set_style_bg_color(g_ui.batt_card[1].bar, hex(c_l), LV_PART_INDICATOR);
                lv_label_set_text(g_ui.batt_card[1].sub_lbl,
                                  (state.left_batt.i_chrg_ma > 0) ? "CHARGING - TAP FOR DETAILS" : "MAX17260 - TAP FOR DETAILS");
                lv_label_set_text_fmt(g_ui.batt_card[1].mah_lbl, "%u / %u mAh",
                                      static_cast<unsigned>(state.left_batt.capacity_mah),
                                      static_cast<unsigned>(DESIGN_CAP_MAH));
            }
            else
            {
                const bool searching = (r_l == ConnStatus::SEARCHING);
                lv_obj_set_style_opa(g_ui.batt_card[1].card, LV_OPA_70, 0);
                lv_obj_set_style_bg_color(g_ui.batt_card[1].dot, hex(searching ? COL_AMBER : COL_RED), 0);
                lv_label_set_text(g_ui.batt_card[1].pct_lbl, searching ? "SEARCHING" : "OFFLINE");
                lv_obj_set_style_text_color(g_ui.batt_card[1].pct_lbl, hex(searching ? COL_AMBER_LIGHT : COL_ROSE), 0);
                lv_obj_add_flag(g_ui.batt_card[1].chev, LV_OBJ_FLAG_HIDDEN);
                lv_bar_set_value(g_ui.batt_card[1].bar, 0, LV_ANIM_OFF);
                lv_label_set_text(g_ui.batt_card[1].sub_lbl,
                                  searching ? "AWAITING R <> L TELEMETRY" : "R <> L DISCONNECTED");
                lv_label_set_text_fmt(g_ui.batt_card[1].mah_lbl, "-- / %u mAh",
                                      static_cast<unsigned>(DESIGN_CAP_MAH));

                if (g_batt_detail_glove == 1)
                {
                    show_batt_overview();
                }
            }

            // 4. Single-Glove Detailed View (if active)
            if (g_batt_detail_glove == 0 || g_batt_detail_glove == 1)
            {
                const bool is_r = (g_batt_detail_glove == 0);
                const BatteryManager::BatteryDetails &b = is_r ? state.right_batt : state.left_batt;
                const float pct = clampf(b.pct, 0.0f, 100.0f);
                const uint32_t col = battery_color_hex(pct);
                const bool is_charging = (b.i_chrg_ma > 0);
                const bool is_full = (pct >= 99.5f && b.i_chrg_ma >= 0);

                lv_label_set_text(g_ui.detail_title, is_r ? "RIGHT GLOVE (G1)" : "LEFT GLOVE (G2)");
                lv_obj_set_style_text_color(g_ui.detail_title, hex(is_r ? COL_SKY : COL_VIOLET), 0);

                if (is_full)
                {
                    lv_label_set_text(g_ui.detail_state_lbl, "FULL");
                    lv_obj_set_style_text_color(g_ui.detail_state_lbl, hex(COL_EMERALD), 0);
                    lv_obj_set_style_bg_color(g_ui.detail_state_pill, hex(COL_EMERALD_BG), 0);
                    lv_obj_set_style_border_color(g_ui.detail_state_pill, hex(COL_EMERALD), 0);
                }
                else if (is_charging)
                {
                    lv_label_set_text(g_ui.detail_state_lbl, "CHARGING");
                    lv_obj_set_style_text_color(g_ui.detail_state_lbl, hex(COL_EMERALD), 0);
                    lv_obj_set_style_bg_color(g_ui.detail_state_pill, hex(COL_EMERALD_BG), 0);
                    lv_obj_set_style_border_color(g_ui.detail_state_pill, hex(COL_EMERALD), 0);
                }
                else
                {
                    lv_label_set_text(g_ui.detail_state_lbl, "DISCHG");
                    lv_obj_set_style_text_color(g_ui.detail_state_lbl, hex(COL_TEXT_SECOND), 0);
                    lv_obj_set_style_bg_color(g_ui.detail_state_pill, hex(COL_SLATE_BG), 0);
                    lv_obj_set_style_border_color(g_ui.detail_state_pill, hex(COL_TEXT_DIM), 0);
                }

                lv_bar_set_value(g_ui.detail_bar, static_cast<int32_t>(std::lround(pct)), LV_ANIM_OFF);
                lv_obj_set_style_bg_color(g_ui.detail_bar, hex(col), LV_PART_INDICATOR);

                std::snprintf(buf, sizeof(buf), "%.1f%%", pct);
                lv_label_set_text(g_ui.detail_soc, buf);
                lv_obj_set_style_text_color(g_ui.detail_soc, hex(col), 0);

                lv_label_set_text_fmt(g_ui.detail_cap, "%u mAh", static_cast<unsigned>(b.capacity_mah));
                lv_label_set_text_fmt(g_ui.detail_cap_sub, "of %u mAh nom.", static_cast<unsigned>(DESIGN_CAP_MAH));

                lv_label_set_text_fmt(g_ui.detail_curr, "%s%d mA", (b.i_chrg_ma > 0) ? "+" : "", static_cast<int>(b.i_chrg_ma));
                lv_obj_set_style_text_color(g_ui.detail_curr, hex(is_charging ? COL_EMERALD : COL_AMBER_LIGHT), 0);
                lv_label_set_text(g_ui.detail_curr_sub, is_full ? "CHARGE COMPLETE" : (is_charging ? "CHARGING" : "DISCHARGING"));

                // SOH placeholder until BatteryDetails includes SOH / FullCapRep
                lv_label_set_text(g_ui.detail_soh, "--%");
                lv_obj_set_style_text_color(g_ui.detail_soh, hex(COL_TEXT_MUTED), 0);
                lv_label_set_text(g_ui.detail_soh_sub, "SOH PLACEHOLDER");
            }
        }

        void update_dev_section(const UIState &state)
        {
            if (g_active_screen != Screen::DEV) return;
            if (g_active_dev_glove == 1 && state.comms.r_to_l != ConnStatus::CONNECTED) return;

            const GestureEngine::GloveState &g = (g_active_dev_glove == 0) ? state.right_glove : state.left_glove;
            const bool update_text = g_force_dev_text || ((g_dev_frame_counter % DEV_TEXT_DIVIDER) == 0);
            g_dev_frame_counter++;
            g_force_dev_text = false;

            // 30 Hz bars, ~10 Hz numeric mV labels
            for (int i = 0; i < 10; i++)
            {
                uint16_t mv = g.sensors[i];
                if (mv > ADC_MAX_MV) mv = ADC_MAX_MV;
                lv_bar_set_value(g_ui.sensors[i].bar, static_cast<int32_t>(mv), LV_ANIM_OFF);

                if (update_text)
                {
                    lv_label_set_text_fmt(g_ui.sensors[i].val, "%u", static_cast<unsigned>(g.sensors[i]));
                }
            }

            if (update_text)
            {
                char buf[16];
                const float qw = g.q_w;
                const float qx = g.q_x;
                const float qy = g.q_y;
                const float qz = g.q_z;

                std::snprintf(buf, sizeof(buf), "%.2f", qw); lv_label_set_text(g_ui.imu_quat_lbl[0], buf);
                std::snprintf(buf, sizeof(buf), "%.2f", qx); lv_label_set_text(g_ui.imu_quat_lbl[1], buf);
                std::snprintf(buf, sizeof(buf), "%.2f", qy); lv_label_set_text(g_ui.imu_quat_lbl[2], buf);
                std::snprintf(buf, sizeof(buf), "%.2f", qz); lv_label_set_text(g_ui.imu_quat_lbl[3], buf);

                // Compute Z-Y-X Euler angles (deg) only on the 10 Hz Dev screen tick
                constexpr float RAD2DEG = 57.2957795f;
                const float sinr_cosp = 2.0f * (qw * qx + qy * qz);
                const float cosr_cosp = 1.0f - 2.0f * (qx * qx + qy * qy);
                const float roll      = std::atan2(sinr_cosp, cosr_cosp) * RAD2DEG;

                const float sinp  = clampf(2.0f * (qw * qy - qz * qx), -1.0f, 1.0f);
                const float pitch = std::asin(sinp) * RAD2DEG;

                const float siny_cosp = 2.0f * (qw * qz + qx * qy);
                const float cosy_cosp = 1.0f - 2.0f * (qy * qy + qz * qz);
                const float yaw       = std::atan2(siny_cosp, cosy_cosp) * RAD2DEG;

                std::snprintf(buf, sizeof(buf), "%.1f°", yaw);   lv_label_set_text(g_ui.imu_euler_lbl[0], buf);
                std::snprintf(buf, sizeof(buf), "%.1f°", pitch); lv_label_set_text(g_ui.imu_euler_lbl[1], buf);
                std::snprintf(buf, sizeof(buf), "%.1f°", roll);  lv_label_set_text(g_ui.imu_euler_lbl[2], buf);
            }
        }
    } // anonymous namespace

    // =========================================================================
    // 11. PUBLIC GloveUI API
    // =========================================================================

    void init(uint8_t initial_brightness_pct)
    {
        if (g_initialized) return;

        g_brightness_pct = (initial_brightness_pct > 100) ? 100 : initial_brightness_pct;

        lv_obj_t *root = lv_scr_act();
        reset_container(root);
        lv_obj_set_size(root, SCREEN_WIDTH, SCREEN_HEIGHT);
        lv_obj_set_style_bg_color(root, hex(COL_BG_ROOT), 0);
        lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

        build_top_bar(root);
        build_nav_rail(root);

        // Main Content Container (276x216 at 44,24 with 6px padding -> 264x204)
        lv_obj_t *content = lv_obj_create(root);
        reset_container(content);
        lv_obj_set_pos(content, 44, 24);
        lv_obj_set_size(content, 276, 216);
        lv_obj_set_style_pad_all(content, 6, 0);

        build_screen_home(content);
        build_screen_dev(content);
        build_screen_settings(content);

        build_comms_modal();
        build_battery_modal();

        // Register LVGL pointer input driver & crosshair cursor on system layer
        lv_indev_drv_init(&g_indev_drv);
        g_indev_drv.type    = LV_INDEV_TYPE_POINTER;
        g_indev_drv.read_cb = indev_read;
        lv_indev_t *mouse_indev = lv_indev_drv_register(&g_indev_drv);

        lv_obj_t *crosshair = lv_label_create(lv_layer_sys());
        lv_label_set_text(crosshair, LV_SYMBOL_PLUS);
        lv_obj_set_style_text_color(crosshair, hex(COL_ROSE), 0);
        lv_indev_set_cursor(mouse_indev, crosshair);

        // Sync initial slider label, default themes & LVGL tick clock
        lv_label_set_text_fmt(g_ui.bright_val_lbl, "%u%%", static_cast<unsigned>(g_brightness_pct));
        refresh_dev_glove_theme();
        set_screen(Screen::HOME);

        g_last_tick_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
        g_has_prev     = false;
        g_initialized  = true;
    }

    void update(const UIState &state, UICommands &out_cmds)
    {
        if (!g_initialized) return;

        // 1. Feed latest cursor & mode state to LVGL pointer driver
        g_cursor         = state.cursor;
        g_ui_mode_active = (state.active_mode == GestureEngine::InputMode::UI);

        // 2. Dirty-checked UI updates
        update_mode_section(state);
        update_comms_section(state.comms);
        update_battery_section(state);
        update_dev_section(state);
        g_has_prev = true;

        // 3. Advance LVGL tick & process input/rendering
        uint32_t now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
        lv_tick_inc(now_ms - g_last_tick_ms);
        g_last_tick_ms = now_ms;
        lv_timer_handler();

        // 4. Emit 1-shot commands triggered during lv_timer_handler()
        out_cmds.restart_comms_requested = g_pending_restart_comms;
        out_cmds.brightness_changed      = g_pending_brightness;
        out_cmds.brightness_pct          = g_brightness_pct;

        g_pending_restart_comms = false;
        g_pending_brightness    = false;
    }

    Screen active_screen()
    {
        return g_active_screen;
    }

    void set_screen(Screen screen)
    {
        uint8_t idx = static_cast<uint8_t>(screen);
        if (idx >= 3) return;

        g_active_screen = screen;
        for (uint8_t i = 0; i < 3; i++)
        {
            if (g_ui.screens[i] == nullptr) continue;
            if (i == idx) lv_obj_clear_flag(g_ui.screens[i], LV_OBJ_FLAG_HIDDEN);
            else          lv_obj_add_flag(g_ui.screens[i], LV_OBJ_FLAG_HIDDEN);
        }

        if (screen == Screen::DEV)
        {
            g_force_dev_text = true;
        }
        refresh_nav_styles();
    }
}