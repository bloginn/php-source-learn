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
   | Authors: Bob Weinand <bwoebi@php.net>                                |
   |          Dmitry Stogov <dmitry@zend.com>                             |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef ZEND_AST_H
#define ZEND_AST_H

typedef struct _zend_ast zend_ast;

#include "zend.h"

typedef enum _zend_ast_kind {
	/* first 256 kinds are reserved for opcodes */
	ZEND_CONST = 256,
	ZEND_BOOL_AND,
	ZEND_BOOL_OR,
	ZEND_SELECT,
	ZEND_UNARY_PLUS,/* unary 一元的 */
	ZEND_UNARY_MINUS,
} zend_ast_kind;

struct _zend_ast {
	unsigned short kind;
	unsigned short children;
	union {
		zval     *val;
		zend_ast *child;
	} u;
};

ZEND_API zend_ast *zend_ast_create_constant(zval *zv); /* 语法解析器调用，主要创建常量 */

ZEND_API zend_ast *zend_ast_create_unary(uint kind, zend_ast *op0); /* 语法解析器调用，主要用于!~-+四个符号接常量时调用 */
ZEND_API zend_ast *zend_ast_create_binary(uint kind, zend_ast *op0, zend_ast *op1); /* 语法解析器调用，主要用于两个常量的逻辑运算调用 */
ZEND_API zend_ast *zend_ast_create_ternary(uint kind, zend_ast *op0, zend_ast *op1, zend_ast *op2); /* 语法解析器调用，主要用于三元运算符调用 */
ZEND_API zend_ast* zend_ast_create_dynamic(uint kind); /* 语法解析器调用，主要用于数组双箭头=>的调用 */
ZEND_API void zend_ast_dynamic_add(zend_ast **ast, zend_ast *op); /* 语法解析器调用，配合zend_ast_create_dynamic使用，添加数组的值 */
ZEND_API void zend_ast_dynamic_shrink(zend_ast **ast);

ZEND_API int zend_ast_is_ct_constant(zend_ast *ast); /* zend_compile.c的zend_do_constant_expression函数调用 */

ZEND_API void zend_ast_evaluate(zval *result, zend_ast *ast, zend_class_entry *scope TSRMLS_DC); /* zend_compile.c的zend_do_constant_expression函数和zend_execute_API.c中调用，主要用于逻辑求值 */ 

ZEND_API zend_ast *zend_ast_copy(zend_ast *ast); /* 复制ast数据 */
ZEND_API void zend_ast_destroy(zend_ast *ast); /* 释放ast数据 */

#endif
