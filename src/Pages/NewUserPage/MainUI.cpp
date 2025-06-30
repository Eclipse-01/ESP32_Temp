#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// 屏幕尺寸定义
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// 全局变量
static lv_obj_t *temp_label;
static lv_obj_t *humi_label;
static lv_obj_t *temp_arc;
static lv_obj_t *humi_arc;
static lv_obj_t *temp_value_label;
static lv_obj_t *humi_value_label;
static lv_obj_t *status_label;
static lv_timer_t *data_timer;

// 模拟数据变量
static float current_temp = 25.0f;
static float current_humi = 50.0f;
static float temp_trend = 0.1f;
static float humi_trend = 0.2f;

// 颜色定义
static const lv_color_t TEMP_COLOR = LV_COLOR_MAKE(255, 100, 100);  // 橙红色
static const lv_color_t HUMI_COLOR = LV_COLOR_MAKE(100, 150, 255);  // 蓝色
static const lv_color_t BG_COLOR = LV_COLOR_MAKE(30, 30, 40);       // 深灰蓝色

// 模拟传感器数据生成函数
static void generate_sensor_data(void)
{
    // 温度模拟 (15-35°C)
    current_temp += temp_trend;
    if (current_temp > 35.0f) {
        current_temp = 35.0f;
        temp_trend = -0.1f - (rand() % 20) * 0.01f;
    } else if (current_temp < 15.0f) {
        current_temp = 15.0f;
        temp_trend = 0.1f + (rand() % 20) * 0.01f;
    }
    
    // 添加小幅随机波动
    current_temp += (rand() % 21 - 10) * 0.02f;
    
    // 湿度模拟 (30-80%)
    current_humi += humi_trend;
    if (current_humi > 80.0f) {
        current_humi = 80.0f;
        humi_trend = -0.2f - (rand() % 30) * 0.01f;
    } else if (current_humi < 30.0f) {
        current_humi = 30.0f;
        humi_trend = 0.2f + (rand() % 30) * 0.01f;
    }
    
    // 添加小幅随机波动
    current_humi += (rand() % 21 - 10) * 0.03f;
}

// 数据更新定时器回调函数
static void data_update_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    
    // 生成新的模拟数据
    generate_sensor_data();
    
    // 更新温度显示
    char temp_str[16];
    snprintf(temp_str, sizeof(temp_str), "%.1f°C", current_temp);
    lv_label_set_text(temp_value_label, temp_str);
    
    // 更新温度弧形进度条 (15-35°C 映射到 0-100)
    int temp_percent = (int)((current_temp - 15.0f) / 20.0f * 100);
    temp_percent = temp_percent < 0 ? 0 : (temp_percent > 100 ? 100 : temp_percent);
    lv_arc_set_value(temp_arc, temp_percent);
    
    // 更新湿度显示
    char humi_str[16];
    snprintf(humi_str, sizeof(humi_str), "%.1f%%", current_humi);
    lv_label_set_text(humi_value_label, humi_str);
    
    // 更新湿度弧形进度条 (30-80% 映射到 0-100)
    int humi_percent = (int)((current_humi - 30.0f) / 50.0f * 100);
    humi_percent = humi_percent < 0 ? 0 : (humi_percent > 100 ? 100 : humi_percent);
    lv_arc_set_value(humi_arc, humi_percent);
    
    // 更新状态信息
    char status_str[64];
    if (current_temp > 30.0f && current_humi > 70.0f) {
        snprintf(status_str, sizeof(status_str), "Status: Hot and Humid");
        lv_obj_set_style_text_color(status_label, lv_color_make(255, 150, 100), 0);
    } else if (current_temp < 20.0f) {
        snprintf(status_str, sizeof(status_str), "Status: Cold");
        lv_obj_set_style_text_color(status_label, lv_color_make(150, 200, 255), 0);
    } else if (current_humi < 40.0f) {
        snprintf(status_str, sizeof(status_str), "Status: Dry");
        lv_obj_set_style_text_color(status_label, lv_color_make(255, 200, 100), 0);
    } else {
        snprintf(status_str, sizeof(status_str), "Status: Comfortable");
        lv_obj_set_style_text_color(status_label, lv_color_make(100, 255, 150), 0);
    }
    lv_label_set_text(status_label, status_str);
}

// 创建弧形仪表盘
static lv_obj_t* create_arc_gauge(lv_obj_t *parent, int x, int y, int size, 
                                  lv_color_t color, const char *title)
{
    // 创建弧形进度条
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_set_size(arc, size, size);
    lv_obj_set_pos(arc, x, y);
    lv_arc_set_rotation(arc, 135);  // 起始角度
    lv_arc_set_bg_angles(arc, 0, 270);  // 背景弧
    lv_arc_set_value(arc, 0);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);   // 移除旋钮
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);  // 禁用点击
    
    // 设置主弧宽度和颜色
    lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_make(60, 60, 70), LV_PART_MAIN);
    
    // 设置指示弧宽度和颜色
    lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
    
    // 创建标题标签
    lv_obj_t *title_label = lv_label_create(parent);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_align_to(title_label, arc, LV_ALIGN_OUT_TOP_MID, 0, -5);
    
    return arc;
}

// 创建数值标签
static lv_obj_t* create_value_label(lv_obj_t *parent, lv_obj_t *arc, const char *initial_text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, initial_text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    lv_obj_align_to(label, arc, LV_ALIGN_CENTER, 0, 0);
    
    return label;
}

// Main dashboard creation function
void create_dashboard(void)
{
    // Get current screen
    lv_obj_t *scr = lv_scr_act();
    
    // Set background color
    lv_obj_set_style_bg_color(scr, BG_COLOR, 0);
    
    // Create main container
    lv_obj_t *main_cont = lv_obj_create(scr);
    lv_obj_set_size(main_cont, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(main_cont, 0, 0);
    lv_obj_set_style_bg_color(main_cont, BG_COLOR, 0);
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_set_style_pad_all(main_cont, 10, 0);
    
    // Create title
    lv_obj_t *title = lv_label_create(main_cont);
    lv_label_set_text(title, "Environment Monitor Dashboard");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    
    // Create temperature arc gauge (left)
    temp_arc = create_arc_gauge(main_cont, 20, 60, 100, TEMP_COLOR, "Temperature");
    temp_value_label = create_value_label(main_cont, temp_arc, "25.0°C");
    
    // Create humidity arc gauge (right)
    humi_arc = create_arc_gauge(main_cont, 180, 60, 100, HUMI_COLOR, "Humidity");
    humi_value_label = create_value_label(main_cont, humi_arc, "50.0%");
    
    // Create status label
    status_label = lv_label_create(main_cont);
    lv_label_set_text(status_label, "Status: Comfortable");
    lv_obj_set_style_text_color(status_label, lv_color_make(100, 255, 150), 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -30);
    
    // Create legend
    lv_obj_t *legend_cont = lv_obj_create(main_cont);
    lv_obj_set_size(legend_cont, 280, 25);
    lv_obj_set_style_bg_color(legend_cont, lv_color_make(40, 40, 50), 0);
    lv_obj_set_style_border_width(legend_cont, 1, 0);
    lv_obj_set_style_border_color(legend_cont, lv_color_make(80, 80, 90), 0);
    lv_obj_set_style_radius(legend_cont, 5, 0);
    lv_obj_align(legend_cont, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    // Temperature range label
    lv_obj_t *temp_range = lv_label_create(legend_cont);
    lv_label_set_text(temp_range, "Temp: 15-35°C");
    lv_obj_set_style_text_color(temp_range, TEMP_COLOR, 0);
    lv_obj_set_style_text_font(temp_range, &lv_font_montserrat_12, 0);
    lv_obj_align(temp_range, LV_ALIGN_LEFT_MID, 10, 0);
    
    // Humidity range label
    lv_obj_t *humi_range = lv_label_create(legend_cont);
    lv_label_set_text(humi_range, "Humi: 30-80%");
    lv_obj_set_style_text_color(humi_range, HUMI_COLOR, 0);
    lv_obj_set_style_text_font(humi_range, &lv_font_montserrat_12, 0);
    lv_obj_align(humi_range, LV_ALIGN_RIGHT_MID, -10, 0);
    
    // Create and start data update timer (update every 2 seconds)
    data_timer = lv_timer_create(data_update_timer_cb, 2000, NULL);
    
    // Initialize data update
    data_update_timer_cb(NULL);
}
