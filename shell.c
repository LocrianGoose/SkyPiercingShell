// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

char *getWord(char *lastCh)
{
	int maxLen = 16;
	char *word = malloc(maxLen * sizeof(char));
	char buf;
	int i = 0;

	while ((buf = getchar()) != ' ' && buf != '\n') {
		if (i >= maxLen) {
			maxLen <<= 1;
			word = realloc(word, maxLen * sizeof(char));
		}
		word[i++] = buf;
	}
	if (i == 0) {
		*lastCh = 0;
		free(word);
		return NULL;
	}
	word[i] = 0;
	*lastCh = buf;
	return word;
}

void freeList(char **command)
{
	for (int i = 0; command[i] != 0; i++) {
		free(command[i]);
	}
	free(command);
}

char **getList(void)
{
	int maxLen = 2, i = 0;
	char lastCh = ' ';
	char **list = malloc(maxLen * sizeof(char *));
	while (lastCh != '\n') {
		if (i >= maxLen) {
			maxLen = maxLen << 1;
			list = realloc(list, maxLen * sizeof(char *));
		}
		list[i++] = getWord(&lastCh);
		if (lastCh == 0)
			i--;
	}
	if (i == 0) {
		freeList(list);
		return NULL;
	}
	list[i] = 0;
	return list;
}

char isExit(char *word)
{
	return !strncmp(word, "exit", 5) || !strncmp(word, "quit", 5);
}


int main(void)
{
	char **command;

	while (1) {
		command = getList();
		if (command == NULL) {
			continue;
		}
		if (isExit(command[0])) {
			freeList(command);
			break;
		}
		if (fork() > 0) {
			wait(NULL);
		} else {
			if (execvp(command[0], command) < 0) {
				perror("exec failed");
				return 1;
			}
		}
		freeList(command);
	}
	return 0;
}
