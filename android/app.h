#ifndef APP_H
#define APP_H
#include "render.h"
void app_init(const char *data_dir, const char *ext_dir);
void app_frame(surf_t *fb);
void app_tap(void);
#endif
