#include "lv_fs_spiffs.h"
#include "LittleFS.h"

static void * fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode) {
    // Chuyển đường dẫn từ "assets/..." thành "/assets/..." để LittleFS hiểu
    String full_path = "/" + String(path);
    
    const char * flags = (mode == LV_FS_MODE_WR) ? "w" : "r";
    File f = LittleFS.open(full_path, flags);
    
    if(!f || f.isDirectory()) {
        Serial.printf("=> LVGL FS Error: Khong thay file tại %s\n", full_path.c_str());
        return NULL;
    }

    Serial.printf("=> LVGL FS OK: Dang mo file %s (%d bytes)\n", full_path.c_str(), (int)f.size());
    
    // Cấp phát động đối tượng File để giữ trạng thái mở
    return (void *)new File(f);
}

static lv_fs_res_t fs_close(lv_fs_drv_t * drv, void * file_p) {
    File * f = (File *)file_p;
    f->close();
    delete f;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br) {
    File * f = (File *)file_p;
    *br = f->read((uint8_t *)buf, btr);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence) {
    File * f = (File *)file_p;
    SeekMode mode = SeekSet;
    if(whence == LV_FS_SEEK_CUR) mode = SeekCur;
    else if(whence == LV_FS_SEEK_END) mode = SeekEnd;
    else if(whence == LV_FS_SEEK_SET) mode = SeekSet;

    f->seek(pos, mode);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p) {
    File * f = (File *)file_p;
    *pos_p = f->position();
    return LV_FS_RES_OK;
}

void lv_fs_littlefs_init(void) {
    static lv_fs_drv_t drv;
    lv_fs_drv_init(&drv);

    drv.letter = 'S'; // Khớp với "S:" trong code gen
    drv.open_cb = fs_open;
    drv.close_cb = fs_close;
    drv.read_cb = fs_read;
    drv.seek_cb = fs_seek;
    drv.tell_cb = fs_tell;

    lv_fs_drv_register(&drv);
    Serial.println("LVGL FS: Da dang ky o dia S: thanh cong!");
}