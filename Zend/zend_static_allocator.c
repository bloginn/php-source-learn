/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) 1998-2016 Zend Technologies Ltd. (http://www.zend.com) |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.00 of the Zend license,     |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.zend.com/license/2_00.txt.                                |
   | If you did not receive a copy of the Zend license and are unable to  |
   | obtain it through the world-wide-web, please send a note to          |
   | license@zend.com so we can mail you a copy immediately.              |
   +----------------------------------------------------------------------+
   | Authors: Andi Gutmans <andi@zend.com>                                |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#include "zend_static_allocator.h"
/* 该文件中的函数未见使用 */
/* Not checking emalloc() and erealloc() return values as they are supposed to bailout */

inline static void block_init(Block *block, zend_uint block_size)/* 初始化一个大小为block_size的块 */
{
	block->pos = block->bp = (char *) emalloc(block_size);/* 申请一个block_size内存 */
	block->end = block->bp + block_size;/* 将块的end的指针指向最后 */
}

inline static char *block_allocate(Block *block, zend_uint size)
{
	char *retval = block->pos;
	if ((block->pos += size) >= block->end) {
		return (char *)NULL;
	}
	return retval;
}

inline static void block_destroy(Block *block)/* 释放块内存 */
{
	efree(block->bp);
}

void static_allocator_init(StaticAllocator *sa)/* 静态分配器初始化,申请一个块 */
{
	sa->Blocks = (Block *) emalloc(sizeof(Block));
	block_init(sa->Blocks, ALLOCATOR_BLOCK_SIZE);/* ALLOCATOR_BLOCK_SIZE为400000字节,大约390kb */
	sa->num_blocks = 1;
	sa->current_block = 0;
}

char *static_allocator_allocate(StaticAllocator *sa, zend_uint size)/* 静态分配器分配一个块 */
{
	char *retval;

	retval = block_allocate(&sa->Blocks[sa->current_block], size);
	if (retval) {
		return retval;
	}
	sa->Blocks = (Block *) erealloc(sa->Blocks, ++sa->num_blocks);
	sa->current_block++;
	block_init(&sa->Blocks[sa->current_block], (size > ALLOCATOR_BLOCK_SIZE) ? size : ALLOCATOR_BLOCK_SIZE);
	retval = block_allocate(&sa->Blocks[sa->current_block], size);
	return retval;
}

void static_allocator_destroy(StaticAllocator *sa)/* 释放静态分配器中的所有块 */
{
	zend_uint i;

	for (i=0; i<sa->num_blocks; i++) {
		block_free(&sa->Blocks[i]);
	}
	efree(sa->Blocks);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
