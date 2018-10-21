// SPDX-License-Identifier: NOLICENSE AHAAHAH
#include <stdio.h>
#include <stdlib.h>

char *getWord(char *lastCh)
{
	int maxLen = 16;
	char *word = malloc(maxLen * sizeof(char));
	char buf;
	int i = 0;

	while ((buf = getchar()) != ' ' && buf != '\n') {
		if (i >= maxLen)
			word = realloc(word, (maxLen <<= 1) * sizeof(char));
		word[i++] = buf;
	}
	*lastCh = buf;
	word[i] = 0;
	return word;
}

char **get_list(void)
{
	int maxLen = 4, i = 0;
	char lastCh = ' ';
	char **list = malloc(maxLen * sizeof(char *));

	while (lastCh != '\n') {
		if (i >= maxLen)
			list = realloc(list, (maxLen <<= 1) * sizeof(char *));
		list[i++] = getWord(&lastCh);
	}
	list[i] = 0;
	return list;
}

int main(void)
{
	return 0;
}
