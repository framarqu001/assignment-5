all: first

first: first.c
	gcc -Wall -Werror -g -fsanitize=address,undefined first.c -o first

clean:
	rm -rf first
