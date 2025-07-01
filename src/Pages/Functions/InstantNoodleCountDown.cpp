#include "Pages.h"
#include "lvgl.h"
#include <Arduino.h>

// --- Pin Definitions ---
#define BUZZER_PIN 3

// --- Color Definitions (Light Theme) ---
static const lv_color_t BG_COLOR = lv_color_hex(0xF8F9FA);      // Very light gray background
static const lv_color_t TEXT_COLOR = lv_color_hex(0x2C3E50);    // Dark blue-gray text
static const lv_color_t ACCENT_COLOR = lv_color_hex(0x3498DB);  // Blue accent
static const lv_color_t SUCCESS_COLOR = lv_color_hex(0x27AE60); // Green
static const lv_color_t WARNING_COLOR = lv_color_hex(0xF39C12); // Orange
static const lv_color_t DANGER_COLOR = lv_color_hex(0xE74C3C);  // Red
static const lv_color_t BORDER_COLOR = lv_color_hex(0xE8E8E8);  // Light border

// --- Timer States ---
typedef enum {
    TIMER_STATE_IDLE,       // Timer is not running
    TIMER_STATE_RUNNING,    // Timer is counting down
    TIMER_STATE_FINISHED,   // Timer reached zero
    TIMER_STATE_ALARMING    // Buzzer is active
} timer_state_t;

// --- Input States for State Machine ---
typedef enum {
    INPUT_STATE_WAIT_RELEASE,    // Waiting for button release after page entry
    INPUT_STATE_READY,           // Ready for input
    INPUT_STATE_PRESSING,        // Button is being pressed
    INPUT_STATE_LONG_PRESSING    // Long press detected
} input_state_t;

// --- Global Static Variables ---
static lv_obj_t *noodle_screen;     // The countdown page screen object
static lv_obj_t *time_label;        // Countdown time display
static lv_obj_t *status_label;      // Status message
static lv_obj_t *progress_bar;      // Long press progress bar
static lv_obj_t *progress_label;    // Progress bar label
static lv_obj_t *hint_label;        // Hint text

// Timer management
static lv_timer_t *countdown_timer; // Main countdown timer
static lv_timer_t *input_timer;     // Input handling timer
static lv_timer_t *buzzer_timer;    // Buzzer control timer

// State variables
static timer_state_t timer_state = TIMER_STATE_IDLE;
static input_state_t input_state = INPUT_STATE_WAIT_RELEASE;
static int countdown_seconds = 0;   // Current countdown value
static int press_duration = 0;      // Button press duration in ms
static bool buzzer_active = false;  // Buzzer state
static unsigned long buzzer_start_time = 0; // When buzzer started
static unsigned long last_countdown_update = 0; // For accurate 1-second intervals

// --- Constants ---
const int COUNTDOWN_TOTAL_SECONDS = 180;    // 3 minutes = 180 seconds
const int TIMER_INTERVAL_MS = 20;           // Input timer interval
const int COUNTDOWN_INTERVAL_MS = 100;      // Countdown update interval
const int BUZZER_INTERVAL_MS = 50;          // Buzzer control interval
const int LONG_PRESS_DURATION_MS = 1000;   // Required long press duration
const int CLICK_DURATION_MS_MAX = 300;     // Maximum single click duration
const int BUZZER_DURATION_MS = 30000;      // Buzzer runs for 30 seconds max
const int BUZZER_BEEP_INTERVAL_MS = 500;   // Buzzer beep interval

// --- Forward Declarations ---
static void create_noodle_page(void);
static void countdown_timer_cb(lv_timer_t *timer);
static void input_timer_cb(lv_timer_t *timer);
static void buzzer_timer_cb(lv_timer_t *timer);
static void cleanup_noodle_page(void);
static void start_countdown(void);
static void stop_countdown(void);
static void start_buzzer(void);
static void stop_buzzer(void);
static void update_display(void);
static void format_time(int seconds, char* buffer);
static bool is_button_pressed(int pin_number);
static lv_color_t get_countdown_color(int remaining_seconds);

// --- Helper Functions ---

static bool is_button_pressed(int pin_number)
{
    return (digitalRead(pin_number) == LOW);
}

static void format_time(int seconds, char* buffer)
{
    int minutes = seconds / 60;
    int secs = seconds % 60;
    sprintf(buffer, "%02d:%02d", minutes, secs);
}

static lv_color_t get_countdown_color(int remaining_seconds)
{
    if (remaining_seconds > 60) {
        return SUCCESS_COLOR;  // Green for > 1 minute
    } else if (remaining_seconds > 30) {
        return WARNING_COLOR;  // Orange for 30-60 seconds
    } else {
        return DANGER_COLOR;   // Red for < 30 seconds
    }
}

// --- Buzzer Control ---

static void start_buzzer(void)
{
    buzzer_active = true;
    buzzer_start_time = millis();
    digitalWrite(BUZZER_PIN, HIGH);
    timer_state = TIMER_STATE_ALARMING;
    
    // Start buzzer timer for beeping pattern
    if (!buzzer_timer) {
        buzzer_timer = lv_timer_create(buzzer_timer_cb, BUZZER_INTERVAL_MS, NULL);
    }
    
    Serial.println("Buzzer started - Instant noodles are ready!");
}

static void stop_buzzer(void)
{
    buzzer_active = false;
    digitalWrite(BUZZER_PIN, LOW);
    timer_state = TIMER_STATE_IDLE;
    
    // Stop buzzer timer
    if (buzzer_timer) {
        lv_timer_del(buzzer_timer);
        buzzer_timer = NULL;
    }
    
    Serial.println("Buzzer stopped");
}

// --- Timer Control ---

static void start_countdown(void)
{
    countdown_seconds = COUNTDOWN_TOTAL_SECONDS;
    timer_state = TIMER_STATE_RUNNING;
    last_countdown_update = millis(); // Initialize the timestamp
    Serial.println("Starting 3-minute instant noodle countdown");
}

static void stop_countdown(void)
{
    timer_state = TIMER_STATE_IDLE;
    countdown_seconds = 0;
    Serial.println("Countdown stopped");
}

// --- Display Update ---

static void update_display(void)
{
    static char time_str[16];
    static char status_str[64];
    
    switch (timer_state) {
        case TIMER_STATE_IDLE:
            strcpy(time_str, "03:00");
            strcpy(status_str, "Ready to cook instant noodles");
            lv_obj_set_style_text_color(time_label, TEXT_COLOR, 0);
            lv_obj_set_style_text_color(status_label, TEXT_COLOR, 0);
            lv_label_set_text(hint_label, "Long press to start countdown");
            break;
            
        case TIMER_STATE_RUNNING:
            format_time(countdown_seconds, time_str);
            sprintf(status_str, "Wait for your noodles!");
            lv_obj_set_style_text_color(time_label, get_countdown_color(countdown_seconds), 0);
            lv_obj_set_style_text_color(status_label, get_countdown_color(countdown_seconds), 0);
            lv_label_set_text(hint_label, "Long press to stop countdown");
            break;
            
        case TIMER_STATE_FINISHED:
        case TIMER_STATE_ALARMING:
            strcpy(time_str, "00:00");
            strcpy(status_str, "Instant noodles are ready!");
            lv_obj_set_style_text_color(time_label, DANGER_COLOR, 0);
            lv_obj_set_style_text_color(status_label, DANGER_COLOR, 0);
            lv_label_set_text(hint_label, "Long press to stop alarm");
            break;
    }
    
    lv_label_set_text(time_label, time_str);
    lv_label_set_text(status_label, status_str);
}

// --- UI Creation ---

static void create_noodle_page(void)
{
    noodle_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(noodle_screen, BG_COLOR, 0);
    lv_obj_set_style_pad_all(noodle_screen, 20, 0);

    // Title with back arrow
    lv_obj_t *title_container = lv_obj_create(noodle_screen);
    lv_obj_remove_style_all(title_container);
    lv_obj_set_size(title_container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(title_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(title_container, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *title_label = lv_label_create(title_container);
    lv_label_set_text(title_label, "Instant Noodle Timer");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title_label, TEXT_COLOR, 0);

    // Back arrow (click to return to clock)
    lv_obj_t *arrow_icon = lv_label_create(title_container);
    lv_label_set_text(arrow_icon, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(arrow_icon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(arrow_icon, ACCENT_COLOR, 0);

    // Main timer container
    lv_obj_t *timer_container = lv_obj_create(noodle_screen);
    lv_obj_set_size(timer_container, lv_pct(90), lv_pct(50));
    lv_obj_align(timer_container, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_color(timer_container, lv_color_white(), 0);
    lv_obj_set_style_border_color(timer_container, BORDER_COLOR, 0);
    lv_obj_set_style_border_width(timer_container, 2, 0);
    lv_obj_set_style_radius(timer_container, 12, 0);
    lv_obj_set_style_shadow_width(timer_container, 8, 0);
    lv_obj_set_style_shadow_color(timer_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(timer_container, LV_OPA_10, 0);

    // Countdown time display (large)
    time_label = lv_label_create(timer_container);
    lv_label_set_text(time_label, "03:00");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(time_label, TEXT_COLOR, 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -10);

    // Status message
    status_label = lv_label_create(timer_container);
    lv_label_set_text(status_label, "Ready to cook instant noodles");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(status_label, TEXT_COLOR, 0);
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 25);

    // Long press progress bar (initially hidden)
    progress_bar = lv_bar_create(noodle_screen);
    lv_obj_set_size(progress_bar, lv_pct(80), 8);
    lv_obj_align(progress_bar, LV_ALIGN_BOTTOM_MID, 0, -60);
    lv_bar_set_range(progress_bar, 0, LONG_PRESS_DURATION_MS);
    lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
    lv_obj_set_style_radius(progress_bar, 4, 0);
    lv_obj_set_style_bg_color(progress_bar, ACCENT_COLOR, LV_PART_INDICATOR);
    lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);

    // Progress bar label (initially hidden)
    progress_label = lv_label_create(noodle_screen);
    lv_label_set_text(progress_label, "Hold to start...");
    lv_obj_set_style_text_font(progress_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(progress_label, ACCENT_COLOR, 0);
    lv_obj_align(progress_label, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_add_flag(progress_label, LV_OBJ_FLAG_HIDDEN);

    // Hint text
    hint_label = lv_label_create(noodle_screen);
    lv_label_set_text(hint_label, "Long press to start countdown");
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0x808080), 0);
    lv_obj_align(hint_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    // Initialize display
    update_display();
}

// --- Timer Callbacks ---

static void countdown_timer_cb(lv_timer_t *timer)
{
    if (timer_state == TIMER_STATE_RUNNING) {
        unsigned long now = millis();
        
        // Only update countdown every 1000ms (1 second)
        if (now - last_countdown_update >= 1000) {
            last_countdown_update = now;
            countdown_seconds--;
            
            if (countdown_seconds <= 0) {
                countdown_seconds = 0;
                timer_state = TIMER_STATE_FINISHED;
                start_buzzer();
            }
            
            update_display();
        }
    }
}

static void buzzer_timer_cb(lv_timer_t *timer)
{
    if (timer_state == TIMER_STATE_ALARMING) {
        // Check if buzzer has been running too long
        if (millis() - buzzer_start_time > BUZZER_DURATION_MS) {
            stop_buzzer();
            update_display();
            return;
        }
        
        // Create beeping pattern
        static unsigned long last_beep_time = 0;
        static bool beep_state = false;
        unsigned long now = millis();
        
        if (now - last_beep_time > BUZZER_BEEP_INTERVAL_MS) {
            beep_state = !beep_state;
            digitalWrite(BUZZER_PIN, beep_state ? HIGH : LOW);
            last_beep_time = now;
        }
    }
}

static void input_timer_cb(lv_timer_t *timer)
{
    bool is_pressed = is_button_pressed(BUTTON_PIN);
    
    switch (input_state) {
        case INPUT_STATE_WAIT_RELEASE:
            if (!is_pressed) {
                input_state = INPUT_STATE_READY;
                press_duration = 0;
            }
            break;
            
        case INPUT_STATE_READY:
            if (is_pressed) {
                input_state = INPUT_STATE_PRESSING;
                press_duration = 0;
                
                // Show progress bar for long press indication
                if (timer_state == TIMER_STATE_IDLE || timer_state == TIMER_STATE_RUNNING || timer_state == TIMER_STATE_ALARMING) {
                    lv_obj_clear_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(progress_label, LV_OBJ_FLAG_HIDDEN);
                    
                    if (timer_state == TIMER_STATE_IDLE) {
                        lv_label_set_text(progress_label, "Hold to start...");
                    } else if (timer_state == TIMER_STATE_RUNNING) {
                        lv_label_set_text(progress_label, "Hold to stop...");
                    } else {
                        lv_label_set_text(progress_label, "Hold to stop alarm...");
                    }
                }
            }
            break;
            
        case INPUT_STATE_PRESSING:
            if (is_pressed) {
                press_duration += TIMER_INTERVAL_MS;
                
                // Update progress bar
                if (!lv_obj_has_flag(progress_bar, LV_OBJ_FLAG_HIDDEN)) {
                    lv_bar_set_value(progress_bar, press_duration, LV_ANIM_OFF);
                }
                
                // Check for long press
                if (press_duration >= LONG_PRESS_DURATION_MS) {
                    input_state = INPUT_STATE_LONG_PRESSING;
                    
                    // Handle long press action based on current state
                    switch (timer_state) {
                        case TIMER_STATE_IDLE:
                            start_countdown();
                            break;
                        case TIMER_STATE_RUNNING:
                            stop_countdown();
                            break;
                        case TIMER_STATE_FINISHED:
                        case TIMER_STATE_ALARMING:
                            stop_buzzer();
                            break;
                    }
                    
                    // Hide progress bar
                    lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(progress_label, LV_OBJ_FLAG_HIDDEN);
                    
                    update_display();
                }
            } else {
                // Button released - check if it was a short press (single click)
                if (press_duration > 0 && press_duration <= CLICK_DURATION_MS_MAX) {
                    // Single click detected - navigate back to clock
                    cleanup_noodle_page();
                    create_dashboard();
                    Serial.println("Single click detected, navigating back to Clock page.");
                    return;
                }
                
                // Not a single click, reset to ready state
                input_state = INPUT_STATE_READY;
                press_duration = 0;
                
                // Hide progress bar
                lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(progress_label, LV_OBJ_FLAG_HIDDEN);
                lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
            }
            break;
            
        case INPUT_STATE_LONG_PRESSING:
            if (!is_pressed) {
                input_state = INPUT_STATE_READY;
                press_duration = 0;
            }
            break;
    }
}

// --- Cleanup Function ---

static void cleanup_noodle_page(void)
{
    // Stop all timers
    if (countdown_timer) {
        lv_timer_del(countdown_timer);
        countdown_timer = NULL;
    }
    if (input_timer) {
        lv_timer_del(input_timer);
        input_timer = NULL;
    }
    if (buzzer_timer) {
        lv_timer_del(buzzer_timer);
        buzzer_timer = NULL;
    }
    
    // Stop buzzer
    stop_buzzer();
    
    // Clean up UI
    if (noodle_screen) {
        lv_obj_del(noodle_screen);
        noodle_screen = NULL;
        time_label = NULL;
        status_label = NULL;
        progress_bar = NULL;
        progress_label = NULL;
        hint_label = NULL;
    }
    
    // Reset state
    timer_state = TIMER_STATE_IDLE;
    input_state = INPUT_STATE_WAIT_RELEASE;
    countdown_seconds = 0;
    press_duration = 0;
    buzzer_active = false;
    last_countdown_update = 0;
    
    Serial.println("Instant noodle countdown page cleaned up");
}

// --- Public Entry Function ---

void Page_InstantNoodleCountDown(void)
{
    // Initialize buzzer pin
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    
    // Clean up any existing page
    cleanup_noodle_page();
    
    // Reset state
    timer_state = TIMER_STATE_IDLE;
    input_state = INPUT_STATE_WAIT_RELEASE;
    countdown_seconds = 0;
    press_duration = 0;
    buzzer_active = false;
    last_countdown_update = 0;
    
    // Create UI
    create_noodle_page();
    lv_scr_load(noodle_screen);
    
    // Start timers
    countdown_timer = lv_timer_create(countdown_timer_cb, COUNTDOWN_INTERVAL_MS, NULL);
    input_timer = lv_timer_create(input_timer_cb, TIMER_INTERVAL_MS, NULL);
    
    Serial.println("Instant noodle countdown page loaded");
}
