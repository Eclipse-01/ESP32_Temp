#include <lvgl.h>
#include "Pages.h"
#include <string.h>
#include <WiFi.h>
#include <ESP32WebServer.h>      // 使用 ESP32WebServer
#include <DNSServer.h>
#include <ArduinoJson.h>         // 使用 ArduinoJson
#include <Wire.h>                // 包含 Wire 库
#include <Preferences.h>         // 用于非易失性存储 (NVS)

// --- 物理按键配置 ---
// !!重要!!: 请根据你的硬件连接修改此引脚
#define BUTTON_PIN 9
// !!重要!!: 请确保在你的主 setup() 函数中调用了 pinMode(BUTTON_PIN, INPUT_PULLUP);

// --- 全局变量和对象 ---
static ESP32WebServer server(80);      // 创建一个在80端口的Web服务器
static DNSServer dnsServer;            // 创建DNS服务器
static lv_obj_t *label_status;         // 用于在LVGL界面底部显示状态的标签
static char sta_name[20];              // STA模式下的主机名
static bool web_server_started = false; // 标记Web服务器是否已启动
static String connecting_ssid;         // 保存正在连接的SSID
static String connecting_password;     // 保存正在连接的密码
static bool wifi_connected_waiting_for_button = false; // 等待物理按键标志
static lv_timer_t *network_timer = NULL; // 用于网络处理的定时器
static lv_timer_t *button_timer = NULL; // 用于按键检测的定时器

// --- WiFi扫描状态 ---
static int scan_status = -1; // -1:未开始, 0:正在扫描, 1:已完成
static unsigned long scan_start_time = 0;
static String scan_result_json; // 缓存扫描结果

// 创建一个Preferences对象，命名空间为 "wifi-creds"
Preferences preferences;

// --- Web服务器配网页面 (HTML & JavaScript) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>Spitha WLAN Setup</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background-color: #f2f2f7; color: #333; margin: 0; padding: 20px; display: flex; justify-content: center; align-items: center; min-height: 100vh; }
    .container { background-color: #fff; padding: 25px; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); width: 100%; max-width: 400px; }
    h1 { color: #007aff; text-align: center; font-size: 24px; margin-bottom: 20px; }
    label { font-weight: 600; display: block; margin-top: 15px; margin-bottom: 5px; }
    select, input[type="password"], input[type="submit"], input[type="text"] { width: 100%; padding: 12px; border: 1px solid #ccc; border-radius: 8px; box-sizing: border-box; font-size: 16px; }
    input[type="submit"] { background-color: #007aff; color: white; border: none; cursor: pointer; margin-top: 25px; font-weight: bold; transition: background-color 0.2s; }
    input[type="submit"]:hover { background-color: #0056b3; }
    .spinner { margin: 20px auto; width: 40px; height: 40px; position: relative; display: none; }
    .double-bounce1, .double-bounce2 { width: 100%; height: 100%; border-radius: 50%; background-color: #007aff; opacity: 0.6; position: absolute; top: 0; left: 0; animation: sk-bounce 2.0s infinite ease-in-out; }
    .double-bounce2 { animation-delay: -1.0s; }
    @keyframes sk-bounce { 0%, 100% { transform: scale(0.0) } 50% { transform: scale(1.0) } }
    #status { text-align: center; margin-top: 20px; font-weight: 500; display: none; }
    #ssid_manual { display: none; margin-top: 10px; }
  </style>
</head>
<body>
  <div class="container">
    <h1><svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M5 12.55a11 11 0 0 1 14.08 0"></path><path d="M1.42 9a16 16 0 0 1 21.16 0"></path><path d="M8.53 16.11a6 6 0 0 1 6.95 0"></path><line x1="12" y1="20" x2="12.01" y2="20"></line></svg> WLAN Setup</h1>
    <form id="wifiForm">
      <label for="ssid">Choose a Network:</label>
      <select name="ssid" id="ssid" required></select>
      <input type="text" id="ssid_manual" name="ssid_manual" placeholder="Enter SSID manually">
      <label for="password">Password:</label>
      <input type="password" name="password" id="password">
      <input type="submit" value="Connect">
    </form>
    <div class="spinner" id="spinner">
      <div class="double-bounce1"></div>
      <div class="double-bounce2"></div>
    </div>
    <div id="status"></div>
  </div>
  <script>
    function showSpinner(show) {
      document.getElementById('spinner').style.display = show ? 'block' : 'none';
    }
    function showStatus(message, isError = false) {
      const statusEl = document.getElementById('status');
      statusEl.textContent = message;
      statusEl.style.color = isError ? '#ff3b30' : '#34c759';
      statusEl.style.display = 'block';
    }
    window.onload = () => {
      showSpinner(true);
      showStatus('Scanning for networks...', false);
      fetch('/scan')
        .then(response => response.json())
        .then(data => {
          showSpinner(false);
          document.getElementById('status').style.display = 'none';
          const select = document.getElementById('ssid');
          select.innerHTML = '';
          data.forEach(net => {
            const option = document.createElement('option');
            option.value = net.ssid;
            option.textContent = `${net.ssid} (${net.rssi}dBm, ${net.secure ? 'Protected' : 'Open'})`;
            select.appendChild(option);
          });
          // 添加手动输入选项
          const manualOption = document.createElement('option');
          manualOption.value = '__manual__';
          manualOption.textContent = 'Other (Enter manually)';
          select.appendChild(manualOption);
        })
        .catch(error => {
            showSpinner(false);
            showStatus('Failed to scan networks. Please refresh.', true);
        });
      // 监听下拉框变化，切换手动输入框显示
      document.getElementById('ssid').addEventListener('change', function() {
        const manualInput = document.getElementById('ssid_manual');
        if (this.value === '__manual__') {
          manualInput.style.display = 'block';
          manualInput.required = true;
        } else {
          manualInput.style.display = 'none';
          manualInput.required = false;
        }
      });
    };
    document.getElementById('wifiForm').addEventListener('submit', (e) => {
      e.preventDefault();
      showSpinner(true);
      let ssid = document.getElementById('ssid').value;
      const manualInput = document.getElementById('ssid_manual');
      if (ssid === '__manual__') {
        ssid = manualInput.value;
      }
      const password = document.getElementById('password').value;
      showStatus(`Connecting to "${ssid}"...`);
      fetch('/connect', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: `ssid=${encodeURIComponent(ssid)}&password=${encodeURIComponent(password)}`
      })
      .then(response => response.text())
      .then(text => {
        showSpinner(false);
        if (text === 'success') {
          showStatus('Success! Device is connecting to the new network. This access point will now close.');
        } else {
          showStatus('Connection failed. Please check the password and try again.', true);
        }
      })
      .catch(error => {
        showSpinner(false);
        showStatus('An error occurred. Please try again.', true);
      });
    });
  </script>
</body>
</html>
)rawliteral";


/**
 * @brief Wi-Fi事件回调函数
 */
static void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (label_status == NULL && event != ARDUINO_EVENT_WIFI_STA_GOT_IP) return; 

    switch (event) {
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            Serial.println("Client connected to AP.");
            lv_async_call([](void*){
                create_setup_finished_page();
            }, NULL);
            break;
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            Serial.println("Client disconnected from AP.");
            lv_async_call([](void*){
                if(label_status) lv_label_set_text(label_status, ""); // 清空或不显示
            }, NULL);
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP: { 
            Serial.print("STA Got IP: ");
            Serial.println(WiFi.localIP());

            preferences.begin("wifi-creds", false);
            preferences.putString("ssid", connecting_ssid);
            preferences.putString("password", connecting_password);
            preferences.end();
            Serial.println("Wi-Fi credentials saved to NVS.");

            // 停止AP和服务器相关服务
            WiFi.softAPdisconnect(true);
            server.stop(); 
            dnsServer.stop();
            web_server_started = false;
            if (network_timer) {
                lv_timer_del(network_timer);
                network_timer = NULL;
            }
            Serial.println("AP mode and web server stopped.");

            // 更新UI
            lv_async_call([](void*){
                lv_obj_clean(lv_scr_act()); 

                lv_obj_t* cont = lv_obj_create(lv_scr_act());
                lv_obj_remove_style_all(cont);
                lv_obj_set_size(cont, lv_pct(80), LV_SIZE_CONTENT);
                lv_obj_center(cont);
                lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
                lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                lv_obj_set_style_pad_row(cont, 20, 0);

                lv_obj_t* label_success = lv_label_create(cont);
                lv_obj_set_style_text_font(label_success, &lv_font_montserrat_22, 0);
                lv_label_set_text_fmt(label_success, "#34C759 %s#\nConnected!", LV_SYMBOL_OK);
                lv_obj_set_style_text_align(label_success, LV_TEXT_ALIGN_CENTER, 0);

                lv_obj_t* label_info = lv_label_create(cont);
                lv_obj_set_style_text_font(label_info, &lv_font_montserrat_14, 0);
                lv_label_set_text_fmt(label_info, "SSID: %s\nIP: %s",
                                      WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
                lv_obj_set_style_text_align(label_info, LV_TEXT_ALIGN_CENTER, 0);
                
                lv_obj_t* label_tip = lv_label_create(cont);
                lv_obj_set_style_text_font(label_tip, &lv_font_montserrat_16, 0);
                lv_label_set_text(label_tip, "\nPress the button to continue");
                lv_obj_set_style_text_align(label_tip, LV_TEXT_ALIGN_CENTER, 0);

            }, NULL);
            
            wifi_connected_waiting_for_button = true;
            break;
        } 
        default:
            break;
    }
}


/**
 * @brief 启动AP模式
 */
static void start_ap_spitha(void) {
    WiFi.mode(WIFI_AP_STA);
    const char *ap_ssid = "Spitha";
    WiFi.softAP(ap_ssid);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(sta_name, sizeof(sta_name), "Spitha_%02X%02X", mac[4], mac[5]);
    WiFi.setHostname(sta_name);

    WiFi.onEvent(onWifiEvent);
}

/**
 * @brief LVGL定时器回调，用于处理网络请求
 */
static void network_server_timer(lv_timer_t *timer) {
    if (web_server_started) {
        dnsServer.processNextRequest();
        server.handleClient();
    }
}

/**
 * @brief 设置和启动Web服务器及DNS服务器
 */
static void start_web_server() {
    if(web_server_started) return;

    dnsServer.start(53, "*", WiFi.softAPIP());

    server.on("/", HTTP_GET, [](){
        Serial.println("HTTP GET /");
        server.send_P(200, "text/html", index_html);
    });
    
    server.on("/generate_204", HTTP_GET, [](){
      Serial.println("HTTP GET /generate_204");
      server.sendHeader("Location", "http://" + WiFi.softAPIP().toString(), true);
      server.send(302, "text/plain", "");
    });

    server.on("/hotspot-detect.html", HTTP_GET, [](){
      Serial.println("HTTP GET /hotspot-detect.html");
      server.send_P(200, "text/html", index_html);
    });

    server.on("/scan", HTTP_GET, [](){
        Serial.println("HTTP GET /scan");
        if (scan_status != 1) {
            // 首次或未扫描，执行一次扫描
            do_wifi_scan_once();
        }
        server.send(200, "application/json", scan_result_json);
    });
    
    server.on("/connect", HTTP_POST, [](){
        Serial.println("HTTP POST /connect");
        if (server.hasArg("ssid") && server.hasArg("password")) {
            connecting_ssid = server.arg("ssid");
            connecting_password = server.arg("password");
            
            WiFi.begin(connecting_ssid.c_str(), connecting_password.c_str());
            server.send(200, "text/plain", "success");
        } else {
            server.send(400, "text/plain", "bad request");
        }
    });

    server.on("/favicon.ico", HTTP_GET, [](){
        Serial.println("HTTP GET /favicon.ico");
        server.send(204); // No Content
    });

    server.onNotFound([](){
        Serial.print("HTTP NotFound: ");
        Serial.println(server.uri());
        server.send_P(200, "text/html", index_html);
    });

    server.begin();
    web_server_started = true;
    Serial.println("Web server started.");

    // --- 修改: 使用LVGL定时器处理网络，而不是后台任务 ---
    if (network_timer == NULL) {
        network_timer = lv_timer_create(network_server_timer, 5, NULL);
    }
}


/**
 * @brief LVGL定时器回调，用于检测物理按键
 */
static void physical_button_check_timer(lv_timer_t *timer) {
    if (wifi_connected_waiting_for_button) {
        if (digitalRead(BUTTON_PIN) == LOW) {
            vTaskDelay(pdMS_TO_TICKS(50));
            if (digitalRead(BUTTON_PIN) == LOW) {
                wifi_connected_waiting_for_button = false;
                Serial.println("Physical button pressed. Transitioning to the next page...");
                // 跳转到设置完成页面
                create_setup_finished_page();
                // 删除此定时器
                if(button_timer) {
                    lv_timer_del(button_timer);
                    button_timer = NULL;
                }
            }
        }
    }
}


/**
 * @brief 执行一次WiFi扫描并缓存结果到scan_result_json
 */
void do_wifi_scan_once() {
    scan_status = 0;
    int n = WiFi.scanNetworks(false, true); // 同步扫描
    JsonDocument doc;
    JsonArray networks = doc.to<JsonArray>();
    for (int i = 0; i < n; ++i) {
        JsonObject net = networks.add<JsonObject>();
        net["ssid"] = WiFi.SSID(i);
        net["rssi"] = WiFi.RSSI(i);
        net["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    serializeJson(doc, scan_result_json);
    scan_status = 1;
    WiFi.scanDelete();
}

/**
 * @brief 创建并显示WLAN配置页面.
 */
void WLAN_Setup_Page(void)
{
    start_ap_spitha();  
    start_web_server(); 

    static lv_style_t style_screen;
    static lv_style_t style_title;
    static lv_style_t style_tip_text;
    static lv_style_t style_main_container;
    static bool styles_inited = false;

    if (!styles_inited) {
        lv_style_init(&style_screen);
        lv_style_set_bg_color(&style_screen, lv_color_hex(0xFFFFFF));
        lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);

        lv_style_init(&style_title);
        lv_style_set_text_font(&style_title, &lv_font_montserrat_22);
        lv_style_set_text_color(&style_title, lv_color_hex(0x333333));

        lv_style_init(&style_tip_text);
        lv_style_set_text_font(&style_tip_text, &lv_font_montserrat_14);
        lv_style_set_text_color(&style_tip_text, lv_color_hex(0x555555));
        lv_style_set_text_line_space(&style_tip_text, 4);

        lv_style_init(&style_main_container);
        lv_style_set_width(&style_main_container, lv_pct(90));
        lv_style_set_height(&style_main_container, LV_SIZE_CONTENT);
        lv_style_set_pad_column(&style_main_container, 20);

        styles_inited = true;
    }

    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_add_style(screen, &style_screen, 0);

    lv_obj_t *label_title = lv_label_create(screen);
    lv_obj_add_style(label_title, &style_title, 0);
    lv_label_set_text(label_title, LV_SYMBOL_WIFI " WLAN Configuration");
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *main_container = lv_obj_create(screen);
    lv_obj_remove_style_all(main_container);
    lv_obj_add_style(main_container, &style_main_container, 0);
    lv_obj_center(main_container);
    lv_obj_set_layout(main_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(main_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(main_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label_tip = lv_label_create(main_container);
    lv_obj_add_style(label_tip, &style_tip_text, 0);
    lv_label_set_text_fmt(label_tip, "1. Connect to Wi-Fi \"%s\"\n2. Visit http://%s in your browser\n3. Select your network", "Spitha", WiFi.softAPIP().toString().c_str());
    lv_label_set_long_mode(label_tip, LV_LABEL_LONG_WRAP);
    lv_obj_set_flex_grow(label_tip, 1);
    lv_obj_set_style_max_width(label_tip, lv_pct(100), 0);

    #if LV_USE_QRCODE
        const char *wifi_qr_data = "WIFI:T:WPA;S:Spitha;P:;;";
        lv_obj_t *qr_code = lv_qrcode_create(main_container);
        lv_qrcode_set_size(qr_code, 90);
        lv_qrcode_set_dark_color(qr_code, lv_color_hex(0x222222));
        lv_qrcode_set_light_color(qr_code, lv_color_hex(0xFFFFFF));
        lv_qrcode_update(qr_code, wifi_qr_data, strlen(wifi_qr_data));
        lv_obj_set_style_border_width(qr_code, 0, 0);
    #else
        lv_obj_t *qr_placeholder = lv_obj_create(main_container);
        lv_obj_set_size(qr_placeholder, 90, 90);
        lv_obj_set_style_bg_color(qr_placeholder, lv_color_hex(0x222222), 0);
        lv_obj_set_style_border_width(qr_placeholder, 0, 0);
    #endif

    // 创建LVGL定时器以轮询物理按键
    if(button_timer == NULL) {
        button_timer = lv_timer_create(physical_button_check_timer, 50, NULL);
    }

    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);
}
