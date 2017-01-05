#include <stdio.h>

#include "tsrm_config_common.h"
#include "tsrm_strtok_r.h"
/* static 表示该函数只限本文件访问 inline内联函数 请求编译器在此函数的被调用处将此函数实现插入，而不是像普通函数那样生成调用代码*/
static inline int in_character_class(char ch, const char *delim)
{
	while (*delim) {
		if (*delim == ch) {
			return 1;
		}
		delim++;
	}
	return 0;
}

char *tsrm_strtok_r(char *s, const char *delim, char **last)
{
	char *token;

	if (s == NULL) {
		s = *last;
	}

	while (*s && in_character_class(*s, delim)) {
		s++;
	}
	if (!*s) {
		return NULL;
	}

	token = s;

	while (*s && !in_character_class(*s, delim)) {
		s++;
	}
	if (!*s) {
		*last = s;
	} else {
		*s = '\0';
		*last = s + 1;
	}
	return token;
}

#if 0

main()
{
	char foo[] = "/foo/bar//\\barbara";
	char *last;
	char *token;

	token = tsrm_strtok_r(foo, "/\\", &last);
	while (token) {
		printf ("Token = '%s'\n", token);
		token = tsrm_strtok_r(NULL, "/\\", &last);
	}
	
	return 0;
}

#endif
