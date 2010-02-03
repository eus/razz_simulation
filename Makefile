.PHONY: clean doc test

CFLAGS := -g3 -Werror

razz: razz.o card.o

card_test: card_test.o card.o

razz.o:

card.o: card.h

card_test.o: card.o

test: card_test
	valgrind --leak-check=full ./card_test

doc:
	doxygen

clean:
	rm -f *.o
