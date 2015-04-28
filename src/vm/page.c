/******************************************************************************
 |   Assignment:  PintOS Part 1 - Virtual Memory
 |
 |      Authors:  Chukwudi Iwueze, Katie Park, Justin Winata, Jesse Wright
 |     Language:  ANSI C99 (Ja?)
 |
 |        Class:  CS439
 |   Instructor:  Rellermeyer, Jan S.
 |
 +-----------------------------------------------------------------------------
 |
 |  Description:  Virtual memory, etc.
 |
 |    Algorithm:  Hash table for SPT...
 |
 |   Required Features Not Included:  Eviction, swapping, etc.
 |
 |   Known Bugs:  None.
 |
******************************************************************************/

/////////////////
//             //
//  Resources  //
//             //
/////////////////

/*
	Supplemental Page Table: "Gives you information about the status of 
	 virtual memory (which addresses are logicially mapped, etc.)."
	 Courtesy https://groups.google.com/forum/#!topic/12au-cs140/5LDnfoOMdfY
*/

/*
	-Functionalities:
		>Your “s-page table” must be able to decide where to load executable 
			and which corresponding page of executable to load
		>Your “s-page table ” must be able to decide how to get swap disk and 
			which part (in sector) of swap disk stores the corresponding page
	-Implementation
		>Use hash table (recommend)
	-Usage
		>Rewrite load_segment() (in process.c) to populate s-page table 
			without loading pages into memory
		>Page fault handler then loads pages after consulting s-page table
	Courtesy http://courses.cs.vt.edu/cs3204/spring2007/pintos/Project3Session
		Spring2007.ppt (slide 12).
*/

/////////////
//         //
//  TO-DO  //
//         //
/////////////

/*
	TODO:
		Modify the following:
		---------------------------
		load_segment() in process.c
		page_fault() in exception.c
*/

////////////////
//            //
//  Includes  //
//            //
////////////////

#include <stdio.h>
#include <hash.h>
#include "vm/page.h"
#include "filesys/file.h"
#include "kernel/malloc.h"
#include "kernel/synch.h"
#include "vm/page.h"
#include "kernel/pte.h"

/////////////////////////
//                     //
//  Globale variables  //
//                     //
/////////////////////////

/* Talk about the hash table used for the SPT a little bit. Make sure to 
	mention the key is the address of a page. */
static struct hash spt;			/* Supplemental Page Table */
static struct lock lock;		/* Lock for synchronization of SPT */

///////////////
//           //
//  Structs  //
//           //
///////////////

/* struct page moved to page.h */

//////////////////
//              //
//  Prototypes  //
//              //
//////////////////

unsigned page_hash (const struct hash_elem *, void *);
bool page_less (const struct hash_elem *, const struct hash_elem *, void *);

/////////////////
//             //
//  Functions  //
//             //
/////////////////

/*
 * Function:  add_page 
 * --------------------
 *	Adds a page entry to the supplemental page table to keep track of the page's
 *		information such as when a page is allocated. Acquires the SPT's lock 
 *		before creating the hash element for the and page structures to wrap 
 *		the given address.
 *
 *  addr: the virtual address for a give page
 *
 *  returns: the created page struct that contains the corresponding hash_elem 
 *		struct for the SPT hash table
 */
struct page *
add_page (void *addr)
{
	printf("Calling add_page for addr %p\n", addr);
	lock_acquire (&lock);
	printf("Lock acquired in add_page for addr %p\n", addr);
    struct hash_elem *elem = (struct hash_elem *) malloc (sizeof (struct hash_elem));
    struct page *page = hash_entry (elem, struct page, hash_elem);
    page->addr = addr;
    hash_insert (&spt, &page->hash_elem);
    lock_release (&lock);
    printf("Lock released in add_page for page addr %p\n", page);
    printf("add_page successful for addr %p in page %p\n", addr, page);
    return page;
}

/*
 * Function:  remove_page 
 * --------------------
 *	Removes a page entry from the supplemental page table, such as when a page 
 *		is deallocated (see end of setup_stack function in process.c for an 
 *		example). 
 *
 *  page: the page to be removed from the SPT
 */
void
remove_page (struct page *page)
{
	printf("Calling remvoe_page for %p\n", page);
	lock_acquire (&lock);
	printf("Lock acquired in release_page for page addr %p\n", page);
	struct hash_elem *e = hash_delete (&spt, &page->hash_elem);
	lock_release (&lock);
	printf("lock released in remove_page for page addr %p\n", page);
	free (page);
	free (e); //TODO: Figure out if this is needed
	printf("remove_page successful for page %p\n", page);
}

/*
 * Function:  spt_init
 * --------------------
 *	Initializes the hash table for the supplemental page table using hash_init 
 *		and also initializes the lock for the SPT's hash table.
 */
void
spt_init (void) 
{
	printf("Calling spt_init...\n"); 
	lock_init(&lock);
	hash_init(&spt, page_hash, page_less, NULL);
	printf("spt_init successful: %p\n", &spt); 
}

/*
 * Function:  page_hash 
 * --------------------
 *	Hashes a page's address for use by the SPT's hash_table, especially for 
 		insertions, look ups, and deletions.
 *
 *  spt_elem: a pointer to a hash_elem struct that's associated with a page 
 *		and used to insert that page into the SPT's hash table
 *	aux: an unused parameter that comes as part of the hash_table page_hash 
 *		function signature
 *
 *  returns: a hash value for a page
 */
unsigned
page_hash (const struct hash_elem *spt_elem, void *aux UNUSED)
{
	const struct page *page = hash_entry (spt_elem, struct page, hash_elem);
	return hash_bytes (&page->addr, sizeof page->addr);
}

/*
 * Function:  page_less
 * --------------------
 *	Compares to hash_element structs and determines which element's associated 
 		entry in the SPT's hash table is "less." "Less" for SPT entries is 
 		defined as having a lower addr member within the page struct.
 *
 *  first: the first hash_elem to be compared
 *	second: the second hash_elem to be compared
 *	aux: an unused parameter that comes as part of the hash_table page_less 
 *		function signature
 *
 *  returns: a boolean value of whether a is less than b or not
 */
bool
page_less (const struct hash_elem *first, const struct hash_elem *second, void *aux UNUSED)
{
	const struct page *a = hash_entry (first, struct page, hash_elem);
	const struct page *b = hash_entry (second, struct page, hash_elem);
	return a->addr < b->addr;
}

/*
 * Function:  page_lookup
 * --------------------
 *	Looks up an address in the SPT's hash table using the hash_find function.
 *
 *  addr: the address to look for
 *
 *  returns: the page with the given address if found in the table or null if not
 */
struct page*
page_lookup (void *addr)
{
	struct page p;
	struct hash_elem *e;
	p.addr = addr;
	e = hash_find (&spt, &p.hash_elem);
	return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

void
page_destructor(struct hash_elem *e, void *aux UNUSED)
{
	free(hash_entry(e, struct page, hash_elem));	//Hope that's all...
}
