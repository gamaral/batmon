#CFLAGS := -g -W -Wall
CFLAGS := -O3

all: batmon

batmon: main.c config.c config.h
	$(CC) $(CFLAGS) -o batmon main.c config.c

clean:
	rm -f *.o batmon

install: batmon
	install -s batmon /usr/local/sbin

config_install:
	install -D batmon.conf /etc/batmon.conf
