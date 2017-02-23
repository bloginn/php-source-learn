/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) 1998-2009 Zend Technologies Ltd. (http://www.zend.com) |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.00 of the Zend license,     |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.zend.com/license/2_00.txt.                                |
   | If you did not receive a copy of the Zend license and are unable to  |
   | obtain it through the world-wide-web, please send a note to          |
   | license@zend.com so we can mail you a copy immediately.              |
   +----------------------------------------------------------------------+
   | Authors: David Soria Parra <david.soriaparra@sun.com>                |
   +----------------------------------------------------------------------+
*/

/* $Id: $ */

provider php {
	probe exception__caught(char *classname);
	probe exception__thrown(char* classname);
	probe request__startup(char* request_file, char* request_uri, char* request_method);
	probe request__shutdown(char* request_file, char* request_uri, char* request_method);
	probe compile__file__entry(char * compile_file, char *compile_file_translated);
	probe compile__file__return(char *compile_file, char *compile_file_translated);
	probe error(char *errormsg, char *request_file, int lineno);
	probe execute__entry(char* request_file, int lineno);
	probe execute__return(char* request_file, int lineno);
	probe function__entry(char* function_name, char* request_file, int lineno, char* classname, char* scope);
	probe function__return(char* function_name, char* request_file, int lineno, char* classname, char* scope);
};

/*#pragma D attributes Private/Private/Unknown provider php module
#pragma D attributes Private/Private/Unknown provider php function
#pragma D attributes Evolving/Evolving/Common provider php provider */

/*

dtrace是性能分析工具，可以调试用，不是很了解，时间关系，后期研究
可以参考PHP官网文档 http://php.net/manual/zh/features.dtrace.systemtap.php

PHP 和 DTrace 介绍

包括 Solaris，Mac OS X，Oracle Linux 和 BSD 在内的很多操作系统 都提供了 DTrace 跟踪调试框架，它永远可用，并且额外消耗极低。 
DTrace 可以跟踪操作系统行为和用户程序的执行情况。 
它可以显示参数值，也可以用来分析性能问题。 用户可以使用 DTrace D 脚本语言创建脚本文件来监控探针，进而高效分析数据指针。

除非用户使用 DTrace D 脚本激活并监控 PHP 探针，否则它并不会运行，所以不会给正常的应用执行带来任何性能损耗。 
即使 PHP 探针被激活，它的消耗也是非常低的，甚至可以直接在生产系统中使用。

在运行时可以激活 PHP “用户级静态定义跟踪”（USDT）探针。 举例说明，如果 D 脚本正在监控 PHP function-entry 探针， 
那么，每当 PHP 脚本函数被调用的时候，这个探针将被触发，同时 D 脚本中所关联的动作代码将被执行。 在脚本的动作代码中，
可以打印出诸如函数所在源文件等信息的探针参数， 也可以记录诸如函数执行次数这样的聚合数据。

这里仅描述了 PHP USDT 探针。请参考各操作系统文档中关于 DTrace 的部分获取更多信息， 例如，如何使用 DTrace 跟踪函数调用，
如何跟踪操作系统行为等。 需要注意的是，不同平台提供的 DTrace 功能并不完全相同。

从 PHP 5.4 开始加入 DTrace 静态探针，之前的版本需要使用 » PECL 扩展来实现跟踪功能。这个扩展现在已经废弃。

在某些 Linux 发行版中，可以使用 SystemTap 工具来监控 PHP DTrace 静态探针

*/