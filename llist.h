/*
 *  General Linked List Header
 *
 *  18-Nov-2008  Marko Kallinki  Support for build-in memory manager
 *  16-Sep-2008  Marko Kallinki  Major library upgrade
 *  04-Jun-2007  Marko Kallinki  Improvements
 *  03-Feb-2007  Marko Kallinki  Initial version
 *
 */

#ifdef __cplusplus
extern "C"
{
#endif


#ifndef LLIST_H
#define LLIST_H


/* --------------------------------------------------------------- */

/*
 *  General (double) linked list library.
 *
 *  Uses list main object to make node operations simple and efficient.
 *  Nodes can also exists and be handled outside the list object.
 *
 *  None of the functions checks if e.g. 'list' parameter is not NULL.
 *  It makes no sense in any context, so library user should understand
 *  and fix the calling code rather than this library silently ignore
 *  those cases by making "zillions" of unnecessary NULL and sanity
 *  checks, and still not having possibility to always fail gracefully.
 *
 *  (This library is not thread safe, and any list operations should
 *  be finished before next one can be started with same list object)
 *
 */

/* --------------------------------------------------------------- */
/* 1. Structure and prototypes for list usage                      */
/* --------------------------------------------------------------- */


/*
 *  Enumerated llist library parameters and return values.
 */

typedef enum
  {
    LLIST_NO  = 0,            /* FALSE */
    LLIST_YES = 1             /* TRUE */
  } lbool_e;

typedef enum
  {
    LLOOP_FORWARD  = 0,       /* Loop from head to tail */
    LLOOP_BACKWARD = 1        /* Loop from tail to head */
  } lloop_e;

typedef enum
  {
    LNODECMP_SMALLER = -1,    /* or any <0 */
    LNODECMP_EQUAL   =  0,
    LNODECMP_GREATER =  1     /* or any >0 */
  } lnodecmp_e;

typedef enum
  {
    LLISTCMP_MATCH_INORDER   = 1,   /* List 1 match with list2 and in same order.     */
    LLISTCMP_MATCH_REVERSE   = 2,   /* List 1 match with list2 and in reverse order.  */
    LLISTCMP_MATCH_NONORDER  = 4,   /* List 1 match with list2 but in random order.   */
    LLISTCMP_MATCH_SUBSET    = 8,   /* List 1 match as strict  subset inside list 2.  */
    LLISTCMP_MATCH_REVSUBSET = 16,  /* List 1 match as reverse subset inside list 2.  */
    LLISTCMP_MATCH_INCLUDED  = 32,  /* List 1 nodes all in list2 (in random order).   */
    LLISTCMP_MATCH_COVERED   = 64,  /* List 1 node values all covered by list 2.      */
    LLISTCMP_MATCH_PARTIAL   = 128, /* Only few nodes had match; considered no match. */
    LLISTCMP_MATCH_NOTHING   = 256, /* None of the nodes match between lists.         */
  } llistcmp_e;

#define LLISTCMP_NO_LIMITS   ~0     /* Accept any result above (also worst performance) */


/* --------------------------------------------------------------- */

/*
 *  (lnode_t*) -  Node type holding information about sibling nodes.
 */
#define LLIST_NODE \
  lnode_t * next; \
  lnode_t * prev;

typedef struct lnode_t lnode_t;

struct lnode_t
{
  LLIST_NODE
};


/*
 *  Callback prototypes. Node parameters guaranteed to be not NULLs when called.
 *  - NodeClone_f can return NULL for filtering purposes.
 *  - NodeClear_f can return NULL if node also became deallocated.
 *    (recommended to return same node pointer, since memory pool can be in use)
 */
typedef lnode_t *  (*NodeClear_f) ( lnode_t * node );
typedef lnode_t *  (*NodeClone_f) ( const lnode_t * node,  unsigned user_data );
typedef lbool_e    (*NodeFilter_f)( const lnode_t * node,  unsigned user_data );
typedef signed int (*NodeCmp_f)   ( const lnode_t * node1, const lnode_t * node2 );


/*
 *  (llist_t*) -  List type holding common information about nodes.
 */
typedef struct
{
  lnode_t *   first;
  lnode_t *   last;
  NodeClear_f clear_func;
  unsigned    node_size;
  unsigned    count;

  /*
   *  Optional build-in memory pool support
   *  Uses mpool.h which is specially designed for LList.
   */
  void * memorypool;

} llist_t;


/* --------------------------------------------------------------- */

/*
 *  Examle how to describe your own linked list nodes.
 *  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^�
 *
 * typedef struct
 * {
 *   LLIST_NODE
 *   // your own variables:
 *   int    id;
 *   int    value;
 *   char * str;
 *   // or any other stuff..
 * } your_struct_t;
 *
 */


/* --------------------------------------------------------------- */
/* 2. Simple basic functions for linked list handling.             */
/* --------------------------------------------------------------- */


/*
 *  Initialize (and allocates) the dynamic linked list object (llist_t*)
 *  with given default node size to be used to auto-allocate nodes.
 *  NodeClear can be given if node needs cleaning before removal,
 *  (e.g. contains pointers to other allocated content) as list performs
 *  auto-dealloc during node removal to pointer returned by NodeClear.
 */
llist_t * LInit( unsigned node_size, NodeClear_f NodeClear, void * memorypool );


/*
 *  Initialize static linked list object (llist_t).
 */
#define  LSetup( staticlist, _node_size, _NodeClear ) \
  staticlist.first      = NULL;                       \
  staticlist.last       = NULL;                       \
  staticlist.clear_func = _NodeClear;                 \
  staticlist.node_size  = _node_size;                 \
  staticlist.count      = 0;                          \
  staticlist.memorypool = NULL;


/*
 *  Set up static or dynamic list based on another list
 *  (Recommended in case lists swap nodes and share same memory pool)
 */
#define LSetupFrom( _list, _from_list )                              \
  ((llist_t*)_list)->first      = NULL;                              \
  ((llist_t*)_list)->last       = NULL;                              \
  ((llist_t*)_list)->clear_func = ((llist_t*)_from_list)->free_func; \
  ((llist_t*)_list)->node_size  = ((llist_t*)_from_list)->node_size; \
  ((llist_t*)_list)->count      = 0;                                 \
  ((llist_t*)_list)->memorypool = ((llist_t*)_from_list)->memorypool;


/*
 *  Create new nodes (allocates node with default size and auto-attach into list).
 *  No proper handling for out-of-memory cases, see LAlloc.
 */
lnode_t *  LCreateLast(   llist_t * list );
lnode_t *  LCreateFirst(  llist_t * list );
lnode_t *  LCreateBefore( llist_t * list, lnode_t * before );
lnode_t *  LCreateAfter(  llist_t * list, lnode_t * after  );


/*
 *  Get list and node object information.
 *  Parameters may not be NULLs, but NULL can be returned if no node.
 */
#define    LCount( list )    ((unsigned)(list)->count)
#define    LFirst( list )    ((list)->first)
#define    LLast(  list )    ((list)->last)
#define    LNext(  node )    ((node)->next)
#define    LPrev(  node )    ((node)->prev)

#define    LIsFirst( node )  (!(node)||!(node)->prev)
#define    LIsLast(  node )  (!(node)||!(node)->next)


/*
 *  Forward and backward loops for linked lists.
 */
#define    LFor( type, node, list ) \
    for( node = (type)LFirst(list); node != NULL; node = (type)LNext(node) )

#define    LBack( type, node, list ) \
    for( node = (type)LLast(list); node != NULL; node = (type)LPrev(node) )


/*
 *  Move single node inside of list.
 */
void       LMoveFirst(  llist_t * list, lnode_t * node );
void       LMoveLast(   llist_t * list, lnode_t * node );
void       LMoveAfter(  llist_t * list, lnode_t * node, lnode_t * after );
void       LMoveBefore( llist_t * list, lnode_t * node, lnode_t * before );


/*
 *  Removal of nodes calls NodeClear callback function and deallocates node(s).
 */
void       LRemoveLast(  llist_t *  list );
void       LRemoveFirst( llist_t *  list );
void       LRemove(      llist_t *  list, lnode_t * node );
void       LRemoveAll(   llist_t *  list );


/*
 *  Disposes (dynamic) list and sets it NULL, along with removing all nodes.
 */
void       LDispose( llist_t ** list );


/* --------------------------------------------------------------- */

/*
 *  Example how to init (dynamic) list and create new nodes.
 *  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^�
 *
 *  llist_t * mylist = LInit(sizeof(your_struct_t), NULL, NULL);
 *  ...
 *
 *  your_struct_t * newnode;
 *
 *  newnode = (your_struct_t*)LCreateLast(mylist);
 *  newnode->id = 1;
 *
 *  newnode = (your_struct_t*)LCreateLast(mylist);
 *  newnode->id = 2;
 *
 *  ...
 *  LDispose(&mylist);
 */


/*
 *  Example how to setup (static) list and create new nodes.
 *  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^�
 *
 *  llist_t mylist;
 *  LSetup(mylist, sizeof(your_struct_t), NULL, NULL);
 *  ...
 *
 *  your_struct_t * newnode;
 *
 *  newnode = (your_struct_t*)LCreateLast(&mylist);
 *  newnode->id = 1;
 *
 *  newnode = (your_struct_t*)LCreateLast(&mylist);
 *  newnode->id = 2;
 *
 *  ...
 *  RemoveAll(&mylist);
 */


/*
 *  Examle how easy it is to traverse through linked list.
 *  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^�
 *
 *  lnode_t * node = LFirst(list);
 *
 *  while(node)
 *    {
 *      printf("node id: %d", ((your_struct_t*)node)->id);
 *      node = LNext(node);
 *    }
 *
 *  ...
 *  your_struct_t* node;
 *
 *  LFor( your_struct_t*, node, list)
 *    {
 *      printf("node id: %d", node->id);
 *    }
 */


/* ------------------------------------------------------------------ */
/* 3. More complex functions for more demanding use of linked lists.  */
/* ------------------------------------------------------------------ */


/*
 *  Allocate node with any size and optionally with os_block_alloc_no_wait.
 *  Node can be attached to linked list separately with LAttach* functions.
 */
lnode_t *  LAlloc( unsigned any_size, lbool_e no_wait );


/*
 *  Detach (pop) node(s) from linked list, either the only a certain node or
 *  also certain amount (where 0 means rest of) from given direction.
 */
lnode_t *  LDetachLast(   llist_t * list );
lnode_t *  LDetachFirst(  llist_t * list );
lnode_t *  LDetachAll(    llist_t * list );
void       LDetach(       llist_t * list, lnode_t * node );
void       LDetachMany(   llist_t * list, lnode_t * node, unsigned number, lloop_e direction );
unsigned   LDetachCount(  lnode_t * node, lloop_e direction );


/*
 *  Attach (push) node(s) back to linked list (to the same or an another list).
 *
 *  The target list must share same setup with list where node was detached from:
 *  - Same structures (your_struct_t) or derived from common structure.
 *  - Same NodeClear function, otherwise risk of memoryleaks, etc.
 *  - Same memorypool used, otherwise risk of invalid memory deallocations.
 *  - Same default node size (sizeof(your_struct_t)), if LFilterClone is used.
 *  - Same usage of expanded node casting, otherwise certain resets.
 */
void       LAttachLast(   llist_t * list, lnode_t * node );
void       LAttachFirst(  llist_t * list, lnode_t * node );
void       LAttachBefore( llist_t * list, lnode_t * node, lnode_t * before );
void       LAttachAfter(  llist_t * list, lnode_t * node, lnode_t * after  );


/*
 *  Sorted attachment adds node in place of first match from looping direction and
 *  should not used with other attach methods and only for list already in order.
 */
void       LAttachSorted( llist_t * list, lnode_t * node, NodeCmp_f NodeCmp, lloop_e direction );


/*
 *  Deallocate all detached nodes by first calling given function and then
 *  auto-dealloc from OS or memory pool.
 */
void       LDealloc( lnode_t * node, NodeClear_f NodeClear, void * memorypool );


/*
 *  Handle linked list based on index numbers (zero based, and -1 returned if no nodes).
 */
lnode_t *  LGetNode(   llist_t * list, int index );
int        LGetIndex(  llist_t * list, lnode_t * node );
int        LSetIndex(  llist_t * list, lnode_t * node, int index );
int        LLastIndex( llist_t * list );


/*
 *  Swap node placements inside/between linked list(s).
 */
void       LSwapInside(  llist_t * list,  lnode_t * node1, lnode_t * node2 );
void       LSwapBetween( llist_t * list1, lnode_t * node1, llist_t * list2, lnode_t * node2 );
void       LSwapAll(     llist_t * list1, llist_t * list2 );


/*
 *  Splits the list by given node being first of the allocated new list.
 *  Joins nodes to first list from latter list and deallocates it.
 */
llist_t *  LSplit( llist_t * list, lnode_t *  node );
void       LJoin(  llist_t * list, llist_t ** other );


/*
 *  Verify that list contains a certain node.
 */
lbool_e    LContains( llist_t * list, lnode_t * node );


/*
 *  Compare nodes of two list together with given compare function.
 *  - LLISTCMP_NO_LIMITS for all or limit the compare options for greater speed by adding limit mask.
 *  - LLISTCMP_MATCH_NOTHING return value means no match, if limit mask used, otherwise can
 *    also return e.g. LLISTCMP_MATCH_PARTIAL which also can be seen as no match in some cases.
 */
llistcmp_e LCompare( llist_t * list, llist_t * other, NodeCmp_f NodeCmp, unsigned limit_mask );


/*
 *  Find single node which content match to other node or given criteria.
 */
lnode_t *  LFindPair( lnode_t * start_node, NodeCmp_f    NodeCmp,    lnode_t * match_node, lloop_e direction );
lnode_t *  LFindNode( lnode_t * start_node, NodeFilter_f NodeFilter, unsigned user_data,   lloop_e direction );


/*
 *  Traverse trough nodes and count/remove/operate nodes which match by filter.
 */
unsigned   LFilterCount(   llist_t * list, NodeFilter_f NodeFilter,  unsigned user_data );
unsigned   LFilterRemove(  llist_t * list, NodeFilter_f NodeFilter,  unsigned user_data );
unsigned   LFilterOperate( llist_t * list, NodeFilter_f NodeFilter,  unsigned user_data_filter,
                                           NodeFilter_f NodeOperate, unsigned user_data_operate );

/*
 *  Clone list (allocates other list if NULL given and allocates copy nodes if no clone function given),
 *  or move items from one list to other list by filtering with given callback function.
 */
void       LFilterClone(  llist_t * list, llist_t ** other, NodeClone_f  NodeClone,  unsigned user_data );
void       LFilterMove(   llist_t * list, llist_t ** other, NodeFilter_f NodeFilter, unsigned user_data );


/*
 *  Remove duplicate nodes based on compare function from given direction.
 */
void       LUnique( llist_t * list, NodeCmp_f NodeCmp, lloop_e direction );


/*
 *  Change the order of items into reverse or random.
 */
void       LShuffle( llist_t * list, int (*randomizer)(void) );
void       LReverse( llist_t * list );


/*
 *  Sort linked list with given compare function if verified not to be sorted.
 */
lbool_e    LVerify(  llist_t * list, NodeCmp_f NodeCmp );
void       LSort(    llist_t * list, NodeCmp_f NodeCmp );


/* --------------------------------------------------------------- */

/*
 *  Examples of compare functions needed for NodeCmp_f callback function.
 *  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^�
 *
 *  int compare_my_numbers( const lnode_t * node1, const lnode_t * node2 )
 *  {
 *    return ( (your_struct_t*)node1->value - (your_struct_t*)node2->value );
 *
 *    // WARNING: You might consider using if-statements and LNODECMP_SMALLER/
 *    // LNODECMP_LARGER/LNODECMP_EQUAL instead of this "trick", since too big
 *    // positive unsigned subtracted by small one may result conversion issue:
 *    // LNODECMP_GREATER == (unsigned)65535 -> (int)-32768 == LNODECMP_SMALLER
 *  }
 *
 *
 *  int compare_my_strings( const lnode_t * node1, const lnode_t * node2 )
 *  {
 *    return ucs2_strcmp( (your_struct_t*)node1->name, (your_struct_t*)node2->name );
 *  }
 *
 *  ...
 *  LSort(list, compare_my_numbers );
 *  LSort(list, compare_my_strings );
 *
 */

/*
 *  Examles how easy it is to move nodes inside the linked list.
 *  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^�
 *
 *  // swap node with its next node (here, inside the same list)
 *  LSwapInside(list, node, LNext(node));
 *
 *  // or move node from list1 as first into the list2.
 *  LDetach(list1, node);
 *  LAttachFirst(list2, node);
 *
 *  // or with indices, get 5th node and move it to be 3rd.
 *  node = LGetNode(list, 5);
 *  (void)LSetIndex(list, node, 3);
 *
 */

/*
 *  Example how to use LFind* -functions.
 *  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^�
 *
 *  // Find if there is (in same or in different list) node with same information.
 *  node = LFindPair( LFirst(c1list), compare_my_nodes, LFirst(c2list),LLOOP_FORWARD);
 *
 */


/* ------------------------------------------------------------------ */
/* 4. Something extraordinary ...                                     */
/* ------------------------------------------------------------------ */

/*
 *  Bidirectional traverse functions.
 *
 *  lnode_t *  LLoopHead(   llist_t * list, lloop_e direction );
 *  lnode_t *  LLoopNext(   lnode_t * node, lloop_e direction );
 *  lbool_e    LIsLoopHead( llist_t * node, lloop_e direction );
 */
typedef struct { lnode_t* link[2]; } llink_t;

#define    LLoopHead(   list, direction )  (((llink_t*)list)->link[direction])
#define    LLoopNext(   node, direction )  (((llink_t*)node)->link[direction])
#define    LIsLoopHead( node, direction )  (!(node)||!((llink_t*)node)->link[LLOOPBACKWARD-direction])

#define    LLoop( type, node, list, direction ) \
    for( node = (type)LLoopHead(list, direction); node; node = (type)LLoopNext(node, direction) )


/*
 *  Expanded Nodes.
 *
 *  One can use this library also with items with other placement of
 *  "LLIST_NODE" fields in the structure than in the beginning.
 *
 *  This can be done by using any existing fixed structure and allocating
 *  an extra space for it to add linked list fields at the end of it.
 *
 *  // FreeNode_f parameter is mandatory for LInit if LRemove* functions used.
 *  // Use LExpandedDealloc for FreeNode_f or use LCastObject for detached
 *  // nodes or in your own free callback function before deallocation.
 *  mylist = LInit( 0, LExpandedDealloc, NULL );
 *
 *  {
 *    any_struct_t * object;
 *
 *    // Calculate the needed size and get offset
 *    unsigned size   = sizeof(any_struct_t);
 *    unsigned offset = LExpandedSize( &size );
 *
 *    // Allocate object normally (or use LAlloc) with expanded size.
 *    object = (any_struct_t*)os_block_alloc_and_clear( size );
 *
 *    // Typecast object to node with offset and put it into linked list.
 *    LAttachLast(mylist, LCastNode( (void*)object, offset ));
 *  }
 *
 *  {
 *    // Typecast node back to your object before use (or deallocation).
 *    node = LDetachFirst(mylist);
 *    object = (any_struct_t*)LCastObject( node );
 *
 *    // Pass object to anywhere, as it can now be used and deallocated normally.
 *    ..._ObjectHandler( object );
 *  }
 *
 */

lnode_t *  LCastNode( void * object, unsigned offset );
void *     LCastObject( lnode_t * node );

unsigned   LExpandedSize(   unsigned * size );
lnode_t *  LExpandedDealloc( lnode_t * node );


/* --------------------------------------------------------------- */

#endif /* LLIST_H */

#ifdef __cplusplus
}
#endif

