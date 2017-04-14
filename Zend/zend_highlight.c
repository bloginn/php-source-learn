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
#include <zend_language_parser.h>
#include "zend_compile.h"
#include "zend_highlight.h"
#include "zend_ptr_stack.h"
#include "zend_globals.h"

ZEND_API void zend_html_putc(char c) /* 仅在本文件的zend_html_puts函数中调用 */
{ /* 该函数是为了转换特殊的HTML字符，然后打印，例如空格转成HTML的&nbsp; */
	switch (c) {
		case '\n':
			ZEND_PUTS("<br />"); /* 等价write_func(("<br />"), strlen(("<br />"))) */
			break;
		case '<':
			ZEND_PUTS("&lt;"); /* 等价write_func(("&lt;"), strlen(("&lt;"))) */
			break;
		case '>':
			ZEND_PUTS("&gt;"); /* 等价write_func(("&gt;"), strlen(("&gt;"))) */
			break;
		case '&':
			ZEND_PUTS("&amp;"); /* 等价write_func(("&amp;"), strlen(("&amp;"))) */
			break;
		case ' ':
			ZEND_PUTS("&nbsp;"); /* 等价write_func(("&nbsp;"), strlen(("&nbsp;"))) */
			break;
		case '\t':
			ZEND_PUTS("&nbsp;&nbsp;&nbsp;&nbsp;"); /* 等价write_func(("&nbsp;&nbsp;&nbsp;&nbsp;"), strlen(("&nbsp;&nbsp;&nbsp;&nbsp;"))) */
			break;
		default:
			ZEND_PUTC(c); /* 等价write_func((c), strlen((c))) */
			break;
	}
}


ZEND_API void zend_html_puts(const char *s, uint len TSRMLS_DC) /* 将字符串转义成HTML文本 */
{
	const unsigned char *ptr = (const unsigned char*)s, *end = ptr + len;
	unsigned char *filtered = NULL;
	size_t filtered_len;

	if (LANG_SCNG(output_filter)) { /* LANG_SCNG(output_filter)等价language_scanner_globals.output_filter，输出过滤器 */
		LANG_SCNG(output_filter)(&filtered, &filtered_len, ptr, len TSRMLS_CC);
		ptr = filtered;
		end = filtered + filtered_len;
	}

	while (ptr<end) {
		if (*ptr==' ') {
			do {
				zend_html_putc(*ptr);
			} while ((++ptr < end) && (*ptr==' '));
		} else {
			zend_html_putc(*ptr++);
		}
	}

	if (LANG_SCNG(output_filter)) {
		efree(filtered);
	}
}


ZEND_API void zend_highlight(zend_syntax_highlighter_ini *syntax_highlighter_ini TSRMLS_DC)/* PHP代码高亮输出 */
{
	zval token;
	int token_type;
	char *last_color = syntax_highlighter_ini->highlight_html;
	char *next_color;

	zend_printf("<code>");
	zend_printf("<span style=\"color: %s\">\n", last_color);
	/* highlight stuff coming back from zendlex() */
	token.type = 0;
	while ((token_type=lex_scan(&token TSRMLS_CC))) { /* 调用词法解析器解析代码 */
		switch (token_type) {
			case T_INLINE_HTML:
				next_color = syntax_highlighter_ini->highlight_html;
				break;
			case T_COMMENT:
			case T_DOC_COMMENT:
				next_color = syntax_highlighter_ini->highlight_comment;
				break;
			case T_OPEN_TAG:
			case T_OPEN_TAG_WITH_ECHO:
				next_color = syntax_highlighter_ini->highlight_default;
				break;
			case T_CLOSE_TAG:
				next_color = syntax_highlighter_ini->highlight_default;
				break;
			case '"':
			case T_ENCAPSED_AND_WHITESPACE:
			case T_CONSTANT_ENCAPSED_STRING:
				next_color = syntax_highlighter_ini->highlight_string;
				break;
			case T_WHITESPACE:
				zend_html_puts((char*)LANG_SCNG(yy_text), LANG_SCNG(yy_leng) TSRMLS_CC);  /* no color needed */
				token.type = 0;
				continue;
				break;
			default:
				if (token.type == 0) {
					next_color = syntax_highlighter_ini->highlight_keyword;
				} else {
					next_color = syntax_highlighter_ini->highlight_default;
				}
				break;
		}

		if (last_color != next_color) {
			if (last_color != syntax_highlighter_ini->highlight_html) {
				zend_printf("</span>");
			}
			last_color = next_color;
			if (last_color != syntax_highlighter_ini->highlight_html) {
				zend_printf("<span style=\"color: %s\">", last_color);
			}
		}

		zend_html_puts((char*)LANG_SCNG(yy_text), LANG_SCNG(yy_leng) TSRMLS_CC);

		if (token.type == IS_STRING) {
			switch (token_type) {
				case T_OPEN_TAG:
				case T_OPEN_TAG_WITH_ECHO:
				case T_CLOSE_TAG:
				case T_WHITESPACE:
				case T_COMMENT:
				case T_DOC_COMMENT:
					break;
				default:
					str_efree(token.value.str.val);
					break;
			}
		}
		token.type = 0;
	}

	if (last_color != syntax_highlighter_ini->highlight_html) {
		zend_printf("</span>\n");
	}
	zend_printf("</span>\n");
	zend_printf("</code>");
}

ZEND_API void zend_strip(TSRMLS_D) /* strip剥除的意思，很明显，该函数是去除PHP代码无效内容 */
{
	zval token;
	int token_type;
	int prev_space = 0;

	token.type = 0;
	while ((token_type=lex_scan(&token TSRMLS_CC))) {
		switch (token_type) {
			case T_WHITESPACE: /* 去除多余空格，即连续空格只保留一个 */
				if (!prev_space) {
					zend_write(" ", sizeof(" ") - 1);
					prev_space = 1;
				}
						/* lack of break; is intentional */
			case T_COMMENT: /* 去除注释 */
			case T_DOC_COMMENT: /* 去除注释 */
				token.type = 0;
				continue;
			
			case T_END_HEREDOC: /* 去除heredoc end */
				zend_write((char*)LANG_SCNG(yy_text), LANG_SCNG(yy_leng));
				/* read the following character, either newline or ; */
				if (lex_scan(&token TSRMLS_CC) != T_WHITESPACE) {
					zend_write((char*)LANG_SCNG(yy_text), LANG_SCNG(yy_leng));
				}
				zend_write("\n", sizeof("\n") - 1);
				prev_space = 1;
				token.type = 0;
				continue;

			default:
				zend_write((char*)LANG_SCNG(yy_text), LANG_SCNG(yy_leng));
				break;
		}

		if (token.type == IS_STRING) {
			switch (token_type) {
				case T_OPEN_TAG:
				case T_OPEN_TAG_WITH_ECHO:
				case T_CLOSE_TAG:
				case T_WHITESPACE:
				case T_COMMENT:
				case T_DOC_COMMENT:
					break;

				default:
					STR_FREE(token.value.str.val); /* 等价于 if (token.value.str.val) { str_efree(token.value.str.val); }*/
					break;
			}
		}
		prev_space = token.type = 0;
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */

