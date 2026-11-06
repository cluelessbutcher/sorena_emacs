#ifndef GLOBALS_H_
#define GLOBALS_H_

#include <signal.h>
#include <stdlib.h>
#include <string.h>

typedef int (*fn_t)(int, int);

extern int fill_column;
extern int keyboard_macro[];
extern char *command_string;
extern char goto_label[];
extern int if_level;
extern int can_erase_to_eol;
extern int can_reverse_video;
extern char *modename[];
extern char *mode2name[];
extern char modecode[];
extern struct key_tab keytab[];
extern struct name_bind names[];
extern struct name_bind legacy_names[];
extern const char *const help_text[];
extern int global_mode;
extern int global_flags;
extern int autosave_interval;
extern int autosave_countdown;
extern int screen_garbage;
extern int message_present;
extern int executing_command_line;
extern int storing_macro;
extern int display_commands;
extern int display_input;
extern struct buffer *store_buffer;

extern volatile sig_atomic_t chg_width, chg_height;
extern int shown_row;
extern int shown_col;
extern int left_column;
extern int meta_char;
extern int ctlx_char;
extern int ctlc_char;
extern int region_active;
extern int region_shift;
extern int show_line_numbers;
extern int default_indent;
extern unsigned long line_gen;
extern int repeat_key;
extern int abort_char;

extern int quote_char;
extern int tabmask;
extern struct kill *kill_last;
extern struct kill *kill_head;
extern int kill_used;
extern int *keyboard_macro_pos;
extern int *keyboard_macro_end;
extern int keyboard_macro_mode;
extern int keyboard_macro_repeat;
extern int restflag;
extern int last_key;
extern int random_seed;
extern int macro_debug;
extern char error_text[];
extern char true_text[];
extern char false_text[];
extern int command_status;
extern int saveflag;
extern char *file_line;
extern int file_line_size;
extern int file_line_len;
extern int subprocess_status;
extern int accept_nulls;
extern int justflag;
extern int overlap;
extern int scroll_lines;

extern int cursor_row;
extern int cursor_col;
extern int thisflag;
extern int lastflag;
extern int goal_column;
extern struct window *curwp;
extern struct buffer *curbp;
extern struct buffer *buffer_head;
extern struct window *window_head;
extern struct window *saved_window;
extern struct buffer *list_buffer;

extern char search_pattern[];
extern char reversed_pattern[];
extern char replace_pattern[];

extern unsigned int match_bytes;
extern unsigned int previous_match_bytes;
extern char *matched_text;
extern struct line *match_line;
extern int match_offset;

extern char *directive_names[];

extern struct terminal term;

#endif
