/*
 *  Fixed-Block Memory Pool
 *
 *  01-Apr-2009  Marko Kallinki  Finalized for public library
 *  18-Nov-2008  Marko Kallinki  Fully functional version
 *  30-Sep-2008  Marko Kallinki  Initial version
 *
 */

/* ----------------------------------------------------------------- */


#ifdef __cplusplus
extern "C"
{
#endif

#ifndef MPOOL_H
#define MPOOL_H


/* --------------------------------------------------------------- */

/*
 *  Memory Pool for fixed blocksize.
 *
 *  For many algorihtms (e.g. linked list), the memory handling is most
 *  time-consuming process. In case fixed size of blocks are constanly
 *  allocated and deallocated, the performance can be improved significantly
 *  by using private memory pools.
 *
 *  Default setup of memory pool:
 *    void * pool = MPoolInit(sizeof(your_struct_t), MPOOL_FALSE, MPOOL_TRUE);
 *
 *  This memory pool works as stand-alone library, but since this was designed
 *  specially for LList linked list (llist.h), this works perfectly with it.
 *
 *  Default setup with memory pool:
 *    llist_t * list = LInit(sizeof(your_struct_t), NULL, pool);
 *
 */

/* --------------------------------------------------------------- */


typedef enum
{
  MPOOL_FAILURE = 0,
  MPOOL_SUCCESS = 1
} mpool_result_e;

typedef enum
{
  MPOOL_WAIT   = 0,         /* Uses os_block_alloc (which waits until free memory available) */
  MPOOL_NOWAIT = 1          /* Uses os_block_alloc_no_wait (which returns NULL if memory run out) */
} mpool_alloc_e;

typedef enum
{
  MPOOL_NO_MEMSET   = 0,    /* Blocks are returned as uninitialized by MPoolAlloc */
  MPOOL_ZERO_MEMSET = 1     /* Blocks are filled with zeroes during MPoolAlloc */
} mpool_memset_e;

typedef enum
{
  MPOOL_RESERVE_RELEASE = 0,          /* No reservation; pool can shrink during deallocs  */
  MPOOL_RESERVE_FOR_ONE_USE,          /* Reserve space for next allocs (one time usage)   */
  MPOOL_RESERVE_PERMANENTLY,          /* Reserve space permanently in pool (no shrinking) */
} mpool_reservation_e;

typedef struct
{
  unsigned             block_size;     /* Size of one block        */
  unsigned             block_space;    /* Number of blocks in pool */
  unsigned             blocks_used;    /* Number of used blocks    */
  unsigned             blocks_free;    /* Number of free blocks    */
  mpool_reservation_e  reservation;    /* Is there preallocated space for next block allocs    */
  mpool_alloc_e        alloc;          /* os_block_alloc_no_wait() instead of os_block_alloc() */
  mpool_memset_e       memset;         /* Is allocated block auto-filled with zeroes           */
} mpool_state_t;


/*
 *  The minimum number of blocks requestes from OS for pool at once.
 *  The amount of blocks in pool is always multiple of the group.
 *
 */
#define MPOOL_BLOCKS_IN_GROUP  32


/* --------------------------------------------------------------- */


/*
 *  Initializes a memory pool, and reverses one group of
 *  blocks, which is always queranteed to be available.
 *
 *  Parameters
 *    unsigned block_size        : Common size of blocks in pool.
 *    mpool_alloc_e allocation
 *        MPOOL_WAIT             : Uses os_block_alloc, which waits
 *                                 in case of OS out of memory.
 *        MPOOL_NOWAIT           : Uses os_block_alloc_no_wait, which
 *                                 waits until OS returns a memory po�nter.
 *    mpool_bool_e memset_zero
 *        MPOOL_NO_MEMSET        : Returns memory area uncleared (faster).
 *        MPOOL_ZERO_MEMSET      : Clears the memory are with zeroes
 *
 *  Returns
 *    void * : Opeque pointer to memory pool, which may
 *             be NULL if OS out of memory (in theory).
 */
void * MPoolInit( unsigned block_size, mpool_alloc_e allocation, mpool_memset_e memory );


/*
 *  Allocates block (size given during init) from memory pool.
 *
 *  Parametes
 *    void * pool : Memomy pool
 *
 *  Returns
 *    void *      : Allocated block, which may be NULL if no_wait
 *                  used (during init) and no revervation made
 *                  while OS runs out of memory.
 */
void * MPoolAlloc(void * pool);


/*
 *  Allocates block from memory pool, with parameters that overwrite values
 *  given during memory pool init. Can be used for allocate blocks which size
 *  vary and bigger blocks are like exceptions, thus smaller blocks benefit
 *  from memory pool. Alternatively no_wait flag can be used if pool uses the
 *  os_block_alloc by default but there is temporarely very huge amount of
 *  blocks allocated and out of memory error can be handled cracefully.
 *
 *  Parametes
 *    void * pool                 : Memomy pool.
 *    unsigned size               : Either allocated from pool if size fits, otherwise from OS.
 *    mpool_alloc_e allocation    : Can use os_block_alloc_no_wait() insteas of os_block_alloc().
 *    mpool_memset_e memory       : Can clear the memory area with zeroes.
 *
 *  Returns
 *    void *      : Allocated block, which may
 *                  be NULL if OS out of memory.
 */
void * MPoolAllocFlexible(void * pool, unsigned size, mpool_alloc_e allocation, mpool_memset_e memory);


/*
 *  Deallocates the block. In case block was not allocated from the pool,
 *  if falls back for os_block_dealloc.
 *
 *  Parameters
 *    void *  pool  : Memory pool
 *    void * block  : Block to be freed
 *
 */
void MPoolDealloc(void * pool, void * block);


/*
 *  Enlarge the memorypool to guarantee that
 *  next block allocations will success.
 *
 *  Parameters
 *    void *              pool     : Memory pool.
 *    unsigned            amount   : Number of blockspace wanted.
 *    mpool_reservation_t mode
 *      MPOOL_RESERVE_FOR_ONE_USE  : Quarantees allocations once, after which
 *                                   pool can shrink during deallocation.
 *
 *      MPOOL_RESERVE_PERMANENTLY  : Quarantees requested space permantly.
 *
 *      MPOOL_RESERVE_RELEASE      : Release permanently reserved space,
 *                                   (group allocated during init is always kept)
 *
 *  Returns
 *    unsigned                     : Number of quaranteed allocations,
 *                                   may be less or more than requested, since
 *                                   os_block_alloc_no_wait is always used.
 */
unsigned MPoolReserveSpace(void * pool, unsigned amount, mpool_reservation_e mode);


/*
 *  Extracts block from memorypool, by allocating
 *  memory from underlying OS for copy, thus pointer 
 *  can ne released by os_block_dealloc directly.
 *  Used if pointer ownership is given to any functions
 *  which doesn't handle the memory pool deallocations.
 *  Extraction cancels the benefit of pool usage.
 *
 *  Parameters
 *    void *  pool   : Memory pool.
 *    void ** block  : Pointer to allocated block,
 *                     which is updated on success.
 *
 *  Returns
 *    MPOOL_SUCCESS  : On success, or block not from pool.
 *    MPOOL_FAILURE  : OS out of memory, block stays in pool.
 */
mpool_result_e MPoolExtract(void * pool, void ** block);


/*
 *  Get memory pool statistics.
 *
 *  Parameters
 *    void * pool      : Memory pool.
 *
 *  Returns
 *    mpool_state_t    : Current block amounts and modes set during init.
 *
 */
mpool_state_t MPoolGetStatistics(void * pool);


/*
 *  Disposes the memory pool. Note that all pointers to
 *  blocks allocated from pool becomes invalid (and freed).
 *
 *  Parameters
 *    void * pool      : Memory pool, which becomes NULL.
 *
 */
void MPoolDispose(void ** pool);


#endif /* MPOOL_H */

#ifdef __cplusplus
}
#endif

