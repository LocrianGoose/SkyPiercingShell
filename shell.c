// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

typedef struct {
	char instruction;
	char *file;
} SpecialCase;

char *getWord(char *lastCh)
{
	int maxLen = 16;
	char *word = malloc(maxLen * sizeof(char));
	char buf;
	int i = 0;
	*lastCh = 0;

	while ((buf = getchar()) != ' ' && buf != '\n' &&
				buf != '>' && buf != '<' && buf != '|') {
		if (i + 1 >= maxLen) {
			maxLen <<= 1;
			word = realloc(word, maxLen * sizeof(char));
		}
		word[i++] = buf;
	}
	*lastCh = buf;
	if (i == 0) {
		free(word);
		return NULL;
	}
	word[i] = 0;
	return word;
}

void freeList(char **command)
{
	for (int i = 0; command[i] != 0; i++) {
		free(command[i]);
	}
	free(command);
}

char **getList(SpecialCase *special)
{
	int maxLen = 4, i = 0;
	char lastCh = ' ';
	char **list = malloc(maxLen * sizeof(char *));

	while (lastCh != '\n') {
		if (i + 1 >= maxLen) {
			maxLen = maxLen << 1;
			list = realloc(list, maxLen * sizeof(char *));
		}
		list[i] = getWord(&lastCh);
		if (list[i] != NULL)
			i++;
		switch (lastCh) {
		case '>':
			special->instruction = '>';
			do {
				special->file = getWord(&lastCh);
			} while (special->file == NULL);
			break;
		case '<':
			special->instruction = '<';
			do {
				special->file = getWord(&lastCh);
			} while (special->file == NULL);
			break;
		case '|':
			special->instruction = '|'
			break;
		}
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

int sendCmd(char **command, SpecialCase *special)
{
	int fd[2];

	switch (special->instruction) {
	case 0:
		if (fork() > 0) {
			wait(NULL);
		} else {
			if (execvp(command[0], command) < 0) {
				perror("exec failed");
				return 1;
			}
		}
		break;
	case '>':
		fd[1] = open(special->file,
				O_WRONLY | O_CREAT | O_TRUNC,
				S_IRUSR | S_IWUSR);
		free(special->file);
		if (fork() > 0) {
			wait(NULL);
			dup2(1, fd[1]);
		} else {
			dup2(fd[1], 1);
			if (execvp(command[0], command) < 0) {
				perror("exec failed");
				return 1;
			}
		}
		close(fd[1]);
		break;
	case '<':
		fd[0] = open(special->file, O_RDONLY, S_IRUSR);
		free(special->file);
		if (fork() > 0) {
			wait(NULL);
			dup2(0, fd[0]);
		} else {
			dup2(fd[0], 0);
			if (execvp(command[0], command) < 0) {
				perror("exec failed");
				return 1;
			}
		}
		close(fd[0]);
		break;
	case '|':
		pipe(fd);
		if (fork() > 0) {
			wait(NULL);
		} else {
			close(fd[0]);
			dup2(fd[1], 1);
			if (execvp(command[0], command) < 0) {
				perror("exec failed");
				return 1;
			}
		}
		close(fd[1]);
		dup(fd[0], 0);
		break;

	}
	return 0;
}


int main(void)
{
	char **command;
	SpecialCase *special = malloc(sizeof(SpecialCase));

	while (1) {
		special->instruction = 0;
		command = getList(special);
		if (command == NULL) {
			continue;
		}
		if (isExit(command[0])) {
			freeList(command);
			break;
		}
		sendCmd(command, special);
		freeList(command);
	}
	free(special);
	return 0;
}
