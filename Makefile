CC = gcc
INCLUDES = -I/usr/include/libdrm
LIBRARIES = -lpciaccess
CFLAGS = -Wall -Wextra $(LIBRARIES) $(INCLUDES)
PROGRAM_NAME = intel_gpu_top

all:
	$(CC) $(CFLAGS) -c lib/*c
	$(CC) $(CFLAGS) *.o $(PROGRAM_NAME).c -o $(PROGRAM_NAME)

clean:
	rm -f *.o $(PROGRAM_NAME)
