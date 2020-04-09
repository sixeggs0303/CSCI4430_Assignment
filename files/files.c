
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
		stripes[h]->blocks = (unsigned char **)malloc(n * sizeof(unsigned char *));

		// Split file into block
		// declare block with for loop and chunk file
		int maxH = ceil(log(numberOfStripe) / log(10));
		//printf("max h is %d\n", maxH);

		for (int i = 0; i < n; i++)
		{
			stripes[h]->blocks[i] = (unsigned char *)malloc(blockSize);
			pread(fd, stripes[h]->blocks[i], blockSize, (i + h * k) * blockSize);
		}
		encodeData(n, k, stripes[h], blockSize);
	}
	stripesToFile(fileName, n, k, blockSize, stripes);

	//Free
	for (int h = 0; h < numberOfStripe; h++)
	{
		for (int i = 0; i < n; i++)
		{
			free(stripes[h]->blocks[i]);
		}
		free(stripes[h]->blocks);
		free(stripes[h]);
	}
	free(stripes);
}

// Usage: The file will be splited with fileName input,n and k
// You can also add Stripe **stripes parameter to preserve the Object Lists
void restoreBlocks(char *fileName, int n, int k, int blockSize, Stripe **stripes, int *workNode)
{
	// Initialize the stripe
	int numberOfStripe = number_of_stripe(fileName, k, blockSize);
	stripes = (Stripe **)malloc(sizeof(Stripe) * numberOfStripe);

	// declare block with for loop and read blocks from disk
	int maxH = ceil(log(numberOfStripe) / log(10));

	// Split file into stripe
	for (int h = 0; h < numberOfStripe; h++)
	{
		// Declare Stripe Object
		stripes[h] = (Stripe *)malloc(sizeof(Stripe));

		// declare block Array inside a Stripe
		stripes[h]->blocks = (unsigned char **)malloc(n * sizeof(unsigned char *));

		// n blocks is unnecessary, fix it later
		// initialize blocks
		for (int i = 0; i < n; i++)
		{
			stripes[h]->blocks[i] = (unsigned char *)malloc(blockSize);
		}

		for (int i = 0; i < k; i++)
		{
			char *blockName = (char *)malloc(sizeof(char) * 1024);
			//stripes[h]->blocks[workNode[i]] = (unsigned char *)malloc(blockSize);

			// Read the block
			sprintf(blockName, "%s-%0*d_%d", fileName, maxH, h, workNode[i]);
			FILE *bptr = fopen(blockName, "rb");
			if (bptr == NULL)
			{
				printf("file open error: %s (Errno:%d)\n", (char *)strerror(errno), errno);
				return;
			}
			fread(stripes[h]->blocks[workNode[i]], 1, blockSize, bptr);
			printf("Finished reading block [%s] into stripes[%d]->blocks[%d]\n", blockName, h, workNode[i]);
			fclose(bptr);
		}
		decodeData(n, k, stripes[h], blockSize, workNode);
	}

	//Stripes prepared

	stripesToFile(fileName, n, k, blockSize, stripes);

	//Free
	for (int h = 0; h < numberOfStripe; h++)
	{
		for (int i = 0; i < n; i++)
		{
			free(stripes[h]->blocks[i]);
		}
		free(stripes[h]->blocks);
		free(stripes[h]);
	}
	free(stripes);
}

void merge_file(char *filename, unsigned char **file_list, int blockSize, int fileSize, int n, int k, int deleteBlock)
{
	//printf("Inside merge file function\n");

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
		FILE *fp1 = fopen(file_list[stripePtr * n + serverIDPtr], "r");
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
		fclose(fp1);
		serverIDPtr++;
		if (serverIDPtr == k)
		{
			serverIDPtr = 0;
			stripePtr++;
		}
	}
	printf("File restored->[%s].\n", mergedFilename);
	fclose(original_file);

	if (deleteBlock)
	{
		int numberOfBlocks = ceil((double)fileSize / ((double)blockSize * k)) * n;
		for (int i = 0; i < numberOfBlocks; i++)
		{
			remove(file_list[i]);
		}
		printf("Removed %d cache.\n", numberOfBlocks);
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
			fileList[list_length] = malloc(sizeof(unsigned char) * 255);
			strcpy(fileList[list_length], de->d_name);
			list_length++;
		}
	}

	closedir(dr);
	sort_strings(fileList, list_length);
	free(string_pattern);
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
	free(fileChunkName);
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
	stripe->encodeMatrix = malloc(sizeof(uint8_t) * (n * k));
	stripe->errorsMatrix = malloc(sizeof(uint8_t) * (k * k));
	stripe->invertMatrix = malloc(sizeof(uint8_t) * (k * k));
	stripe->table = malloc(sizeof(uint8_t) * (32 * k * (n - k)));
	uint8_t *decodedMatrix = malloc(sizeof(uint8_t) * (k * k));

	//Get list of failNodes
	int *failNodes = malloc(sizeof(int) * (n - k));
	int failNodesIndex = 0;
	int workNodesIndex = 0;
	for (int i = 0; i < n; i++)
	{
		if ((workNodes[workNodesIndex] == i) && (workNodesIndex < k))
		{
			workNodesIndex++;
		}
		else
		{
			failNodes[failNodesIndex++] = i;
		}
	}

	gf_gen_rs_matrix(stripe->encodeMatrix, n, k);

	// WorkNodes = the array of blocks index that is fetched from server
	for (int i = 0; i < k; i++)
	{
		int r = workNodes[i];
		for (int j = 0; j < k; j++)
		{
			stripe->errorsMatrix[k * i + j] = stripe->encodeMatrix[k * r + j];
		}
	}

	gf_invert_matrix(stripe->errorsMatrix, stripe->invertMatrix, k);

	//Generate decodeMatrix
	memset(decodedMatrix, 0, sizeof(uint8_t) * k * k);
	for (int i = 0; i < (n - k); i++)
	{
		int r = failNodes[i];
		if (r < k)
		{
			for (int j = 0; j < k; j++)
			{
				decodedMatrix[k * i + j] = stripe->invertMatrix[k * r + j];
			}
		}
	}

	ec_init_tables(k, n - k, decodedMatrix, stripe->table);

	unsigned char **existingBlocks = malloc(sizeof(unsigned char *) * k);
	unsigned char **missingBlocks = malloc(sizeof(unsigned char *) * (n - k));
	workNodesIndex = 0;
	failNodesIndex = 0;
	for (int i = 0; i < n; i++)
	{
		if (i == failNodes[failNodesIndex])
		{
			missingBlocks[failNodesIndex++] = (unsigned char *)stripe->blocks[i];
		}
		else
		{
			existingBlocks[workNodesIndex++] = (unsigned char *)stripe->blocks[i];
		}
	}

	ec_encode_data(blockSize, k, n - k, stripe->table, existingBlocks, missingBlocks);

	free(decodedMatrix);
	free(failNodes);
	return stripe->encodeMatrix;
}

void generateMetadata(char *metadataName, char *filename, int fileSize)
{
	char temp[1024] = "_META_";
	strcat(temp, filename);
	strcpy(metadataName, temp);
	printf("Generate metadata: %s\n", metadataName);
	FILE *mfptr = fopen(metadataName, "w");
	fprintf(mfptr, "%d", fileSize);
	fclose(mfptr);
}

int getFileSizeFromMetadata(char *metadataName)
{
	int fileSize;
	FILE *mfptr = fopen(metadataName, "r");
	fscanf(mfptr, "%d", &fileSize);
	fclose(mfptr);
	return fileSize;
}
