#include <Arduino.h>
#include <esp_display_panel.hpp>

#include <lvgl.h>
#include "lvgl_v8_port.h"

//-------Modify include-------------------------
#include "ui.h"
#include <SPIFFS.h>
#include "ui_Screen1.h"
#include "ui_Screen2.h"
#include "lv_fs_spiffs.h"


using namespace esp_panel::drivers;
using namespace esp_panel::board;


//----Modify extern----------
extern void lv_fs_spiffs_init(void);

//----Modify Variable


void setup()
{
    String title = "LVGL porting example";

    Serial.begin(115200);

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

    lv_split_jpeg_init();
    
    if (!SPIFFS.begin(true)) {
      Serial.println("SPIFFS Mount Failed");
    } 
    else {
      Serial.println("SPIFFS Mounted");
    }

    lv_fs_spiffs_init();

    ui_init();

    lv_img_set_src(ui_SC1backGround, "S:/Backgroud.jpg");
    lv_img_set_src(ui_SC2backGround, "S:/Backgroud2.jpg");
    /* Release the mutex */
    lvgl_port_unlock();
}

void loop()
{
    Serial.println("IDLE loop");
    delay(1000);
}