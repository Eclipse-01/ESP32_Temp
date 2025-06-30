#include "Pages.h"
#include "lvgl.h"
#include "Wire.h"
#include "Arduino.h"
#include <Preferences.h>

// --- Defines ---
#define SETUP_FINISHED_TITLE_TEXT "Setup Finished"
#define SETUP_FINISHED_SUBTITLE_TEXT "Hope you enjoy your new device"
#define BUTTON_PROMPT_TEXT "Press the button to continue"
#define BUTTON_GPIO 9
#define FIREWORK_PARTICLE_COUNT 8
#define FIREWORK_BURST_COUNT 5

// --- Static variables for UI objects ---
static lv_obj_t *setup_finished_container = NULL;
static lv_obj_t *firework_container = NULL;
static lv_obj_t *success_icon = NULL;
static lv_obj_t *setup_finished_title = NULL;
static lv_obj_t *setup_finished_subtitle = NULL;
static lv_obj_t *prompt_label = NULL;
static lv_timer_t *hardware_button_timer = NULL;
static lv_timer_t *firework_timer = NULL;
static lv_timer_t *debug_timer = NULL;  // 调试定时器
static int firework_burst_counter = 0;

// --- Function Prototypes ---
void create_dashboard(void);
static void create_firework_at(lv_coord_t x, lv_coord_t y);
static void firework_timer_cb(lv_timer_t *timer);
static void debug_timer_cb(lv_timer_t *timer);  // 调试函数

// --- Debug Timer ---
static void debug_timer_cb(lv_timer_t *timer) {
    // 打印调试信息
    LV_LOG_USER("=== DEBUG INFO ===");
    if (setup_finished_container) {
        LV_LOG_USER("Container exists, opacity: %d", lv_obj_get_style_opa(setup_finished_container, 0));
        LV_LOG_USER("Container size: %dx%d", lv_obj_get_width(setup_finished_container), lv_obj_get_height(setup_finished_container));
    }
    if (setup_finished_title) {
        LV_LOG_USER("Title exists, opacity: %d", lv_obj_get_style_opa(setup_finished_title, 0));
        LV_LOG_USER("Title text: %s", lv_label_get_text(setup_finished_title));
    }
    if (success_icon) {
        LV_LOG_USER("Icon exists, text: %s", lv_label_get_text(success_icon));
    }
    
    // 删除调试定时器（只运行一次）
    lv_timer_del(timer);
    debug_timer = NULL;
}

// --- Event Handlers, Timers & Animation Callbacks ---
static void firework_anim_ready_cb(lv_anim_t *a) {
    if(a->var) {
        lv_obj_del((lv_obj_t*)a->var);
    }
}

static void go_to_dashboard(void) {
    if (hardware_button_timer) {
        lv_timer_del(hardware_button_timer);
        hardware_button_timer = NULL;
    }
    if (firework_timer) {
        lv_timer_del(firework_timer);
        firework_timer = NULL;
    }
    if (debug_timer) {
        lv_timer_del(debug_timer);
        debug_timer = NULL;
    }
    
    lv_obj_clean(lv_scr_act());
    create_dashboard();
}

static void check_button_and_goto_dashboard(lv_timer_t *timer) {
    if (digitalRead(BUTTON_GPIO) == LOW) {
        LV_LOG_USER("Hardware button pressed, proceeding to dashboard.");
        go_to_dashboard();
    }
}

// --- UI & Animation Creation ---
static void create_firework_at(lv_coord_t x, lv_coord_t y) {
    if (!firework_container) return;
    
    lv_color_t color = lv_palette_main((lv_palette_t)lv_rand(LV_PALETTE_RED, LV_PALETTE_DEEP_PURPLE));

    for (int i = 0; i < FIREWORK_PARTICLE_COUNT; i++) {
        lv_obj_t *p = lv_obj_create(firework_container);
        lv_obj_remove_style_all(p);
        lv_obj_set_size(p, lv_rand(3, 6), lv_rand(3, 6));
        lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(p, color, 0);
        lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
        lv_obj_set_pos(p, x, y);

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, p);
        lv_anim_set_time(&a, lv_rand(800, 1500));
        lv_anim_set_delay(&a, lv_rand(0, 100));
        
        float angle = (i / (float)FIREWORK_PARTICLE_COUNT) * 2 * PI;
        lv_coord_t dist = lv_rand(40, 100);
        lv_coord_t end_x = x + dist * cos(angle);
        lv_coord_t end_y = y + dist * sin(angle);
        lv_anim_set_values(&a, x, end_x);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
        lv_anim_start(&a);

        lv_anim_set_values(&a, y, end_y);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_anim_start(&a);

        lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_bg_opa);
        lv_anim_set_ready_cb(&a, firework_anim_ready_cb);
        lv_anim_start(&a);
    }
}

static void firework_timer_cb(lv_timer_t *timer) {
    create_firework_at(lv_rand(20, lv_disp_get_hor_res(NULL) - 20), lv_rand(20, lv_disp_get_ver_res(NULL) - 80));
    firework_burst_counter++;

    if (firework_burst_counter >= FIREWORK_BURST_COUNT) {
        lv_timer_del(timer);
        firework_timer = NULL;
    }
}

void create_setup_finished_page(void) {
    LV_LOG_USER("Creating setup finished page...");
    
    // 1. Clean and setup screen
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x111111), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
    
    firework_burst_counter = 0;

    // 2. 创建烟花容器
    firework_container = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(firework_container);
    lv_obj_set_size(firework_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(firework_container, 0, 0);
    lv_obj_move_background(firework_container);

    // 3. 启动烟花
    firework_timer = lv_timer_create(firework_timer_cb, 400, NULL);

    // 4. 创建主要内容容器
    setup_finished_container = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(setup_finished_container);
    lv_obj_set_size(setup_finished_container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(setup_finished_container);
    lv_obj_set_flex_flow(setup_finished_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(setup_finished_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 5. FIX: 使用默认字体创建图标
    success_icon = lv_label_create(setup_finished_container);
    lv_label_set_text(success_icon, LV_SYMBOL_OK);
    // 先尝试默认字体，如果不行再用大字体
    const lv_font_t* icon_font = &lv_font_montserrat_48;
    if (!icon_font) {
        icon_font = LV_FONT_DEFAULT;
        LV_LOG_WARN("Montserrat 48 not available, using default font");
    }
    lv_obj_set_style_text_font(success_icon, icon_font, 0);
    lv_obj_set_style_text_color(success_icon, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_margin_bottom(success_icon, 25, 0);

    // 6. FIX: 使用备用字体方案创建标题
    setup_finished_title = lv_label_create(setup_finished_container);
    lv_label_set_text(setup_finished_title, SETUP_FINISHED_TITLE_TEXT);
    
    // 字体优先级：montserrat_28 -> montserrat_20 -> montserrat_16 -> 默认字体
    const lv_font_t* title_font = NULL;
    if (&lv_font_montserrat_28) {
        title_font = &lv_font_montserrat_28;
    } else if (&lv_font_montserrat_20) {
        title_font = &lv_font_montserrat_20;
        LV_LOG_WARN("Using montserrat_20 instead of montserrat_28");
    } else if (&lv_font_montserrat_16) {
        title_font = &lv_font_montserrat_16;
        LV_LOG_WARN("Using montserrat_16 instead of montserrat_28");
    } else {
        title_font = LV_FONT_DEFAULT;
        LV_LOG_WARN("Using default font for title");
    }
    
    lv_obj_set_style_text_font(setup_finished_title, title_font, 0);
    lv_obj_set_style_text_color(setup_finished_title, lv_color_white(), 0);
    lv_obj_set_style_margin_bottom(setup_finished_title, 10, 0);

    // 7. FIX: 创建副标题，使用更保守的字体
    setup_finished_subtitle = lv_label_create(setup_finished_container);
    lv_label_set_text(setup_finished_subtitle, SETUP_FINISHED_SUBTITLE_TEXT);
    
    const lv_font_t* subtitle_font = NULL;
    if (&lv_font_montserrat_16) {
        subtitle_font = &lv_font_montserrat_16;
    } else if (&lv_font_montserrat_14) {
        subtitle_font = &lv_font_montserrat_14;
        LV_LOG_WARN("Using montserrat_14 instead of montserrat_16");
    } else {
        subtitle_font = LV_FONT_DEFAULT;
        LV_LOG_WARN("Using default font for subtitle");
    }
    
    lv_obj_set_style_text_font(setup_finished_subtitle, subtitle_font, 0);
    lv_obj_set_style_text_color(setup_finished_subtitle, lv_color_hex(0xcccccc), 0);

    // 8. 创建提示标签
    prompt_label = lv_label_create(lv_scr_act());
    lv_label_set_text(prompt_label, BUTTON_PROMPT_TEXT);
    lv_obj_set_style_text_font(prompt_label, LV_FONT_DEFAULT, 0);  // 直接使用默认字体
    lv_obj_set_style_text_color(prompt_label, lv_color_hex(0x888888), 0);
    lv_obj_align(prompt_label, LV_ALIGN_BOTTOM_MID, 0, -20);

    // 9. 确保文本在前景
    lv_obj_move_foreground(setup_finished_container);
    lv_obj_move_foreground(prompt_label);

    // 10. FIX: 先设置为完全不透明，跳过淡入动画进行测试
    lv_obj_set_style_opa(setup_finished_container, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(prompt_label, LV_OPA_COVER, 0);

    // 11. FIX: 添加明显的背景色用于调试
    lv_obj_set_style_bg_color(setup_finished_container, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(setup_finished_container, LV_OPA_30, 0);

    // 12. 创建按钮检测定时器
    hardware_button_timer = lv_timer_create(check_button_and_goto_dashboard, 100, NULL);

    // 13. 创建调试定时器（2秒后输出调试信息）
    debug_timer = lv_timer_create(debug_timer_cb, 2000, NULL);

    // 14. 保存设置
    Preferences preferences;
    preferences.begin("init", false);
    preferences.putBool("finished", true);
    preferences.end();

    // 15. 强制刷新
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);

    LV_LOG_USER("Setup finished page created successfully");
}