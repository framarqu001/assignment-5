all: second

second: second.c
	gcc -Wall -Werror -g -fsanitize=address,undefined second.c -o second

clean:
	rm -rf second
