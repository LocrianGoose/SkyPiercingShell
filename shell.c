// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * check malloc
 *
 *
 */

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

char **getList(int *fd, char *lastCh)
{
	int maxLen = 4, i = 0;
	char *fileName;
	char **list = malloc(maxLen * sizeof(char *));

	fd[0] = 0;
	fd[1] = 1;
	while (*lastCh != '\n' && *lastCh != '|') {
		if (i + 1 >= maxLen) {
			maxLen = maxLen << 1;
			list = realloc(list, maxLen * sizeof(char *));
		}
		list[i] = getWord(lastCh);
		if (list[i] != NULL)
			i++;
		switch (*lastCh) {
		case '>':
			do {
				fileName = getWord(lastCh);
			} while (fileName == NULL);
			if (fd[1] != 1)
				close(fd[1]);
			fd[1] = open(fileName,
				O_RDWR | O_CREAT | O_TRUNC, 0666);
			free(fileName);
			break;
		case '<':
			do {
				fileName = getWord(lastCh);
			} while (fileName == NULL);
			if (fd[0] != 0)
				close(fd[0]);
			fd[0] = open(fileName, O_RDONLY);
			free(fileName);
			break;
		case '|':
			if (i == 0) {
				perror("Syntax error");
			}
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


void freeSuperList(char ***command)
{
	for (int i = 0; command[i] != 0; i++)
		freeList(command[i]);
	free(command);
}

char ***getSuperList(int (**fd)[2])
{
	char ***superList;
	char lastChar = ' ';
	int maxLen = 1;
	int i = 0;

	superList = malloc(sizeof(char **));
	*fd = malloc(sizeof(int[2]));
	while (lastChar != '\n') {
		lastChar = ' ';
		if (i + 1 >= maxLen) {
			maxLen = maxLen << 1;
			superList = realloc(superList, maxLen * sizeof(char **));
			*fd = realloc(*fd, maxLen * sizeof(int[2]));
		}
		superList[i] = getList((*fd)[i], &lastChar);
		if (superList[i] != NULL)
			i++;
	}
	if (i == 0) {
		freeSuperList(superList);
		return NULL;
	}
	superList[i] = 0;
	return superList;
}

int sendCmd(char **command, int *fd)
{
        if (fork() > 0) {
               // wait(NULL);
        } else {
                dup2(fd[0], 0);
                dup2(fd[1], 1);
        	if (fd[1] != 1)
                	close(fd[1]);
        	if (fd[0] != 0)
                	close(fd[0]);
                if (execvp(command[0], command) < 0) {
                        perror("exec failed");
                	exit(1);
		}
        }
        if (fd[1] != 1)
                close(fd[1]);
        if (fd[0] != 0)
                close(fd[0]);
        return 0;
}


int sendPipeCmd(char ***command, int (*fd)[2], int i)
{
	if (fork() > 0) {
	} else {
		if (i != 0) {
			dup2(fd[i - 1][0], 0);
			close(fd[i - 1][1]);
		} else {
			dup2(fd[0][0], 0);
			if (fd[0][0] != 0)
				close(fd[0][0]);
		}
		if (command[i + 1] != 0) {
			dup2(fd[i][1], 1);
			if (fd[i][1] != 1)
				close(fd[i][1]);
			if (i != 0)
				close(fd[i][0]);
		} else {
			dup2(fd[i][1], 1);
			if (fd[i][1] != 1)
				close(fd[i][1]);
		}
		if (execvp(command[i][0], command[i]) < 0) {
			perror("exec failed");
			exit(1);
		}
	}
	return 0;
}

int sendSuperCmd(char ***command, int (*fd)[2])
{
	int i, tmp[2];
	tmp[0] = fd[0][0];
	pipe(fd[0]);
	tmp[1] = fd[0][1];
	sendPipeCmd(command, &tmp, 0);
	close(fd[0][1]);
	for (i = 1; command[i + 1] != 0; i++) {
		pipe(fd[i]);
		sendPipeCmd(command, fd, i);
		close(fd[i - 1][0]);
		close(fd[i][1]);
	}
	sendPipeCmd(command, fd, i);
	close(fd[i - 1][0]);
	if (fd[i][1] != 1)
		close(fd[i][1]);
	return 0;
}


int main(void)
{
	int (*fd)[2];
	char ***command;

	while (1) {
		command = getSuperList(&fd);
		if (command == NULL)
			continue;

		if (isExit(command[0][0])) {
			freeSuperList(command);
			break;
		}
		if (command[1] == 0) {
			sendCmd(command[0], fd[0]);
		} else {
			sendSuperCmd(command, fd);
		}
		freeSuperList(command);
		while (wait(NULL) != -1);
	}
	return 0;
}
