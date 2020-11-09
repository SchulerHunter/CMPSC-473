CFLAGS = -std=gnu99
SOURCES = project2.c my_memory.c
OUT = out
default:
	gcc $(CFLAGS) $(SOURCES) $(LIBS) -o $(OUT)
debug:
	gcc -g $(CFLAGS) $(SOURCES)  -o $(OUT)
all:
	gcc $(SOURCES) $(LIBS) -o $(OUT)
clean:
	rm $(OUT)
