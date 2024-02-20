#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

// calculates prefix sum for num_elements in segment for new array, given array for previous iteration
// new array is b, old array is a
void HillisSteele(int *new_array,int *old_array, int start_index, int end_index, int step) {
	for (int i = start_index; i < end_index; i++) {
		if (i - step >= 0) {
			new_array[i] = new_array[i] + old_array[i - step];
		}
	}
	return;
}

// reads input file and puts into array "a"
int* ReadFromFile(int *a, char *filename, int num_elements) {
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		perror("Error reading file");
		return NULL;
	}
	for (int i = 0; i < num_elements; i++) {
		fscanf(fp, "%d", &a[i]);
	}

	fclose(fp);
	return a;
}

// output array b to given file
int OutputToFile(int *b, int num_elements,  char *filename) {
	FILE *fp = fopen(filename, "w+");
	if (fp == NULL) {
		perror("Error writing into output file.");
		return 1;
	}
	for (int i = 0; i < num_elements; i++) {
		fprintf(fp,"%d\n", b[i]);
	}
	fclose(fp);
	return 0;
}

// main program
int main(int argc, char *argv[]) {
	int num_args = 5;
	if (argc < num_args) {
		perror("Usage: ./test <num_elements> <num_cores> <input_filename> <output_filename.txt>");
		return 1;
	}
	int segment_id;
	int num_elements = atoi(argv[1]);
	int num_cores = atoi(argv[2]);
	
	// input checking, makes sure num_elements and num_cores are valid
	if (num_elements <= 0 || num_cores < 1) {
		perror("Elements and cores must be more than 1.\n");
	}

	char *filename = argv[3]; // input file
	char *output_file = argv[4];

	const int size = num_elements * sizeof(int); // size of shared mem seg for one array, doubles for a and b
	int segment_size = num_elements / num_cores; // size of segment in each core, truncated
	int remainder = 0;                           // remainder from truncation, updates later
	if (num_elements % num_cores != 0) {        
		remainder = num_elements % num_cores;
	}

	// create shared mem seg
	segment_id = shmget(IPC_PRIVATE, size * 2, S_IRUSR | S_IWUSR);
	int* shared_memory = (int*) shmat(segment_id, NULL, 0);
	if (segment_id == -1) {
		perror("shmget");
		exit(1);
	}
	if (shared_memory == (void*) -1) {
		perror("shmat");
		exit(1);
	}

	// attach mem seg to arrays a and b
	int* a = shared_memory;
	int* b = shared_memory + num_elements;
	if (a == (int*) -1) {
		perror("shmat");
		exit(1);
	}
	a = ReadFromFile(a, argv[3], num_elements);
	// assign b to a
	for (int i = 0; i < num_elements; i++) {
		b[i] = a[i];
	}

	int step = 1;
	// loop for every iteration of hillis steele
	// after full iteration, b = a, then reset. mutated values go to b, as array changes, then end result updated in a.
	while (1) {
		// exit condition, when the step exceeds array size
		if (0 + step > num_elements - 1) {
			break;
		}
		for (int i = 0; i < num_cores; i++) {
			// create children
			pid_t pid = fork();
		
			if (pid < 0) {
				// error creating children
				perror("fork failed");
				return 1;
			} else if (pid == 0) {
				int start_index = i * segment_size;
				int end_index = start_index + segment_size;
				if (i == num_cores - 1 && remainder != 0) {
					end_index += remainder;
				}
				// safety check to make sure no undefined behavior
				if (end_index > num_elements) {
					end_index = num_elements;
				}
				// running algorithm for section in child, then clean up and go to next child
				HillisSteele(b, a, start_index, end_index, step);
				shmdt(b);
				shmdt(a);
				exit(0);
			}
		}
		for (int i = 0; i < num_cores; i++) {
			// parent process waits for child to complete
			wait(NULL);
		}
		// set a to b now that b is fully updated with the new iteration
		// a is ready for next iteration now
		for (int i = 0; i < num_elements; i++) {
			a[i] = b[i];
		}
		// increase step by power of 2
		step *= 2;
	}

	if (OutputToFile(b, num_elements, output_file) == 0) { 
		printf("Output written to: %s\n", output_file);
	}

	// memory cleanup
	shmdt(a);
	shmdt(b);
	if (shmctl(segment_id, IPC_RMID, NULL) == -1) {
		perror("shmctl");
		return 1;
	}
	
	return 0;
}
