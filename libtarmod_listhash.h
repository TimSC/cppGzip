/* @configure_input@ */

/*
**  Copyright 1998-2002 University of Illinois Board of Trustees
**  Copyright 1998-2002 Mark D. Roth
**  All rights reserved.
**
**  libtarmod_listhash.h - header file for listhash module
**
**  Mark D. Roth <roth@uiuc.edu>
**  Campus Information Technologies and Educational Services
**  University of Illinois at Urbana-Champaign
*/

#ifndef libtarmod_LISTHASH_H
#define libtarmod_LISTHASH_H

#ifdef __cplusplus
extern "C" {
#endif

/***** list.c **********************************************************/

/*
** Comparison function (used to determine order of elements in a list)
** returns less than, equal to, or greater than 0
** if data1 is less than, equal to, or greater than data2
*/
typedef int (*libtarmod_cmpfunc_t)(void *, void *);

/*
** Free function (for freeing allocated memory in each element)
*/
typedef void (*libtarmod_freefunc_t)(void *);

/*
** Plugin function for libtarmod_list_iterate()
*/
typedef int (*libtarmod_iterate_func_t)(void *, void *);

/*
** Matching function (used to find elements in a list)
** first argument is the data to search for
** second argument is the list element it's being compared to
** returns 0 if no match is found, non-zero otherwise
*/
typedef int (*libtarmod_matchfunc_t)(void *, void *);


struct libtarmod_node
{
	void *data;
	struct libtarmod_node *next;
	struct libtarmod_node *prev;
};
typedef struct libtarmod_node *libtarmod_listptr_t;

struct libtarmod_list
{
	libtarmod_listptr_t first;
	libtarmod_listptr_t last;
	libtarmod_cmpfunc_t cmpfunc;
	int flags;
	unsigned int nents;
};
typedef struct libtarmod_list libtarmod_list_t;


/* values for flags */
#define LIST_USERFUNC	0	/* use cmpfunc() to order */
#define LIST_STACK	1	/* new elements go in front */
#define LIST_QUEUE	2	/* new elements go at the end */


/* reset a list pointer */
void libtarmod_listptr_reset(libtarmod_listptr_t *);

/* retrieve the data being pointed to */
void *libtarmod_listptr_data(libtarmod_listptr_t *);

/* creates a new, empty list */
libtarmod_list_t *libtarmod_list_new(int, libtarmod_cmpfunc_t);

/* call a function for every element in a list */
int libtarmod_list_iterate(libtarmod_list_t *,
				   libtarmod_iterate_func_t, void *);

/* empty the list */
void libtarmod_list_empty(libtarmod_list_t *,
				  libtarmod_freefunc_t);

/* remove and free() the entire list */
void libtarmod_list_free(libtarmod_list_t *,
				 libtarmod_freefunc_t);

/* add elements */
int libtarmod_list_add(libtarmod_list_t *, void *);

/* removes an element from the list - returns -1 on error */
void libtarmod_list_del(libtarmod_list_t *,
				libtarmod_listptr_t *);

/* returns 1 when valid data is returned, or 0 at end of list */
int libtarmod_list_next(libtarmod_list_t *,
				libtarmod_listptr_t *);

/* returns 1 when valid data is returned, or 0 at end of list */
int libtarmod_list_prev(libtarmod_list_t *,
				libtarmod_listptr_t *);

/* return 1 if the data matches a list entry, 0 otherwise */
int libtarmod_list_search(libtarmod_list_t *,
				  libtarmod_listptr_t *, void *,
				  libtarmod_matchfunc_t);

/* return number of elements from list */
unsigned int libtarmod_list_nents(libtarmod_list_t *);

/* adds elements from a string delimited by delim */
int libtarmod_list_add_str(libtarmod_list_t *, char *, char *);

/* string matching function */
int libtarmod_str_match(char *, char *);


/***** hash.c **********************************************************/

/*
** Hashing function (determines which bucket the given key hashes into)
** first argument is the key to hash
** second argument is the total number of buckets
** returns the bucket number
*/
typedef unsigned int (*libtarmod_hashfunc_t)(void *, unsigned int);


struct libtarmod_hashptr
{
	int bucket;
	libtarmod_listptr_t node;
};
typedef struct libtarmod_hashptr libtarmod_hashptr_t;

struct libtarmod_hash
{
	int numbuckets;
	libtarmod_list_t **table;
	libtarmod_hashfunc_t hashfunc;
	unsigned int nents;
};
typedef struct libtarmod_hash libtarmod_hash_t;


/* reset a hash pointer */
void libtarmod_hashptr_reset(libtarmod_hashptr_t *);

/* retrieve the data being pointed to */
void *libtarmod_hashptr_data(libtarmod_hashptr_t *);

/* default hash function, optimized for 7-bit strings */
unsigned int libtarmod_str_hashfunc(char *, unsigned int);

/* return number of elements from hash */
unsigned int libtarmod_hash_nents(libtarmod_hash_t *);

/* create a new hash */
libtarmod_hash_t *libtarmod_hash_new(int, libtarmod_hashfunc_t);

/* empty the hash */
void libtarmod_hash_empty(libtarmod_hash_t *,
				  libtarmod_freefunc_t);

/* delete all the libtarmod_nodes of the hash and clean up */
void libtarmod_hash_free(libtarmod_hash_t *,
				 libtarmod_freefunc_t);

/* returns 1 when valid data is returned, or 0 at end of list */
int libtarmod_hash_next(libtarmod_hash_t *,
				libtarmod_hashptr_t *);

/* return 1 if the data matches a list entry, 0 otherwise */
int libtarmod_hash_search(libtarmod_hash_t *,
				  libtarmod_hashptr_t *, void *,
				  libtarmod_matchfunc_t);

/* return 1 if the key matches a list entry, 0 otherwise */
int libtarmod_hash_getkey(libtarmod_hash_t *,
				  libtarmod_hashptr_t *, void *,
				  libtarmod_matchfunc_t);

/* inserting data */
int libtarmod_hash_add(libtarmod_hash_t *, void *);

/* delete an entry */
int libtarmod_hash_del(libtarmod_hash_t *,
			       libtarmod_hashptr_t *);

#ifdef __cplusplus
}
#endif

#endif /* ! libtarmod_LISTHASH_H */

