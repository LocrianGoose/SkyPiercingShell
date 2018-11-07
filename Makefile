all:
	gcc shell.c -o shell -Wall -Werror -lm
	./checkpatch.pl --no-tree -f shell.c
