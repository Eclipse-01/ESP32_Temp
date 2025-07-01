#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "Pages.h"
#include <Arduino.h> // 添加此行以解决 digitalRead 未定义问题

// --- 浅色系颜色定义 ---
static const lv_color_t BG_COLOR = LV_COLOR_MAKE(245, 245, 245); // 浅灰色背景
static const lv_color_t TEXT_COLOR = LV_COLOR_MAKE(50, 50, 50);    // 深灰色文本
static const lv_color_t BORDER_COLOR = LV_COLOR_MAKE(220, 220, 220); // 边框颜色
static const lv_color_t ARC_BG_COLOR = LV_COLOR_MAKE(230, 230, 230); // 弧形背景色

// --- 渐变颜色定义 ---
static const lv_color_t TEMP_COLOR_COLD = LV_COLOR_MAKE(0, 120, 200);    // 冷色 (蓝色)
static const lv_color_t TEMP_COLOR_COMFORT = LV_COLOR_MAKE(0, 180, 80);   // 舒适 (绿色)
static const lv_color_t TEMP_COLOR_HOT = LV_COLOR_MAKE(230, 50, 50);     // 热色 (红色)

static const lv_color_t HUMI_COLOR_DRY = LV_COLOR_MAKE(200, 150, 0);     // 干燥 (棕黄色)
static const lv_color_t HUMI_COLOR_COMFORT = LV_COLOR_MAKE(0, 160, 255);  // 舒适 (天蓝色)
static const lv_color_t HUMI_COLOR_WET = LV_COLOR_MAKE(0, 100, 200);     // 潮湿 (深蓝色)

// 全局变量
static lv_obj_t *temp_value_label;
static lv_obj_t *humi_value_label;
static lv_obj_t *temp_arc;
static lv_obj_t *humi_arc;
static lv_obj_t *status_label;
static lv_timer_t *data_timer;

// --- 按钮检测定时器相关 ---
static lv_timer_t *about_btn_timer = NULL;
static bool about_btn_last_state = true;

// 颜色插值辅助函数
static lv_color_t interpolate_color(float value, float min_val, float max_val, lv_color_t start_color, lv_color_t end_color)
{
    // 将值限制在范围内
    if (value < min_val) value = min_val;
    if (value > max_val) value = max_val;

    float ratio = (max_val - min_val == 0) ? 0 : (value - min_val) / (max_val - min_val);

    return lv_color_mix(start_color, end_color, (uint8_t)(ratio * 255.0f));
}

// 数据更新定时器回调函数
static void data_update_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    // 使用实际传感器数据
    float current_temp = sht20_temp; // 以SHT20为主
    float current_humi = sht20_humi;

    // 更新温度显示
    char temp_str[32];
    snprintf(temp_str, sizeof(temp_str), "%.1f°C", current_temp);
    lv_label_set_text(temp_value_label, temp_str);

    // 更新温度弧形进度条 (15-35°C 映射到 0-100)
    int temp_percent = (int)((current_temp - 15.0f) / 20.0f * 100);
    temp_percent = temp_percent < 0 ? 0 : (temp_percent > 100 ? 100 : temp_percent);
    lv_arc_set_value(temp_arc, temp_percent);

    // 根据温度值进行颜色渐变
    const float temp_min_range = 15.0f;
    const float temp_comfort_mid = 23.0f; // 舒适区中点
    const float temp_max_range = 35.0f;
    lv_color_t new_temp_color;
    if (current_temp <= temp_comfort_mid) {
        new_temp_color = interpolate_color(current_temp, temp_min_range, temp_comfort_mid, TEMP_COLOR_COLD, TEMP_COLOR_COMFORT);
    } else {
        new_temp_color = interpolate_color(current_temp, temp_comfort_mid, temp_max_range, TEMP_COLOR_COMFORT, TEMP_COLOR_HOT);
    }
    lv_obj_set_style_arc_color(temp_arc, new_temp_color, LV_PART_INDICATOR);

    // 更新湿度显示
    char humi_str[32];
    snprintf(humi_str, sizeof(humi_str), "%.1f%%", current_humi);
    lv_label_set_text(humi_value_label, humi_str);

    // 更新湿度弧形进度条 (30-80% 映射到 0-100)
    int humi_percent = (int)((current_humi - 30.0f) / 50.0f * 100);
    humi_percent = humi_percent < 0 ? 0 : (humi_percent > 100 ? 100 : humi_percent);
    lv_arc_set_value(humi_arc, humi_percent);

    // 根据湿度值进行颜色渐变
    const float humi_min_range = 30.0f;
    const float humi_comfort_mid = 55.0f; // 舒适区中点
    const float humi_max_range = 80.0f;
    lv_color_t new_humi_color;
    if (current_humi <= humi_comfort_mid) {
        new_humi_color = interpolate_color(current_humi, humi_min_range, humi_comfort_mid, HUMI_COLOR_DRY, HUMI_COLOR_COMFORT);
    } else {
        new_humi_color = interpolate_color(current_humi, humi_comfort_mid, humi_max_range, HUMI_COLOR_COMFORT, HUMI_COLOR_WET);
    }
    lv_obj_set_style_arc_color(humi_arc, new_humi_color, LV_PART_INDICATOR);
}

// 创建弧形仪表盘
static lv_obj_t* create_arc_gauge(lv_obj_t *parent, int x, int y, int size,
                                  lv_color_t color, const char *title)
{
    // 创建弧形进度条
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_set_size(arc, size, size);
    lv_obj_set_pos(arc, x, y);
    lv_arc_set_rotation(arc, 135);      // 起始角度
    lv_arc_set_bg_angles(arc, 0, 270);  // 背景弧
    lv_arc_set_value(arc, 0);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);   // 移除旋钮
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);  // 禁用点击

    // 设置主弧(背景)宽度和颜色
    lv_obj_set_style_arc_width(arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, ARC_BG_COLOR, LV_PART_MAIN);

    // 设置指示弧宽度和颜色
    lv_obj_set_style_arc_width(arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);

    // 创建标题标签
    lv_obj_t *title_label = lv_label_create(parent);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, TEXT_COLOR, 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_align_to(title_label, arc, LV_ALIGN_OUT_TOP_MID, 0, -10);

    return arc;
}

// 创建数值标签
static lv_obj_t* create_value_label(lv_obj_t *parent, lv_obj_t *arc, const char *initial_text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, initial_text);
    lv_obj_set_style_text_color(label, TEXT_COLOR, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_22, 0); // 加大字体
    lv_obj_align_to(label, arc, LV_ALIGN_CENTER, 0, 0);

    return label;
}

// 按钮检测定时器回调函数
static void about_btn_check_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    // 检查物理按钮（短按进入关于页）
    bool btn_state = digitalRead(BUTTON_PIN);
    if (!btn_state && about_btn_last_state) { // 检测到按下（下降沿）
        // 先切换到一个新的空白屏幕，确保所有元素被清除
        lv_obj_t *new_scr = lv_obj_create(NULL);
        lv_scr_load(new_scr);

        // 停止当前页面的定时器，防止资源冲突
        if (data_timer) {
            lv_timer_del(data_timer);
            data_timer = NULL;
        }
        if (about_btn_timer) {
            lv_timer_del(about_btn_timer);
            about_btn_timer = NULL;
        }
        Page_About();
    }
    about_btn_last_state = btn_state;
}

// 主仪表盘创建函数
void create_dashboard(void)
{
    // 清理可能存在的定时器，防止资源冲突
    if (data_timer) {
        lv_timer_del(data_timer);
        data_timer = NULL;
    }
    if (about_btn_timer) {
        lv_timer_del(about_btn_timer);
        about_btn_timer = NULL;
    }

    // 创建新的屏幕
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_scr_load(scr);

    // 设置背景颜色
    lv_obj_set_style_bg_color(scr, BG_COLOR, 0);

    // 创建主容器 (可以省略，直接在屏幕上创建)
    // 为了保持结构，这里保留，但设为透明
    lv_obj_t *main_cont = lv_obj_create(scr);
    lv_obj_set_size(main_cont, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(main_cont, 0, 0);
    lv_obj_set_style_bg_opa(main_cont, LV_OPA_TRANSP, 0); // 设置背景透明
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_set_style_pad_all(main_cont, 10, 0);

    // 创建标题
    lv_obj_t *title = lv_label_create(main_cont);
    lv_label_set_text(title, "Environment Monitor");
    lv_obj_set_style_text_color(title, TEXT_COLOR, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // 创建温度弧形仪表盘 (左)
    temp_arc = create_arc_gauge(main_cont, 30, 60, 120, TEMP_COLOR_COMFORT, "Temperature");
    temp_value_label = create_value_label(main_cont, temp_arc, "25.8°C");

    // 创建湿度弧形仪表盘 (右)
    humi_arc = create_arc_gauge(main_cont, 170, 60, 120, HUMI_COLOR_COMFORT, "Humidity");
    humi_value_label = create_value_label(main_cont, humi_arc, "55.2%");

    // 创建图例容器
    lv_obj_t *legend_cont = lv_obj_create(main_cont);
    lv_obj_set_size(legend_cont, 280, 30);
    lv_obj_set_style_bg_color(legend_cont, lv_color_white(), 0);
    lv_obj_set_style_border_width(legend_cont, 1, 0);
    lv_obj_set_style_border_color(legend_cont, BORDER_COLOR, 0);
    lv_obj_set_style_radius(legend_cont, 8, 0);
    lv_obj_align(legend_cont, LV_ALIGN_BOTTOM_MID, 0, -5);

    // 温度范围标签
    lv_obj_t *temp_range = lv_label_create(legend_cont);
    lv_label_set_text(temp_range, "Temp: 15-35°C");
    lv_obj_set_style_text_color(temp_range, lv_color_darken(TEMP_COLOR_COMFORT, 20), 0);
    lv_obj_set_style_text_font(temp_range, &lv_font_montserrat_12, 0);
    lv_obj_align(temp_range, LV_ALIGN_LEFT_MID, 15, 0);

    // 湿度范围标签
    lv_obj_t *humi_range = lv_label_create(legend_cont);
    lv_label_set_text(humi_range, "Humi: 30-80%");
    lv_obj_set_style_text_color(humi_range, lv_color_darken(HUMI_COLOR_COMFORT, 20), 0);
    lv_obj_set_style_text_font(humi_range, &lv_font_montserrat_12, 0);
    lv_obj_align(humi_range, LV_ALIGN_RIGHT_MID, -15, 0);

    // 创建状态信息标签并赋值给全局变量
    status_label = lv_label_create(main_cont);
    lv_obj_align_to(status_label, legend_cont, LV_ALIGN_OUT_BOTTOM_MID, 0, 10); // 放置在图例下方
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(status_label, TEXT_COLOR, 0); // 初始颜色可以设置为默认文本颜色

    // 创建并启动数据更新定时器 (每2秒更新一次)
    data_timer = lv_timer_create(data_update_timer_cb, 2000, NULL);

    // 启动物理按钮检测定时器（50ms轮询）
    if (!about_btn_timer) {
        about_btn_timer = lv_timer_create(about_btn_check_timer_cb, 50, NULL);
    }
}
