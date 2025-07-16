#include "keybindings.h"
#include "common.h"
#include <string.h>
#include <ctype.h>
#include <ncurses.h>
#include <ncurses.h>
#include <panel.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "ui.h"
#include "operations.h"
#include "sysinfo.h"
#include "navigation.h"


// 最大动态绑定数
#define MAX_DYNAMIC_BINDINGS 64

/*========== 静态绑定区 ==========*/

// 保留方向键和固定按键的静态绑定
const StaticBinding static_bindings[] = {
    {KEY_UP,    "上移光标",    HANDLER_TYPE_VOID, {.void_handler = handle_cursor_up}},
    {KEY_DOWN,  "下移光标",    HANDLER_TYPE_VOID, {.void_handler = handle_cursor_down}},
    {KEY_LEFT,  "左移分栏",    HANDLER_TYPE_VOID, {.void_handler = handle_column_left}},
    {KEY_RIGHT, "右移分栏",    HANDLER_TYPE_VOID, {.void_handler = handle_column_right}},

    {'h', "返回主目录", HANDLER_TYPE_VOID, {.void_handler = navigate_to_home}},
    {'f', "文件搜索",   HANDLER_TYPE_VOID, {.void_handler = find_prompt}},
    {'g', "内容搜索",   HANDLER_TYPE_VOID, {.void_handler = grep_prompt}},
    {'w', "打开回收站", HANDLER_TYPE_VOID, {.void_handler = open_trash}},
    {'d', "删除文件",   HANDLER_TYPE_VOID, {.void_handler = delete_current_file}},
    {'c', "复制文件",   HANDLER_TYPE_VOID, {.void_handler = copy_current_file_wrapper}},
    {'p', "粘贴文件",   HANDLER_TYPE_VOID, {.void_handler = paste_file}},
    {'l', "返回上次目录", HANDLER_TYPE_VOID, {.void_handler = navigate_to_last}},
    {'e', "打开脚本库", HANDLER_TYPE_VOID, {.void_handler = open_bashscript_home}},
    {'u', "恢复一次文件", HANDLER_TYPE_VOID, {.void_handler = restore_from_trash }},
    {'r', "永久删除文件", HANDLER_TYPE_VOID, {.void_handler = delete_current_entry }},
    {'t', "清空回收站", HANDLER_TYPE_VOID, {.void_handler = clear_trash }},
    {'a', "显示隐藏文件", HANDLER_TYPE_VOID, {.void_handler = toggle_hidden_files}},
    {'z', "临时进入shell", HANDLER_TYPE_VOID, {.void_handler = shell_wrapper}},
    {27,   "返回/取消",  HANDLER_TYPE_VOID, { .void_handler = refresh_dir }},
	{'\n', "打开文件",   HANDLER_TYPE_VOID, { .void_handler = handle_open_file }},
	{'?',    "显示帮助",   HANDLER_TYPE_VOID, { .void_handler = show_keybindings_help }},
};

// ========== 动态绑定区 ==========

// 动态绑定表及计数
DynamicBinding dynamic_bindings[MAX_DYNAMIC_BINDINGS];
size_t dynamic_bindings_count = 0;

// ========== 功能函数注册区 ==========

// 这里请你把所有可绑定功能的函数名和函数指针填上
// 示例：
RegisteredFunction registered_functions[] = {
    {"show_sysinfo_popup", HANDLER_TYPE_VOID, .void_handler = show_sysinfo_popup},
    {"find_prompt", HANDLER_TYPE_VOID, .void_handler = find_prompt},
    {"open_trash", HANDLER_TYPE_VOID, .void_handler = open_trash},
    {"handle_delete_file", HANDLER_TYPE_VOID, .void_handler = handle_delete_file},
    // 你其他功能函数放这里...
    {NULL, 0, {NULL}} // 结束标志
};

// ========== 辅助函数 ==========

void save_dynamic_bindings_to_file(const char *filename) {
	(void)filename;
    FILE *fp = fopen(BINDINGS_CONFIG_FILE, "w");
    if (!fp) return;

    for (size_t i = 0; i < dynamic_bindings_count; i++) {
        fprintf(fp, "%d=%s\n", dynamic_bindings[i].key, dynamic_bindings[i].func_name);
    }

    fclose(fp);
}

void load_dynamic_bindings_from_file(const char *filename) {
	(void)filename;
    FILE *fp = fopen(BINDINGS_CONFIG_FILE, "r");
    if (!fp) return;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = 0;
        int key = atoi(line);
        char *func_name = eq + 1;
        char *newline = strchr(func_name, '\n');
        if (newline) *newline = 0;

        if (dynamic_bindings_count < MAX_DYNAMIC_BINDINGS) {
            dynamic_bindings[dynamic_bindings_count].key = key;
            strncpy(dynamic_bindings[dynamic_bindings_count].func_name, func_name, sizeof(dynamic_bindings[0].func_name) - 1);
            dynamic_bindings[dynamic_bindings_count].func_name[sizeof(dynamic_bindings[0].func_name) - 1] = 0;
            dynamic_bindings_count++;
        }
    }
    fclose(fp);
}

/*========功能函数============-*/
// 查找静态绑定处理函数
static bool handle_static_keyevent(int ch) {
    for (size_t i = 0; i < sizeof(static_bindings) / sizeof(static_bindings[0]); i++) {
        if (static_bindings[i].key == ch) {
            if (static_bindings[i].handler_type == HANDLER_TYPE_VOID && static_bindings[i].handler.void_handler) {
                static_bindings[i].handler.void_handler();
                return true;
            }
            // 如果你有其它类型，也可以拓展这里
        }
    }
    return false;
}

/*============功能函数============-*/
// 查找注册功能函数指针
static RegisteredFunction *find_registered_function(const char *name) {
    for (size_t i = 0; registered_functions[i].name != NULL; i++) {
        if (strcmp(registered_functions[i].name, name) == 0) {
            return &registered_functions[i];
        }
    }
    return NULL;
}


/*============功能函数============-*/
// 查找动态绑定中某按键对应函数名
static const char *get_funcname_for_key(int key) {
    for (size_t i = 0; i < dynamic_bindings_count; i++) {
        if (dynamic_bindings[i].key == key) {
            return dynamic_bindings[i].func_name;
        }
    }
    return NULL;
}


/*============功能函数============-*/
// 添加或更新动态绑定
bool dynamic_bind_key(int key, const char *func_name) {
    // 先检查是否已绑定，更新
    for (size_t i = 0; i < dynamic_bindings_count; i++) {
        if (dynamic_bindings[i].key == key) {
            strncpy(dynamic_bindings[i].func_name, func_name, sizeof(dynamic_bindings[i].func_name) - 1);
            dynamic_bindings[i].func_name[sizeof(dynamic_bindings[i].func_name) - 1] = 0;
            return true;
        }
    }
    // 新增绑定
    if (dynamic_bindings_count < MAX_DYNAMIC_BINDINGS) {
        dynamic_bindings[dynamic_bindings_count].key = key;
        strncpy(dynamic_bindings[dynamic_bindings_count].func_name, func_name, sizeof(dynamic_bindings[dynamic_bindings_count].func_name) - 1);
        dynamic_bindings[dynamic_bindings_count].func_name[sizeof(dynamic_bindings[dynamic_bindings_count].func_name) - 1] = 0;
        dynamic_bindings_count++;
        return true;
    }
    return false; // 已满
}

/*============功能函数============-*/
// 解绑按键绑定
bool dynamic_unbind_key(int key) {
    for (size_t i = 0; i < dynamic_bindings_count; i++) {
        if (dynamic_bindings[i].key == key) {
            // 用最后一个覆盖当前位置，减少碎片
            dynamic_bindings[i] = dynamic_bindings[dynamic_bindings_count - 1];
            dynamic_bindings_count--;
            return true;
        }
    }
    return false;
}

// ========== 主处理函数 ==========//

void init_keybindings(void) {
    dynamic_bindings_count = 0;
	load_dynamic_bindings_from_file(CONFIG_FILE);
}


/*============功能函数============-*/

bool handle_keyevent(int ch) {
    // 先静态绑定处理（方向键和固定按键）
    if (handle_static_keyevent(ch)) {
        return true;
        refresh_dir();
    }

    // 再动态绑定处理
    const char *funcname = get_funcname_for_key(ch);
    if (funcname) {
        RegisteredFunction *func = find_registered_function(funcname);
        if (func) {
            switch (func->type) {
                case HANDLER_TYPE_VOID:
                    func->void_handler();
                    return true;
                case HANDLER_TYPE_PANEL:
                    func->panel_handler(stdscr);
                    return true;
                case HANDLER_TYPE_CHAR:
                    func->char_handler("param");
                    return true;
                case HANDLER_TYPE_BOOL:
                    func->bool_handler("arg1", "arg2");
                    return true;
            }
        }
    }

    // 未处理
    return false;
    refresh_dir();
}

/*============ 按键帮助==============*/
void show_keybindings_help(void) {
    clear();
    printw("=== 快捷键帮助 ===\n\n");

    // 先显示静态绑定
    size_t static_count = sizeof(static_bindings) / sizeof(static_bindings[0]);
    printw("【静态绑定快捷键】\n");
    for (size_t i = 0; i < static_count; i++) {
        int key = static_bindings[i].key;
        const char *desc = static_bindings[i].desc;
        const char *keyname = NULL;

        if (key >= 32 && key < 256) {
            printw(" %c : %s\n", key, desc);
        } else {
            switch (key) {
                case KEY_UP:    keyname = "↑"; break;
                case KEY_DOWN:  keyname = "↓"; break;
                case KEY_LEFT:  keyname = "←"; break;
                case KEY_RIGHT: keyname = "→"; break;
                case 27:        keyname = "ESC"; break;
                case '\n':      keyname = "ENTER"; break;
                default:        keyname = "?"; break;
            }
            printw("%5s : %s\n", keyname, desc);
        }
    }

    // 再显示动态绑定
    printw("\n【动态绑定快捷键】\n");
    if (dynamic_bindings_count == 0) {
        printw("无动态绑定\n");
    } else {
        for (size_t i = 0; i < dynamic_bindings_count; i++) {
            int key = dynamic_bindings[i].key;
            const char *desc = dynamic_bindings[i].func_name;
            const char *keyname = NULL;

            if (key >= 32 && key < 256) {
                printw(" %c : %s\n", key, desc);
            } else {
                switch (key) {
                    case KEY_UP:    keyname = "↑"; break;
                    case KEY_DOWN:  keyname = "↓"; break;
                    case KEY_LEFT:  keyname = "←"; break;
                    case KEY_RIGHT: keyname = "→"; break;
                    case 27:        keyname = "ESC"; break;
                    case '\n':      keyname = "ENTER"; break;
                    default:        keyname = "?"; break;
                }
                printw("%5s : %s\n", keyname, desc);
            }
        }
    }

    printw("\n按任意键返回...");
    refresh();
    getch();
}

/*========BIND_MENU功能函数========-*/

// 获取键名字符串，方便显示（简易版）
static const char *my_keyname(int key) {
    switch(key) {
        case KEY_UP: return "↑";
        case KEY_DOWN: return "↓";
        case KEY_LEFT: return "←";
        case KEY_RIGHT: return "→";
        case 27: return "ESC";
        default:
            if (key >= 32 && key < 127) {
                static char s[2] = {0};
                s[0] = (char)key;
                return s;
            }
            return "?";
    }
}

/*============辅助函数============-*/
// 显示当前动态绑定列表
static void show_dynamic_bindings(WINDOW *win) {
    werase(win);
    box(win, 0, 0);
    mvwprintw(win, 0, 2, " 动态按键绑定 ");

    for (size_t i = 0; i < dynamic_bindings_count; i++) {
        mvwprintw(win, (int)i + 1, 2, "%s : %s",
                  my_keyname(dynamic_bindings[i].key),
                  dynamic_bindings[i].func_name);
    }
    wrefresh(win);
}

/*============辅助函数============-*/
// 选择功能函数
static const char* select_function(WINDOW *win) {
    int highlight = 0;
    int ch;
    size_t count = 0;
    // 计算注册函数数量
    while (registered_functions[count].name != NULL) count++;

    keypad(win, TRUE);
    while(1) {
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 0, 2, " 选择功能函数（上下键选择，Enter确认）");
        int start = (highlight < MENU_HEIGHT-2) ? 0 : (highlight - (MENU_HEIGHT-3));

        for (int i = 0; i < MENU_HEIGHT-2 && (start+i) < (int)count; i++) {
            if (start + i == highlight) {
                wattron(win, A_REVERSE);
            }
            mvwprintw(win, i+1, 2, "%s", registered_functions[start+i].name);
            if (start + i == highlight) {
                wattroff(win, A_REVERSE);
            }
        }
        wrefresh(win);
        ch = wgetch(win);
        if (ch == KEY_UP) {
            if (highlight > 0) highlight--;
        } else if (ch == KEY_DOWN) {
            if (highlight < (int)count-1) highlight++;
        } else if (ch == '\n' || ch == KEY_ENTER) {
            return registered_functions[highlight].name;
        } else if (ch == 27) { // ESC退出
            return NULL;
        }
    }
}

/*============辅助函数============-*/
// 绑定新按键
static void bind_new_key(WINDOW *win) {
    werase(win);
    box(win, 0, 0);
    mvwprintw(win, 1, 2, "请输入要绑定的按键(ESC取消): ");
    wrefresh(win);
    int ch = wgetch(win);
    if (ch == 27) return; // 取消
    if (ch == ERR) return;

    // 检查是否静态绑定按键，不能绑定
    int static_keys[] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
                         'h','f','g','w','d','c','p','l','r','a'};
    for (size_t i = 0; i < sizeof(static_keys)/sizeof(static_keys[0]); i++) {
        if (ch == static_keys[i]) {
            mvwprintw(win, 3, 2, "该按键为静态绑定，无法动态绑定。");
            wrefresh(win);
            wgetch(win);
            return;
        }
    }

    // 选择功能
    const char *func = select_function(win);
    if (!func) return;

    if (dynamic_bind_key(ch, func)) {
        mvwprintw(win, 3, 2, "绑定成功: %s -> %s", my_keyname(ch), func);
    } else {
        mvwprintw(win, 3, 2, "绑定失败，绑定表已满。");
    }
    wrefresh(win);
    wgetch(win);
}

/*============辅助函数============-*/
// 解绑按键
static void unbind_key(WINDOW *win) {
    werase(win);
    box(win, 0, 0);
    mvwprintw(win, 1, 2, "请输入要解绑的按键(ESC取消): ");
    wrefresh(win);
    int ch = wgetch(win);
    if (ch == 27) return;
    if (dynamic_unbind_key(ch)) {
        mvwprintw(win, 3, 2, "解绑成功: %s", my_keyname(ch));
    } else {
        mvwprintw(win, 3, 2, "该按键未绑定。");
    }
    wrefresh(win);
    wgetch(win);
}


/*============键值菜单函数============-*/
// 绑定管理主菜单
// 从配置文件加载字段值，设置全局变量和环境变量
void binding_menu(void) {
    const char *items[ITEM_COUNT] = {
        "1. 显示动态绑定",
        "2. 绑定新按键",
        "3. 解绑按键",
        "4. 保存绑定",
        "ESC 退出菜单"
    };

    int highlight = 0;  // 当前高亮的选项
    int ch;

    WINDOW *menu_win = newwin(MENU_HEIGHT + 1, MENU_WIDTH -7,
                              (LINES - MENU_HEIGHT) / 2,
                              (COLS - MENU_WIDTH) / 2);
    PANEL *menu_panel = new_panel(menu_win);
    keypad(menu_win, TRUE);
    wbkgd(menu_win, COLOR_PAIR(COLOR_MENU));
    top_panel(menu_panel);
    update_panels();
    doupdate();

    while (1) {
        werase(menu_win);
        box(menu_win, 0, 0);
        mvwprintw(menu_win, 1, 2, "快捷键绑定管理");

        for (int i = 0; i < ITEM_COUNT; ++i) {
            int y = 3 + i;  // 改为从第2行起画选项
            if (i == highlight) {
                wattron(menu_win, COLOR_PAIR(COLOR_SELECTED));
                mvwprintw(menu_win, y, 2, "%s", items[i]);
                wattroff(menu_win, COLOR_PAIR(COLOR_MENU_HL));
            } else {
                mvwprintw(menu_win, y, 2, "%s", items[i]);
            }
        }

        wrefresh(menu_win);
        ch = wgetch(menu_win);

        if (ch == KEY_UP) {
            highlight = (highlight - 1 + ITEM_COUNT) % ITEM_COUNT;
        } else if (ch == KEY_DOWN) {
            highlight = (highlight + 1) % ITEM_COUNT;
        } else if (ch == '\n') {
            switch (highlight) {
                case 0:
                    show_dynamic_bindings(menu_win);
                    wgetch(menu_win);
                    break;
                case 1:
                    bind_new_key(menu_win);
                    break;
                case 2:
                    unbind_key(menu_win);
                    break;
                case 3:
                    save_dynamic_bindings_to_file(CONFIG_FILE);
                    mvwprintw(menu_win, MENU_HEIGHT - 2, 2, "已保存！");
                    wrefresh(menu_win);
                    wgetch(menu_win);
                    break;
                case 4:
                    goto exit_menu;
            }
        } else if (ch == 27) {  // ESC
            break;
        }
    }

exit_menu:
    del_panel(menu_panel);
    delwin(menu_win);
    update_panels();
    doupdate();
}


