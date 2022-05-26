CC = gcc

SFIND = ssu.finder

all : $(SFIND)

$(SFIND) : ssu_sfinder.c
	$(CC) -o $@ $^ -lcrypto

clean :
	rm -rf *.o
	rm -rf $(SFIND)
