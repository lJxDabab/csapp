/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define WSIZE 4
#define DSIZE 8
//当堆内存不够时，向内核申请的堆空间
#define CHUNKSIZE (1<<12)
//将val放入p开始的4字节中
#define PUT(p,val) (*(unsigned int*)(p) = (val))
//获得头部和脚部的编码
#define PACK(size, alloc) ((size) | (alloc))
//从头部或脚部获得块大小和已分配位
#define GET_SIZE(p) (*(unsigned int*)(p) & ~0x7)
#define GET_ALLO(p) (*(unsigned int*)(p) & 0x1)
//获得块的头部和脚部
#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
//获得上一个块和下一个块
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE((char*)(bp) - DSIZE))

#define MAX(x,y) ((x)>(y)?(x):(y))
static char *heap_listp;
/* 
 * mm_init - initialize the malloc package.
 */

static void *imme_coalesce(void*bp)
{
    size_t prev_alloc=GET_ALLO(FTRP(PREV_BLKP(bp)));
    size_t next_alloc=GET_ALLO(HDRP(NEXT_BLKP(bp)));
    size_t size=GET_SIZE(HDRP(bp));
    if(prev_alloc && next_alloc){
        return bp;
    }else if(prev_alloc&& (!next_alloc))
    {
        size+=GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp),PACK(size,0));
        PUT(FTRP(bp),PACK(size,0));
    }else if((!prev_alloc) && next_alloc)
    {
        size+=GET_SIZE(FTRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
        PUT(FTRP(bp),PACK(size,0));
        bp=PREV_BLKP(bp);
    }else{
        size+=GET_SIZE(HDRP(NEXT_BLKP(bp))) +
        GET_SIZE(FTRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
    }
    return bp;
}

static void *expand_heap(size_t words)
{
    size_t size;
    void*bp;
    size=words%2 ? (words+1)*WSIZE : words*WSIZE;
    if((bp=mem_sbrk(size))==(void*)-1)
    {
        return NULL;
    }
    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));
    return imme_coalesce(bp);
} 
int mm_init(void)
{
    if((heap_listp=mem_sbrk(4*WSIZE))==(void*)-1)
    {
        return -1;
    }
    PUT(heap_listp,0);
    PUT(heap_listp+(1*WSIZE),PACK(DSIZE,1));
    PUT(heap_listp+(2*WSIZE),PACK(DSIZE,1));
    PUT(heap_listp+(3*WSIZE),PACK(0,1));
    heap_listp+=DSIZE;
    if(expand_heap(CHUNKSIZE/WSIZE)==NULL)
    {
        return -1;
    }
    return 0;
} 

static void *first_fit(size_t asize)
{
  void *bp=heap_listp;
  size_t size;
  while((size=GET_SIZE(HDRP(bp)))!=0)
  {
     if((size>=asize)&&(!GET_ALLO(HDRP(bp))))
     {
         return bp;
     }
     bp=NEXT_BLKP(bp);
  }
  return NULL;
}
static void *delay_coalesce(){
    void *bp=heap_listp;
    while(GET_SIZE(HDRP(bp))!=0){
        if(!GET_ALLO(HDRP(bp)))
        {
            bp=imme_coalesce(bp);
        }
        bp=NEXT_BLKP(bp);
    }
}

static void place(void *bp,size_t asize)
{
    size_t remain_size=GET_SIZE(HDRP(bp))-asize;
    if(remain_size<DSIZE)
    {
        PUT(HDRP(bp),PACK(GET_SIZE(HDRP(bp)),1));
        PUT(FTRP(bp),PACK(GET_SIZE(HDRP(bp)),1));
    }
    else{
        PUT(HDRP(bp),PACK(asize,1));
        PUT(FTRP(bp),PACK(asize,1));
        PUT(HDRP(NEXT_BLKP(bp)),PACK(remain_size,0));
        PUT(FTRP(NEXT_BLKP(bp)),PACK(remain_size,0));
    }
}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    void *bp;
    if(size==0)
    {
        return 0;
    }
    if(size<=DSIZE)
    {
        asize=2*DSIZE;
    }
    else{
        asize=DSIZE*((size+(DSIZE)+(DSIZE-1))/DSIZE);
    }
    if((bp=first_fit(asize))!=NULL)
    {
        place(bp,asize);
        return bp;
    }
    extendsize=MAX(CHUNKSIZE,asize);
   if((bp=expand_heap(extendsize/WSIZE))==NULL)
   {
       return NULL;
   }
   place(bp,asize);
   return bp;
   
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size;
    size=GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr),PACK(size,0));
    PUT(FTRP(ptr),PACK(size,0));
    imme_coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t asize,ptr_size;
    void*new_bp;
    if(ptr==NULL)
    {
        return mm_malloc(size);
    }
    if(size==0)
    {
        mm_free(ptr);
        return NULL;
    }
   asize= size<=DSIZE? 2*DSIZE:DSIZE*((size+DSIZE+DSIZE-1)/DSIZE);
   new_bp=imme_coalesce(ptr);
   ptr_size=GET_SIZE(HDRP(new_bp));
   PUT(HDRP(new_bp),PACK(ptr_size,1));
   PUT(FTRP(new_bp),PACK(ptr_size,1));
   if(new_bp!=ptr)
   {
       memcpy(new_bp,ptr,GET_SIZE(HDRP(ptr))-DSIZE);
   }
   if(ptr_size==asize)
   {
       return new_bp;
   }
   else if (ptr_size>asize)
   {
       place(new_bp,asize);
       return new_bp;
   }else{
       ptr=mm_malloc(asize);
       if(ptr==NULL)
       {
           return NULL;
       }
       memcpy(ptr,new_bp,ptr_size-DSIZE);
       mm_free(new_bp);
       return ptr;
   }
}














