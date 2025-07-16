#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include "sysinfo.h"

#define INFO_LINES 20
#define INFO_COLS  60



void show_sysinfo_popup(void) {
    // 读取当前目录
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "termux-dialog -t '当前目录' -i '%s'", current_dir);
    system(cmd);
}
