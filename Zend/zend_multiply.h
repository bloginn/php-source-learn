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
   | Authors: Sascha Schumann <sascha@schumann.cx>                        |
   |          Ard Biesheuvel <ard.biesheuvel@linaro.org>                  |
   +----------------------------------------------------------------------+
*/

/* $Id$ */
/* ASM: assemble汇编，ZEND_SIGNED_MULTIPLY_LONG用于LONG类型的符号相乘 */
#if defined(__i386__) && defined(__GNUC__)

#define ZEND_SIGNED_MULTIPLY_LONG(a, b, lval, dval, usedval) do {	\ /* __asm__或asm用来声明一个内联汇编表达式,__asm__是GCC关键字asm的宏定义 */
	long __tmpvar; 													\ /* __asm__(汇编语句模板: 输出部分: 输入部分: 破坏描述部分) */
	__asm__ ("imul %3,%0\n"											\ /* imul 带符号数乘法指令 IMUL(Integer MULtiply) */
		"adc $0,%1" 												\ /* adc (Add Carry): 带进位加 */
			: "=r"(__tmpvar),"=r"(usedval) 							\ /* imul %3,%0指令的结果输入到__tmpvar，adc $0,%1指令的结果输入到usedval */
			: "0"(a), "r"(b), "1"(0));								\ /* 正常情况是%0为__tmpvar,%1为usedval,%2为a,%3为b,%4为0,由于(a)前面加了0,所以%0为a,%3为b,$1为0 */
	if (usedval) (dval) = (double) (a) * (double) (b);				\
	else (lval) = __tmpvar;											\
} while (0)

#elif defined(__x86_64__) && defined(__GNUC__)

#define ZEND_SIGNED_MULTIPLY_LONG(a, b, lval, dval, usedval) do {	\
	long __tmpvar; 													\
	__asm__ ("imul %3,%0\n"											\
		"adc $0,%1" 												\
			: "=r"(__tmpvar),"=r"(usedval) 							\
			: "0"(a), "r"(b), "1"(0));								\
	if (usedval) (dval) = (double) (a) * (double) (b);				\
	else (lval) = __tmpvar;											\
} while (0)

#elif defined(__arm__) && defined(__GNUC__)

#define ZEND_SIGNED_MULTIPLY_LONG(a, b, lval, dval, usedval) do {	\
	long __tmpvar; 													\
	__asm__("smull %0, %1, %2, %3\n"								\
		"sub %1, %1, %0, asr #31"									\
			: "=r"(__tmpvar), "=r"(usedval)							\
			: "r"(a), "r"(b));										\
	if (usedval) (dval) = (double) (a) * (double) (b);				\
	else (lval) = __tmpvar;											\
} while (0)

#elif defined(__aarch64__) && defined(__GNUC__)

#define ZEND_SIGNED_MULTIPLY_LONG(a, b, lval, dval, usedval) do {	\
	long __tmpvar; 													\
	__asm__("mul %0, %2, %3\n"										\
		"smulh %1, %2, %3\n"										\
		"sub %1, %1, %0, asr #63\n"									\
			: "=X"(__tmpvar), "=X"(usedval)							\
			: "X"(a), "X"(b));										\
	if (usedval) (dval) = (double) (a) * (double) (b);				\
	else (lval) = __tmpvar;											\
} while (0)

#elif SIZEOF_LONG == 4 && defined(HAVE_ZEND_LONG64)

#define ZEND_SIGNED_MULTIPLY_LONG(a, b, lval, dval, usedval) do {	\
	zend_long64 __result = (zend_long64) (a) * (zend_long64) (b);	\
	if (__result > LONG_MAX || __result < LONG_MIN) {				\
		(dval) = (double) __result;									\
		(usedval) = 1;												\
	} else {														\
		(lval) = (long) __result;									\
		(usedval) = 0;												\
	}																\
} while (0)

#else

#define ZEND_SIGNED_MULTIPLY_LONG(a, b, lval, dval, usedval) do {	\
	long   __lres  = (a) * (b);										\
	long double __dres  = (long double)(a) * (long double)(b);		\
	long double __delta = (long double) __lres - __dres;			\
	if ( ((usedval) = (( __dres + __delta ) != __dres))) {			\ /* 实际上是__lres是不是等于__dres */
		(dval) = __dres;											\ /* 如果不等于则执行(dval) = __dres,如果等于执行(lval) = __lres */
	} else {														\
		(lval) = __lres;											\
	}																\
} while (0)

#endif

/*
__asm__ __violate__ ("movl %1,%0" : "=r" (result) : "m" (input));

"movl %1,%0"是指令模板；"%0"和"%1"代表指令的操作数，称为占位符，内嵌汇编靠它们将C 语言表达式与指令操作数相对应。指令模板后面用小括号括起来的是C语言表达式，
本例中只有两个："result"和"input"，他们按照出现的顺序分 别与指令操作数"%0"，"%1"对应；注意对应顺序：第一个C 表达式对应"%0"；第二个表达式对应"%1"，依次类推，
操作数至多有10 个，分别用"%0","%1"...."%9"表示。在每个操作数前面有一个用引号括起来的字符串，字符串的内容是对该操作数的限制或者说要求。 
"result"前面的限制字符串是"=r"，其中"="表示"result"是输出操作数，"r" 表示需要将"result"与某个通用寄存器相关联，
先将操作数的值读入寄存器，然后在指令中使用相应寄存器，而不是"result"本身，当然指令执行 完后需要将寄存器中的值存入变量"result"，
从表面上看好像是指令直接对"result"进行操作，实际上GCC做了隐式处理，这样我们可以少写一 些指令。"input"前面的"r"表示该表达式需要先放入某个寄存器，然后在指令中使用该寄存器参加运算。
C表达式或者变量与寄存器的关系由GCC自动处理，我们只需使用限制字符串指导GCC如何处理即可。限制字符必须与指令对操作数的要求相匹配，
否则产生的 汇编代码将会有错，读者可以将上例中的两个"r"，都改为"m"(m表示操作数放在内存，而不是寄存器中)，编译后得到的结果是：movl input, result
很明显这是一条非法指令，因此限制字符串必须与指令对操作数的要求匹配。例如指令movl允许寄存器到寄存器，立即数到寄存器等，但是不允许内存到内存的操作，因此两个操作数不能同时使用"m"作为限定字符。
内嵌汇编语法如下：
       __asm__(汇编语句模板: 输出部分: 输入部分: 破坏描述部分)
共四个部分：汇编语句模板，输出部分，输入部分，破坏描述部分，各部分使用":"格开，汇编语句模板必不可少，其他三部分可选，如果使用了后面的部分，而前面部分为空，也需要用":"格开，相应部分内容为空。例如：
 __asm__ __volatile__("cli": : :"memory")
*/
