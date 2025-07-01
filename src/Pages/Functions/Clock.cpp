#include "Pages.h"
#include "lvgl.h"
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

// --- Color Definitions (Light Theme) ---
static const lv_color_t BG_COLOR = lv_color_hex(0xF8F9FA);      // Very light gray background
static const lv_color_t TEXT_COLOR = lv_color_hex(0x2C3E50);    // Dark blue-gray text
static const lv_color_t ACCENT_COLOR = lv_color_hex(0x3498DB);  // Blue accent
static const lv_color_t TIME_COLOR = lv_color_hex(0x1A1A1A);    // Almost black for time
static const lv_color_t DATE_COLOR = lv_color_hex(0x5D6D7E);    // Gray for date
static const lv_color_t BORDER_COLOR = lv_color_hex(0xE8E8E8);  // Light border

// --- Global Static Variables ---
static lv_obj_t *clock_screen;    // The clock page screen object
static lv_obj_t *time_label;      // Time display label
static lv_obj_t *date_label;      // Date display label
static lv_obj_t *status_label;    // Status label for NTP sync
static lv_timer_t *clock_timer;   // Timer for clock updates
static lv_timer_t *input_timer;   // Timer for input handling

// NTP and time variables
static bool ntp_synced = false;
static bool ntp_initializing = false;
static unsigned long last_ntp_sync = 0;
static unsigned long ntp_init_start = 0;
static const unsigned long NTP_SYNC_INTERVAL = 3600000; // Sync every hour
static const unsigned long NTP_INIT_TIMEOUT = 10000; // 10 seconds timeout for initial sync

// Input handling variables
static int press_duration = 0;
static bool ignore_initial_press = true;

// --- Constants ---
const int TIMER_INTERVAL_MS = 20;        // Input timer polling interval (ms)
const int CLOCK_UPDATE_INTERVAL_MS = 100; // Clock update interval (ms)
const int CLICK_DURATION_MS_MAX = 300;   // Maximum duration for a single click (ms)

// NTP server configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600;     // GMT+8 for China
const int daylightOffset_sec = 0;        // No daylight saving

// --- Forward Declarations ---
static void create_clock_page(void);
static void clock_update_timer_cb(lv_timer_t *timer);
static void clock_input_timer_cb(lv_timer_t *timer);
static void cleanup_clock_page(void);
static bool is_button_pressed(int pin_number);
static void init_ntp_time(void);
static void update_time_display(void);
static void update_status_display(void);

// --- NTP Time Functions ---

static void init_ntp_time(void)
{
    if (WiFi.status() == WL_CONNECTED && !ntp_initializing) {
        Serial.println("Starting NTP time initialization...");
        ntp_initializing = true;
        ntp_init_start = millis();
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        Serial.println("NTP initialization started, waiting for sync...");
    }
}

static void check_ntp_sync_status(void)
{
    if (!ntp_initializing) {
        return;
    }
    
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        // NTP sync successful
        ntp_synced = true;
        ntp_initializing = false;
        last_ntp_sync = millis();
        Serial.println("NTP time synchronized successfully");
    } else if (millis() - ntp_init_start > NTP_INIT_TIMEOUT) {
        // Timeout reached
        ntp_initializing = false;
        ntp_synced = false;
        Serial.println("NTP initialization timeout");
    }
    // If neither condition is met, still initializing
}

static void update_time_display(void)
{
    struct tm timeinfo;
    static char time_str[16];
    static char date_str[32];
    
    if (ntp_synced && getLocalTime(&timeinfo)) {
        // Format time as HH:MM:SS
        strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);
        
        // Format date
        strftime(date_str, sizeof(date_str), "%Y-%m-%d", &timeinfo);
        
        // Update labels
        lv_label_set_text(time_label, time_str);
        lv_label_set_text(date_label, date_str);
    } else {
        lv_label_set_text(time_label, "--:--:--");
        lv_label_set_text(date_label, "----/--/--");
    }
}

static void update_status_display(void)
{
    if (ntp_initializing) {
        // Show animated dots while initializing
        static int dot_count = 0;
        static unsigned long last_dot_update = 0;
        unsigned long now = millis();
        
        if (now - last_dot_update > 500) { // Update every 500ms
            last_dot_update = now;
            dot_count = (dot_count + 1) % 4; // 0, 1, 2, 3, then back to 0
        }
        
        char status_text[32];
        strcpy(status_text, "Please wait");
        for (int i = 0; i < dot_count; i++) {
            strcat(status_text, ".");
        }
        
        lv_label_set_text(status_label, status_text);
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xF39C12), 0); // Orange for loading
    } else if (WiFi.status() != WL_CONNECTED) {
        lv_label_set_text(status_label, "WiFi Disconnected");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xE74C3C), 0);
    } else if (!ntp_synced) {
        lv_label_set_text(status_label, "Time Not Synced");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xF39C12), 0);
    } else {
        lv_label_set_text(status_label, "Time Synchronized");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x27AE60), 0);
    }
}

// --- UI Creation Functions ---

static void create_clock_page(void)
{
    clock_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(clock_screen, BG_COLOR, 0);
    lv_obj_set_style_pad_all(clock_screen, 20, 0);

    // Title with back arrow
    lv_obj_t *title_container = lv_obj_create(clock_screen);
    lv_obj_remove_style_all(title_container);
    lv_obj_set_size(title_container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(title_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(title_container, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *title_label = lv_label_create(title_container);
    lv_label_set_text(title_label, "Clock");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title_label, TEXT_COLOR, 0);

    // Back arrow (click to return to dashboard)
    lv_obj_t *arrow_icon = lv_label_create(title_container);
    lv_label_set_text(arrow_icon, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(arrow_icon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(arrow_icon, ACCENT_COLOR, 0);

    // Main clock container
    lv_obj_t *clock_container = lv_obj_create(clock_screen);
    lv_obj_set_size(clock_container, lv_pct(90), lv_pct(60));
    lv_obj_align(clock_container, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_color(clock_container, lv_color_white(), 0);
    lv_obj_set_style_border_color(clock_container, BORDER_COLOR, 0);
    lv_obj_set_style_border_width(clock_container, 2, 0);
    lv_obj_set_style_radius(clock_container, 12, 0);
    lv_obj_set_style_shadow_width(clock_container, 8, 0);
    lv_obj_set_style_shadow_color(clock_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(clock_container, LV_OPA_10, 0);

    // Time display (large)
    time_label = lv_label_create(clock_container);
    lv_label_set_text(time_label, "--:--:--");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(time_label, TIME_COLOR, 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -20);

    // Date display (medium)
    date_label = lv_label_create(clock_container);
    lv_label_set_text(date_label, "----/--/--");
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(date_label, DATE_COLOR, 0);
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 25);

    // Status display
    status_label = lv_label_create(clock_screen);
    lv_label_set_text(status_label, "Please wait...");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xF39C12), 0); // Orange for loading
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    // Hint text
    lv_obj_t *hint_label = lv_label_create(clock_screen);
    lv_label_set_text(hint_label, "Click to go to Noodle Timer");
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0x808080), 0);
    lv_obj_align(hint_label, LV_ALIGN_BOTTOM_MID, 0, 15);

    // Initialize NTP
    init_ntp_time();
    
    // Update displays
    update_time_display();
    update_status_display();
}

// --- Timer Callbacks ---

static void clock_update_timer_cb(lv_timer_t *timer)
{
    // Check NTP sync status if initializing
    if (ntp_initializing) {
        check_ntp_sync_status();
    }
    
    // Check if we need to re-sync NTP
    if (!ntp_initializing && millis() - last_ntp_sync > NTP_SYNC_INTERVAL && WiFi.status() == WL_CONNECTED) {
        init_ntp_time();
    }
    
    // Update time display
    update_time_display();
    
    // Update status display more frequently during initialization
    static int status_update_counter = 0;
    int update_frequency = ntp_initializing ? 2 : 50; // Every 200ms during init, every 5s normally
    
    if (++status_update_counter >= update_frequency) {
        update_status_display();
        status_update_counter = 0;
    }
}

static void clock_input_timer_cb(lv_timer_t *timer)
{
    if (is_button_pressed(BUTTON_PIN)) {
        if (ignore_initial_press) {
            return;
        }
        press_duration += TIMER_INTERVAL_MS;
    } else { // Button is released
        if (ignore_initial_press) {
            ignore_initial_press = false;
            press_duration = 0;
            return;
        }
        // Single click navigates to instant noodle countdown
        if (press_duration > 0 && press_duration <= CLICK_DURATION_MS_MAX) {
            cleanup_clock_page();
            Page_InstantNoodleCountDown();
            Serial.println("Click detected, navigating to Instant Noodle Countdown.");
            return;
        }
        // Reset press duration
        press_duration = 0;
    }
}

// --- Cleanup Functions ---

static void cleanup_clock_page(void)
{
    if (clock_timer) {
        lv_timer_del(clock_timer);
        clock_timer = NULL;
    }
    if (input_timer) {
        lv_timer_del(input_timer);
        input_timer = NULL;
    }
    if (clock_screen) {
        lv_obj_del(clock_screen);
        clock_screen = NULL;
        time_label = NULL;
        date_label = NULL;
        status_label = NULL;
    }
    
    // Reset NTP state
    ntp_initializing = false;
    ntp_synced = false;
    
    press_duration = 0;
    ignore_initial_press = true;
}

// --- Public Page Entry Function ---

void Page_Clock(void)
{
    // Clean up any existing pages
    cleanup_clock_page();
    
    // Reset state variables
    press_duration = 0;
    ignore_initial_press = true;
    
    // Create the clock page
    create_clock_page();
    lv_scr_load(clock_screen);
    
    // Start timers
    clock_timer = lv_timer_create(clock_update_timer_cb, CLOCK_UPDATE_INTERVAL_MS, NULL);
    input_timer = lv_timer_create(clock_input_timer_cb, TIMER_INTERVAL_MS, NULL);
    
    Serial.println("Clock page loaded");
}

// --- Helper Functions ---

static bool is_button_pressed(int pin_number)
{
    return (digitalRead(pin_number) == LOW);
}
