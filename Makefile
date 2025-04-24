all:
	gcc -o test test.c treealoc.c b_tree.c visual.c -lSDL2 -lpthread \
	   -Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=realloc