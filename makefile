
SCHED = scheduler
ASSESTS = assets

CC = gcc -std=c99 
CFLAGS  = -g -Wall -Wpedantic -Wextra -pthread

TARGET = fat32

all: $(TARGET)
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c 

clean:
	rm -f $(TARGET)
