#include "lvgl.h"
#include <SPIFFS.h>
#include "lv_fs_spiffs.h"

static void * spiffs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode)
{
    File * f = new File(SPIFFS.open(path, mode == LV_FS_MODE_WR ? "w" : "r"));
    if (!*f) {
        delete f;
        return NULL;
    }
    return f;
}

static lv_fs_res_t spiffs_close(lv_fs_drv_t * drv, void * file_p)
{
    File * f = (File *)file_p;
    f->close();
    delete f;
    return LV_FS_RES_OK;
}

static lv_fs_res_t spiffs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br)
{
    File * f = (File *)file_p;
    *br = f->read((uint8_t *)buf, btr);
    return LV_FS_RES_OK;
}

static lv_fs_res_t spiffs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence)
{
    File * f = (File *)file_p;
    f->seek(pos);
    return LV_FS_RES_OK;
}

static lv_fs_res_t spiffs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p)
{
    File * f = (File *)file_p;
    *pos_p = f->position();
    return LV_FS_RES_OK;
}

void lv_fs_spiffs_init(void)
{
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);

    fs_drv.letter = 'S';   // S:/
    fs_drv.open_cb = spiffs_open;
    fs_drv.close_cb = spiffs_close;
    fs_drv.read_cb = spiffs_read;
    fs_drv.seek_cb = spiffs_seek;
    fs_drv.tell_cb = spiffs_tell;

    lv_fs_drv_register(&fs_drv);
}
