/* readline.c -- Emacs-like line editing.
   John Harper. */

#include <vmm/tty.h>
#include <vmm/kernel.h>
#include <vmm/string.h>
#include <vmm/lists.h>

#define kprintf kernel->printf

/* State info for the readline() function. */
struct rl_info {
    struct tty *tty;

    long cursor_pos;
    u_long cursor_x, cursor_y;
    long mark_pos;
    long prompt_end_x, prompt_end_y;

    size_t line_len, buf_len;
    char *line_buf;

    int current_prefix_arg, next_prefix_arg;
    bool valid_current_arg, valid_next_arg;

    bool had_esc, had_ctrl_x, in_cursor;

    /* The current history item being edited, or NULL for the bottom line. */
    struct rl_history *current_item;
};

#define PREFIX_ARG(rl) ((rl)->valid_current_arg ? (rl)->current_prefix_arg : 1)

/* A node in a tty's history list. */
struct rl_history {
    list_node_t node;
    size_t line_len;
    long cursor_pos;
    char line_buf[0];
};

#define SIZEOF_HISTORY(line_len) (sizeof(struct rl_history) + (line_len) + 1)

/* The maximum number of history lines stored by each tty. */
int rl_history_max = 50;

/* If TRUE lines that are the same as the previous line aren't entered into
   the history list. */
bool rl_no_duplicates = TRUE;


/* Utility functions. */

/* Work out the screen position of the logical character POS. */
static inline void
rl_find_pos(struct rl_info *rl, u_long pos, u_long *xp, u_long *yp)
{
    /* Assume that each character only takes one glyph for now.. */
    *xp = (pos + rl->prompt_end_x) % rl->tty->video.mi.cols;
    *yp = rl->prompt_end_y + (pos + rl->prompt_end_x) / rl->tty->video.mi.cols;
}    

/* Update the `cursor_x' and `cursor_y' fields in RL from its logical cursor
   position. If there's no vertical room the screen is scrolled. */
static void
rl_find_cursor(struct rl_info *rl)
{
    rl_find_pos(rl, rl->cursor_pos, &rl->cursor_x, &rl->cursor_y);
    while(rl->cursor_y >= rl->tty->video.mi.rows)
    {
	rl->prompt_end_y--;
	rl->cursor_y--;
	tty_module.scroll_up(rl->tty);
    }
}

/* Set the position of the tty's cursor to the position defined in RL. */
static inline void
rl_set_cursor(struct rl_info *rl)
{
    tty_module.set_cursor(rl->tty, rl->cursor_x, rl->cursor_y);
}

/* Redraw all of the line RL starting at logical position START. */
static void
rl_redisplay_from(struct rl_info *rl, u_long start)
{
    u_long x, y;
    if(start >= rl->line_len)
	return;
    rl_find_pos(rl, start, &x, &y);
    if(y >= rl->tty->video.mi.rows)
	return;
    rl->tty->x = x; rl->tty->y = y;
    tty_module.printn(rl->tty, rl->line_buf + start, rl->line_len - start);
    rl_set_cursor(rl);
}

/* Redraw all of the line RL. */
static void
rl_redisplay(struct rl_info *rl)
{
    rl->tty->x = rl->prompt_end_x;
    rl->tty->y = rl->prompt_end_y;
    tty_module.printn(rl->tty, rl->line_buf, rl->line_len);
    rl_set_cursor(rl);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#if 0
/* Redraw the section of the line RL starting at START for EXTENT
   characters. */
static void
rl_redisplay_extent(struct rl_info *rl, u_long start, long extent)
{
    long x, y;
    if((start >= rl->line_len) || (extent <= 0))
	return;
    if(start + extent > rl->line_len)
	extent = rl->line_len - start;
    rl_find_pos(rl, start, &x, &y);
    if(y >= rl->tty->video.mi.rows)
	return;
    rl->tty->x = x; rl->tty->y = y;
    tty_module.printn(rl->tty, rl->line_buf + start, extent);
    rl_set_cursor(rl);
}
#endif
#pragma GCC diagnostic pop



/* Fix the buffer of RL so it can hold at least MIN-LEN characters. */
static bool
rl_realloc_buf(struct rl_info *rl, size_t min_len)
{
    rl->buf_len = round_to(min_len + 8, 256) - 8;
    if(rl->line_buf == NULL)
	rl->line_buf = kernel->malloc(rl->buf_len);
    else
	rl->line_buf = kernel->realloc(rl->line_buf, rl->buf_len);
    return rl->line_buf != NULL;
}

/* Insert the string STR of length LEN into the line RL at its cursor
   position. */
static bool
rl_insert(struct rl_info *rl, char *str, size_t len)
{
    if((rl->line_len + len >= rl->buf_len)
       && !rl_realloc_buf(rl, rl->line_len + len + 2))
	return FALSE;
    memmove(rl->line_buf + rl->cursor_pos + len,
	    rl->line_buf + rl->cursor_pos,
	    rl->line_len - rl->cursor_pos);
    memcpy(rl->line_buf + rl->cursor_pos, str, len);
    rl->line_len += len;
    if(rl->mark_pos >= rl->cursor_pos)
	rl->mark_pos += len;
    rl->cursor_pos += len;
    rl_find_cursor(rl);
    rl_redisplay_from(rl, rl->cursor_pos - len);
    return TRUE;
}

/* Delete EXTENT characters from the line RL at position START-POS. */
static bool
rl_delete(struct rl_info *rl, long start_pos, long extent)
{
    u_long x, y;
    if(start_pos > (long)rl->line_len)
    {
	extent -= (rl->line_len - start_pos);
	if(extent <= 0)
	    return TRUE;
	start_pos = rl->line_len;
    }
    memmove(rl->line_buf + start_pos,
	    rl->line_buf + start_pos + extent,
	    rl->line_len - (start_pos + extent));
    if(rl->cursor_pos >= start_pos)
    {
	if(rl->cursor_pos < (start_pos + extent))
	    rl->cursor_pos = start_pos;
	else
	    rl->cursor_pos -= extent;
    }
    if(rl->mark_pos >= start_pos)
    {
	if(rl->mark_pos < (start_pos + extent))
	    rl->mark_pos = start_pos;
	else
	    rl->mark_pos -= extent;
    }
    rl->line_len -= extent;
    rl_find_pos(rl, rl->line_len, &x, &y);
    rl->tty->x = x; rl->tty->y = y;
    tty_module.clear_chars(rl->tty, extent);
    rl_find_cursor(rl);
    rl_redisplay_from(rl, rl->cursor_pos);
    return TRUE;
}


/* History manipulation. */

#if 0
static void
rl_print_history(struct rl_info *rl)
{
    struct rl_history *rlh = (struct rl_history *)rl->tty->rl_history.head;
    kprintf("history: <");
    while(rlh->node.succ)
    {
	kprintf(" `%s'", rlh->line_buf);
	rlh = (struct rl_history *)rlh->node.succ;
    }
    kprintf(" >\n");
}
#endif

/* Allocate a new history entry for the string LINE of length LENGTH
   with the cursor at offset CURSOR-POS. */
static inline struct rl_history *
rl_alloc_history(const char *line, size_t length, long cursor_pos)
{
    struct rl_history *rlh = kernel->malloc(SIZEOF_HISTORY(length));
    DB(("rl_alloc_history: line=%p length=%Z cursor_pos=%d\n",
	line, length, cursor_pos));
    if(rlh != NULL)
    {
	rlh->line_len = length;
	rlh->cursor_pos = cursor_pos;
	memcpy(rlh->line_buf, line, length);
	rlh->line_buf[length] = 0;
    }
    return rlh;
}

/* Unlinks RLH from the list it's in then free()'s it. */
static inline void
rl_free_history(struct rl_history *rlh)
{
    DB(("rl_free_history: rlh=%p\n", rlh));
    remove_node(&rlh->node);
    kernel->free(rlh);
}

/* Free all history entries in the list HISTORY. */
void
rl_free_history_list(list_t *history)
{
    DB(("rl_free_history_list: history=%p\n", history));
    while(!list_empty_p(history))
	rl_free_history((struct rl_history *)history->head);
}

/* Add a new history for the current line in RL to the start of the tty's
   history list. This also trims the history list to have no more than
   `rl_history_max' entries. If ITEM is non-NULL it points to the node
   in the list to be replaced by the new entry. */
static bool
rl_add_history(struct rl_info *rl, struct rl_history *item)
{
    struct rl_history *rlh;
    DB(("rl_add_history: rl=%p item=%p\n", rl, item));
    while(rl->tty->rl_history_size >= rl_history_max)
    {
	rlh = (struct rl_history *)rl->tty->rl_history.tailpred;
	rl_free_history(rlh);
	rl->tty->rl_history_size--;
	if(rl->current_item == rlh)
	    rl->current_item = NULL;
	if(item == rlh)
	    item = NULL;
    }
    if((item != NULL) && (rl->line_len == item->line_len))
    {
	if(memcmp(rl->line_buf, item->line_buf, rl->line_len))
	{
	    memcpy(item->line_buf, rl->line_buf, rl->line_len);
	    item->line_buf[rl->line_len] = 0;
	}
	item->cursor_pos = rl->cursor_pos;
	return TRUE;
    }
    rlh = rl_alloc_history(rl->line_buf, rl->line_len, rl->cursor_pos);
    if(rlh != NULL)
    {
	if(item == NULL)
	{
	    prepend_node(&rl->tty->rl_history, &rlh->node);
	    rl->tty->rl_history_size++;
	}
	else
	{
	    insert_node(&rl->tty->rl_history, &rlh->node, &item->node);
	    rl_free_history(item);
	}
	return TRUE;
    }
    else
	return FALSE;
}

/* Load the history item RLH into the current line buffer of RL. */
static bool
rl_load_history(struct rl_info *rl, struct rl_history *rlh)
{
    DB(("rl_load_history: rl=%p rlh=%p\n", rl, rlh));
    if((rlh->line_len >= rl->buf_len)
       && !rl_realloc_buf(rl, rlh->line_len + 1))
	return FALSE;
    rl->tty->x = rl->prompt_end_x;
    rl->tty->y = rl->prompt_end_y;
    tty_module.clear_chars(rl->tty, rl->line_len);
    memcpy(rl->line_buf, rlh->line_buf, rlh->line_len);
    rl->line_len = rlh->line_len;
    rl->cursor_pos = rlh->cursor_pos;
    rl->mark_pos = 0;
    if(rl->tty->rl_history.head == &rlh->node)
    {
	rl_free_history(rlh);
	rl->tty->rl_history_size--;
	rl->current_item = NULL;
    }
    else
	rl->current_item = rlh;
    rl_find_cursor(rl);
    rl_redisplay(rl);
    return TRUE;
}

/* Switch to edit history item RLH in RL. */
static bool
rl_use_history(struct rl_info *rl, struct rl_history *rlh)
{
    DB(("rl_use_history: rl=%p rlh=%p\n", rl, rlh));
    /* First copy the line being edited into a history item.. */
    if(!rl_add_history(rl, rl->current_item))
	return FALSE;
    /* Now install the new line.. */
    if(!rl_load_history(rl, rlh))
	return FALSE;
    return TRUE;
}

/* After entering a line, use this function to add it to the history
   list, if necessary. */
static bool
rl_enter_history(struct rl_info *rl)
{
    struct rl_history *item;
    DB(("rl_enter_history: rl=%p\n", rl));
    if(rl->line_len == 0)
	return TRUE;
    if(rl->current_item == NULL)
	item = NULL;
    else
	item = (struct rl_history *)rl->tty->rl_history.head;
    if(rl_no_duplicates && !list_empty_p(&rl->tty->rl_history))
    {
	struct rl_history *last = (struct rl_history *)rl->tty->rl_history.head;
	if(rl->current_item != NULL)
	     last = last->node.succ->succ ? (struct rl_history *)last->node.succ : NULL;
	if((last->line_len != rl->line_len)
	   || memcmp(last->line_buf, rl->line_buf, last->line_len))
	    return rl_add_history(rl, item);
	else
	{
	    if(item != NULL)
		rl_free_history(item);
	    return TRUE;
	}
    }
    else
	return rl_add_history(rl, item);
}


/* Commands. */

static inline bool
rl_forward_char(struct rl_info *rl, int count)
{
    rl->cursor_pos += count;
    if(rl->cursor_pos < 0)
    {
	rl->cursor_pos = 0;
	tty_module.beep(rl->tty);
    }
    else if(rl->cursor_pos > (long)rl->line_len)
    {
	rl->cursor_pos = rl->line_len;
	tty_module.beep(rl->tty);
    }
    return TRUE;
}

static inline bool
rl_beginning_of_line(struct rl_info *rl)
{
    rl->cursor_pos = 0;
    return TRUE;
}

static inline bool
rl_end_of_line(struct rl_info *rl)
{
    rl->cursor_pos = rl->line_len;
    return TRUE;
}

static bool
rl_backward_delete_char(struct rl_info *rl)
{
    long length = PREFIX_ARG(rl);
    long start = rl->cursor_pos - length;
    if(start < 0)
    {
	length += start;
	start = 0;
    }
    rl_delete(rl, start, length);
    return TRUE;
}

static inline bool
rl_set_mark(struct rl_info *rl)
{
    rl->mark_pos = rl->cursor_pos;
    return TRUE;
}

static inline bool
rl_self_insert(struct rl_info *rl, u_char c)
{
    char buf[1];
    buf[0] = c;
    rl_insert(rl, buf, 1);
    return TRUE;
}

static inline bool
rl_exchange_point_and_mark(struct rl_info *rl)
{
    long tmp = rl->cursor_pos;
    rl->cursor_pos = rl->mark_pos;
    rl->mark_pos = tmp;
    return TRUE;
}

static bool
rl_beginning_of_history(struct rl_info *rl)
{
    if(!list_empty_p(&rl->tty->rl_history))
	rl_use_history(rl, (struct rl_history *)rl->tty->rl_history.tailpred);
    else
	tty_module.beep(rl->tty);
    return FALSE;
}

static bool
rl_end_of_history(struct rl_info *rl)
{
    if(rl->current_item == NULL)
	return FALSE;
    if(!list_empty_p(&rl->tty->rl_history))
	rl_use_history(rl, (struct rl_history *)rl->tty->rl_history.head);
    else
	tty_module.beep(rl->tty);
    return FALSE;
}

static bool
rl_next_history(struct rl_info *rl, int count)
{
    struct rl_history *rlh;
    DB(("rl_next_history: rl=%p count=%d\n", rl, count));
    if(rl->current_item != NULL)
	rlh = rl->current_item;
    else
    {
	if(list_empty_p(&rl->tty->rl_history))
	    goto error;
	rlh = (struct rl_history *)rl->tty->rl_history.head;
	count++;
    }
    while(count < 0)
    {
	rlh = (struct rl_history *)rlh->node.succ;
	if(rlh->node.succ == NULL)
	    goto error;
	count++;
    }
    while(count > 0)
    {
	rlh = (struct rl_history *)rlh->node.pred;
	if(rlh->node.pred == NULL)
	{
	error:
	    tty_module.beep(rl->tty);
	    return FALSE;
	}
	count--;
    }
    rl_use_history(rl, rlh);
    return FALSE;
}


/* Main key loop. */

/* Reads a line of input from TTY, returns a malloc()'d buffer containing
   the line or NULL. *LENGTHP is set to the length of the entered string. */
char *
readline(struct tty *tty, size_t *lengthp)
{
    struct rl_info rl;
    bool no_output = FALSE;

    rl.tty = tty;
    rl.cursor_pos = 0;
    rl.prompt_end_x = tty->x;
    rl.prompt_end_y = tty->y;
    rl.line_len = 0;
    rl.line_buf = NULL;
    if(!rl_realloc_buf(&rl, 0))
	return NULL;
    rl.current_item = NULL;
    rl_find_cursor(&rl);
    rl_set_cursor(&rl);

    rl.valid_next_arg = FALSE;
    rl.had_esc = rl.had_ctrl_x = rl.in_cursor = FALSE;
    while(1)
    {
	int c = tty_module.get_char(rl.tty);
	bool cursor_moved = FALSE;
	if(c == -1)
	    break;

	rl.current_prefix_arg = rl.next_prefix_arg;
	rl.valid_current_arg = rl.valid_next_arg;
	rl.valid_next_arg = FALSE;

	if(!rl.had_esc && !rl.had_ctrl_x)
	{
	    switch(c)
	    {
	    case '\x1B': //'':
		rl.had_esc = TRUE;
		break;

	    case '\x18': //'':
		rl.had_ctrl_x = TRUE;
		break;

	    case '\x06': //'':
		cursor_moved = rl_forward_char(&rl, PREFIX_ARG(&rl));
		break;

	    case '\x02': //'':
		cursor_moved = rl_forward_char(&rl, -PREFIX_ARG(&rl));
		break;

	    case '\x01': //'':
		cursor_moved = rl_beginning_of_line(&rl);
		break;

	    case '\x05': //'':
		cursor_moved = rl_end_of_line(&rl);
		break;

	    case '\x04': //'':
		if(rl.cursor_pos == (long)rl.line_len)
		{
		    if(rl.cursor_pos == 0)
			no_output = TRUE;
		    goto finished;
		}
		else
		    rl_delete(&rl, rl.cursor_pos, PREFIX_ARG(&rl));
		break;

	    case '\x7F': //'':
		cursor_moved = rl_backward_delete_char(&rl);
		break;

	    case '\r': //'^M':
	    case '\n':
		rl_enter_history(&rl);
		rl.cursor_pos = rl.line_len;
		rl_find_cursor(&rl);
		rl_set_cursor(&rl);
		tty_module.printn(rl.tty, "\n", 1);
		rl.line_buf[rl.line_len++] = '\n';
		goto finished;

	    case '\x0C': //'':
		rl_redisplay(&rl);
		break;

	    case '\0':					/* C-SPC */
		cursor_moved = rl_set_mark(&rl);
		break;

	    case '\x0E': //'':
		cursor_moved = rl_next_history(&rl, PREFIX_ARG(&rl));
		break;

	    case '\x10': //'':
		cursor_moved = rl_next_history(&rl, -PREFIX_ARG(&rl));
		break;

	    default:
		cursor_moved = rl_self_insert(&rl, c);
	    }
	}
	else if(!rl.had_esc && rl.had_ctrl_x)
	{
	    rl.had_ctrl_x = FALSE;
	    switch(c)
	    {
	    case '\x1B': //^[':
		rl.had_esc = TRUE;
		break;

	    case '\x18': //'^X':
		cursor_moved = rl_exchange_point_and_mark(&rl);
		break;

	    default:
		tty_module.beep(rl.tty);
	    }
	}
	else if(rl.had_esc && !rl.had_ctrl_x)
	{
	    rl.had_esc = FALSE;
	    if(!rl.in_cursor)
	    {
		switch(c)
		{
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		    if(rl.valid_current_arg)
		    {
			if(rl.current_prefix_arg >= 0)
			    rl.next_prefix_arg = (rl.current_prefix_arg * 10) + (c - '0');
			else
			    rl.next_prefix_arg = (rl.current_prefix_arg * 10) - (c - '0');
		    }
		    else
			rl.next_prefix_arg = c - '0';
		    rl.valid_next_arg = TRUE;
		    break;

		case '-':
		    if(rl.valid_current_arg)
			rl.next_prefix_arg = -rl.current_prefix_arg;
		    else
			rl.next_prefix_arg = -1;
		    rl.valid_next_arg = TRUE;
		    break;

		case '[':
		    rl.in_cursor = rl.had_esc = TRUE;
		    rl.next_prefix_arg = rl.current_prefix_arg;
		    break;

		case '<':
		    cursor_moved = rl_beginning_of_history(&rl);
		    break;

		case '>':
		    cursor_moved = rl_end_of_history(&rl);
		    break;

		default:
		    tty_module.beep(rl.tty);
		}
	    }
	    else
	    {
		rl.in_cursor = FALSE;
		switch(c)
		{
		case 'A':		/* Up */
		    cursor_moved = rl_next_history(&rl, -PREFIX_ARG(&rl));
		    break;

		case 'B':		/* Down */
		    cursor_moved = rl_next_history(&rl, PREFIX_ARG(&rl));
		    break;

		case 'C':		/* Right */
		    cursor_moved = rl_forward_char(&rl, PREFIX_ARG(&rl));
		    break;

		case 'D':		/* Left */
		    cursor_moved = rl_forward_char(&rl, -PREFIX_ARG(&rl));
		    break;

		default:
		    tty_module.beep(rl.tty);
		}
	    }
	}
	else
	{
	    rl.had_esc = rl.had_ctrl_x = FALSE;
	    tty_module.beep(rl.tty);
	}
	if(cursor_moved)
	{
	    rl_find_cursor(&rl);
	    rl_set_cursor(&rl);
	}
    }

finished:
    if(no_output)
    {
	kernel->free(rl.line_buf);
	*lengthp = 0;
	return NULL;
    }
    else
    {
	char *ptr = kernel->realloc(rl.line_buf, rl.line_len + 1);
	if(ptr != NULL)
	    ptr[rl.line_len] = 0;
	*lengthp = rl.line_len;
	return ptr;
    }
}
