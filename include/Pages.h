#ifndef PAGES_H
#define PAGES_H

#include <lvgl.h>
#define BUTTON_PIN 9
// 屏幕尺寸定义
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240


extern bool last_button_state;
extern bool current_button_state;

extern float lm75_temp;
extern float sht20_temp;
extern float sht20_humi;
extern float esp32_temp;
extern uint32_t ram_free;
extern uint8_t cpu_usage;
extern int8_t wifi_rssi;

void NewUserPage1_Hello(void);
void WLAN_Setup_Page(void);
void create_dashboard(void);
void create_setup_finished_page(void);
void SendSensorDataToServer(void);
bool Init_Connection(void);

void Page_About(void);
void Page_Reset(void);

// 页面管理相关函数和变量的声明

#endif