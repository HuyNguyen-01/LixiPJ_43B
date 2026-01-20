#include <stdint.h>
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
AsyncWebSocket ws("/ws");


//----Modify extern----------
extern void lv_fs_littlefs_init(void);

//----Modify Variable
String global_ssid = "";
String global_pass = "";
String global_ip   = "";
String global_gateway = ""; // Biến lưu địa chỉ Gateway
String global_subnet  = "";
bool global_dhcp;

//Extern "C" Variable
extern "C"{
  uint16_t money_list[9]; 
  int active_money_count = 0; // Biến đếm số lượng mệnh giá được nạp
}

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
                "ipadress": "192.168.5.100",
                "gateway": "192.168.5.1",
                "subnet": "255.255.255.0",
                "dhcp": true,
                "1000": false,
                "2000": false,
                "5000": false,
                "10000": true,
                "20000": true,
                "50000": true,
                "100000": true,
                "200000": false,
                "500000": false
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
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, configFile);

    if (!error) {
      // Gán vào các biến toàn cục (String) của bạn
      global_ssid = doc["ssid"] | "";
      global_pass = doc["pass"] | "";
      global_ip   = doc["ipadress"] | "";
      global_gateway = doc["gateway"] | "";  
      global_subnet  = doc["subnet"] | ""; 
      global_dhcp = doc["dhcp"] | false;

      // 2. Xử lý các mệnh giá tiền
      active_money_count = 0; // Reset bộ đếm trước khi nạp
      // Danh sách các key cần kiểm tra
      const char* money_keys[] = {"1000", "2000", "5000", "10000", "20000", "50000", "100000", "200000", "500000"};
      for (const char* key : money_keys) {
        if (doc[key] == true) {
          // Chuyển key từ chuỗi sang số, chia cho 1000 rồi lưu vào mảng
          uint32_t full_value = atol(key); 
          money_list[active_money_count] = (uint16_t)(full_value / 1000);
          active_money_count++;
        }
      }


      Serial.println("--- Cấu hình đã tải ---");
      Serial.printf("SSID: %s | IP: %s | DHCP: %s\n", 
                      global_ssid.c_str(), global_ip.c_str(), global_dhcp ? "Bật" : "Tắt");
      Serial.print("Mệnh giá kích hoạt (k): ");
      for (int i = 0; i < active_money_count; i++) {
        Serial.print(money_list[i]);
        if (i < active_money_count - 1) Serial.print(", ");
      }
      Serial.println();

      //lOAD VÀO lvgl
      if (ui_Screen3) { 
          lv_textarea_set_text(ui_TextAreWifiName, global_ssid.c_str());
          lv_textarea_set_text(ui_TextAreWifiPass, global_pass.c_str());
          lv_textarea_set_text(ui_TextAreIP, global_ip.c_str());
          lv_textarea_set_text(ui_TextAreGateway, global_gateway.c_str());
          lv_textarea_set_text(ui_TextAreSubnet, global_subnet.c_str());
          
          auto setCB = [&](lv_obj_t* obj, const char* key) {
            if (obj) {
              if (doc[key] == true) lv_obj_add_state(obj, LV_STATE_CHECKED);
              else lv_obj_clear_state(obj, LV_STATE_CHECKED);
            }
          };

          setCB(ui_CheckboxDHCP, "dhcp");
          setCB(ui_Checkbox1000, "1000");
          setCB(ui_Checkbox2000, "2000");
          setCB(ui_Checkbox5000, "5000");
          setCB(ui_Checkbox10000, "10000");
          setCB(ui_Checkbox20000, "20000");
          setCB(ui_Checkbox50000, "50000");
          setCB(ui_Checkbox100000, "100000");
          setCB(ui_Checkbox200000, "200000");
          setCB(ui_Checkbox500000, "500000");
      }
      
    } else {
        Serial.print("Lỗi đọc JSON: ");
        Serial.println(error.f_str());
    }

    configFile.close();
}

// Thêm extern "C" để "báo" cho trình biên dịch rằng hàm này có thể dùng cho file .c
extern "C"{
  // void saveConfig_c(const char* ssid, const char* pass, const char* ip) {
  //   // Tạo chuỗi JSON từ các tham số truyền vào
  //   StaticJsonDocument<256> doc;
  //   doc["ssid"] = ssid;
  //   doc["pass"] = pass;
  //   doc["ipadress"] = ip;

  //   File configFile = LittleFS.open("/config.json", "w");
  //   if (configFile) {
  //     serializeJson(doc, configFile);
  //     configFile.close();
  //     Serial.println("Lưu file thành công!");
  //   } else {
  //     Serial.println("Lỗi mở file!");
  //   }
  // }

  void saveConfig_c(const char* ssid, const char* pass, const char* ip, const char* gateway, const char* subnet,
                     bool dhcp, bool m1k, bool m2k, bool m5k, 
                     bool m10k, bool m20k, bool m50k, 
                     bool m100k, bool m200k, bool m500k) {
                      
    StaticJsonDocument<1024> doc;
    // 1. Đọc file hiện tại lên để lấy các giá trị cũ
    File readFile = LittleFS.open("/config.json", "r");
    if (readFile) {
        deserializeJson(doc, readFile);
        readFile.close();
    }

    // 2. Chỉ cập nhật nếu chuỗi không rỗng (bỏ qua nếu rỗng/null)
    if (ssid && strlen(ssid) > 0) doc["ssid"] = ssid;
    if (pass && strlen(pass) > 0) doc["pass"] = pass;
    if (ip && strlen(ip) > 0)     doc["ipadress"] = ip;
    if (gateway && strlen(gateway) > 0) doc["gateway"] = gateway;
    if (subnet && strlen(subnet) > 0)   doc["subnet"] = subnet;
    
    // 3. Cập nhật DHCP và các mệnh giá tiền (bool luôn có giá trị)
    doc["dhcp"] = dhcp;
    doc["1000"] = m1k;
    doc["2000"] = m2k;
    doc["5000"] = m5k;
    doc["10000"] = m10k;
    doc["20000"] = m20k;
    doc["50000"] = m50k;
    doc["100000"] = m100k;
    doc["200000"] = m200k;
    doc["500000"] = m500k;

    // 4. Ghi lại vào file
    File writeFile = LittleFS.open("/config.json", "w");
    if (writeFile) {
      serializeJson(doc, writeFile);
      writeFile.close();
      Serial.println("Config updated successfully!");
    } else {
      Serial.println("Failed to write config.json");
    }
  }

  void send_spin_now_to_all_clients() {
    // Kiểm tra xem có client nào đang kết nối không để tránh lãng phí tài nguyên
    if (ws.count() > 0) {
      ws.textAll("spin_now");
      Serial.println("Sent spin_now to all socket clients!");
    }
  }

} 

void onWsEvent(
  AsyncWebSocket *server,
  AsyncWebSocketClient *client,
  AwsEventType type,
  void *arg,
  uint8_t *data,
  size_t len
) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected\n", client->id());
      break;

    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;

    case WS_EVT_DATA: {
      AwsFrameInfo *info = (AwsFrameInfo*)arg;
      if (info->opcode == WS_TEXT) {
        String msg;
        for (size_t i = 0; i < len; i++) msg += (char)data[i];
        Serial.printf("WS recv: %s\n", msg.c_str());
      }
      break;
    }
    default:
      break;
  }
}


void setup()
{
    String title = "LVGL porting example";

    Serial.begin(115200);

    // uint32_t seed = esp_random(); 
    // srand(seed);

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
      if (!global_dhcp) {
        IPAddress local_IP, gateway, subnet;
        // Chuyển đổi String sang IPAddress
        if (local_IP.fromString(global_ip) && 
            gateway.fromString(global_gateway) && 
            subnet.fromString(global_subnet)) {
            if (!WiFi.config(local_IP, gateway, subnet)) {
                Serial.println("Cấu hình Static IP thất bại!");
            } else {
                Serial.println("Đã thiết lập Static IP thành công.");
            }
        } else {
          Serial.println("Định dạng IP/Gateway/Subnet không hợp lệ, đang dùng DHCP mặc định.");
        }
      } else {
          Serial.println("Chế độ: DHCP (Tự động nhận IP)");
      }
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
    // Gửi Back.png
    server.on("/Back.png", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/Back.png", "image/png");
    });

    // Gửi But.png
    server.on("/But.png", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/But.png", "image/png");
    });

    // Gửi Back_H.png
    server.on("/Back_H.png", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/Back_H.png", "image/png");
    });

    server.on("/admin/spin", HTTP_GET, [](AsyncWebServerRequest *request) {
      // Broadcast tới tất cả client WebSocket
      ws.textAll("spin_now");
      request->send(200, "application/json",
        "{\"status\":\"ok\",\"msg\":\"spin command sent\"}"
      );
      Serial.println("API /admin/spin triggered -> WS broadcast spin_now");
    });
   
    server.on("/money-config", HTTP_GET, [](AsyncWebServerRequest *request) {
      // 1. Tạo tài liệu JSON để phản hồi
      DynamicJsonDocument responseDoc(2048);
      JsonArray root = responseDoc.to<JsonArray>();

      // 2. Duyệt qua mảng money_list để tạo đối tượng JSON
      for (int i = 0; i < active_money_count; i++) {
          uint32_t full_value = money_list[i] * 1000; // Quy đổi ngược lại giá trị đầy đủ
          int weight = 100; // Trọng số cơ sở cho các mệnh giá < 100.000

          // 3. Tính toán trọng số theo quy tắc của bạn
          if (full_value == 100000) {
              weight = 50;  // Bằng 1/2 của mức cơ sở
          } 
          else if (full_value == 200000) {
              weight = 25;  // Bằng 1/2 của 100.000
          } 
          else if (full_value == 500000) {
              weight = 12;  // Bằng 1/2 của 200.000 (lấy xấp xỉ)
          }
          // Các giá trị < 100.000 (như 10k, 20k, 50k) giữ nguyên weight = 100

          // 4. Thêm vào mảng JSON
          JsonObject obj = root.createNestedObject();
          obj["value"] = full_value;
          obj["weight"] = weight;
      }

      // 5. Chuyển đổi sang chuỗi và gửi phản hồi
      String response;
      serializeJson(root, response);
      request->send(200, "application/json", response);
    });

}

void loop()
{
  if (shouldStartServer) {
    shouldStartServer = false; // Reset cờ ngay lập tức
    if (!serverStarted) {
        ws.onEvent(onWsEvent);
        server.addHandler(&ws);
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