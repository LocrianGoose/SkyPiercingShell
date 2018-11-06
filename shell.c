// SPDX-License-Identifier: GPL-3.0-or-later

/*
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
#include <signal.h>
#include <stddef.h>


char *getWord(char *lastCh)
{
	int maxLen = 1;
	char *word = NULL;
	char buf;
	int i = 0;
	*lastCh = 0;

	while ((buf = getchar()) != ' ' && buf != '\n' &&
				buf != '>' && buf != '<' && buf != '|') {
		if (i + 1 >= maxLen) {
			maxLen <<= 1;
			word = realloc(word, maxLen * sizeof(char));
			if (word == NULL) {
				perror("realloc failed");
				exit(1);
			}
		}
		word[i++] = buf;
	}
	*lastCh = buf;
	if (i > 0)
		word[i] = 0;
	return word;
}

void freeList(char **command)
{
	for (int i = 0; command[i] != 0; i++)
		free(command[i]);
	free(command);
}

void freeSuperList(char ***command, int (*fd)[2])
{
	for (int i = 0; command[i] != 0; i++)
		freeList(command[i]);
	free(command);
	free(fd);
}

char **getList(int *fd, char *lastCh)
{
	int maxLen = 1, i = 0;
	char *fileName;
	char **list = NULL;

	fd[0] = 0;
	fd[1] = 1;
	while (*lastCh != '\n' && *lastCh != '|') {
		if (i + 1 >= maxLen) {
			maxLen = maxLen << 1;
			list = realloc(list, maxLen * sizeof(char *));
			if (list == NULL) {
				perror("realloc failed");
				exit(1);
			}
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
				puts("Syntax error");
				freeList(list);
				exit(1);
			}
			break;
		}
	}
	if (i > 0)
		list[i] = 0;
	return list;
}



char ***getSuperList(int (**fd)[2], int *length)
{
	char ***superList = NULL;
	char lastChar = ' ';
	int maxLen = 1;
	int i = 0;

	while (lastChar != '\n') {
		lastChar = ' ';
		if (i + 1 >= maxLen) {
			maxLen = maxLen << 1;
			superList = realloc(superList,
					maxLen * sizeof(char **));
			if (superList == NULL) {
				perror("realloc failed");
				exit(1);
			}
			*fd = realloc(*fd, maxLen * sizeof(int[2]));
			if (*fd == NULL) {
				perror("realloc failed");
				exit(1);
			}
		}
		superList[i] = getList((*fd)[i], &lastChar);
		if (superList[i] != NULL)
			i++;
	}
	if (i > 0)
		superList[i] = 0;
	*length = i;
	return superList;
}

char isExit(char *word)
{
	return !strncmp(word, "exit", 5) || !strncmp(word, "quit", 5);
}


int sendCmd(char **command, int *fd)
{
	pid_t pid = fork();

	if (pid < 0) {
		perror("fork failed");
		exit(1);
	} else if (pid == 0) {
		dup2(fd[0], 0);
		dup2(fd[1], 1);
		if (fd[1] != 1)
			close(fd[1]);
		if (fd[0] != 0)
			close(fd[0]);
		if (execvp(command[0], command) < 0) {
			perror("exec failed");
			puts(command[0]);
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
	pid_t pid = fork();

	if (pid < 0) {
		perror("fork failed");
		exit(1);
	} else if (pid == 0) {
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

int sendSuperCmd(char ***command, int (*fd)[2], int length)
{
	if (command[1] == 0) {
		sendCmd(command[0], fd[0]);
	} else {
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
	}
	return 0;
}

//Issue: Ctrl+C is being read by getWord()
void INT_handler(int sig)
{
	sigset_t sigset;

	sigemptyset(&sigset);
	if (sig == SIGINT) {
		puts("SIGINT...");
		sigaddset(&sigset, SIGINT);
		kill(-getpid(), SIGINT);
		sigwait(&sigset, &sig);
	}
}

void install_handler(void)
{
	struct sigaction setup_action;
	sigset_t block_mask;

	sigemptyset(&block_mask);
	sigaddset(&block_mask, SIGINT);
	sigaddset(&block_mask, SIGQUIT);
	setup_action.sa_handler = INT_handler;
	setup_action.sa_mask = block_mask;
	setup_action.sa_flags = 0;
	sigaction(SIGINT, &setup_action, NULL);
}

int main(void)
{
	int (*fd)[2], length;
	char ***command;

	install_handler();
	while (1) {
		fd = NULL;
		command = getSuperList(&fd, &length);
		if (command == NULL)
			continue;

		if (isExit(command[0][0]))
			break;
		sendSuperCmd(command, fd, length);
		freeSuperList(command, fd);
		while (wait(NULL) != -1)
			;
	}
	return 0;
}
