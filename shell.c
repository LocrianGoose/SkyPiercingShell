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

typedef struct {
	char **sentence;
	char *input;
	char *output;
	int flag;
} Command;

int superClose(int fd)
{
	if (fd != 1 && fd != 0 && fd != -1 && fd != 2)
		if (close(fd)) {
			perror("close failed");
			return -1; //?
		}
	return 0;
}

void *superRealloc(void *ptr, int size)
{
	ptr = realloc(ptr, size);
	if (ptr == NULL) {
		perror("realloc failed");
		exit(1); //?
	}
	return ptr;
}

int superOpen(char *fileName, int flag)
{
	int fd = -1;

	if (fileName == NULL) {
		return flag;
	} else if (flag == 0) {
		fd = open(fileName, O_RDONLY);
	} else if (flag == 1) {
		fd = open(fileName,
			O_RDWR | O_CREAT | O_TRUNC, 0666);
	}
	if (fd == -1) {
		perror("open failed");
		return -1;
	}
	return fd;
}

void superDup2(int oldfd, int newfd)
{
	if (dup2(oldfd, newfd) == -1) {
		perror("dup2 failed");
		exit(1); //?
	}
	superClose(oldfd);
}

char *getWord(char *lastChar)
{
	int maxLen = 1;
	char *word = NULL;
	char buf, bufold = 0, bufoldold = 0;
	int i = 0;

	while (((buf = getchar()) != ' ' && buf != '\n' &&
				buf != '>' && buf != '<' &&
				buf != '|' && buf != '&')
				|| bufold == '\\' || bufold == '\"') {
		if (i + 2 >= maxLen) {
			maxLen <<= 1;
			word = superRealloc(word, maxLen * sizeof(char));
		}
		if (buf == '\n')
			printf("> ");
		if ((buf == '\"' && !bufold) ||
				(buf == '\\' && bufold != '\\')) {
			bufoldold = bufold;
			bufold = buf;
		} else {
			if (bufold == '\"' && buf == '\"') {
				bufold = 0;
				bufoldold = 0;
				continue;
			} else if (bufold == '\\' && buf == '\"') {
				bufold = bufoldold;
				word[i++] = '\"';
			} else if (bufoldold == '\"' && bufold == '\\') {
				word[i++] = '\\';
				word[i++] = buf;
				bufold = '\"';
			} else {
				word[i++] = buf;
			}
		}
	}
	*lastChar = buf;
	if (i > 0)
		word[i] = 0;
	return word;
}

void freeSentence(char **sentence)
{
	for (int i = 0; sentence[i] != NULL; i++)
		free(sentence[i]);
	free(sentence);
}

void freeCommand(Command *command)
{
	freeSentence(command->sentence);
	free(command->input);
	free(command->output);
	free(command);
}

void freeSuperCommand(Command **superCommand)
{
	for (int i = 0; superCommand[i] != NULL; i++)
		freeCommand(superCommand[i]);
	free(superCommand);
}

int handleLastChar(char *lastChar, Command *command, int i)
{
	switch (*lastChar) {
	case '<':
		free(command->input);
		do {
			if (*lastChar == '\n')
				printf("> ");
			command->input = getWord(lastChar);
		} while (command->input == NULL);
		break;
	case '>':
		free(command->output);
		do {
			if (*lastChar == '\n')
				printf("> ");
			command->output = getWord(lastChar);
		} while (command->output == NULL);
		break;
	case '|':
		if (i == 0) {
			puts("Syntax error");
			return 1;
		}
		if (command->output == NULL)
			command->flag = PIPE_CODE;
		break;
	}
	return 0;
}

Command *getCommand(char *lastChar)
{
	int maxLen = 1, i = 0;
	Command *command = malloc(sizeof(Command));

	command->sentence = NULL;
	command->input = NULL;
	command->output = NULL;
	command->flag = 0;

	while (*lastChar != '\n' && *lastChar != '|' && *lastChar != '&') {
		if (i + 1 >= maxLen) {
			maxLen = maxLen << 1;
			command->sentence = superRealloc(command->sentence,
					maxLen * sizeof(char *));
		}
		command->sentence[i] = getWord(lastChar);
		if (command->sentence[i] != NULL)
			i++;
		if (handleLastChar(lastChar, command, i)) {
			freeCommand(command);
			return (Command *) -1;
		}
	}
	if (i == 0) {
		freeCommand(command);
		command = NULL;
	} else {
		command->sentence[i] = NULL;
	}
	return command;
}



Command **getSuperCommand(int *length)
{
	Command **superCommand = NULL;
	char lastChar = ' ';
	int maxLen = 1, i = 0, specialCase = 0;

	*length = 0;
	while (lastChar != '\n' || specialCase) {
		if (lastChar == '\n')
			printf("> ");
		lastChar = ' ';
		if (i + 1 >= maxLen) {
			maxLen = maxLen << 1;
			superCommand = superRealloc(superCommand,
					maxLen * sizeof(Command *));
		}
		superCommand[i] = getCommand(&lastChar);
		if (superCommand[i] == (Command *) -1) {
			*length = -1;
		} else if (superCommand[i] != NULL) {
			i++;
			specialCase = 0;
		}
		if (i != 0 && lastChar == '&') {
			if (superCommand[i - 1]->flag == AMPERSAND) {
				superCommand[i - 1]->flag = 0;
				specialCase = 1;
			} else {
				superCommand[i - 1]->flag = AMPERSAND;
			}
		}
		if (lastChar == '|')
			specialCase = 1;
	}
	if (i > 0)
		superCommand[i] = NULL;
	if (*length != -1)
		*length = i;
	return superCommand;
}

char isExit(char *word)
{
	return !strncmp(word, "exit", 5) || !strncmp(word, "quit", 5);
}

int customCommand(char **command)
{
	char *home;

	if (!strncmp(command[0], "cd", 3)) {
		if (command[1] == NULL ||
		(command[1][0] == '~' && strlen(command[1]) == 1)) {
			home = getenv("HOME");
			if (home == NULL || chdir(home) < 0) {
				perror("cd failed");
				return -1;
			}
		} else if (chdir(command[1]) < 0) {
			perror("cd failed");
			return -1;
		}
	} else if (isExit(command[0])) {
		exit(0);
	} else {
		return 0;
	}
	return 1;
}

pid_t sendCommand(char **command, int *fd, int closeNext, int flag)
{
	pid_t pid = 1;
	int custom = customCommand(command);

	if (!custom) {
		pid = fork();

		if (pid < 0) {
			perror("fork failed");
			pid = -1;
		} else if (pid == 0) {
			superClose(closeNext);
			superDup2(fd[0], 0);
			superDup2(fd[1], 1);
			if (execvp(command[0], command) < 0) {
				perror("exec failed");
				exit(1);
			}
		} else if (flag == AMPERSAND) {
			if (setpgid(pid, pid) < 0) {
				perror("setpgid failed");
				kill(pid, SIGTERM);
				return -1;
			}
			printf("%s with pid %d is working in a background\n",
					command[0], pid);
		}
	} else if (custom < 1) {
		pid = -1;
	}
	superClose(fd[1]);
	superClose(fd[0]);
	return pid;
}



int sendSuperCommand(Command **superCommand, int length)
{
	int superfd[2], pipefd[2], closeNext, i, wstatus;
	pid_t chldpid;

	closeNext = 0;
	pipefd[0] = 0;
	for (i = 0; i < length; i++) {
		superfd[0] = pipefd[0];
		if (superCommand[i]->flag == PIPE_CODE &&
				superCommand[i + 1]->input == NULL) {
			if (pipe(pipefd)) {
				perror("pipe failed");
				superClose(superfd[0]);
				break;
			}
			closeNext = pipefd[0];
			superfd[1] = pipefd[1];
		} else {
			superfd[1] = superOpen(superCommand[i]->output, 1);
			pipefd[0] = 0;
			closeNext = 0;
		}
		if (superCommand[i]->input != NULL)
			superfd[0] = superOpen(superCommand[i]->input, 0);
		if (superfd[0] == -1 || superfd[1] == -1) {
			superClose(superfd[0]);
			superClose(superfd[1]);
			superClose(closeNext);
			break;
		}
		chldpid = sendCommand(superCommand[i]->sentence, superfd,
				closeNext, superCommand[i]->flag);
		if (chldpid < 0)
			break;
		if (!superCommand[i]->flag && chldpid != 1) {
			waitpid(chldpid, &wstatus, WUNTRACED);
			if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus)) {
				break;
			} else if (WIFSTOPPED(wstatus)) {
				printf("Process with pid %d stopped\n",
						chldpid);
				break;
			}
		}
	}
	return 0;
}

void printBar(void)
{
	char host[128];
	char *user = getenv("USER");

	if (gethostname(host, 127) < 0 || user == NULL) {
		printf("anon> ");
		return;
	}
	printf("%s@%s ", user, host);
	if (!strcmp(user, "root"))
		putchar('#');
	else
		putchar('$');
	putchar(' ');
}

void handler(int sig)
{
	sigset_t sigset;
	const struct timespec timeout = {1, 1};

	sigemptyset(&sigset);
	if (sig == SIGINT) {
		puts(" SIGINT...");
		sigaddset(&sigset, SIGINT);
		kill(-getpid(), SIGINT);
		sigtimedwait(&sigset, NULL, &timeout);
	} else if (sig == SIGTSTP) {
		puts(" SIGTSTP...");
		sigaddset(&sigset, SIGTSTP);
		kill(-getpid(), SIGTSTP);
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
	sigaddset(&block_mask, SIGTSTP);
	setup_action.sa_handler = handler;
	setup_action.sa_mask = block_mask;
	setup_action.sa_flags = SA_RESTART;
	sigaction(SIGINT, &setup_action, NULL);
	setup_action.sa_handler = handler;
	sigaction(SIGTSTP, &setup_action, NULL);
}


int main(void)
{
	int length;
	Command **superCommand;

	install_handler();
	while (1) {
		printBar();
		superCommand = getSuperCommand(&length);
		if (length > 0)
			sendSuperCommand(superCommand, length);
		freeSuperCommand(superCommand);
		while (waitpid(-1, NULL, WNOHANG) > 0)
			;
	}
	return 0;
}
