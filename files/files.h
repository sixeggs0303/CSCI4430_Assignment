#include <isa-l.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h> 
#include <math.h>

// Stripe Structure
typedef struct stripe{
    unsigned char** blocks;
    uint8_t* encodeMatrix;
    uint8_t* errorsMatrix;
    uint8_t* invertMatrix;
    uint8_t* table;
} Stripe;

// Encoding
uint8_t* encodeData(int n, int k, Stripe *stripe, size_t blockSize);

// 1. File chunk & merge

//! This calculate number of stripe
int number_of_stripe(char *file_name, int k, int blockSize);

// Split File into k * number of stripe blocks, return a stripe array, use in client
void chunkFile(char *file_name, int n, int k, int blockSize, Stripe **stripes);

// convert stripe to file blocks, used in server
void stripeToFile(char* fileName, int k, int blockSize, Stripe *stripe, int stripeIndex);

// convert blocks to a file parts, used in server
void blockToFile(char* fileName, int k, int blockSize, unsigned char* block, int stripeIndex, int blockIndex);

// Merge file wth file_list
void merge_file(char *filename, char **file_list, int blockSize, int fileSize, int deleteBlock);

// Split all blocks
void stripesToFile(char* fileName, int n, int k, int blockSize, Stripe **stripes);

// 2. File Search

// Defining comparator function as per the requirement
static int comparator(const void *a, const void *b);

// Function to sort the string array
//void sort_strings(char **arr, int n);
void sort_strings(unsigned char **arr, int n);

// The function to find the file, returning file_list
int find_file(char *fileName, unsigned char **fileList);

// Function to create metadata
void generateMetadata(char* metadataName, char* filename, int fileSize);



int fileSizeOf(char *filename);