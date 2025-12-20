#ifndef LV_FS_LITTLEFS_H
#define LV_FS_LITTLEFS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * @brief Khởi tạo filesystem LittleFS cho LVGL
 *
 * Sau khi gọi:
 *  - Có thể dùng đường dẫn: "S:/file.bin"
 *  - Dùng cho Binary LVGL / SquareLine
 *
 * Phải gọi sau lv_init() và sau LittleFS.begin()
 */
void lv_fs_littlefs_init(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_FS_LITTLEFS_H */
