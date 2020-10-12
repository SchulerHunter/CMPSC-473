CFLAGS = -std=gnu99 
LIBS = -lpthread -lrt -lm
SOURCES = project1.c scheduler.c 
OUT = out
default:
	gcc $(CFLAGS) $(SOURCES) $(LIBS) -o $(OUT)
debug:
	gcc -g $(CFLAGS) $(SOURCES) $(LIBS) -o $(OUT)
all:
	gcc $(SOURCES) $(LIBS) -o $(OUT)
clean:
	rm $(OUT)
