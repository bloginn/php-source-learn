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

#include "zend_extensions.h"

ZEND_API zend_llist zend_extensions;
static int last_resource_number;

int zend_load_extension(const char *path)/* 通过传递路径加载扩展 */
{
#if ZEND_EXTENSIONS_SUPPORT
	DL_HANDLE handle;/* DL_HANDLE等价void * 无类型指针 */
	zend_extension *new_extension;
	zend_extension_version_info *extension_version_info;

	handle = DL_LOAD(path);
	if (!handle) {
#ifndef ZEND_WIN32
		fprintf(stderr, "Failed loading %s:  %s\n", path, DL_ERROR());/* 加载扩展失败 */
/* See http://support.microsoft.com/kb/190351 */
#ifdef PHP_WIN32
		fflush(stderr);
#endif
#else
		fprintf(stderr, "Failed loading %s\n", path);
#endif
		return FAILURE;
	}

	extension_version_info = (zend_extension_version_info *) DL_FETCH_SYMBOL(handle, "extension_version_info");/* dlsym()函数获取扩展版本信息 */
	if (!extension_version_info) {
		extension_version_info = (zend_extension_version_info *) DL_FETCH_SYMBOL(handle, "_extension_version_info");
	}
	new_extension = (zend_extension *) DL_FETCH_SYMBOL(handle, "zend_extension_entry");
	if (!new_extension) {
		new_extension = (zend_extension *) DL_FETCH_SYMBOL(handle, "_zend_extension_entry");
	}
	if (!extension_version_info || !new_extension) {
		fprintf(stderr, "%s doesn't appear to be a valid Zend extension\n", path);
/* See http://support.microsoft.com/kb/190351 */
#ifdef PHP_WIN32
		fflush(stderr);
#endif
		DL_UNLOAD(handle);
		return FAILURE;
	}


	/* allow extension to proclaim compatibility with any Zend version */
	if (extension_version_info->zend_extension_api_no != ZEND_EXTENSION_API_NO &&(!new_extension->api_no_check || new_extension->api_no_check(ZEND_EXTENSION_API_NO) != SUCCESS)) {
		if (extension_version_info->zend_extension_api_no > ZEND_EXTENSION_API_NO) {
			fprintf(stderr, "%s requires Zend Engine API version %d.\n"
					"The Zend Engine API version %d which is installed, is outdated.\n\n",
					new_extension->name,
					extension_version_info->zend_extension_api_no,
					ZEND_EXTENSION_API_NO);
/* See http://support.microsoft.com/kb/190351 */
#ifdef PHP_WIN32
			fflush(stderr);
#endif
			DL_UNLOAD(handle);
			return FAILURE;
		} else if (extension_version_info->zend_extension_api_no < ZEND_EXTENSION_API_NO) {
			fprintf(stderr, "%s requires Zend Engine API version %d.\n"
					"The Zend Engine API version %d which is installed, is newer.\n"
					"Contact %s at %s for a later version of %s.\n\n",
					new_extension->name,
					extension_version_info->zend_extension_api_no,
					ZEND_EXTENSION_API_NO,
					new_extension->author,
					new_extension->URL,
					new_extension->name);
/* See http://support.microsoft.com/kb/190351 */
#ifdef PHP_WIN32
			fflush(stderr);
#endif
			DL_UNLOAD(handle);
			return FAILURE;
		}
	} else if (strcmp(ZEND_EXTENSION_BUILD_ID, extension_version_info->build_id) &&
	           (!new_extension->build_id_check || new_extension->build_id_check(ZEND_EXTENSION_BUILD_ID) != SUCCESS)) {
		fprintf(stderr, "Cannot load %s - it was built with configuration %s, whereas running engine is %s\n",
					new_extension->name, extension_version_info->build_id, ZEND_EXTENSION_BUILD_ID);
/* See http://support.microsoft.com/kb/190351 */
#ifdef PHP_WIN32
		fflush(stderr);
#endif
		DL_UNLOAD(handle);
		return FAILURE;
	} else if (zend_get_extension(new_extension->name)) {
		fprintf(stderr, "Cannot load %s - extension already loaded\n", new_extension->name);/* 如果已经加载过，则报此错误 */
/* See http://support.microsoft.com/kb/190351 */
#ifdef PHP_WIN32
		fflush(stderr);
#endif
		DL_UNLOAD(handle);
		return FAILURE;
	}/* 以上主要检查扩展的兼容性和是否重复加载 */

	return zend_register_extension(new_extension, handle);/* 真正的注册扩展在这个函数里 */
#else /* 对应第一个if */
	fprintf(stderr, "Extensions are not supported on this platform.\n");/* 如果ZEND_EXTENSIONS_SUPPORT为0 该平台不支持扩展 */
/* See http://support.microsoft.com/kb/190351 */
#ifdef PHP_WIN32
	fflush(stderr);
#endif
	return FAILURE;
#endif
}


int zend_register_extension(zend_extension *new_extension, DL_HANDLE handle)/* 把扩展注册到双向链表中 */
{
#if ZEND_EXTENSIONS_SUPPORT /* 如果不支持扩展的话，也就免谈了 */
	zend_extension extension;

	extension = *new_extension;
	extension.handle = handle;

	zend_extension_dispatch_message(ZEND_EXTMSG_NEW_EXTENSION, &extension);/* ZEND_EXTMSG_NEW_EXTENSION 1 */

	zend_llist_add_element(&zend_extensions, &extension);/* 将扩展添加到存储扩展信息的双向链表中，备用 */

	/*fprintf(stderr, "Loaded %s, version %s\n", extension.name, extension.version);*/
#endif

	return SUCCESS;
}


static void zend_extension_shutdown(zend_extension *extension TSRMLS_DC)/* 关闭扩展 */
{
#if ZEND_EXTENSIONS_SUPPORT
	if (extension->shutdown) {
		extension->shutdown(extension);/* 调用扩展的关闭触发函数 */
	}
#endif
}

static int zend_extension_startup(zend_extension *extension)/* 启动扩展 */
{
#if ZEND_EXTENSIONS_SUPPORT
	if (extension->startup) {
		if (extension->startup(extension)!=SUCCESS) { /* 调用扩展的启动触发函数 */
			return 1;
		}
		zend_append_version_info(extension);
	}
#endif
	return 0;
}


int zend_startup_extensions_mechanism()
{
	/* Startup extensions mechanism */ /* 启动扩展机制 */
	zend_llist_init(&zend_extensions, sizeof(zend_extension), (void (*)(void *)) zend_extension_dtor, 1);
	last_resource_number = 0;
	return SUCCESS;
}


int zend_startup_extensions()
{
	zend_llist_apply_with_del(&zend_extensions, (int (*)(void *)) zend_extension_startup);
	return SUCCESS;
}


void zend_shutdown_extensions(TSRMLS_D)
{
	zend_llist_apply(&zend_extensions, (llist_apply_func_t) zend_extension_shutdown TSRMLS_CC);
	zend_llist_destroy(&zend_extensions);
}


void zend_extension_dtor(zend_extension *extension)
{
#if ZEND_EXTENSIONS_SUPPORT && !ZEND_DEBUG
	if (extension->handle) {
		DL_UNLOAD(extension->handle);
	}
#endif
}


static void zend_extension_message_dispatcher(const zend_extension *extension, int num_args, va_list args TSRMLS_DC)
{
	int message;
	void *arg;

	if (!extension->message_handler || num_args!=2) {
		return;
	}
	message = va_arg(args, int);
	arg = va_arg(args, void *);
	extension->message_handler(message, arg);
}


ZEND_API void zend_extension_dispatch_message(int message, void *arg)
{
	TSRMLS_FETCH();

	zend_llist_apply_with_arguments(&zend_extensions, (llist_apply_with_args_func_t) zend_extension_message_dispatcher TSRMLS_CC, 2, message, arg);
}


ZEND_API int zend_get_resource_handle(zend_extension *extension)/* 获取资源句柄位置 */
{
	if (last_resource_number<ZEND_MAX_RESERVED_RESOURCES) { /* ZEND_MAX_RESERVED_RESOURCES为4，最大保留资源 */
		extension->resource_number = last_resource_number;
		return last_resource_number++;
	} else {
		return -1;
	}
}


ZEND_API zend_extension *zend_get_extension(const char *extension_name)/* 通过扩展名称查找扩展信息 */
{
	zend_llist_element *element;/* zend_llist_element是双向链表结构struct zend_llist_element { *next;*prev;data[1];} */

	for (element = zend_extensions.head; element; element = element->next) {
		zend_extension *extension = (zend_extension *) element->data;

		if (!strcmp(extension->name, extension_name)) {
			return extension;
		}
	}
	return NULL;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
