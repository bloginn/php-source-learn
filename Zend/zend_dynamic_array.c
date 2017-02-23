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
   |          Zeev Suraski <zeev@zend.com>                                |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#include "zend.h"

typedef struct _dynamic_array {
        char *array;
        unsigned int element_size;/* 每个元素大小 */
        unsigned int current;
        unsigned int allocated;/* 分配数量 */
} dynamic_array;/* 动态数组结构体 */

ZEND_API int zend_dynamic_array_init(dynamic_array *da, unsigned int element_size, unsigned int size)/* 初始化动态数组，未见调用 */
{
	da->element_size = element_size;
	da->allocated = size;
	da->current = 0;
	da->array = (char *) emalloc(size*element_size);
	if (da->array == NULL) {
		return 1;
	}
	return 0;
}

ZEND_API void *zend_dynamic_array_push(dynamic_array *da)/* 未见调用 */
{
	if (da->current == da->allocated) {
		da->allocated *= 2;
		da->array = (char *) erealloc(da->array, da->allocated*da->element_size);
	}
	return (void *)(da->array+(da->current++)*da->element_size);
}

ZEND_API void *zend_dynamic_array_pop(dynamic_array *da)/* 未见调用 */
{
	return (void *)(da->array+(--(da->current))*da->element_size);

}

ZEND_API void *zend_dynamic_array_get_element(dynamic_array *da, unsigned int index)/* 未见调用 */
{
	if (index >= da->current) {
		return NULL;
	}
	return (void *)(da->array+index*da->element_size);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
