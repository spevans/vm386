/* lists.h -- Doubly-linked list handling.
   John Harper. */

#ifndef _VMM_LISTS_H
#define _VMM_LISTS_H

#include <vmm/types.h>

/* This file defines an Amiga-like doubly linked list type. Note that the
   the way it works is a bit weird, notably that the `succ' field of the
   last node in the list isn't NULL -- it points to a NULL. This leads
   to list traversing like,

	list_node_t *nxt, *x = SOME_LIST->head;
	while((nxt = x->succ) != NULL)
	{
	    ...
	    x = nxt;
	}

   It may look weird but it works quite well. The general idea is that
   any structure which should be in a doubly-linked list has a list_node_t
   as its first element. */


typedef struct __list_node {
    struct __list_node *succ;
    struct __list_node *pred;
} list_node_t;

typedef struct __list {
    struct __list_node *head;
    struct __list_node *tail;
    struct __list_node *tailpred;
} list_t;


/* Initialise a list structure so that LIST is empty. */
static inline void
init_list(list_t *list)
{
    list->tailpred = (list_node_t *)&list->head;
    list->tail = NULL;
    list->head = (list_node_t *)&list->tail;
}

/* Append the node NODE to the end of the list LIST. */
static inline void
append_node(list_t *list, list_node_t *node)
{
    list_node_t *tmp, *tmp2;
    tmp = (list_node_t *)&list->tail;
    tmp2 = tmp->pred;
    tmp->pred = node;
    node->succ = tmp;
    node->pred = tmp2;
    tmp2->succ = node;
}

/* Put the node NODE onto the front of the list LIST. */
static inline void
prepend_node(list_t *list, list_node_t *node)
{
    list_node_t *tmp = list->head;
    list->head = node;
    node->succ = tmp;
    node->pred = (list_node_t *)list;
    tmp->pred = node;
}

/* Insert the node NODE into the list LIST after the node PREV. */
static inline void
insert_node(__attribute__((unused)) list_t *list, list_node_t *node, list_node_t *prev)
{
    list_node_t *tmp = prev->succ;
    if(tmp != NULL)
    {
	node->succ = tmp;
	node->pred = prev;
	tmp->pred = node;
	prev->succ = node;
    }
    else
    {
	node->succ = prev;
	tmp = prev->pred;
	node->pred = tmp;
	prev->pred = node;
	tmp->succ = node;
    }
}

/* Remove the node NODE from the list it is in. */
static inline void
remove_node(list_node_t *node)
{
    list_node_t *tmp = node->succ;
    node = node->pred;
    node->succ = tmp;
    tmp->pred = node;
}

/* TRUE if the list L is empty. */
static inline bool
list_empty_p(list_t *list)
{
    return list->tailpred == (list_node_t *)list;
}

#define LIST_EMPTY_P(l) list_empty_p(l)

#endif /* _VMM_LISTS_H */
