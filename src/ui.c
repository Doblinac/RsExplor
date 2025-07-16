#include "common.h"
#include "ui.h"
#include "operations.h"
#include "keybindings.h"

#include <ncurses.h>
#include <panel.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#define _POSIX_C_SOURCE 200809L


void init_curses_ui(void) {
    initscr();
    update_layout();   // 如果这个是基于 initscr 后的窗口布局，必须放这里
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    init_colors();
}

void handle_signal(int sig) {
	(void) sig;
    endwin();
    exit(0);
}

/*============填色功能函数============-*/
void init_colors(void) {
    start_color();
    init_pair(COLOR_DIR, COLOR_BLUE, COLOR_BLACK);
    init_pair(COLOR_FILE, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLOR_EXEC, COLOR_GREEN, COLOR_BLACK);
    init_pair(COLOR_SELECTED, COLOR_BLACK, COLOR_WHITE);
    init_pair(COLOR_TITLE, COLOR_CYAN, COLOR_BLACK);
    init_pair(COLOR_STATUS, COLOR_YELLOW, COLOR_BLACK);
    init_pair(COLOR_PATHBAR, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(COLOR_MENU, COLOR_GREEN, COLOR_BLACK);
    init_pair(COLOR_NUMBER, COLOR_YELLOW, COLOR_BLACK);
    init_pair(COLOR_IMAGE, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(COLOR_WARNING, COLOR_RED, COLOR_BLACK);
}


/*============更新准备打印============-*/

void update_layout(void) {
    getmaxyx(stdscr, term_height, term_width);
    statusbar_y = term_height - 1;
    filelist_max_rows = statusbar_y - filelist_start_y - 2; // 减少2行
    if (filelist_max_rows < 1) filelist_max_rows = 1;       // 防止负数或0
    files_per_page = filelist_max_rows * COLUMNS;
    filelist_max_rows = statusbar_y - filelist_start_y;
}

/*==========打印路径窗口函数============-*/

void draw_path_bar(void) {
    attron(COLOR_PAIR(COLOR_PATHBAR) | A_REVERSE);
    mvhline(pathbar_y, 0, ' ', term_width);  // 清空路径栏背景

    const char *prefix = "路径: ";
    mvprintw(pathbar_y, 2, "%s", prefix);
    attron(A_BOLD);

    // 拆分路径
    char temp_path[MAX_PATH];
    strncpy(temp_path, current_dir, sizeof(temp_path));
    temp_path[sizeof(temp_path)-1] = '\0'; // 保证 null 结尾

    char *components[32];  // 最多支持32层
    int count = 0;

    char *token = strtok(temp_path, "/");
    while (token && count < 32) {
        components[count++] = token;
        token = strtok(NULL, "/");
    }

    char short_path[MAX_PATH] = "../";
    short_path[0] = '\0';

    // 拼接最后3个组件（或更少）
    int start = count > 3 ? count - 3 : 0;
    for (int i = start; i < count; ++i) {
        strcat(short_path, components[i]);
        if (i < count - 1) strcat(short_path, "/");
    }

    printw("%s", short_path);

    attroff(A_BOLD);
    attroff(COLOR_PAIR(COLOR_PATHBAR) | A_REVERSE);
}

/*============主窗口函数============-*/
void draw_border(void) {
    attron(COLOR_PAIR(COLOR_TITLE));
    for (int i = 0; i < term_width; i++) {
        mvaddch(border_y, i, ACS_HLINE);
        mvaddch(statusbar_y - 1, i, ACS_HLINE);
    }
    for (int i = border_y + 1; i < statusbar_y - 1; i++) {
        mvaddch(i, 0, ACS_VLINE);
        mvaddch(i, term_width - 1, ACS_VLINE);
    }
    mvaddch(border_y, 0, ACS_ULCORNER);
    mvaddch(border_y, term_width - 1, ACS_URCORNER);
    mvaddch(statusbar_y - 1, 0, ACS_LLCORNER);
    mvaddch(statusbar_y - 1, term_width - 1, ACS_LRCORNER);
    mvprintw(border_y, 2, " RsExplor [ 文件管理 ]");
    attroff(COLOR_PAIR(COLOR_TITLE));
}


/*============主显示区============-*/
// 文件列表绘制（支持分页）
void draw_file_list(void) {
    int col_width = term_width / COLUMNS - 6;

    int max_rows = filelist_max_rows;
    int start_index = (cursor_pos / files_per_page) * files_per_page;

    for (int col = 0; col < COLUMNS; col++) {
        for (int row = 0; row < max_rows; row++) {
            int idx = start_index + col * max_rows + row;
            if (idx >= file_count) continue;

            int y = filelist_start_y + row+1;
            int x = col * (term_width / COLUMNS) + 2;

            // 显示编号
            attron(COLOR_PAIR(COLOR_NUMBER));
            mvprintw(y, x, "%2d ", idx == 0 ? 0 : idx);
            attroff(COLOR_PAIR(COLOR_NUMBER));
            x += 3;

            // 颜色判定
            int color_pair = COLOR_FILE;
            if (files[idx].is_dir) color_pair = COLOR_DIR;
            else if (files[idx].is_exec) color_pair = COLOR_EXEC;
            else if (files[idx].is_image) color_pair = COLOR_IMAGE;

            // 高亮选中项
            if (idx == cursor_pos) attron(COLOR_PAIR(COLOR_SELECTED));
            else attron(COLOR_PAIR(color_pair));

            char display_name[col_width + 1];
            strncpy(display_name, files[idx].name, col_width);
            display_name[col_width] = '\0';

            if (files[idx].is_dir) strncat(display_name, "/", col_width - strlen(display_name));

            const char *icon = " ";
            if (files[idx].is_dir) icon = "📁";
            else if (files[idx].is_image) icon = "🖼️";
            else if (files[idx].is_exec) icon = "⚙️";

            mvprintw(y, x, "%s %s", icon, display_name);
            attroff(COLOR_PAIR(idx == cursor_pos ? COLOR_SELECTED : color_pair));
        }
    }
}

/*============状态栏============-*/
void draw_status_bar(void) {
    if (term_height <= 1 || term_width < 20 || cursor_pos >= file_count) return;

    FileEntry *f = &files[cursor_pos];

    // 图标
    const char *icon = f->is_dir ? "📁" :
                     f->is_exec ? "⚡" :
                     f->is_image ? "🖼" : "📄";

    // 文件大小
    char size_str[16] = "";
    if (!f->is_dir) {
        if (f->size == (off_t)-1) {
            strcpy(size_str, "| (error)");
        } else if (f->size < 1024) {
            snprintf(size_str, sizeof(size_str), "| %ldB", (long)f->size);
        } else if (f->size < 1024 * 1024) {
            snprintf(size_str, sizeof(size_str), "| %.1fKB", (double)f->size / 1024.0);
        } else {
            snprintf(size_str, sizeof(size_str), "| %.1fMB", (double)f->size / (1024.0 * 1024));
        }
    }

    // 分页信息（右对齐，占 19 列）
    int current_page = (cursor_pos / files_per_page) + 1;
    int total_pages = (file_count + files_per_page - 1) / files_per_page;
    char page_info[32];
    snprintf(page_info, sizeof(page_info), "第 %d/%d页(%d)", current_page, total_pages, file_count);

    // 左侧宽度 = 剩余宽度
    int left_width = term_width - 20;  // 19列分页 + 1空格
    if (left_width < 10) left_width = 10;  // 最小限制

    // 拼接左边字符串
    char left_buf[512];
// 拼接完整左侧信息（icon name size），再统一裁剪
	char raw_info[512];
	snprintf(raw_info, sizeof(raw_info), "%s %s %s", icon, f->name, size_str);
// 裁剪至 left_width 宽度（确保不会超出）
	snprintf(left_buf, sizeof(left_buf), "%-*.*s", left_width, left_width, raw_info);

    // 显示状态栏
    attron(COLOR_PAIR(COLOR_STATUS));
    mvhline(term_height - 1, 0, ' ', term_width);  // 清空整行
    mvprintw(term_height - 1, 0, "%.*s", left_width, left_buf);
    mvprintw(term_height - 1, term_width - strlen(page_info) - 1, "%s", page_info);
    attroff(COLOR_PAIR(COLOR_STATUS));
}
/*============消息框============-*/

// 阻塞式实现
void show_message_confim(const char *msg, ...) {
    int width = strlen(msg) + 4;
    WINDOW *win = newwin(5, width, (LINES-5)/2, (COLS-width)/2);
    
    box(win, 0, 0);
    mvwprintw(win, 2, 2, "%s %s", msg, "[ ok ]");
    wrefresh(win);
    
    // 等待回车键
    noecho(); // 关闭输入回显
    while(getch() != '\n');
    
    delwin(win);
    refresh();
}

/*============§    §============-*/
// 增强版阻塞式
void show_message(const char *fmt, ...) {
    // 1. 解析可变参数
    va_list args;
    va_start(args, fmt);
    char full_msg[1024];
    vsnprintf(full_msg, sizeof(full_msg), fmt, args);
    va_end(args);

    // 2. 计算行数和最大宽度
    int line_count = 1;
    int max_width = 0;
    int current_width = 0;
    
    for (const char *p = full_msg; *p; p++) {
        if (*p == '\n') {
            line_count++;
            max_width = MAX(max_width, current_width);
            current_width = 0;
        } else {
            current_width++;
        }
    }
    max_width = MAX(max_width, current_width);
    
    // 3. 计算窗口尺寸（最小宽度20，高度动态）
    int win_width = MAX(max_width + 4, 20);  // 左右各2字符边距
    int win_height = line_count + 4;         // 上下各2行边距
    
    // 4. 创建居中窗口
    WINDOW *win = newwin(win_height, win_width, 
                        (LINES - win_height) / 2,
                        (COLS - win_width) / 2);
    
    // 5. 绘制边框和内容
    box(win, 0, 0);
    
    // 6. 逐行打印（自动居中）
    char *line = strtok(full_msg, "\n");
    int y = 2;  // 从第3行开始打印
    while (line) {
        int x = (win_width - strlen(line)) / 2;
        mvwprintw(win, y++, MAX(x, 2), "%s %s", line, "[ ok ]"); // 保证最小缩进2字符
        line = strtok(NULL, "\n");
    }
    
    // 7. 交互处理
    wrefresh(win);
    noecho();
    while (getch() != '\n');
    
    // 8. 清理
    delwin(win);
    refresh();
}
/*============§ §============-*/
// 非阻塞式实现
void show_message_timed(const char *msg, int ms) {
    int width = strlen(msg) + 4;
    WINDOW *win = newwin(5, width, (LINES-5)/2, (COLS-width)/2);
    
    box(win, 0, 0);
    mvwprintw(win, 2, 2, "%s", msg);
    wrefresh(win);
    
    // 延时后自动关闭
    napms(ms); // ncurses提供的毫秒级延时
    delwin(win);
    refresh();
}

/*============辅助函数============-*/
void prompt_input(const char *prompt, char *buf, int bufsize) {
    echo();
    curs_set(1);

    int y = term_height - 1;
    attron(COLOR_PAIR(COLOR_WARNING));
    mvprintw(y, 0, "%-*s", term_width, "");
    mvprintw(y, 0, "%s", prompt);
    attroff(COLOR_PAIR(COLOR_WARNING));
    move(y, strlen(prompt));
    refresh();

    getnstr(buf, bufsize - 1);

    noecho();
    curs_set(0);
}

/*============功能函数============-*/
void toggle_hidden_files(void) {
    show_hidden_files = !show_hidden_files;
    
    // 刷新当前目录显示
    scan_directory(current_dir);
    
    // 显示状态提示（使用你的消息显示函数）
    char msg[64];
    snprintf(msg, sizeof(msg), "隐藏文件: %s", 
            show_hidden_files ? "✔ 显示" : "✖ 隐藏");
    show_message(msg);
}

// 从 grep 结果显示格式中提取匹配行内容
const char *extract_grep_line(const char *display_text) {
    // 示例格式: "src/main.c [123] int main() {"
    const char *bracket = strchr(display_text, ']');
    if (bracket && *(bracket + 1) != '\0') {
        return bracket + 2;  // 跳过 "] " 两个字符
    }
    return display_text;  // 如果没有匹配到格式，返回原字符串
}


/*============§☆☆☆§============-*/

// 二级菜单：压缩包操作
int show_submenu(int menu_top, int menu_left, const char *filepath) {
	(void)filepath;
    const char *suboptions[] = {
        "解压到当前目录",
        "查看压缩内容",
        "测试压缩包完整性",
        "取消"
    };
    int count = sizeof(suboptions) / sizeof(suboptions[0]);
    int width = 28;
    int height = count + 2;
    int left = menu_left + 20; // 紧贴一级菜单右侧

    WINDOW *win = newwin(height, width, menu_top, left);
    PANEL *panel = new_panel(win);
    box(win, 0, 0);
    keypad(win, TRUE);

    int highlight = 0;
    int ch;

    while (1) {
        for (int i = 0; i < count; i++) {
            if (i == highlight) wattron(win, A_REVERSE);
            mvwprintw(win, i + 1, 1, "%-26s", suboptions[i]);
            wattroff(win, A_REVERSE);
        }
        update_panels();
        doupdate();

        ch = wgetch(win);
        if (ch == KEY_UP)
            highlight = (highlight - 1 + count) % count;
        else if (ch == KEY_DOWN)
            highlight = (highlight + 1) % count;
        else if (ch == '\n' || ch == KEY_ENTER || ch == 27)
            break;
    }

    hide_panel(panel);
    del_panel(panel);
    delwin(win);
    update_panels();
    doupdate();

    return highlight;
}


/*============§☆☆☆§============-*/

// 一级菜单
int show_exec_menu(int y, int x, const char *filepath) {
	(void)y, (void)x, (void)filepath;
    const char *options[] = {
        "取消",
        "BASH",
        "ViEdit",
        "安卓打开",
        "查看/解压"
    };
    int option_count = sizeof(options) / sizeof(options[0]);

    int menu_width = 20;
    int menu_height = option_count + 2;
    int menu_top = LINES - menu_height - 1;
    int menu_left = 0;

    WINDOW *win = newwin(menu_height, menu_width, menu_top, menu_left);
    PANEL *panel = new_panel(win);
    box(win, 0, 0);
    keypad(win, TRUE);

    int highlight = 0;
    int ch;

    while (1) {
        for (int i = 0; i < option_count; i++) {
            if (i == highlight) wattron(win, A_REVERSE);
            mvwprintw(win, i + 1, 1, "%-18s", options[i]);
            wattroff(win, A_REVERSE);
        }
        update_panels();
        doupdate();

        ch = wgetch(win);
        if (ch == KEY_UP)
            highlight = (highlight - 1 + option_count) % option_count;
        else if (ch == KEY_DOWN)
            highlight = (highlight + 1) % option_count;
        else if (ch == '\n' || ch == KEY_ENTER) {
            if (highlight == 4) {  // “压缩包操作”
                if (is_compressed_file(filepath)) {
                    int sub_sel = show_submenu(menu_top, menu_left, filepath);
                    if (sub_sel == 0) {
						extract_archive(filepath);
                    } else if (sub_sel == 1) {
                        list_archive_contents(filepath);
                    } else if (sub_sel == 2) {
                        test_archive(filepath);
                    }
                } else {
						show_message_timed("不是压缩文件", 600);
                }
                continue; // 回到一级菜单
            } else {
                break;
            }
        } else if (ch == 27) {  // ESC
            highlight = 0;
            break;
        }
    }

    hide_panel(panel);
    del_panel(panel);
    delwin(win);
    update_panels();
    doupdate();

    // 处理其他一级菜单选项
    switch (highlight) {
        case 0: break; // 取消
        case 1: execute_selected_file(filepath); break;
        case 2: edit_selected_file(filepath); break;
        case 3: open_with_termux_open(filepath); break;
    }

    return highlight;
}


/*============注册表函数============-*/

// 获取用户主目录
char *get_home_directory(void) {
    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "环境变量 HOME 未设置。\n");
        exit(1);
    }
    return strdup(home);
}

// 返回完整的配置文件路径：$HOME/.rsexplor/config.txt
char *get_config_file_path(void) {
    char *home = get_home_directory();
    size_t len = strlen(home) + strlen(CONFIG_FILE) + 2;
    char *config_path = malloc(len);
    if (!config_path) {
        fprintf(stderr, "内存分配失败。\n");
        exit(1);
    }
    snprintf(config_path, len, "%s/%s", home, CONFIG_FILE);
    free(home);
    return config_path;
}

/*============功能函数============-*/
// 确保配置目录存在（如 ~/.rsexplor）
void ensure_config_dir_exists(const char *config_path) {
    char *path_copy = strdup(config_path);
    if (!path_copy) return;

    char *dir = dirname(path_copy);
    struct stat st = {0};

    if (stat(dir, &st) == -1) {
        mkdir(dir, 0700);  // 只创建一级
    }

    free(path_copy);
}

/*============功能函数============-*/
// 创建默认配置文件，交互设置 BASH_HOME，同时写入 TRASH_DIR
void create_config_file(const char *config_path) {
    ensure_config_dir_exists(config_path);

    char bash_home[MAX_PATH];
    char trash_dir[MAX_PATH];
    printf("配置文件 %s 不存在。\n", config_path);

    // 输入 BASH_HOME
    while (1) {
        printf("请输入 BASH_HOME 的路径 (例如: /data/data/com.termux/files/home): ");
        if (!fgets(bash_home, MAX_PATH, stdin)) {
            fprintf(stderr, "读取输入失败。\n");
            exit(1);
        }
        bash_home[strcspn(bash_home, "\n")] = '\0';

        if (bash_home[0] == '\0') {
            printf("路径不能为空，请重新输入。\n");
            continue;
        }
        if (bash_home[0] != '/') {
            printf("路径必须是绝对路径，请重新输入。\n");
            continue;
        }
        break;
    }

    // 询问是否使用自定义 TRASH_DIR
    while (1) {
        char choice[8];
        printf("是否使用自定义 TRASH_DIR 路径？(y/n): ");
        if (!fgets(choice, sizeof(choice), stdin)) {
            fprintf(stderr, "读取输入失败。\n");
            exit(1);
        }
        choice[strcspn(choice, "\n")] = '\0';

        if (choice[0] == 'y' || choice[0] == 'Y') {
            printf("请输入 TRASH_DIR 的路径: ");
            if (!fgets(trash_dir, MAX_PATH, stdin)) {
                fprintf(stderr, "读取输入失败。\n");
                exit(1);
            }
            trash_dir[strcspn(trash_dir, "\n")] = '\0';
            if (trash_dir[0] == '\0') {
                printf("路径不能为空，请重新输入。\n");
                continue;
            }
            break;
        } else if (choice[0] == 'n' || choice[0] == 'N') {
            snprintf(trash_dir, MAX_PATH, "%s/.trash", bash_home);
            break;
        } else {
            printf("请输入 y 或 n。\n");
        }
    }

    // 写入配置文件
    FILE *file = fopen(config_path, "w");
    if (!file) {
        fprintf(stderr, "无法创建配置文件 %s。\n", config_path);
        exit(1);
    }

    fprintf(file, "BASH_HOME=%s\n", bash_home);
    fprintf(file, "TRASH_DIR=%s\n", trash_dir);
    fclose(file);

    printf("配置文件已创建。\n");
}

/*============功能函数============-*/
// 从配置文件加载字段值，设置全局变量和环境变量
void load_config(void) {
    char *config_path = get_config_file_path();
    FILE *file = fopen(config_path, "r");

    if (!file) {
        create_config_file(config_path);
        file = fopen(config_path, "r");
        if (!file) {
            fprintf(stderr, "无法打开配置文件 %s。\n", config_path);
            free(config_path);
            exit(1);
        }
    }

    char line[PATH_MAX * 2];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';  // 去除换行
        if (line[0] == '#' || line[0] == '\0') continue;

        char *key = strtok(line, "=");
        char *val = strtok(NULL, "");

        if (!key || !val) continue;

        if (strcmp(key, "BASH_HOME") == 0) {
            strncpy(g_bash_home, val, MAX_PATH - 1);
        } else if (strcmp(key, "TRASH_DIR") == 0) {
            strncpy(g_trash_dir, val, MAX_PATH - 1);
        }
    }

    fclose(file);

    // 设置默认值（兜底）
    if (g_bash_home[0] == '\0') {
        const char *home = getenv("HOME");
        strncpy(g_bash_home, home ? home : "/data/data/com.termux/files/home", MAX_PATH - 1);
    }
    if (g_trash_dir[0] == '\0') {
        snprintf(g_trash_dir, MAX_PATH, "%s/.trash", g_bash_home);
    }

    // 设置环境变量
    setenv("BASH_HOME", g_bash_home, 1);
    setenv("TRASH_DIR", g_trash_dir, 1);

    printf("配置文件路径: %s\n", config_path);
    printf("BASH_HOME 设置为: %s\n", g_bash_home);
    printf("TRASH_DIR 设置为: %s\n", g_trash_dir);

    free(config_path);
}




