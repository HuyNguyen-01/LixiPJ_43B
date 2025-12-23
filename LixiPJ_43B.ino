#include <Arduino.h>
#include <esp_display_panel.hpp>

#include <lvgl.h>
#include "lvgl_v8_port.h"

//-------Modify include-------------------------
#include "ui.h"
#include "LittleFS.h"
#include <WiFi.h>
// #include "ui_Screen4.h"
// #include "ui_Screen5.h"
#include "lv_fs_spiffs.h"
#include <ArduinoJson.h>

#include <ESPAsyncWebServer.h>


using namespace esp_panel::drivers;
using namespace esp_panel::board;

AsyncWebServer server(80); // Khởi tạo server ở cổng 80


//----Modify extern----------
extern void lv_fs_littlefs_init(void);

//----Modify Variable
String global_ssid = "";
String global_pass = "";
String global_ip   = "";



unsigned long lastWifiCheck = 0;
const unsigned long wifiInterval = 2000; // Kiểm tra mỗi 2 giây
String lastIP = "";
long lastRSSI = -999;
bool lastConnectStatus = false;
static bool serverStarted = false;
bool shouldStartServer = false;
//----Modify Function-----------

void initAndLoadConfig() {
    // 1. Khởi tạo LittleFS với tùy chọn tự động Format nếu có lỗi
    if (!LittleFS.begin(true)) {
        Serial.println("Lỗi Mount LittleFS!");
        return;
    }

    // 2. Kiểm tra file, nếu chưa có thì tạo ngay nội dung mặc định
    if (!LittleFS.exists("/config.json")) {
        Serial.println("File config.json chưa tồn tại. Đang khởi tạo...");
        File configFile = LittleFS.open("/config.json", "w");
        if (configFile) {
            // Sử dụng Raw String để chuỗi JSON trông sạch sẽ hơn
            const char* defaultJSON = R"({
                "ssid": "RD",
                "pass": "Itdc@12345",
                "ipadress": "192.168.5.100"
            })";
            configFile.print(defaultJSON);
            configFile.close();
            Serial.println("Đã tạo file mặc định thành công.");
        }
    }

    // 3. Mở file để đọc dữ liệu vào biến toàn cục
    File configFile = LittleFS.open("/config.json", "r");
    if (!configFile) {
        Serial.println("Không thể mở file config!");
        return;
    }

    // 4. Giải mã JSON
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, configFile);

    if (!error) {
      // Gán vào các biến toàn cục (String) của bạn
      global_ssid = doc["ssid"] | "";
      global_pass = doc["pass"] | "";
      global_ip   = doc["ipadress"] | "";

      Serial.println("--- Cấu hình đã tải ---");
      Serial.printf("SSID: %s | IP: %s\n", global_ssid.c_str(), global_ip.c_str());
    } else {
        Serial.print("Lỗi đọc JSON: ");
        Serial.println(error.f_str());
    }

    configFile.close();
}

// Thêm extern "C" để "báo" cho trình biên dịch rằng hàm này có thể dùng cho file .c
extern "C" void saveConfig_c(const char* ssid, const char* pass, const char* ip) {
    // Tạo chuỗi JSON từ các tham số truyền vào
    StaticJsonDocument<256> doc;
    doc["ssid"] = ssid;
    doc["pass"] = pass;
    doc["ipadress"] = ip;

    File configFile = LittleFS.open("/config.json", "w");
    if (configFile) {
        serializeJson(doc, configFile);
        configFile.close();
        Serial.println("Lưu file thành công!");
    } else {
        Serial.println("Lỗi mở file!");
    }
}



void setup()
{
    String title = "LVGL porting example";

    Serial.begin(115200);

    // 1. Khởi tạo bộ nhớ tệp trước
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
    } else {
        Serial.println("LittleFS Mounted");
    }

    Serial.println("Initializing board");
    Board *board = new Board();
    board->init();

    #if LVGL_PORT_AVOID_TEARING_MODE
    auto lcd = board->getLCD();
    // When avoid tearing function is enabled, the frame buffer number should be set in the board driver
    lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
    auto lcd_bus = lcd->getBus();
    /**
     * As the anti-tearing feature typically consumes more PSRAM bandwidth, for the ESP32-S3, we need to utilize the
     * "bounce buffer" functionality to enhance the RGB data bandwidth.
     * This feature will consume `bounce_buffer_size * bytes_per_pixel * 2` of SRAM memory.
     */
    if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
        static_cast<BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 30);
    }
#endif
#endif
    assert(board->begin());

    Serial.println("Initializing LVGL");
    lvgl_port_init(board->getLCD(), board->getTouch());

    Serial.println("Creating UI");
    /* Lock the mutex due to the LVGL APIs are not thread-safe */
    lvgl_port_lock(-1);

    // lv_split_jpeg_init();
    
    // if (!SPIFFS.begin(true)) {
    //   Serial.println("SPIFFS Mount Failed");
    // } 
    // else {
    //   Serial.println("SPIFFS Mounted");
    // }

    //lv_fs_spiffs_init();
    lv_fs_littlefs_init();
    ui_init();

    //lv_img_set_src(ui_SC1backGround, "S:/Backgroud.jpg");
    //lv_img_set_src(ui_SC2backGround, "S:/Backgroud2.jpg");
    /* Release the mutex */
    lvgl_port_unlock();

    initAndLoadConfig();

    WiFi.mode(WIFI_STA);
    if (global_ssid != "" && global_ssid != "NULL") {
        WiFi.begin(global_ssid.c_str(), global_pass.c_str());
        Serial.printf("Bắt đầu kết nối tới: %s\n", global_ssid.c_str());
    } else {
        Serial.println("Chưa có cấu hình WiFi. Vui lòng thiết lập qua UI.");
    }

    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        Serial.print("WiFi Event: ");
        Serial.println(event);
        
        if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            String ip = WiFi.localIP().toString();
            Serial.print("Đã có IP: ");
            Serial.println(ip);
            shouldStartServer = true;
        } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            Serial.println("WiFi bị ngắt kết nối.");
            // Tùy chọn: server.end(); serverStarted = false; nếu muốn tắt server khi mất mạng
        }
    });

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        // Gửi file index.html từ LittleFS
        request->send(LittleFS, "/index.html", "text/html");
    });
   
 

}

void loop()
{
  if (shouldStartServer) {
    shouldStartServer = false; // Reset cờ ngay lập tức
    if (!serverStarted) {
        server.begin();
        serverStarted = true;
        Serial.println("AsyncWebServer started safely from Loop!");
    }
  }

  if (millis() - lastWifiCheck >= wifiInterval) {
    lastWifiCheck = millis();

    bool currentStatus = (WiFi.status() == WL_CONNECTED);
    String currentIP = currentStatus ? WiFi.localIP().toString() : "0.0.0.0";
    long currentRSSI = currentStatus ? WiFi.RSSI() : 0;

    // CHỈ CẬP NHẬT KHI CÓ THAY ĐỔI (Status, IP hoặc RSSI lệch đáng kể)
    // Ở đây tôi để RSSI lệch > 2dBm mới cập nhật để tránh nhảy số liên tục
    if (currentStatus != lastConnectStatus || currentIP != lastIP || abs(currentRSSI - lastRSSI) > 2) {
        if (lvgl_port_lock(0)) {
            if (currentStatus) {
                // Cập nhật Label trạng thái & RSSI
                lv_label_set_text_fmt(ui_SC4LabelWifi, "Connected: %ld dBm", currentRSSI);
                lv_label_set_text_fmt(ui_SC5LabelWifi, "Connected: %ld dBm", currentRSSI);
                
                // Cập nhật Label IP
                lv_label_set_text(ui_SC4LabelIPAdress0, currentIP.c_str());
                lv_label_set_text(ui_SC5LabelIPAdress0, currentIP.c_str());
            } 
            else {
                lv_label_set_text(ui_SC4LabelWifi, "Disconnected");
                lv_label_set_text(ui_SC4LabelIPAdress0, "0.0.0.0");
                lv_label_set_text(ui_SC5LabelWifi, "Disconnected");
                lv_label_set_text(ui_SC5LabelIPAdress0, "0.0.0.0");
            }
            lvgl_port_unlock();
            
            // Cập nhật lại giá trị đệm
            lastIP = currentIP;
            lastRSSI = currentRSSI;
            lastConnectStatus = currentStatus;
        }
    }
  }


}