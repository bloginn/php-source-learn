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

#include <ctype.h>

#include "zend.h"
#include "zend_operators.h"
#include "zend_variables.h"
#include "zend_globals.h"
#include "zend_list.h"
#include "zend_API.h"
#include "zend_strtod.h"
#include "zend_exceptions.h"
#include "zend_closures.h"

#if ZEND_USE_TOLOWER_L
#include <locale.h>
static _locale_t current_locale = NULL;
/* this is true global! may lead to strange effects on ZTS, but so may setlocale() */
#define zend_tolower(c) _tolower_l(c, current_locale)
#else
#define zend_tolower(c) tolower(c) /* zend_tolower只在本文件中有使用 */
#endif

#define TYPE_PAIR(t1,t2) (((t1) << 4) | (t2))

static const unsigned char tolower_map[256] = {
0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
0x40,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f, /* 0x61等于十进制97,ascii表中97正好是a,A的小写对应tolower_map[65]即0x61 */
0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x5b,0x5c,0x5d,0x5e,0x5f, /* 直到这一行的0x7a,0x7a-0x61+1=0x19+1正好是26个英文字母 */
0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
};

#define zend_tolower_ascii(c) (tolower_map[(unsigned char)(c)]) /* 主要将ascii表中的字符A~Z转换成小写 zend_tolower_ascii只在本文件中有使用 */

/**
 * Functions using locale lowercase:
 	 	zend_binary_strncasecmp_l
 	 	zend_binary_strcasecmp_l
		zend_binary_zval_strcasecmp
		zend_binary_zval_strncasecmp
		string_compare_function_ex
		string_case_compare_function
 * Functions using ascii lowercase:
  		zend_str_tolower_copy
		zend_str_tolower_dup
		zend_str_tolower
		zend_binary_strcasecmp
		zend_binary_strncasecmp
 */

ZEND_API int zend_atoi(const char *str, int str_len)/* 将字符串转换成int整数 */ /* {{{ */ 
{
	int retval;

	if (!str_len) {
		str_len = strlen(str);
	}
	retval = strtol(str, NULL, 0);/* strtol() 函数用来将字符串采用十进制转换为长整型数(long) */
	if (str_len>0) {
		switch (str[str_len-1]) {
			case 'g':
			case 'G':
				retval *= 1024;
				/* break intentionally missing */
			case 'm':
			case 'M':
				retval *= 1024;
				/* break intentionally missing */
			case 'k':
			case 'K':
				retval *= 1024;
				break;
		}
	}
	return retval;
}
/* }}} */

ZEND_API long zend_atol(const char *str, int str_len)/* 将字符串转换成long整数 */ /* {{{ */ 
{
	long retval;

	if (!str_len) {
		str_len = strlen(str);
	}
	retval = strtol(str, NULL, 0);/* strtol() 函数用来将字符串采用十进制转换为长整型数(long) */
	if (str_len>0) {
		switch (str[str_len-1]) {
			case 'g':
			case 'G':
				retval *= 1024;
				/* break intentionally missing */
			case 'm':
			case 'M':
				retval *= 1024;
				/* break intentionally missing */
			case 'k':
			case 'K':
				retval *= 1024;
				break;
		}
	}
	return retval;
}
/* }}} */

ZEND_API double zend_string_to_double(const char *number, zend_uint length)/* 未见调用，将字符串转换成双进度浮点型 */ /* {{{ */
{
	double divisor = 10.0;/*因子*/
	double result = 0.0;/*结果*/
	double exponent;
	const char *end = number+length;
	const char *digit = number;

	if (!length) {
		return result;
	}

	while (digit < end) {
		if ((*digit <= '9' && *digit >= '0')) {
			result *= 10;
			result += *digit - '0';
		} else if (*digit == '.') {
			digit++;
			break;
		} else if (toupper(*digit) == 'E') {
			exponent = (double) atoi(digit+1);
			result *= pow(10.0, exponent);
			return result;
		} else {
			return result;
		}
		digit++;
	}

	while (digit < end) {
		if ((*digit <= '9' && *digit >= '0')) {
			result += (*digit - '0') / divisor;
			divisor *= 10;
		} else if (toupper(*digit) == 'E') {
			exponent = (double) atoi(digit+1);
			result *= pow(10.0, exponent);
			return result;
		} else {
			return result;
		}
		digit++;
	}
	return result;
}
/* }}} */

ZEND_API void convert_scalar_to_number(zval *op TSRMLS_DC)/* 标量转换为数字 */ /* {{{ */
{
	switch (Z_TYPE_P(op)) { /* Z_TYPE_P(op)等价(*op).type */
		case IS_STRING:
			{
				char *strval;

				strval = Z_STRVAL_P(op);
				if ((Z_TYPE_P(op)=is_numeric_string(strval, Z_STRLEN_P(op), &Z_LVAL_P(op), &Z_DVAL_P(op), 1)) == 0) {
					ZVAL_LONG(op, 0);
				}
				str_efree(strval);
				break;
			}
		case IS_BOOL:
			Z_TYPE_P(op) = IS_LONG;
			break;
		case IS_RESOURCE:
			zend_list_delete(Z_LVAL_P(op));
			Z_TYPE_P(op) = IS_LONG;
			break;
		case IS_OBJECT:
			convert_to_long_base(op, 10);
			break;
		case IS_NULL:
			ZVAL_LONG(op, 0);
			break;
	}
}
/* }}} */

/* {{{ zendi_convert_scalar_to_number */
#define zendi_convert_scalar_to_number(op, holder, result)			\
	if (op==result) {												\
		if (Z_TYPE_P(op) != IS_LONG) {								\
			convert_scalar_to_number(op TSRMLS_CC);					\
		}															\
	} else {														\
		switch (Z_TYPE_P(op)) {										\
			case IS_STRING:											\
				{													\
					if ((Z_TYPE(holder)=is_numeric_string(Z_STRVAL_P(op), Z_STRLEN_P(op), &Z_LVAL(holder), &Z_DVAL(holder), 1)) == 0) {	\
						ZVAL_LONG(&(holder), 0);							\
					}														\
					(op) = &(holder);										\
					break;													\
				}															\
			case IS_BOOL:													\
			case IS_RESOURCE:												\
				ZVAL_LONG(&(holder), Z_LVAL_P(op));							\
				(op) = &(holder);											\
				break;														\
			case IS_NULL:													\
				ZVAL_LONG(&(holder), 0);									\
				(op) = &(holder);											\
				break;														\
			case IS_OBJECT:													\
				(holder) = (*(op));											\
				zval_copy_ctor(&(holder));									\
				convert_to_long_base(&(holder), 10);						\
				if (Z_TYPE(holder) == IS_LONG) {							\
					(op) = &(holder);										\
				}															\
				break;														\
		}																	\
	}

/* }}} */

/* {{{ zendi_convert_to_long */
#define zendi_convert_to_long(op, holder, result)					\
	if (op == result) {												\
		convert_to_long(op);										\
	} else if (Z_TYPE_P(op) != IS_LONG) {							\
		switch (Z_TYPE_P(op)) {										\
			case IS_NULL:											\
				Z_LVAL(holder) = 0;									\
				break;												\
			case IS_DOUBLE:											\
				Z_LVAL(holder) = zend_dval_to_lval(Z_DVAL_P(op));	\
				break;												\
			case IS_STRING:											\
				Z_LVAL(holder) = strtol(Z_STRVAL_P(op), NULL, 10);	\
				break;												\
			case IS_ARRAY:											\
				Z_LVAL(holder) = (zend_hash_num_elements(Z_ARRVAL_P(op))?1:0);	\
				break;												\
			case IS_OBJECT:											\
				(holder) = (*(op));									\
				zval_copy_ctor(&(holder));							\
				convert_to_long_base(&(holder), 10);				\
				break;												\
			case IS_BOOL:											\
			case IS_RESOURCE:										\
				Z_LVAL(holder) = Z_LVAL_P(op);						\
				break;												\
			default:												\
				zend_error(E_WARNING, "Cannot convert to ordinal value");	\
				Z_LVAL(holder) = 0;									\
				break;												\
		}															\
		Z_TYPE(holder) = IS_LONG;									\
		(op) = &(holder);											\
	}

/* }}} */

/* {{{ zendi_convert_to_boolean */
#define zendi_convert_to_boolean(op, holder, result)				\
	if (op==result) {												\
		convert_to_boolean(op);										\
	} else if (Z_TYPE_P(op) != IS_BOOL) {							\
		switch (Z_TYPE_P(op)) {										\
			case IS_NULL:											\
				Z_LVAL(holder) = 0;									\
				break;												\
			case IS_RESOURCE:										\
			case IS_LONG:											\
				Z_LVAL(holder) = (Z_LVAL_P(op) ? 1 : 0);			\
				break;												\
			case IS_DOUBLE:											\
				Z_LVAL(holder) = (Z_DVAL_P(op) ? 1 : 0);			\
				break;												\
			case IS_STRING:											\
				if (Z_STRLEN_P(op) == 0								\
					|| (Z_STRLEN_P(op)==1 && Z_STRVAL_P(op)[0]=='0')) {	\
					Z_LVAL(holder) = 0;								\
				} else {											\
					Z_LVAL(holder) = 1;								\
				}													\
				break;												\
			case IS_ARRAY:											\
				Z_LVAL(holder) = (zend_hash_num_elements(Z_ARRVAL_P(op))?1:0);	\
				break;												\
			case IS_OBJECT:											\
				(holder) = (*(op));									\
				zval_copy_ctor(&(holder));							\
				convert_to_boolean(&(holder));						\
				break;												\
			default:												\
				Z_LVAL(holder) = 0;									\
				break;												\
		}															\
		Z_TYPE(holder) = IS_BOOL;									\
		(op) = &(holder);											\
	}

/* }}} */

/* {{{ convert_object_to_type */
#define convert_object_to_type(op, ctype, conv_func)										\
	if (Z_OBJ_HT_P(op)->cast_object) {														\
		zval dst;																			\
		if (Z_OBJ_HT_P(op)->cast_object(op, &dst, ctype TSRMLS_CC) == FAILURE) {			\
			zend_error(E_RECOVERABLE_ERROR,													\
				"Object of class %s could not be converted to %s", Z_OBJCE_P(op)->name,		\
			zend_get_type_by_const(ctype));													\
		} else {																			\
			zval_dtor(op);																	\
			Z_TYPE_P(op) = ctype;															\
			op->value = dst.value;															\
		}																					\
	} else {																				\
		if (Z_OBJ_HT_P(op)->get) {															\
			zval *newop = Z_OBJ_HT_P(op)->get(op TSRMLS_CC);								\
			if (Z_TYPE_P(newop) != IS_OBJECT) {												\
				/* for safety - avoid loop */												\
				zval_dtor(op);																\
				*op = *newop;																\
				FREE_ZVAL(newop);															\
				conv_func(op);																\
			}																				\
		}																					\
	}

/* }}} */

ZEND_API void convert_to_long(zval *op)/* 将值转换成LONG类型 */ /* {{{ */
{
	if (Z_TYPE_P(op) != IS_LONG) {
		convert_to_long_base(op, 10);
	}
}
/* }}} */

ZEND_API void convert_to_long_base(zval *op, int base) /* {{{ */
{
	long tmp;

	switch (Z_TYPE_P(op)) {
		case IS_NULL: /* 如果op为NULL类型，将其值设置为0 */
			Z_LVAL_P(op) = 0; /* Z_LVAL_P(op)等价(*op).value.lval */
			break;
		case IS_RESOURCE: {
				TSRMLS_FETCH();

				zend_list_delete(Z_LVAL_P(op)); /* zend_list.c文件_zend_list_delete函数 */
			}
			/* break missing intentionally */
		case IS_BOOL: /* bool不处理，因为bool的值只有0，1 */
		case IS_LONG:
			break;
		case IS_DOUBLE:
			Z_LVAL_P(op) = zend_dval_to_lval(Z_DVAL_P(op)); /* Z_DVAL_P(op)等价(*op).value.dval */
			break;
		case IS_STRING:
			{
				char *strval = Z_STRVAL_P(op); /* Z_STRVAL_P(op)等价(*op).value.str.val */

				Z_LVAL_P(op) = strtol(strval, NULL, base); /* C语言strtol()函数：将字符串转换成long(长整型数) */
				str_efree(strval);
			}
			break;
		case IS_ARRAY:
			tmp = (zend_hash_num_elements(Z_ARRVAL_P(op))?1:0);
			zval_dtor(op);
			Z_LVAL_P(op) = tmp;
			break;
		case IS_OBJECT:
			{
				int retval = 1;
				TSRMLS_FETCH();

				convert_object_to_type(op, IS_LONG, convert_to_long); /* 调用convert_object_to_type宏转换 */

				if (Z_TYPE_P(op) == IS_LONG) {
					return;
				} /* 再次检测是否转换成功，否则报NOTICE级别错误 */
				zend_error(E_NOTICE, "Object of class %s could not be converted to int", Z_OBJCE_P(op)->name);

				zval_dtor(op);
				ZVAL_LONG(op, retval);/* 由于NOTICE级别的错误不是致命的，所以还需继续执行，将op的值置为1 */
				return;
			}
		default:
			zend_error(E_WARNING, "Cannot convert to ordinal value");
			zval_dtor(op);
			Z_LVAL_P(op) = 0;
			break;
	}

	Z_TYPE_P(op) = IS_LONG;/* 最后将类型改为LONG类型 */
}
/* }}} */

ZEND_API void convert_to_double(zval *op)/* 将op的值转换成双精度浮点型 */ /* {{{ */
{
	double tmp;

	switch (Z_TYPE_P(op)) {
		case IS_NULL:
			Z_DVAL_P(op) = 0.0;/* 为什么(double)(NULL)的值不是0.0,貌似没有走到这里 */
			break;
		case IS_RESOURCE: {
				TSRMLS_FETCH();

				zend_list_delete(Z_LVAL_P(op));/* zend_list.c文件_zend_list_delete函数 */
			}
			/* break missing intentionally */ /* 不加break的话,一直往下执行，直到遇到break(446行) http://www.cnblogs.com/xiaozong/p/6372924.html */
		case IS_BOOL:
		case IS_LONG:
			Z_DVAL_P(op) = (double) Z_LVAL_P(op);
			break;
		case IS_DOUBLE:
			break;
		case IS_STRING:
			{
				char *strval = Z_STRVAL_P(op);

				Z_DVAL_P(op) = zend_strtod(strval, NULL);
				str_efree(strval);
			}
			break;
		case IS_ARRAY:
			tmp = (zend_hash_num_elements(Z_ARRVAL_P(op))?1:0);
			zval_dtor(op);
			Z_DVAL_P(op) = tmp;
			break;
		case IS_OBJECT:
			{
				double retval = 1.0;
				TSRMLS_FETCH();

				convert_object_to_type(op, IS_DOUBLE, convert_to_double);

				if (Z_TYPE_P(op) == IS_DOUBLE) {
					return;
				}/* 再次检测是否转换成功，否则报NOTICE级别错误 */
				zend_error(E_NOTICE, "Object of class %s could not be converted to double", Z_OBJCE_P(op)->name);

				zval_dtor(op);
				ZVAL_DOUBLE(op, retval);/* 由于NOTICE级别的错误不是致命的，所以还需继续执行，将op的值置为1.0 */
				break;
			}
		default:
			zend_error(E_WARNING, "Cannot convert to real value (type=%d)", Z_TYPE_P(op));
			zval_dtor(op);
			Z_DVAL_P(op) = 0;
			break;
	}
	Z_TYPE_P(op) = IS_DOUBLE;
}
/* }}} */

ZEND_API void convert_to_null(zval *op)/* 将数据转换成null类型 */ /* {{{ */
{
	if (Z_TYPE_P(op) == IS_OBJECT) {
		if (Z_OBJ_HT_P(op)->cast_object) { /* 等价于(*op).value.obj.handlers.cast_object */
			zval *org;
			TSRMLS_FETCH();

			ALLOC_ZVAL(org);
			*org = *op;
			if (Z_OBJ_HT_P(op)->cast_object(org, op, IS_NULL TSRMLS_CC) == SUCCESS) { /* 实际上调用zend_object_handers.c文件中的zend_std_cast_object_tostring(org,op,IS_NULL TSRMLS_CC) */
				zval_dtor(org);
				return;
			}
			*op = *org;
			FREE_ZVAL(org);
		}
	}

	zval_dtor(op);
	Z_TYPE_P(op) = IS_NULL;
}
/* }}} */

ZEND_API void convert_to_boolean(zval *op)/* 将数据转换成bool类型 */ /* {{{ */
{
	int tmp;

	switch (Z_TYPE_P(op)) {
		case IS_BOOL:
			break;
		case IS_NULL:
			Z_LVAL_P(op) = 0;
			break;
		case IS_RESOURCE: {
				TSRMLS_FETCH();

				zend_list_delete(Z_LVAL_P(op));
			}
			/* break missing intentionally */
		case IS_LONG:
			Z_LVAL_P(op) = (Z_LVAL_P(op) ? 1 : 0);
			break;
		case IS_DOUBLE:
			Z_LVAL_P(op) = (Z_DVAL_P(op) ? 1 : 0);
			break;
		case IS_STRING:
			{
				char *strval = Z_STRVAL_P(op);

				if (Z_STRLEN_P(op) == 0
					|| (Z_STRLEN_P(op)==1 && Z_STRVAL_P(op)[0]=='0')) {
					Z_LVAL_P(op) = 0;
				} else {
					Z_LVAL_P(op) = 1;
				}
				str_efree(strval);
			}
			break;
		case IS_ARRAY:
			tmp = (zend_hash_num_elements(Z_ARRVAL_P(op))?1:0);
			zval_dtor(op);
			Z_LVAL_P(op) = tmp;
			break;
		case IS_OBJECT:
			{
				zend_bool retval = 1;
				TSRMLS_FETCH();

				convert_object_to_type(op, IS_BOOL, convert_to_boolean);

				if (Z_TYPE_P(op) == IS_BOOL) {
					return;
				}

				zval_dtor(op);
				ZVAL_BOOL(op, retval);
				break;
			}
		default:
			zval_dtor(op);
			Z_LVAL_P(op) = 0;
			break;
	}
	Z_TYPE_P(op) = IS_BOOL;
}
/* }}} */

ZEND_API void _convert_to_cstring(zval *op ZEND_FILE_LINE_DC)/* 将数据转换成string类型 */ /* {{{ */
{
	double dval;
	switch (Z_TYPE_P(op)) {
		case IS_DOUBLE: {
			TSRMLS_FETCH();
			dval = Z_DVAL_P(op);
			Z_STRLEN_P(op) = zend_spprintf(&Z_STRVAL_P(op), 0, "%.*H", (int) EG(precision), dval);/* Z_STRLEN_P(op)等价(*op).value.str.len */
			/* %H already handles removing trailing zeros from the fractional part, yay */
			break;
		}
		default:
			_convert_to_string(op ZEND_FILE_LINE_CC);
	}
	Z_TYPE_P(op) = IS_STRING;
}
/* }}} */

ZEND_API void _convert_to_string(zval *op ZEND_FILE_LINE_DC)/* 将数据转换成string类型 */ /* {{{ */
{
	long lval;
	double dval;

	switch (Z_TYPE_P(op)) {
		case IS_NULL:
			Z_STRVAL_P(op) = STR_EMPTY_ALLOC();/* Z_STRVAL_P(op)等价(*op).value.str.val */
			Z_STRLEN_P(op) = 0;/* Z_STRLEN_P(op)等价(*op).value.str.len */
			break;
		case IS_STRING:
			break;
		case IS_BOOL:
			if (Z_LVAL_P(op)) {
				Z_STRVAL_P(op) = estrndup_rel("1", 1);/* Z_STRVAL_P(op)等价(*op).value.str.val */
				Z_STRLEN_P(op) = 1;/* Z_STRLEN_P(op)等价(*op).value.str.len */
			} else {
				Z_STRVAL_P(op) = STR_EMPTY_ALLOC();/* Z_STRVAL_P(op)等价(*op).value.str.val */
				Z_STRLEN_P(op) = 0;/* Z_STRLEN_P(op)等价(*op).value.str.len */
			}
			break;
		case IS_RESOURCE: {
			long tmp = Z_LVAL_P(op);
			TSRMLS_FETCH();

			zend_list_delete(Z_LVAL_P(op));
			Z_STRLEN_P(op) = zend_spprintf(&Z_STRVAL_P(op), 0, "Resource id #%ld", tmp);
			break;
		}
		case IS_LONG:
			lval = Z_LVAL_P(op);

			Z_STRLEN_P(op) = zend_spprintf(&Z_STRVAL_P(op), 0, "%ld", lval);/* Z_STRLEN_P(op)等价(*op).value.str.len */
			break;
		case IS_DOUBLE: {
			TSRMLS_FETCH();
			dval = Z_DVAL_P(op);
			Z_STRLEN_P(op) = zend_spprintf(&Z_STRVAL_P(op), 0, "%.*G", (int) EG(precision), dval);/* Z_STRLEN_P(op)等价(*op).value.str.len */
			/* %G already handles removing trailing zeros from the fractional part, yay */
			break;
		}
		case IS_ARRAY:
			zend_error(E_NOTICE, "Array to string conversion");
			zval_dtor(op);
			Z_STRVAL_P(op) = estrndup_rel("Array", sizeof("Array")-1);/* Z_STRVAL_P(op)等价(*op).value.str.val */
			Z_STRLEN_P(op) = sizeof("Array")-1;/* Z_STRLEN_P(op)等价(*op).value.str.len */
			break;
		case IS_OBJECT: {
			TSRMLS_FETCH();

			convert_object_to_type(op, IS_STRING, convert_to_string);

			if (Z_TYPE_P(op) == IS_STRING) {
				return;
			}

			zend_error(E_NOTICE, "Object of class %s to string conversion", Z_OBJCE_P(op)->name);
			zval_dtor(op);
			Z_STRVAL_P(op) = estrndup_rel("Object", sizeof("Object")-1);/* Z_STRVAL_P(op)等价(*op).value.str.val */
			Z_STRLEN_P(op) = sizeof("Object")-1;/* Z_STRLEN_P(op)等价(*op).value.str.len */
			break;
		}
		default:
			zval_dtor(op);
			ZVAL_BOOL(op, 0);
			break;
	}
	Z_TYPE_P(op) = IS_STRING;
}
/* }}} */

static void convert_scalar_to_array(zval *op, int type TSRMLS_DC)/* 将标量转换成array类型 */ /* {{{ */
{
	zval *entry;

	ALLOC_ZVAL(entry);
	*entry = *op;
	INIT_PZVAL(entry);

	switch (type) {
		case IS_ARRAY:
			ALLOC_HASHTABLE(Z_ARRVAL_P(op)); /* 等价(Z_ARRVAL_P(op)) = (HashTable *) emalloc(sizeof(HashTable)) */
			zend_hash_init(Z_ARRVAL_P(op), 0, NULL, ZVAL_PTR_DTOR, 0);
			zend_hash_index_update(Z_ARRVAL_P(op), 0, (void *) &entry, sizeof(zval *), NULL);
			Z_TYPE_P(op) = IS_ARRAY;
			break;
		case IS_OBJECT:
			object_init(op);
			zend_hash_update(Z_OBJPROP_P(op), "scalar", sizeof("scalar"), (void *) &entry, sizeof(zval *), NULL);
			break;
	}
}
/* }}} */

ZEND_API void convert_to_array(zval *op)/* 将数据转换成array类型 */ /* {{{ */
{
	TSRMLS_FETCH();

	switch (Z_TYPE_P(op)) {
		case IS_ARRAY: /* 如果op本来就是array类型，就不做处理 */
			break;
/* OBJECTS_OPTIMIZE */
		case IS_OBJECT:
			{
				zval *tmp;
				HashTable *ht;

				ALLOC_HASHTABLE(ht);/* 等价(ht) = (HashTable *) emalloc(sizeof(HashTable)) */
				zend_hash_init(ht, 0, NULL, ZVAL_PTR_DTOR, 0);
				if (Z_OBJCE_P(op) == zend_ce_closure) {
					convert_scalar_to_array(op, IS_ARRAY TSRMLS_CC);
					if (Z_TYPE_P(op) == IS_ARRAY) {
						zend_hash_destroy(ht);
						FREE_HASHTABLE(ht);/* 等价efree(ht) */ 
						return;
					}
				} else if (Z_OBJ_HT_P(op)->get_properties) {
					HashTable *obj_ht = Z_OBJ_HT_P(op)->get_properties(op TSRMLS_CC);
					if (obj_ht) {
						zend_hash_copy(ht, obj_ht, (copy_ctor_func_t) zval_add_ref, (void *) &tmp, sizeof(zval *));
					}
				} else {
					convert_object_to_type(op, IS_ARRAY, convert_to_array);

					if (Z_TYPE_P(op) == IS_ARRAY) {
						zend_hash_destroy(ht);
						FREE_HASHTABLE(ht);/* 等价efree(ht) */
						return;
					}
				}
				zval_dtor(op);
				Z_TYPE_P(op) = IS_ARRAY;
				Z_ARRVAL_P(op) = ht;
			}
			break;
		case IS_NULL:
			ALLOC_HASHTABLE(Z_ARRVAL_P(op));/* 等价(Z_ARRVAL_P(op)) = (HashTable *) emalloc(sizeof(HashTable)) */
			zend_hash_init(Z_ARRVAL_P(op), 0, NULL, ZVAL_PTR_DTOR, 0);
			Z_TYPE_P(op) = IS_ARRAY;
			break;
		default:
			convert_scalar_to_array(op, IS_ARRAY TSRMLS_CC);
			break;
	}
}
/* }}} */

ZEND_API void convert_to_object(zval *op)/* 将数据转换成object类型 */ /* {{{ */
{
	TSRMLS_FETCH();

	switch (Z_TYPE_P(op)) {
		case IS_ARRAY:
			{
				object_and_properties_init(op, zend_standard_class_def, Z_ARRVAL_P(op));
				break;
			}
		case IS_OBJECT:
			break;
		case IS_NULL:
			object_init(op);
			break;
		default:
			convert_scalar_to_array(op, IS_OBJECT TSRMLS_CC);
			break;
	}
}
/* }}} */

ZEND_API void multi_convert_to_long_ex(int argc, ...)/* 多个数据转换成long类型 */ /* {{{ */
{
	zval **arg;
	va_list ap; /* 定义一个 va_list 类型的变量ap */

	va_start(ap, argc); /* 然后应该对ap进行初始化，让它指向可变参数表里面的第一个参数,即argc */

	while (argc--) {
		arg = va_arg(ap, zval **); /* 然后调用va_arg获取参数，它的第一个参数是ap，第二个参数是要获取的参数的指定类型，然后返回这个指定类型的值 */
		convert_to_long_ex(arg);
	}

	va_end(ap); /* 我们有必要将这个 ap 指针关掉，以免发生危险，方法是调用 va_end，他是输入的参数 ap 置为 NULL */
}
/* }}} */

ZEND_API void multi_convert_to_double_ex(int argc, ...)/* 多个数据转换成double类型 */ /* {{{ */
{
	zval **arg;
	va_list ap; /* 同上 */

	va_start(ap, argc); /* 同上 */

	while (argc--) {
		arg = va_arg(ap, zval **); /* 同上 */
		convert_to_double_ex(arg);
	}

	va_end(ap); /* 同上 */
}
/* }}} */

ZEND_API void multi_convert_to_string_ex(int argc, ...)/* 多个数据转换成double类型 */ /* {{{ */
{
	zval **arg;
	va_list ap; /* 同上 */

	va_start(ap, argc); /* 同上 */

	while (argc--) {
		arg = va_arg(ap, zval **); /* 同上 */
		convert_to_string_ex(arg);
	}

	va_end(ap); /* 同上 */
}
/* }}} */

ZEND_API int add_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 两个值相加的处理函数 */ /* {{{ */
{
	zval op1_copy, op2_copy;
	int converted = 0;

	while (1) {
		switch (TYPE_PAIR(Z_TYPE_P(op1), Z_TYPE_P(op2))) { /* #define TYPE_PAIR(t1,t2) (((t1) << 4) | (t2)) */
			case TYPE_PAIR(IS_LONG, IS_LONG): { /* 1<<4|1等于17 */
				long lval = Z_LVAL_P(op1) + Z_LVAL_P(op2);

				/* check for overflow by comparing sign bits */
				if ((Z_LVAL_P(op1) & LONG_SIGN_MASK) == (Z_LVAL_P(op2) & LONG_SIGN_MASK)
					&& (Z_LVAL_P(op1) & LONG_SIGN_MASK) != (lval & LONG_SIGN_MASK)) { /* 没搞明白，后期研究 */

					ZVAL_DOUBLE(result, (double) Z_LVAL_P(op1) + (double) Z_LVAL_P(op2));
				} else {
					ZVAL_LONG(result, lval);/* ZVAL_LONG和ZVAL_DOUBLE这两个宏在zend_API.h中定义，主要是将数据存入op中，并且修改类型 */
				}
				return SUCCESS;
			}

			case TYPE_PAIR(IS_LONG, IS_DOUBLE): /* 1<<4|2等于18 */
				ZVAL_DOUBLE(result, ((double)Z_LVAL_P(op1)) + Z_DVAL_P(op2));
				return SUCCESS;

			case TYPE_PAIR(IS_DOUBLE, IS_LONG): /* 2<<4|1等于33 */
				ZVAL_DOUBLE(result, Z_DVAL_P(op1) + ((double)Z_LVAL_P(op2)));
				return SUCCESS;

			case TYPE_PAIR(IS_DOUBLE, IS_DOUBLE):/* 2<<4|2等于34 */
				ZVAL_DOUBLE(result, Z_DVAL_P(op1) + Z_DVAL_P(op2));
				return SUCCESS;

			case TYPE_PAIR(IS_ARRAY, IS_ARRAY): { /* 4<<4|4等于68 */
				zval *tmp;

				if ((result == op1) && (result == op2)) {
					/* $a += $a */
					return SUCCESS;
				}
				if (result != op1) {
					*result = *op1;
					zval_copy_ctor(result);
				}
				zend_hash_merge(Z_ARRVAL_P(result), Z_ARRVAL_P(op2), (void (*)(void *pData)) zval_add_ref, (void *) &tmp, sizeof(zval *), 0);
				return SUCCESS;
			}

			default:
				if (!converted) {
					ZEND_TRY_BINARY_OBJECT_OPERATION(ZEND_ADD);

					zendi_convert_scalar_to_number(op1, op1_copy, result);
					zendi_convert_scalar_to_number(op2, op2_copy, result);
					converted = 1;
				} else {
					zend_error(E_ERROR, "Unsupported operand types");
					return FAILURE; /* unknown datatype */
				}
		}
	}
}
/* }}} */

ZEND_API int sub_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 两个值相减的处理函数 */ /* {{{ */
{
	zval op1_copy, op2_copy;
	int converted = 0;

	while (1) {
		switch (TYPE_PAIR(Z_TYPE_P(op1), Z_TYPE_P(op2))) {/* 同上 */
			case TYPE_PAIR(IS_LONG, IS_LONG): {/* 同上 */
				long lval = Z_LVAL_P(op1) - Z_LVAL_P(op2);

				/* check for overflow by comparing sign bits */
				if ((Z_LVAL_P(op1) & LONG_SIGN_MASK) != (Z_LVAL_P(op2) & LONG_SIGN_MASK)
					&& (Z_LVAL_P(op1) & LONG_SIGN_MASK) != (lval & LONG_SIGN_MASK)) {/* 同上 */

					ZVAL_DOUBLE(result, (double) Z_LVAL_P(op1) - (double) Z_LVAL_P(op2));
				} else {
					ZVAL_LONG(result, lval);/* 同上 */
				}
				return SUCCESS;

			}
			case TYPE_PAIR(IS_LONG, IS_DOUBLE):/* 同上 */
				ZVAL_DOUBLE(result, ((double)Z_LVAL_P(op1)) - Z_DVAL_P(op2));
				return SUCCESS;

			case TYPE_PAIR(IS_DOUBLE, IS_LONG):/* 同上 */
				ZVAL_DOUBLE(result, Z_DVAL_P(op1) - ((double)Z_LVAL_P(op2)));
				return SUCCESS;

			case TYPE_PAIR(IS_DOUBLE, IS_DOUBLE):/* 同上 */
				ZVAL_DOUBLE(result, Z_DVAL_P(op1) - Z_DVAL_P(op2));
				return SUCCESS;

			default:
				if (!converted) {
					ZEND_TRY_BINARY_OBJECT_OPERATION(ZEND_SUB);

					zendi_convert_scalar_to_number(op1, op1_copy, result);
					zendi_convert_scalar_to_number(op2, op2_copy, result);
					converted = 1;
				} else {
					zend_error(E_ERROR, "Unsupported operand types");
					return FAILURE; /* unknown datatype */
				}
		}
	}
}
/* }}} */

ZEND_API int mul_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 两个值相乘的处理函数 */ /* {{{ */
{
	zval op1_copy, op2_copy;
	int converted = 0;

	while (1) {
		switch (TYPE_PAIR(Z_TYPE_P(op1), Z_TYPE_P(op2))) {/* 同上 */
			case TYPE_PAIR(IS_LONG, IS_LONG): {
				long overflow;
				/* ZEND_SIGNED_MULTIPLY_LONG宏定义在zend_multiply.h中 */
				ZEND_SIGNED_MULTIPLY_LONG(Z_LVAL_P(op1),Z_LVAL_P(op2), Z_LVAL_P(result),Z_DVAL_P(result),overflow);/* overflow的值会在ZEND_SIGNED_MULTIPLY_LONG中决定,其对于参数usedval */
				Z_TYPE_P(result) = overflow ? IS_DOUBLE : IS_LONG;
				return SUCCESS;

			}
			case TYPE_PAIR(IS_LONG, IS_DOUBLE):
				ZVAL_DOUBLE(result, ((double)Z_LVAL_P(op1)) * Z_DVAL_P(op2));
				return SUCCESS;

			case TYPE_PAIR(IS_DOUBLE, IS_LONG):
				ZVAL_DOUBLE(result, Z_DVAL_P(op1) * ((double)Z_LVAL_P(op2)));
				return SUCCESS;

			case TYPE_PAIR(IS_DOUBLE, IS_DOUBLE):
				ZVAL_DOUBLE(result, Z_DVAL_P(op1) * Z_DVAL_P(op2));
				return SUCCESS;

			default:
				if (!converted) {
					ZEND_TRY_BINARY_OBJECT_OPERATION(ZEND_MUL);

					zendi_convert_scalar_to_number(op1, op1_copy, result);
					zendi_convert_scalar_to_number(op2, op2_copy, result);
					converted = 1;
				} else {
					zend_error(E_ERROR, "Unsupported operand types");
					return FAILURE; /* unknown datatype */
				}
		}
	}
}
/* }}} */

ZEND_API int pow_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 两个值次方的处理函数 */ /* {{{ */
{
	zval op1_copy, op2_copy;
	int converted = 0;

	while (1) {
		switch (TYPE_PAIR(Z_TYPE_P(op1), Z_TYPE_P(op2))) {/* 同上 */
			case TYPE_PAIR(IS_LONG, IS_LONG):
				if (Z_LVAL_P(op2) >= 0) {
					long l1 = 1, l2 = Z_LVAL_P(op1), i = Z_LVAL_P(op2);

					if (i == 0) {
						ZVAL_LONG(result, 1L); /* 任何数的0次方为1 */
						return SUCCESS;
					} else if (l2 == 0) {
						ZVAL_LONG(result, 0); /* 0的任何次方为0 */
						return SUCCESS;
					}

					while (i >= 1) {
						long overflow;
						double dval = 0.0;

						if (i % 2) {
							--i;
							ZEND_SIGNED_MULTIPLY_LONG(l1, l2, l1, dval, overflow);
							if (overflow) {
								ZVAL_DOUBLE(result, dval * pow(l2, i));
								return SUCCESS;
							}
						} else {
							i /= 2;
							ZEND_SIGNED_MULTIPLY_LONG(l2, l2, l2, dval, overflow);
							if (overflow) {
								ZVAL_DOUBLE(result, (double)l1 * pow(dval, i));
								return SUCCESS;
							}
						}
					}
					/* i == 0 */
					ZVAL_LONG(result, l1);
				} else {
					ZVAL_DOUBLE(result, pow((double)Z_LVAL_P(op1), (double)Z_LVAL_P(op2)));
				}
				return SUCCESS;

			case TYPE_PAIR(IS_LONG, IS_DOUBLE):
				ZVAL_DOUBLE(result, pow((double)Z_LVAL_P(op1), Z_DVAL_P(op2)));
				return SUCCESS;

			case TYPE_PAIR(IS_DOUBLE, IS_LONG):
				ZVAL_DOUBLE(result, pow(Z_DVAL_P(op1), (double)Z_LVAL_P(op2)));
				return SUCCESS;

			case TYPE_PAIR(IS_DOUBLE, IS_DOUBLE):
				ZVAL_DOUBLE(result, pow(Z_DVAL_P(op1), Z_DVAL_P(op2)));
				return SUCCESS;

			default:
				if (!converted) {
					ZEND_TRY_BINARY_OBJECT_OPERATION(ZEND_POW);

					if (Z_TYPE_P(op1) == IS_ARRAY) {
						ZVAL_LONG(result, 0);
						return SUCCESS;
					} else {
						zendi_convert_scalar_to_number(op1, op1_copy, result);
					}
					if (Z_TYPE_P(op2) == IS_ARRAY) {
						ZVAL_LONG(result, 1L);
						return SUCCESS;
					} else {
						zendi_convert_scalar_to_number(op2, op2_copy, result);
					}
					converted = 1;
				} else {
					zend_error(E_ERROR, "Unsupported operand types");
					return FAILURE;
				}
		}
	}
}
/* }}} */

ZEND_API int div_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 两个值相除的处理函数 */ /* {{{ */
{
	zval op1_copy, op2_copy;
	int converted = 0;

	while (1) {
		switch (TYPE_PAIR(Z_TYPE_P(op1), Z_TYPE_P(op2))) {/* 同上 */
			case TYPE_PAIR(IS_LONG, IS_LONG):
				if (Z_LVAL_P(op2) == 0) {
					zend_error(E_WARNING, "Division by zero"); /* 0作为被除数时报的错误 */
					ZVAL_BOOL(result, 0);
					return FAILURE;			/* division by zero */
				} else if (Z_LVAL_P(op2) == -1 && Z_LVAL_P(op1) == LONG_MIN) {
					/* Prevent overflow error/crash */
					ZVAL_DOUBLE(result, (double) LONG_MIN / -1);
					return SUCCESS;
				}
				if (Z_LVAL_P(op1) % Z_LVAL_P(op2) == 0) { /* integer */
					ZVAL_LONG(result, Z_LVAL_P(op1) / Z_LVAL_P(op2)); /* 如果能被整除，则为LONG类型，否则为double型 */
				} else {
					ZVAL_DOUBLE(result, ((double) Z_LVAL_P(op1)) / Z_LVAL_P(op2));
				}
				return SUCCESS;

			case TYPE_PAIR(IS_DOUBLE, IS_LONG):
				if (Z_LVAL_P(op2) == 0) {
					zend_error(E_WARNING, "Division by zero");
					ZVAL_BOOL(result, 0);
					return FAILURE;			/* division by zero */
				}
				ZVAL_DOUBLE(result, Z_DVAL_P(op1) / (double)Z_LVAL_P(op2));
				return SUCCESS;

			case TYPE_PAIR(IS_LONG, IS_DOUBLE):
				if (Z_DVAL_P(op2) == 0) {
					zend_error(E_WARNING, "Division by zero");
					ZVAL_BOOL(result, 0);
					return FAILURE;			/* division by zero */
				}
				ZVAL_DOUBLE(result, (double)Z_LVAL_P(op1) / Z_DVAL_P(op2));
				return SUCCESS;

			case TYPE_PAIR(IS_DOUBLE, IS_DOUBLE):
				if (Z_DVAL_P(op2) == 0) {
					zend_error(E_WARNING, "Division by zero");
					ZVAL_BOOL(result, 0);
					return FAILURE;			/* division by zero */
				}
				ZVAL_DOUBLE(result, Z_DVAL_P(op1) / Z_DVAL_P(op2));
				return SUCCESS;

			default:
				if (!converted) {
					ZEND_TRY_BINARY_OBJECT_OPERATION(ZEND_DIV);

					zendi_convert_scalar_to_number(op1, op1_copy, result);
					zendi_convert_scalar_to_number(op2, op2_copy, result);
					converted = 1;
				} else {
					zend_error(E_ERROR, "Unsupported operand types");
					return FAILURE; /* unknown datatype */
				}
		}
	}
}
/* }}} */

ZEND_API int mod_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 两个值求余的处理函数 */ /* {{{ */
{
	zval op1_copy, op2_copy;
	long op1_lval;

	if (Z_TYPE_P(op1) != IS_LONG || Z_TYPE_P(op2) != IS_LONG) {
		ZEND_TRY_BINARY_OBJECT_OPERATION(ZEND_MOD);

		zendi_convert_to_long(op1, op1_copy, result);
		op1_lval = Z_LVAL_P(op1);
		zendi_convert_to_long(op2, op2_copy, result);
	} else {
		op1_lval = Z_LVAL_P(op1);
	}

	if (Z_LVAL_P(op2) == 0) {
		zend_error(E_WARNING, "Division by zero");
		ZVAL_BOOL(result, 0);
		return FAILURE;			/* modulus by zero */
	}

	if (Z_LVAL_P(op2) == -1) {
		/* Prevent overflow error/crash if op1==LONG_MIN */
		ZVAL_LONG(result, 0);
		return SUCCESS;
	}

	ZVAL_LONG(result, op1_lval % Z_LVAL_P(op2));
	return SUCCESS;
}
/* }}} */

ZEND_API int boolean_xor_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 两个布尔值异或的处理函数 */ /* {{{ */
{
	zval op1_copy, op2_copy;
	long op1_lval;

	if (Z_TYPE_P(op1) != IS_BOOL || Z_TYPE_P(op2) != IS_BOOL) {
		ZEND_TRY_BINARY_OBJECT_OPERATION(ZEND_BOOL_XOR);

		zendi_convert_to_boolean(op1, op1_copy, result);
		op1_lval = Z_LVAL_P(op1);
		zendi_convert_to_boolean(op2, op2_copy, result);
	} else {
		op1_lval = Z_LVAL_P(op1);
	}

	ZVAL_BOOL(result, op1_lval ^ Z_LVAL_P(op2));
	return SUCCESS;
}
/* }}} */

ZEND_API int boolean_not_function(zval *result, zval *op1 TSRMLS_DC)/* 布尔值取非的处理函数 */ /* {{{ */
{
	zval op1_copy;

	if (Z_TYPE_P(op1) != IS_BOOL) {
		ZEND_TRY_UNARY_OBJECT_OPERATION(ZEND_BOOL_NOT);

		zendi_convert_to_boolean(op1, op1_copy, result);
	}

	ZVAL_BOOL(result, !Z_LVAL_P(op1));
	return SUCCESS;
}
/* }}} */

ZEND_API int bitwise_not_function(zval *result, zval *op1 TSRMLS_DC)/* 取反的处理函数 */ /* {{{ */
{

	switch (Z_TYPE_P(op1)) {
		case IS_LONG:
			ZVAL_LONG(result, ~Z_LVAL_P(op1));
			return SUCCESS;
		case IS_DOUBLE:
			ZVAL_LONG(result, ~zend_dval_to_lval(Z_DVAL_P(op1)));
			return SUCCESS;
		case IS_STRING: {
			int i;
			zval op1_copy = *op1;

			Z_TYPE_P(result) = IS_STRING;
			Z_STRVAL_P(result) = estrndup(Z_STRVAL(op1_copy), Z_STRLEN(op1_copy));
			Z_STRLEN_P(result) = Z_STRLEN(op1_copy);
			for (i = 0; i < Z_STRLEN(op1_copy); i++) {
				Z_STRVAL_P(result)[i] = ~Z_STRVAL(op1_copy)[i];
			}
			return SUCCESS;
		}
		default:
			ZEND_TRY_UNARY_OBJECT_OPERATION(ZEND_BW_NOT);

			zend_error(E_ERROR, "Unsupported operand types"); /* 比如:echo ~array(1,2,3);这样的代码 */
			return FAILURE;
	}
}
/* }}} */

ZEND_API int bitwise_or_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 两个值逻辑或的处理函数 */ /* {{{ */
{
	zval op1_copy, op2_copy;
	long op1_lval;

	if (Z_TYPE_P(op1) == IS_STRING && Z_TYPE_P(op2) == IS_STRING) {
		zval *longer, *shorter;
		char *result_str;
		int i, result_len;

		if (Z_STRLEN_P(op1) >= Z_STRLEN_P(op2)) {
			longer = op1;
			shorter = op2;
		} else {
			longer = op2;
			shorter = op1;
		}

		Z_TYPE_P(result) = IS_STRING;
		result_len = Z_STRLEN_P(longer);
		result_str = estrndup(Z_STRVAL_P(longer), Z_STRLEN_P(longer));
		for (i = 0; i < Z_STRLEN_P(shorter); i++) {
			result_str[i] |= Z_STRVAL_P(shorter)[i];
		}
		if (result==op1) {
			str_efree(Z_STRVAL_P(result));
		}
		Z_STRVAL_P(result) = result_str;
		Z_STRLEN_P(result) = result_len;
		return SUCCESS;
	}

	if (Z_TYPE_P(op1) != IS_LONG || Z_TYPE_P(op2) != IS_LONG) {
		ZEND_TRY_BINARY_OBJECT_OPERATION(ZEND_BW_OR);

		zendi_convert_to_long(op1, op1_copy, result);
		op1_lval = Z_LVAL_P(op1);
		zendi_convert_to_long(op2, op2_copy, result);
	} else {
		op1_lval = Z_LVAL_P(op1);
	}

	ZVAL_LONG(result, op1_lval | Z_LVAL_P(op2));
	return SUCCESS;
}
/* }}} */

ZEND_API int bitwise_and_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 两个值逻辑且的处理函数 */ /* {{{ */
{
	zval op1_copy, op2_copy;
	long op1_lval;

	if (Z_TYPE_P(op1) == IS_STRING && Z_TYPE_P(op2) == IS_STRING) {
		zval *longer, *shorter;
		char *result_str;
		int i, result_len;

		if (Z_STRLEN_P(op1) >= Z_STRLEN_P(op2)) {
			longer = op1;
			shorter = op2;
		} else {
			longer = op2;
			shorter = op1;
		}

		Z_TYPE_P(result) = IS_STRING;
		result_len = Z_STRLEN_P(shorter);
		result_str = estrndup(Z_STRVAL_P(shorter), Z_STRLEN_P(shorter));
		for (i = 0; i < Z_STRLEN_P(shorter); i++) {
			result_str[i] &= Z_STRVAL_P(longer)[i];
		}
		if (result==op1) {
			str_efree(Z_STRVAL_P(result));
		}
		Z_STRVAL_P(result) = result_str;
		Z_STRLEN_P(result) = result_len;
		return SUCCESS;
	}

	if (Z_TYPE_P(op1) != IS_LONG || Z_TYPE_P(op2) != IS_LONG) {
		ZEND_TRY_BINARY_OBJECT_OPERATION(ZEND_BW_AND);

		zendi_convert_to_long(op1, op1_copy, result);
		op1_lval = Z_LVAL_P(op1);
		zendi_convert_to_long(op2, op2_copy, result);
	} else {
		op1_lval = Z_LVAL_P(op1);
	}

	ZVAL_LONG(result, op1_lval & Z_LVAL_P(op2));
	return SUCCESS;
}
/* }}} */

ZEND_API int bitwise_xor_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 两个值异或的处理函数 */ /* {{{ */
{
	zval op1_copy, op2_copy;
	long op1_lval;

	if (Z_TYPE_P(op1) == IS_STRING && Z_TYPE_P(op2) == IS_STRING) {
		zval *longer, *shorter;
		char *result_str;
		int i, result_len;

		if (Z_STRLEN_P(op1) >= Z_STRLEN_P(op2)) {
			longer = op1;
			shorter = op2;
		} else {
			longer = op2;
			shorter = op1;
		}

		Z_TYPE_P(result) = IS_STRING;
		result_len = Z_STRLEN_P(shorter);
		result_str = estrndup(Z_STRVAL_P(shorter), Z_STRLEN_P(shorter));
		for (i = 0; i < Z_STRLEN_P(shorter); i++) {
			result_str[i] ^= Z_STRVAL_P(longer)[i];
		}
		if (result==op1) {
			str_efree(Z_STRVAL_P(result));
		}
		Z_STRVAL_P(result) = result_str;
		Z_STRLEN_P(result) = result_len;
		return SUCCESS;
	}

	if (Z_TYPE_P(op1) != IS_LONG || Z_TYPE_P(op2) != IS_LONG) {
		ZEND_TRY_BINARY_OBJECT_OPERATION(ZEND_BW_XOR);

		zendi_convert_to_long(op1, op1_copy, result);
		op1_lval = Z_LVAL_P(op1);
		zendi_convert_to_long(op2, op2_copy, result);
	} else {
		op1_lval = Z_LVAL_P(op1);
	}

	ZVAL_LONG(result, op1_lval ^ Z_LVAL_P(op2));
	return SUCCESS;
}
/* }}} */

ZEND_API int shift_left_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 按位左移处理函数 */ /* {{{ */
{
	zval op1_copy, op2_copy;
	long op1_lval;

	if (Z_TYPE_P(op1) != IS_LONG || Z_TYPE_P(op2) != IS_LONG) {
		ZEND_TRY_BINARY_OBJECT_OPERATION(ZEND_SL);

		zendi_convert_to_long(op1, op1_copy, result);
		op1_lval = Z_LVAL_P(op1);
		zendi_convert_to_long(op2, op2_copy, result);
	} else {
		op1_lval = Z_LVAL_P(op1);
	}

	ZVAL_LONG(result, op1_lval << Z_LVAL_P(op2));
	return SUCCESS;
}
/* }}} */

ZEND_API int shift_right_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 按位右移处理函数 */ /* {{{ */
{
	zval op1_copy, op2_copy;
	long op1_lval;

	if (Z_TYPE_P(op1) != IS_LONG || Z_TYPE_P(op2) != IS_LONG) {
		ZEND_TRY_BINARY_OBJECT_OPERATION(ZEND_SR);

		zendi_convert_to_long(op1, op1_copy, result);
		op1_lval = Z_LVAL_P(op1);
		zendi_convert_to_long(op2, op2_copy, result);
	} else {
		op1_lval = Z_LVAL_P(op1);
	}

	ZVAL_LONG(result, op1_lval >> Z_LVAL_P(op2));
	return SUCCESS;
}
/* }}} */

/* must support result==op1 */
ZEND_API int add_char_to_string(zval *result, const zval *op1, const zval *op2)/* 追加字符到字符串的处理函数 */ /* {{{ */
{
	int length = Z_STRLEN_P(op1) + 1;
	char *buf = str_erealloc(Z_STRVAL_P(op1), length + 1);

	buf[length - 1] = (char) Z_LVAL_P(op2);
	buf[length] = 0;
	ZVAL_STRINGL(result, buf, length, 0);
	return SUCCESS;
}
/* }}} */

/* must support result==op1 */
ZEND_API int add_string_to_string(zval *result, const zval *op1, const zval *op2)/* 追加字符串到字符串的处理函数 */ /* {{{ */
{
	int length = Z_STRLEN_P(op1) + Z_STRLEN_P(op2);
	char *buf = str_erealloc(Z_STRVAL_P(op1), length + 1);

	memcpy(buf + Z_STRLEN_P(op1), Z_STRVAL_P(op2), Z_STRLEN_P(op2));
	buf[length] = 0;
	ZVAL_STRINGL(result, buf, length, 0);
	return SUCCESS;
}
/* }}} */

ZEND_API int concat_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 两个值合并的处理函数 */ /* {{{ */
{
	zval op1_copy, op2_copy;
	int use_copy1 = 0, use_copy2 = 0;

	if (Z_TYPE_P(op1) != IS_STRING || Z_TYPE_P(op2) != IS_STRING) {
		ZEND_TRY_BINARY_OBJECT_OPERATION(ZEND_CONCAT);

		if (Z_TYPE_P(op1) != IS_STRING) {
			zend_make_printable_zval(op1, &op1_copy, &use_copy1);/* 将op1生成可打印的值 */
		}
		if (Z_TYPE_P(op2) != IS_STRING) {
			zend_make_printable_zval(op2, &op2_copy, &use_copy2);/* 将op2生成可打印的值 */
		}
	}

	if (use_copy1) {
		/* We have created a converted copy of op1. Therefore, op1 won't become the result so
		 * we have to free it.
		 */
		if (result == op1) {
			zval_dtor(op1);
		}
		op1 = &op1_copy;
	}
	if (use_copy2) {
		op2 = &op2_copy;
	}
	if (result==op1 && !IS_INTERNED(Z_STRVAL_P(op1))) {	/* special case, perform operations on result */
		uint res_len = Z_STRLEN_P(op1) + Z_STRLEN_P(op2);

		if (Z_STRLEN_P(result) < 0 || (int) (Z_STRLEN_P(op1) + Z_STRLEN_P(op2)) < 0) {
			efree(Z_STRVAL_P(result));
			ZVAL_EMPTY_STRING(result);
			zend_error(E_ERROR, "String size overflow");
		}

		Z_STRVAL_P(result) = safe_erealloc(Z_STRVAL_P(result), res_len, 1, 1);

		memcpy(Z_STRVAL_P(result)+Z_STRLEN_P(result), Z_STRVAL_P(op2), Z_STRLEN_P(op2));
		Z_STRVAL_P(result)[res_len]=0;
		Z_STRLEN_P(result) = res_len;
	} else {
		int length = Z_STRLEN_P(op1) + Z_STRLEN_P(op2);
		char *buf;

		if (Z_STRLEN_P(op1) < 0 || Z_STRLEN_P(op2) < 0 || (int) (Z_STRLEN_P(op1) + Z_STRLEN_P(op2)) < 0) {
			zend_error(E_ERROR, "String size overflow");
		}
		buf = (char *) safe_emalloc(length, 1, 1);

		memcpy(buf, Z_STRVAL_P(op1), Z_STRLEN_P(op1));
		memcpy(buf + Z_STRLEN_P(op1), Z_STRVAL_P(op2), Z_STRLEN_P(op2));
		buf[length] = 0;
		ZVAL_STRINGL(result, buf, length, 0);
	}
	if (use_copy1) {
		zval_dtor(op1);
	}
	if (use_copy2) {
		zval_dtor(op2);
	}
	return SUCCESS;
}
/* }}} */

ZEND_API int string_compare_function_ex(zval *result, zval *op1, zval *op2, zend_bool case_insensitive TSRMLS_DC)/* 比较两个字符串是否相等，case_insensitive是否为不区分大小写形式 */ /* {{{ */
{
	zval op1_copy, op2_copy;
	int use_copy1 = 0, use_copy2 = 0;

	if (Z_TYPE_P(op1) != IS_STRING) {
		zend_make_printable_zval(op1, &op1_copy, &use_copy1);
	}
	if (Z_TYPE_P(op2) != IS_STRING) {
		zend_make_printable_zval(op2, &op2_copy, &use_copy2);
	}

	if (use_copy1) {
		op1 = &op1_copy;
	}
	if (use_copy2) {
		op2 = &op2_copy;
	}

	if (case_insensitive) {
		ZVAL_LONG(result, zend_binary_zval_strcasecmp(op1, op2)); /* case insensitive不区分大小写,不区分大小写比较op1与op2 */
	} else {
		ZVAL_LONG(result, zend_binary_zval_strcmp(op1, op2)); /* 区分大小写比较op1与op2 */
	}

	if (use_copy1) {
		zval_dtor(op1);
	}
	if (use_copy2) {
		zval_dtor(op2);
	}
	return SUCCESS;
}
/* }}} */

ZEND_API int string_compare_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 大小写敏感比较字符串 */ /* {{{ */
{
	return string_compare_function_ex(result, op1, op2, 0 TSRMLS_CC);
}
/* }}} */

ZEND_API int string_case_compare_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 大小写不敏感比较字符串 */ /* {{{ */
{
	return string_compare_function_ex(result, op1, op2, 1 TSRMLS_CC);
}
/* }}} */

#if HAVE_STRCOLL
ZEND_API int string_locale_compare_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 与 string_compare_function 函数一样 */ /* {{{ */
{
	zval op1_copy, op2_copy;
	int use_copy1 = 0, use_copy2 = 0;

	if (Z_TYPE_P(op1) != IS_STRING) {
		zend_make_printable_zval(op1, &op1_copy, &use_copy1);
	}
	if (Z_TYPE_P(op2) != IS_STRING) {
		zend_make_printable_zval(op2, &op2_copy, &use_copy2);
	}

	if (use_copy1) {
		op1 = &op1_copy;
	}
	if (use_copy2) {
		op2 = &op2_copy;
	}

	ZVAL_LONG(result, strcoll(Z_STRVAL_P(op1), Z_STRVAL_P(op2)));/* strcoll() 函数根据环境变量LC_COLLATE来比较字符串,若字符串 str1 和 str2 相同则返回0。若 str1 大于 str2 则返回大于 0 的值，否则返回小于 0 的值 */

	if (use_copy1) {
		zval_dtor(op1);
	}
	if (use_copy2) {
		zval_dtor(op2);
	}
	return SUCCESS;
}
/* }}} */
#endif

ZEND_API int numeric_compare_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 比较数值的处理函数 */ /* {{{ */
{
	zval op1_copy, op2_copy;

	op1_copy = *op1;
	zval_copy_ctor(&op1_copy);

	op2_copy = *op2;
	zval_copy_ctor(&op2_copy);

	convert_to_double(&op1_copy);
	convert_to_double(&op2_copy);

	ZVAL_LONG(result, ZEND_NORMALIZE_BOOL(Z_DVAL(op1_copy)-Z_DVAL(op2_copy)));/* ZEND_NORMALIZE_BOOL(n)等价((n) ? (((n)>0) ? 1 : -1) : 0) 将n取-1，0，1三种情况 */

	return SUCCESS;
}
/* }}} */

static inline void zend_free_obj_get_result(zval *op TSRMLS_DC) /* {{{ */
{
	if (Z_REFCOUNT_P(op) == 0) {
		GC_REMOVE_ZVAL_FROM_BUFFER(op);
		zval_dtor(op);
		FREE_ZVAL(op);
	} else {
		zval_ptr_dtor(&op);
	}
}
/* }}} */

ZEND_API int compare_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 通用类型比较处理函数 */ /* {{{ */
{
	int ret;
	int converted = 0;
	zval op1_copy, op2_copy;
	zval *op_free;

	while (1) {
		switch (TYPE_PAIR(Z_TYPE_P(op1), Z_TYPE_P(op2))) {
			case TYPE_PAIR(IS_LONG, IS_LONG):
				ZVAL_LONG(result, Z_LVAL_P(op1)>Z_LVAL_P(op2)?1:(Z_LVAL_P(op1)<Z_LVAL_P(op2)?-1:0));
				return SUCCESS;

			case TYPE_PAIR(IS_DOUBLE, IS_LONG):
				Z_DVAL_P(result) = Z_DVAL_P(op1) - (double)Z_LVAL_P(op2);
				ZVAL_LONG(result, ZEND_NORMALIZE_BOOL(Z_DVAL_P(result)));
				return SUCCESS;

			case TYPE_PAIR(IS_LONG, IS_DOUBLE):
				Z_DVAL_P(result) = (double)Z_LVAL_P(op1) - Z_DVAL_P(op2);
				ZVAL_LONG(result, ZEND_NORMALIZE_BOOL(Z_DVAL_P(result)));
				return SUCCESS;

			case TYPE_PAIR(IS_DOUBLE, IS_DOUBLE):
				if (Z_DVAL_P(op1) == Z_DVAL_P(op2)) {
					ZVAL_LONG(result, 0);
				} else {
					Z_DVAL_P(result) = Z_DVAL_P(op1) - Z_DVAL_P(op2);
					ZVAL_LONG(result, ZEND_NORMALIZE_BOOL(Z_DVAL_P(result)));
				}
				return SUCCESS;

			case TYPE_PAIR(IS_ARRAY, IS_ARRAY):
				zend_compare_arrays(result, op1, op2 TSRMLS_CC);
				return SUCCESS;

			case TYPE_PAIR(IS_NULL, IS_NULL):
				ZVAL_LONG(result, 0);
				return SUCCESS;

			case TYPE_PAIR(IS_NULL, IS_BOOL):
				ZVAL_LONG(result, Z_LVAL_P(op2) ? -1 : 0);
				return SUCCESS;

			case TYPE_PAIR(IS_BOOL, IS_NULL):
				ZVAL_LONG(result, Z_LVAL_P(op1) ? 1 : 0);
				return SUCCESS;

			case TYPE_PAIR(IS_BOOL, IS_BOOL):
				ZVAL_LONG(result, ZEND_NORMALIZE_BOOL(Z_LVAL_P(op1) - Z_LVAL_P(op2)));
				return SUCCESS;

			case TYPE_PAIR(IS_STRING, IS_STRING):
				zendi_smart_strcmp(result, op1, op2);
				return SUCCESS;

			case TYPE_PAIR(IS_NULL, IS_STRING):
				ZVAL_LONG(result, zend_binary_strcmp("", 0, Z_STRVAL_P(op2), Z_STRLEN_P(op2)));
				return SUCCESS;

			case TYPE_PAIR(IS_STRING, IS_NULL):
				ZVAL_LONG(result, zend_binary_strcmp(Z_STRVAL_P(op1), Z_STRLEN_P(op1), "", 0));
				return SUCCESS;

			case TYPE_PAIR(IS_OBJECT, IS_NULL):
				ZVAL_LONG(result, 1);
				return SUCCESS;

			case TYPE_PAIR(IS_NULL, IS_OBJECT):
				ZVAL_LONG(result, -1);
				return SUCCESS;

			default:
				if (Z_TYPE_P(op1) == IS_OBJECT && Z_OBJ_HANDLER_P(op1, compare)) {
					return Z_OBJ_HANDLER_P(op1, compare)(result, op1, op2 TSRMLS_CC);
				} else if (Z_TYPE_P(op2) == IS_OBJECT && Z_OBJ_HANDLER_P(op2, compare)) {
					return Z_OBJ_HANDLER_P(op2, compare)(result, op1, op2 TSRMLS_CC);
				}

				if (Z_TYPE_P(op1) == IS_OBJECT && Z_TYPE_P(op2) == IS_OBJECT) {
					if (Z_OBJ_HANDLE_P(op1) == Z_OBJ_HANDLE_P(op2)) {
						/* object handles are identical, apparently this is the same object */
						ZVAL_LONG(result, 0);
						return SUCCESS;
					}
					if (Z_OBJ_HANDLER_P(op1, compare_objects) == Z_OBJ_HANDLER_P(op2, compare_objects)) {
						ZVAL_LONG(result, Z_OBJ_HANDLER_P(op1, compare_objects)(op1, op2 TSRMLS_CC));
						return SUCCESS;
					}
				}
				if (Z_TYPE_P(op1) == IS_OBJECT) {
					if (Z_OBJ_HT_P(op1)->get) {
						op_free = Z_OBJ_HT_P(op1)->get(op1 TSRMLS_CC);
						ret = compare_function(result, op_free, op2 TSRMLS_CC);
						zend_free_obj_get_result(op_free TSRMLS_CC);
						return ret;
					} else if (Z_TYPE_P(op2) != IS_OBJECT && Z_OBJ_HT_P(op1)->cast_object) {
						ALLOC_INIT_ZVAL(op_free);
						if (Z_OBJ_HT_P(op1)->cast_object(op1, op_free, Z_TYPE_P(op2) TSRMLS_CC) == FAILURE) {
							ZVAL_LONG(result, 1);
							zend_free_obj_get_result(op_free TSRMLS_CC);
							return SUCCESS;
						}
						ret = compare_function(result, op_free, op2 TSRMLS_CC);
						zend_free_obj_get_result(op_free TSRMLS_CC);
						return ret;
					}
				}
				if (Z_TYPE_P(op2) == IS_OBJECT) {
					if (Z_OBJ_HT_P(op2)->get) {
						op_free = Z_OBJ_HT_P(op2)->get(op2 TSRMLS_CC);
						ret = compare_function(result, op1, op_free TSRMLS_CC);
						zend_free_obj_get_result(op_free TSRMLS_CC);
						return ret;
					} else if (Z_TYPE_P(op1) != IS_OBJECT && Z_OBJ_HT_P(op2)->cast_object) {
						ALLOC_INIT_ZVAL(op_free);
						if (Z_OBJ_HT_P(op2)->cast_object(op2, op_free, Z_TYPE_P(op1) TSRMLS_CC) == FAILURE) {
							ZVAL_LONG(result, -1);
							zend_free_obj_get_result(op_free TSRMLS_CC);
							return SUCCESS;
						}
						ret = compare_function(result, op1, op_free TSRMLS_CC);
						zend_free_obj_get_result(op_free TSRMLS_CC);
						return ret;
					} else if (Z_TYPE_P(op1) == IS_OBJECT) {
						ZVAL_LONG(result, 1);
						return SUCCESS;
					}
				}
				if (!converted) {
					if (Z_TYPE_P(op1) == IS_NULL) {
						zendi_convert_to_boolean(op2, op2_copy, result);
						ZVAL_LONG(result, Z_LVAL_P(op2) ? -1 : 0);
						return SUCCESS;
					} else if (Z_TYPE_P(op2) == IS_NULL) {
						zendi_convert_to_boolean(op1, op1_copy, result);
						ZVAL_LONG(result, Z_LVAL_P(op1) ? 1 : 0);
						return SUCCESS;
					} else if (Z_TYPE_P(op1) == IS_BOOL) {
						zendi_convert_to_boolean(op2, op2_copy, result);
						ZVAL_LONG(result, ZEND_NORMALIZE_BOOL(Z_LVAL_P(op1) - Z_LVAL_P(op2)));
						return SUCCESS;
					} else if (Z_TYPE_P(op2) == IS_BOOL) {
						zendi_convert_to_boolean(op1, op1_copy, result);
						ZVAL_LONG(result, ZEND_NORMALIZE_BOOL(Z_LVAL_P(op1) - Z_LVAL_P(op2)));
						return SUCCESS;
					} else {
						zendi_convert_scalar_to_number(op1, op1_copy, result);
						zendi_convert_scalar_to_number(op2, op2_copy, result);
						converted = 1;
					}
				} else if (Z_TYPE_P(op1)==IS_ARRAY) {
					ZVAL_LONG(result, 1);
					return SUCCESS;
				} else if (Z_TYPE_P(op2)==IS_ARRAY) {
					ZVAL_LONG(result, -1);
					return SUCCESS;
				} else if (Z_TYPE_P(op1)==IS_OBJECT) {
					ZVAL_LONG(result, 1);
					return SUCCESS;
				} else if (Z_TYPE_P(op2)==IS_OBJECT) {
					ZVAL_LONG(result, -1);
					return SUCCESS;
				} else {
					ZVAL_LONG(result, 0);
					return FAILURE;
				}
		}
	}
}
/* }}} */

static int hash_zval_identical_function(const zval **z1, const zval **z2)/* 比较两个值是否完全相同，与下面的函数不同，这里返回0表示完全相同 */ /* {{{ */
{
	zval result;
	TSRMLS_FETCH();

	/* is_identical_function() returns 1 in case of identity and 0 in case
	 * of a difference;
	 * whereas this comparison function is expected to return 0 on identity,
	 * and non zero otherwise.
	 */
	if (is_identical_function(&result, (zval *) *z1, (zval *) *z2 TSRMLS_CC)==FAILURE) {
		return 1;
	}
	return !Z_LVAL(result);
}
/* }}} */

ZEND_API int is_identical_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 比较两个值是否完全相同，相同返回1，identical完全相同的意思 */ /* {{{ */
{
	Z_TYPE_P(result) = IS_BOOL;
	if (Z_TYPE_P(op1) != Z_TYPE_P(op2)) {
		Z_LVAL_P(result) = 0;/* 类型都不相同，直接返回0 */
		return SUCCESS;
	}/* 下面都是类型相同的情况 */
	switch (Z_TYPE_P(op1)) {
		case IS_NULL:
			Z_LVAL_P(result) = 1;/* NULL类型只有一个值，所以相同 */
			break;
		case IS_BOOL:
		case IS_LONG:
		case IS_RESOURCE:
			Z_LVAL_P(result) = (Z_LVAL_P(op1) == Z_LVAL_P(op2));
			break;
		case IS_DOUBLE:
			Z_LVAL_P(result) = (Z_DVAL_P(op1) == Z_DVAL_P(op2));
			break;
		case IS_STRING:
			Z_LVAL_P(result) = ((Z_STRLEN_P(op1) == Z_STRLEN_P(op2))
				&& (!memcmp(Z_STRVAL_P(op1), Z_STRVAL_P(op2), Z_STRLEN_P(op1))));/* 长度相同，且内容相同是返回1 */
			break;
		case IS_ARRAY:
			Z_LVAL_P(result) = (Z_ARRVAL_P(op1) == Z_ARRVAL_P(op2) ||
				zend_hash_compare(Z_ARRVAL_P(op1), Z_ARRVAL_P(op2), (compare_func_t) hash_zval_identical_function, 1 TSRMLS_CC)==0);
			break;
		case IS_OBJECT:
			if (Z_OBJ_HT_P(op1) == Z_OBJ_HT_P(op2)) {
				Z_LVAL_P(result) = (Z_OBJ_HANDLE_P(op1) == Z_OBJ_HANDLE_P(op2));
			} else {
				Z_LVAL_P(result) = 0;
			}
			break;
		default:
			Z_LVAL_P(result) = 0;
			return FAILURE;
	}
	return SUCCESS;
}
/* }}} */

ZEND_API int is_not_identical_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 比较两个值是否不完全相同，不完全相同返回1 */ /* {{{ */
{
	if (is_identical_function(result, op1, op2 TSRMLS_CC) == FAILURE) {
		return FAILURE;
	}
	Z_LVAL_P(result) = !Z_LVAL_P(result);
	return SUCCESS;
}
/* }}} */

ZEND_API int is_equal_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 比较两个值是否相等 */ /* {{{ */
{
	if (compare_function(result, op1, op2 TSRMLS_CC) == FAILURE) {
		return FAILURE;
	}
	ZVAL_BOOL(result, (Z_LVAL_P(result) == 0));
	return SUCCESS;
}
/* }}} */

ZEND_API int is_not_equal_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 比较两个值是否不相等 */ /* {{{ */
{
	if (compare_function(result, op1, op2 TSRMLS_CC) == FAILURE) {
		return FAILURE;
	}
	ZVAL_BOOL(result, (Z_LVAL_P(result) != 0));
	return SUCCESS;
}
/* }}} */

ZEND_API int is_smaller_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 比较op1是否小于op2 */  /* {{{ */
{
	if (compare_function(result, op1, op2 TSRMLS_CC) == FAILURE) {
		return FAILURE;
	}
	ZVAL_BOOL(result, (Z_LVAL_P(result) < 0));
	return SUCCESS;
}
/* }}} */

ZEND_API int is_smaller_or_equal_function(zval *result, zval *op1, zval *op2 TSRMLS_DC)/* 比较op1是否小于或等于op2 */ /* {{{ */
{
	if (compare_function(result, op1, op2 TSRMLS_CC) == FAILURE) {
		return FAILURE;
	}
	ZVAL_BOOL(result, (Z_LVAL_P(result) <= 0));
	return SUCCESS;
}
/* }}} */

ZEND_API zend_bool instanceof_function_ex(const zend_class_entry *instance_ce, const zend_class_entry *ce, zend_bool interfaces_only TSRMLS_DC) /* {{{ */
{
	zend_uint i;

	for (i=0; i<instance_ce->num_interfaces; i++) {
		if (instanceof_function(instance_ce->interfaces[i], ce TSRMLS_CC)) {
			return 1;
		}
	}
	if (!interfaces_only) {
		while (instance_ce) {
			if (instance_ce == ce) {
				return 1;
			}
			instance_ce = instance_ce->parent;
		}
	}

	return 0;
}
/* }}} */

ZEND_API zend_bool instanceof_function(const zend_class_entry *instance_ce, const zend_class_entry *ce TSRMLS_DC) /* {{{ */
{
	return instanceof_function_ex(instance_ce, ce, 0 TSRMLS_CC);
}
/* }}} */

#define LOWER_CASE 1
#define UPPER_CASE 2
#define NUMERIC 3

static void increment_string(zval *str)/* 字符串增量处理函数，$a="zz"; $a++; var_dump($a); 的输出结果为string(3) "aaa"; */ /* {{{ */
{
	int carry=0;/* carry表示是非有向前进位 */
	int pos=Z_STRLEN_P(str)-1;
	char *s=Z_STRVAL_P(str);
	char *t;
	int last=0; /* Shut up the compiler warning */
	int ch;

	if (Z_STRLEN_P(str) == 0) {
		str_efree(Z_STRVAL_P(str));
		Z_STRVAL_P(str) = estrndup("1", sizeof("1")-1);
		Z_STRLEN_P(str) = 1; /* 这就是为什么$a=""; $a++; var_dump($a); 的输出结果为string(1) "1"; */
		return;
	}

	if (IS_INTERNED(s)) {
		Z_STRVAL_P(str) = s = estrndup(s, Z_STRLEN_P(str));
	}

	while (pos >= 0) {
		ch = s[pos];
		if (ch >= 'a' && ch <= 'z') {
			if (ch == 'z') {
				s[pos] = 'a';
				carry=1;
			} else {
				s[pos]++;
				carry=0;
			}
			last=LOWER_CASE;
		} else if (ch >= 'A' && ch <= 'Z') {
			if (ch == 'Z') {
				s[pos] = 'A';
				carry=1;
			} else {
				s[pos]++;
				carry=0;
			}
			last=UPPER_CASE;
		} else if (ch >= '0' && ch <= '9') {
			if (ch == '9') {
				s[pos] = '0';
				carry=1;
			} else {
				s[pos]++;
				carry=0;
			}
			last = NUMERIC;
		} else {
			carry=0;
			break;
		}
		if (carry == 0) {
			break;
		}
		pos--;
	}

	if (carry) {
		t = (char *) emalloc(Z_STRLEN_P(str)+1+1);
		memcpy(t+1, Z_STRVAL_P(str), Z_STRLEN_P(str));
		Z_STRLEN_P(str)++;
		t[Z_STRLEN_P(str)] = '\0';
		switch (last) {
			case NUMERIC:
				t[0] = '1';
				break;
			case UPPER_CASE:
				t[0] = 'A';
				break;
			case LOWER_CASE:
				t[0] = 'a';
				break;
		}
		str_efree(Z_STRVAL_P(str));
		Z_STRVAL_P(str) = t;
	}
}
/* }}} */

ZEND_API int increment_function(zval *op1)/* 增量处理函数，例如代码：$a="123"; $a++; var_dump($a); 输出结果int(124); */ /* {{{ */
{
	switch (Z_TYPE_P(op1)) {
		case IS_LONG:
			if (Z_LVAL_P(op1) == LONG_MAX) { /* 这一段很好理解，如果op1的值已经达到LONG取值范围的最大值，则使用double类型表示 */
				/* switch to double */
				double d = (double)Z_LVAL_P(op1);
				ZVAL_DOUBLE(op1, d+1);
			} else {
			Z_LVAL_P(op1)++;
			}
			break;
		case IS_DOUBLE:
			Z_DVAL_P(op1) = Z_DVAL_P(op1) + 1;
			break;
		case IS_NULL:
			ZVAL_LONG(op1, 1);
			break;
		case IS_STRING: {
				long lval;
				double dval;

				switch (is_numeric_string(Z_STRVAL_P(op1), Z_STRLEN_P(op1), &lval, &dval, 0)) {
					case IS_LONG:
						str_efree(Z_STRVAL_P(op1));
						if (lval == LONG_MAX) { /* 这一段很好理解，如果op1的值已经达到LONG取值范围的最大值，则使用double类型表示 */
							/* switch to double */
							double d = (double)lval;
							ZVAL_DOUBLE(op1, d+1);
						} else {
							ZVAL_LONG(op1, lval+1);
						}
						break;
					case IS_DOUBLE:
						str_efree(Z_STRVAL_P(op1));
						ZVAL_DOUBLE(op1, dval+1);
						break;
					default:
						/* Perl style string increment */
						increment_string(op1); /* 这里和decrement_function不一样，$a="abc";$a--不变,$a++则为"abd"; */
						break;
				}
			}
			break;
		case IS_OBJECT:
			if (Z_OBJ_HANDLER_P(op1, do_operation)) {
				zval *op2;
				int res;
				TSRMLS_FETCH();

				MAKE_STD_ZVAL(op2);
				ZVAL_LONG(op2, 1);
				res = Z_OBJ_HANDLER_P(op1, do_operation)(ZEND_ADD, op1, op1, op2 TSRMLS_CC);
				zval_ptr_dtor(&op2);

				return res;
			}
			return FAILURE;
		default:
			return FAILURE;
	}
	return SUCCESS;
}
/* }}} */

ZEND_API int decrement_function(zval *op1)/* 减量处理函数，例如代码：$a="123"; $a--; var_dump($a); 输出结果int(122); */ /* {{{ */
{
	long lval;
	double dval;

	switch (Z_TYPE_P(op1)) {
		case IS_LONG:
			if (Z_LVAL_P(op1) == LONG_MIN) { /* #define LONG_MIN (- LONG_MAX - 1),这一段很好理解，如果op1的值已经达到LONG取值范围的最小值，则使用double类型表示 */
				double d = (double)Z_LVAL_P(op1);
				ZVAL_DOUBLE(op1, d-1);
			} else {
			Z_LVAL_P(op1)--;
			}
			break;
		case IS_DOUBLE:
			Z_DVAL_P(op1) = Z_DVAL_P(op1) - 1;
			break;
		case IS_STRING:		/* Like perl we only support string increment */
			if (Z_STRLEN_P(op1) == 0) { /* consider as 0 */
				str_efree(Z_STRVAL_P(op1));
				ZVAL_LONG(op1, -1); /* 这就是为什么$a=""; $a--; var_dump($a); 的输出结果为int(-1); */
				break;
			}
			switch (is_numeric_string(Z_STRVAL_P(op1), Z_STRLEN_P(op1), &lval, &dval, 0)) {/* 判断op1的值是否为数值字符串，如果不为数值返回0，如果为数值，返回是IS_LONG或者IS_DOUBLE */
				case IS_LONG:
					str_efree(Z_STRVAL_P(op1));
					if (lval == LONG_MIN) { /* #define LONG_MIN (- LONG_MAX - 1),这一段很好理解，如果op1的值已经达到LONG取值范围的最小值，则使用double类型表示 */
						double d = (double)lval;
						ZVAL_DOUBLE(op1, d-1);
					} else {
						ZVAL_LONG(op1, lval-1); /* 这就是为什么$a="123"; $a--; var_dump($a); 的输出结果为int型而不是string类型; */
					}
					break;
				case IS_DOUBLE:
					str_efree(Z_STRVAL_P(op1));
					ZVAL_DOUBLE(op1, dval - 1);
					break;
			}
			break;
		case IS_OBJECT:
			if (Z_OBJ_HANDLER_P(op1, do_operation)) {
				zval *op2;
				int res;
				TSRMLS_FETCH();

				MAKE_STD_ZVAL(op2);
				ZVAL_LONG(op2, 1);
				res = Z_OBJ_HANDLER_P(op1, do_operation)(ZEND_SUB, op1, op1, op2 TSRMLS_CC);
				zval_ptr_dtor(&op2);

				return res;
			}
			return FAILURE;
		default:
			return FAILURE;
	}

	return SUCCESS;
}
/* }}} */

ZEND_API int zval_is_true(zval *op) /* {{{ */
{
	convert_to_boolean(op);
	return (Z_LVAL_P(op) ? 1 : 0);
}
/* }}} */

#ifdef ZEND_USE_TOLOWER_L
ZEND_API void zend_update_current_locale(void) /* {{{ */
{
	current_locale = _get_current_locale();
}
/* }}} */
#endif

ZEND_API char *zend_str_tolower_copy(char *dest, const char *source, unsigned int length)/* 将字符串source的前length长度段转换成小写复制到dest中并返回desc */ /* {{{ */
{
	register unsigned char *str = (unsigned char*)source;
	register unsigned char *result = (unsigned char*)dest;
	register unsigned char *end = str + length;

	while (str < end) {
		*result++ = zend_tolower_ascii(*str++); /* zend_tolower_ascii('A')的结果为a */
	}
	*result = '\0';

	return dest;
}
/* }}} */

ZEND_API char *zend_str_tolower_dup(const char *source, unsigned int length)/* 获取source前length长度段的小写字符串 */ /* {{{ */
{
	return zend_str_tolower_copy((char *)emalloc(length+1), source, length);
}
/* }}} */

ZEND_API void zend_str_tolower(char *str, unsigned int length)/* 将字符串转换成小写，主要对26个英文字母进行转换 */ /* {{{ */
{
	register unsigned char *p = (unsigned char*)str;
	register unsigned char *end = p + length;

	while (p < end) {
		*p = zend_tolower_ascii(*p); /* zend_tolower_ascii('A')的结果为a */
		p++;
	}
}
/* }}} */

ZEND_API int zend_binary_strcmp(const char *s1, uint len1, const char *s2, uint len2) /* {{{ */
{/* 大小写敏感的比较两个字符串是否相等，相等返回0，不相等返回对应的长度差值 */
	int retval;

	if (s1 == s2) {
		return 0;
	}
	retval = memcmp(s1, s2, MIN(len1, len2));/* memcmp是比较内存区域buf1和buf2的前count个字节,相等返回0 */
	if (!retval) {
		return (len1 - len2);
	} else {
		return retval;
	}
}
/* }}} */

ZEND_API int zend_binary_strncmp(const char *s1, uint len1, const char *s2, uint len2, uint length) /* {{{ */
{/* 大小写敏感的比较两个字符串是否相等，相等返回0，不相等返回对应的长度差值 */
	int retval;

	if (s1 == s2) {
		return 0;
	}
	retval = memcmp(s1, s2, MIN(length, MIN(len1, len2)));/* memcmp是比较内存区域buf1和buf2的前count个字节,相等返回0 */
	if (!retval) {
		return (MIN(length, len1) - MIN(length, len2));
	} else {
		return retval;
	}
}
/* }}} */

ZEND_API int zend_binary_strcasecmp(const char *s1, uint len1, const char *s2, uint len2) /* {{{ */
{/* 大小写不敏感的比较两个字符串是否相等，相等返回0，不相等返回对应的字符差值 */
	int len;
	int c1, c2;

	if (s1 == s2) {
		return 0;
	}

	len = MIN(len1, len2);
	while (len--) {
		c1 = zend_tolower_ascii(*(unsigned char *)s1++); /* zend_tolower_ascii('A')的结果为a */
		c2 = zend_tolower_ascii(*(unsigned char *)s2++); /* zend_tolower_ascii('A')的结果为a */
		if (c1 != c2) {
			return c1 - c2;
		}
	}

	return len1 - len2;
}
/* }}} */

ZEND_API int zend_binary_strncasecmp(const char *s1, uint len1, const char *s2, uint len2, uint length) /* {{{ */
{/* 大小写不敏感的比较两个字符串是否相等，相等返回0，不相等返回对应的字符差值 */
	int len;
	int c1, c2;

	if (s1 == s2) {
		return 0;
	}
	len = MIN(length, MIN(len1, len2));
	while (len--) {
		c1 = zend_tolower_ascii(*(unsigned char *)s1++); /* zend_tolower_ascii('A')的结果为a */
		c2 = zend_tolower_ascii(*(unsigned char *)s2++); /* zend_tolower_ascii('A')的结果为a */
		if (c1 != c2) {
			return c1 - c2;
		}
	}

	return MIN(length, len1) - MIN(length, len2);
}
/* }}} */

ZEND_API int zend_binary_strcasecmp_l(const char *s1, uint len1, const char *s2, uint len2) /* {{{ */
{/* 大小写不敏感的比较两个字符串是否相等，相等返回0，不相等返回对应的字符差值 */
	int len;
	int c1, c2;

	if (s1 == s2) {
		return 0;
	}

	len = MIN(len1, len2);
	while (len--) {
		c1 = zend_tolower((int)*(unsigned char *)s1++);/* 统一转换成小写对比，大小写不敏感的处理方式 */
		c2 = zend_tolower((int)*(unsigned char *)s2++);/* 统一转换成小写对比，大小写不敏感的处理方式 */
		if (c1 != c2) {
			return c1 - c2; /* 比如s1为abca,s2为abcBdfe,则返回-1 */
		}
	}

	return len1 - len2;
}
/* }}} */

ZEND_API int zend_binary_strncasecmp_l(const char *s1, uint len1, const char *s2, uint len2, uint length) /* {{{ */
{/* 与上一个函数差不多，只不过多了一个length参数限制 */
	int len;
	int c1, c2;

	if (s1 == s2) {
		return 0;
	}
	len = MIN(length, MIN(len1, len2));
	while (len--) {
		c1 = zend_tolower((int)*(unsigned char *)s1++);
		c2 = zend_tolower((int)*(unsigned char *)s2++);
		if (c1 != c2) {
			return c1 - c2;
		}
	}

	return MIN(length, len1) - MIN(length, len2);
}
/* }}} */

ZEND_API int zend_binary_zval_strcmp(zval *s1, zval *s2)/* 区分大小写比较两个值 */ /* {{{ */
{
	return zend_binary_strcmp(Z_STRVAL_P(s1), Z_STRLEN_P(s1), Z_STRVAL_P(s2), Z_STRLEN_P(s2));
}
/* }}} */

ZEND_API int zend_binary_zval_strncmp(zval *s1, zval *s2, zval *s3)/* 区分大小写比较两个值，s3长度限制 */ /* {{{ */
{
	return zend_binary_strncmp(Z_STRVAL_P(s1), Z_STRLEN_P(s1), Z_STRVAL_P(s2), Z_STRLEN_P(s2), Z_LVAL_P(s3));
}
/* }}} */

ZEND_API int zend_binary_zval_strcasecmp(zval *s1, zval *s2)/* 不区分大小写比较两个值 */ /* {{{ */
{
	return zend_binary_strcasecmp_l(Z_STRVAL_P(s1), Z_STRLEN_P(s1), Z_STRVAL_P(s2), Z_STRLEN_P(s2));
}
/* }}} */

ZEND_API int zend_binary_zval_strncasecmp(zval *s1, zval *s2, zval *s3)/* 不区分大小写比较两个值，s3长度限制 */ /* {{{ */
{
	return zend_binary_strncasecmp_l(Z_STRVAL_P(s1), Z_STRLEN_P(s1), Z_STRVAL_P(s2), Z_STRLEN_P(s2), Z_LVAL_P(s3));
}
/* }}} */

ZEND_API void zendi_smart_strcmp(zval *result, zval *s1, zval *s2) /* {{{ */
{
	int ret1, ret2;
	int oflow1, oflow2;
	long lval1 = 0, lval2 = 0;
	double dval1 = 0.0, dval2 = 0.0;

	if ((ret1=is_numeric_string_ex(Z_STRVAL_P(s1), Z_STRLEN_P(s1), &lval1, &dval1, 0, &oflow1)) &&
		(ret2=is_numeric_string_ex(Z_STRVAL_P(s2), Z_STRLEN_P(s2), &lval2, &dval2, 0, &oflow2))) {
#if ULONG_MAX == 0xFFFFFFFF
		if (oflow1 != 0 && oflow1 == oflow2 && dval1 - dval2 == 0. &&
			((oflow1 == 1 && dval1 > 9007199254740991. /*0x1FFFFFFFFFFFFF*/)
			|| (oflow1 == -1 && dval1 < -9007199254740991.))) {
#else
		if (oflow1 != 0 && oflow1 == oflow2 && dval1 - dval2 == 0.) {
#endif
			/* both values are integers overflown to the same side, and the
			 * double comparison may have resulted in crucial accuracy lost */
			goto string_cmp;
		}
		if ((ret1==IS_DOUBLE) || (ret2==IS_DOUBLE)) {
			if (ret1!=IS_DOUBLE) {
				if (oflow2) {
					/* 2nd operand is integer > LONG_MAX (oflow2==1) or < LONG_MIN (-1) */
					ZVAL_LONG(result, -1 * oflow2);
					return;
				}
				dval1 = (double) lval1;
			} else if (ret2!=IS_DOUBLE) {
				if (oflow1) {
					ZVAL_LONG(result, oflow1);
					return;
				}
				dval2 = (double) lval2;
			} else if (dval1 == dval2 && !zend_finite(dval1)) {
				/* Both values overflowed and have the same sign,
				 * so a numeric comparison would be inaccurate */
				goto string_cmp;
			}
			Z_DVAL_P(result) = dval1 - dval2;
			ZVAL_LONG(result, ZEND_NORMALIZE_BOOL(Z_DVAL_P(result)));
		} else { /* they both have to be long's */
			ZVAL_LONG(result, lval1 > lval2 ? 1 : (lval1 < lval2 ? -1 : 0));
		}
	} else {
string_cmp:
		Z_LVAL_P(result) = zend_binary_zval_strcmp(s1, s2);
		ZVAL_LONG(result, ZEND_NORMALIZE_BOOL(Z_LVAL_P(result)));
	}
}
/* }}} */

static int hash_zval_compare_function(const zval **z1, const zval **z2 TSRMLS_DC) /* {{{ */
{
	zval result;

	if (compare_function(&result, (zval *) *z1, (zval *) *z2 TSRMLS_CC)==FAILURE) {
		return 1;
	}
	return Z_LVAL(result);
}
/* }}} */

ZEND_API int zend_compare_symbol_tables_i(HashTable *ht1, HashTable *ht2 TSRMLS_DC) /* {{{ */
{
	return ht1 == ht2 ? 0 : zend_hash_compare(ht1, ht2, (compare_func_t) hash_zval_compare_function, 0 TSRMLS_CC);
}
/* }}} */

ZEND_API void zend_compare_symbol_tables(zval *result, HashTable *ht1, HashTable *ht2 TSRMLS_DC) /* {{{ */
{
	ZVAL_LONG(result, ht1 == ht2 ? 0 : zend_hash_compare(ht1, ht2, (compare_func_t) hash_zval_compare_function, 0 TSRMLS_CC));
}
/* }}} */

ZEND_API void zend_compare_arrays(zval *result, zval *a1, zval *a2 TSRMLS_DC) /* {{{ */
{
	zend_compare_symbol_tables(result, Z_ARRVAL_P(a1), Z_ARRVAL_P(a2) TSRMLS_CC);
}
/* }}} */

ZEND_API void zend_compare_objects(zval *result, zval *o1, zval *o2 TSRMLS_DC) /* {{{ */
{
	Z_TYPE_P(result) = IS_LONG;

	if (Z_OBJ_HANDLE_P(o1) == Z_OBJ_HANDLE_P(o2)) {
		Z_LVAL_P(result) = 0;
		return;
	}

	if (Z_OBJ_HT_P(o1)->compare_objects == NULL) {
		Z_LVAL_P(result) = 1;
	} else {
		Z_LVAL_P(result) = Z_OBJ_HT_P(o1)->compare_objects(o1, o2 TSRMLS_CC);
	}
}
/* }}} */

ZEND_API void zend_locale_sprintf_double(zval *op ZEND_FILE_LINE_DC) /* {{{ */
{
	TSRMLS_FETCH();

	Z_STRLEN_P(op) = zend_spprintf(&Z_STRVAL_P(op), 0, "%.*G", (int) EG(precision), (double)Z_DVAL_P(op));
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
