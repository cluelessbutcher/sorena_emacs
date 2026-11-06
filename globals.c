#include "estruct.h"
#include "globals.h"

int fill_column = 80;
int keyboard_macro[NKBDM];
char *command_string = NULL;
char goto_label[NPAT] = "";
int if_level = 0;
int can_erase_to_eol = TRUE;
int can_reverse_video = FALSE;
char *modename[] = {
	"WRAP", "CMODE", "SPELL", "EXACT", "VIEW", "OVER",
	"MAGIC", "ASAVE", "UTF-8", "INDENT", "PAIR"
};

char *mode2name[] = {
	"Wrap", "Cmode", "Spell", "Exact", "View", "Over",
	"Magic", "Asave", "utf-8", "Indent", "Pair"
};

char modecode[] = "WCSEVOMAUIP";
int global_mode = MDINDENT | MDPAIR;
int global_flags = GFREAD;
int autosave_interval = 256;
int autosave_countdown = 256;
int screen_garbage = TRUE;
int message_present = FALSE;
int executing_command_line = FALSE;
int storing_macro = FALSE;
int display_commands = TRUE;
int display_input = TRUE;
struct buffer *store_buffer = NULL;
int shown_row = HUGE;
int shown_col = HUGE;
int left_column = 0;
int meta_char = CONTROL | '[';
int ctlx_char = CONTROL | 'X';
int ctlc_char = CONTROL | 'C';
int region_active = FALSE;
int region_shift = FALSE;
int show_line_numbers = TRUE;
int default_indent = 4;
unsigned long line_gen = 1;
int repeat_key = CONTROL | 'U';
int abort_char = CONTROL | 'G';

int quote_char = 0x11;
int tabmask = 0x07;
struct kill *kill_last = NULL;
struct kill *kill_head = NULL;
int kill_used = KBLOCK;
int *keyboard_macro_pos;
int *keyboard_macro_end = &keyboard_macro[0];
int keyboard_macro_mode = STOP;
int keyboard_macro_repeat = 0;
int restflag = FALSE;
int last_key = 0;
int random_seed = 0;
int macro_debug = FALSE;
char error_text[] = "ERROR";
char true_text[] = "TRUE";
char false_text[] = "FALSE";
int command_status = TRUE;
int saveflag = 0;
char *file_line = NULL;
int file_line_size = 0;
int subprocess_status = 0;
int accept_nulls = FALSE;
int justflag = FALSE;
int overlap = 0;
int scroll_lines = 1;

int cursor_row;
int cursor_col;
int thisflag;
int lastflag;
int goal_column;
struct window *curwp;
struct buffer *curbp;
struct buffer *buffer_head;
struct window *window_head;
struct window *saved_window;
struct buffer *list_buffer;

char search_pattern[NPAT];
char reversed_pattern[NPAT];
char replace_pattern[NPAT];

unsigned int match_bytes = 0;
unsigned int previous_match_bytes = 0;
char *matched_text = NULL;
struct line *match_line = NULL;
int match_offset = 0;

char *directive_names[] = {
	"if", "else", "endif",
	"goto", "return", "endm",
	"while", "endwhile", "break",
	"force"
};
