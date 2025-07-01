#include "Pages.h"
#include "lvgl.h"
// If your development environment is Arduino, you need to include Arduino.h
#include <Arduino.h>

// --- Color Definitions (Light Theme) ---
static const lv_color_t BG_COLOR = lv_color_hex(0xF5F5F5);      // Light gray background
static const lv_color_t TEXT_COLOR = lv_color_hex(0x323232);    // Dark gray text
static const lv_color_t ACCENT_COLOR = lv_color_hex(0x3498DB);  // Blue for the progress bar

// --- Global Static Variables ---
static lv_obj_t *about_screen;    // The "About" page screen object
static lv_obj_t *info_screen;     // The "Info" page screen object
static lv_obj_t *progress_bar;    // The progress bar widget

// Timers for each page
static lv_timer_t *about_page_timer;  // Timer for the about page
static lv_timer_t *info_page_timer;   // NEW: Timer for the info page

static int press_duration = 0;    // Variable to record button press duration (ms)
static bool ignore_initial_press = true;

// --- Constants ---
const int LONG_PRESS_DURATION_MS = 1000; // Required duration for a long press (ms)
const int CLICK_DURATION_MS_MAX = 300;   // Maximum duration for a single click (ms)
const int TIMER_INTERVAL_MS = 20;        // Timer polling interval (ms)

// --- Add this enum definition near the top with other declarations ---
typedef enum {
    STATE_WAIT_FOR_INITIAL_RELEASE,
    STATE_READY_FOR_CLICK,
    STATE_BUTTON_IS_PRESSED
} info_page_input_state_t;

// --- Forward Declarations ---
static void create_info_page(void);
static void about_page_timer_cb(lv_timer_t *timer);
static void info_page_timer_cb(lv_timer_t *timer); // NEW
static bool is_button_pressed(int pin_number);
static void cleanup_about_page(void);
static void cleanup_info_page(void); // NEW
static lv_obj_t* create_info_row(lv_obj_t* parent, const char* label_text, const char* value_text); // NEW Helper function

// Forward declaration for navigation
void create_dashboard(void); // Assuming this function exists elsewhere to create the dashboard page.
void Page_About(void);


/**
 * @brief NEW: Helper function to create a standardized "Label: Value" row.
 * This massively simplifies the create_info_page function.
 */
static lv_obj_t* create_info_row(lv_obj_t* parent, const char* label_text, const char* value_text)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row); // Remove default container styles
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Left-side Label
    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, label_text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label, TEXT_COLOR, 0);

    // Right-side Value
    lv_obj_t *value = lv_label_create(row);
    lv_label_set_text(value, value_text);
    lv_obj_set_style_text_font(value, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(value, TEXT_COLOR, 0);
    
    // FIX: This is the correct way to right-align the value
    lv_obj_set_flex_grow(value, 1); // Allow the value label to take all remaining space
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0); // Align the text inside the label to the right

    return row; // Return the row container
}


/**
 * @brief Creates the info page (triggered by a long press)
 * FIX: This function is now much cleaner using the helper.
 */
static void create_info_page(void)
{
    info_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(info_screen, BG_COLOR, 0);

    lv_obj_t *cont = lv_obj_create(info_screen);
    lv_obj_set_size(cont, lv_pct(85), LV_SIZE_CONTENT);
    lv_obj_center(cont);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cont, 10, 0);
    lv_obj_set_style_pad_row(cont, 10, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);

    lv_obj_t *title_label = lv_label_create(cont);
    lv_label_set_text(title_label, "ESP Smart Node");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title_label, TEXT_COLOR, 0);
    lv_obj_set_width(title_label, lv_pct(100));
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_margin_bottom(title_label, 15, 0);
    
    // Use the helper function to create all the info rows
    create_info_row(cont, "Developer:", "Fang Leyang");
    create_info_row(cont, "Student ID:", "1034230231");
    create_info_row(cont, "Software Version:", "V1.7");
    create_info_row(cont, "CPU Info:", "ESP32-C3");
    create_info_row(cont, "RAM Info:", "320KB");
    create_info_row(cont, "ROM Info:", "4MB");
    create_info_row(cont, "System Info:", "PandaOS v1.0");

    lv_obj_t *hint_label = lv_label_create(cont);
    lv_label_set_text(hint_label, "Click to go back");
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0x808080), 0);
    lv_obj_set_width(hint_label, lv_pct(100));
    lv_obj_set_style_text_align(hint_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_margin_top(hint_label, 20, 0);

    // NEW: Create a timer specifically for this page
    info_page_timer = lv_timer_create(info_page_timer_cb, TIMER_INTERVAL_MS, NULL);
}

/**
 * @brief Creates the "About" page (the main page)
 */
static void create_about_page(void)
{
    about_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(about_screen, BG_COLOR, 0);
    lv_obj_set_style_pad_all(about_screen, 20, 0);

    lv_obj_t *about_label = lv_label_create(about_screen);
    lv_label_set_text(about_label, "About");
    lv_obj_set_style_text_font(about_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(about_label, TEXT_COLOR, 0);
    lv_obj_align(about_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *arrow_icon = lv_label_create(about_screen);
    lv_label_set_text(arrow_icon, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(arrow_icon, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(arrow_icon, TEXT_COLOR, 0);
    lv_obj_align(arrow_icon, LV_ALIGN_TOP_RIGHT, 0, 0);

    progress_bar = lv_bar_create(about_screen);
    lv_obj_set_size(progress_bar, lv_pct(100), 10);
    lv_obj_align(progress_bar, LV_ALIGN_CENTER, 0, 0);
    lv_bar_set_range(progress_bar, 0, LONG_PRESS_DURATION_MS);
    lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);

    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
    lv_obj_set_style_radius(progress_bar, 5, 0);
    lv_obj_set_style_bg_color(progress_bar, ACCENT_COLOR, LV_PART_INDICATOR);
    lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *hint_label = lv_label_create(about_screen);
    lv_label_set_text(hint_label, "Long Press to Enter");
    lv_obj_align(hint_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0x808080), 0);
}


/**
 * @brief Timer callback for the "About" page ONLY.
 */
static void about_page_timer_cb(lv_timer_t *timer)
{
    if (is_button_pressed(BUTTON_PIN)) {
        if (ignore_initial_press) {
            return; // Wait for the button release that initiated this page load
        }
        press_duration += TIMER_INTERVAL_MS;
        if (press_duration > CLICK_DURATION_MS_MAX) {
            lv_obj_clear_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
            lv_bar_set_value(progress_bar, press_duration, LV_ANIM_OFF);
        }
        if (press_duration >= LONG_PRESS_DURATION_MS) {
            cleanup_about_page();
            cleanup_info_page();
            // 进入Info页面前重置状态，保证下次还能进入
            press_duration = 0;
            ignore_initial_press = true;
            create_info_page();
            lv_scr_load(info_screen);
        }
    } else { // Button is released
        if (ignore_initial_press) {
            ignore_initial_press = false;
            press_duration = 0;
            return;
        }
        if (press_duration > 0 && press_duration <= CLICK_DURATION_MS_MAX) {
            cleanup_about_page();
            cleanup_info_page();
            // 单击时跳转到dashboard，并重置状态
            press_duration = 0;
            ignore_initial_press = true;
            Page_Reset();
            Serial.println("Click detected, navigating to Reset Page.");
            return;
        }
        press_duration = 0;
        lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
        lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief NEW: Timer callback for the "Info" page ONLY.
 * It just listens for a simple click to go back.
 */
static void info_page_timer_cb(lv_timer_t *timer) {
    static info_page_input_state_t current_state = STATE_WAIT_FOR_INITIAL_RELEASE;
    bool is_pressed_now = is_button_pressed(BUTTON_PIN);
    switch (current_state) {
        case STATE_WAIT_FOR_INITIAL_RELEASE:
            if (!is_pressed_now) {
                current_state = STATE_READY_FOR_CLICK;
            }
            break;
        case STATE_READY_FOR_CLICK:
            if (is_pressed_now) {
                current_state = STATE_BUTTON_IS_PRESSED;
            }
            break;
        case STATE_BUTTON_IS_PRESSED:
            if (!is_pressed_now) {
                current_state = STATE_WAIT_FOR_INITIAL_RELEASE;
                cleanup_info_page();
                // 切换前确保About页面状态干净
                cleanup_about_page();
                Page_About();
                return;
            }
            break;
    }
}


/**
 * @brief Cleans up all resources used by the "About" page.
 */
static void cleanup_about_page(void)
{
    if (about_page_timer) {
        lv_timer_del(about_page_timer);
        about_page_timer = NULL;
    }
    if (about_screen) {
        lv_obj_del(about_screen);
        about_screen = NULL;
        progress_bar = NULL;
    }
    // 确保全局状态变量重置
    press_duration = 0;
    ignore_initial_press = true;
}

/**
 * @brief NEW: Cleans up all resources used by the "Info" page.
 */
static void cleanup_info_page(void)
{
    if (info_page_timer) {
        lv_timer_del(info_page_timer);
        info_page_timer = NULL;
    }
    if (info_screen) {
        lv_obj_del(info_screen);
        info_screen = NULL;
    }
    // 确保全局状态变量重置
    press_duration = 0;
    ignore_initial_press = true;
}


/**
 * @brief Entry function for the "About" page.
 */
void Page_About(void)
{
    // 切换前先清理Info页面，防止状态残留
    cleanup_info_page();
    // 进入About页面时强制重置状态变量，保证长按和单击都能正常响应
    press_duration = 0;
    ignore_initial_press = true;
    create_about_page();
    lv_scr_load(about_screen);
    about_page_timer = lv_timer_create(about_page_timer_cb, TIMER_INTERVAL_MS, NULL);
}

/**
 * @brief Checks if the button is pressed.
 */
static bool is_button_pressed(int pin_number)
{
    return (digitalRead(pin_number) == LOW);
}