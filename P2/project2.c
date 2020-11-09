// project2.c
// Author: Ata Fatahi Baarzi @mailto azf82@psu.edu
//Description: Parser and Handler for Project 2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <stdint.h>

extern void setup( int malloc_type, int mem_size, void* start_of_memory );
extern void *my_malloc( int size );
extern void my_free( void *ptr );

FILE *fd;
struct handle{
	char  _handle;
	int ** addresses;
	struct handle * next;
	int num_allocs;
};
typedef struct handle handle_t;
struct ops{
	int numops ;
	char handle;
	char type;
	int size ;
};
typedef struct ops ops_t;

int open_file(char *filename)
{
	char mode = 'r';
	fd = fopen(filename, &mode);
	if (fd == NULL)
	{
		printf("Invalid input file specified: %s\n", filename);
		return -1;
	}
	else
	{
		return 0;
	}
}

void close_file()
{
	fclose(fd);
}

int  read_next_ops(ops_t * op){
	char line[1024];
	if(!fgets(line, 1024, fd)) {
		return 0;
	}

	char *token;
	char* rest=line;
	if(token=strtok_r(rest, " ", &rest)) {
    op->handle = *token;
	} else {
		return 0;
	}
	if(token=strtok_r(rest, " ", &rest)) {
		op->numops = atof(token);
	} else {
		return 0;
	}
	if(token=strtok_r(rest, " ", &rest)) {
    op->type = *token;
	} else {
		return 0;
	}
	if(token=strtok_r(rest, " ", &rest)) {
		op->size = atof(token);
	} else {
		return 0;
	}
	return 1;
}
int main(int argc, char *argv[]){


	if (argc < 3)
	{
		printf ("Not enough parameters specified.  Usage: a.out <allocation_type> <input_file>\n");
		printf ("  Allocation type: 0 - Buddy System\n");
		printf ("  Allocation type: 1 - Slab Allocation\n");
		return -1;
	}

	if (open_file(argv[2]) < 0){
		return -1;
	}


	int size;
	int RAM_SIZE=1<<20;//1024*1024
	void* RAM=malloc(RAM_SIZE);//1024*1024
	setup(atoi(argv[1]),RAM_SIZE,RAM);


	handle_t * handles = NULL;
	ops_t * op = (ops_t*) malloc(sizeof(ops_t));
	while(read_next_ops(op)){
		if(op->type == 'M'){//do my_alloc
			if (handles == NULL){
				handles = (handle_t *) malloc(sizeof(handle_t));
				handles->_handle = op->handle;
				handles->num_allocs = 0;
				handles->addresses = (int**)malloc(sizeof(int*)*(op->numops+1));
				//printf("handles %c",handles->_handle);
				handles->next = NULL;
				for (int i=1; i<= op->numops; i++){
					void * x = my_malloc(op->size);
					if(!(intptr_t)x || (intptr_t)x == -1){
						if( handles != NULL && handles->num_allocs == 0)
						{         free(handles);
							handles = NULL;
						}
						printf("Allocation Error %c\n",op->handle );
						break;
					}
					else
					{

						*(handles->addresses+i) = (int*)x;
						handles->num_allocs+=1;
						printf("Start of first Chunk %c is: %d\n",op->handle, (int)((void*)(*(handles->addresses+i)) - RAM));
					}
				}
			}
			else{
				handle_t * hp = handles;
				// printf("handles %c",hp->_handle);
				while (hp->next != NULL){
					//printf("handles %c",hp->_handle);
					hp = hp->next;
				}
				hp->next = (handle_t *) malloc(sizeof(handle_t));
				hp->next->_handle = op->handle;
				hp->next->num_allocs = 0;
				hp->next->addresses =  (int**)malloc(sizeof(int*)*(op->numops+1));
				hp->next->next= NULL;
				for (int i=1; i<= op->numops; i++){

					void * x = my_malloc(op->size);
					if(!(intptr_t)x || (intptr_t)x == -1){
						if(hp->next != NULL && hp->next->num_allocs == 0)
						{
							free(hp->next);
							hp->next = NULL;
						}
						printf("Allocation Error %c\n",op->handle);
						break;
					}
					else
					{
						*(hp->next->addresses+i) = (int*)x;
						hp->next->num_allocs +=1;
						printf("Start of Chunk %c is: %d\n",op->handle, (int)((void*)(*(hp->next->addresses+i)) - RAM));
					}
				}
			}
		}

		else if(op->type == 'F'){//do free
			handle_t * hp1 = NULL;
			hp1= handles;
			while(hp1 !=NULL){
				if (hp1->_handle == op->handle){
					int index = op->numops;
					hp1->num_allocs-=1;
					my_free((void *)(*(hp1->addresses+index)));
					printf("freed object %c at %d\n",  op->handle,(int)((void*)(*(hp1->addresses+index)) - RAM));
					break;
				}
				hp1 = hp1->next;
			}

		}
		else{
			printf("Incorrect input file content\n");
			return 0;
		}

	}


	return 0;
}
