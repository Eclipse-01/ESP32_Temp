#ifndef PAGES_H
#define PAGES_H

#include <lvgl.h>
#define BUTTON_PIN 9

extern bool last_button_state;
extern bool current_button_state;

void NewUserPage1_Hello(void);
void WLAN_Setup_Page(void);
void create_dashboard(void);
void do_wifi_scan_once(void);
void create_setup_finished_page(void);

// 页面管理相关函数和变量的声明

#endif