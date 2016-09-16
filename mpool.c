/*
 *  Fixed-Block Memory Pool
 *
 *  01-Apr-2009  Marko Kallinki  Finalized for public library
 *  18-Nov-2008  Marko Kallinki  Fully functional version
 *  30-Sep-2008  Marko Kallinki  Initial version
 *
 */

/*
 *  Memory pool allocator.
 *
 *  Memory handling is most time consuming operation of many tasks, e.g.node
 *  allocations for linked list, but if the block size is fixed, memory pool
 *  can be allocated for certain amount of blocks. Pool can be used for first
 *  few allocations and after capacity has been reached allocation for a new
 *  silo will occur. Removing blocks also restores the capacity.
 *
 *  One bit in bitfied is used to mark each used block space.
 *
 *   memchart: [10011011]
 *  +------*----+----+----+----+----+----+----+----*
 *  |Silo1 |Used|....|....|Used|Used|....|Used|Used|
 *  +------*----+----+----+----+----+----+----+----*
 *  |Silo2 |....|....|....|....|Used|Used|Used|Used|
 *  +------*----+----+----+----+----+----+----+----*
 *  |...   |
 *
 *
 *  The amount of blocks in one silo is limited to bitfield size, 
 *  but as a result block allocations are superfast and overhead of operations
 *  is almost nonexisting. The speed is somewhat comparable to array of blocks.
 *
 *  The memory area/usage violations cannot be detected as easily, as for other
 *  normally allocated memory blocks, but the usage of this memory pool should
 *  be limited only for well-known and tested cases, instead of general usage
 *  for any purposes. For example usage of fixed size structures for linked 
 *  list nodes rarely causes any memory issues, like tail-overwrites.
 *
 *  Expanding arrays (where size is doubled after capacity reached) also have
 *  certain amount of unused space. And pool itself takes a little less space
 *  than allocating blocks induvidually, as each allocation has some header
 *  overhead.
 *
 */


#ifdef MPOOL_UNITTEST

static void * os_block_alloc(unsigned size);
static void * os_block_alloc_and_clear(unsigned size);
static void * os_block_alloc_no_wait(unsigned size);
static void   os_block_dealloc(void * ptr);

#else /* MPOOL_UNITTEST */

/* Embedded OS headers */
#include "global.h"
#include "type_def.h"
#include "os.h"

#endif /* MPOOL_UNITTEST */

/* External libraries */
#include <string.h>
#include <stddef.h>
#include <assert.h>
#include "llist.h"

/* Library header */
#include "mpool.h"


/* ----------------------------------------------------------------- */


typedef struct
{
  LLIST_NODE
  unsigned memchart;
  void * addr_limit;
  /* blocks[SILO_CAPACITY] */
} msilo_t;


typedef struct
{
  unsigned capacity;
  unsigned used;
  signed   reserved;
  unsigned block_size;
  unsigned modes;
  llist_t  silos;
} mpool_t;


#if MPOOL_BLOCKS_IN_GROUP > 32
#error "Error: Maximum amount of blocks in group is 32"
#endif


#define SILO_CAPACITY                MPOOL_BLOCKS_IN_GROUP
#define SILO_CAPACITY_MASK           0xFFFFFFFF //(unsigned)(pow(2,SILO_CAPACITY)-1) /* constant by preprocessor */
#define SILO_IS_FULL( silo )         (((silo)->memchart & SILO_CAPACITY_MASK) == SILO_CAPACITY_MASK)
#define SILO_IS_EMPTY( silo )        (((silo)->memchart ^ SILO_CAPACITY_MASK) == SILO_CAPACITY_MASK)
#define SILO_ADDRESS( silo, block )  ((void*)(silo) < block && block < (silo)->addr_limit)
#define SILO_SIZE( size )            (sizeof(msilo_t) + (size) * SILO_CAPACITY)
#define SILO_LIMIT( silo, size )     (void*)((char*)(silo) + SILO_SIZE(size))
#define MODE_MEMSET( pool )          ((mpool)->modes & MPOOL_TRUE)
#define MODE_NOWAIT( pool )          ((mpool)->modes & (MPOOL_TRUE < 1))


/*
 *
 *
 */
static msilo_t * create_new_silo( mpool_t * mpool, lbool_e no_wait )
{
  msilo_t * silo = (msilo_t*)LAlloc(SILO_SIZE(mpool->block_size), no_wait);

  if (silo)
    {
      silo->addr_limit = SILO_LIMIT(silo, mpool->block_size);
      mpool->capacity += SILO_CAPACITY;

      LAttachLast(&mpool->silos, (lnode_t*)silo);
    }

  return silo;
}


/*
 *
 *
 */
static void cleanup_empty_silos( mpool_t * mpool, mpool_bool_e check_all)
{
  msilo_t * silo = (msilo_t*)LFirst(&mpool->silos);

  /* Skip the first silo (since it is permanent) */
  silo = (msilo_t*)LNext(silo);

  while(silo)
    {
      if (SILO_IS_EMPTY(silo))
        {
          LRemove(&mpool->silos, (lnode_t*)silo);
          mpool->capacity -= SILO_CAPACITY;

          if (check_all == MPOOL_FALSE)
            {
              break;
            }
        }

      silo = (msilo_t*)LNext(silo);
    }
}


/*
 *
 *
 */
static void * silo_block_alloc(mpool_t * mpool, msilo_t * silo)
{
  char * ptr = (char*)silo + sizeof(msilo_t);
  unsigned memchart = silo->memchart;
  register unsigned bitmask = 1;
  register unsigned index   = 0;

  /* Optimization for while loop (max 8 loops instead of 32) */
  if ((memchart & 0x000000FF) == 0x000000FF)
    {
      if ((memchart & 0x0000FF00) != 0x0000FF00)
        {
          index   = 8;
          bitmask = 1 << 8;
        }
      else if ((memchart & 0x00FF0000) != 0x00FF0000)
        {
          index   = 16;
          bitmask = 1 << 16;
        }
      else
        {
          index   = 24;
          bitmask = 1 << 24;
        }
    }

  /* Get index to free "blocks[0..index..32]" */
  while(memchart & bitmask)
    {
      bitmask = bitmask << 1;
      index++;
    }

  /* Mark the "blocks[index]" as used */
  silo->memchart |= bitmask;

  mpool->used++;

  if (mpool->reserved > 0)
    {
      mpool->reserved--;
    }

  return (void*)(ptr + mpool->block_size * index);
}


/*
 *
 *
 */
static void silo_block_dealloc(mpool_t * mpool, msilo_t * silo, void * block)
{
  /* Calculate index (instead of search) */
  unsigned index = (unsigned)((unsigned)block - 
      ((unsigned)silo + sizeof(msilo_t))) / mpool->block_size;

  /* Mark the "blocks[index]" as free */
  silo->memchart &= ~(1 << index);

  mpool->used--;
}


/* ----------------------------------------------------------------- */


/*
 *
 *
 */
void * MPoolInit( unsigned block_size, mpool_bool_e alloc_no_wait, mpool_bool_e memset_zero )
{
  mpool_t * mpool;
  unsigned init_size;

  /* Fix the alingment */
  if (block_size % sizeof(void*))
    {
      block_size += sizeof(void*) - (block_size % sizeof(void*));
    }

  /* Allocation includes memory pool and linked list headers and one silo */
  init_size = sizeof(mpool_t) + SILO_SIZE(block_size);

  if (alloc_no_wait)
    {
      mpool = os_block_alloc_no_wait(init_size);
    }
  else
    {
      mpool = os_block_alloc(init_size);
    }

  if (mpool)
    {
      msilo_t * silo;

      mpool->block_size = block_size;
      mpool->capacity   = SILO_CAPACITY;
      mpool->reserved   = SILO_CAPACITY;
      mpool->used       = 0;
      mpool->modes      = alloc_no_wait | (memset_zero < 1);

      LSetup(mpool->silos, 0, NULL);

      silo = (msilo_t*)((char*)mpool + sizeof(mpool_t));
      silo->next       = NULL;
      silo->prev       = NULL;
      silo->memchart   = 0;
      silo->addr_limit = SILO_LIMIT(silo, block_size);

      LAttachLast(&mpool->silos, (lnode_t*)silo);
    }

  return mpool;
}


/*
 *
 *
 */
void * MPoolAlloc(void * pool)
{
  void * block = NULL;

  if (pool)
    {
      mpool_t * mpool = (mpool_t*)pool;
      msilo_t * silo  = (msilo_t*)LLast(&mpool->silos);

      do
        {
          if (!SILO_IS_FULL(silo))
            {
            block = silo_block_alloc(mpool, silo);
            break;
            }

          silo = (msilo_t*)LPrev(silo);
        }
      while(silo);

      if (!block)
        {
          silo = create_new_silo(mpool, MODE_NOWAIT(mpool));

          if (silo)
            {
              block = silo_block_alloc(mpool, silo);
            }
        }

      if (MODE_MEMSET(mpool) && block)
        {
          memset(block, 0, mpool->block_size);
        }
    }

  return block;
}


/*
 *
 *
 */
void * MPoolAllocFlexible(void * pool, unsigned size, mpool_bool_e alloc_no_wait, mpool_bool_e memset_zero)
{
  void * block = NULL;

  if (pool)
    {
      mpool_t * mpool = (mpool_t*)pool;

      if (size <= mpool->block_size)
        {
          msilo_t * silo  = (msilo_t*)LLast(&mpool->silos);

          do
            {
              if (!SILO_IS_FULL(silo))
                {
                block = silo_block_alloc(mpool, silo);
                break;
                }

              silo = (msilo_t*)LPrev(silo);
            }
          while(silo);

          if (!block)
            {
              silo = create_new_silo(mpool, alloc_no_wait);

              if (silo)
                {
                  block = silo_block_alloc(mpool, silo);
                }
            }
        }
      else
        {
          if (alloc_no_wait)
            {
              block = os_block_alloc_no_wait(size);
            }
          else
            {
              block = os_block_alloc(size);
            }
        }

      if (MODE_MEMSET(mpool) && block)
        {
          memset(block, 0, mpool->block_size);
        }
    }

  return block;
}



/*
 *
 *
 */
void MPoolDealloc(void * pool, void ** block)
{
  if (block)
    {
      mpool_t * mpool = (mpool_t*)pool;
      msilo_t * silo  = (msilo_t*)LLast(&mpool->silos);

      if (pool)
        {
          do
            {
              if (SILO_ADDRESS(silo, *block))
                {
                  silo_block_dealloc(mpool, silo, *block);

                  if (LCount(&mpool->silos) > 1
                      && mpool->reserved == SILO_CAPACITY
                      && mpool->capacity > mpool->used * 2)
                    {
                      cleanup_empty_silos(mpool, MPOOL_FALSE);
                    }

                  return;
                }

              silo = (msilo_t*)LPrev(silo);
            }
          while(silo);
        }

      os_block_dealloc(*block);
      *block = NULL;
    }
}


/*
 *
 *
 */
mpool_bool_e MPoolExtract(void * pool, void ** block)
{
  if (pool && block)
    {
      mpool_t * mpool = (mpool_t*)pool;
      msilo_t * silo  = (msilo_t*)LFirst(&mpool->silos);
 
      do
        {
          if (SILO_ADDRESS(silo, *block))
            {
              void * ptr = os_block_alloc_no_wait(mpool->block_size);

              if (ptr)
                {
                  memcpy(ptr, *block, mpool->block_size);
                  silo_block_dealloc(mpool, silo, *block);
                  *block = ptr;
                  return MPOOL_TRUE;
                }
            }

          silo = (msilo_t*)LNext(silo);
        }
      while(silo);
    }

  return MPOOL_FALSE;
}


/*
 *
 *
 */
unsigned MPoolReserveSpace(void * pool, unsigned amount, mpool_reservation_e mode)
{
  mpool_t * mpool = (mpool_t *)pool;
  unsigned reserved = 0;

  if (mpool)
    {
      if (mode == MPOOL_RESERVE_RELEASE)
        {
          if (mpool->reserved < 0)
            {
              mpool->reserved = -mpool->reserved;
            }

          if (mpool->reserved == SILO_CAPACITY)
            {
              cleanup_empty_silos(mpool, MPOOL_TRUE);
            }
        }
      else /* MPOOL_RESERVE_FOR_ONE_USE or ... */
        {
          reserved = mpool->capacity - mpool->used;

          while(reserved < amount)
            {
              if (!create_new_silo(mpool, LLIST_NO))
                {
                  break;
                }

              reserved += SILO_CAPACITY;
            }

          mpool->reserved += (unsigned)reserved;

          if (mode == MPOOL_RESERVE_PERMANENTLY)
            {
              /* Negative value prevents shrinking */
              mpool->reserved = -mpool->reserved;
            }
        }
    }

  return reserved;
}


/*
 *
 *
 */
mpool_state_t MPoolGetStatistics(void * pool)
{
  mpool_state_t statistics = {0, 0, 0, 0, 
      MPOOL_RESERVE_RELEASE, MPOOL_FALSE, MPOOL_FALSE};

  if (pool)
    {
      mpool_t * mpool = (mpool_t *)pool;

      statistics.block_size  = mpool->block_size;
      statistics.block_space = mpool->capacity;
      statistics.blocks_used = mpool->used;
      statistics.blocks_free = mpool->capacity - mpool->used;

      if (mpool->reserved != SILO_CAPACITY)
        {
          if (mpool->reserved < 0)
            {
              statistics.reservation = MPOOL_RESERVE_PERMANENTLY;
            }
          else
            {
              statistics.reservation = MPOOL_RESERVE_FOR_ONE_USE;
            }
        }

      if (MODE_NOWAIT(mpool))
        {
          statistics.alloc_no_wait = MPOOL_TRUE;
        }

      if (MODE_MEMSET(mpool))
        {
          statistics.memset_zero = MPOOL_TRUE;
        }
    }

  return statistics;
}


/*
 *
 *
 */
void MPoolDispose(void ** pool)
{
  if (*pool)
    {
      mpool_t * mpool = (mpool_t *)*pool;

      if (mpool)
        {
          /* The first node is inside the mpool object */
          (void)LDetachFirst(&mpool->silos);
          LRemoveAll(&mpool->silos);
          os_block_dealloc(mpool);
        }

      *pool = NULL;
    }
}


/* ----------------------------------------------------------------- */

#ifdef MPOOL_UNITTEST

#include <math.h>
#include <memory.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>


static unsigned os_block_alloc_occupied = 0;

static void * os_block_alloc(unsigned size)
{
  return memcpy(malloc(size), "deadbeef", 8);
}

static void * os_block_alloc_and_clear(unsigned size)
{
  return memset(malloc(size), 0, size);
}

static void * os_block_alloc_no_wait(unsigned size)
{
  return (os_block_alloc_occupied ? NULL : os_block_alloc(size));
}

static void os_block_dealloc(void* ptr)
{
  free(ptr);
}

static void dump_statistics(void * pool)
{
  mpool_state_t statistics = MPoolGetStatistics(pool);

  printf("block_size %d",statistics.block_size);
  printf("block_space %d",statistics.block_space);
  printf("blocks_used %d",statistics.blocks_used);
  printf("blocks_free %d",statistics.blocks_free);
  printf("reservation %d",statistics.reservation);
  if (statistics.alloc_no_wait) printf("- os_block_alloc_no_wait in use\n");  
  if (statistics.memset_zero)   printf("- memset zero in use\n");
}

#define OUTER_LOOP 1500
#define INNER_LOOP 1000


/*
 *  Test harness.
 *
 */
int main(void)
{

  mpool_state_t statistics;
  void * pool;
  void * block;
  void * table[INNER_LOOP*5];
  unsigned i = 0, j = 1, k = 1;

  clock_t start_time;
  clock_t end_time;

  printf("\nUnittest - mpool.c\n");

  /* Testset 1: Alloc/Dealloc */
  for(k=1;k<5;k+=1)
    {
  
      start_time = clock();    

      for(j=0;j<OUTER_LOOP;j++)
        {
          for(i=0;i<k*INNER_LOOP;i++)
            {
              table[i] = os_block_alloc(10);
            }

          for(i=0;i<k*INNER_LOOP;i++)
            {
            os_block_dealloc(table[i]);
            }
        }
 
      end_time = clock();

      printf("\nTime %3d ms", end_time - start_time);

      start_time = clock();
      
      for(j=0;j<OUTER_LOOP;j++)
        {
          pool = MPoolInit(10, MPOOL_FALSE, MPOOL_FALSE);

          for(i=0;i<k*INNER_LOOP;i++)
            {
            table[i] = MPoolAlloc(pool);
            }

          for(i=0;i<k*INNER_LOOP;i++)
            {
            MPoolDealloc(pool, &table[i]);
            }

          MPoolDispose(&pool);
        }

      end_time = clock();

      printf(" vs. %3d ms", end_time - start_time);
      printf(" (blocks %d)", INNER_LOOP*k);

    }


  pool = MPoolInit(10, MPOOL_FALSE, MPOOL_FALSE);
  block = MPoolAlloc(pool);
  MPoolExtract(pool, &block);

  statistics = MPoolGetStatistics(pool);

  printf("\nUnittest - Done.\n");

  return 0;
}


#endif /* MPOOL_UNITTEST */
