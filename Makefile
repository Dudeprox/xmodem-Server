PORT=53514
CFLAGS = -DPORT=$(PORT) -g -Wall -Werror #-fsanitize=address

all: xmodemserver client1

xmodemserver: xmodemserver.o crc16.o helper.o 
	gcc ${CFLAGS} -o $@ $^

client1: client1.o crc16.o
	gcc ${CFLAGS} -o $@ $^

%.o: %.c crc16.h helper.h xmodemserver.h
	gcc ${CFLAGS} -c $<

clean:
	rm *.o xmodemserver client1