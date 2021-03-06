/*
 * mm/rmap.c - physical to virtual reverse mappings
 *
 * Copyright 2001, Rik van Riel <riel@conectiva.com.br>
 * Released under the General Public License (GPL).
 *
 *
 * Simple, low overhead pte-based reverse mapping scheme.
 * This is kept modular because we may want to experiment
 * with object-based reverse mapping schemes. Please try
 * to keep this thing as modular as possible.
 */

/*
 * Locking:
 * - the page->pte_chain is protected by the PG_chainlock bit,
 *   which nests within the pagemap_lru_lock, then the
 *   mm->page_table_lock, and then the page lock.
 * - because swapout locking is opposite to the locking order
 *   in the page fault path, the swapout path uses trylocks
 *   on the mm->page_table_lock
 */
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/swap.h>

#include <asm/pgalloc.h>
#include <asm/rmap.h>
#include <asm/smplock.h>

/* #define DEBUG_RMAP */

/*
 * Shared pages have a chain of pte_chain structures, used to locate
 * all the mappings to this page. We only need a pointer to the pte
 * here, the page struct for the page table page contains the process
 * it belongs to and the offset within that process.
 *
 * A singly linked list should be fine for most, if not all, workloads.
 * On fork-after-exec the mapping we'll be removing will still be near
 * the start of the list, on mixed application systems the short-lived
 * processes will have their mappings near the start of the list and
 * in systems with long-lived applications the relative overhead of
 * exit() will be lower since the applications are long-lived.
 */
struct pte_chain {
	struct pte_chain * next;
	pte_t * ptep;
};

static inline struct pte_chain * pte_chain_alloc(zone_t *);
static inline void pte_chain_free(struct pte_chain *, struct pte_chain *,
		struct page *, zone_t *);
static void alloc_new_pte_chains(zone_t *);

/**
 * page_referenced - test if the page was referenced
 * @page: the page to test
 *
 * Quick test_and_clear_referenced for all mappings to a page,
 * returns the number of processes which referenced the page.
 * Caller needs to hold the pte_chain_lock.
 */
int page_referenced(struct page * page)
{
	struct pte_chain * pc;
	int referenced = 0;

	if (PageTestandClearReferenced(page))
		referenced++;

	/* Check all the page tables mapping this page. */
	for (pc = page->pte_chain; pc; pc = pc->next) {
		if (ptep_test_and_clear_young(pc->ptep))
			referenced++;
	}

	return referenced;
}

/**
 * page_add_rmap - add reverse mapping entry to a page
 * @page: the page to add the mapping to
 * @ptep: the page table entry mapping this page
 *
 * Add a new pte reverse mapping to a page.
 * The caller needs to hold the mm->page_table_lock.
 */
void page_add_rmap(struct page * page, pte_t * ptep)
{
	struct pte_chain * pte_chain;

#ifdef DEBUG_RMAP
	if (!page || !ptep)
		BUG();
	if (!pte_present(*ptep))
		BUG();
	if (!ptep_to_mm(ptep));
		BUG();
#endif

	if (!VALID_PAGE(page) || PageReserved(page))
		return;

#ifdef DEBUG_RMAP
	pte_chain_lock(page);
	{
		struct pte_chain * pc;
		for (pc = page->pte_chain; pc; pc = pc->next) {
			if (pc->ptep == ptep)
				BUG();
		}
	}
	pte_chain_unlock(page);
#endif

	pte_chain = pte_chain_alloc(page_zone(page));

	pte_chain_lock(page);

	/* Hook up the pte_chain to the page. */
	pte_chain->ptep = ptep;
	pte_chain->next = page->pte_chain;
	page->pte_chain = pte_chain;

	pte_chain_unlock(page);
}

/**
 * page_remove_rmap - take down reverse mapping to a page
 * @page: page to remove mapping from
 * @ptep: page table entry to remove
 *
 * Removes the reverse mapping from the pte_chain of the page,
 * after that the caller can clear the page table entry and free
 * the page.
 * Caller needs to hold the mm->page_table_lock.
 */
void page_remove_rmap(struct page * page, pte_t * ptep)
{
	struct pte_chain * pc, * prev_pc = NULL;
	zone_t *zone;

	if (!page || !ptep)
		BUG();
	if (!VALID_PAGE(page) || PageReserved(page))
		return;

	zone = page_zone(page);

	pte_chain_lock(page);
	for (pc = page->pte_chain; pc; prev_pc = pc, pc = pc->next) {
		if (pc->ptep == ptep) {
			pte_chain_free(pc, prev_pc, page, zone);
			goto out;
		}
	}
#ifdef DEBUG_RMAP
	/* Not found. This should NEVER happen! */
	printk(KERN_ERR "page_remove_rmap: pte_chain %p not present.\n", ptep);
	printk(KERN_ERR "page_remove_rmap: only found: ");
	for (pc = page->pte_chain; pc; pc = pc->next)
		printk("%p ", pc->ptep);
	printk("\n");
	printk(KERN_ERR "page_remove_rmap: driver cleared PG_reserved ?\n");
#endif

out:
	pte_chain_unlock(page);
	return;
			
}

/**
 * try_to_unmap_one - worker function for try_to_unmap
 * @page: page to unmap
 * @ptep: page table entry to unmap from page
 *
 * Internal helper function for try_to_unmap, called for each page
 * table entry mapping a page. Because locking order here is opposite
 * to the locking order used by the page fault path, we use trylocks.
 * Locking:
 *	pagemap_lru_lock		page_launder()
 *	    page lock			page_launder(), trylock
 *		pte_chain_lock		page_launder()
 *		    mm->page_table_lock	try_to_unmap_one(), trylock
 */
static int FASTCALL(try_to_unmap_one(struct page *, pte_t *));
static int try_to_unmap_one(struct page * page, pte_t * ptep)
{
	unsigned long address = ptep_to_address(ptep);
	struct mm_struct * mm = ptep_to_mm(ptep);
	struct vm_area_struct * vma;
	pte_t pte;
	int ret;

	if (!mm)
		BUG();

	/*
	 * We need the page_table_lock to protect us from page faults,
	 * munmap, fork, etc...
	 */
	if (!spin_trylock(&mm->page_table_lock))
		return SWAP_AGAIN;

	/* During mremap, it's possible pages are not in a VMA. */
	vma = find_vma(mm, address);
	if (!vma) {
		ret = SWAP_FAIL;
		goto out_unlock;
	}

	/* The page is mlock()d, we cannot swap it out. */
	if (vma->vm_flags & VM_LOCKED) {
		ret = SWAP_FAIL;
		goto out_unlock;
	}

	/* Nuke the page table entry. */
	pte = ptep_get_and_clear(ptep);
	flush_tlb_page(vma, address);
	flush_cache_page(vma, address);

	/* Store the swap location in the pte. See handle_pte_fault() ... */
	if (PageSwapCache(page)) {
		swp_entry_t entry;
		entry.val = page->index;
		swap_duplicate(entry);
		set_pte(ptep, swp_entry_to_pte(entry));
	}

	/* Move the dirty bit to the physical page now the pte is gone. */
	if (pte_dirty(pte))
		set_page_dirty(page);

	mm->rss--;
	page_cache_release(page);
	ret = SWAP_SUCCESS;

out_unlock:
	spin_unlock(&mm->page_table_lock);
	return ret;
}

/**
 * try_to_unmap - try to remove all page table mappings to a page
 * @page: the page to get unmapped
 *
 * Tries to remove all the page table entries which are mapping this
 * page, used in the pageout path.  Caller must hold pagemap_lru_lock
 * and the page lock.  Return values are:
 *
 * SWAP_SUCCESS	- we succeeded in removing all mappings
 * SWAP_AGAIN	- we missed a trylock, try again later
 * SWAP_FAIL	- the page is unswappable
 * SWAP_ERROR	- an error occurred
 */
int try_to_unmap(struct page * page)
{
	struct pte_chain * pc, * next_pc, * prev_pc = NULL;
	zone_t *zone = page_zone(page);
	int ret = SWAP_SUCCESS;

	/* This page should not be on the pageout lists. */
	if (!VALID_PAGE(page) || PageReserved(page))
		BUG();
	if (!PageLocked(page))
		BUG();
	/* We need backing store to swap out a page. */
	if (!page->mapping)
		BUG();

	for (pc = page->pte_chain; pc; pc = next_pc) {
		next_pc = pc->next;
		switch (try_to_unmap_one(page, pc->ptep)) {
			case SWAP_SUCCESS:
				/* Free the pte_chain struct. */
				pte_chain_free(pc, prev_pc, page, zone);
				break;
			case SWAP_AGAIN:
				/* Skip this pte, remembering status. */
				prev_pc = pc;
				ret = SWAP_AGAIN;
				continue;
			case SWAP_FAIL:
				return SWAP_FAIL;
			case SWAP_ERROR:
				return SWAP_ERROR;
		}
	}

	return ret;
}

/**
 * page_over_rsslimit - test if the page is over its RSS limit
 * @page - page to test
 *
 * This function returns true if the process owning this page
 * is over its RSS (resident set size) limit.  For shared pages
 * we penalise it only if all processes using it are over their
 * rss limits.
 * The caller needs to hold the page's pte_chain_lock.
 */
int page_over_rsslimit(struct page * page)
{
	struct pte_chain * pte_chain = page->pte_chain;
	struct mm_struct * mm;
	pte_t * ptep;

	/* No process is using the page. */
	if (!pte_chain)
		return 0;

	do {
		ptep = pte_chain->ptep;
		mm = ptep_to_mm(ptep);

		/*
		 * If the process is under its RSS limit, stop
		 * scanning and don't penalise the page.
		 */
		if(!mm->rlimit_rss || mm->rss <= mm->rlimit_rss)
			return 0;
		
		pte_chain = pte_chain->next;
	} while (pte_chain);

	return 1;
}

/**
 ** No more VM stuff below this comment, only pte_chain helper
 ** functions.
 **/

static inline void pte_chain_push(zone_t * zone,
		struct pte_chain * pte_chain)
{
	pte_chain->ptep = NULL;
	pte_chain->next = zone->pte_chain_freelist;
	zone->pte_chain_freelist = pte_chain;
}

static inline struct pte_chain * pte_chain_pop(zone_t * zone)
{
	struct pte_chain *pte_chain;

	pte_chain = zone->pte_chain_freelist;
	zone->pte_chain_freelist = pte_chain->next;
	pte_chain->next = NULL;

	return pte_chain;
}

/**
 * pte_chain_free - free pte_chain structure
 * @pte_chain: pte_chain struct to free
 * @prev_pte_chain: previous pte_chain on the list (may be NULL)
 * @page: page this pte_chain hangs off (may be NULL)
 * @zone: memory zone to free pte chain in
 *
 * This function unlinks pte_chain from the singly linked list it
 * may be on and adds the pte_chain to the free list. May also be
 * called for new pte_chain structures which aren't on any list yet.
 * Caller needs to hold the pte_chain_lock if the page is non-NULL.
 */
static inline void pte_chain_free(struct pte_chain * pte_chain,
		struct pte_chain * prev_pte_chain, struct page * page,
		zone_t * zone)
{
	if (prev_pte_chain)
		prev_pte_chain->next = pte_chain->next;
	else if (page)
		page->pte_chain = pte_chain->next;

	spin_lock(&zone->pte_chain_freelist_lock);
	pte_chain_push(zone, pte_chain);
	spin_unlock(&zone->pte_chain_freelist_lock);
}

/**
 * pte_chain_alloc - allocate a pte_chain struct
 * @zone: memory zone to allocate pte_chain for
 *
 * Returns a pointer to a fresh pte_chain structure. Allocates new
 * pte_chain structures as required.
 * Caller needs to hold the page's pte_chain_lock.
 */
static inline struct pte_chain * pte_chain_alloc(zone_t * zone)
{
	struct pte_chain * pte_chain;

	spin_lock(&zone->pte_chain_freelist_lock);

	/* Allocate new pte_chain structs as needed. */
	if (!zone->pte_chain_freelist)
		alloc_new_pte_chains(zone);

	/* Grab the first pte_chain from the freelist. */
	pte_chain = pte_chain_pop(zone);

	spin_unlock(&zone->pte_chain_freelist_lock);

	return pte_chain;
}

/**
 * alloc_new_pte_chains - convert a free page to pte_chain structures
 * @zone: memory zone to allocate pte_chains for
 *
 * Grabs a free page and converts it to pte_chain structures. We really
 * should pre-allocate these earlier in the pagefault path or come up
 * with some other trick.
 *
 * Note that we cannot use the slab cache because the pte_chain structure
 * is way smaller than the minimum size of a slab cache allocation.
 * Caller needs to hold the zone->pte_chain_freelist_lock
 */
static void alloc_new_pte_chains(zone_t *zone)
{
	struct pte_chain * pte_chain = (void *) get_zeroed_page(GFP_ATOMIC);
	int i = PAGE_SIZE / sizeof(struct pte_chain);

	if (pte_chain) {
		for (; i-- > 0; pte_chain++)
			pte_chain_push(zone, pte_chain);
	} else {
		/* Yeah yeah, I'll fix the pte_chain allocation ... */
		panic("Fix pte_chain allocation, you lazy bastard!\n");
	}
}
