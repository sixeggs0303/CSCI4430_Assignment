
#include "files.h"

// Chunking & Merging Helper functions
// This calculate number of stripes
int number_of_stripe(char *fileName, int k, int blockSize)
{
	//printf("Inside no. stripe function:\n");
	int filesize = fileSizeOf(fileName);

	int stripeAmount = ceil((double)filesize / (blockSize * k));
	return stripeAmount;
}

// Usage: The file will be splited with fileName input,n and k
// You can also add Stripe **stripes parameter to preserve the Object Lists
void chunkFile(char *fileName, int n, int k, int blockSize, Stripe **stripes)
{
	// Initialize the stripe
	stripes = (Stripe **)malloc(sizeof(Stripe) * number_of_stripe(fileName, k, blockSize));

	// Read the file
	int fd = open(fileName, O_RDONLY);

	if (!fd)
	{
		printf("file open error: %s (Errno:%d)\n", (char *)strerror(errno), errno);
		return;
	}

	int numberOfStripe = number_of_stripe(fileName, k, blockSize);
	printf("number of chunk: %d\n", numberOfStripe);
	// Split file into stripe
	for (int h = 0; h < numberOfStripe; h++)
	{
		// Declare Stripe Object
		stripes[h] = (Stripe *)malloc(sizeof(Stripe));

		// declare block Array inside a Stripe
		stripes[h]->blocks = (unsigned char **)malloc(n * k * sizeof(blockSize));

		// Split file into block
		// declare block with for loop and chunk file
		int maxH = ceil(log(numberOfStripe) / log(10));
		//printf("max h is %d\n", maxH);

		for (int i = 0; i < n; i++)
		{
			stripes[h]->blocks[i] = (unsigned char *)malloc(blockSize);
			if (i < k)
			{
				pread(fd, stripes[h]->blocks[i], blockSize, (i + h * k) * blockSize);
			}
			/*
			// Declare file chunk name string
			char *fileChunkName = (char *)malloc(sizeof(char) * 255);
			sprintf(fileChunkName, "%s-%0*d-%d", fileName, maxH, h, i);
			//printf("%s\n",fileChunkName);
			FILE *wfptr = fopen(fileChunkName, "wb");
			fwrite(stripes[h]->blocks[i], 1, blockSize, wfptr);
			//printf("Block created: %s\n", fileChunkName);
			fclose(wfptr);
			*/
		}
		encodeData(n, k, stripes[h], blockSize);
	}
	stripesToFile(fileName, n, k, blockSize, stripes);
}

void merge_file(char *filename, unsigned char **file_list, int blockSize, int fileSize,int n,int k,int deleteBlock)
{
	//printf("Inside merge file function\n");
	int numberOfStripe = ceil((double)fileSize / (blockSize * k));
	Stripe** stripes = malloc(numberOfStripe*n*blockSize);
	for(int j = 0; j < numberOfStripe; j++){
		//stripes[0] = malloc(n * blockSize);
		//stripes[j]->encodeMatrix = malloc(sizeof(uint8_t) * (n * k));
		//stripes[j]->table = malloc(sizeof(uint8_t) * (32 * k * (n - k)));
		//stripes[j]->blocks = (unsigned char**)malloc(n * sizeof(unsigned char*) );

		stripes[j] = (Stripe*)malloc(n*blockSize);
		stripes[j]->blocks = (unsigned char**)malloc(5*blockSize);
		for(int i = 0; i < n; i++){
			stripes[j]->blocks[i] = (unsigned char*)malloc(blockSize);
			//printf("%d %d\n", j, i);
		}
	}

	//Write the merged file to "result_filename"
	char mergedFilename[1024];
	strcpy(mergedFilename, "result_");
	strcat(mergedFilename, filename);

	FILE *original_file = fopen(mergedFilename, "w");
	if (original_file == NULL)
	{
		printf("file open error: %s (Errno:%d)\n", (char *)strerror(errno), errno);
		return;
	}

	int mergedBytes = 0;
	int c;
	int serverIDPtr = 0;
	int stripePtr = 0;
	//printf("Inside merge file\n");
	while ((fileSize - mergedBytes) > 0)
	{
		// Merge content in file_list
		FILE *fp1 = fopen(file_list[stripePtr*n + serverIDPtr], "r");
		if (fp1 == NULL)
		{
			perror("Error ");
			return;
		}
		//printf("Mergeing %s, merged size = %d\n", file_list[stripePtr*n + i], mergedBytes);
		
		while (((c = fgetc(fp1)) != EOF) && ((fileSize - mergedBytes) > 0))
		{
			fputc(c, original_file);
			
			mergedBytes++;
		}
		// Now debugging this
		fread(stripes[stripePtr]->blocks[serverIDPtr],1,blockSize,fp1);
		
		fclose(fp1);
		serverIDPtr++;
		if(serverIDPtr==k){
			serverIDPtr = 0;
			stripePtr++;
		}
	}
	printf("File restored->[%s].\n",mergedFilename);
	fclose(original_file);

	if(deleteBlock){
		int numberOfBlocks = ceil((double)fileSize / ((double)blockSize*k)) * n;
		for(int i = 0; i<numberOfBlocks;i++){
			remove(file_list[i]);
		}
		printf("Removed %d cache.\n",numberOfBlocks);
	}
}

// Defining comparator function as per the requirement
static int comparator(const void *a, const void *b)
{
	// setting up rules for comparison
	return strcmp(*(const char **)a, *(const char **)b);
}

// Function to sort the array
void sort_strings(unsigned char **arr, int n)
{
	// calling qsort function to sort the array
	// with the help of Comparator
	qsort(arr, n, sizeof(*arr), comparator);
}

// Return File List
int find_file(char *fileName, unsigned char **fileList)
{

	//printf("Inside File Search function:\n");
	//printf("%s\n",fileName);
	struct dirent *de; // Pointer for directory entry

	// opendir() returns a pointer of DIR type.
	DIR *dr = opendir(".");

	if (dr == NULL) // opendir returns NULL if couldn't open directory
	{
		printf("Could not open current directory");
		return -1;
	}
	int list_length = 0;
	char *string_pattern = malloc(1024);
	sprintf(string_pattern, "%s-", fileName);
	while ((de = readdir(dr)) != NULL)
	{
		if (strstr(de->d_name, string_pattern))
		{
			fileList[list_length] = malloc(sizeof(unsigned char)*255);
			strcpy(fileList[list_length],de->d_name);
			list_length++;
		}
	}

	closedir(dr);
	sort_strings(fileList, list_length);
	return list_length;
}

void stripesToFile(char *fileName, int n, int k, int blockSize, Stripe **stripes)
{
	for (int i = 0; i < number_of_stripe(fileName, k, blockSize); i++)
	{
		stripeToFile(fileName, n, blockSize, stripes[i], i);
	}
}
void stripeToFile(char *fileName, int k, int blockSize, Stripe *stripe, int stripeIndex)
{

	int numberOfStripe = number_of_stripe(fileName, k, blockSize);
	int maxH = ceil(log(numberOfStripe) / log(10));
	for (int i = 0; i < k; i++)
	{
		blockToFile(fileName, k, blockSize, stripe->blocks[i], stripeIndex, i);
		/*
			// Declare file chunk name string
			char *fileChunkName = (char *)malloc(sizeof(char) * 255);
			sprintf(fileChunkName, "%s-%0*d-%d", fileName, maxH, stripeIndex, i);
			//printf("%s\n",fileChunkName);
			FILE *wfptr = fopen(fileChunkName, "wb");
			fwrite(stripe->blocks[i], 1, blockSize, wfptr);
			//printf("Block created: %s\n", fileChunkName);
			fclose(wfptr);	
			*/
	}
}

void blockToFile(char *fileName, int k, int blockSize, unsigned char *block, int stripeIndex, int blockIndex)
{

	int numberOfStripe = number_of_stripe(fileName, k, blockSize);
	int maxH = ceil(log(numberOfStripe) / log(10));

	// Declare file chunk name string
	char *fileChunkName = (char *)malloc(sizeof(char) * 255);
	sprintf(fileChunkName, "%s-%0*d_%d", fileName, maxH, stripeIndex, blockIndex);
	//printf("%s\n",fileChunkName);

	FILE *wfptr = fopen(fileChunkName, "wb");
	fwrite(block, 1, blockSize, wfptr);
	//printf("Block created: %s\n", fileChunkName);
	fclose(wfptr);
}

// encode
uint8_t *encodeData(int n, int k, Stripe *stripe, size_t blockSize)
{
	stripe->encodeMatrix = malloc(sizeof(uint8_t) * (n * k));
	stripe->table = malloc(sizeof(uint8_t) * (32 * k * (n - k)));

	gf_gen_rs_matrix(stripe->encodeMatrix, n, k);

	ec_init_tables(k, n - k, &stripe->encodeMatrix[k * k], stripe->table);

	unsigned char **blocksData = malloc(sizeof(unsigned char **) * n);
	for (int i = 0; i < n; i++)
	{
		blocksData[i] = stripe->blocks[i];
	}
	ec_encode_data(blockSize, k, n - k, stripe->table, blocksData, &blocksData[k]);
	//printf("%s",stripe->blocks[k]);
	return stripe->encodeMatrix;
}

uint8_t *decodeData(int n, int k, Stripe *stripe, size_t blockSize, int workNodes[])
{
	gf_gen_rs_matrix(stripe->encodeMatrix, n, k);
	// WorkNodes = the array of blocks index that is fetched from server
	
	for(int i = 0; i < k; i++){
		int r = workNodes[i];
		for(int j = 0; j < k; j++)
		{
			stripe->errorsMatrix[k*i+j] = stripe->encodeMatrix[k * r + j];
		}
	}
	
	gf_invert_matrix(stripe->errorsMatrix, stripe->invertMatrix, k);
	ec_init_tables(k, n - k, &stripe->encodeMatrix[k * k], stripe->table);

	unsigned char **blocksData = malloc(sizeof(unsigned char **) * n);
	for (int i = 0; i < n; i++)
	{
		blocksData[i] = stripe->blocks[i];
	}

	ec_encode_data(blockSize, k, n - k, stripe->table, blocksData, &blocksData[k]);
	return stripe->encodeMatrix;
}

void generateMetadata(char* metadataName, char* filename, int fileSize)
{
	char temp[1024] = "_META_";
	strcat(temp, filename);
	strcpy(metadataName, temp);
	printf("Generate metadata: %s\n",metadataName);
	FILE *mfptr = fopen(metadataName,"w");
	fprintf(mfptr,"%d",fileSize);
	fclose(mfptr);
}

int getFileSizeFromMetadata(char* metadataName)
{	
	int fileSize;
	FILE *mfptr = fopen(metadataName,"r");
	fscanf(mfptr,"%d",&fileSize);
	fclose(mfptr);
	return fileSize;
}

// Main for testing purposes or usage example
/*
int main(){
	// Mock data
	int k = 3;
	int n = 5;
	int blockSize = 40960;
	char* fileName = "dawn.jpg";
    int numberOfStripe = number_of_stripe(fileName, k, blockSize);
	printf("Number of chunks: %d\n", numberOfStripe);
    Stripe **stripes;
    chunkFile(fileName, n, k, blockSize, stripes);
	int stripeIndex = 12;
	int blockIndex = 2;
	//blockToFile(fileName, k, blockSize, stripes[stripeIndex]->blocks[blockIndex],stripeIndex,blockIndex);
	//stripeToFile(fileName, k, blockSize, stripes[stripeIndex], stripeIndex);
	//stripesToFile(fileName, k, blockSize, stripes);
	unsigned char** fileList = malloc(sizeof(unsigned char)*255*k*numberOfStripe);
	find_file(fileName, fileList);
	printf("%s\n",fileList[0]);
	//merge_file(fileName, fileList, blockSize, fileSizeOf(fileName), 1);
	printf("Hello World\n");
}
*/