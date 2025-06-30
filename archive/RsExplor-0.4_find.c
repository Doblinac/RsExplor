#include <ctype.h>
#include <ncurses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <libgen.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>

#define MAX_PATH 1024
#define MAX_FILES 1000
#define COLUMNS 2  // 恢复两列显示
#define MAX_ROWS 15
#define ROOT_DIR "/storage/emulated/0"
#define SEARCH_TIMEOUT 2

// 颜色定义
#define COLOR_DIR 1
#define COLOR_FILE 2
#define COLOR_EXEC 3
#define COLOR_SELECTED 4
#define COLOR_TITLE 5
#define COLOR_STATUS 6
#define COLOR_PATHBAR 7
#define COLOR_MENU 8
#define COLOR_NUMBER 9
#define COLOR_IMAGE 10

typedef struct {
    char name[MAX_PATH];
    int is_dir;
    int is_exec;
    int is_image;
    int is_hidden; 
} FileEntry;

FileEntry files[MAX_FILES];
int file_count = 0;
char current_dir[MAX_PATH];
int cursor_pos = 0;
int term_width, term_height;
char previous_dir[MAX_PATH];
char number_input[10] = "";
int num_input_len = 0;
int awaiting_c_prefix = 0;
int clipboard_mode = 0; // 0=无, 1=复制, 2=移动
char clipboard_path[MAX_PATH] = "";
int in_find_mode = 0;  // 是否处于find模式
int show_hidden_files = 0;     // 是否显示隐藏文件
char search_root[MAX_PATH] = ""; // 记录搜索根目录
char find_back_path[MAX_PATH] = "";  // 搜索前目录备份
int last_cursor_pos = 0;            // 记录搜索前光标位置
void init_colors() {
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
}

void show_message(const char *msg);
void prompt_input(const char *prompt, char *buf, int bufsize);
int is_hidden_file(const char *filename);
void handle_navigation_key(int ch);
void render(void);
int compare_files(const void *a, const void *b);
int is_hidden_file(const char *filename);

int is_hidden_file(const char *filename) {
    // 简单判断以 '.' 开头为隐藏
    const char *base = strrchr(filename, '/');
    base = base ? base + 1 : filename;
    return (base[0] == '.');
}

// 处理上下左右等按键移动光标
void handle_navigation_key(int ch) {
    int items_per_col = (file_count + COLUMNS - 1) / COLUMNS;
    
    switch(ch) {
        case KEY_UP:
            if (cursor_pos > 0) cursor_pos--;
            break;
        case KEY_DOWN:
            if (cursor_pos < file_count - 1) cursor_pos++;
            break;
        case KEY_LEFT: {
            if (cursor_pos >= items_per_col) 
                cursor_pos -= items_per_col;
            break;
        }
        case KEY_RIGHT: {
            if (cursor_pos + items_per_col < file_count) 
                cursor_pos += items_per_col;
            break;
        }
    }
}

void draw_path_bar() {
    attron(COLOR_PAIR(COLOR_PATHBAR));
    for (int i = 0; i < term_width; i++) {
        mvaddch(1, i, ACS_HLINE);
    }
    mvaddch(1, 0, ACS_LTEE);
    mvaddch(1, term_width-1, ACS_RTEE);
    
    mvprintw(0, 2, " 路径: ");
    attron(A_BOLD);
    printw("%s", current_dir);
    attroff(A_BOLD);
    attroff(COLOR_PAIR(COLOR_PATHBAR));
}

void draw_border() {
    attron(COLOR_PAIR(COLOR_TITLE));
    for (int i = 0; i < term_width; i++) {
        mvaddch(2, i, ACS_HLINE);
        mvaddch(term_height-3, i, ACS_HLINE);
    }
    for (int i = 3; i < term_height-3; i++) {
        mvaddch(i, 0, ACS_VLINE);
        mvaddch(i, term_width-1, ACS_VLINE);
    }
    mvaddch(2, 0, ACS_ULCORNER);
    mvaddch(2, term_width-1, ACS_URCORNER);
    mvaddch(term_height-3, 0, ACS_LLCORNER);
    mvaddch(term_height-3, term_width-1, ACS_LRCORNER);
    mvprintw(2, 2, " 文件浏览器 ");
    attroff(COLOR_PAIR(COLOR_TITLE));
}

int is_image_file(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;
    
    return strcasecmp(ext, ".jpg") == 0 ||
           strcasecmp(ext, ".jpeg") == 0 ||
           strcasecmp(ext, ".png") == 0 ||
           strcasecmp(ext, ".gif") == 0 ||
           strcasecmp(ext, ".bmp") == 0;
}

void scan_directory(const char *path) {
    // 新增：跳过find模式的路径解析
    if (in_find_mode) return;
    
    // 特殊处理find模式
    if (strncmp(path, "find://", 7) == 0) {
        in_find_mode = 1;
        strcpy(current_dir, path);
        return;
    }

    // 普通目录模式
    in_find_mode = 0;
    char resolved_path[PATH_MAX];
    
    if (realpath(path, resolved_path) == NULL) {
        show_message("路径解析失败");
        beep();
        return;
    }

    if (strncmp(resolved_path, ROOT_DIR, strlen(ROOT_DIR)) != 0) {
        show_message("超出访问范围");
        beep();
        return;
    }

    DIR *dir = opendir(resolved_path);
    if (!dir) {
        show_message("无法打开目录");
        beep();
        return;
    }

    strncpy(current_dir, resolved_path, MAX_PATH);
    file_count = 0;

    if (strcmp(resolved_path, ROOT_DIR) != 0) {
        strcpy(files[file_count].name, "..");
        files[file_count].is_dir = 1;
        files[file_count].is_exec = 0;
        files[file_count].is_image = 0;
        file_count++;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && file_count < MAX_FILES) {
        if (!show_hidden_files && entry->d_name[0] == '.') continue;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        strncpy(files[file_count].name, entry->d_name, MAX_PATH);

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", resolved_path, entry->d_name);

        struct stat statbuf;
        if (stat(full_path, &statbuf) == 0) {
            files[file_count].is_dir = S_ISDIR(statbuf.st_mode);
            files[file_count].is_exec = (statbuf.st_mode & S_IXUSR) ? 1 : 0;
            files[file_count].is_image = is_image_file(entry->d_name);
        }
        file_count++;
    }
    closedir(dir);

    // 修改排序函数为普通C函数
  		  if (!in_find_mode) {
      	  qsort(files, file_count, sizeof(FileEntry), compare_files);
  		  }
		}

// 添加比较函数（替代lambda）
int compare_files(const void *a, const void *b) {
    const FileEntry *fa = (const FileEntry *)a;
    const FileEntry *fb = (const FileEntry *)b;
    
    if (fa->is_dir && !fb->is_dir) return -1;
    if (!fa->is_dir && fb->is_dir) return 1;
    
    return strcasecmp(fa->name, fb->name);
}

//绘制文件列表
void draw_file_list() {
    int items_per_col = (file_count + COLUMNS - 1) / COLUMNS;
    int col_width = term_width / COLUMNS - 6; // 列宽稍微加宽
    
    for (int col = 0; col < COLUMNS; col++) {
        for (int row = 0; row < items_per_col; row++) {
            int idx = col * items_per_col + row;
            if (idx >= file_count) continue;
            
            int y = row + 4;
            int x = col * (term_width / COLUMNS) + 2;
            
            // 显示数字编号（find模式和普通模式都显示）
            attron(COLOR_PAIR(COLOR_NUMBER));
            mvprintw(y, x, "%2d ", idx+1);
            x += 3;
            attroff(COLOR_PAIR(COLOR_NUMBER));
            
            // 文件类型颜色
            int color_pair;
            if (files[idx].is_dir) {
                color_pair = COLOR_DIR;
            } else if (files[idx].is_exec) {
                color_pair = COLOR_EXEC;
            } else if (files[idx].is_image) {
                color_pair = COLOR_IMAGE;
            } else {
                color_pair = COLOR_FILE;
            }
            
            // 高亮选中项
            if (idx == cursor_pos) {
                attron(COLOR_PAIR(COLOR_SELECTED));
            } else {
                attron(COLOR_PAIR(color_pair));
            }
            
            // 准备显示内容
            char display_name[col_width + 1];
            
            if (in_find_mode) {
                // FIND模式：显示相对路径（已处理为./开头）
                strncpy(display_name, files[idx].name, col_width);
            } else {
                // 普通模式：只显示文件名
                strncpy(display_name, files[idx].name, col_width);
                
                // 如果是目录，添加/后缀
                if (files[idx].is_dir) {
                    strncat(display_name, "/", col_width - strlen(display_name));
                }
            }
            display_name[col_width] = '\0';
            
            // 添加图标
            const char *icon = " ";
            if (files[idx].is_dir) icon = "📁";
            else if (files[idx].is_image) icon = "🖼️";
            else if (files[idx].is_exec) icon = "⚙️";
            
            mvprintw(y, x, "%s %s", icon, display_name);
            
            // 关闭属性
            attroff(COLOR_PAIR(idx == cursor_pos ? COLOR_SELECTED : color_pair));
        }
    }
}

void draw_status_bar() {
    attron(COLOR_PAIR(COLOR_STATUS));
    
    char status[256];
    if (in_find_mode) {
        snprintf(status, sizeof(status), 
            "搜索: %s | 找到 %d 个项目 | 数字+空格跳转",
            search_root, file_count);
    } else {
        snprintf(status, sizeof(status),
            "位置: %s | %s %s",
            current_dir,
            files[cursor_pos].name,
            files[cursor_pos].is_dir ? "(目录)" : 
            files[cursor_pos].is_exec ? "(可执行)" : 
            files[cursor_pos].is_image ? "(图片)" : "(文件)");
    }
    
    mvprintw(term_height-1, 0, "%-*s", term_width-1, status);
    
    // 显示数字输入
    if (num_input_len > 0) {
        mvprintw(term_height-1, term_width-10, "输入: %s", number_input);
    }
    
    attroff(COLOR_PAIR(COLOR_STATUS));
}

void execute_with_editor(const char *path) {
    def_prog_mode();
    endwin();
    
    printf("使用VI编辑器打开文件...\n输入:q退出编辑器\n\n");
    char cmd[MAX_PATH + 10];
    snprintf(cmd, sizeof(cmd), "vi \"%s\"", path);
    
    pid_t pid = fork();
    if (pid == 0) {
        execl("/system/bin/sh", "sh", "-c", cmd, (char *)0);
        perror("execl failed");
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
    
    reset_prog_mode();
    refresh();
}

// 调用 Bash
void execute_with_bash(const char *filepath) {
    char cmd[MAX_PATH + 10];
    snprintf(cmd, sizeof(cmd), "bash \"%s\"", filepath);
    endwin(); // 退出 ncurses 界面
    system(cmd);
    printf("按任意键返回...");
    getchar();
    initscr(); // 重新初始化 ncurses
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
}

// 显示执行逻辑
void show_exec_menu(const char *filepath) {
    const char *options[] = { "使用 vi 打开", "用 bash 执行", "取消" };
    const int option_count = 3;
    int selected = 0;

    // 刷新界面防止误触
    nodelay(stdscr, TRUE);
    while (getch() != ERR);
    nodelay(stdscr, FALSE);

    while (1) {
        // 显示菜单
        for (int i = 0; i < option_count; ++i) {
            if (i == selected) attron(A_REVERSE);
            mvprintw(term_height - 3 + i, 2, "%d. %s", i + 1, options[i]);
            attroff(A_REVERSE);
        }
        refresh();

        int ch = getch();
        if (ch == KEY_UP) {
            selected = (selected - 1 + option_count) % option_count;
        } else if (ch == KEY_DOWN) {
            selected = (selected + 1) % option_count;
        } else if (ch >= '1' && ch <= '3') {
            selected = ch - '1';
            ch = '\n';  // 模拟回车
        } else if (ch == '\n' || ch == KEY_ENTER) {
            break;
        } else if (ch == 27) { // ESC 取消
            selected = 2;
            break;
        }
    }

    // 清除菜单
    for (int i = 0; i < option_count; ++i) {
        move(term_height - 3 + i, 0);
        clrtoeol();
    }

    // 执行选项
    if (selected == 0) {
        execute_with_editor(filepath);
    } else if (selected == 1) {
        execute_with_bash(filepath);
    } else {
        // 取消，不执行任何操作
    }
}

// 重新绘制界面
void render() {
    clear();
    draw_path_bar();
    draw_border();
    draw_file_list();
    draw_status_bar();
    refresh();
}

// 调用 FIND 
void enter_find_mode() {
    // 备份当前状态（目录+光标位置）
    strncpy(find_back_path, current_dir, MAX_PATH-1);
    last_cursor_pos = cursor_pos;
    
    // 确保路径以/结尾（防止路径拼接错误）
    if (find_back_path[strlen(find_back_path)-1] != '/') {
        strcat(find_back_path, "/");
    }
    	
    // 新增：记录当前目录到全局变量
    strcpy(search_root, current_dir);
	
    char keyword[256];
    prompt_input("搜索当前目录及子目录 (如: *.txt): ", keyword, sizeof(keyword));
    if (keyword[0] == '\0') return;

    // 存储当前目录作为搜索根目录
    strcpy(search_root, current_dir);
    
    // 确保搜索根目录以/结尾
    if (search_root[strlen(search_root)-1] != '/') {
        strcat(search_root, "/");
    }

    // 构建find命令（限制3层子目录）
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "find '%s' -maxdepth 3 -iname '*%s*' 2>/dev/null",
        current_dir, keyword);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        show_message("搜索失败");
        return;
    }

    // 处理搜索结果
    file_count = 0;
    char result_path[MAX_PATH];
    
    while (fgets(result_path, sizeof(result_path), fp) && file_count < MAX_FILES) {
        result_path[strcspn(result_path, "\n")] = '\0';
        
        // 转换为相对路径（以./开头）
        if (strncmp(result_path, search_root, strlen(search_root)) == 0) {
            snprintf(files[file_count].name, MAX_PATH, ".%s", 
                    result_path + strlen(search_root));
        } else {
            strncpy(files[file_count].name, result_path, MAX_PATH);
        }

        // 设置文件属性
        struct stat statbuf;
        if (stat(result_path, &statbuf) == 0) {
            files[file_count].is_dir = S_ISDIR(statbuf.st_mode);
            files[file_count].is_exec = (statbuf.st_mode & S_IXUSR) ? 1 : 0;
            files[file_count].is_image = is_image_file(result_path);
        }
        file_count++;
    }
    pclose(fp);

    // 进入find模式
    in_find_mode = 1;
    strcpy(current_dir, "find://results");
    show_message(file_count ? "找到结果，按ESC返回" : "无匹配结果");
}

void execute_current_file() {
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, files[cursor_pos].name);
    
    def_prog_mode();
    endwin();
    
    printf("执行: %s\n", full_path);
    char cmd[MAX_PATH + 10];
    snprintf(cmd, sizeof(cmd), "bash \"%s\"", full_path);
    
    int ret = system(cmd);
    printf("\n返回码: %d\n按任意键继续...", ret);
    getchar();
    
    reset_prog_mode();
    refresh();
}

// 图片查看函数
void view_image(const char *path) {
    def_prog_mode();
    endwin();
    
    printf("正在查看图片: %s\n", path);
    
    char cmd[MAX_PATH + 20];
    snprintf(cmd, sizeof(cmd), "termux-open \"%s\"", path);
    
    int ret = system(cmd);
    if (ret != 0) {
        printf("无法打开图片，请确保已安装termux-api\n");
    }
    
    printf("按任意键继续...");
    getchar();
    
    reset_prog_mode();
    refresh();
}

// 处理C组合键
void handle_c_combination_key(int ch) {
    awaiting_c_prefix = 0; // 无论是否有效组合键都重置状态
    
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, files[cursor_pos].name);
    
    switch (ch) {
        case 'c': // 复制
            strncpy(clipboard_path, full_path, MAX_PATH-1);
            clipboard_mode = 1; // 1表示复制模式
            show_message("已复制到剪贴板");
            break;
            
        case 'v': // 移动
            strncpy(clipboard_path, full_path, MAX_PATH-1);
            clipboard_mode = 2; // 2表示移动模式
            show_message("已剪切到剪贴板");
            break;
            
        case 'p': // 粘贴
            if (clipboard_path[0] == '\0') {
                show_message("剪贴板为空");
                break;
            }
            
            char cmd[2*MAX_PATH + 20];
            if (clipboard_mode == 1) { // 复制
                snprintf(cmd, sizeof(cmd), "cp -r \"%s\" \"%s/\"", 
                        clipboard_path, current_dir);
            } else { // 移动
                snprintf(cmd, sizeof(cmd), "mv \"%s\" \"%s/\"", 
                        clipboard_path, current_dir);
            }
            
            int ret = system(cmd);
            if (ret == 0) {
                show_message(clipboard_mode == 1 ? "复制成功" : "移动成功");
                scan_directory(current_dir); // 刷新目录
            } else {
                show_message("操作失败");
            }
            break;
            
        default:
            show_message("未知组合键");
    }
}

// 处理数字输入（仅处理数字，空格和退格在主循环中处理）
void handle_number_input(int ch) {
    if (isdigit(ch) && num_input_len < sizeof(number_input)-1) {
        number_input[num_input_len++] = ch;
        number_input[num_input_len] = '\0';
    }
}

void handle_signal(int sig) {
    endwin();
    exit(0);
}

// FIND 快捷键提示信息
void show_message(const char *msg) {
    int y = term_height - 1; // 最底下一行
    attron(COLOR_PAIR(3));
    mvprintw(y, 0, "%-*s", term_width, msg); // 清除并显示信息
    attroff(COLOR_PAIR(3));
    refresh();
    napms(1000); // 暂停 1 秒钟
}

void prompt_input(const char *prompt, char *buf, int bufsize) {
    echo();             // 打开回显
    curs_set(1);        // 显示光标

    int y = term_height - 1;  // 最底行
    attron(COLOR_PAIR(3));
    mvprintw(y, 0, "%-*s", term_width, "");      // 清空该行
    mvprintw(y, 0, "%s", prompt);               // 显示提示
    attroff(COLOR_PAIR(3));
    move(y, strlen(prompt));                    // 光标移到提示后
    refresh();

    getnstr(buf, bufsize - 1);                  // 获取输入

    noecho();           // 关闭回显
    curs_set(0);        // 隐藏光标
}

int main() {
    initscr();
    getmaxyx(stdscr, term_height, term_width);
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    init_colors();
    signal(SIGINT, handle_signal);

    // 当前目录
    getcwd(current_dir, sizeof(current_dir));
    if (strncmp(current_dir, ROOT_DIR, strlen(ROOT_DIR)) != 0) {
        strcpy(current_dir, ROOT_DIR);
    }

    scan_directory(current_dir);  // 扫描当前目录

    // 剪切板相关变量
    int clipboard_mode = 0; // 1=复制，2=剪切
    char clipboard_path[MAX_PATH] = "";
    int awaiting_shortcut = 0; // 空格激活快捷键

    while (1) {
        clear();
        draw_path_bar();
        draw_border();
        draw_file_list();
        draw_status_bar();
        refresh();

        int ch = getch();

    // 1. 优先处理数字输入
   	 if (isdigit(ch)) {
    	    handle_number_input(ch);
   	     continue;
 	   }
    
    // 2. 处理空格确认（必须在数字输入后）
	    if (ch == ' ' && num_input_len > 0) {
    	    int selected_num = atoi(number_input) - 1;
   	     if (selected_num >= 0 && selected_num < file_count) {
       	     cursor_pos = selected_num;
        }
    	    num_input_len = 0;
   	     number_input[0] = '\0';
    	    continue;
  	  }
        
        if (ch == 'f' || ch == 'F') {
    		enter_find_mode();
    		continue;
		}

		    // 4. 处理C组合键
  	  if (awaiting_c_prefix) {
  	      handle_c_combination_key(ch);
   	     continue;
 	   } else if (ch == 'C') {
   	     awaiting_c_prefix = true;
  	      show_message("等待组合键: c=复制 p=粘贴 v=移动");
    	    continue;
	    }
    
			if (ch == 27 && in_find_mode) {  // ESC键
   			 in_find_mode = 0;
    
  			  if (access(find_back_path, F_OK) == 0 && 
     		   strncmp(find_back_path, ROOT_DIR, strlen(ROOT_DIR)) == 0) {
       		 scan_directory(find_back_path);
    		    cursor_pos = last_cursor_pos;  // 恢复光标位置
   		 } else {
       		 scan_directory(ROOT_DIR);      
   			 }
   		 continue;
			}	
       
        switch(ch) {
            case KEY_UP:
                if (cursor_pos > 0) cursor_pos--;
                break;
            case KEY_DOWN:
                if (cursor_pos < file_count - 1) cursor_pos++;
                break;
            case KEY_LEFT: {
                int items_per_col = (file_count + COLUMNS - 1) / COLUMNS;
                if (cursor_pos >= items_per_col) cursor_pos -= items_per_col;
                break;
            }
            case KEY_RIGHT: {
                int items_per_col = (file_count + COLUMNS - 1) / COLUMNS;
                if (cursor_pos + items_per_col < file_count) cursor_pos += items_per_col;
                break;
            }
            case '\n':
			case KEY_ENTER: {
    			num_input_len = 0;
    			number_input[0] = '\0';

   			 char full_path[MAX_PATH];
    			if (strncmp(current_dir, "find://", 7) == 0) {
        			strcpy(full_path, files[cursor_pos].name);  // 已是绝对路径
   			 } else {
        			snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, files[cursor_pos].name);
   			 }

   			 if (files[cursor_pos].is_dir) {
        			scan_directory(full_path);
        			cursor_pos = 0;
   			 } else if (files[cursor_pos].is_image) {
        			view_image(full_path);  // 图片直接打开
    			} else {
        			show_exec_menu(full_path);  // 其他类型弹菜单
    			}
    			break;
			}
            case 'q':
                endwin();
                return 0;
        }
    }
    
    endwin();
    return 0;
}