#ifndef COMMON_H
#define COMMON_H

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
#include <sys/ioctl.h>
#include <termios.h>

#define MAX_PATH 4096   
#define MAX_FILES 1024  
#define COLUMNS 2
#define MAX_ROWS 15
#define ROOT_DIR "/storage/emulated/0"
#define SEARCH_TIMEOUT 2

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
#define COLOR_WARNING 11
#define COLOR_CURSOR_HL 12
#define COLOR_MENU_HL 13  // 高亮菜单项颜色，例如白底蓝字
#define TRASH_SIZE 100
#define CONFIG_FILE ".rsexplor/config.txt"
#define BINDINGS_CONFIG_FILE ".rsexplor/bindings.conf"
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define HOME_DIR getenv("HOME")  
#define MAX_SHOW_WIDTH 32
#define CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))
#define ROWS_PER_COLUMN 21
#define COLS_NUM 2
#define MENU_WIDTH 28
#define MAX_MENU_ITEMS 5
#define MENU_HEIGHT 8  // 5个选项 + 边框

#define MAX_LINES 1024
#define MAX_LINE_LENGTH 512
#define ITEM_COUNT 5
#define MAX_DYNAMIC_BINDINGS 64
#define FUNC_NAME_LEN 64


// 文件系统相关类型
typedef struct {
    char name[MAX_PATH];   	
    char path[MAX_PATH];        // 文件原路径
    char trash_path[MAX_PATH];  // 回收站路径
    time_t delete_time;         // 删除时间戳(使用time_t标准类型)
} TrashEntry;

typedef struct {
    char name[MAX_PATH];       // 显示名称
    char full_path[MAX_PATH];  // 完整路径
    bool is_dir;               // 使用bool更语义化
    bool is_exec;              // 使用bool而非int
    bool is_image;
    bool is_hidden;
    off_t size;
    int grep_line_no;          // 仅grep模式使用
} FileEntry;

// 文件浏览器结构体（用于模块化、支持多窗格/弹窗/搜索结果等）
typedef struct {
    FileEntry *files;     // 文件项数组（动态分配或引用全局）
    int total_files;      // 条目数
    int cursor_pos;       // 当前光标位置
    int x_offset;         // 横向偏移（用于多列或滚动）
    int col_width;        // 每列宽度
    int visible_cols;     // 当前可见列数
} FileBrowser;

typedef enum {
    CLIP_MODE_COPY = 0,
    CLIP_MODE_CUT = 1
} ClipboardMode;

typedef enum {
    SEARCH_NONE,
    SEARCH_FIND,
    SEARCH_GREP
} SearchType;

typedef enum {
    MAIN_INPUT_MODE,    // 主程序快捷键模式
    DIRECT_INPUT_MODE   // 直接输入模式
} InputMode;


typedef enum { 
	KEYBOARD_HIDDEN, 
	KEYBOARD_VISIBLE 
} KeyboardState;

extern KeyboardState current_kb_state;
extern int keyboard_height;  // 建议默认值：3（行）
extern ClipboardMode clip_mode;
extern FileEntry files[MAX_FILES];
extern FileBrowser browser[MAX_FILES];
extern int file_count;
extern char current_dir[MAX_PATH];
extern int cursor_pos;
extern int term_width, term_height;
extern char previous_dir[MAX_PATH];
extern char number_input[10];
extern int num_input_len;
extern int trash_count;
extern int clipboard_mode;
extern char clipboard_path[MAX_PATH];
extern char search_root[MAX_PATH];
extern char find_back_path[MAX_PATH];
extern int last_cursor_pos;
extern int awaiting_c_prefix; // 0: 初始状态，1: 等待c或m，2: 等待数字，3: 等待回车
extern char operation; 

extern char clipboard[MAX_FILES][MAX_PATH]; // 存储复制的文件路径
extern int clipboard_count; // 复制的文件数量
extern TrashEntry file_trash[TRASH_SIZE]; 
extern InputMode current_input_mode;
extern char full_path[MAX_PATH];
extern char trash_path[MAX_PATH];
extern char g_bash_home[MAX_PATH];
extern char g_trash_dir[MAX_PATH];
extern char clipboard_path[MAX_PATH];  // 存储路径
extern bool clipboard_is_cut;          // 标记是剪切还是复制
extern int clipboard_count;            // 文件数量
extern char last_visited_dir[MAX_PATH];
extern bool in_find_mode;
extern bool show_hidden_files;  // true=显示隐藏文件，false=隐藏
extern int start_index;
extern int files_per_page;
extern int term_height, term_width;
extern int pathbar_y;
extern int border_y ;
extern int filelist_start_y ;
extern int statusbar_y;
extern int filelist_max_rows;
extern int file_count;
extern char current_dir[MAX_PATH];  // 当前目录，需提前设置好

/*============bind_menu============-*/

typedef enum {
    HANDLER_TYPE_VOID,
    HANDLER_TYPE_PANEL,
    HANDLER_TYPE_CHAR,
    HANDLER_TYPE_BOOL
} HandlerType;

// 静态绑定结构
typedef struct {
    int key;
    const char *desc;
    HandlerType handler_type;
    union {
        void (*void_handler)(void);
        void (*panel_handler)(WINDOW *);
        void (*char_handler)(const char *);
        bool (*bool_handler)(const char *, const char *);
    } handler;
} StaticBinding;

// 动态绑定结构
typedef struct {
    int key;
    char func_name[FUNC_NAME_LEN];
} DynamicBinding;

// 注册的功能函数结构
typedef struct {
    const char *name;
    HandlerType type;
    union {
        void (*void_handler)(void);
        void (*panel_handler)(WINDOW *);
        void (*char_handler)(const char *);
        bool (*bool_handler)(const char *, const char *);
    };
} RegisteredFunction;


// 全局动态绑定数组和计数，定义在keybindings.c
extern DynamicBinding dynamic_bindings[MAX_DYNAMIC_BINDINGS];
extern size_t dynamic_bindings_count;
extern RegisteredFunction registered_functions[];

#endif // COMMON_H


