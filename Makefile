#
#	Makefile
#	Calculus Invaders
#	Scritto da Riccardo Angius
#	Ultima modifica: 14 gennaio 2012
#
calculus_invaders: main.o invdrslib.o
	gcc -o calculus_invaders main.o invdrslib.o -lncurses -lpthread -lm
main.o: main.c invdrslib.h
	gcc -c -o main.o main.c
invdrslib.o: invdrslib.c invdrslib.h
	gcc -c -o invdrslib.o invdrslib.c
clean:
	rm calculus_invaders
	rm *.o
