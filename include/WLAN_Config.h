#ifndef WLAN_CONFIG_H
#define WLAN_CONFIG_H

#include <lvgl.h>

/**
 * @brief 初始化WLAN配置页面.
 *
 * 该函数会完成以下工作:
 * 1. 启动一个名为 "Spitha" 的 Wi-Fi 接入点 (AP).
 * 2. 启动一个Web服务器，监听在 192.168.4.1.
 * 3. 在LVGL屏幕上显示引导页，包含一个二维码，方便用户连接AP.
 * 4. 创建一个后台任务，用于监听配网状态并更新UI.
 */
void Page_WLAN_Config_Init(void);

#endif // WLAN_CONFIG_H
