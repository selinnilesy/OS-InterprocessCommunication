all: world monster

world: world.o
	 g++ -o world world.o

monster: monster.o
	 g++   -o monster monster.o

world.o: world.cpp message.h logging.h logging.c
	 g++ -Wall  -c world.cpp message.h logging.h logging.c

monster.o: monster.cpp message.h logging.h logging.c
	 g++ -Wall  -c monster.cpp message.h logging.h logging.c

clean:
	 rm world.o
	 rm monster.o
	 rm monster
	 rm world
