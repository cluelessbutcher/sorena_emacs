#ifndef LINE_H_
#define LINE_H_

#include "utf8.h"

struct buffer;

struct line {
	struct line *l_fp;
	struct line *l_bp;
	int l_size;
	int l_used;
	char l_text[1];
};

#define line_next(lp)       ((lp)->l_fp)
#define line_prev(lp)       ((lp)->l_bp)
#define lgetc(lp, n)    ((lp)->l_text[(n)]&0xFF)
#define lputc(lp, n, c) ((lp)->l_text[(n)]=(c))
#define line_length(lp)     ((lp)->l_used)

extern void line_free(struct line *lp);
extern void buffer_changed(int flag);
extern int cmd_insert_space(int f, int n);
extern int insert_string(char *instr);
extern int insert_char(int n, int c);
extern int overwrite_char(int c);
extern int overwrite_string(char *ostr);
extern int insert_newline(void);
extern int delete_bytes(long n, int kflag);
extern int delete_characters(long n, int kflag);
extern int char_at_dot(unicode_t *);
extern char *getctext(void);
extern int putctext(char *iline);
extern int delete_newline(void);
extern void kdelete(void);
extern int kinsert(int c);
extern int cmd_yank(int f, int n);
extern struct line *line_alloc(int);
extern int insert_byte(int n, int c);
extern void line_number_hint(struct line *lp, long n);
extern void line_number_flush(void);
extern long buffer_line_number(struct buffer *bp, struct line *lp);
extern long line_number(struct line *lp);
extern int line_put_byte(struct line *lp, int off, int c);
extern int insert_bytes(const char *s, int len);
extern int line_truncate(struct line *lp, int len);
extern int insert_text(const char *s, int len);
extern struct line *line_at(long L);
struct buffer;
extern long undo_record_insert(struct line *lp, int off, const char *s, int len);
extern long undo_record_insert_buf(struct line *lp, int off, const char *s, int len);
extern long undo_record_insert_fill(struct line *lp, int off, int c, int n);
extern long undo_record_delete(struct line *lp, int off, const char *s, int len);
extern long undo_record_replace(struct line *lp, int off, const char *olds, const char *news, int len);
extern int buffer_was_modified;

#endif
