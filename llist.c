/*
 *  General Linked List
 *
 *  29-Sep-2008  Marko Kallinki  Support for build-in memory manager
 *  16-Sep-2008  Marko Kallinki  Major library upgrade
 *  04-Jun-2007  Marko Kallinki  Improvements
 *  03-Feb-2007  Marko Kallinki  Initial version
 *
 */

/*
 *  General Linked List Implementation.
 *  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 *  The principles of this implementation was:
 *  - to replace a huge amount of developers own private list implementations.
 *  - to support that, minimize amount of code to be written by library users.
 *  - to support above, this library can also handle all memory allocations.
 *  - as linked lists are widely used, pay attention to performance as well.
 *  - introduce wide range of functions to cover majorities needs, as many of
 *    "rival" libraries tend to have too limited set of functionality thus
 *    causing others to create once again their own linked list libraries.
 *
 *
 *  "It began with the forging of the Great Lists. Three most complex were
 *  given to the Core; immortal, wisest and fairest of all beings. 
 *  Seven most specific, to the Protocal Lords, great miners and craftsmen
 *  of the I/O streams. And nine, nine linked lists were gifted to the race 
 *  of UI, who above all else desire simplicity. For within these lists was
 *  bound the strength and the will to govern over each platform.
 *  But they were all of them deceived, for a new linked list was made." 
 *
 *  by Marko Kallinki , 3rd of February 2007 :)
 *
 */

/* ------------------------------------------------------------------------- */


#if defined LLIST_UNITTEST || defined MPOOL_UNITTEST

static void * os_block_alloc(unsigned size);
static void * os_block_alloc_and_clear(unsigned size);
static void * os_block_alloc_no_wait(unsigned size);
static void   os_block_dealloc(void * ptr);

#else /* LLIST_UNITTEST */

/* Embedded OS headers */
#include "global.h"
#include "type_def.h"
#include "os.h"

#endif /* LLIST_UNITTEST */

/* External libraries */
#include <string.h>
#include <stddef.h>
#include <assert.h>
#include "mpool.h"

/* Library header */
#include "llist.h"

/* --------------------------------------------------------------- */

/*
 *  None of the functions checks if e.g. list parameter is not NULL,
 *  since that is so fundamental issue that it should never occur,
 *  as it makes no sense in any context, so designer should fix
 *  their code rather than this library silently ignore those cases,
 *  thus making "zillions" of unnecessary NULL and sanity checks.
 *
 *  This library is not thread safe, and any list operations should
 *  be finished before next one can be started with same list object.
 *
 */


/*
 *  Some internal macros.
 *
 */
#define  LInitFor( type, node, list ) \
    type node = (type)LFirst(list); \
    for( ;(node);(node) = (type)LNext(node) )

#define  LInitBack( type, node, list ) \
    type node = (type)LLast(list); \
    for( ;(node);(node) = (type)LPrev(node) )

#define _LAllocate( list ) \
    ( (list)->memorypool ? (lnode_t*)MPoolAlloc((list)->memorypool) : \
    (lnode_t*)os_block_alloc_and_clear((list)->node_size) )


/*
 *  Allocates new linked list object from RAM.
 *  (used to init llist_t* -pointer declarations)
 */
llist_t * LInit( unsigned node_size, NodeClear_f NodeClear, void * memorypool )
{
  llist_t * list   = (llist_t*)os_block_alloc_and_clear(sizeof(llist_t));
  list->clear_func = NodeClear;
  list->node_size  = node_size;

  if (memorypool)
    {
      mpool_state_t stat = MPoolGetStatistics(memorypool);
      /* Sanity check for node to fit into pool block */
      assert(stat.block_size >= node_size);
      /* Sanity check for nodes being zeroed during alloc */
      assert(stat.memset_zero);
    }

  return list;
}


/*
 *  Creates new node at the end of the linked list.
 *
 */
lnode_t * LCreateLast( llist_t * list )
{
  lnode_t * node = _LAllocate(list);
  
  if (list->last)
    {
      /* (last)->next & prev<-(node) */
      node->prev = list->last;
      list->last->next = node;
      list->count++;
    }
  else
    {
      list->first = node;
      list->count = 1;
    }

  list->last = node;

  return node;
}


/*
 *  Creates new node at the beginning of the linked list.
 *
 */
lnode_t * LCreateFirst( llist_t * list )
{
  lnode_t * node = _LAllocate(list);

  if (list->first)
    {
      /* (node)->next & prev<-(first) */
      node->next = list->first;
      list->first->prev = node;
      list->count++;
    }
  else
    {
      list->last  = node;
      list->count = 1;
    }

  list->first = node;

  return node;
}


/*
 *  Creates new node before other node.
 *
 */
lnode_t * LCreateBefore( llist_t * list, lnode_t * before )
{
  if (!before || !before->prev)
    {
      return LCreateFirst( list );
    }
  else
    {
      lnode_t * node = _LAllocate(list);
      /* (other->prev)->next & prev<-(node)->next & prev<-(other) */
      before->prev->next = node;
      node->prev   = before->prev;
      node->next   = before;
      before->prev = node;
      list->count++;
      return node;
    }
}


/*
 *  Creates new node after other node.
 *
 */
lnode_t * LCreateAfter( llist_t * list, lnode_t * after )
{
  if (!after || !after->next)
    {
      return LCreateLast( list );
    }
  else
    {
      lnode_t * node = _LAllocate(list);
      /* (other)->next & prev<-(node)->next & prev<-(other->next) */
      after->next->prev = node;
      node->next  = after->next;
      node->prev  = after;
      after->next = node;
      list->count++;
      return node;
    }
}


/*
 *  Internal detachment for moving.
 *
 */
static void DetachForMoving( llist_t * list, lnode_t * node )
{
  if (node->next)
    {
      node->next->prev = node->prev;
    }
  else
    {
      list->last = node->prev;
    }

  if (node->prev)
    {
      node->prev->next = node->next;
    }
  else
    {
      list->first = node->next;
    }
}


/*
 *  Move node as a first in list.
 *
 */
void LMoveFirst( llist_t * list, lnode_t * node )
{
  if (node && node != list->first)
    {
      DetachForMoving(list, node);

      node->prev = NULL;
      node->next = list->first;
      list->first->prev = node;
      list->first = node;
    }
}


/*
 *  Move node as a last in list.
 *
 */
void LMoveLast( llist_t * list, lnode_t * node )
{
  if (node && node != list->last)
    {
      DetachForMoving(list, node);

      node->next = NULL;
      node->prev = list->last;
      list->last->next = node;
      list->last = node;
    }
}


/*
 *  Move node before other node.
 *
 */
void LMoveAfter(  llist_t * list, lnode_t * node, lnode_t * after )
{
  if (node && after && node != after)
    {
      DetachForMoving(list, node);

      if (after->next)
        {
          after->next->prev = node;
          node->next = after->next;
        }
      else
        {
          list->last = node;
          node->next = NULL;
        }

      node->prev = after;
      after->next = node;
    }
}


/*
 *  Move node after other node.
 *
 */
void LMoveBefore( llist_t * list, lnode_t * node, lnode_t * before )
{
  if (node && before && node != before)
    {
      DetachForMoving(list, node);

      if (before->prev)
        {
          before->prev->next = node;
          node->prev = before->prev;
        }
      else
        {
          list->first = node;
          node->prev  = NULL;
        }

      before->prev = node;
      node->next = before;
    }
}


/*
 *
 *
 */
 void LRemoveLast(  llist_t * list )
{
  LRemove(list, LLast(list));
}


/*
 *
 *
 */
 void LRemoveFirst( llist_t * list )
{
  LRemove(list, LFirst(list));
}


/*
 *  Removes node from linked list and calls 
 *  given deallocator or deallocates it.
 */
void LRemove( llist_t * list, lnode_t * node )
{
  if (node)
    {
      LDetach( list, node );

      if (list->clear_func)
        {
          if (list->clear_func(node))
            {
              if (list->memorypool)
                {
                  MPoolDealloc(list->memorypool, (void*)&node);
                }
              else
                {
                  os_block_dealloc(node);
                }
            }
        }
      else
        {
          os_block_dealloc(node);
        }
    }
}


/*
 *  Cleans the list from nodes by deallocating them,
 *  but the list object itself remains valid.
 */
void LRemoveAll( llist_t * list )
{
  LDealloc(list->first, list->clear_func, list->memorypool);

  list->first = NULL;
  list->last  = NULL;
  list->count = 0;
}


/*
 *  Disposes the linked list by deallocating nodes
 *  and finally the list object itself.
 */
void LDispose( llist_t ** list )
{
  if (*list)
    {
      LDealloc((*list)->first, (*list)->clear_func, (*list)->memorypool);
      os_block_dealloc(*list);
      *list = NULL;
    }
}


/* --------------------------------------------------------------- */

/*
 *  Allocates (detached) node with any given size, at least size of
 *  sizeof(lnode_t*)), with os_block_alloc or os_block_alloc_no_wait.
 *  Node can be attached to linked list separately after created.
 */
lnode_t * LAlloc( unsigned any_size, lbool_e no_wait )
{
  lnode_t * node;  

  if (no_wait != LLIST_NO)
    {
      node = os_block_alloc_no_wait(any_size);
      
      if (node)
        {
          (void)memset(node, 0, any_size);
        }
    }
  else
    {
      node = os_block_alloc_and_clear(any_size);
    }

  return node;
}


/*
 *  Deallocates all detached nodes.
 *
 */
void LDealloc( lnode_t * node, NodeClear_f NodeClear, void * memorypool )
{
  if (node)
    {
      lnode_t * remove;

      /*
       *  Safety loop to find the first node.
       */
      while(node->prev)
        {
          node = node->prev;
        }

      /*
       *  Deallocate all linked nodes.
       */
      if (memorypool)
        {
          if (NodeClear)
            {
              do{
                  remove = node;
                  node = node->next;

                  if (NodeClear(remove))
                    {
                      MPoolDealloc(memorypool, (void*)&remove);
                    }
                }
              while(node);
            }
          else
            {
              do{
                  remove = node;
                  node = node->next;
                  MPoolDealloc(memorypool, (void*)&remove);
                }
              while(node);
            }
        }
      else
        {
          if (NodeClear)
            {
              do{
                  remove = node;
                  node = node->next;

                  if (NodeClear(remove))
                    {
                      os_block_dealloc(remove);
                    }
                }
              while(node);
            }
          else
            {
              do{
                  remove = node;
                  node = node->next;
                  os_block_dealloc(remove);
                }
              while(node);
            }
        }
    }
}


/*
 *  Detach (pop) the first node from the list.
 *
 */
lnode_t * LDetachFirst( llist_t * list )
{
  lnode_t * node = list->first;

  if (list->count > 1)
    {
      list->count--;
      list->first = node->next;
      node->next->prev = NULL;
      node->next  = NULL;
    }
  else
    {
      list->count = 0;
      list->first = NULL;
      list->last  = NULL;
    }

  return node;
}


/*
 *  Detach (pop) the last node from the list.
 *
 */
lnode_t * LDetachLast( llist_t * list )
{
  lnode_t * node = list->last;

  if (list->count > 1)
    {
      list->count--;
      list->last = node->prev;
      node->prev->next = NULL;
      node->prev = NULL;
    }
  else
    {
      list->count = 0;
      list->first = NULL;
      list->last  = NULL;
    }

  return node;
}


/*
 *  Detach all nodes from linked list.
 *
 */
lnode_t * LDetachAll( llist_t * list )
{
  lnode_t * node = list->first;

  list->first = NULL;
  list->last  = NULL;
  list->count = 0;

  return node;
}


/*
 *  Detach one node from linked list.
 *
 */
void LDetach( llist_t * list, lnode_t * node )
{
  if (node)
    {
      if (node->next)
        {
          node->next->prev = node->prev;
        }
      else
        {
          list->last = node->prev;
        }

      if (node->prev)
        {
          node->prev->next = node->next;
        }
      else
        {
          list->first = node->next;
        }

      node->next = NULL;
      node->prev = NULL;
      list->count--;
    }
}


/*
 *  Detach number of nodes from linked list starting with given node.
 *
 */
void LDetachMany( llist_t * list, lnode_t * node, unsigned number, lloop_e direction )
{
  if (node)
    {
      lnode_t * loop = node;
      unsigned count;

      if (number == 0)
        {
          /*
           *  Detach all the rest to the end of the list.
           */
          number = (unsigned)~0;
        }

      count = number;

      if (direction == LLOOP_FORWARD)
        {
          /*
           *  Get pointer to last node to be detached.
           */
          while(--count && loop->next)
            {
              loop = loop->next;
            }
        }
      else /* LLOOP_BACKWARD */
        {
          /*
           *  Get pointer to first node to be detached.
           */
          while(--count && node->prev)
            {
              node = node->prev;
            }
        }

      if (loop->next)
        {
          loop->next->prev = node->prev;
        }
      else
        {
          list->last = node->prev;
        }

      if (node->prev)
        {
          node->prev->next = loop->next;
        }
      else
        {
          list->first = loop->next;
        }

      loop->next = NULL;
      node->prev = NULL;
      list->count -= (number - count);
    }
}


/*
 *  Count detached nodes.
 *
 */
unsigned LDetachCount( lnode_t * node, lloop_e direction )
{
  unsigned i = 0;

  if (direction == LLOOP_FORWARD)
    {
      while(node)
        {
          if (node = node->next)
            {
              if (node = node->next)
                {
                  if (node = node->next)
                    {
                      if (node = node->next)
                        {
                          node = node->next;
                          i = i + 5;
                        }
                      else i += 4;
                    }
                  else i += 3;
                }
              else i += 2;
            } 
          else i++;
        }
    }
  else
    {
      while(node)
        {
          if (node = node->prev)
            {
              if (node = node->prev)
                {
                  if (node = node->prev)
                    {
                      if (node = node->prev)
                        {
                          node = node->prev;
                          i = i + 5;
                        }
                      else i += 4;
                    }
                  else i += 3;
                }
              else i += 2;
            } 
          else i++;
        }
    }

  return i;
}


/*
 *  Attach node(s) to end of the linked list.
 *
 */
void LAttachLast( llist_t * list, lnode_t * node )
{
  LAttachAfter( list, node, list->last );
}


/*
 *  Attach node(s) to beginning of the linked list.
 *
 */
void LAttachFirst( llist_t * list, lnode_t * node )
{
  LAttachBefore( list, node, list->first );
}


/*
 *  Attach node(s) to linked list, after other given node.
 *
 */
void LAttachAfter( llist_t * list, lnode_t * node, lnode_t * after )
{
  lnode_t * loop = node;
  unsigned count = 1;

  /*
   *  Count detached nodes and get pointer to last one.
   */
  while(loop->next)
    {
      loop = loop->next;
      count++;
    }

  if (after)
    {
      if (after->next)
        {
          after->next->prev = loop;
          loop->next = after->next;
        }
      else
        {
          list->last = loop;
        }

      node->prev = after;
      after->next = node;
    }
  else
    {
      /*
       *  If 'after' is NULL, the list is probably empty
       *  (after = list->first/last, which can be NULL),
       *  otherwise it doesn't make any sense -> assert!
       */
      assert(!list->count);

      list->first = node;
      list->last  = loop;
    }

  list->count += count;
}


/*
 *  Attach (push) node(s) to linked list, before other given node.
 *
 */
void LAttachBefore( llist_t * list, lnode_t * node, lnode_t * before )
{
  lnode_t * loop = node;
  unsigned count = 1;

  /*
   *  Count detached nodes and get pointer to last one.
   */
  while(loop->next)
    {
      loop = loop->next;
      count++;
    }

  if (before)
    {
      if (before->prev)
        {
          before->prev->next = node;
          node->prev = before->prev;
        }
      else
        {
          list->first = node;
        }

      before->prev = loop;
      loop->next = before;
    }
  else
    {
      /*
       *  If 'before' is NULL, the list is probably empty
       *  (before = list->first/last, which can be NULL),
       *  otherwise it doesn't make any sense -> assert!
       */
      assert(!list->count);

      list->first = node;
      list->last  = loop;
    }

  list->count += count;
}


/*
 *
 *
 */
void LAttachSorted( llist_t * list, lnode_t * node, NodeCmp_f NodeCmp, lloop_e direction )
{
  if (node && NodeCmp)
    {
      if (direction == LLOOP_FORWARD)
        {
          LInitFor(lnode_t*, loop, list)
            {
              if (NodeCmp(node, loop) <= LNODECMP_EQUAL)
                {
                  LAttachBefore(list, node, loop);
                  return;
                }
            }

          LAttachLast(list, node);
        }
      else
        {  
          LInitBack(lnode_t*, loop, list)
            {
              if (NodeCmp(node, loop) >= LNODECMP_EQUAL)
                {
                  LAttachAfter(list, node, loop);
                  return;
                }
            }

          LAttachFirst(list, node);
        }
    }
}


/*
 *  Swaps node placements inside the same list.
 *
 */
void LSwapInside( llist_t * list, lnode_t * node1, lnode_t * node2 )
{
  if (node1 && node2)
    {
      /*
       *  Nodes in same list can be next each other
       *  or either can be first or last of the list.
       *  Nodes cannot be NULLs, as it makes no sense.
       */
      if (node1->next == node2)
        {
          if (node2->next)
            {
              node2->next->prev = node1;
            }
          else
            {
              list->last = node1;
            }

          if (node1->prev)
            {
              node1->prev->next = node2;
            }
          else
            {
              list->first = node2;
            }

          node1->next = node2->next;
          node2->next = node1;
          node2->prev = node1->prev;
          node1->prev = node2;
        }
      else if (node1->prev == node2)
        {
          if (node2->prev)
            {
              node2->prev->next = node1;
            }
          else
            {
              list->first = node1;
            }

          if (node1->next)
            {
              node1->next->prev = node2;
            }
          else
            {
              list->last = node2;
            }

          node2->next = node1->next;
          node1->next = node2;
          node1->prev = node2->prev;
          node2->prev = node1;
        }
      else if (node1 != node2)
        {
          lnode_t* swap;

          if (node1->prev)
            {
              node1->prev->next = node2;

              if (node2->prev)
                {
                  node2->prev->next = node1;
                }
              else
                {
                  list->first = node1;
                }
            }
          else
            {
              list->first = node2;
              node2->prev->next = node1;
            }

          if (node1->next)
            {
              node1->next->prev = node2;

              if (node2->next)
                {
                  node2->next->prev = node1;
                }
              else
                {
                  list->last = node1;
                }
            }
          else
            {
              list->last = node2;
              node2->next->prev = node1;
            }

          swap        = node1->next;
          node1->next = node2->next;
          node2->next = swap;

          swap        = node1->prev;
          node1->prev = node2->prev;
          node2->prev = swap;
        }
    }
}


/*
 *  Swaps node placements between two lists.
 *
 */
void LSwapBetween( llist_t * list1, lnode_t * node1, llist_t * list2, lnode_t * node2 )
{
  if (list1 == list2)
    {
      LSwapInside(list1, node1, node2);
    }
  else if (node1 && node2)
    {
      lnode_t * swap;

      if (node1->prev)
        {
          node1->prev->next = node2;
        }
      else
        {
          list1->first = node2;
        }

      if (node1->next)
        {
          node1->next->prev = node2;
        }
      else
        {
          list1->last = node2;
        }

      if (node2->prev)
        {
          node2->prev->next = node1;
        }
      else
        {
          list2->first = node1;
        }

      if (node2->next)
        {
          node2->next->prev = node1;
        }
      else
        {
          list2->last = node1;
        }

      swap        = node1->next;
      node1->next = node2->next;
      node2->next = swap;

      swap        = node1->prev;
      node1->prev = node2->prev;
      node2->prev = swap;
    }
}


/*
 *  Swap all nodes between linked lists.
 *
 */
void LSwapAll( llist_t * list1, llist_t * list2 )
{
  lnode_t * swap;
  unsigned count;

  swap         = list1->first;
  list1->first = list2->first;
  list2->first = swap;

  swap         = list1->last;
  list1->last  = list2->last;
  list2->last  = swap;

  count        = list1->count;
  list1->count = list2->count;
  list2->count = count;
}


/*
 *  Splits the list by given node being first of the new list.
 *  Allocates a new list object.
 */
llist_t * LSplit( llist_t * list, lnode_t * node )
{
  llist_t * newlist = LInit( list->node_size, list->clear_func, list->memorypool );
  int       index   = LGetIndex(list, node);

  if (index > -1)
    {
      unsigned movecount  = list->count - index;

      newlist->count = movecount;
      newlist->first = node;
      newlist->last  = list->last;

      if (node->prev)
        { 
          node->prev->next = NULL;
          list->last = node->prev;
        }
      else
        {
          list->first = NULL;
          list->last  = NULL;
        }

      node->prev   = NULL;
      list->count -= movecount;
    }

  return newlist;
}


/*
 *  Joins nodes of latter list at the end of the first list.
 *  Dealloctes the latter list object.
 */
void LJoin( llist_t * list, llist_t ** other )
{
  if (other)
    {
      if ((*other)->first)
        {
          if (list->last)
            {
              list->last->next = (*other)->first;
            }
          else
            {
              list->first = (*other)->first;
            }

          (*other)->first->prev = list->last;
          list->last   = (*other)->last;
          list->count += (*other)->count;
        }

      os_block_dealloc(*other);
      *other = NULL;
    }
}


/*
 *  Clones the list with given clone callback function or 
 *  based on known node size if callback function is NULL.
 */
void LFilterClone( llist_t * list, llist_t ** other, NodeClone_f NodeClone, unsigned user_data )
{
  llist_t * clone = NULL;
  lnode_t * loop  = list->first;
  lnode_t * node  = NULL;

  /*
   *  Use exiting list object or create a new one.
   */
  if (*other)
    {
      clone = *other;
    }
  else
    {
      clone  = LInit(list->node_size, list->clear_func, list->memorypool);
      *other = clone;
    }

  if (list->count)
    {
      if (NodeClone)
        {
          /*
           *  If node sizes vary - created with LAlloc(any_size != node_size)
           *  or nodes possess any other allocated data NodeClone function 
           *  will be called to do node cloning.
           */
          while(loop)
            {
              /*
               *  We allow NodeClone to filter out nodes, by returning NULL.
               */
              node = NodeClone((const lnode_t*)loop, user_data);

              if (node) 
                {
                  node->next = NULL; /* safety */
                  node->prev = NULL;
                  LAttachLast(clone, node);
                }

              loop = loop->next;
            }
        }
      else
        {
          /*
           *  Auto-clones all the nodes.
           */
          while(loop)
            {
              lnode_t * node = _LAllocate(list);

              if (node) 
                {
                  memcpy( node, loop, list->node_size );
                  node->next = NULL;
                  node->prev = NULL;
                  LAttachLast(clone, node);
                }

              loop = loop->next;
            }
        }
    }
}


/*
 *  Moves the nodes to other list based on filter function given.
 *
 */
void LFilterMove( llist_t * list, llist_t ** other, NodeFilter_f NodeFilter, unsigned user_data )
{
  llist_t * clone = NULL;
  lnode_t * loop  = LFirst(list);
  lnode_t * node;

  /*
   *  Use exiting list object or create a new one.
   */
  if (*other)
    {
      clone = *other;
    }
  else
    {
      clone = LInit(list->node_size, list->clear_func, list->memorypool);
      *other = clone;
    }

  if (NodeFilter)
    {
      while(loop)
        {
          node = loop;
          loop = LNext(loop);

          if (NodeFilter(node, user_data) != LLIST_NO)
            {
              LDetach(list, node);
              LAttachLast(clone, node);
            }
        }
    }
}


/*
 *  Count number of nodes match to filter.
 */
unsigned LFilterCount( llist_t * list, NodeFilter_f NodeFilter, unsigned user_data)
{
  lnode_t * node = LFirst(list);
  unsigned count = LCount(list);
  unsigned match = 0;
  
  if (NodeFilter)
    {
      /*
       *  Use unlooping to minimize while loop.
       */
      while(count > 10)
        {
          match += NodeFilter(node, user_data);
          node = LNext(node);
          match += NodeFilter(node, user_data);
          node = LNext(node);
          match += NodeFilter(node, user_data);
          node = LNext(node);
          match += NodeFilter(node, user_data);
          node = LNext(node);
          match += NodeFilter(node, user_data);
          node = LNext(node);
          match += NodeFilter(node, user_data);
          node = LNext(node);
          match += NodeFilter(node, user_data);
          node = LNext(node);
          match += NodeFilter(node, user_data);
          node = LNext(node);
          match += NodeFilter(node, user_data);
          node = LNext(node);
          match += NodeFilter(node, user_data);
          node = LNext(node);
          count -= 10;
        }

      while(node)
        {
          match += NodeFilter(node, user_data);
          node = LNext(node);
        }
    }
  else
    {
      match = count;
    }

  return match;
}


/*
 *
 *
 */
unsigned LFilterRemove( llist_t * list, NodeFilter_f NodeFilter, unsigned user_data )
{
  lnode_t * node = LFirst(list);
  unsigned match = 0;

  if (NodeFilter)
    {
      while(node)
        {
          if (NodeFilter(node, user_data))
            {
              lnode_t * remove = node;
              node = LNext(node);
              LRemove(list, remove);
              match++;
            }
          else
            {
              node = LNext(node);
            }
        }
    }

  return match;
}


/*
 *
 *
 */
unsigned LFilterOperate( llist_t * list, NodeFilter_f NodeFilter,  unsigned user_data_filter,
                                         NodeFilter_f NodeOperate, unsigned user_data_operate)
{
  lnode_t * node = LFirst(list);
  unsigned match = 0;

  if (NodeOperate)
    {
      if (NodeFilter)
        {
          while(node)
            {
              if (NodeFilter(node, user_data_filter))
                {
                  lnode_t * operate = node;
                  node = LNext(node);
                  (void)NodeOperate(operate, user_data_operate);
                  match++;
                }
              else
                {
                  node = LNext(node);
                }
            }
        }
      else
        {
          while(node)
            {
              lnode_t * operate = node;
              node = LNext(node);
              (void)NodeOperate(operate, user_data_operate);
              match++;
            }
        }
    }

  return match;
}


/*
 *  Count each list 1 node value found from list 2.
 *
 */
static unsigned LCompareValues( llist_t * list1, llist_t * list2, NodeCmp_f NodeCmp )
{
  unsigned  match = 0;

  /*
   *  Following lists have match in values:
   *  [1,5,3,3,1]  ==  [1,2,3,4,5,6]
   */
  LInitFor(lnode_t *, node1, list1)
    {
      LInitFor(lnode_t *, node2, list2)
        {
          if (NodeCmp(node1, node2) == LNODECMP_EQUAL)
            {
              match++;
              break;
            }
        }
    }

  return match;
}


/*
 *  Count list1 nodes found in list 2.
 *
 */
static unsigned LCompareNonOrder( llist_t * list1, llist_t * list2, NodeCmp_f NodeCmp )
{
  lnode_t * node1, * node2;
  lnode_t * prev  = NULL;
  unsigned  match = 0;

  /*
   *  Use prev linking field as a marker if match for this node
   *  already found to eliminate counting one node more than once
   *  in case of more than one node shares the same value.
   *
   *  Example following lists are not match, eventhough contains
   *  all the same values: [1,2,2,3,4,5] != [1,2,3,4,4,5]
   */
  #define MARK_USED( node ) node->prev = NULL
  #define MARK_FREE( node ) node->prev = node
  #define CHECK_MARKER( node ) node->prev != NULL

  MARK_FREE(LFirst(list2));

  LFor(lnode_t *, node1, list1)
    {
      LFor(lnode_t *, node2, list2)
        {
          if (CHECK_MARKER(node2))
            {
              if (NodeCmp(node1, node2) == LNODECMP_EQUAL)
                {
                  MARK_USED(node2);
                  match++;
                }
            }
        }
    }

  /*
   *  Fix the prev linking of linked list.
   */
  LFor(lnode_t *, node2, list2)
    {
      node2->prev = prev;
      prev = node2;
    }

  return match;
}


/*
 *  Compare each list1 node to list2 in same position.
 *
 */
static unsigned LCompareInOrder( lnode_t * node1, lnode_t * node2, NodeCmp_f NodeCmp, lloop_e direction )
{
  unsigned match = 0;

  while(node1 && node2)
    {
      if (NodeCmp(node1, node2) != LNODECMP_EQUAL)
        {
          break;
        }

      node1 = node1->next;
      node2 = LLoopNext(node2, direction);
      match++;
    }

  return match;
}


/*
 *  Compares two linked lists and returns:
 *  - LLISTCMP_MATCH_INORDER    - List 1 match with list2 and in same order.
 *  - LLISTCMP_MATCH_REVERSE    - List 1 match with list2 and in reverse order.
 *  - LLISTCMP_MATCH_NONORDER   - List 1 match with list2 but in random order.
 *
 *  - LLISTCMP_MATCH_SUBSET     - List 1 match as strict  subset inside list 2.
 *  - LLISTCMP_MATCH_REVSUBSET  - List 1 match as reverse subset inside list 2.
 *  - LLISTCMP_MATCH_INCLUDED   - List 1 nodes all in list2 but in random order.
 *  - LLISTCMP_MATCH_COVERED    - List 1 node values all covered by list 2.
 *
 *  - LLISTCMP_MATCH_PARTIAL    - Only few nodes had match; considered no match.
 *  - LLISTCMP_MATCH_NOTHING    - None of the nodes match between lists.
 */
llistcmp_e LCompare( llist_t * list1, llist_t * list2, NodeCmp_f NodeCmp )
{
  if (list1->count > 0)
    {
      unsigned match  = 0;

      if (list1->count == list2->count)
        {
          /*
           *  Check if list1 nodes all in list2 in same order.
           */
          match = LCompareInOrder(list1->first, list2->first, NodeCmp, LLOOP_FORWARD);

          if (match == list1->count)
            {
              return LLISTCMP_MATCH_INORDER;
            }

          match = LCompareInOrder(list1->first, list2->last, NodeCmp, LLOOP_BACKWARD);

          if (match == list1->count)
            {
              return LLISTCMP_MATCH_REVERSE;
            }

          /*
           *  Check if list1 nodes all in list2 in any order.
           */
          match = LCompareNonOrder(list1, list2, NodeCmp);

          if (match == list1->count)
            {
              return LLISTCMP_MATCH_NONORDER;
            }
          else if (match == 0)
            {
              return LLISTCMP_MATCH_NOTHING;
            }
        }
      else if (list1->count < list2->count)
        {
          /*
           *  Limit the search of subset based on its length.
           */
          unsigned count = list2->count - list1->count;
          lnode_t * node = list2->first;

          while(count--)
            {
              /*
               *  Check if list1 found in list2 in current positions.
               */
              match = LCompareInOrder(list1->first, node, NodeCmp, LLOOP_FORWARD);

              if (match == list1->count)
                {
                  return LLISTCMP_MATCH_SUBSET;
                }

              node = node->next;
            }

          /*
           *  Reverse subset search.
           */
          count = list2->count - list1->count;
          node  = list2->last;

          while(count--)
            {
              /*
               *  Check if list1 found in list2 in current positions.
               */
              match = LCompareInOrder(list1->first, node, NodeCmp, LLOOP_BACKWARD);

              if (match == list1->count)
                {
                  return LLISTCMP_MATCH_REVSUBSET;
                }

              node = node->prev;
            }

          /*
           *  Check if list1 nodes all in list2 in any order.
           */
          match = LCompareNonOrder(list1, list2, NodeCmp);

          if (match == list1->count)
            {
              return LLISTCMP_MATCH_INCLUDED;
            }
          else if (match == 0)
            {
              return LLISTCMP_MATCH_NOTHING;
            }
        }

      /*
       *  Check if each list 1 node value is found from list 2.
       */
      match = LCompareValues(list1, list2, NodeCmp);

      if (match == list1->count)
        {
          return LLISTCMP_MATCH_COVERED;
        }
      else if (match > 0)
        {
          return LLISTCMP_MATCH_PARTIAL;
        }
    }

  return LLISTCMP_MATCH_NOTHING;
}


/*
 *  Check if list contains the node.
 *
 */
lbool_e LContains( llist_t * list, lnode_t * node )
{
  if (LGetIndex(list, node) > -1)
    {
      return LLIST_YES;   
    }

  return LLIST_NO;
}


/*
 *  Remove duplicate nodes based on compare function.
 *
 */
void LUnique( llist_t * list, NodeCmp_f NodeCmp, lloop_e direction )
{
  if (NodeCmp)
    {
      lnode_t * loop = LLoopHead(list, direction);

      while(loop)
        {
          lnode_t * node = loop;

          if (direction == LLOOP_FORWARD)
            {
              /*
               *  Remove all duplicates proceeded the first one.
               */
              while(LNext(node))
                {
                  if (NodeCmp(loop, LNext(node)) == LNODECMP_EQUAL)
                    {
                      LRemove(list, LNext(node));
                    }
                  else
                    {
                      node = LNext(node);
                    }
                }
            }
          else
            {
              /*
               *  Remove all duplicates preceded the last one.
               */
              while(LPrev(node))
                {
                  if (NodeCmp(loop, LPrev(node)) == LNODECMP_EQUAL)
                    {
                      LRemove(list, LPrev(node));
                    }
                  else
                    {
                      node = LPrev(node);
                    }
                }
            }

          loop = LLoopNext(loop, direction);
        }
    }
}


/*
 *  Get index number for node in the list.
 *
 */
int LGetIndex( llist_t * list, lnode_t * node )
{
  lnode_t * search = list->first;

  if (search && node)
    {
      register unsigned int index = 0;

      /*
       *  Minimize checks in loop by using Sentinel -search algorithm.
       */
      list->last->next = node;

      while(search != node) 
        {
          search = search->next;
          index++;
        }

      list->last->next = NULL;

      if (index < list->count)
        {
          return (int)index;
        }
    }

  /* not found */
  return -1;
}


/*
 *  Get node from the list with certain index.
 *
 */
lnode_t * LGetNode( llist_t * list, int index )
{
  lnode_t * search = NULL;

  if (index < (int)list->count)
    {
      /*
       *  Minimize loop by checking if index is closer to begin or end.
       */
      if (index < (int)(list->count / 2))
        {
          search = list->first;
          while(--index >= 0) 
            {
              search = search->next;
            }
        }
      else
        {
          index  = list->count - index;
          search = list->last;
          while(--index)
            {
              search = search->prev;
            }
        }
    }

  return search;
}


/*
 *  Set new index (change the placement) for node.
 *
 */
int LSetIndex( llist_t * list, lnode_t * node, int index )
{
  if (node)
    {
      lnode_t * current = LGetNode(list, index);

      if (current)
        {
          LMoveBefore(list, node, current);
          return index;
        }
      else
        {
          /*
           *  If given index out of range, add node as last.
           */
          LMoveLast(list, node);
          return LLastIndex(list);
        }
    }

  /* no node */
  return -1;
}


/*
 *  Return the last index.
 *
 */
int LLastIndex( llist_t * list )
{
  return (int)(list->count - 1);
}


/*
 *  Find node starting from start_node which match based by given filter function.
 *
 */
lnode_t * LFindNode( lnode_t * start_node, NodeFilter_f NodeFilter, unsigned user_data, lloop_e direction )
{
  while(start_node)
    {
      if (NodeFilter(start_node, user_data) == LLIST_YES)
        { 
          /*
           *  Return the node, which content matches.
           */
          return start_node;
        }

      start_node = LLoopNext(start_node, direction);
    }

  /* no match */
  return NULL;
}


/*
 *  Find node starting from start_node which match with content of match_node.
 *
 */
lnode_t * LFindPair( lnode_t * start_node, NodeCmp_f NodeCmp, lnode_t * match_node, lloop_e direction )
{
  if (match_node)
    {
      while(start_node)
        {
          if (NodeCmp(start_node, match_node) == LNODECMP_EQUAL)
            {
              /*
               *  Return the node, which content matches.
               */
              return start_node;
            }

          start_node = LLoopNext(start_node, direction);
        }
    }

  /* no match */
  return NULL;
}


/*
 *  Find node starting from start_node, that has member variable with given value.
 *
 *  (This functionality may be more convinient to be written to application side, thus
 *   allowing one to compare several structure members and use range checking)
 *
 *  NOTE: Currently not introduced in header file as public function! (Will be removed?)
 */
static lnode_t * LFindValue( lnode_t * start_node, unsigned offset, signed int value, unsigned size, lloop_e direction )
{
  void * mem = &value;

  while(start_node)
    {
      char * member = ((char*)(start_node) + offset);

      if (memcmp(member, mem, size) == 0)
        {
          /*
           *  Return the node, which content matches.
           */
          return start_node;
        }

      start_node = LLoopNext(start_node, direction);
    }

  /* no match */
  return NULL;
}


/*
 *  Find node starting from start_node, that has member variable pointing to equal data.
 *
 *  (This functionality may be more convinient to be written to application side, thus
 *   allowing one to compare several structure members and use range checking)
 *
 *  NOTE: Currently not introduced in header file as public function! (Will be removed?)
 */
static lnode_t * LFindData( lnode_t * start_node, unsigned offset, const char * data, unsigned size, lloop_e direction )
{
  if (data)
    {
      if (size == 0)
        {
          size = (unsigned)strlen(data)+1;
        }

      while(start_node)
        {
          char * member = (*(char**)((char*)(start_node) + offset));

          if (member && !memcmp(member, data, size))
            {
              /*
               *  Return the node, which points to equal data.
               */
              return start_node;
            }

          start_node = LLoopNext(start_node, direction);
        }
    }
  else
    {
      while(start_node)
        {
          char * member = (*(char**)((char*)(start_node) + offset));

          if (member == NULL)
            {
              /*
               *  Return the node, which points to NULL data.
               */
              return start_node;
            }

          start_node = LLoopNext(start_node, direction);
        }
    }

  /* no match */
  return NULL;
}


/*
 *  Shuffle node order with given randomizer (e.g. rand()),
 *  or internal "quick and dirty" method if NULL is given.
 */
void LShuffle( llist_t * list, int (*randomizer)(void) )
{
  if (LCount(list) > 1)
    {
      lnode_t * node1 = LFirst(list);
      lnode_t * node2 = LFirst(list);

      static int random = 0;
      int loop = LCount(list) * 2;

      random += (randomizer ? randomizer() : (int)list);
      node2   = LGetNode(list, (int)(((unsigned)random) % LCount(list)));

      while(--loop)
        {
          /*
           *  Next to be swapped is the next of previously randomized.
           *  (Optimization to prevent randomizing and fecthing both nodes)
           */
          if (node2->next)
            {
              node1 = node2->next;
            }
          else
            {
              node1 = list->first;
            }

          /*
           *  Another node to be swapped is randomized.
           */
          random += (randomizer ? randomizer() : random | (int)node1 + (int)node2);
          node2   = LGetNode(list, (int)(((unsigned)random) % LCount(list)));

          LSwapInside(list, node1, node2);
        }
    }
}


/*
 *  Checks if the linked list is already sorted with given compare function.
 *
 */
lbool_e LVerify(  llist_t * list, NodeCmp_f NodeCmp )
{
  unsigned result = LLIST_YES; /* List in order */

  if (LCount(list) > 1)
    {
      lnode_t* node = list->first;

      /*
       *  New (unsorted) nodes are most often added into end of the list,
       *  thus this loops from end to beginning. Except checking the first
       *  node first just to avoid worst case scenario more often.
       */
      if (NodeCmp(node, node->next) > LNODECMP_EQUAL)
        {
          result = LLIST_NO;
        }
      else if (LCount(list) > 2)
        {
          lnode_t * head = node;

          /*
           *  First one was checked already, so remove it temporarely.
           */
          head->next->prev = NULL;

          /*
           *  Continue from the end of the list.
           */
          node = list->last->prev;
    
          while(node)
            {
              if (NodeCmp(node, node->next) > LNODECMP_EQUAL)
                {
                  result = LLIST_NO;
                  break;
                }
    
              node = node->prev;
            }

          /*
           *  Correct the head of the list.
           */
          head->next->prev = head;
        }
    }

  return result;
}


/*
 *  Sorts the list according to given compare function.
 *
 */
void LSort( llist_t * list, NodeCmp_f NodeCmp )
{
  if (NodeCmp && LCount(list) > 1)
    {
      llist_t tlist = {NULL, NULL, NULL, 0, 0};
      llist_t blist = {NULL, NULL, NULL, 0, 0};
      lnode_t* highnode = LFirst(list);
      lnode_t* lownode  = LFirst(list);
      lnode_t* node     = LFirst(list);
      unsigned count    = LCount(list);

      /*
       *  Optimization: add sentinel node to speed up
       *  attach operations to top and bottomlists.
       */
      lnode_t sentinel  = {NULL, NULL};
      tlist.last  = &sentinel;
      tlist.first = &sentinel;
      blist.first = &sentinel;
      blist.last  = &sentinel;

      /*
       *  Sort linked list with double insertion sort.
       */
      while(node)
        {
          node = node->next;
          /*
           *  Loop the (remaining) list through and
           *  find lowest and highest nodes.
           */
          while(node)
            {
              if (NodeCmp(node, highnode) >= LNODECMP_EQUAL)
                {
                  highnode = node;
                }
              else if (NodeCmp(node, lownode) < LNODECMP_EQUAL)
                {
                  lownode = node;
                }

              node = node->next;
            }

          /*
           *  Detach highest node from linked list and attach as
           *  first of botton list    [ "7" -> ...,8,9,BOTTOM]
           */
          LDetach(list, highnode);

          /*
           *  Inline solution of LAttachFirst( blist, highnode );
           *  (last)->next & prev<-(first)
           */
          highnode->next = blist.first;
          blist.first->prev = highnode;
          blist.first = highnode;

          if (highnode != lownode)
            {
              /*
               *  Detach lowest node from linked list and attach as
               *  last of top list    [TOP,1,2,...  <- "3" ]
               */
              LDetach(list, lownode);

              /*
               *  Inline solution of LAttachLast( tlist, lownode );
               *  (last)->next & prev<-(node)
               */
              lownode->prev = tlist.last;
              tlist.last->next = lownode;
              tlist.last = lownode;
            }

          node     = list->first;
          highnode = node;
          lownode  = node;
        }

      /*
       *  Remove the sentinel node references.
       */
      tlist.first = tlist.first->next;
      tlist.first->prev = NULL;
      blist.last  = blist.last->prev;
      blist.last->next  = NULL;

      /*
       *  Finally join top and bottom back to orginal list.
       *  LIST = [TOP,1,2,3,4,...] + [...,5,6,7,8,9,BOTTOM]
       */
      LAttachLast(list, tlist.first);
      LAttachLast(list, blist.first);
    }
}


/*
 * Sorting method used is demonstrated below:
 *
 * [ 5, 6A, 2A, 7, 9, 6B, 2B, 6C, 8, 3, 1, 4, 5] <- orginal
 *
 *                                                        top-stack       botton-stack
 * [ 5, 6A, 2A, 7,*9, 6B, 2B, 6C, 8, 3,*1, 4, 5] ->  [1,..            +               ..,9]
 * [ 5, 6A,*2A, 7,    6B, 2B, 6C,*8, 3,    4, 5] ->  [1,2A,..         +             ..,8,9]
 * [ 5, 6A,   *7,     6B,*2B, 6C,    3,    4, 5] ->  [1,2A,2B,..      +           ..,7,8,9]
 * [ 5, 6A,           6B,    *6C,   *3,    4, 5] ->  [1,2A,2B,3,..    +        ..,6C,7,8,9]
 * [ 5, 6A           *6B,                 *4, 5] ->  [1,2A,2B,3,4,..  +     ..,6B,6C,7,8,9]
 * [*5,*6A                                    5] ->  [1,2A,2B,3,4,5,..+  ..,6A,6B,6C,7,8,9]
 * [                                         *5] ->  [1,2A,2B,3,4,5,..+..,5,6A,6B,6C,7,8,9]
 *
 * [ 1, 2A, 2B, 3, 4, 5, 5, 6A, 6B, 6C, 7, 8, 9] <- sorted
 *
 * - 6A, 6B, 6C are all equal, and they remain their orginal order in botton-stack, as well
 *   2A and 2B are equal and remain their order in top-stack, thus algorithm is stable.
 * - worst case scenario is if list is in reverse order, best case if already in order.
 *
 */


/*
 *  Rearrange linked list in reverse order.
 *
 */
void LReverse( llist_t * list )
{
  if (LCount(list) > 1)
    {
      lnode_t * next = list->first->next;
      lnode_t * node = list->first->next;
      list->last     = list->first;

      while(node)
        {
          next = node->next;

          node->next = list->first;
          list->first->prev = node;
          list->first = node;

          node = next;
        }

      list->first->prev = NULL;
      list->last->next  = NULL;
    }
}


/*
 * Internal typacast for expanded nodes.
 *   
 */
typedef struct 
  { 
    LLIST_NODE
    unsigned offset;
  } lnode_offset_t;


/*
 *  Calculate new size and offset to add lnode_t fields at the end.
 *
 */
unsigned LExpandedSize( unsigned * size )
{
  /*
   *  Correct alingment by 4 (just in case, since sizeof is
   *  always aligned by 4 in C) and increase the node size.
   */
  unsigned offset = *size;

  if ((*size % 4) != 0)
    {
      offset = *size + 4 - (*size % 4);
    }

  *size = offset +  sizeof(lnode_offset_t);

  return offset;
}


/*
 *  Typecast object to lnode with given offset.
 *
 */
lnode_t * LCastNode( void * object, unsigned offset )
{
  lnode_offset_t * offset_node =
      (lnode_offset_t*)&(*(char**)((char*)(object) + offset));

  offset_node->next   = NULL;
  offset_node->prev   = NULL;
  offset_node->offset = offset;

  return (lnode_t*)offset_node;
}


/*
 *  Typecast node back to object by stored offset.
 *
 */
void * LCastObject( lnode_t* node )
{
  return &(*(char**)((char*)(node) - ((lnode_offset_t*)node)->offset));
}


/*
 *  Deallocate Expanded.
 *
 */
lnode_t * LExpandedDealloc( lnode_t * node )
{
  void * object = LCastObject( node );
  os_block_dealloc(object);
  return NULL;
}


/* --------------------------------------------------------------- */

#if defined LLIST_UNITTEST || defined MPOOL_UNITTEST

/*
 *  gcc -fprofile-arcs -ftest-coverage -DLLIST_UNITTEST -o llist.exe llist.c
 *
 *  Test coverage (100%):
 *  ./llist.exe
 *  gcov llist.c
 *
 *  Memory leaks (0 bytes):
 *  valgrind ./llist.exe
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

static unsigned os_block_alloc_occupied = LLIST_NO;

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


#elif defined LLIST_UNITTEST

/*
 *  Test structure
 *
 */
typedef struct
{
  LLIST_NODE
  int    id;
  int    value;
  char * string;
} test_record_t;


static void unittest_show(char * name, llist_t * list)
{
  if (list)
    {
      test_record_t * record = NULL;
      lnode_t * node = LFirst(list);
      lnode_t * prev = NULL;
      unsigned count = 0;

      printf("%s List, with %d nodes:\n", name, LCount(list));

      if (!node) printf("no nodes\n"); else
        {
          assert(!LPrev(LFirst(list)));     /* 1. verify head of list */
          assert(!LNext(LLast(list)));      /* 2. verify tail of list */

          while(node)
            {
              assert(LPrev(node) == prev);  /* 3. verify backward linking ... */
              prev   = node;
              record = (test_record_t*)node;
              node   = LNext(node);         /* 4. ... while using forward linking */
              count++;

              printf("element id=%d, value=%d\n", record->id, record->value);
            }
        }
      assert(LCount(list) == count);        /* 5. verify list count */
    }
  else
    {
      printf("no list\n");
    }

  printf("-------------\n");
}

static signed int unittest_compare(const lnode_t* node1, const lnode_t* node2)
{
  return (signed int)(((test_record_t*)node1)->value - ((test_record_t*)node2)->value);
}

static lbool_e unittest_filter(const lnode_t* node, unsigned user_data)
{
  return (((test_record_t*)node)->value > (int)user_data ? LLIST_YES : LLIST_NO);
}

static lbool_e unittest_operate(const lnode_t* node, unsigned user_data)
{
  printf("Operating node %d by increasing value %d by %d\n", 
    ((test_record_t*)node)->id, ((test_record_t*)node)->value, user_data);

  ((test_record_t*)node)->value += user_data;
  return LLIST_YES; /* meaningless */
}

static lnode_t* unittest_clone(const lnode_t* node, unsigned user_data)
{
  test_record_t* newrecord = (test_record_t*)os_block_alloc(sizeof(test_record_t));
  newrecord->id    = ((test_record_t*)node)->id;
  newrecord->value = ((test_record_t*)node)->value + user_data;
  return (lnode_t*)newrecord;
}

static lnode_t * unittest_free(lnode_t* node)
{
  printf("free any allocated data node %d might possess.\n",
      ((test_record_t*)node)->id);
  return node;
}

static llist_t * unittest_generate_list( unsigned count, ... )
{
  llist_t * list = LInit(sizeof(test_record_t), NULL, NULL);
  test_record_t * record;
  va_list valuelist;
  unsigned i = 0;

  va_start(valuelist, count);

  while(i < count)
    {
      record = (test_record_t*)LCreateLast(list);
      record->id = ++i;
      record->value = va_arg(valuelist, int);
    }

  va_end(valuelist);

  return list;
}

static void unittest_dispose_all( llist_t * list, ... )
{
  va_list lists;
  va_start(lists, list);

  while(list != NULL) 
    {
      LDispose(&list);
      list = va_arg(lists, llist_t *);
    }

  va_end(lists);
}

/* ------ Testset 1 - create nodes ------ */

/* Output:
 *
 *  List, with 4 nodes:
 *  element id = 3, value = 333
 *  element id = 4, value = 4444
 *  element id = 1, value = 1
 *  element id = 2, value = 22
 *
 */
static void unittest_testset1( void )
{
  llist_t * list  = LInit( sizeof(test_record_t), NULL, NULL );
  llist_t * list2 = LInit( sizeof(test_record_t), unittest_free, NULL );
  test_record_t * record = NULL;
  lnode_t * node;

  printf("\nTestset 1 - create nodes.\n\n");

  record = (test_record_t*)LCreateFirst(list);
  assert(list->count == 1);
  assert(!list->last->next);
  assert(!list->first->prev);
  LRemove(list, (lnode_t*)record);
  assert(list->count == 0);

  record = (test_record_t*)LCreateLast(list);
  record->id = 1;
  record->value = 1;

  record = (test_record_t*)LCreateLast(list);
  record->id = 2;
  record->value = 22;

  record = (test_record_t*)LCreateFirst(list);
  record->id = 3;
  record->value = 333;

  node = LCreateBefore(list, LFirst(list));
  LRemove(list, node);

  node = LCreateBefore(list, LLast(list));
  LRemove(list, node);

  node = LCreateAfter(list, LLast(list));
  LRemove(list, node);

  node = LCreateAfter(list, LFirst(list));

  ((test_record_t*)node)->id = 4;
  ((test_record_t*)node)->value = 4444;

  assert(list->count == 4); /* 4 nodes in the list */
  unittest_show("", list);

  LRemove(list, LNext(LFirst(list)));
  assert(list->count == 3);

  LRemoveLast(list);
  assert(list->count == 2);

  LRemoveFirst(list);
  assert(list->count == 1);

  LRemoveFirst(list);
  assert(list->count == 0);

  LRemoveFirst(list);
  assert(list->count == 0);

  LDispose(&list);
  assert(!list);
  LDispose(&list);

  record = (test_record_t*)LCreateFirst(list2);
  record->id = 101;

  record = (test_record_t*)LCreateFirst(list2);
  record->id = 102;

  LRemoveFirst(list2);
  LDispose(&list2);
  assert(!list2);
  LDispose(&list2);
}

/* ------ Testset 2 - moving nodes ------ */
static void unittest_testset2( void )
{
  llist_t * list  = unittest_generate_list( 4,   1, 2, 3, 4);
  lnode_t * node;

  printf("\nTestset 2 - detach/attach.\n\n");

  node = LFirst(list);
  LMoveFirst(list, LLast(list));
  assert(LFirst(list) != node);
  assert(LPrev(node) == LFirst(list));

  node = LLast(list);
  LMoveLast(list, LNext(LFirst(list)));
  assert(LLast(list) != node);
  assert(LNext(node) == LLast(list));

  LMoveFirst(list, LFirst(list));
  LMoveLast(list, LLast(list));

  node = LNext(LFirst(list));
  assert(LPrev(node) == LFirst(list));
  LMoveAfter(list, LFirst(list), LNext(LFirst(list)));
  assert(LPrev(node) != LFirst(list));

  node = LPrev(LLast(list));
  assert(LNext(node) == LLast(list));
  LMoveBefore(list, LLast(list), LPrev(LLast(list)));
  assert(LNext(node) != LLast(list));

  LMoveAfter(list, LFirst(list), LLast(list));
  LMoveBefore(list, LLast(list), LFirst(list));

  LMoveAfter(list, LFirst(list), LFirst(list));
  LMoveBefore(list, LLast(list), LLast(list));

  unittest_show("reordered", list);
  unittest_dispose_all(list, NULL);
}


/* ------ Testset 3 - detach/attach ------ */
static void unittest_testset3( void )
{
  llist_t * list;
  lnode_t * node;
  lnode_t * node1;
  lnode_t * node2;

  printf("\nTestset 3 - detach/attach.\n\n");

  list  = unittest_generate_list( 4,   22, 4444, 1, 333);
  node1 = LDetachFirst(list);
  node2 = LDetachLast(list);

  assert(node1);
  assert(node2);
  assert(list->count == 2); /* 2 nodes in the list */

  LAttachFirst(list, node2);
  LAttachLast(list, node1);

  assert(list->count == 4); /* 4 node in the list */
  unittest_show("", list);

  node1 = LFirst(list);
  LDetachMany(list, node1, 3, LLOOP_FORWARD);
  assert(LDetachCount(node1, LLOOP_FORWARD) == 3);

  assert(list->count == 1); /* 1 node in the list */
  unittest_show("", list);

  node2 = LFirst(list);
  LDetachMany(list, node2, 0, LLOOP_FORWARD); /* detach rest */
  assert(LDetachCount(node2, LLOOP_FORWARD) == 1);

  LAttachLast(list, node2);
  node2 = LDetachFirst(list);
  LAttachLast(list, node2);
  node2 = LDetachLast(list);
  LAttachLast(list, node2);

  LAttachLast(list, node1);
  assert(list->count == 4); /* 4 nodes in the list */
  unittest_show("", list);

  node = LNext(LFirst(list));
  LDetachMany(list, node, 2, LLOOP_FORWARD);
  assert(LNext(LFirst(list)) == LLast(list));
  assert(LPrev(LLast(list)) == LFirst(list));
  LAttachAfter(list, node, LFirst(list));

  node = LNext(LFirst(list));
  LDetachMany(list, LNext(node), 2, LLOOP_BACKWARD);
  LAttachBefore(list, node, LLast(list));

  node = LDetachAll(list);
  assert(LCount(list) == 0);
  assert(LDetachCount(node, LLOOP_FORWARD) == 4);
  assert(LDetachCount(LNext(LNext(node)), LLOOP_BACKWARD) == 3);
  LAttachBefore(list, node, LFirst(list));
  assert(LCount(list) == 4); /* 4 nodes in the list */

  unittest_show("", list);
  unittest_dispose_all(list, NULL);
}


/* ------ Testset 4 - detach/attach ------ */
static void unittest_testset4( void )
{
  test_record_t * record = NULL;
  llist_t * list  = unittest_generate_list( 4,   22, 4444, 1, 333);
  lnode_t * node;

  printf("\nTestset 4 - alloc/dealloc.\n\n");

  unittest_show("", list);

  node = LFirst(list);
  LDetachMany(list, node, 2, LLOOP_FORWARD);
  LDealloc(LNext(node), NULL, NULL);

  unittest_show("", list);

  record = (test_record_t*)LAlloc( sizeof(test_record_t), LLIST_YES);
  record->id = 2;
  record->value = 22;

  LAttachFirst(list, (lnode_t*)record);

  record = (test_record_t*)LAlloc( sizeof(test_record_t), LLIST_NO);
  record->id = 3;
  record->value = 333;

  LAttachFirst(list, (lnode_t*)record);

  unittest_show("", list);
  unittest_dispose_all(list, NULL);
}


/* ------ Testset 5 - indecis ------ */
static void unittest_testset5( void )
{
  llist_t * list  = unittest_generate_list( 4,   22, 4444, 1, 333);
  lnode_t * node;
  int index;

  printf("\nTestset 5 - indecis.\n\n");

  node = LGetNode(list, 0);
  assert(node == LFirst(list));

  node = LGetNode(list, -100);
  assert(node);

  node = LGetNode(list, LCount(list)-2);
  assert(node);
  assert(((test_record_t*)node)->id == 3);

  index = LSetIndex(list, NULL,  0);
  assert(index == -1);

  node = LGetNode(list, -100);
  assert(node);

  index = LSetIndex(list, node, 100);
  assert(index == LLastIndex(list));

  (void)LSetIndex(list, node, 1);
  index = LGetIndex(list, node);
  assert(index == 1);

  unittest_show("", list);
  unittest_dispose_all(list, NULL);
}


/* ------ Testset 6 - split/join ------ */
static void unittest_testset6( void )
{
  llist_t * list   = unittest_generate_list( 4,   1, 22, 333, 4444);
  llist_t * c1list = NULL;

  printf("\nTestset 6 - split/join.\n\n");

  c1list = LSplit(list, LNext(LFirst(list)));
  unittest_show("orginal", list);
  unittest_show("splitted", c1list);

  LJoin(list, &c1list);
  unittest_show("orginal", list);
  unittest_show("splitted", c1list);

  c1list = LSplit(list, LFirst(list));
  unittest_show("orginal", list);
  unittest_show("splitted", c1list);

  LJoin(list, &c1list);
  unittest_show("orginal", list);
  unittest_show("splitted", c1list);

  assert(list->count == 4); /* 4 nodes in the list */
  unittest_dispose_all(list, c1list, NULL);
}


/* ------ Testset 7 - swapping inside ------ */
static void unittest_testset7( void )
{
  llist_t * list = unittest_generate_list( 4,   1, 22, 333, 4444);

  printf("\nTestset 7 - swapping inside.\n\n");

  LSwapInside(list, LFirst(list), LFirst(list));

  unittest_show("", list);

  LSwapInside(list, LNext(LFirst(list)), LLast(list));
  LSwapInside(list, LPrev(LLast(list)),  LFirst(list));

  LSwapInside(list, LFirst(list), LLast(list));

  LSwapInside(list, LLast(list),  LNext(LFirst(list)));
  LSwapInside(list, LFirst(list), LPrev(LLast(list)));

  unittest_show("", list);

  LSwapInside(list, LNext(LFirst(list)), LFirst(list));
  LSwapInside(list, LPrev(LLast(list)),  LLast(list));

  LSwapInside(list, LFirst(list), LNext(LFirst(list)));

  LSwapInside(list, LFirst(list), LNext(LFirst(list)));
  LSwapInside(list, LLast(list),  LPrev(LLast(list)));

  unittest_show("", list);
  unittest_dispose_all(list, NULL);
}


/* ------ Testset 8 - filtering ------ */
static void unittest_testset8( void )
{
  llist_t * list    = unittest_generate_list( 4,   1, 22, 333, 4444);
  llist_t * list2   = unittest_generate_list( 11,  10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110);
  llist_t * unique1 = unittest_generate_list( 7,   2, 2, 2, 4, 5, 5, 5);
  llist_t * unique2 = unittest_generate_list( 7,   2, 2, 2, 4, 5, 5, 5);
  llist_t * c1list  = NULL;
  llist_t * c2list  = NULL;

  printf("\nTestset 8 - clone.\n\n");

  LFilterClone(list, &c1list, NULL, 0);
  unittest_show("orginal", list);
  unittest_show("clone1", c1list);

  LFilterClone(list, &c2list, unittest_clone, 10000);
  unittest_show("clone2", c2list);

  LRemoveAll(c2list);
  LFilterClone(list, &c2list, unittest_clone, 10000);
  unittest_show("clone2", c2list);

  LDispose(&c2list);
  LFilterMove(list, &c2list, unittest_filter, 22);
  unittest_show("filtered", c2list);

  LRemoveAll(c2list);
  LFilterMove(list, &c2list, unittest_filter, 22);
  unittest_show("filtered", c2list);

  unittest_show("filtered(count)", list2);
  assert(LFilterCount(list2, unittest_filter, 70) == 4);
  
  assert(LFilterOperate(list2, unittest_filter, 70, unittest_operate, 1000) == 4);
  unittest_show("filtered(operate)", list2);

  assert(LFilterRemove(list2, unittest_filter, 20) == 9);
  unittest_show("filtered(remove)", list2);

  unittest_show("duplicates", unique1);
  LUnique(unique1, unittest_compare, LLOOP_FORWARD);
  unittest_show("duplicates", unique1);
  assert( ((test_record_t*)LFirst(unique1))->id == 1 );
  assert( ((test_record_t*)LLast(unique1))->id  == 5 );

  unittest_show("duplicates", unique2);
  LUnique(unique2, unittest_compare, LLOOP_BACKWARD);
  unittest_show("duplicates", unique2);
  assert( ((test_record_t*)LFirst(unique2))->id == 3 );
  assert( ((test_record_t*)LLast(unique2))->id  == 7 );

  unittest_dispose_all(list, list2, c1list, c2list, NULL);
}


/* ------ Testset 9 - swapping between------ */
static void unittest_testset9( void )
{
  llist_t * c1list = unittest_generate_list( 4,   1, 22, 333, 4444);
  llist_t * c2list = unittest_generate_list( 4,   1, 22, 333, 4444);

  printf("\nTestset 9 - swapping between.\n\n");

  LSwapBetween(c1list, LFirst(c1list), c1list, LFirst(c1list));

  unittest_show("clone1", c1list);
  unittest_show("clone2", c2list);

  LSwapBetween(c1list, LFirst(c1list), c2list, LFirst(c2list));
  LSwapBetween(c1list, LLast(c1list), c2list, LLast(c2list));

  LSwapBetween(c1list, LNext(LFirst(c1list)), c2list, LPrev(LLast(c2list)));

  unittest_show("clone1", c1list);
  unittest_show("clone2", c2list);

  LSwapBetween(c1list, LFirst(c1list), c2list, LLast(c2list));
  LSwapBetween(c1list, LLast(c1list), c2list, LFirst(c2list));

  LSwapBetween(c1list, LNext(LFirst(c1list)), c2list, LPrev(LLast(c2list)));

  unittest_show("clone1", c1list);
  unittest_show("clone2", c2list);

  LSwapAll(c1list, c2list);

  unittest_show("clone1", c1list);
  unittest_show("clone2", c2list);

  LSwapAll(c1list, c2list);

  unittest_dispose_all( c1list, c2list, NULL);
}


/* ------ Testset 10 - compare ------ */
static void unittest_testset10( void )
{
  llist_t * orginal   = unittest_generate_list( 6,   1, 2, 3, 4, 5, 6);
  llist_t * inorder   = unittest_generate_list( 6,   1, 2, 3, 4, 5, 6);
  llist_t * reverse   = unittest_generate_list( 6,   6, 5, 4, 3, 2, 1);
  llist_t * nonorder  = unittest_generate_list( 6,   4, 5, 1, 2, 3, 6);
  llist_t * subset    = unittest_generate_list( 4,   2, 3, 4, 5);
  llist_t * revsubset = unittest_generate_list( 4,   5, 4, 3, 2);
  llist_t * included  = unittest_generate_list( 4,   1, 3, 4, 6);
  llist_t * covered   = unittest_generate_list( 6,   1, 1, 6, 5, 4, 5);
  llist_t * partial   = unittest_generate_list( 2,   2,  99 );
  llist_t * nothing   = unittest_generate_list( 2,   88, 99 );
  llist_t * nothing2  = unittest_generate_list( 6,   11, 22, 33, 44, 55, 66 );
  lnode_t * node;

  printf("\nTestset 10 - compare.\n\n");

  unittest_show("orginal", orginal);

  node = LFirst(orginal);
  assert(LContains(orginal, node));
  assert(!LContains(nothing, node));

  assert(LCompare(inorder,   orginal, unittest_compare) == LLISTCMP_MATCH_INORDER);
  assert(LCompare(reverse,   orginal, unittest_compare) == LLISTCMP_MATCH_REVERSE);
  assert(LCompare(nonorder,  orginal, unittest_compare) == LLISTCMP_MATCH_NONORDER);
  assert(LCompare(subset,    orginal, unittest_compare) == LLISTCMP_MATCH_SUBSET);
  assert(LCompare(orginal,   subset,  unittest_compare) != LLISTCMP_MATCH_SUBSET);
  assert(LCompare(revsubset, orginal, unittest_compare) == LLISTCMP_MATCH_REVSUBSET);
  assert(LCompare(included,  orginal, unittest_compare) == LLISTCMP_MATCH_INCLUDED);
  assert(LCompare(covered,   orginal, unittest_compare) == LLISTCMP_MATCH_COVERED);
  assert(LCompare(orginal,   covered, unittest_compare) != LLISTCMP_MATCH_COVERED);
  assert(LCompare(partial,   orginal, unittest_compare) == LLISTCMP_MATCH_PARTIAL);
  assert(LCompare(nothing,   orginal, unittest_compare) == LLISTCMP_MATCH_NOTHING);
  assert(LCompare(nothing2,  orginal, unittest_compare) == LLISTCMP_MATCH_NOTHING);

  LRemoveAll(orginal);
  assert(LCompare(nothing,  orginal, unittest_compare) == LLISTCMP_MATCH_NOTHING);
  LRemoveAll(nothing);
  assert(LCompare(nothing,  orginal, unittest_compare) == LLISTCMP_MATCH_NOTHING);

  unittest_dispose_all( orginal, reverse, inorder, nonorder, subset,
      revsubset, included, covered, partial, nothing, nothing2, NULL);
}


/* ------ Testset 11 - finding ------ */
static void unittest_testset11( void )
{
  const char* string = "unittest";
  llist_t * list   = unittest_generate_list( 6,   1, 2, 3, 4, 5, 1);
  llist_t * list2  = unittest_generate_list( 4,   3, 4, 5, 6);
  lnode_t * node;

  printf("\nTestset 11 - finding.\n\n");

  node = LFindPair( LNext(LFirst(list)), unittest_compare, LLast(list), LLOOP_FORWARD);
  assert(node != NULL);
  assert(((test_record_t*)node)->id == 6);

  node = LFindPair( LNext(LFirst(list)), unittest_compare, LLast(list), LLOOP_BACKWARD);
  assert(node != NULL);
  assert(((test_record_t*)node)->id == 1);

  node = LFindPair( LFirst(list), unittest_compare, LFirst(list2), LLOOP_FORWARD);
  assert(node != NULL);
  node = LFindPair( LFirst(list), unittest_compare, LLast(list2),  LLOOP_FORWARD);
  assert(node == NULL);

  node = LFindNode(LLast(list), unittest_filter, 9999, LLOOP_BACKWARD);
  assert(node == NULL);

  node = LFindNode(LFirst(list), unittest_filter, 1, LLOOP_FORWARD);
  assert(node == LNext(LFirst(list)));

  node = LFindValue( LFirst(list), offsetof(test_record_t, id), -3, sizeof(int), LLOOP_FORWARD);
  assert(node == NULL);

  node = LFindValue( LFirst(list), offsetof(test_record_t, id), 3, sizeof(int), LLOOP_FORWARD);
  assert(((test_record_t*)node)->id == 3);

  /* modify node 3 data field */
  ((test_record_t*)node)->string = (char*)string;

  node = LFindData( LFirst(list), offsetof(test_record_t, string), (char*)string, strlen(string)+1, LLOOP_FORWARD);
  assert(((test_record_t*)node)->id == 3);

  node = LFindData( LFirst(list), offsetof(test_record_t, string), "nonsense", 0, LLOOP_FORWARD);
  assert(node == NULL);

  node = LFindData( LFirst(list), offsetof(test_record_t, string), "unittest", 0, LLOOP_FORWARD);
  assert(((test_record_t*)node)->id == 3);

  node = LFindData( node, offsetof(test_record_t, string), NULL, 0, LLOOP_BACKWARD);
  assert(((test_record_t*)node)->id != 3);

  node = LFindData( LFirst(list), offsetof(test_record_t, string), NULL, 0, LLOOP_FORWARD);
  assert(LIsFirst(node));
  assert(!LIsLast(node));

  LRemoveAll(list2);
  assert(LIsFirst(list2->first)); /* NULL */
  assert(LIsLast(list2->first));  /* NULL */

  unittest_dispose_all(list, list2, NULL);
}

/* ------ Testset 12 - sorting ------ */
static void unittest_testset12( void )
{
  test_record_t * record1 = NULL;
  test_record_t * record2 = NULL;
  test_record_t * record3 = NULL;
  llist_t * list = unittest_generate_list( 4, 1, 22, 333, 4444);

  printf("\nTestset 12 - sorting.\n\n");

  unittest_show("unsorted", list);
  LSort(list, unittest_compare);

  unittest_show("sorted", list);
  LSort(list, unittest_compare);

  record1 = (test_record_t*)LCreateBefore(list, LLast(list));
  record1->id = 5;
  record1->value = 1;

  record1 = (test_record_t*)LCreateAfter(list, LFirst(list));
  record1->id = 6;
  record1->value = 4444;

  record1 = (test_record_t*)LCreateAfter(list, LFirst(list));
  record1->id = 7;
  record1->value = 333;
  assert(list->count == 7);

  assert(LVerify(list, unittest_compare) == LLIST_NO);
  unittest_show("unsorted", list);

  LSort(list, unittest_compare);
  unittest_show("sorted", list);

  LShuffle(list, NULL);
  unittest_show("unsorted", list);

  LReverse(list);
  LSort(list, unittest_compare);
  unittest_show("sorted", list);

  LVerify(list, unittest_compare);

  record1 = (test_record_t*)LAlloc(sizeof(test_record_t), LLIST_NO);
  record1->id = 801;
  record1->value = 22;
  LAttachSorted(list, (lnode_t*)record1, unittest_compare, LLOOP_FORWARD);
  assert(list->count == 8);

  record2 = (test_record_t*)LAlloc(sizeof(test_record_t), LLIST_NO);
  record2->id = 802;
  record2->value = 22;
  LAttachSorted(list, (lnode_t*)record2, unittest_compare, LLOOP_FORWARD);
  assert(LPrev(record1) == (lnode_t*)record2);

  record3 = (test_record_t*)LAlloc(sizeof(test_record_t), LLIST_NO);
  record3->id = 803;
  record3->value = 22;
  LAttachSorted(list, (lnode_t*)record3, unittest_compare, LLOOP_BACKWARD);
  assert(LPrev(LPrev(record3)) == (lnode_t*)record1);

  record1 = (test_record_t*)LAlloc(sizeof(test_record_t), LLIST_NO);
  record1->id = 804;
  record1->value = 0;
  LAttachSorted(list, (lnode_t*)record1, unittest_compare, LLOOP_BACKWARD);
  assert(LFirst(list) == (lnode_t*)record1);

  record1 = (test_record_t*)LAlloc(sizeof(test_record_t), LLIST_NO);
  record1->id = 805;
  record1->value = 9999;
  LAttachSorted(list, (lnode_t*)record1, unittest_compare, LLOOP_FORWARD);
  assert(LLast(list) == (lnode_t*)record1);

  LVerify(list, unittest_compare);
  unittest_show("sorted", list);

  LReverse(list);
  LVerify(list, unittest_compare);

  unittest_dispose_all(list, NULL);
}


/* ------ Testset 13 - expanded nodes ------ */
static void unittest_testset13( void )
{
  typedef struct
    {
      int id;
      int value;
      char padding;
    } any_struct_t;

  llist_t * list = LInit(0, LExpandedDealloc, NULL);
  any_struct_t * object = NULL;
  lnode_t * node = NULL;
  unsigned size;
  unsigned offset;

  printf("\nTestset 12 - expanded size.\n\n");

  size   = sizeof(any_struct_t) + /* test */ 1;
  offset = LExpandedSize( &size );
  assert( size != sizeof(any_struct_t) );

  object = (any_struct_t*)os_block_alloc_and_clear( size );
  object->id    = 1;
  object->value = 10;
  LAttachLast(list, LCastNode( (void*)object, offset ));

  object = (any_struct_t*)os_block_alloc_and_clear( size + 1);
  object->id    = 2;
  object->value = 20;
  LAttachLast(list, LCastNode( (void*)object, offset ));

  object = (any_struct_t*)os_block_alloc_and_clear( size + 2);
  object->id    = 3;
  object->value = 30;
  LAttachLast(list, LCastNode( (void*)object, offset ));

  object = (any_struct_t*)os_block_alloc_and_clear( size + 3);
  object->id    = 4;
  object->value = 40;
  LAttachLast(list, LCastNode( (void*)object, offset ));

  assert(LCount(list) == 4);
  printf("List with %d expanded nodes.\n", LCount(list)); 
  node = LFirst(list);

  while(node)
    {
      object = LCastObject(node);
      printf("object id %d value = %d\n", object->id, object->value);
      node = LNext(node);
    }

  assert(LCount(list) == 4);
  LRemoveAll(list);
  assert(LCount(list) == 0);

  unittest_dispose_all(list, NULL);
}


/*
 *  Test harness for linked list.
 *
 */
int main(void)
{
  clock_t start_time;
  clock_t end_time;

  printf("\nunittest - llist.c\n");

  start_time = clock();

  /* Testset 1 - create/remove */
  unittest_testset1();

  /* Testset 2 - moving nodes */
  unittest_testset2();

  /* Testset 3 - detach/attach */
  unittest_testset3();

  /* Testset 4 - alloc/dealloc */
  unittest_testset4();

  /* Testset 5 - indices */
  unittest_testset5();

  /* Testset 6 - split/join */
  unittest_testset6();

  /* Testset 7 - swapping inside */
  unittest_testset7();

  /* Testset 8 - filtering */
  unittest_testset8();

  /* Testset 9 - swapping between */
  unittest_testset9();

  /* Testset 10 - compare */
  unittest_testset10();

  /* Testset 11 - finding */
  unittest_testset11();

  /* Testset 12 - sorting */
  unittest_testset12();

  /* Testset 13 - expanded nodes */
  unittest_testset13();

  end_time = clock();

  printf("\nTime %d ms", end_time - start_time);

  printf("\nunittest - done.\n");
  return 0;
}

#endif /* UNITTEST_MAIN */
