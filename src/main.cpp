#include <Arduino.h>
#include <TFT_eSPI.h>       // 引入TFT_eSPI库
#include <lvgl.h>           // 引入LVGL库
#include "Pages.h"       // 引入页面管理头文件
#include <Preferences.h> // 引入Preferences库，用于存储设置状态
#include <Wire.h> // 用于I2C
#include <WiFi.h> // 用于WiFi信号强度读取
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#define IIC_SCL 1
#define IIC_SDA 0

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

// ========== 传感器与系统信息全局变量 ==========
float lm75_temp = 0.0f;
float sht20_temp = 0.0f;
float sht20_humi = 0.0f;
float esp32_temp = 0.0f;
uint32_t ram_free = 0;
uint8_t cpu_usage = 0;
int8_t wifi_rssi = 0;

unsigned long last_read_time = 0;

// ========== LM75 读取函数 ==========
float read_lm75_temp() {
    Wire.beginTransmission(0x48); // LM75 I2C地址
    Wire.write(0x00); // 温度寄存器
    Wire.endTransmission(false);
    Wire.requestFrom(0x48, 2);
    if (Wire.available() == 2) {
        uint8_t msb = Wire.read();
        uint8_t lsb = Wire.read();
        int16_t temp = ((msb << 8) | lsb) >> 5;
        return temp * 0.125f;
    }
    return 0.0f;
}

// ========== SHT20 读取函数 ==========
void read_sht20(float &temp, float &humi) {
    // 触发温度测量
    Wire.beginTransmission(0x40);
    Wire.write(0xF3);
    Wire.endTransmission();
    delay(85);
    Wire.requestFrom(0x40, 2);
    if (Wire.available() == 2) {
        uint16_t raw = (Wire.read() << 8) | Wire.read();
        temp = -46.85f + 175.72f * (raw & 0xFFFC) / 65536.0f;
    }
    // 触发湿度测量
    Wire.beginTransmission(0x40);
    Wire.write(0xF5);
    Wire.endTransmission();
    delay(29);
    Wire.requestFrom(0x40, 2);
    if (Wire.available() == 2) {
        uint16_t raw = (Wire.read() << 8) | Wire.read();
        humi = -6.0f + 125.0f * (raw & 0xFFFC) / 65536.0f;
    }
}

// ========== ESP32 内置温度读取 ==========
float read_esp32_temp() {
    return temperatureRead();
}

// ========== RAM/CPU/WiFi信号读取 ==========
uint32_t get_ram_free() {
    return ESP.getFreeHeap();
}
uint8_t get_cpu_usage() {
    // ESP32 Arduino/FreeRTOS 没有直接API获取CPU占用率，这里模拟一个67%~100%之间的随机值
    return 67 + (esp_random() % 34); // 67~100
}
int8_t get_wifi_rssi() {
    return WiFi.RSSI();
}

// ======================== 主程序 ========================

void setup()
{
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    // --- 步骤 1: 初始化硬件与软件库 ---
    tft.begin();
    tft.setRotation(1); 

    // 初始化WiFi（如无连接则进入STA模式）
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.mode(WIFI_STA);
        WiFi.begin();
        // 可选: 等待连接或跳过
    }
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
        create_dashboard();
    } else {
        NewUserPage1_Hello();
    }

    Wire.begin(IIC_SDA, IIC_SCL);

    Serial.println("Setup done, LVGL is running.");
    Init_Connection();
}


void loop()
{
    // LVGL 的心跳
    lv_timer_handler();
    lv_tick_inc(10);
    delay(10);

    // 串口心跳监控
    static unsigned long last_heartbeat = 0;
    static uint32_t heartbeat_counter = 0;
    unsigned long now = millis();
    if (now - last_heartbeat >= 1000) { // 每秒输出一次心跳
        last_heartbeat = now;
        Serial.printf("Heartbeat: %lu, Free RAM: %lu\n", ++heartbeat_counter, ESP.getFreeHeap());
    }

    // 读取按钮状态并处理
    static bool last_button_state = HIGH;
    bool current_button_state = digitalRead(BUTTON_PIN);

    last_button_state = current_button_state;

    static unsigned long last_read = 0;
    if (now - last_read >= 2000) {
        last_read = now;
        lm75_temp = read_lm75_temp();
        read_sht20(sht20_temp, sht20_humi);
        esp32_temp = read_esp32_temp();
        ram_free = get_ram_free();
        cpu_usage = get_cpu_usage();
        wifi_rssi = get_wifi_rssi();
        static unsigned long last_send = 0;
        if (now - last_send >= 10000) {
            last_send = now;
            SendSensorDataToServer(); // 发送传感器数据到服务器
        }
        // 可在此处调用LVGL刷新数据的函数
    }
}