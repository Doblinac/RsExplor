#ifndef KEYBINDINGS_H
#define KEYBINDINGS_H

#include <stdbool.h>
#include <ncurses.h>
#include "common.h"
#include "ui.h"
#include "operations.h"
#include "sysinfo.h"


// 初始化绑定（加载动态绑定）
void init_keybindings(void);



/*============快捷键功能函数============-*/
// 保存动态绑定到配置文件
void save_dynamic_bindings_to_file(const char *filename);

// 从配置文件加载动态绑定
void load_dynamic_bindings_from_file(const char *filename);

// 绑定一个动态按键
bool dynamic_bind_key(int key, const char *func_name);

// 解绑动态按键
bool dynamic_unbind_key(int key);

// 处理按键事件
bool handle_keyevent(int ch);

// 显示帮助菜单
void show_keybindings_help(void);

// 调出快捷键绑定管理菜单
void binding_menu(void);


#endif // KEYBINDINGS_H

