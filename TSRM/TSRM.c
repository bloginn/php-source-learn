/*
   +----------------------------------------------------------------------+
   | Thread Safe Resource Manager                                         |
   +----------------------------------------------------------------------+
   | Copyright (c) 1999-2011, Andi Gutmans, Sascha Schumann, Zeev Suraski |
   | This source file is subject to the TSRM license, that is bundled     |
   | with this package in the file LICENSE                                |
   +----------------------------------------------------------------------+
   | Authors:  Zeev Suraski <zeev@zend.com>                               |
   +----------------------------------------------------------------------+
*/

#include "TSRM.h"

#ifdef ZTS

#include <stdio.h>

#if HAVE_STDARG_H /* 在main/php_config.h文件中定义 */
#include <stdarg.h>
#endif

typedef struct _tsrm_tls_entry tsrm_tls_entry;

struct _tsrm_tls_entry {
	void **storage; /* 指针数组 */
	int count; /* 全局变量数 */
	THREAD_T thread_id; /* 线程id */
	tsrm_tls_entry *next; /* 下一个线程的地址 */
};


typedef struct {
	size_t size; /* size_t等价unsigned int */
	ts_allocate_ctor ctor; /* 构造方法指针 */
	ts_allocate_dtor dtor; /* 析构方法指针 */
	int done; /* 0或1，*/
} tsrm_resource_type;


/* The memory manager table */
static tsrm_tls_entry	**tsrm_tls_table=NULL;
static int				tsrm_tls_table_size; /* 线程数量记录值 */
static ts_rsrc_id		id_count; /* ts_rsrc_id等价int id_count是一个全局静态变量,通过自增产生资源ID*/

/* The resource sizes table */
static tsrm_resource_type	*resource_types_table=NULL; /* tsrm_resource_table可以看做是一个哈希表 */
static int					resource_types_table_size;


static MUTEX_T tsmm_mutex;	/* thread-safe memory manager mutex */

/* New thread handlers */
static tsrm_thread_begin_func_t tsrm_new_thread_begin_handler;
static tsrm_thread_end_func_t tsrm_new_thread_end_handler;

/* Debug support */
int tsrm_error(int level, const char *format, ...);

/* Read a resource from a thread's resource storage */
static int tsrm_error_level;
static FILE *tsrm_error_file;

#if TSRM_DEBUG
#define TSRM_ERROR(args) tsrm_error args
#define TSRM_SAFE_RETURN_RSRC(array, offset, range)																		\
	{																													\
		int unshuffled_offset = TSRM_UNSHUFFLE_RSRC_ID(offset);															\
																														\
		if (offset==0) {																								\
			return &array;																								\
		} else if ((unshuffled_offset)>=0 && (unshuffled_offset)<(range)) {												\
			TSRM_ERROR((TSRM_ERROR_LEVEL_INFO, "Successfully fetched resource id %d for thread id %ld - 0x%0.8X",		\
						unshuffled_offset, (long) thread_resources->thread_id, array[unshuffled_offset]));				\
			return array[unshuffled_offset];																			\
		} else {																										\
			TSRM_ERROR((TSRM_ERROR_LEVEL_ERROR, "Resource id %d is out of range (%d..%d)",								\
						unshuffled_offset, TSRM_SHUFFLE_RSRC_ID(0), TSRM_SHUFFLE_RSRC_ID(thread_resources->count-1)));	\
			return NULL;																								\
		}																												\
	}
#else
#define TSRM_ERROR(args)
#define TSRM_SAFE_RETURN_RSRC(array, offset, range)		\
	if (offset==0) {									\
		return &array;									\
	} else {											\
		return array[TSRM_UNSHUFFLE_RSRC_ID(offset)];	\
	}
#endif

#if defined(PTHREADS)
/* Thread local storage */
static pthread_key_t tls_key;
# define tsrm_tls_set(what)		pthread_setspecific(tls_key, (void*)(what))
# define tsrm_tls_get()			pthread_getspecific(tls_key)

#elif defined(TSRM_ST)
static int tls_key;
# define tsrm_tls_set(what)		st_thread_setspecific(tls_key, (void*)(what))
# define tsrm_tls_get()			st_thread_getspecific(tls_key)

#elif defined(TSRM_WIN32)
static DWORD tls_key;
# define tsrm_tls_set(what)		TlsSetValue(tls_key, (void*)(what))
# define tsrm_tls_get()			TlsGetValue(tls_key)

#elif defined(BETHREADS)
static int32 tls_key;
# define tsrm_tls_set(what)		tls_set(tls_key, (void*)(what))
# define tsrm_tls_get()			(tsrm_tls_entry*)tls_get(tls_key)

#else
# define tsrm_tls_set(what)
# define tsrm_tls_get()			NULL
# warning tsrm_set_interpreter_context is probably broken on this platform
#endif

/* Startup TSRM (call once for the entire process) 用于sapi调用，TSRM启动函数，整个生命周期只执行一遍，大部分调用参数 tsrm_startup(1, 1, 0, NULL); */
TSRM_API int tsrm_startup(int expected_threads, int expected_resources, int debug_level, char *debug_filename)
{				/* tsrm_startup(预期线程数,预期资源数,日志级别,日志文件) */
#if defined(GNUPTH)
	pth_init();
#elif defined(PTHREADS)
	pthread_key_create( &tls_key, 0 ); /* 用来创建线程私有数据。该函数从 TSD 池中分配一项，将其地址值赋给 key 供以后访问使用。第 2 个参数是一个销毁函数，它是可选的，可以为 NULL，为 NULL 时，则系统调用默认的销毁函数进行相关的数据注销*/
#elif defined(TSRM_ST)
	st_init();
	st_key_create(&tls_key, 0);
#elif defined(TSRM_WIN32)
	tls_key = TlsAlloc();
#elif defined(BETHREADS)
	tls_key = tls_allocate();
#endif

	tsrm_error_file = stderr; /* stderr:标准错误输出设备,对应终端的屏幕,将错误输出文件置为屏幕 */
	tsrm_error_set(debug_level, debug_filename); /* 大部分调用tsrm_startup函数的debug_level为0，表示不打日志，除了php5isapi */
	tsrm_tls_table_size = expected_threads;

	tsrm_tls_table = (tsrm_tls_entry **) calloc(tsrm_tls_table_size, sizeof(tsrm_tls_entry *)); /* 申请线程所需内存空间 */
	if (!tsrm_tls_table) {
		TSRM_ERROR((TSRM_ERROR_LEVEL_ERROR, "Unable to allocate TLS table"));
		return 0;
	}
	id_count=0;

	resource_types_table_size = expected_resources; /* 申请资源所需内存空间 */
	resource_types_table = (tsrm_resource_type *) calloc(resource_types_table_size, sizeof(tsrm_resource_type));
	if (!resource_types_table) {
		TSRM_ERROR((TSRM_ERROR_LEVEL_ERROR, "Unable to allocate resource types table"));
		free(tsrm_tls_table);
		tsrm_tls_table = NULL;
		return 0;
	}

	tsmm_mutex = tsrm_mutex_alloc(); /* 分配互斥锁将本线程锁定 */

	tsrm_new_thread_begin_handler = tsrm_new_thread_end_handler = NULL;

	TSRM_ERROR((TSRM_ERROR_LEVEL_CORE, "Started up TSRM, %d expected threads, %d expected resources", expected_threads, expected_resources));
	return 1;
}


/* Shutdown TSRM (call once for the entire process) TSRM关闭函数，整个生命周期只执行一遍 用于sapi调用，主要用于释放内存，结束线程 */
TSRM_API void tsrm_shutdown(void)
{
	int i;

	if (tsrm_tls_table) {
		for (i=0; i<tsrm_tls_table_size; i++) {
			tsrm_tls_entry *p = tsrm_tls_table[i], *next_p; /* 定义p指针并指向第i个线程资源，和下一个指针next_p */

			while (p) {
				int j;

				next_p = p->next;
				for (j=0; j<p->count; j++) {
					if (p->storage[j]) {
						if (resource_types_table && !resource_types_table[j].done && resource_types_table[j].dtor) {
							resource_types_table[j].dtor(p->storage[j], &p->storage);
						}
						free(p->storage[j]);
					}
				}
				free(p->storage);
				free(p);
				p = next_p;
			}
		}
		free(tsrm_tls_table);
		tsrm_tls_table = NULL;
	}
	if (resource_types_table) {
		free(resource_types_table);
		resource_types_table=NULL;
	}
	tsrm_mutex_free(tsmm_mutex);
	tsmm_mutex = NULL;
	TSRM_ERROR((TSRM_ERROR_LEVEL_CORE, "Shutdown TSRM"));
	if (tsrm_error_file!=stderr) {
		fclose(tsrm_error_file);
	}
#if defined(GNUPTH)
	pth_kill();
#elif defined(PTHREADS)
	pthread_setspecific(tls_key, 0);
	pthread_key_delete(tls_key);
#elif defined(TSRM_WIN32)
	TlsFree(tls_key);
#endif
}


/* allocates a new thread-safe-resource id 分配一个新的线程安全资源id */
TSRM_API ts_rsrc_id ts_allocate_id(ts_rsrc_id *rsrc_id, size_t size, ts_allocate_ctor ctor, ts_allocate_dtor dtor)
{
	int i;

	TSRM_ERROR((TSRM_ERROR_LEVEL_CORE, "Obtaining a new resource id, %d bytes", size));

	tsrm_mutex_lock(tsmm_mutex);

	/* obtain a resource id */
	*rsrc_id = TSRM_SHUFFLE_RSRC_ID(id_count++); /* 等价于((id_count++)+1) */
	TSRM_ERROR((TSRM_ERROR_LEVEL_CORE, "Obtained resource id %d", *rsrc_id));

	/* store the new resource type in the resource sizes table */
	if (resource_types_table_size < id_count) {
		resource_types_table = (tsrm_resource_type *) realloc(resource_types_table, sizeof(tsrm_resource_type)*id_count);
		if (!resource_types_table) {
			tsrm_mutex_unlock(tsmm_mutex);
			TSRM_ERROR((TSRM_ERROR_LEVEL_ERROR, "Unable to allocate storage for resource"));
			*rsrc_id = 0;
			return 0;
		}
		resource_types_table_size = id_count;
	}
	resource_types_table[TSRM_UNSHUFFLE_RSRC_ID(*rsrc_id)].size = size; /* TSRM_UNSHUFFLE_RSRC_ID(*rsrc_id)等价于((*rsrc_id)-1) */
	resource_types_table[TSRM_UNSHUFFLE_RSRC_ID(*rsrc_id)].ctor = ctor;
	resource_types_table[TSRM_UNSHUFFLE_RSRC_ID(*rsrc_id)].dtor = dtor;
	resource_types_table[TSRM_UNSHUFFLE_RSRC_ID(*rsrc_id)].done = 0;

	/* enlarge the arrays for the already active threads */
	for (i=0; i<tsrm_tls_table_size; i++) {
		tsrm_tls_entry *p = tsrm_tls_table[i];

		while (p) {
			if (p->count < id_count) {
				int j;

				p->storage = (void *) realloc(p->storage, sizeof(void *)*id_count);
				for (j=p->count; j<id_count; j++) {
					p->storage[j] = (void *) malloc(resource_types_table[j].size);
					if (resource_types_table[j].ctor) {
						resource_types_table[j].ctor(p->storage[j], &p->storage);
					}
				}
				p->count = id_count;
			}
			p = p->next;
		}
	}
	tsrm_mutex_unlock(tsmm_mutex);

	TSRM_ERROR((TSRM_ERROR_LEVEL_CORE, "Successfully allocated new resource id %d", *rsrc_id));
	return *rsrc_id;
}

/* 获取一个新的资源 */
static void allocate_new_resource(tsrm_tls_entry **thread_resources_ptr, THREAD_T thread_id)
{
	int i;

	TSRM_ERROR((TSRM_ERROR_LEVEL_CORE, "Creating data structures for thread %x", thread_id));
	(*thread_resources_ptr) = (tsrm_tls_entry *) malloc(sizeof(tsrm_tls_entry));
	(*thread_resources_ptr)->storage = (void **) malloc(sizeof(void *)*id_count);
	(*thread_resources_ptr)->count = id_count;
	(*thread_resources_ptr)->thread_id = thread_id;
	(*thread_resources_ptr)->next = NULL;

	/* Set thread local storage to this new thread resources structure */
	tsrm_tls_set(*thread_resources_ptr);

	if (tsrm_new_thread_begin_handler) {
		tsrm_new_thread_begin_handler(thread_id, &((*thread_resources_ptr)->storage));
	}
	for (i=0; i<id_count; i++) {
		if (resource_types_table[i].done) {
			(*thread_resources_ptr)->storage[i] = NULL;
		} else
		{
			(*thread_resources_ptr)->storage[i] = (void *) malloc(resource_types_table[i].size);
			if (resource_types_table[i].ctor) {
				resource_types_table[i].ctor((*thread_resources_ptr)->storage[i], &(*thread_resources_ptr)->storage);
			}
		}
	}

	if (tsrm_new_thread_end_handler) {
		tsrm_new_thread_end_handler(thread_id, &((*thread_resources_ptr)->storage));
	}

	tsrm_mutex_unlock(tsmm_mutex);
}


/* fetches the requested resource for the current thread */
TSRM_API void *ts_resource_ex(ts_rsrc_id id, THREAD_T *th_id)
{
	THREAD_T thread_id;
	int hash_value;
	tsrm_tls_entry *thread_resources;

#ifdef NETWARE
	/* The below if loop is added for NetWare to fix an abend while unloading PHP
	 * when an Apache unload command is issued on the system console.
	 * While exiting from PHP, at the end for some reason, this function is called
	 * with tsrm_tls_table = NULL. When this happened, the server abends when
	 * tsrm_tls_table is accessed since it is NULL.
	 */
	if(tsrm_tls_table) {
#endif
	if (!th_id) {
		/* Fast path for looking up the resources for the current
		 * thread. Its used by just about every call to
		 * ts_resource_ex(). This avoids the need for a mutex lock
		 * and our hashtable lookup.
		 */
		thread_resources = tsrm_tls_get();

		if (thread_resources) {
			TSRM_ERROR((TSRM_ERROR_LEVEL_INFO, "Fetching resource id %d for current thread %d", id, (long) thread_resources->thread_id));
			/* Read a specific resource from the thread's resources.
			 * This is called outside of a mutex, so have to be aware about external
			 * changes to the structure as we read it.
			 */
			TSRM_SAFE_RETURN_RSRC(thread_resources->storage, id, thread_resources->count);
		}
		thread_id = tsrm_thread_id();
	} else {
		thread_id = *th_id;
	}

	TSRM_ERROR((TSRM_ERROR_LEVEL_INFO, "Fetching resource id %d for thread %ld", id, (long) thread_id));
	tsrm_mutex_lock(tsmm_mutex);

	hash_value = THREAD_HASH_OF(thread_id, tsrm_tls_table_size);
	thread_resources = tsrm_tls_table[hash_value];

	if (!thread_resources) {
		allocate_new_resource(&tsrm_tls_table[hash_value], thread_id);
		return ts_resource_ex(id, &thread_id);
	} else {
		 do {
			if (thread_resources->thread_id == thread_id) {
				break;
			}
			if (thread_resources->next) {
				thread_resources = thread_resources->next;
			} else {
				allocate_new_resource(&thread_resources->next, thread_id);
				return ts_resource_ex(id, &thread_id);
				/*
				 * thread_resources = thread_resources->next;
				 * break;
				 */
			}
		 } while (thread_resources);
	}
	tsrm_mutex_unlock(tsmm_mutex);
	/* Read a specific resource from the thread's resources.
	 * This is called outside of a mutex, so have to be aware about external
	 * changes to the structure as we read it.
	 */
	TSRM_SAFE_RETURN_RSRC(thread_resources->storage, id, thread_resources->count);
#ifdef NETWARE
	}	/* if(tsrm_tls_table) */
#endif
}

/* frees an interpreter context.  You are responsible for making sure that
 * it is not linked into the TSRM hash, and not marked as the current interpreter */
void tsrm_free_interpreter_context(void *context)
{
	tsrm_tls_entry *next, *thread_resources = (tsrm_tls_entry*)context;
	int i;

	while (thread_resources) {
		next = thread_resources->next;

		for (i=0; i<thread_resources->count; i++) {
			if (resource_types_table[i].dtor) {
				resource_types_table[i].dtor(thread_resources->storage[i], &thread_resources->storage);
			}
		}
		for (i=0; i<thread_resources->count; i++) {
			free(thread_resources->storage[i]);
		}
		free(thread_resources->storage);
		free(thread_resources);
		thread_resources = next;
	}
}

void *tsrm_set_interpreter_context(void *new_ctx)
{
	tsrm_tls_entry *current;

	current = tsrm_tls_get();

	/* TODO: unlink current from the global linked list, and replace it
	 * it with the new context, protected by mutex where/if appropriate */

	/* Set thread local storage to this new thread resources structure */
	tsrm_tls_set(new_ctx);

	/* return old context, so caller can restore it when they're done */
	return current;
}


/* allocates a new interpreter context */
void *tsrm_new_interpreter_context(void)
{
	tsrm_tls_entry *new_ctx, *current;
	THREAD_T thread_id;

	thread_id = tsrm_thread_id();
	tsrm_mutex_lock(tsmm_mutex);

	current = tsrm_tls_get();

	allocate_new_resource(&new_ctx, thread_id);
	
	/* switch back to the context that was in use prior to our creation
	 * of the new one */
	return tsrm_set_interpreter_context(current);
}


/* frees all resources allocated for the current thread 释放当前线程获得的所有资源，与下面的函数很相似 */
void ts_free_thread(void)
{
	tsrm_tls_entry *thread_resources;
	int i;
	THREAD_T thread_id = tsrm_thread_id();
	int hash_value;
	tsrm_tls_entry *last=NULL;

	tsrm_mutex_lock(tsmm_mutex);
	hash_value = THREAD_HASH_OF(thread_id, tsrm_tls_table_size);
	thread_resources = tsrm_tls_table[hash_value];

	while (thread_resources) {
		if (thread_resources->thread_id == thread_id) {
			for (i=0; i<thread_resources->count; i++) {
				if (resource_types_table[i].dtor) {
					resource_types_table[i].dtor(thread_resources->storage[i], &thread_resources->storage);
				}
			}
			for (i=0; i<thread_resources->count; i++) {
				free(thread_resources->storage[i]);
			}
			free(thread_resources->storage);
			if (last) {
				last->next = thread_resources->next;
			} else {
				tsrm_tls_table[hash_value] = thread_resources->next;
			}
			tsrm_tls_set(0);
			free(thread_resources);
			break;
		}
		if (thread_resources->next) {
			last = thread_resources;
		}
		thread_resources = thread_resources->next;
	}
	tsrm_mutex_unlock(tsmm_mutex);
}


/* frees all resources allocated for all threads except current 释放所有线程获得的所有资源，除了当前线程 */
void ts_free_worker_threads(void)
{
	tsrm_tls_entry *thread_resources;
	int i;
	THREAD_T thread_id = tsrm_thread_id(); /* 获取当前的线程id */
	int hash_value;
	tsrm_tls_entry *last=NULL;

	tsrm_mutex_lock(tsmm_mutex);
	hash_value = THREAD_HASH_OF(thread_id, tsrm_tls_table_size); /* 等价取余thread_id%tsrm_tls_table_size */
	thread_resources = tsrm_tls_table[hash_value];

	while (thread_resources) {
		if (thread_resources->thread_id != thread_id) { /* 如果线程id不等于当前线程id则释放 */
			for (i=0; i<thread_resources->count; i++) {
				if (resource_types_table[i].dtor) {
					resource_types_table[i].dtor(thread_resources->storage[i], &thread_resources->storage);
				}
			}
			for (i=0; i<thread_resources->count; i++) {
				free(thread_resources->storage[i]);
			}
			free(thread_resources->storage);
			if (last) {
				last->next = thread_resources->next;
			} else {
				tsrm_tls_table[hash_value] = thread_resources->next;
			}
			free(thread_resources);
			if (last) {
				thread_resources = last->next;
			} else {
				thread_resources = tsrm_tls_table[hash_value];
			}
		} else { /* 如果线程id等于当前线程id */
			if (thread_resources->next) {
				last = thread_resources;
			}
			thread_resources = thread_resources->next;
		}
	}
	tsrm_mutex_unlock(tsmm_mutex);
}


/* deallocates all occurrences of a given id 释放所有的当前资源id */
void ts_free_id(ts_rsrc_id id) /* ts_rsrc_id为int */
{
	int i;
	int j = TSRM_UNSHUFFLE_RSRC_ID(id); /* 等价((id)-1) */

	tsrm_mutex_lock(tsmm_mutex);

	TSRM_ERROR((TSRM_ERROR_LEVEL_CORE, "Freeing resource id %d", id));

	if (tsrm_tls_table) {
		for (i=0; i<tsrm_tls_table_size; i++) {
			tsrm_tls_entry *p = tsrm_tls_table[i];

			while (p) {
				if (p->count > j && p->storage[j]) {
					if (resource_types_table && resource_types_table[j].dtor) {
						resource_types_table[j].dtor(p->storage[j], &p->storage);
					}
					free(p->storage[j]);
					p->storage[j] = NULL;
				}
				p = p->next;
			}
		}
	}
	resource_types_table[j].done = 1;

	tsrm_mutex_unlock(tsmm_mutex);

	TSRM_ERROR((TSRM_ERROR_LEVEL_CORE, "Successfully freed resource id %d", id));
}




/*
 * Utility Functions 公用函数
 */

/* Obtain the current thread id 获得当前线程id */
TSRM_API THREAD_T tsrm_thread_id(void)
{
#ifdef TSRM_WIN32 /* 如果是Windows系统中编译则定义这个宏，见TSRM.h第16行 */
	return GetCurrentThreadId();
#elif defined(GNUPTH) /* 在tsrm.m4文件中通过AC_DEFINE宏定义 */
	return pth_self();
#elif defined(PTHREADS) /* 在tsrm.m4文件中通过AC_DEFINE宏定义 */
	return pthread_self();
#elif defined(NSAPI) /* 在sapi/nsapi/nsapi.c文件中定义,只有以NSAPI形式运行PHP是定义 */
	return systhread_current();
#elif defined(PI3WEB)
	return PIThread_getCurrent();
#elif defined(TSRM_ST) /* 在tsrm.m4文件中通过AC_DEFINE宏定义 */
	return st_thread_self();
#elif defined(BETHREADS) /* 在tsrm.m4文件中通过AC_DEFINE宏定义 */
	return find_thread(NULL);
#endif
}


/* Allocate a mutex 分配一个互斥锁 */
TSRM_API MUTEX_T tsrm_mutex_alloc(void)
{
	MUTEX_T mutexp; /* MUTEX_T宏定义在TSRM.H中 例如define MUTEX_T pthread_mutex_t *等 */
#ifdef TSRM_WIN32
	mutexp = malloc(sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(mutexp);
#elif defined(GNUPTH)
	mutexp = (MUTEX_T) malloc(sizeof(*mutexp));
	pth_mutex_init(mutexp);
#elif defined(PTHREADS)
	mutexp = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(mutexp,NULL);
#elif defined(NSAPI)
	mutexp = crit_init();
#elif defined(PI3WEB)
	mutexp = PIPlatform_allocLocalMutex();
#elif defined(TSRM_ST)
	mutexp = st_mutex_new();
#elif defined(BETHREADS)
	mutexp = (beos_ben*)malloc(sizeof(beos_ben));
	mutexp->ben = 0;
	mutexp->sem = create_sem(1, "PHP sempahore"); 
#endif
#ifdef THR_DEBUG
	printf("Mutex created thread: %d\n",mythreadid());
#endif
	return( mutexp );
}


/* Free a mutex 互斥锁销毁 成功返回0 */
TSRM_API void tsrm_mutex_free(MUTEX_T mutexp)
{
	if (mutexp) {
#ifdef TSRM_WIN32
		DeleteCriticalSection(mutexp);
		free(mutexp);
#elif defined(GNUPTH)
		free(mutexp);
#elif defined(PTHREADS)
		pthread_mutex_destroy(mutexp);
		free(mutexp);
#elif defined(NSAPI)
		crit_terminate(mutexp);
#elif defined(PI3WEB)
		PISync_delete(mutexp);
#elif defined(TSRM_ST)
		st_mutex_destroy(mutexp);
#elif defined(BETHREADS)
		delete_sem(mutexp->sem);
		free(mutexp);  
#endif
	}
#ifdef THR_DEBUG
	printf("Mutex freed thread: %d\n",mythreadid());
#endif
}


/*
  Lock a mutex. 互斥锁加锁
  A return value of 0 indicates success 成功返回0
*/
TSRM_API int tsrm_mutex_lock(MUTEX_T mutexp)
{
	TSRM_ERROR((TSRM_ERROR_LEVEL_INFO, "Mutex locked thread: %ld", tsrm_thread_id()));
#ifdef TSRM_WIN32
	EnterCriticalSection(mutexp);
	return 0;
#elif defined(GNUPTH)
	if (pth_mutex_acquire(mutexp, 0, NULL)) {
		return 0;
	}
	return -1;
#elif defined(PTHREADS)
	return pthread_mutex_lock(mutexp); /* 线程调用该函数让互斥锁上锁，如果该互斥锁已被另一个线程锁定和拥有，则调用该线程将阻塞，直到该互斥锁变为可用为止 */
#elif defined(NSAPI)
	crit_enter(mutexp);
	return 0;
#elif defined(PI3WEB)
	return PISync_lock(mutexp);
#elif defined(TSRM_ST)
	return st_mutex_lock(mutexp);
#elif defined(BETHREADS)
	if (atomic_add(&mutexp->ben, 1) != 0)  
		return acquire_sem(mutexp->sem);   
	return 0;
#endif
}


/*
  Unlock a mutex. 互斥锁解锁
  A return value of 0 indicates success 成功返回0
*/
TSRM_API int tsrm_mutex_unlock(MUTEX_T mutexp)
{
	TSRM_ERROR((TSRM_ERROR_LEVEL_INFO, "Mutex unlocked thread: %ld", tsrm_thread_id()));
#ifdef TSRM_WIN32
	LeaveCriticalSection(mutexp);
	return 0;
#elif defined(GNUPTH)
	if (pth_mutex_release(mutexp)) {
		return 0;
	}
	return -1;
#elif defined(PTHREADS)
	return pthread_mutex_unlock(mutexp);
#elif defined(NSAPI)
	crit_exit(mutexp);
	return 0;
#elif defined(PI3WEB)
	return PISync_unlock(mutexp);
#elif defined(TSRM_ST)
	return st_mutex_unlock(mutexp);
#elif defined(BETHREADS)
	if (atomic_add(&mutexp->ben, -1) != 1) 
		return release_sem(mutexp->sem);
	return 0;   
#endif
}

/*
  Changes the signal mask of the calling thread 更改调用线程的信号掩码
*/
#ifdef HAVE_SIGPROCMASK
TSRM_API int tsrm_sigmask(int how, const sigset_t *set, sigset_t *oldset)
{
	TSRM_ERROR((TSRM_ERROR_LEVEL_INFO, "Changed sigmask in thread: %ld", tsrm_thread_id()));/*打印更改的线程id日志*/
	/* TODO: add support for other APIs */
#ifdef PTHREADS
	return pthread_sigmask(how, set, oldset); /* 用作在主调线程里控制信号掩码 */
#else
	return sigprocmask(how, set, oldset); /* 用于改变进程的当前阻塞信号集,也可以用来检测当前进程的信号掩码 */
#endif
}
#endif


TSRM_API void *tsrm_set_new_thread_begin_handler(tsrm_thread_begin_func_t new_thread_begin_handler)
{ /*设置线程初始句柄，没太明白，后期补充*/
	void *retval = (void *) tsrm_new_thread_begin_handler;

	tsrm_new_thread_begin_handler = new_thread_begin_handler;
	return retval;
}


TSRM_API void *tsrm_set_new_thread_end_handler(tsrm_thread_end_func_t new_thread_end_handler)
{ /*设置线程结束句柄，没太明白，后期补充*/
	void *retval = (void *) tsrm_new_thread_end_handler;

	tsrm_new_thread_end_handler = new_thread_end_handler;
	return retval;
}



/*
 * Debug support
 */

#if TSRM_DEBUG
int tsrm_error(int level, const char *format, ...)
{/* 例如 tsrm_error(1,"%d %f %s", 123, 2.3, "abc")会输出TSRM: 123 2.3 abc到屏幕上 */
	if (level<=tsrm_error_level) {
		va_list args; /* va_list 是在C语言中解决变参问题的一组宏，所在头文件：<stdarg.h> */
		int size;

		fprintf(tsrm_error_file, "TSRM:  ");
		va_start(args, format);
		size = vfprintf(tsrm_error_file, format, args); /* vfprintf()会根据参数format字符串来转换并格式化数据，然后将结果输出到参数stream指定的文件中 */
		va_end(args);/* 关于vfprintf函数的具体用法，百度百科中很详细 */
		fprintf(tsrm_error_file, "\n");
		fflush(tsrm_error_file);
		return size;
	} else {
		return 0;
	}
}
#endif


void tsrm_error_set(int level, char *debug_filename) /* 该函数为设置错误级别和错误输出文件，只有tsrm_startup()函数调用 */
{
	tsrm_error_level = level; /* 设置错误级别，一共3个级别，在TSRM.h文件128-130行定义 */

#if TSRM_DEBUG
	if (tsrm_error_file!=stderr) { /* close files opened earlier */
		fclose(tsrm_error_file); /* 如果错误输出文件不是终端屏幕，先关闭之前打开的文件 */
	}

	if (debug_filename) {
		tsrm_error_file = fopen(debug_filename, "w"); /* 如果文件存在，以可写的形式打开该文件 */
		if (!tsrm_error_file) {
			tsrm_error_file = stderr; /* 如果文件打开失败，将错误输出文件改成终端屏幕 */
		}
	} else {
		tsrm_error_file = stderr; /* 如果文件不存在，将错误输出文件改成终端屏幕 */
	}
#endif
}

#endif /* ZTS */
