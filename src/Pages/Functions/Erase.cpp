#include "Pages.h"
#include "lvgl.h"
// If your development environment is Arduino, you need to include Arduino.h
#include <Arduino.h>
#include <nvs_flash.h> // You would need this for the actual NVS clear
#include <Preferences.h> // For NVS operations

// --- Color Definitions (Light Theme) ---
static const lv_color_t BG_COLOR = lv_color_hex(0xF5F5F5);      // Light gray background
static const lv_color_t TEXT_COLOR = lv_color_hex(0x323232);    // Dark gray text
static const lv_color_t ACCENT_COLOR = lv_color_hex(0x3498DB);  // Blue for the progress bar
static const lv_color_t WARN_COLOR = lv_color_hex(0xE74C3C);    // A red for warnings

// --- Global Static Variables ---
static lv_obj_t *about_screen;    // The "About" page screen object
static lv_obj_t *info_screen;     // The "Info" page screen object
static lv_obj_t *reset_screen;    // NEW (Reset Page): The "Reset" page screen object
static lv_obj_t *progress_bar;    // The progress bar widget (reused by pages)

// Timers for each page
static lv_timer_t *about_page_timer;  // Timer for the about page
static lv_timer_t *info_page_timer;   // Timer for the info page
static lv_timer_t *reset_page_timer;  // NEW (Reset Page): Timer for the reset page

static int press_duration = 0;    // Variable to record button press duration (ms)
static bool ignore_initial_press = true;

// --- Constants ---
const int LONG_PRESS_DURATION_MS = 1000; // Required duration for a long press (ms)
const int CLICK_DURATION_MS_MAX = 300;   // Maximum duration for a single click (ms)
const int TIMER_INTERVAL_MS = 20;        // Timer polling interval (ms)

// --- Input State Enum for Info Page ---
typedef enum {
    STATE_WAIT_FOR_INITIAL_RELEASE,
    STATE_READY_FOR_CLICK,
    STATE_BUTTON_IS_PRESSED
} info_page_input_state_t;

// --- Forward Declarations ---
static void create_reset_page(void); // NEW (Reset Page)
static void about_page_timer_cb(lv_timer_t *timer);
static void info_page_timer_cb(lv_timer_t *timer);
static void reset_page_timer_cb(lv_timer_t *timer); // NEW (Reset Page)
static bool is_button_pressed(int pin_number);
static void cleanup_about_page(void);
static void cleanup_info_page(void);
static void cleanup_reset_page(void); // NEW (Reset Page)
static lv_obj_t* create_info_row(lv_obj_t* parent, const char* label_text, const char* value_text);
static void clear_nvs_data(void); // NEW (Reset Page): The actual reset function

// Forward declaration for navigation
void Page_Reset(void); // NEW (Reset Page): Public entry function


// --- Placeholder Functions (for compilation without the full project) ---
#ifndef BUTTON_PIN
#define BUTTON_PIN 0 // Define a default button pin if not already defined
#endif


// NEW (Reset Page): Placeholder for the actual NVS clear function
static void clear_nvs_data(void) {
    Serial.println("**********************************");
    Serial.println("CLEARING ALL NVS DATA...");
    // 使用 Preferences 库将 "finished" 键值设置为 0
    Preferences preferences;
    preferences.begin("init", false);
    preferences.putBool("finished", false); // 设置为 0（false）
    preferences.end();
    Serial.println("NVS Erased. Device will now restart.");
    Serial.println("**********************************");
    delay(1000); // Give serial time to print
    ESP.restart(); // Restart the device after clearing data
}


// --- UI Creation and Helper Functions ---

static lv_obj_t* create_info_row(lv_obj_t* parent, const char* label_text, const char* value_text)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, label_text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label, TEXT_COLOR, 0);

    lv_obj_t *value = lv_label_create(row);
    lv_label_set_text(value, value_text);
    lv_obj_set_style_text_font(value, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(value, TEXT_COLOR, 0);
    
    lv_obj_set_flex_grow(value, 1);
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);

    return row;
}

/**
 * @brief NEW (Reset Page): Creates the "Reset" page.
 * This page provides a way to factory reset the device.
 */
static void create_reset_page(void)
{
    reset_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(reset_screen, BG_COLOR, 0);
    lv_obj_set_style_pad_all(reset_screen, 20, 0);

    // Title
    lv_obj_t *title_label = lv_label_create(reset_screen);
    lv_label_set_text(title_label, "Factory Reset");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title_label, TEXT_COLOR, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 0);

    // Arrow icon (to signify click action goes to dashboard)
    lv_obj_t *arrow_icon = lv_label_create(reset_screen);
    lv_label_set_text(arrow_icon, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(arrow_icon, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(arrow_icon, TEXT_COLOR, 0);
    lv_obj_align(arrow_icon, LV_ALIGN_TOP_RIGHT, 0, 0);

    // Progress Bar
    progress_bar = lv_bar_create(reset_screen);
    lv_obj_set_size(progress_bar, lv_pct(100), 10);
    lv_obj_align(progress_bar, LV_ALIGN_CENTER, 0, 40); // Move down a bit to make room for warning
    lv_bar_set_range(progress_bar, 0, LONG_PRESS_DURATION_MS);
    lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);

    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
    lv_obj_set_style_radius(progress_bar, 5, 0);
    lv_obj_set_style_bg_color(progress_bar, WARN_COLOR, LV_PART_INDICATOR); // Use warning color for progress
    lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);

    // Warning Text
    lv_obj_t *warning_label = lv_label_create(reset_screen);
    lv_label_set_text(warning_label, "This will erase all settings!");
    lv_obj_align_to(warning_label, progress_bar, LV_ALIGN_OUT_TOP_MID, 0, -15);
    lv_obj_set_style_text_font(warning_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(warning_label, TEXT_COLOR, 0);

    // Hint Text
    lv_obj_t *hint_label = lv_label_create(reset_screen);
    lv_label_set_text(hint_label, "Long Press to Confirm Reset");
    lv_obj_align(hint_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0x808080), 0);
}


// --- Timer Callbacks (Input Handling) ---

static void about_page_timer_cb(lv_timer_t *timer)
{
    if (is_button_pressed(BUTTON_PIN)) {
        if (ignore_initial_press) {
            return;
        }
        press_duration += TIMER_INTERVAL_MS;
        if (press_duration > CLICK_DURATION_MS_MAX) {
            lv_obj_clear_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
            lv_bar_set_value(progress_bar, press_duration, LV_ANIM_OFF);
        }
        if (press_duration >= LONG_PRESS_DURATION_MS) {
            cleanup_about_page();
            // 此处原本调用 create_info_page()，已移除
            // 你可以根据需要添加其它页面跳转逻辑
            // 例如：create_dashboard();
            // lv_scr_load(dashboard_screen);
        }
    } else { // Button is released
        if (ignore_initial_press) {
            ignore_initial_press = false;
            press_duration = 0;
            return;
        }
        if (press_duration > 0 && press_duration <= CLICK_DURATION_MS_MAX) {
            cleanup_about_page();
            create_dashboard(); // Navigate to dashboard on click
            Serial.println("Click detected, navigating to Dashboard.");
            return;
        }
        // Reset state if press was released before completion
        press_duration = 0;
        lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
        lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
    }
}

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
                cleanup_info_page();
                Page_About(); // Go back to the About page
                return;
            }
            break;
    }
}

/**
 * @brief NEW (Reset Page): Timer callback for the "Reset" page.
 * Handles long press for reset and single click to go back.
 */
static void reset_page_timer_cb(lv_timer_t *timer)
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
            // Long press complete: execute the reset
            cleanup_reset_page();
            clear_nvs_data(); // This function will clear data and restart
            // Code below this line will not be reached due to restart
        }
    } else { // Button is released
        if (ignore_initial_press) {
            ignore_initial_press = false;
            press_duration = 0;
            return;
        }
        // A single click navigates to the clock page (an escape hatch)
        if (press_duration > 0 && press_duration <= CLICK_DURATION_MS_MAX) {
            cleanup_reset_page();
            Page_Clock();
            Serial.println("Click detected, navigating to Clock page.");
            return;
        }
        // Reset state if press was released before completion
        press_duration = 0;
        lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
        lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
    }
}

// --- Cleanup Functions ---

static void cleanup_about_page(void)
{
    if (about_page_timer) {
        lv_timer_del(about_page_timer);
        about_page_timer = NULL;
    }
    if (about_screen) {
        lv_obj_del(about_screen);
        about_screen = NULL;
        progress_bar = NULL; // Clear pointer as it was part of the screen
    }
    press_duration = 0;
    ignore_initial_press = true;
}

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
}

/**
 * @brief NEW (Reset Page): Cleans up all resources used by the "Reset" page.
 */
static void cleanup_reset_page(void)
{
    if (reset_page_timer) {
        lv_timer_del(reset_page_timer);
        reset_page_timer = NULL;
    }
    if (reset_screen) {
        lv_obj_del(reset_screen);
        reset_screen = NULL;
        progress_bar = NULL; // Clear pointer as it was part of the screen
    }
    press_duration = 0;
    ignore_initial_press = true;
}


// --- Public Page Entry Functions ---
// Page_About() 的实现已移除，请在 About.cpp 中维护实现。
// 如需调用，请确保在头文件中有 void Page_About(void); 的声明。

void Page_Reset(void)
{
    // Clean up other pages before loading this one
    cleanup_about_page();
    cleanup_info_page();
    
    // Reset state variables to ensure predictable behavior
    press_duration = 0;
    ignore_initial_press = true;

    create_reset_page();
    lv_scr_load(reset_screen);
    reset_page_timer = lv_timer_create(reset_page_timer_cb, TIMER_INTERVAL_MS, NULL);
}


// --- Generic Helper Functions ---

static bool is_button_pressed(int pin_number)
{
    return (digitalRead(pin_number) == LOW);
}