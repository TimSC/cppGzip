/* @configure_input@ */

/*
**  Copyright 1998-2002 University of Illinois Board of Trustees
**  Copyright 1998-2002 Mark D. Roth
**  All rights reserved.
**
**  libtarmod_list.c - linked list routines
**
**  Mark D. Roth <roth@uiuc.edu>
**  Campus Information Technologies and Educational Services
**  University of Illinois at Urbana-Champaign
*/

#include "libtarmod_listhash.h"

#include <stdio.h>
#include <errno.h>
#include <sys/param.h>

# include <string.h>
# include <stdlib.h>


/*
** libtarmod_listptr_reset() - reset a list pointer
*/
void
libtarmod_listptr_reset(libtarmod_listptr_t *lp)
{
	*lp = NULL;
}


/*
** libtarmod_listptr_data() - retrieve the data pointed to by lp
*/
void *
libtarmod_listptr_data(libtarmod_listptr_t *lp)
{
	return (*lp)->data;
}


/*
** libtarmod_list_new() - create a new, empty list
*/
libtarmod_list_t *
libtarmod_list_new(int flags, libtarmod_cmpfunc_t cmpfunc)
{
	libtarmod_list_t *newlist;

#ifdef DS_DEBUG
	printf("in libtarmod_list_new(%d, 0x%lx)\n", flags, cmpfunc);
#endif

	if (flags != LIST_USERFUNC
	    && flags != LIST_STACK
	    && flags != LIST_QUEUE)
	{
		errno = EINVAL;
		return NULL;
	}

	newlist = (libtarmod_list_t *)calloc(1, sizeof(libtarmod_list_t));
	if (cmpfunc != NULL)
		newlist->cmpfunc = cmpfunc;
	else
		newlist->cmpfunc = (libtarmod_cmpfunc_t)strcmp;
	newlist->flags = flags;

	return newlist;
}


/*
** libtarmod_list_iterate() - call a function for every element
**				      in a list
*/
int
libtarmod_list_iterate(libtarmod_list_t *l,
			       libtarmod_iterate_func_t plugin,
			       void *state)
{
	libtarmod_listptr_t n;

	if (l == NULL)
		return -1;

	for (n = l->first; n != NULL; n = n->next)
	{
		if ((*plugin)(n->data, state) == -1)
			return -1;
	}

	return 0;
}


/*
** libtarmod_list_empty() - empty the list
*/
void
libtarmod_list_empty(libtarmod_list_t *l, libtarmod_freefunc_t freefunc)
{
	libtarmod_listptr_t n;

	for (n = l->first; n != NULL; n = l->first)
	{
		l->first = n->next;
		if (freefunc != NULL)
			(*freefunc)(n->data);
		free(n);
	}

	l->nents = 0;
}


/*
** libtarmod_list_free() - remove and free() the whole list
*/
void
libtarmod_list_free(libtarmod_list_t *l, libtarmod_freefunc_t freefunc)
{
	libtarmod_list_empty(l, freefunc);
	free(l);
}


/*
** libtarmod_list_nents() - return number of elements in the list
*/
unsigned int
libtarmod_list_nents(libtarmod_list_t *l)
{
	return l->nents;
}


/*
** libtarmod_list_add() - adds an element to the list
** returns:
**	0			success
**	-1 (and sets errno)	failure
*/
int
libtarmod_list_add(libtarmod_list_t *l, void *data)
{
	libtarmod_listptr_t n, m;

#ifdef DS_DEBUG
	printf("==> libtarmod_list_add(\"%s\")\n", (char *)data);
#endif

	n = (libtarmod_listptr_t)malloc(sizeof(struct libtarmod_node));
	if (n == NULL)
		return -1;
	n->data = data;
	l->nents++;

#ifdef DS_DEBUG
	printf("    libtarmod_list_add(): allocated data\n");
#endif

	/* if the list is empty */
	if (l->first == NULL)
	{
		l->last = l->first = n;
		n->next = n->prev = NULL;
#ifdef DS_DEBUG
		printf("<== libtarmod_list_add(): list was empty; "
		       "added first element and returning 0\n");
#endif
		return 0;
	}

#ifdef DS_DEBUG
	printf("    libtarmod_list_add(): list not empty\n");
#endif

	if (l->flags == LIST_STACK)
	{
		n->prev = NULL;
		n->next = l->first;
		if (l->first != NULL)
			l->first->prev = n;
		l->first = n;
#ifdef DS_DEBUG
		printf("<== libtarmod_list_add(): LIST_STACK set; "
		       "added in front\n");
#endif
		return 0;
	}

	if (l->flags == LIST_QUEUE)
	{
		n->prev = l->last;
		n->next = NULL;
		if (l->last != NULL)
			l->last->next = n;
		l->last = n;
#ifdef DS_DEBUG
		printf("<== libtarmod_list_add(): LIST_QUEUE set; "
		       "added at end\n");
#endif
		return 0;
	}

	for (m = l->first; m != NULL; m = m->next)
		if ((*(l->cmpfunc))(data, m->data) < 0)
		{
			/*
			** if we find one that's bigger,
			** insert data before it
			*/
#ifdef DS_DEBUG
			printf("    libtarmod_list_add(): gotcha..."
			       "inserting data\n");
#endif
			if (m == l->first)
			{
				l->first = n;
				n->prev = NULL;
				m->prev = n;
				n->next = m;
#ifdef DS_DEBUG
				printf("<== libtarmod_list_add(): "
				       "added first, returning 0\n");
#endif
				return 0;
			}
			m->prev->next = n;
			n->prev = m->prev;
			m->prev = n;
			n->next = m;
#ifdef DS_DEBUG
			printf("<== libtarmod_list_add(): added middle,"
			       " returning 0\n");
#endif
			return 0;
		}

#ifdef DS_DEBUG
	printf("    libtarmod_list_add(): new data larger than current "
	       "list elements\n");
#endif

	/* if we get here, data is bigger than everything in the list */
	l->last->next = n;
	n->prev = l->last;
	l->last = n;
	n->next = NULL;
#ifdef DS_DEBUG
	printf("<== libtarmod_list_add(): added end, returning 0\n");
#endif
	return 0;
}


/*
** libtarmod_list_del() - remove the element pointed to by n
**				  from the list l
*/
void
libtarmod_list_del(libtarmod_list_t *l, libtarmod_listptr_t *n)
{
	libtarmod_listptr_t m;

#ifdef DS_DEBUG
	printf("==> libtarmod_list_del()\n");
#endif

	l->nents--;

	m = (*n)->next;

	if ((*n)->prev)
		(*n)->prev->next = (*n)->next;
	else
		l->first = (*n)->next;
	if ((*n)->next)
		(*n)->next->prev = (*n)->prev;
	else
		l->last = (*n)->prev;

	free(*n);
	*n = m;
}


/*
** libtarmod_list_next() - get the next element in the list
** returns:
**	1			success
**	0			end of list
*/
int
libtarmod_list_next(libtarmod_list_t *l,
			    libtarmod_listptr_t *n)
{
	if (*n == NULL)
		*n = l->first;
	else
		*n = (*n)->next;

	return (*n != NULL ? 1 : 0);
}


/*
** libtarmod_list_prev() - get the previous element in the list
** returns:
**	1			success
**	0			end of list
*/
int
libtarmod_list_prev(libtarmod_list_t *l,
			    libtarmod_listptr_t *n)
{
	if (*n == NULL)
		*n = l->last;
	else
		*n = (*n)->prev;

	return (*n != NULL ? 1 : 0);
}


/*
** libtarmod_str_match() - string matching function
** returns:
**	1			match
**	0			no match
*/
int
libtarmod_str_match(char *check, char *data)
{
	return !strcmp(check, data);
}


/*
** libtarmod_list_add_str() - splits string str into delim-delimited
**				      elements and adds them to list l
** returns:
**	0			success
**	-1 (and sets errno)	failure
*/
int
libtarmod_list_add_str(libtarmod_list_t *l,
			       char *str, char *delim)
{
	char tmp[10240];
	char *tokp, *nextp = tmp;

	strlcpy(tmp, str, sizeof(tmp));
	while ((tokp = strsep(&nextp, delim)) != NULL)
	{
		if (*tokp == '\0')
			continue;
		if (libtarmod_list_add(l, strdup(tokp)))
			return -1;
	}

	return 0;
}


/*
** libtarmod_list_search() - find an entry in a list
** returns:
**	1			match found
**	0			no match
*/
int
libtarmod_list_search(libtarmod_list_t *l,
			      libtarmod_listptr_t *n, void *data,
			      libtarmod_matchfunc_t matchfunc)
{
#ifdef DS_DEBUG
	printf("==> libtarmod_list_search(l=0x%lx, n=0x%lx, \"%s\")\n",
	       l, n, (char *)data);
#endif

	if (matchfunc == NULL)
		matchfunc = (libtarmod_matchfunc_t)libtarmod_str_match;

	if (*n == NULL)
		*n = l->first;
	else
		*n = (*n)->next;

	for (; *n != NULL; *n = (*n)->next)
	{
#ifdef DS_DEBUG
		printf("checking against \"%s\"\n", (char *)(*n)->data);
#endif
		if ((*(matchfunc))(data, (*n)->data) != 0)
			return 1;
	}

#ifdef DS_DEBUG
	printf("no matches found\n");
#endif
	return 0;
}


/*
** libtarmod_list_dup() - copy an existing list
*/
libtarmod_list_t *
libtarmod_list_dup(libtarmod_list_t *l)
{
	libtarmod_list_t *newlist;
	libtarmod_listptr_t n;

	newlist = libtarmod_list_new(l->flags, l->cmpfunc);
	for (n = l->first; n != NULL; n = n->next)
		libtarmod_list_add(newlist, n->data);

#ifdef DS_DEBUG
	printf("returning from libtarmod_list_dup()\n");
#endif
	return newlist;
}


/*
** libtarmod_list_merge() - merge two lists into a new list
*/
libtarmod_list_t *
libtarmod_list_merge(libtarmod_cmpfunc_t cmpfunc, int flags,
			     libtarmod_list_t *list1,
			     libtarmod_list_t *list2)
{
	libtarmod_list_t *newlist;
	libtarmod_listptr_t n;

	newlist = libtarmod_list_new(flags, cmpfunc);

	n = NULL;
	while (libtarmod_list_next(list1, &n) != 0)
		libtarmod_list_add(newlist, n->data);
	n = NULL;
	while (libtarmod_list_next(list2, &n) != 0)
		libtarmod_list_add(newlist, n->data);

	return newlist;
}


