#include "myftp.h"
#include <arpa/inet.h>

void quit_with_usage_msg()
{
	printf("Usage: ./myftpclient clientconfig.txt <list|get|put> <file>\n");
	exit(0);
}

void getData(char *filename, int *n, int *k, int *blockSize, char ipAddress[5][15], int port[5])
{
	//Get the file
    FILE *fptr = fopen(filename, "rb");
    if (fptr == NULL)
    {
        printf("file open error: %s (Errno:%d)\n", (char *)strerror(errno), errno);
        return;
    }
    fseek(fptr, 0, SEEK_END);
    int filesize = ftell(fptr);
    rewind(fptr);
    char *buffer = malloc(sizeof(char) * filesize);
    char serverData[8][255];
    int i = 0;
    while ((!feof(fptr)) && i < 8)
    {
        fscanf(fptr, "%s", buffer);
        strcpy(serverData[i], buffer);
        i++;
    }
    fclose(fptr);

    int index = (strchr(serverData[3], ':')) - serverData[3];

    *n = atoi(serverData[0]);
    *k = atoi(serverData[1]);
    *blockSize = atoi(serverData[2]);
    for (int i = 0; i < 5; i++)
    {
        memcpy(ipAddress[i], serverData[i + 3], index);
        char tem[6];
        memcpy(tem, serverData[i + 3] + index + 1, 5);
        port[i] = atoi(tem);
    }
}

void message_to_server(int sd, struct message_s m_header, char *payload, int payload_length)
{
	struct packet *send_message;
	send_message = malloc(sizeof(struct message_s));
	send_message->header = m_header;
	send_message->header.length = htonl(send_message->header.length);
	if (payload != NULL)
	{
		send_message = realloc(send_message, 10 + payload_length);
		memcpy(send_message->payload, payload, m_header.length - 10);
	}
	if (sendn(sd, send_message, m_header.length) < 0)
	{
		printf("Send error: %s (Errno:%d)\n", strerror(errno), errno);
		exit(0);
	}
	free(send_message);
}

void client_list(int sd)
{
	//Construct List Request
	struct message_s list_request;
	memcpy(list_request.protocol, (unsigned char[]){'m', 'y', 'f', 't', 'p'}, 5);
	list_request.type = 0xA1;
	list_request.length = sizeof(struct message_s);
	struct packet list_request_packet;
	list_request_packet.header = list_request;

	message_to_server(sd, list_request, NULL, 0);

	int len;
	struct packet list_reply;
	int totalsize = 0;
	if ((len = recvn(sd, &list_reply, sizeof(struct message_s))) < 0)
	{
		printf("Send error: %s (Errno:%d)\n", strerror(errno), errno);
		return;
	}

	//Validate Message
	if (check_myftp(list_reply.header.protocol) < 0)
	{
		printf("Invalid Protocol\n");
		return;
	}

	if (list_reply.header.type != 0xA2)
	{
		printf("Invalid Message Type\n");
		return;
	}

	if (len == 0)
		return;

	list_reply.header.length = ntohl(list_reply.header.length);

	if (list_reply.header.length > 10)
	{
		printf("---file list start---\n");
		char payload[Buffer_Size];
		while (1)
		{
			memset(&payload, 0, Buffer_Size);
			if ((len = recv(sd, &payload, Buffer_Size, 0)) < 0)
			{
				printf("Send error: %s (Errno:%d)\n", strerror(errno), errno);
				break;
			}
			printf("%s", payload);
			totalsize += len;
			if (list_reply.header.length - 10 <= totalsize)
				break;
		}

		printf("---file list end---\n");
	}
	return;
}

void client_get(int sd, char *filename)
{
	printf("Get (%s)\n", filename);

	//Construct GET Request Message
	struct message_s get_request;
	memcpy(get_request.protocol, (unsigned char[]){'m', 'y', 'f', 't', 'p'}, 5);
	get_request.type = 0xB1;
	get_request.length = sizeof(struct message_s) + strlen(filename) + 1;

	message_to_server(sd, get_request, filename, strlen(filename) + 1);

	//Receive GET Reply
	struct packet get_reply;
	int len;
	//Error
	if ((len = recvn(sd, &get_reply, sizeof(struct message_s))) < 0)
	{
		printf("Send error: %s (Errno:%d)\n", strerror(errno), errno);
		return;
	}

	if (len == 0)
	{
		printf("0 Packet Received\n");
		return;
	}

	//Check MYFTP
	if (check_myftp(get_reply.header.protocol) < 0)
	{
		printf("Invalid Protocol\n");
		return;
	}

	//Process Reply
	if ((unsigned char)get_reply.header.type == 0xB2)
	{
		//Receive File and write to disk
		struct packet file_data;
		int file_data_len;
		if ((file_data_len = recvn(sd, &file_data, sizeof(struct message_s))) < 0)
		{
			printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
			return;
		}
		if (file_data_len == 0)
		{
			printf("0 Packet Received\n");
			return;
		}
		FILE *fptr = fopen(filename, "w");
		int transfered_data_len = 0;

		file_data.header.length = ntohl(file_data.header.length);

		if (file_data.header.length > 10)
		{
			printf("File size received : %d\n", file_data.header.length - 10);
			char payload[Buffer_Size + 1];
			while (1)
			{
				memset(&payload, 0, Buffer_Size + 1);
				if ((file_data_len = recv(sd, &payload, Buffer_Size, 0)) < 0)
				{
					printf("Send error: %s (Errno:%d)\n", strerror(errno), errno);
					break;
				}
				fwrite(payload, 1, file_data_len, fptr);
				transfered_data_len += file_data_len;
				if (file_data.header.length - 10 <= transfered_data_len)
					break;
			}
		}
		fclose(fptr);
		printf("[%s] Download Completed.\n", filename);
	}
	else if ((unsigned char)get_reply.header.type == 0xB3)
	{
		printf("File does not exist.\n");
		return;
	}
	else
	{
		printf("Invalid message type.\n");
		return;
	}
}

void client_put(int n, int* sd, char *filename)
{
	printf("Put (%s) to %d servers\n", filename, n);

	// Check File existance
	if (access(filename, F_OK) != -1)
		printf("[%s] Exist.\n", filename);
	else
	{
		printf("The File [%s] does not exist.\n", filename);
		return;
	}

	//Construct Put Request
	struct message_s put_request;
	memcpy(put_request.protocol, (unsigned char[]){'m', 'y', 'f', 't', 'p'}, 5);
	put_request.type = 0xC1;
	put_request.length = sizeof(struct message_s) + strlen(filename) + 1;
	for(int i = 0; i < n; i++){
		message_to_server(sd[i], put_request, filename, strlen(filename) + 1);
	}

	//Receive Post Reply
	struct packet post_reply;
	int len;
	//Error
	if ((len = recvn(sd, &post_reply, sizeof(struct message_s))) < 0)
	{
		printf("Send error: %s (Errno:%d)\n", strerror(errno), errno);
		return;
	}

	if (len == 0)
	{
		printf("0 Packet Received\n");
		return;
	}

	//Validate Message
	if (check_myftp(post_reply.header.protocol) < 0)
	{
		printf("Invalid Protocol\n");
		return;
	}

	if (post_reply.header.type != 0xC2)
	{
		printf("Invalid Message Type\n");
		return;
	}

	//Open File
	FILE *fptr = fopen(filename, "rb");
	if (fptr == NULL)
	{
		printf("file open error: %s (Errno:%d)\n", (char *)strerror(errno), errno);
		return;
	}

	//Get File Size
	fseek(fptr, 0, SEEK_END);
	int filesize = ftell(fptr);
	rewind(fptr);

	//Send File
	char *buffer = malloc(sizeof(char) * filesize);
	printf("File Size To Send %lu\n", fread(buffer, sizeof(char), filesize, fptr));
	struct message_s file_data;
	memcpy(file_data.protocol, (unsigned char[]){'m', 'y', 'f', 't', 'p'}, 5);
	file_data.type = 0xFF;
	file_data.length = sizeof(struct message_s) + filesize;
	message_to_server(sd, file_data, buffer, filesize);

	free(buffer);
	fclose(fptr);
	printf("File Transfer Completed.\n");
}

int main(int argc, char **argv)
{
	//Mode
	// 0 - LIST
	// 1 - GET
	// 2 - PUT
	int mode;
	char filename[255];

	//Input Checking
	if ((argc < 3) || (strcmp(argv[1], "clientconfig.txt")) != 0 )
	{
		quit_with_usage_msg();
	}

	if ((strcmp(argv[2], "get")) == 0 || (strcmp(argv[2], "put") == 0))
	{
		if (argc == 4)
		{
			strcpy(filename, argv[3]);
			if (strcmp(argv[2], "get") == 0)
			{
				mode = 1;
			}
			else
			{
				mode = 2;
			}
		}
		else
		{
			quit_with_usage_msg();
		}
	}
	else if ((strcmp(argv[2], "list")) == 0)
	{
		mode = 0;
	}
	else
	{
		quit_with_usage_msg();
	}
	char ipAddress[5][15];
    memset(ipAddress, 0, sizeof(ipAddress));
    int port[5];
    memset(port, 0, sizeof(port));
    int n,k,blockSize,connectedServers = 0,defaultServer;
    getData(argv[1], &n, &k, &blockSize, ipAddress, port);
	
	//Connection Setup




	//Create n socket descriptor for server connection
	int* sd = (int*)malloc(sizeof(int) * n);
	for(int i = 0; i < n; i++){
		sd[i] = socket(AF_INET, SOCK_STREAM, 0);
		struct sockaddr_in server_addr;
		memset(&server_addr, 0, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = inet_addr(ipAddress[i]);
		server_addr.sin_port = htons(port[i]);
		if (connect(sd[i], (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
		{
			printf("Failed to connect %s:::%d\n", ipAddress[i], port[i]);
			//printf("connection error: %s (Errno:%d)\n", strerror(errno), errno);
			//exit(0);
			continue;
		}
		connectedServers++;
		defaultServer = i;
	}
	printf("%d servers connected.\n", connectedServers);

	if(connectedServers == 0){
		printf("All servers are not available. Please try again.\n");
		return 1;
	}

	//Operation
	switch (mode)
	{
	case 0:
		printf("List files from %dth server.\n", defaultServer+1);
		client_list(sd[defaultServer]);
		break;
	case 1:
		if(connectedServers >= k){
			//To work on later
			//client_get(sd, filename);
		}else{
			printf("Unable to GET file if number of servers connected < %d\n", k);
		}
		break;
	case 2:
		if(connectedServers == n){
			//client_put(n, sd, filename);
		}else{
			printf("Unable to PUT file if number of servers connected != %d\n", n);
		}
		break;
	}
	
	for(int i = 0; i < n; i++){
		close(sd[i]);
	}

	return 0;

}

// Chunking Helper functions
// This calculate number of stripes
int number_of_stripe(char* file_name, int k){
    //printf("Inside no. stripe function:\n");
    FILE *fptr = fopen(file_name,"r");
    fseek(fptr, 0, SEEK_END);
    int filesize = ftell(fptr);
    fclose(fptr);
    int stripe_amount = ceil(filesize / (Block_Size * k));
    printf("%d\n",stripe_amount);
    return stripe_amount;
}

// Usage: The file will be splited with file_name input,n and k
// You can also add Stripe **stripes parameter to preserve the Object Lists
void chunk_file(char* file_name, int n, int k){
    //printf("Inside chunk file function:\n");
    // Read the file
    int fd = open(file_name, O_RDONLY);
    
    if (!fd)
    {
        printf("file open error: %s (Errno:%d)\n", (char *)strerror(errno), errno);
        return;
    }
    // Move this line below to main to preserve the Objects
    Stripe **stripes = (Stripe**)malloc(sizeof(Stripe)*number_of_stripe(file_name, k));

    // Split file into stripe
    for(int h = 0;h < number_of_stripe(file_name, k);h++){

        // Declare Stripe Object
        stripes[h] = (Stripe*)malloc(sizeof(Stripe));

        // declare Datablock Array inside a Stripe
        stripes[h]->data_blocks = (unsigned char**)malloc(k*sizeof(Block_Size));

        // declare parity block array inside a Stripe
        stripes[h]->parity_blocks = (unsigned char**)malloc((n-k)*sizeof(Block_Size));

        // Split file into datablock
        // declare Datablock with for loop and chunk file
        for(int i = 0; i < k; i++){
            stripes[h]->data_blocks[i] = (unsigned char*)malloc(Block_Size*sizeof(char));
            pread(fd, stripes[h]->data_blocks[i], Block_Size, i*Block_Size);
            // Declare file chunk name string
            char* file_chunk_name = (char*)malloc(sizeof(char)*255);
            sprintf(file_chunk_name,"%s-%d-%d",file_name,h,i);
            
            printf("%s\n",file_chunk_name);

            FILE* wfptr = fopen(file_chunk_name,"w");
            fwrite(stripes[h]->data_blocks[i], 1, Block_Size, wfptr );
            fclose(wfptr);
        }
    }
}

char** find_file(char* file_name){
    //printf("Inside File Search function:\n");
    struct dirent *de;  // Pointer for directory entry 
  
    // opendir() returns a pointer of DIR type.  
    DIR *dr = opendir("."); 
  
    if (dr == NULL)  // opendir returns NULL if couldn't open directory 
    { 
        printf("Could not open current directory" ); 
        return NULL;
    } 
  
    char** file_list = malloc(sizeof(char)*255*100);
    int i = 0;
    while ((de = readdir(dr)) != NULL) {
        if(strstr(de->d_name, file_name)){
            file_list[i] = de->d_name;
            //printf("%s\n", de->d_name); 
            i++;
        }
    }
    closedir(dr);
    //printf("Print file_list\n");
    //printf("%s\n",file_list[2]);
    return file_list;
}