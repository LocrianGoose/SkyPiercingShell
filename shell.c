// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Let's get dangerous!
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

#define PIPE_CODE -23
#define AMPERSAND -42


int superClose(int fd)
{
	if (fd != 1 && fd != 0)
		if (close(fd)) {
			perror("close failed");
			exit(1);
		}
	return 0;
}

void *superRealloc(void *ptr, int size)
{
	ptr = realloc(ptr, size);
	if (ptr == NULL) {
		perror("realloc failed");
		exit(1);
	}
	return ptr;
}

void superDup2(int oldfd, int newfd)
{
	if (dup2(oldfd, newfd) == -1) {
		perror("dup2 failed");
		exit(1);
	}
	superClose(oldfd);
}

char *getWord(char *lastChar)
{
	int maxLen = 1;
	char *word = NULL;
	char buf;
	int i = 0;

	while ((buf = getchar()) != ' ' && buf != '\n' &&
				buf != '>' && buf != '<' &&
				buf != '|' && buf != '&') {
		if (i + 1 >= maxLen) {
			maxLen <<= 1;
			word = superRealloc(word, maxLen * sizeof(char));
		}
		word[i++] = buf;
	}
	*lastChar = buf;
	if (i > 0)
		word[i] = 0;
	return word;
}

void freeList(char **command)
{
	for (int i = 0; command[i] != NULL; i++)
		free(command[i]);
	free(command);
}

void freeSuperList(char ***command, int (*fd)[3])
{
	for (int i = 0; command[i] != NULL; i++)
		freeList(command[i]);
	free(command);
	free(fd);
}

void handleLastChar(char *lastChar, int *fd, int i)
{
	char *fileName;

	switch (*lastChar) {
	case '>':
		if (i == 0) {
			puts("Syntax error");
			exit(1);
		}
		do {
			fileName = getWord(lastChar);
		} while (fileName == NULL);
		superClose(fd[1]);
		fd[1] = open(fileName,
			O_RDWR | O_CREAT | O_TRUNC, 0666);
		if (fd[1] == -1) {
			perror("open failed");
			exit(1);
		}
		free(fileName);
			break;
	case '<':
		if (i == 0) {
			puts("Syntax error");
			exit(1);
		}
		do {
			fileName = getWord(lastChar);
		} while (fileName == NULL);
		superClose(fd[0]);
		fd[0] = open(fileName, O_RDONLY);
		if (fd[0] == -1) {
			perror("open failed");
			exit(1);
		}
		free(fileName);
		break;
	case '|':
		if (i == 0) {
			puts("Syntax error");
			exit(1);
		}
		if (fd[1] == 1) {
			fd[1] = PIPE_CODE;
			fd[2] = PIPE_CODE;
		}
		break;
	}
}

char **getList(int *fd, char *lastChar)
{
	int maxLen = 1, i = 0;
	char **list = NULL;

	fd[0] = 0;
	fd[1] = 1;
	fd[2] = 0;
	while (*lastChar != '\n' && *lastChar != '|' && *lastChar != '&') {
		if (i + 1 >= maxLen) {
			maxLen = maxLen << 1;
			list = superRealloc(list, maxLen * sizeof(char *));
		}
		list[i] = getWord(lastChar);
		if (list[i] != NULL)
			i++;
		handleLastChar(lastChar, fd, i);
	}
	if (i == 0) {
		free(list);
		list = NULL;
	} else {
		list[i] = NULL;
	}
	return list;
}



char ***getSuperList(int (**fd)[3], int *length)
{
	char ***superList = NULL;
	char lastChar = ' ';
	int maxLen = 1;
	int i = 0;

	while (lastChar != '\n') {
		lastChar = ' ';
		if (i + 1 >= maxLen) {
			maxLen = maxLen << 1;
			superList = superRealloc(superList,
					maxLen * sizeof(char **));
			*fd = superRealloc(*fd, maxLen * sizeof(int[3]));
		}
		superList[i] = getList((*fd)[i], &lastChar);
		if (superList[i] != NULL)
			i++;
		if (i != 0 && lastChar == '&') {
			if ((*fd)[i - 1][2] == AMPERSAND)
				(*fd)[i - 1][2] = 0;
			else
				(*fd)[i - 1][2] = AMPERSAND;
		}
	}
	if (i == 0) {
		free(superList);
		superList = NULL;
	} else {
		superList[i] = NULL;
	}
	*length = i;
	return superList;
}

char isExit(char *word)
{
	return !strncmp(word, "exit", 5) || !strncmp(word, "quit", 5);
}


pid_t sendCmd(char **command, int *fd, int closeNext)
{
	pid_t pid = fork();

	if (pid < 0) {
		perror("fork failed");
		exit(1);
	} else if (pid == 0) {
		superClose(closeNext);
		superDup2(fd[0], 0);
		superDup2(fd[1], 1);
		if (execvp(command[0], command) < 0) {
			perror("exec failed");
			puts(command[0]);
			exit(1);
		}
	}
	superClose(fd[1]);
	superClose(fd[0]);
	return pid;
}



int superSendCmd(char ***command, int (*fd)[3], int length)
{
	int superfd[2], pipefd[2], closeNext, i, wstatus;
	pid_t chldpid;

	pipefd[0] = fd[0][0];
	closeNext = 0;
	for (i = 0; i < length; i++) {
		superfd[0] = pipefd[0];
		if (fd[i][1] == PIPE_CODE && fd[i + 1][0] == 0) {
			if (pipe(pipefd)) {
				perror("pipe failed");
				exit(1);
			}
			closeNext = pipefd[0];
			superfd[1] = pipefd[1];
		} else {
			superfd[1] = fd[i][1];
			closeNext = 0;
		}
		if (fd[i][0] != 0)
			superfd[0] = fd[i][0];
		chldpid = sendCmd(command[i], superfd, closeNext);
		if (!fd[i][2]) {
			waitpid(chldpid, &wstatus, 0);
			if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus))
				break;
		}
	}
	return 0;
}


/* needs testing */
void INT_handler(int sig)
{
	sigset_t sigset;
	const struct timespec timeout = {1, 1};

	sigemptyset(&sigset);
	if (sig == SIGINT) {
		//printf("pid %d ", getpid());
		puts(" SIGINT...");
		sigaddset(&sigset, SIGINT);
		kill(-getpid(), SIGINT);
		sigtimedwait(&sigset, NULL, &timeout);
	} else {
		printf("GAH! Signal %d!!!\n", sig);
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
	setup_action.sa_flags = SA_RESTART;
	sigaction(SIGINT, &setup_action, NULL);
}

int main(void)
{
	int (*fd)[3], length = 0;
	char ***command;

	install_handler();
	while (1) {
		fd = NULL;
		printf("shell%d: ", getpid());
		command = getSuperList(&fd, &length);
		if (command == NULL)
			continue;
		if (isExit(command[0][0]))
			break;
		superSendCmd(command, fd, length);
		freeSuperList(command, fd);
	}
	return 0;
}
