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
#include "zend_constants.h"
#include "zend_execute.h"
#include "zend_variables.h"
#include "zend_operators.h"
#include "zend_globals.h"
#include "zend_API.h"

void free_zend_constant(zend_constant *c)/* 常量的销毁函数 */
{
	if (!(c->flags & CONST_PERSISTENT)) {
		zval_dtor(&c->value);/* 如果c->flags的值不含CONST_PERSISTENT持久化的，则释放c->value */
	}
	str_free(c->name);
}


void copy_zend_constant(zend_constant *c)/* 复制常量 */
{
	c->name = str_strndup(c->name, c->name_len - 1);
	if (!(c->flags & CONST_PERSISTENT)) {
		zval_copy_ctor(&c->value);
	}
}


void zend_copy_constants(HashTable *target, HashTable *source)
{
	zend_constant tmp_constant;

	zend_hash_copy(target, source, (copy_ctor_func_t) copy_zend_constant, &tmp_constant, sizeof(zend_constant));
}


static int clean_non_persistent_constant(const zend_constant *c TSRMLS_DC)
{
	return (c->flags & CONST_PERSISTENT) ? ZEND_HASH_APPLY_STOP : ZEND_HASH_APPLY_REMOVE;/* 等价 return (c->flags是否含持久化)?2:1 */
}


static int clean_non_persistent_constant_full(const zend_constant *c TSRMLS_DC)
{
	return (c->flags & CONST_PERSISTENT) ? 0 : 1;/* 等价 return (c->flags是否含持久化)?0:1 */
}


static int clean_module_constant(const zend_constant *c, int *module_number TSRMLS_DC)
{
	if (c->module_number == *module_number) { /* 如果常量所属的module号等于module_number，返回1 */
		return 1;
	} else {
		return 0;
	}
}


void clean_module_constants(int module_number TSRMLS_DC)/* 清空模块编号为module_number的常量，主要利用zend_hash_apply_with_argument的回调clean_module_constant函数确定是否删除，1表示删除 */
{
	zend_hash_apply_with_argument(EG(zend_constants), (apply_func_arg_t) clean_module_constant, (void *) &module_number TSRMLS_CC);/* 该函数在zend_hash.c中 */
}


int zend_startup_constants(TSRMLS_D)/* 初始化常量保存的全局变量 */
{
	EG(zend_constants) = (HashTable *) malloc(sizeof(HashTable));/* 说明常量保存为一个hash表中 */
	/* 主要是对大小进行调整,不能超过0x80000000,不能少于8,为2的n次方的值,设置为持久化数据,设计析构函数为free_zend_constant */
	if (zend_hash_init(EG(zend_constants), 20, NULL, ZEND_CONSTANT_DTOR, 1)==FAILURE) {/* ZEND_CONSTANT_DTOR等价(void (*)(void *)) free_zend_constant */
		return FAILURE;
	}
	return SUCCESS;
}



void zend_register_standard_constants(TSRMLS_D)/* 注册标准的常量,这就是为什么我们可以直接在代码中获取E_ERROR等常量,而不需要自己定义 */
{
	REGISTER_MAIN_LONG_CONSTANT("E_ERROR", E_ERROR, CONST_PERSISTENT | CONST_CS);/* 等价zend_register_long_constant(”E_ERROR“, sizeof(”E_ERROR“), E_ERROR, CONST_PERSISTENT | CONST_CS, 0 TSRMLS_CC) */
	REGISTER_MAIN_LONG_CONSTANT("E_RECOVERABLE_ERROR", E_RECOVERABLE_ERROR, CONST_PERSISTENT | CONST_CS);
	REGISTER_MAIN_LONG_CONSTANT("E_WARNING", E_WARNING, CONST_PERSISTENT | CONST_CS);
	REGISTER_MAIN_LONG_CONSTANT("E_PARSE", E_PARSE, CONST_PERSISTENT | CONST_CS);
	REGISTER_MAIN_LONG_CONSTANT("E_NOTICE", E_NOTICE, CONST_PERSISTENT | CONST_CS);
	REGISTER_MAIN_LONG_CONSTANT("E_STRICT", E_STRICT, CONST_PERSISTENT | CONST_CS);
	REGISTER_MAIN_LONG_CONSTANT("E_DEPRECATED", E_DEPRECATED, CONST_PERSISTENT | CONST_CS);
	REGISTER_MAIN_LONG_CONSTANT("E_CORE_ERROR", E_CORE_ERROR, CONST_PERSISTENT | CONST_CS);
	REGISTER_MAIN_LONG_CONSTANT("E_CORE_WARNING", E_CORE_WARNING, CONST_PERSISTENT | CONST_CS);
	REGISTER_MAIN_LONG_CONSTANT("E_COMPILE_ERROR", E_COMPILE_ERROR, CONST_PERSISTENT | CONST_CS);
	REGISTER_MAIN_LONG_CONSTANT("E_COMPILE_WARNING", E_COMPILE_WARNING, CONST_PERSISTENT | CONST_CS);
	REGISTER_MAIN_LONG_CONSTANT("E_USER_ERROR", E_USER_ERROR, CONST_PERSISTENT | CONST_CS);
	REGISTER_MAIN_LONG_CONSTANT("E_USER_WARNING", E_USER_WARNING, CONST_PERSISTENT | CONST_CS);
	REGISTER_MAIN_LONG_CONSTANT("E_USER_NOTICE", E_USER_NOTICE, CONST_PERSISTENT | CONST_CS);
	REGISTER_MAIN_LONG_CONSTANT("E_USER_DEPRECATED", E_USER_DEPRECATED, CONST_PERSISTENT | CONST_CS);

	REGISTER_MAIN_LONG_CONSTANT("E_ALL", E_ALL, CONST_PERSISTENT | CONST_CS);

	REGISTER_MAIN_LONG_CONSTANT("DEBUG_BACKTRACE_PROVIDE_OBJECT", DEBUG_BACKTRACE_PROVIDE_OBJECT, CONST_PERSISTENT | CONST_CS);
	REGISTER_MAIN_LONG_CONSTANT("DEBUG_BACKTRACE_IGNORE_ARGS", DEBUG_BACKTRACE_IGNORE_ARGS, CONST_PERSISTENT | CONST_CS);
	/* true/false constants */
	{
		REGISTER_MAIN_BOOL_CONSTANT("TRUE", 1, CONST_PERSISTENT | CONST_CT_SUBST);
		REGISTER_MAIN_BOOL_CONSTANT("FALSE", 0, CONST_PERSISTENT | CONST_CT_SUBST);
		REGISTER_MAIN_BOOL_CONSTANT("ZEND_THREAD_SAFE", ZTS_V, CONST_PERSISTENT | CONST_CS);
		REGISTER_MAIN_BOOL_CONSTANT("ZEND_DEBUG_BUILD", ZEND_DEBUG, CONST_PERSISTENT | CONST_CS);
	}
	REGISTER_MAIN_NULL_CONSTANT("NULL", CONST_PERSISTENT | CONST_CT_SUBST);
}


int zend_shutdown_constants(TSRMLS_D)/* 释放所有常量的值 */
{
	zend_hash_destroy(EG(zend_constants));
	free(EG(zend_constants));
	return SUCCESS;
}


void clean_non_persistent_constants(TSRMLS_D)/* 清空非持久化的常量,主要利用zend_hash_apply的回调clean_non_persistent_constant_full函数确定是否删除，1表示删除 */
{
	if (EG(full_tables_cleanup)) {
		zend_hash_apply(EG(zend_constants), (apply_func_t) clean_non_persistent_constant_full TSRMLS_CC);
	} else {
		zend_hash_reverse_apply(EG(zend_constants), (apply_func_t) clean_non_persistent_constant TSRMLS_CC);
	}
}

ZEND_API void zend_register_null_constant(const char *name, uint name_len, int flags, int module_number TSRMLS_DC)/* 注册空常量,主要用于类常量声明 */
{
	zend_constant c;
	
	ZVAL_NULL(&c.value);
	c.flags = flags;
	c.name = zend_strndup(name, name_len-1);
	c.name_len = name_len;
	c.module_number = module_number;
	zend_register_constant(&c TSRMLS_CC);
}

ZEND_API void zend_register_bool_constant(const char *name, uint name_len, zend_bool bval, int flags, int module_number TSRMLS_DC)/* 注册布尔类型常量 */
{
	zend_constant c;
	
	ZVAL_BOOL(&c.value, bval);
	c.flags = flags;
	c.name = zend_strndup(name, name_len-1);
	c.name_len = name_len;
	c.module_number = module_number;
	zend_register_constant(&c TSRMLS_CC);
}

ZEND_API void zend_register_long_constant(const char *name, uint name_len, long lval, int flags, int module_number TSRMLS_DC)/* 注册整形常量 */
{
	zend_constant c;
	
	ZVAL_LONG(&c.value, lval);
	c.flags = flags;
	c.name = zend_strndup(name, name_len-1);
	c.name_len = name_len;
	c.module_number = module_number;
	zend_register_constant(&c TSRMLS_CC);
}


ZEND_API void zend_register_double_constant(const char *name, uint name_len, double dval, int flags, int module_number TSRMLS_DC)/* 注册浮点类型常量 */
{
	zend_constant c;
	
	ZVAL_DOUBLE(&c.value, dval);
	c.flags = flags;
	c.name = zend_strndup(name, name_len-1);
	c.name_len = name_len;
	c.module_number = module_number;
	zend_register_constant(&c TSRMLS_CC);
}


ZEND_API void zend_register_stringl_constant(const char *name, uint name_len, char *strval, uint strlen, int flags, int module_number TSRMLS_DC)/* 注册字符串类型常量 */
{
	zend_constant c;
	
	ZVAL_STRINGL(&c.value, strval, strlen, 0);
	c.flags = flags;
	c.name = zend_strndup(name, name_len-1);
	c.name_len = name_len;
	c.module_number = module_number;
	zend_register_constant(&c TSRMLS_CC);
}


ZEND_API void zend_register_string_constant(const char *name, uint name_len, char *strval, int flags, int module_number TSRMLS_DC)
{
	zend_register_stringl_constant(name, name_len, strval, strlen(strval), flags, module_number TSRMLS_CC);
}

static int zend_get_special_constant(const char *name, uint name_len, zend_constant **c TSRMLS_DC)/* 获取特殊常量的函数 */
{
	int ret;
	static char haltoff[] = "__COMPILER_HALT_OFFSET__";

	if (!EG(in_execution)) {/* 如果不在执行阶段，返回失败 */
		return 0;
	} else if (name_len == sizeof("__CLASS__")-1 &&
	          !memcmp(name, "__CLASS__", sizeof("__CLASS__")-1)) { /* 如果name为__CLASS__,注意,memcmp()函数是比较的两个buf相等的话,返回0 */
		zend_constant tmp;

		/* Returned constants may be cached, so they have to be stored */
		if (EG(scope) && EG(scope)->name) {
			int const_name_len;
			char *const_name;
			ALLOCA_FLAG(use_heap)
			
			const_name_len = sizeof("\0__CLASS__") + EG(scope)->name_length;
			const_name = do_alloca(const_name_len, use_heap);
			memcpy(const_name, "\0__CLASS__", sizeof("\0__CLASS__")-1);
			zend_str_tolower_copy(const_name + sizeof("\0__CLASS__")-1, EG(scope)->name, EG(scope)->name_length);
			if (zend_hash_find(EG(zend_constants), const_name, const_name_len, (void**)c) == FAILURE) {
				zend_hash_add(EG(zend_constants), const_name, const_name_len, (void*)&tmp, sizeof(zend_constant), (void**)c);
				memset(*c, 0, sizeof(zend_constant));
				Z_STRVAL((**c).value) = estrndup(EG(scope)->name, EG(scope)->name_length);
				Z_STRLEN((**c).value) = EG(scope)->name_length;
				Z_TYPE((**c).value) = IS_STRING;
			}
			free_alloca(const_name, use_heap);
		} else {
			if (zend_hash_find(EG(zend_constants), "\0__CLASS__", sizeof("\0__CLASS__"), (void**)c) == FAILURE) {
				zend_hash_add(EG(zend_constants), "\0__CLASS__", sizeof("\0__CLASS__"), (void*)&tmp, sizeof(zend_constant), (void**)c);
				memset(*c, 0, sizeof(zend_constant));
				Z_STRVAL((**c).value) = estrndup("", 0);
				Z_STRLEN((**c).value) = 0;
				Z_TYPE((**c).value) = IS_STRING;
			}
		}
		return 1;
	} else if (name_len == sizeof("__COMPILER_HALT_OFFSET__")-1 &&
	          !memcmp(name, "__COMPILER_HALT_OFFSET__", sizeof("__COMPILER_HALT_OFFSET__")-1)) {
		const char *cfilename;
		char *haltname;
		int len, clen;

		cfilename = zend_get_executed_filename(TSRMLS_C);
		clen = strlen(cfilename);
		/* check for __COMPILER_HALT_OFFSET__ */
		zend_mangle_property_name(&haltname, &len, haltoff,
			sizeof("__COMPILER_HALT_OFFSET__") - 1, cfilename, clen, 0);
		ret = zend_hash_find(EG(zend_constants), haltname, len+1, (void **) c);/* 这里和zend_compile.c文件的zend_do_halt_compiler_register()对应的 仅被定义于使用了__halt_compiler的文件  */
		efree(haltname);
		return (ret == SUCCESS);
	} else {
		return 0;
	}
}


ZEND_API int zend_get_constant(const char *name, uint name_len, zval *result TSRMLS_DC)/* 通过name获取常量的值赋给result,返回retval的值1或0表示成功还是失败 */
{
	zend_constant *c;
	int retval = 1;
	char *lookup_name;

	if (zend_hash_find(EG(zend_constants), name, name_len+1, (void **) &c) == FAILURE) {/* 先从executors_globals.zend_constants中查找 */
		lookup_name = zend_str_tolower_dup(name, name_len);/* 如果没找到，将name转化成小写 */

		if (zend_hash_find(EG(zend_constants), lookup_name, name_len+1, (void **) &c)==SUCCESS) {/* 然后再去executors_globals.zend_constants中查找 */
			if (c->flags & CONST_CS) {/* 如果查找到的值是区分大小写的，就不给 */
				retval=0;
			}
		} else {
			retval = zend_get_special_constant(name, name_len, &c TSRMLS_CC);/* 如果还是没找到，就去特殊变量中查找 */
		}
		efree(lookup_name);
	}/* 该函数存在性能缺陷,如果查找一个不存在的常量,就得查3次 */

	if (retval) {
		*result = c->value;
		zval_copy_ctor(result);
		Z_SET_REFCOUNT_P(result, 1);
		Z_UNSET_ISREF_P(result);
	}

	return retval;
}

ZEND_API int zend_get_constant_ex(const char *name, uint name_len, zval *result, zend_class_entry *scope, ulong flags TSRMLS_DC)
{
	zend_constant *c;
	int retval = 1;
	const char *colon;
	zend_class_entry *ce = NULL;
	char *class_name;
	zval **ret_constant;

	/* Skip leading \\ */
	if (name[0] == '\\') {
		name += 1;
		name_len -= 1;
	}


	if ((colon = zend_memrchr(name, ':', name_len)) && 		/* 从name查找最后一个字符":"的地址 */
	    colon > name && (*(colon - 1) == ':')) {			/* 如果查找到,且前一个字符也为":"则条件成立,例如 classname::constname */
		int class_name_len = colon - name - 1;				/* calss_name_len为第一个":"前面字符串的长度 */
		int const_name_len = name_len - class_name_len - 2;	/* const_name_len为最后一个":"后面字符串的长度 */
		const char *constant_name = colon + 1;				/* const_name为例子中的constname */
		char *lcname;

		class_name = estrndup(name, class_name_len);		/* class_name为例子中的classname */
		lcname = zend_str_tolower_dup(class_name, class_name_len);/* 将class_name转成小写 */
		if (!scope) {
			if (EG(in_execution)) {
				scope = EG(scope);
			} else {
				scope = CG(active_class_entry);
			}
		}

		if (class_name_len == sizeof("self")-1 &&
		    !memcmp(lcname, "self", sizeof("self")-1)) { /* memcmp()函数是比较的两个buf相等的话,返回0 */
			if (scope) {
				ce = scope;
			} else {
				zend_error(E_ERROR, "Cannot access self:: when no class scope is active");
				retval = 0;
			}
			efree(lcname);
		} else if (class_name_len == sizeof("parent")-1 &&
		           !memcmp(lcname, "parent", sizeof("parent")-1)) {	/* memcmp()函数是比较的两个buf相等的话,返回0 */
			if (!scope) {
				zend_error(E_ERROR, "Cannot access parent:: when no class scope is active");
			} else if (!scope->parent) {
				zend_error(E_ERROR, "Cannot access parent:: when current class scope has no parent");
			} else {
				ce = scope->parent;
			}
			efree(lcname);
		} else if (class_name_len == sizeof("static")-1 &&
		           !memcmp(lcname, "static", sizeof("static")-1)) {	/* memcmp()函数是比较的两个buf相等的话,返回0 */
			if (EG(called_scope)) {
				ce = EG(called_scope);
			} else {
				zend_error(E_ERROR, "Cannot access static:: when no class scope is active");
			}
			efree(lcname);
		} else {
			efree(lcname);
			ce = zend_fetch_class(class_name, class_name_len, flags TSRMLS_CC);
		}
		if (retval && ce) {
			if (zend_hash_find(&ce->constants_table, constant_name, const_name_len+1, (void **) &ret_constant) != SUCCESS) {
				retval = 0;
				if ((flags & ZEND_FETCH_CLASS_SILENT) == 0) {
					zend_error(E_ERROR, "Undefined class constant '%s::%s'", class_name, constant_name);
				}
			}
		} else if (!ce) {
			retval = 0;
		}
		efree(class_name);
		goto finish;
	}/* 这里面是查找类常量 */

	/* non-class constant */
	if ((colon = zend_memrchr(name, '\\', name_len)) != NULL) {
		/* compound constant name */
		int prefix_len = colon - name;
		int const_name_len = name_len - prefix_len - 1;
		const char *constant_name = colon + 1;
		char *lcname;
		int found_const = 0;

		lcname = zend_str_tolower_dup(name, prefix_len);/* 这里是处理最后一个"\"字符前面的字符串全部转换成小写，意味着不区分大小写,因为在注册常量的时候就确定了，请看zend_register_constant()函数 */
		/* Check for namespace constant */

		/* Concatenate lowercase namespace name and constant name */
		lcname = erealloc(lcname, prefix_len + 1 + const_name_len + 1);
		lcname[prefix_len] = '\\';
		memcpy(lcname + prefix_len + 1, constant_name, const_name_len + 1);

		if (zend_hash_find(EG(zend_constants), lcname, prefix_len + 1 + const_name_len + 1, (void **) &c) == SUCCESS) {
			found_const = 1;
		} else {
			/* try lowercase */
			zend_str_tolower(lcname + prefix_len + 1, const_name_len);
			if (zend_hash_find(EG(zend_constants), lcname, prefix_len + 1 + const_name_len + 1, (void **) &c) == SUCCESS) {
				if ((c->flags & CONST_CS) == 0) {
					found_const = 1;
				}
			}
		}
		efree(lcname);
		if(found_const) {
			*result = c->value;
			zval_update_constant_ex(&result, 1, NULL TSRMLS_CC);
			zval_copy_ctor(result);
			Z_SET_REFCOUNT_P(result, 1);
			Z_UNSET_ISREF_P(result);
			return 1;
		}
		/* name requires runtime resolution, need to check non-namespaced name */
		if ((flags & IS_CONSTANT_UNQUALIFIED) != 0) {
			name = constant_name;
			name_len = const_name_len;
			return zend_get_constant(name, name_len, result TSRMLS_CC);
		}
		retval = 0;
finish:
		if (retval) {
			zval_update_constant_ex(ret_constant, 1, ce TSRMLS_CC);
			*result = **ret_constant;
			zval_copy_ctor(result);
			INIT_PZVAL(result);
		}

		return retval;
	}

	return zend_get_constant(name, name_len, result TSRMLS_CC);
}

zend_constant *zend_quick_get_constant(const zend_literal *key, ulong flags TSRMLS_DC)/* 快速查找常量函数 */
{
	zend_constant *c;

	if (zend_hash_quick_find(EG(zend_constants), Z_STRVAL(key->constant), Z_STRLEN(key->constant) + 1, key->hash_value, (void **) &c) == FAILURE) {
		key++;
		if (zend_hash_quick_find(EG(zend_constants), Z_STRVAL(key->constant), Z_STRLEN(key->constant) + 1, key->hash_value, (void **) &c) == FAILURE ||
		    (c->flags & CONST_CS) != 0) {
			if ((flags & (IS_CONSTANT_IN_NAMESPACE|IS_CONSTANT_UNQUALIFIED)) == (IS_CONSTANT_IN_NAMESPACE|IS_CONSTANT_UNQUALIFIED)) {
				key++;
				if (zend_hash_quick_find(EG(zend_constants), Z_STRVAL(key->constant), Z_STRLEN(key->constant) + 1, key->hash_value, (void **) &c) == FAILURE) {
				    key++;
					if (zend_hash_quick_find(EG(zend_constants), Z_STRVAL(key->constant), Z_STRLEN(key->constant) + 1, key->hash_value, (void **) &c) == FAILURE ||
					    (c->flags & CONST_CS) != 0) {

						key--;
						if (!zend_get_special_constant(Z_STRVAL(key->constant), Z_STRLEN(key->constant), &c TSRMLS_CC)) {
							return NULL;
						}
					}
				}
			} else {
				key--;
				if (!zend_get_special_constant(Z_STRVAL(key->constant), Z_STRLEN(key->constant), &c TSRMLS_CC)) {
					return NULL;
				}
			}
		}
	}
	return c;
}

ZEND_API int zend_register_constant(zend_constant *c TSRMLS_DC)/* 单个常量注册函数 */
{
	char *lowercase_name = NULL;
	char *name;
	int ret = SUCCESS;
	ulong chash;

#if 0
	printf("Registering constant for module %d\n", c->module_number);
#endif

	if (!(c->flags & CONST_CS)) {/* 如果是不区分大小写,c->name统一转换成小写 */
		/* keep in mind that c->name_len already contains the '\0' */
		lowercase_name = estrndup(c->name, c->name_len-1);
		zend_str_tolower(lowercase_name, c->name_len-1);
		lowercase_name = (char*)zend_new_interned_string(lowercase_name, c->name_len, 1 TSRMLS_CC);
		name = lowercase_name;
	} else {/* 下面为区分大小写 */
		char *slash = strrchr(c->name, '\\');/* 查找字符"\"在c->name中末次出现的位置 */
		if (slash) {
			lowercase_name = estrndup(c->name, c->name_len-1);
			zend_str_tolower(lowercase_name, slash-c->name);/* 这意味着最后一个字符"\"前面的字符串不区分大小写,例如define('ERR\RR\HEE',1234);echo err\rr\HEE;是合法的 */
			lowercase_name = (char*)zend_new_interned_string(lowercase_name, c->name_len, 1 TSRMLS_CC);
			name = lowercase_name;
		} else {
			name = c->name;
		}
	}
	chash = str_hash(name, c->name_len-1);/* chash = zend_hash_func(name, c->name_len)) 使用times33算法将字符串name转换成整数 */

	/* Check if the user is trying to define the internal pseudo constant name __COMPILER_HALT_OFFSET__ */
	if ((c->name_len == sizeof("__COMPILER_HALT_OFFSET__")
		&& !memcmp(name, "__COMPILER_HALT_OFFSET__", sizeof("__COMPILER_HALT_OFFSET__")-1))
		|| zend_hash_quick_add(EG(zend_constants), name, c->name_len, chash, (void *) c, sizeof(zend_constant), NULL)==FAILURE) {/* 添加到全局变量EG(zend_constants)中 */
		
		/* The internal __COMPILER_HALT_OFFSET__ is prefixed by NULL byte */
		if (c->name[0] == '\0' && c->name_len > sizeof("\0__COMPILER_HALT_OFFSET__")
			&& memcmp(name, "\0__COMPILER_HALT_OFFSET__", sizeof("\0__COMPILER_HALT_OFFSET__")) == 0) {
			name++;
		}
		zend_error(E_NOTICE,"Constant %s already defined", name);/* 如果常量名称为__COMPILER_HALT_OFFSET__或者添加失败(常量存在)就报该错误 */
		str_free(c->name);
		if (!(c->flags & CONST_PERSISTENT)) {
			zval_dtor(&c->value);
		}
		ret = FAILURE;
	}
	if (lowercase_name) {
		str_efree(lowercase_name);
	}
	return ret;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
