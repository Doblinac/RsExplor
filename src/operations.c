#include <stdlib.h>
#include <limits.h>  // 定义 PATH_MAX
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <libgen.h> // 用于 basename()
#include <unistd.h> // 用于 access()
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <ncurses.h>
#include <sys/wait.h>

#include "common.h"
#include "operations.h"
#include "ui.h"
#include "navigation.h"

/*============功能函数============-*/
// 在程序初始化时设置初始值
void init_navigation(void) {
	const char *home = getenv("HOME");
	strncpy(last_visited_dir, home ? home : "/", MAX_PATH);
	
    if (!HOME_DIR || access(HOME_DIR, R_OK) != 0) {
        fprintf(stderr, "警告: 主目录不可访问，将使用根目录\n");
        #undef HOME_DIR
        #define HOME_DIR "/"
    }
}

/*============目录扫描============-*/
void scan_directory(const char *path) {
    // 边界检查：确保路径不为空
    if (path == NULL || path[0] == '\0') {
        show_message("无效路径");
        beep();
        return;
    }
    
    if (strcmp(path, "home://") == 0) {
        scan_directory(HOME_DIR);
        return;
    }

    // 解析真实路径（增加错误处理）
    char resolved_path[MAX_PATH];
    if (realpath(path, resolved_path) == NULL) {
        show_message("路径解析失败");
        beep();
        return;
    }

    // 安全边界检查（保留原有范围限制）
    if (strncmp(resolved_path, ROOT_DIR, strlen(ROOT_DIR)) != 0 && 
        strncmp(resolved_path, HOME_DIR, strlen(HOME_DIR)) != 0) {
        show_message("超出访问范围");
        beep();
        return;
    }

    // 打开目录（增加错误处理）
    DIR *dir = opendir(resolved_path);
    if (!dir) {
        show_message("无法打开目录");
        beep();
        return;
    }

    /* 修复 last_visited_dir 问题 */
    if (!in_find_mode && 
        strncmp(path, "find://", 7) != 0 &&
        strncmp(path, "grep://", 7) != 0 &&
        strcmp(resolved_path, current_dir) != 0) {
        strncpy(last_visited_dir, current_dir, MAX_PATH);
        last_visited_dir[MAX_PATH - 1] = '\0';
    }

    // 更新当前目录
    strncpy(current_dir, resolved_path, MAX_PATH);
    current_dir[MAX_PATH - 1] = '\0';
    file_count = 0;

    // 添加父目录链接（如果不是根目录）
    if (strcmp(resolved_path, ROOT_DIR) != 0) {
        FileEntry *parent = &files[file_count];
        strncpy(parent->name, "..", MAX_PATH);
        parent->is_dir = 1;
        parent->is_exec = 0;
        parent->is_image = 0;
        parent->is_hidden = 0;
        parent->size = -1; // 父目录大小设为-1
        file_count++;
    }

    // 读取目录内容
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && file_count < MAX_FILES) {
        // 跳过特殊目录和隐藏文件（如果设置）
        if (strcmp(entry->d_name, ".") == 0 || 
            strcmp(entry->d_name, "..") == 0 ||
            (!show_hidden_files && entry->d_name[0] == '.')) {
            continue;
        }

        // 填充文件条目
        FileEntry *fe = &files[file_count];
        strncpy(fe->name, entry->d_name, MAX_PATH);
        fe->name[MAX_PATH - 1] = '\0';

        // 构建完整路径并获取文件属性
        char full_path[MAX_PATH * 2];
        snprintf(full_path, sizeof(full_path), "%s/%s", resolved_path, entry->d_name);

        struct stat statbuf;
        if (stat(full_path, &statbuf) == 0) {
            fe->is_dir = S_ISDIR(statbuf.st_mode);
            fe->is_exec = (statbuf.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) ? 1 : 0;
            fe->is_image = is_image_file(entry->d_name);
            fe->is_hidden = (entry->d_name[0] == '.');
            fe->size = fe->is_dir ? -1 : statbuf.st_size; // 目录设为-1，文件设实际大小
        } else {
            // 如果stat失败，设置安全默认值
            fe->is_dir = 0;
            fe->is_exec = 0;
            fe->is_image = 0;
            fe->is_hidden = (entry->d_name[0] == '.');
            fe->size = -1; // 标记为未知大小
        }

        file_count++;
    }
    
    closedir(dir);

    // 排序（如果不在查找模式）
    if (!in_find_mode) {
        qsort(files, file_count, sizeof(FileEntry), compare_files);
    }

    // 重置光标位置
    cursor_pos = (file_count > 0) ? MIN(1, file_count - 1) : 0;
    last_cursor_pos = cursor_pos;
}


/*=========文件类型检查函数===========*/

int is_hidden_file(const char *filename) {
    const char *base = strrchr(filename, '/');
    base = base ? base + 1 : filename;
    return (base[0] == '.');
}

/*============功能函数============-*/
int compare_files(const void *a, const void *b) {
    const FileEntry *fa = (const FileEntry *)a;
    const FileEntry *fb = (const FileEntry *)b;

    if (fa->is_dir && !fb->is_dir) return -1;
    if (!fa->is_dir && fb->is_dir) return 1;

    return strcasecmp(fa->name, fb->name);
}


/*============功能函数============-*/
int is_image_file(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;

    return strcasecmp(ext, ".jpg") == 0 ||
           strcasecmp(ext, ".jpeg") == 0 ||
           strcasecmp(ext, ".png") == 0 ||
           strcasecmp(ext, ".gif") == 0 ||
           strcasecmp(ext, ".bmp") == 0;
}

/*============§☆☆☆§============-*/

bool is_compressed_file(const char *filepath) {
    const char *ext = strrchr(filepath, '.');
    if (!ext) return false;

    return strcmp(ext, ".zip") == 0 ||
           strcmp(ext, ".tar") == 0 ||
           strcmp(ext, ".gz")  == 0 ||
           strcmp(ext, ".rar") == 0 ||
           strcmp(ext, ".7z")  == 0;
}


/*============查看图片============-*/
void view_image(const char *termux_path) {
    char android_path[MAX_PATH];
    convert_termux_path_to_android(android_path, termux_path);

    def_prog_mode();  // 保存当前终端状态
    endwin();         // 暂停 ncurses

    char cmd[MAX_PATH + 128];
    snprintf(cmd, sizeof(cmd),
             "am start -a android.intent.action.VIEW -d 'file://%s' -t 'image/*'",
             android_path);

    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "无法打开图片，请确保已安装 termux-api 并授予存储权限\n");
    }

    printf("按任意键继续...\n");
    getchar();

    reset_prog_mode();  // 恢复终端状态
    refresh();          // 恢复界面
}

/*============新功能函数============-*/

// 获取MIME类型（可选增强）
const char* get_mime_type(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return "application/octet-stream";

    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".pdf") == 0) return "application/pdf";
    if (strcmp(ext, ".mp4") == 0) return "video/mp4";
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".txt") == 0) return "text/plain";

    return "application/octet-stream";
}

/*============状态栏============-*/
// 你现有的 run_in_terminal 示例（请自己实现）
void run_in_terminal(const char *shell, const char *filepath) {
    // 例如 fork + execlp 执行 shell 打开 filepath
    // 或 system 调用 shell 脚本
    // 这里简化示例：
    char cmd[MAX_PATH + 32];
    snprintf(cmd, sizeof(cmd), "%s \"%s\"", shell, filepath);
    endwin();
    system(cmd);
    refresh();
}

/*============菜单功能函数============-*/
void open_with_termux_open(const char *filepath) {
    	check_termux_allow_external();
		char cmd[512];
   	 snprintf(cmd, sizeof(cmd), "termux-open \"%s\"", filepath);
    int ret = system(cmd);
    if (ret != 0) {
        show_message("打开文件失败，请确保已安装 termux-api");
    }
}

/*============check权限============-*/
void check_termux_allow_external(void) {
    const char *prop_path = "/data/data/com.termux/files/home/.termux/termux.properties";
    FILE *fp = fopen(prop_path, "r");
    int found = 0;

    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "allow-external-apps=true")) {
                found = 1;
                break;
            }
        }
        fclose(fp);
    }

    if (!found) {
        def_prog_mode();
        endwin();
        printf("\n⚠️  请在 ~/.termux/termux.properties 中添加：\n");
        printf("    allow-external-apps=true\n");
        printf("然后执行:\n");
        printf("    termux-reload-settings\n\n");
        printf("按任意键继续...\n");
        getchar();
        reset_prog_mode();
        refresh();
    }
}

/*==========路径转换辅助函数===========-*/
void convert_termux_path_to_android(char *android_path, const char *termux_path) {
    const char *prefix = "/storage/emulated/0/";
    const char *termux_prefix = "/data/data/com.termux/files/home/.storage/shared/";
    if (strncmp(termux_path, termux_prefix, strlen(termux_prefix)) == 0) {
        snprintf(android_path, MAX_PATH, "%s%s", prefix, termux_path + strlen(termux_prefix));
    } else {
        strcpy(android_path, termux_path);
    }
}

/*============GREP============-*/

void grep_prompt(void) {
    char keyword[256] = "";

    int win_height = 5;
    int win_width = 40;
    int starty = (term_height - win_height) / 2;
    int startx = (term_width - win_width) / 2;

    WINDOW *win = newwin(win_height, win_width, starty, startx);
    box(win, 0, 0);

    // 提示信息
    mvwprintw(win, 1, 2, "请输入关键词进行 grep 搜索：");

    // 输入框
    mvwprintw(win, 3, 2, "> ");
    wrefresh(win);

    echo();
    mvwgetnstr(win, 3, 4, keyword, sizeof(keyword) - 1);
    noecho();

    delwin(win);
    clear();
    refresh();

    if (strlen(keyword) == 0) return;

    char cmd[512];
	snprintf(cmd, sizeof(cmd), "clear && grep -rn \"%s\" .; echo; echo 按任意键返回...; read -n 1", keyword);
system(cmd);
    run_terminal_search(cmd);
}


/*============辅助函数============-*/
void find_prompt(void) {
    char input[256] = "";

    int win_height = 5;
    int win_width = 50;
    int starty = (term_height - win_height) / 2;
    int startx = (term_width - win_width) / 2;

    WINDOW *win = newwin(win_height, win_width, starty, startx);
    box(win, 0, 0);

    mvwprintw(win, 1, 2, "请输入 find 参数，例如 -iname \"*.jpg\"");
    mvwprintw(win, 3, 2, "> ");
    wrefresh(win);

    echo();
    mvwgetnstr(win, 3, 4, input, sizeof(input) - 1);
    noecho();

    delwin(win);
    clear();
    refresh();

    if (strlen(input) == 0) return;

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "find . %s", input);
    run_terminal_search(cmd);
}


/*==========功能函数：FIND==========-*/

void run_terminal_search(const char *cmd) {
    def_prog_mode();
    endwin();
    
	system("clear");         // 传入命令
	system(cmd);            //执行
	
    printf("\n\033[1;32m[RsExplor 搜索模式]\033[0m 执行命令：%s\n\n", cmd);
    int ret = system(cmd);
    if (ret != 0) {
        printf("\n\033[1;31m命令执行失败\033[0m 或无结果。\n");
    }

    printf("\n\033[1;33m按任意键返回文件浏览器...\033[0m\n");
    getchar();

    reset_prog_mode();
    refresh();
}

/*=======功能函数：BASH脚本============-*/
void execute_selected_file(const char *filepath) {
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, filepath);

    def_prog_mode();  // 保存当前 ncurses 状态
    endwin();         // 暂停 ncurses 模式

    char cmd[MAX_PATH + 10];
    snprintf(cmd, sizeof(cmd), "bash '%s'", filepath); // 或直接执行 '%s'
    system(cmd);

    printf("\n按回车键返回...");
    getchar();

    reset_prog_mode(); // 恢复 ncurses 模式
    refresh();
}

/*=======功能函数：打开编辑器============-*/
void edit_selected_file(const char *filepath) {
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, filepath);

    def_prog_mode();  // 保存当前 ncurses 状态
    endwin();         // 暂停 ncurses 模式

    char cmd[MAX_PATH + 10];
    snprintf(cmd, sizeof(cmd), "vi '%s'", filepath); // 或直接执行 '%s'
    system(cmd);

    printf("\n按回车键返回...");
    getchar();

    reset_prog_mode(); // 恢复 ncurses 模式
    refresh();
}


/*============辅助函数============-*/

// 路径验证
bool validate_path(const char *path) {
    if (path == NULL || strlen(path) == 0) {
        return false;
    }

    struct stat path_stat;
    if (stat(path, &path_stat) != 0) {
        return false;
    }

    return true;
}

/*========功能函数：============-*/
// 复制/剪切当前光标处文件
void copy_current_file(bool is_cut) {
    // 检查选择有效性（和你的删除函数一致）
    if (cursor_pos < 0 || cursor_pos >= file_count) {
        show_message_timed("无效的文件选择", 1000);
        return;
    }

    const FileEntry *selected_file = &files[cursor_pos];
    
    // 禁止操作特殊目录
    if (strcmp(selected_file->name, ".") == 0 || strcmp(selected_file->name, "..") == 0) {
        show_message_timed("不能操作系统目录", 1500);
        return;
    }

    // 存储完整路径到剪贴板
    snprintf(clipboard_path, sizeof(clipboard_path), "%s/%s", current_dir, selected_file->name);
    clipboard_is_cut = is_cut;

    // 用户反馈
    show_message_timed(is_cut ? "已剪切到剪贴板" : "已复制到剪贴板", 1500);
}
/*========功能包装函数============-*/
//包装
void copy_current_file_wrapper(void) {
    copy_current_file(false);  // 这里根据你需求传参数
}


/*========功能函数：============-*/
// 粘贴操作
void paste_file(void) {
    if (clipboard_path[0] == '\0') {
        show_message_timed("剪贴板为空", 1000);
        return;
    }

    // 获取目标文件名
    const char *filename = strrchr(clipboard_path, '/');
    filename = filename ? filename + 1 : clipboard_path;

    // 构造目标路径
    char target_path[MAX_PATH];
    snprintf(target_path, sizeof(target_path), "%s/%s", current_dir, filename);

    // 检查是否要覆盖
    if (access(target_path, F_OK) == 0) {
        char confirm_msg[256];
        snprintf(confirm_msg, sizeof(confirm_msg), "目标已存在，覆盖？ %s", filename);
        if (!confirm_action(confirm_msg)) return;
    }

    // 执行操作
    if (clipboard_is_cut) {
        // 剪切 = 移动文件
        if (rename(clipboard_path, target_path) == 0) {
            show_message_timed("移动成功", 1000);
            refresh_dir();
        } else {
            show_message_timed("移动失败", 1000);
        }
    } else {
        // 复制 = 实际拷贝文件
        if (copy_file(clipboard_path, target_path)) {
            show_message_timed("复制成功", 800);
            refresh_dir();
        } else {
            show_message_timed("复制失败", 800);
        }
    }

    // 清空剪贴板（如果是剪切操作）
    if (clipboard_is_cut) clipboard_path[0] = '\0';
}

// ===== 辅助函数 ======================
// 文件复制实现（与之前相同，但移除了目录处理）
bool copy_file(const char *src, const char *dst) {
    FILE *src_f = fopen(src, "rb");
    if (!src_f) return false;

    FILE *dst_f = fopen(dst, "wb");
    if (!dst_f) {
        fclose(src_f);
        return false;
    }

    char buf[8192];
    size_t bytes;
    while ((bytes = fread(buf, 1, sizeof(buf), src_f)) > 0) {
        if (fwrite(buf, 1, bytes, dst_f) != bytes) {
            fclose(src_f);
            fclose(dst_f);
            remove(dst);
            return false;
        }
    }

    fclose(src_f);
    fclose(dst_f);
    return true;
}

/*============辅助函数============-*/
// 安全获取回退路径
const char *get_fallback_path(void) {
    static char fallback[MAX_PATH];
    
    // 尝试当前目录
    if (access(current_dir, F_OK) == 0) {
        return current_dir;
    }
    
    // 回退到根目录
    strcpy(fallback, ROOT_DIR);
    return fallback;
}

void handle_number_selection(void) {
    if (num_input_len > 0) {
        int selected_num = atoi(number_input);
        if (selected_num >= 0 && selected_num < file_count) {
            cursor_pos = selected_num;
        }
        num_input_len = 0;
        number_input[0] = '\0';
    }
}

/*============功能函数============-*/
void handle_number_input(int ch) {
    if (isdigit(ch) && num_input_len < (int)(sizeof(number_input) - 1)) {
        number_input[num_input_len++] = ch;
        number_input[num_input_len] = '\0';
    }
}

/* 处理打开文件或目录 */
void handle_open_file(void) {
    num_input_len = 0;
    number_input[0] = '\0';

    char full_path[MAX_PATH];
    if (strncmp(current_dir, "find://", 7) == 0) {
        strcpy(full_path, files[cursor_pos].name);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, files[cursor_pos].name);
    }

    if (files[cursor_pos].is_dir) {
        scan_directory(full_path);
        cursor_pos = 0;
    } else if (files[cursor_pos].is_image) {
        view_image(full_path);
    } else {
        show_exec_menu(term_height - 6, 2, full_path);  // 举例：显示在左下角
    }
}

void refresh_dir(void){
	
	scan_directory(current_dir);
	
}


/*============回收站功能函数============-*/

void cleanup(void) {
    clear_trash();
}

/*============功能函数============-*/
// 创建目录（包括父目录）
int create_directory(const char *path) {
    char tmp[MAX_PATH];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0700) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    return mkdir(tmp, 0700);
}

/*============功能函数============-*/
// 递归创建目录实现
int mkdir_p(const char *path, mode_t mode) {
    char tmp[MAX_PATH];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    return mkdir(tmp, mode);
}

/*============功能函数============-*/
// 确认函数
bool confirm_action(const char *prompt) {
    char keyword[2];
    prompt_input(prompt, keyword, sizeof(keyword));
    if (keyword[0] == 'n' || keyword[0] == 'N') {
        return false;
    }
    return true;
}

/*============功能函数============-*/
void delete_current_file(void) {
    // 检查当前选择是否有效
    if (cursor_pos < 0 || cursor_pos >= file_count) {
        show_message_confim("无效的文件选择");
        return;
    }

    const FileEntry *selected_file = &files[cursor_pos];
    
    // 禁止删除特殊目录
    if (strcmp(selected_file->name, ".") == 0 || strcmp(selected_file->name, "..") == 0) {
        show_message_confim("不能删除系统目录");
        return;
    }

    // 构造完整路径
    snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, selected_file->name);
    snprintf(trash_path, sizeof(trash_path), "%s/%s", g_trash_dir, selected_file->name);

    // 确保回收站目录存在（带错误检查）
    if (mkdir_p(g_trash_dir, 0700) != 0 && errno != EEXIST) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "无法创建回收站: %s", strerror(errno));
        show_message(error_msg);
        return;
    }

    // 检查目标文件是否已存在回收站中
    if (access(trash_path, F_OK) == 0) {
        char confirm_msg[256];
        snprintf(confirm_msg, sizeof(confirm_msg), "回收站已存在同名文件，覆盖？(y/N) %s", selected_file->name);
        
        if (!confirm_action(confirm_msg)) {
            show_message_confim("取消删除");
            return;
        }
        if (unlink(trash_path) != 0) {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "无法删除旧文件: %s", strerror(errno));
            show_message(error_msg);
            return;
        }
    }

    // 执行移动操作
    if (rename(full_path, trash_path) == 0) {
        char success_msg[256];
        snprintf(success_msg, sizeof(success_msg), "已移动到回收站: %s", selected_file->name);
        show_message(success_msg);
        refresh_dir(); // 刷新当前目录显示
    } else {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "移动失败: %s", strerror(errno));
        show_message(error_msg);
    }
}

/*============功能函数============-*/
// 还原文件函数

void restore_from_trash(void) {
    if (trash_count <= 0) {
        show_message_confim("回收站为空");
        return;
    }

    // 显示回收站文件列表
    clear();
    printw("回收站中的文件 (共%d个):\n", trash_count);
    for (int i = 0; i < trash_count; i++) {
        printw("%d. %s (原位置: %s)\n", 
              i + 1, 
              basename(file_trash[i].name),
              file_trash[i].path);
    }
    printw("\n按 q 退出\n");
    refresh();

    // 获取用户选择
    echo();
    char input[16];
    printw("请输入要恢复的文件编号: ");
    getstr(input);
    noecho();

    if (input[0] == 'q' || input[0] == 'Q') {
        return;
    }

    int selected_index = atoi(input) - 1;
    if (selected_index < 0 || selected_index >= trash_count) {
        show_message_confim("无效的文件编号");
        return;
    }

    // 获取选中的文件信息
    TrashEntry *entry = &file_trash[selected_index];
    char *trash_path = entry->trash_path;
    char *original_path = entry->path;

    // 检查回收站文件是否存在
    if (access(trash_path, F_OK) != 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "错误: 回收站文件不存在 [%s]", trash_path);
        show_message(msg);
        return;
    }

    // 创建原始路径的父目录（如果需要）
    char parent_dir[MAX_PATH];
    snprintf(parent_dir, sizeof(parent_dir), "%s", original_path);
    char *last_slash = strrchr(parent_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        if (access(parent_dir, F_OK) != 0) {
            if (mkdir_p(parent_dir, 0755) != 0) {
                char msg[256];
                snprintf(msg, sizeof(msg), "无法创建目录: %s", parent_dir);
                show_message(msg);
                return;
            }
        }
    }

    // 检查目标位置是否冲突
    if (access(original_path, F_OK) == 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "目标已存在: %s", original_path);
        show_message(msg);
        return;
    }

    // 执行恢复操作
    if (rename(trash_path, original_path) == 0) {
        // 从回收站数组中移除该项
        for (int i = selected_index; i < trash_count - 1; i++) {
            memcpy(&file_trash[i], &file_trash[i + 1], sizeof(TrashEntry));
        }
        trash_count--;

        char msg[256];
        snprintf(msg, sizeof(msg), "成功恢复: %s → %s", 
                basename(trash_path), original_path);
        show_message(msg);
        refresh_dir();
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "恢复失败: %s", strerror(errno));
        show_message(msg);
    }
}

/*============功能函数============-*/	

void clear_trash(void) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char full_path[1024];

    if (!(dir = opendir(g_trash_dir))) {
        perror("opendir failed");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", g_trash_dir, entry->d_name);

        if (lstat(full_path, &statbuf) == -1) {
            perror("lstat failed");
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            // 递归处理子目录（调用自身）
            clear_trash();  // 不再传递参数
            rmdir(full_path);
        } else {
            unlink(full_path);
        }
    }
    closedir(dir);
}


/*============功能包装函数============-*/
void handle_delete_file(void) {

        delete_current_file();
}

void handle_restore_file(void){
	
	restore_from_trash() ;
	
}

void handle_empty_trash(void){
	
	    clear_trash();
	
}

/*============功能函数============-*/
// 示例操作函数（你可替换为真正函数）
void handle_main_action(int index, const char *filepath) {
    char cmd[512];
    switch (index) {
        case 0: snprintf(cmd, sizeof(cmd), "bash \"%s\"", filepath); break;
        case 1: snprintf(cmd, sizeof(cmd), "vi \"%s\"", filepath); break;
        case 2: snprintf(cmd, sizeof(cmd), "xdg-open \"%s\"", filepath); break;
        case 3: snprintf(cmd, sizeof(cmd), "file \"%s\"; read -n 1 -s", filepath); break;
        case 4: snprintf(cmd, sizeof(cmd), "rm -i \"%s\"; read -n 1 -s", filepath); break;
        default: return;
    }
    endwin();
    system(cmd);
    printf("\n按任意键返回...");
    getchar();
    initscr();
    refresh();
}

/*============§☆☆☆§============-*/

void handle_compressed_action(int index, const char *filepath) {
    char cmd[512];
    switch (index) {
        case 0: snprintf(cmd, sizeof(cmd), "unzip -o \"%s\" -d extracted; read -n 1 -s", filepath); break;
        case 1: snprintf(cmd, sizeof(cmd), "unzip -l \"%s\"; read -n 1 -s", filepath); break;
        case 2: snprintf(cmd, sizeof(cmd), "zip -r compressed.zip \"%s\"; read -n 1 -s", filepath); break;
        default: return;
    }
    endwin();
    system(cmd);
    printf("\n按任意键返回...");
    getchar();
    initscr();
    refresh();
}


/*============§☆☆☆§============-*/
void extract_archive(const char *filepath) {
	char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, filepath);

    def_prog_mode();  // 保存当前 ncurses 状态
    endwin();        // 暂停 ncurses 模式
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "clear; unzip '%s' -d .; echo; read -n1 -p '按任意键返回...'", filepath);
    system(cmd);
    
    printf("\n按回车键返回...");
    getchar();

    reset_prog_mode(); // 恢复 ncurses 模式
    refresh();
}

/*============§☆☆☆§============-*/
void list_archive_contents(const char *filepath) {
	char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, filepath);

    def_prog_mode();  // 保存当前 ncurses 状态
    endwin();         // 暂停 ncurses 模式
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "clear; unzip -l '%s'; echo; read -n1 -p '按任意键返回...'", filepath);
    system(cmd);
    
    printf("\n按回车键返回...");
    getchar();

    reset_prog_mode(); // 恢复 ncurses 模式
    refresh();
}

/*============§☆☆☆§============-*/
void test_archive(const char *filepath) {
	char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, filepath);

    def_prog_mode();  // 保存当前 ncurses 状态
    endwin();         // 暂停 ncurses 模式
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "clear; unzip -t '%s'; echo; read -n1 -p '按任意键返回...'", filepath);
    system(cmd);
    
    printf("\n按回车键返回...");
    getchar();

    reset_prog_mode(); // 恢复 ncurses 模式
    refresh();
}


/*============§☆☆☆§============-*/
//############重要分割线#############//
/*==============§☆☆☆§==========-*/

void shell_in_current_dir(const char *current_dir) {
endwin();  // 暂时退出 curses 控制台

// 切换目录并运行子shell
if (chdir(current_dir) == 0) {
    printf("[INFO] 当前目录：%s\n", current_dir);
    printf("[INFO] 正在进入 bash shell，退出后返回...\n");
    fflush(stdout);

    pid_t pid = fork();
  	  if (pid == 0) {
    	    execlp("bash", "bash", NULL);  // or "zsh"
  	      perror("execlp");
    	    exit(1);
    	} else if (pid > 0) {
   	     int status;
     	   waitpid(pid, &status, 0);
   	 }
	}

// ===== 恢复 rsexplor 的 ncurses 界面 =====
		init_curses_ui();
		signal(SIGINT, handle_signal);   // 如有需要，重新设置 signal handler

		update_layout();                // 重绘大小
		filelist_max_rows = term_height - 5;
		scan_directory(current_dir);    // 重新扫描目录

}

// 假设 current_path 是全局变量或你可以在这里访问它

void shell_wrapper(void) {
    shell_in_current_dir(current_dir);
}

/*============§☆☆☆§============-*/
//############重要分割线#############//
/*==============§☆☆☆§==========-*/

const char *get_cursor_path(FileBrowser *browser) {
    if (!browser) return NULL;
    if (browser->cursor_pos < 0 || browser->cursor_pos >= browser->total_files) {
        return NULL;
    }
    return browser->files[browser->cursor_pos].full_path;
}

void delete_current_entry(void) {
    // 检查有效位置
    if (cursor_pos < 0 || cursor_pos >= file_count) {
        show_message_timed("无效的文件选择", 1000);
        return;
    }

    const FileEntry *selected_file = &files[cursor_pos];

    // 防止删除 . 和 ..
    if (strcmp(selected_file->name, ".") == 0 || strcmp(selected_file->name, "..") == 0) {
        show_message_timed("不能删除系统目录", 1500);
        return;
    }

    // 拼接完整路径
    char target_path[MAX_PATH];
    snprintf(target_path, sizeof(target_path), "%s/%s", current_dir, selected_file->name);

    // 构造确认消息
    char msg[256];
    snprintf(msg, sizeof(msg), "确认删除？\n%s", selected_file->name);

    // 弹出确认框
    show_message_confim(msg);

    // 执行 rm -rf 命令
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", target_path);
    int ret = system(cmd);

    // 提示结果
    if (ret == 0) {
        show_message_timed("删除成功", 1000);
    } else {
        show_message_timed("删除失败", 1000);
    }

    // 刷新文件列表（如有需要）
   	 update_layout(); // 如果你已有此函数
}
