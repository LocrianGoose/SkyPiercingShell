// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

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
	for (int i = 0; command[i] != 0; i++)
		free(command[i]);
	free(command);
}

char **getList(int *fd)
{
	int maxLen = 4, i = 0;
	char lastCh = ' ';
	char *fileName;
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
			do {
				fileName = getWord(&lastCh);
			} while (fileName == NULL);
			fd[1] = open(fileName,
				O_WRONLY | O_CREAT | O_TRUNC,
				S_IRUSR | S_IWUSR);
			free(fileName);
			break;
		case '<':
			do {
				fileName = getWord(&lastCh);
			} while (fileName == NULL);
			fd[0] = open(fileName, O_RDONLY, S_IRUSR);
			free(fileName);
			break;
		case '|':
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

int sendCmd(char **command, int *fd)
{
	if (fork() > 0) {
		wait(NULL);
		dup2(0, fd[0]);
		dup2(1, fd[1]);
	} else {
		dup2(fd[0], 0);
		dup2(fd[1], 1);
		if (execvp(command[0], command) < 0) {
			perror("exec failed");
			return 1;
		}
	}
	if (fd[1] != 1)
		close(fd[1]);
	if (fd[0] != 0)
		close(fd[0]);
	return 0;
}


int main(void)
{
	int fd[2];
	char **command;

	while (1) {
		fd[0] = 0;
		fd[1] = 1;

		command = getList(fd);

		if (command == NULL)
			continue;

		if (isExit(command[0])) {
			freeList(command);
			break;
		}
		sendCmd(command, fd);
		freeList(command);
	}
	return 0;
}
