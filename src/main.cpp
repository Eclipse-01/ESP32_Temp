#include <Arduino.h>
#include <TFT_eSPI.h>       // 引入TFT_eSPI库
#include <lvgl.h>           // 引入LVGL库
#include "Pages.h"       // 引入页面管理头文件
#include <Preferences.h> // 引入Preferences库，用于存储设置状态


// ================= LVGL 与 TFT_eSPI 的“胶水”代码部分  =================

TFT_eSPI tft = TFT_eSPI(); // 创建一个TFT对象

// 1. 定义屏幕分辨率
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;

// 2. 定义LVGL的显示缓冲区
static lv_color_t buf_1[screenWidth * 40];

// 3. LVGL的显示刷新回调函数 (最终修正版)
// 将第三个参数的类型从 void* 修改为 unsigned char*
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, unsigned char *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    // 将 color_p 强制类型转换为 (uint16_t*) 来推送颜色数据
    tft.pushColors((uint16_t *)color_p, w * h, true);
    tft.endWrite();

    lv_display_flush_ready(disp);
}

// ======================== 主程序 ========================

void setup()
{
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    // --- 步骤 1: 初始化硬件与软件库 ---
    tft.begin();
    tft.setRotation(1); 

    lv_init();

    // --- 步骤 2: 创建并配置LVGL显示器 ---
    lv_display_t * disp = lv_display_create(screenWidth, screenHeight);
    if (disp == NULL) {
      Serial.println("Failed to create display!");
      while(1);
    }
    
    // 设置刷新回调函数
    lv_display_set_flush_cb(disp, my_disp_flush);

    // 设置缓冲区
    lv_display_set_buffers(disp, buf_1, NULL, sizeof(buf_1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    // --- 步骤 3: 创建 LVGL 用户界面 ---
    Preferences preferences;
    preferences.begin("init", false);
    bool finished = preferences.getBool("finished", false);
    preferences.end();

    if (finished) {
        create_setup_finished_page();
        // create_dashboard();
    } else {
        NewUserPage1_Hello();
    }


    Serial.println("Setup done, LVGL is running.");
}


void loop()
{
    // LVGL 的心跳
    lv_timer_handler();
    lv_tick_inc(10);
    delay(10);
    // 读取按钮状态并处理
    static bool last_button_state = HIGH;
    bool current_button_state = digitalRead(BUTTON_PIN);
    
    last_button_state = current_button_state;
}