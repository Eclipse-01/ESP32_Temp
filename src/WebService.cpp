#include "WebService.h"
#include <Preferences.h>
#include <HTTPClient.h>

Preferences WiFi_Settings;
#define SERVER_URL "http://192.168.31.228:3000/"

// 新增：后台任务句柄
TaskHandle_t sendDataTaskHandle = NULL;

bool Init_Connection() {
    // 读取WiFi配置
    WiFi_Settings.begin("wifi-creds", false);
    String ssid = WiFi_Settings.getString("ssid", "");
    String password = WiFi_Settings.getString("password", "");
    WiFi_Settings.end();

    if (ssid.length() > 0 && password.length() > 0) {
        WiFi.begin(ssid.c_str(), password.c_str());
        Serial.printf("Connecting to WiFi SSID: %s\n", ssid.c_str());

        unsigned long startAttemptTime = millis();
        const unsigned long timeout = 10000; // 10秒超时

        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
            delay(100);
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("WiFi connected.");
            return true;
        } else {
            Serial.println("WiFi connection failed.");
            return false;
        }
    } else {
        Serial.println("Cannot connect to WiFi, no credentials found.");
        return false;
    }
}

// 新增：后台任务函数
void SendSensorDataTask(void *parameter) {
    // 拷贝数据，防止主线程变量变化
    float lm75 = lm75_temp;
    float sht20t = sht20_temp;
    float sht20h = sht20_humi;
    float esp32t = esp32_temp;
    int ram = ram_free;
    int cpu = cpu_usage;
    int rssi = wifi_rssi;

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected, cannot send data.");
        sendDataTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    HTTPClient *http = new HTTPClient();
    String url = String(SERVER_URL) + "api/iot-data";
    http->begin(url);
    http->addHeader("Content-Type", "application/json");

    String payload = "{";
    payload += "\"lm75_temp\":" + String(lm75, 2) + ",";
    payload += "\"sht20_temp\":" + String(sht20t, 2) + ",";
    payload += "\"sht20_humi\":" + String(sht20h, 2) + ",";
    payload += "\"esp32_temp\":" + String(esp32t, 2) + ",";
    payload += "\"ram_free\":" + String(ram) + ",";
    payload += "\"cpu_usage\":" + String(cpu) + ",";
    payload += "\"wifi_rssi\":" + String(rssi);
    payload += "}";

    Serial.print("Sending payload: ");
    Serial.println(payload);

    int httpResponseCode = http->POST(payload);

    if (httpResponseCode > 0) {
        Serial.printf("Data sent, response code: %d\n", httpResponseCode);
        String response = http->getString();
        Serial.print("Server response: ");
        Serial.println(response);
    } else {
        Serial.printf("Error sending data: %s\n", http->errorToString(httpResponseCode).c_str());
    }

    http->end();
    delete http; // 释放HTTPClient对象，防止内存泄漏
    sendDataTaskHandle = NULL; // 任务结束，重置句柄
    vTaskDelete(NULL); // 任务完成后自删除
}

// 修改：将原同步函数改为只创建任务
void SendSensorDataToServer() {
    if (sendDataTaskHandle == NULL) {
        xTaskCreate(
            SendSensorDataTask,   // 任务函数
            "SendSensorDataTask",// 名称
            4096,                 // 堆栈大小
            NULL,                 // 参数
            1,                    // 优先级
            &sendDataTaskHandle   // 任务句柄
        );
    } else {
        Serial.println("SendSensorDataTask is already running.");
    }
}