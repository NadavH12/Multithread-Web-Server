#include "request.h"
#include "io_helper.h"
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

void put(int); // Put a value in the array
int get(); // Get a value from the array
void* consumer(void* arg); // The consumer thread
void producer(int conn_fd); //producer method
void Pthread_mutex_unlock(pthread_mutex_t* m);
void Pthread_mutex_lock(pthread_mutex_t* m);

char default_root[] = ".";

int t = 0;
int b = 0;

int* buffer_ptr;
int fill_ptr = 0;
int use_ptr = 0;
int count = 0; // Keeps track of how many items are in the array, When we add an item in put() we increment it by 1, When we remove an item in get() we decrement it by 1

pthread_t* threads_ptr;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; //Lock
pthread_cond_t empty = PTHREAD_COND_INITIALIZER; //Condition variable
pthread_cond_t fill = PTHREAD_COND_INITIALIZER; //Condition variable


int main(int argc, char *argv[]) {
    int c;
    char *root_dir = default_root;
    int port = 10000;
	
    
    while ((c = getopt(argc, argv, "d:p:t:b:")) != -1)
	switch (c) {
	case 'd':
	    root_dir = optarg;
	    break;
	case 'p':
	    port = atoi(optarg);
	    break;
	case 't':
	    t = atoi(optarg);
	    break;
	case 'b':
	    b = atoi(optarg);
	    break;
	default:
	    fprintf(stderr, "usage: wserver [-d basedir] [-p port] [-t threads] [-b buffers]\n");
	    exit(1);
	}

	if (port < 1024 || port > 65535){
		fprintf(stderr, "port number is invalid\n");
	    exit(1);
	}

	if (t < 1){
		fprintf(stderr, "threads parameter must be an integer larger than 0\n");
	    exit(1);
	}

	if (b < 1){
		fprintf(stderr, "buffer parameter must be an integer larger than 0\n");
	    exit(1);
	}

	//initialize buffer
	buffer_ptr = malloc(b * sizeof(int));
	threads_ptr = malloc(t * sizeof(pthread_t*));


	//create a fixed-size pool of worker threads
	for(int i = 0; i < t; i++) {
		pthread_create(&threads_ptr[i], NULL, consumer, NULL);
	}	
	
	// run out of this directory
    chdir_or_die(root_dir);

    // now, get to work
    int listen_fd = open_listen_fd_or_die(port);
    while (1) {

		struct sockaddr_in client_addr;
		int client_len = sizeof(client_addr);
		int conn_fd = accept_or_die(listen_fd, (sockaddr_t *) &client_addr, (socklen_t *) &client_len);
		
		producer(conn_fd);
			
	}
    return 0;
}


    
// Add an item to the shared resource (the array buffer)
void put(int conn_fd) {
    buffer_ptr[fill_ptr] = conn_fd;
    fill_ptr = (fill_ptr + 1) % b;
    count++;
    printf("Number of items in buffer: %d\n", count);
}

// Remove an item from the shared resource (the array buffer) 
int get() {
    int tmp = buffer_ptr[use_ptr];
    use_ptr = (use_ptr + 1) % b;
    count--;

    printf("Number of items in buffer: %d\n", count); 
    return tmp;
}

// The producer thread: adds items to the buffer 
void producer(int conn_fd) {

	Pthread_mutex_lock(&mutex);
	while(count == b) {
		printf("==>> buffer is full\n");
		pthread_cond_wait(&empty, &mutex);
	}

	put(conn_fd);
	pthread_cond_signal(&fill); 
	Pthread_mutex_unlock(&mutex);
}

 
// The consumer thread: removes items from the buffer and process them
void* consumer(void* arg) { 
	begin:
		Pthread_mutex_lock(&mutex);
		while(count == 0) {
			printf("==>> buffer is empty. Wainting for data to arrive\n"); 
			pthread_cond_wait(&fill, &mutex);
		}

		int conn_fd = get();
		pthread_cond_signal(&empty); 
		Pthread_mutex_unlock(&mutex);

		request_handle(conn_fd);
		close_or_die(conn_fd);

		printf("Processing item %d\n", conn_fd);
		goto begin;

	return NULL;
}


// Wrapper around pthread_mutex_lock that checks if it fails 
void Pthread_mutex_lock(pthread_mutex_t* m) {
    int rc = pthread_mutex_lock(m); 
    if(rc != 0) {
        fprintf(stderr, "pthread_mutex_lock failed with return value %d\n", rc); 
        exit(EXIT_FAILURE);
    }
}

// Wrapper around pthread_mutex_unlock that checks if it fails 
void Pthread_mutex_unlock(pthread_mutex_t* m) {
    int rc = pthread_mutex_unlock(m); 
    if(rc != 0) {
        fprintf(stderr, "pthread_mutex_unlock failed with return value %d\n", rc); 
        exit(EXIT_FAILURE);
    }
}