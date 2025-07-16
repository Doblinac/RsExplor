#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "common.h"


/*============基础功能函数============-*/
void init_navigation(void) ;
void scan_directory(const char *path);
int is_hidden_file(const char *filename);
int compare_files(const void *a, const void *b);
int is_image_file(const char *filename);
bool is_compressed_file(const char *filepath);
void view_image(const char *path);

/*============功能函数============-*/
void grep_prompt(void) ;
void find_prompt(void) ;
void run_terminal_search(const char *cmd) ;
void execute_selected_file(const char *filepath) ;
void edit_selected_file(const char *filepath) ;


/*============扩展功能函数============-*/
void open_with_termux_open(const char *filepath)  ;
const char* get_mime_type(const char* filename) ;
void run_in_terminal(const char *cmd, const char *filepath) ;
void convert_termux_path_to_android(char *android_path, const char *termux_path);
void check_termux_allow_external(void);

/*==========文件夹操作功能函数==========-*/
bool validate_path(const char *path);
void copy_current_file(bool is_cut) ;
void copy_current_file_wrapper(void);
void paste_file(void) ;
bool copy_file(const char *src, const char *dst);
bool remove_directory_recursive(const char *path);
void refresh_dir(void);

void clear_search_highlights(void);
const char *get_fallback_path(void) ;
void handle_number_selection(void);
void handle_number_input(int ch);
void handle_open_file(void) ;


/*==========回收站功能功能函数===========-*/
void clear_trash(void);
void cleanup(void);
bool confirm_action(const char *prompt);
void delete_current_file(void) ;
void handle_restore_file(void);
void handle_delete_file(void) ;
void restore_from_trash(void) ;
void handle_empty_trash(void);

/*============压缩功能函数============-*/

void view_compressed_file(const char *path);
void extract_file(const char *path, const char *option);
void extract_to_new_folder(const char *filepath);
void view_compressed_file(const char *path);
void handle_compress_option(const char *filepath, int choice) ;
// ui.c 或 ui.h 顶部加入：

void handle_main_action(int index, const char *filepath);
void handle_compressed_action(int index, const char *filepath);
/*============§☆☆☆§============-*/

void extract_archive(const char *filepath) ;
void list_archive_contents(const char *filepath) ;
void test_archive(const char *filepath);

void shell_in_current_dir(const char *path) ;
void shell_wrapper(void) ;

void delete_current_entry(void) ;
const char *get_cursor_path(FileBrowser *browser);

#endif // OPERATIONS_H
