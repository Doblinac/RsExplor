#include "common.h"
#include "ui.h"
#include "operations.h"
#include "keybindings.h"


/*============全局变量============-*/

FileBrowser browser[MAX_FILES];
FileEntry files[MAX_FILES];
int file_count = 0;
char current_dir[MAX_PATH];
int cursor_pos = 0;
char previous_dir[MAX_PATH];
char number_input[10] = "";
int num_input_len = 0;
int awaiting_c_prefix = 0;
int clipboard_mode = 0;
char clipboard_path[MAX_PATH] = "";
bool show_hidden_files = false;
char search_root[MAX_PATH] = "";
int last_cursor_pos = 0;
char operation = 0; 
int batch_start = -1; 
int batch_end = -1;
char clipboard[MAX_FILES][MAX_PATH]; // 存储复制的文件路径
int clipboard_count = 0; // 复制的文件数量
int trash_count = 0;
TrashEntry file_trash[TRASH_SIZE];
int mkdir_p(const char *path, mode_t mode);
char full_path[MAX_PATH];
char trash_path[MAX_PATH];
ClipboardMode clip_mode = CLIP_MODE_COPY; 

/*======读取用户配置=========*/
char g_bash_home[MAX_PATH] = "";
char g_trash_dir[MAX_PATH] = "";

int current_index = 0;  // 初始化为0
bool in_grep_mode = 0;
bool in_find_mode = 0;
char grep_back_path[MAX_PATH];
char find_back_path[MAX_PATH] ;
char grep_keyword[256];
InputMode current_input_mode = MAIN_INPUT_MODE;
bool clipboard_is_cut = false;
char last_visited_dir[MAX_PATH] = {0};
int x_offset = 0;  // 水平滚动偏移

/*============NCURSES============-*/
int start_index = 0;   
int term_width = 0;
int term_height;           // 当前页的起始位置
int file_count ;
int files_per_page = 0;
int term_height, term_width;
int pathbar_y = 0;
int border_y = 1;
int filelist_start_y = 2;
int statusbar_y;
int filelist_max_rows;
int files_per_page;


int main(void) {
	load_config();
    initscr();

	update_layout();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    init_colors();
    signal(SIGINT, handle_signal);
    init_keybindings(); // 初始化按键系统
	
    getcwd(current_dir, sizeof(current_dir));
    if (strncmp(current_dir, ROOT_DIR, strlen(ROOT_DIR)) != 0) {
        strcpy(current_dir, ROOT_DIR);
    }	
    filelist_max_rows = term_height - 5;
    scan_directory(current_dir);

    while (1) {
        // 绘制界面
        clear();
        draw_path_bar();
        draw_border();
        draw_file_list();
        draw_status_bar();
        refresh();

        int ch = getch();
		
        if (isdigit(ch)) {
            handle_number_input(ch);
            continue;
        }

        if (ch == ' ' && num_input_len > 0) {
            handle_number_selection();
            continue;
        }

		if (handle_keyevent(ch)) {
   		 continue;
		}

        if (ch == 'b') {
            binding_menu();
            continue;
        }
		
        /*------ 遗留功能处理 ------*/ 
        switch (ch) {  
            case 'q':
                endwin();
                return 0;
            default:
                show_message_timed("未知按键: %c (按?查看帮助 )",  1000);
        }
    }
}
