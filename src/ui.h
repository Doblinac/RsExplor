#ifndef UI_H
#define UI_H

#include "common.h"

void init_curses_ui(void);
void handle_signal(int sig);
void init_colors(void);
void update_layout(void) ;
void draw_path_bar(void);
void draw_border(void);
void draw_file_list(void);
void draw_status_bar(void);
void highlight_cursor_position(void);
void show_message(const char *msg, ...);
void show_message_confim(const char *msg, ...);
void show_message_timed(const char *msg, int ms) ;
void prompt_input(const char *prompt, char *buf, int bufsize);
void toggle_hidden_files(void) ;
void show_info_window(const char *content);

int show_exec_menu(int y, int x, const char *filepath) ; // ✅ 正确定义

int show_submenu(int menu_top, int menu_left, const char *filepath);
/*============注册功能函数============-*/

char *get_home_directory(void);
char *get_config_file_path(void);
void create_config_file(const char *config_path);
void load_config(void);



#endif // UI_H
