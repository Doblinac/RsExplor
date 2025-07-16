#ifndef NAVIGATION_H
#define NAVIGATION_H

#include "common.h"
#include "operations.h"
#include "ui.h"


void sync_start_index_to_cursor(void);
int get_effective_file_count(void);
/* 光标移动 */
void handle_cursor_up(void);
void handle_cursor_down(void);
void handle_column_left(void);
void handle_column_right(void);

/* 路径处理 */
void get_full_path(char *buf, size_t size);

void open_trash(void) ;
void open_bashscript_home(void) ;
void navigate_to_last(void) ;
void navigate_to_home(void) ;

#endif
