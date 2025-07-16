#include "navigation.h"
#include <ncurses.h>


/*============翻页原子函数============-*/
int get_effective_file_count(void) {
    int count = file_count;
    while (count > 0 && files[count - 1].name[0] == '\0') {
        count--;
    }
    return count;
}
/*============翻页原子函数============-*/
void sync_start_index_to_cursor(void) {
    // 如果 cursor_pos 超出当前页，更新 start_index
    if (cursor_pos < start_index) {
        start_index = (cursor_pos / files_per_page) * files_per_page;
    } else if (cursor_pos >= start_index + files_per_page) {
        start_index = (cursor_pos / files_per_page) * files_per_page;
    }
}

/*============翻页原子函数============-*/

/*============方向键四函数============-*/
/* 上移光标 */
void handle_cursor_up(void) {
    if (file_count == 0) return;

    if (cursor_pos > 0) {
        cursor_pos--;

        if (cursor_pos == 0 && strcmp(files[0].name, "..") == 0) {
            beep();
        }

        sync_start_index_to_cursor();
    } else {
        beep();
    }
}

/* 下移光标 */
void handle_cursor_down(void) {
    if (file_count == 0) return;

    int effective_count = get_effective_file_count();

    if (cursor_pos < effective_count - 1) {
        cursor_pos++;
        sync_start_index_to_cursor();
    } else {
        beep();
    }
}

/* 左移分栏 */
void handle_column_left(void) {
    if (file_count == 0 || COLUMNS <= 1) return;

    int items_per_col = (file_count + COLUMNS - 1) / COLUMNS;
    items_per_col = items_per_col > 0 ? items_per_col : 1;

    if (cursor_pos >= items_per_col) {
        cursor_pos -= items_per_col;
        if (cursor_pos >= file_count) {
            cursor_pos = file_count - 1;
        }
        sync_start_index_to_cursor();
    } else {
        cursor_pos = 0;
        sync_start_index_to_cursor();
    }
}

/* 右移分栏 */
void handle_column_right(void) {
    if (file_count == 0 || COLUMNS <= 1) return;

    int items_per_col = (file_count + COLUMNS - 1) / COLUMNS;
    int new_pos = cursor_pos + items_per_col;

    int effective_count = get_effective_file_count();

    if (new_pos < effective_count) {
        cursor_pos = new_pos;
    } else {
        cursor_pos = effective_count - 1;
    }

    sync_start_index_to_cursor();
}

/*===============导航键结束============*/

/* 获取完整路径 */
void get_full_path(char *buf, size_t size) {
    if (strncmp(current_dir, "find://", 7) == 0) {
        snprintf(buf, size, "%s", files[cursor_pos].name);
    } else {
        snprintf(buf, size, "%s/%s", current_dir, files[cursor_pos].name);
    }
}

void open_trash(void) {
    scan_directory(g_trash_dir);
}

void open_bashscript_home(void) {
    scan_directory(g_bash_home);
}

void navigate_to_home(void) {

    const char *home = getenv("HOME");
    if (!home) home = "/";  // 回退到根目录
    
    scan_directory(home);
    show_message_timed("回到主目录", 500);
}

void navigate_to_last(void) {
    if (in_find_mode) {
        show_message_timed("搜索模式下不可用", 600);
        return;
    }
    
    if (last_visited_dir[0] == '\0') {
        show_message_timed("无上次访问记录", 600);
        return;
    }
    
    char resolved_last[PATH_MAX];
    char resolved_current[PATH_MAX];
    
    // Check realpath() calls for errors
    if (realpath(last_visited_dir, resolved_last) == NULL) {
        show_message("无法解析上次目录路径");
        return;
    }
    if (realpath(current_dir, resolved_current) == NULL) {
        show_message("无法解析当前目录路径");
        return;
    }
    
    if (strcmp(resolved_last, resolved_current) == 0) {
        show_message_timed("当前目录", 600);
        return;
    }
    
    // Since scan_directory() returns void, just call it and assume success
    scan_directory(last_visited_dir);
    strcpy(current_dir, last_visited_dir);
    show_message_timed("回到上次目录", 600);
}

