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
	free(buffer);
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

void client_get(int n, int k, int blockSize, int *sd, char *filename)
{
	int workNodeIndex = 0;
	int workNode[5];
	int connectedServers = 0;
	//Negative if server[i] is not connected
	int *nextBlockToRecvPtr = malloc(sizeof(int) * n);
	for (int i = 0; i < n; i++)
	{
		if (sd[i] != -1)
		{
			connectedServers++;
			nextBlockToRecvPtr[i] = 0;
			workNode[workNodeIndex++] = i;
		}
		else
		{
			nextBlockToRecvPtr[i] = -1;
		}
	}

	printf("Get (%s)\n", filename);

	int blocksToReceive = -1;
	int fullFileSize;
	int numberOfStripe = -1;
	int maxH;
	char *blockName = malloc(sizeof(char) * 1024);
	strcpy(blockName, "_META_");
	strcat(blockName, filename);

	//Do all server received the file
	while (blocksToReceive != 0)
	{
		int maxfd = 0;
		fd_set fds;

		FD_ZERO(&fds);
		for (int i = 0; i < n; i++)
		{
			//if all the blocks are not yet received, put it to fds
			if (nextBlockToRecvPtr[i] >= 0)
			{
				FD_SET(sd[i], &fds);
				maxfd = sd[i] > maxfd ? sd[i] : maxfd;
			}
		}

		//IO Multiplexing
		select(maxfd + 1, NULL, &fds, NULL, NULL);
		for (int serverIndex = 0; serverIndex < workNodeIndex; serverIndex++)
		{
			int serverID = workNode[serverIndex];
			//Check if all blocks for this server is received
			if ((numberOfStripe != -1) && (nextBlockToRecvPtr[serverID] >= numberOfStripe))
			{
				nextBlockToRecvPtr[serverID] = -1;
				continue;
			}

			if (blocksToReceive > 0)
			{
				sprintf(blockName, "%s-%0*d_%d", filename, maxH, nextBlockToRecvPtr[serverID], serverID);
				printf("Get block [%s]\n", blockName);
			}
			printf("Server is %d\n",sd[serverID]);
			//Get file from server
			if (FD_ISSET(sd[serverID], &fds))
			{
				//Construct GET Request Message
				struct message_s get_request;
				memcpy(get_request.protocol, (unsigned char[]){'m', 'y', 'f', 't', 'p'}, 5);
				get_request.type = 0xB1;
				get_request.length = sizeof(struct message_s) + strlen(blockName) + 1;
				message_to_server(sd[serverID], get_request, blockName, strlen(blockName) + 1);

				//Receive GET Reply
				struct packet get_reply;
				int len;
				//Error
				if ((len = recvn(sd[serverID], &get_reply, sizeof(struct message_s))) < 0)
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
					if ((file_data_len = recvn(sd[serverID], &file_data, sizeof(struct message_s))) < 0)
					{
						printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
						return;
					}
					if (file_data_len == 0)
					{
						printf("0 Packet Received\n");
						return;
					}
					FILE *fptr = fopen(blockName, "w");
					int transfered_data_len = 0;

					file_data.header.length = ntohl(file_data.header.length);

					if (file_data.header.length > 10)
					{
						printf("File size received : %d\n", file_data.header.length - 10);
						char payload[Buffer_Size + 1];
						while (1)
						{
							memset(&payload, 0, Buffer_Size + 1);
							if ((file_data_len = recv(sd[serverID], &payload, Buffer_Size, 0)) < 0)
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
					printf("Download Completed from Server %d.\n", serverID);
					blocksToReceive--;
					nextBlockToRecvPtr[serverID]++;
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

			//Process metadata
			if (blocksToReceive < 0)
			{
				fullFileSize = getFileSizeFromMetadata(blockName);
				numberOfStripe = ceil((double)fullFileSize / (blockSize * k));
				printf("Metadata received:\nFile size: %d\nNumber of stripe: %d\n", fullFileSize, numberOfStripe);
				nextBlockToRecvPtr[serverID] = 0;
				blocksToReceive = numberOfStripe * connectedServers;
				serverID = -1;
				maxH = ceil(log(numberOfStripe) / log(10));
				remove(blockName);
			}
		}
	}

	//Decode Here
	//Prepared workNode for decodeData
	for (int i = 0; i < k; i++)
	{
		printf("First k Work Nodes: %d\n", workNode[i]);
	}
	Stripe **stripes;
	restoreBlocks(filename, n, k, blockSize, stripes, workNode);

	//Merge Here
	//Don't merge until u got all the blocks on disk
	unsigned char **blockList = (unsigned char **)malloc(sizeof(unsigned char *) * n * numberOfStripe);
	int blocksToSend = find_file(filename, blockList);
	merge_file(filename, blockList, blockSize, fullFileSize, n, k, 1);

	free(nextBlockToRecvPtr);
	free(blockName);

	for (int i = 0; i < blocksToSend; i++)
	{
		free(blockList[i]);
	}
	free(blockList);
}

void client_put(int n, int k, int blockSize, int *sd, char *filename)
{
	printf("Put (%s) to %d servers\n", filename, n);

	//Flag 1 if the file is transfered to server[i]
	int *completedServer = (int *)malloc(sizeof(int) * n);
	for (int i = 0; i < n; i++)
	{
		completedServer[i] = 0;
	}

	// Check file existance
	if (access(filename, F_OK) != -1)
		printf("[%s] Exist.\n", filename);
	else
	{
		printf("The File [%s] does not exist.\n", filename);
		return;
	}

	//Get file size
	int fileSize = fileSizeOf(filename);

	//Split file into blocks and save in local
	Stripe **stripes;
	chunkFile(filename, n, k, blockSize, stripes);
	int numberOfStripe = number_of_stripe(filename, k, blockSize);
	unsigned char **blockList = (unsigned char **)malloc(sizeof(unsigned char *) * n * numberOfStripe);

	//Current progress for each server
	int *nextBlockToSendPtr = malloc(sizeof(int) * n);
	for (int i = 0; i < n; i++)
	{
		nextBlockToSendPtr[i] = 0;
	}

	//Total number of blocks to send
	int blocksToSend = find_file(filename, blockList) + n;

	//Generate metadata
	unsigned char *metadataName = (unsigned char *)malloc(sizeof(unsigned char) * 1024);
	generateMetadata(metadataName, filename, fileSize);

	printf("Start sending\n");
	//For the last parameter, put 1 for deleting all the blocks after merging. Otherwise, 0.
	//merge_file(filename, blockList, blockSize, fileSize, 1);

	//Do all server received the file
	while (blocksToSend > 0)
	{
		int maxfd = 0;
		fd_set fds;

		FD_ZERO(&fds);
		for (int i = 0; i < n; i++)
		{
			//if the file is not yet uploaded to that server, put it to fds
			if (completedServer[i] == 0)
			{
				FD_SET(sd[i], &fds);
				maxfd = sd[i] > maxfd ? sd[i] : maxfd;
			}
		}

		//IO Multiplexing
		select(maxfd + 1, NULL, &fds, NULL, NULL);

		for (int serverID = 0; serverID < n; serverID++)
		{
			//Check if all blocks for this server is sent
			if (nextBlockToSendPtr[serverID] > numberOfStripe)
			{
				completedServer[serverID] = 1;
				continue;
			}
			if (FD_ISSET(sd[serverID], &fds))
			{
				char blockName[1024];
				if (nextBlockToSendPtr[serverID] == numberOfStripe)
				{
					strcpy(blockName, metadataName);
				}
				else
				{
					strcpy(blockName, blockList[nextBlockToSendPtr[serverID] * n + serverID]);
				}
				//Construct Put Request
				struct message_s put_request;
				memcpy(put_request.protocol, (unsigned char[]){'m', 'y', 'f', 't', 'p'}, 5);
				put_request.type = 0xC1;
				put_request.length = sizeof(struct message_s) + strlen(blockName) + 1;
				message_to_server(sd[serverID], put_request, blockName, strlen(blockName) + 1);

				//Receive Post Reply
				struct packet post_reply;
				int len;
				//Error
				if ((len = recvn(sd[serverID], &post_reply, sizeof(struct message_s))) < 0)
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
				FILE *fptr = fopen(blockName, "rb");
				if (fptr == NULL)
				{
					printf("file open error: %s (Errno:%d)\n", (char *)strerror(errno), errno);
					printf("[Debug]target file: %s\n", blockName);
					printf("[Debug]target server: %d\n", serverID);
					printf("[Debug]nextBlockToSendPtr: %d\n", nextBlockToSendPtr[serverID]);
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
				message_to_server(sd[serverID], file_data, buffer, filesize);

				free(buffer);
				fclose(fptr);
				printf("File %s transfered to Server %d\n", blockName, serverID);

				nextBlockToSendPtr[serverID]++;
				blocksToSend--;
			}
		}
	}

	printf("PUT succeed.\n");

	//Clean cache
	for (int i = 0; i < numberOfStripe * n; i++)
	{
		remove(blockList[i]);
		free(blockList[i]);
	}
	remove(metadataName);
	free(blockList);
	free(completedServer);
	free(nextBlockToSendPtr);
	free(metadataName);
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
	if ((argc < 3) || (strcmp(argv[1], "clientconfig.txt")) != 0)
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
	int n, k, blockSize, connectedServers = 0, defaultServer;
	getData(argv[1], &n, &k, &blockSize, ipAddress, port);

	//Connection Setup

	//Create n socket descriptor for server connection
	int *sd = (int *)malloc(sizeof(int) * n);
	for (int i = 0; i < n; i++)
	{
		sd[i] = socket(AF_INET, SOCK_STREAM, 0);
		struct sockaddr_in server_addr;
		memset(&server_addr, 0, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = inet_addr(ipAddress[i]);
		server_addr.sin_port = htons(port[i]);
		if (connect(sd[i], (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
		{
			printf("Failed to connect %s:::%d\n", ipAddress[i], port[i]);
			sd[i] = -1;
			continue;
		}
		connectedServers++;
		defaultServer = i;
	}

	if (connectedServers == 0)
	{
		printf("All servers are not available. Please try again.\n");
		return 1;
	}

	//Operation
	switch (mode)
	{
	case 0:
		printf("List files from %dth server.\n", defaultServer + 1);
		client_list(sd[defaultServer]);
		break;
	case 1:
		if (connectedServers >= k)
		{
			//To work on later
			client_get(n, k, blockSize, sd, filename);
		}
		else
		{
			printf("Unable to GET file if number of servers connected < %d\n", k);
		}
		break;
	case 2:
		if (connectedServers == n)
		{
			client_put(n, k, blockSize, sd, filename);
		}
		else
		{
			printf("Unable to PUT file if number of servers connected != %d\n", n);
		}
		break;
	}

	for (int i = 0; i < n; i++)
	{
		close(sd[i]);
	}
	free(sd);
	return 0;
}
