CC=gcc
CFLAGS=`pkg-config --cflags gtk4` -Iinclude -Wall -Wextra
LIBS=`pkg-config --libs gtk4`

SRC=src/main.c src/ui.c src/process.c src/scheduler.c src/memory.c src/ipc.c src/deadlock.c src/config.c src/logger.c src/io_system.c src/multicore.c

OUT=serc_os_simulator

all:
	$(CC) $(SRC) -o $(OUT) $(CFLAGS) $(LIBS)

clean:
	rm -f $(OUT)

run: all
	./$(OUT)