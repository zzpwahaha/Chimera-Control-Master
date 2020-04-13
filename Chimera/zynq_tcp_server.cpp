#include "zynq_tcp_server.h"

#define MAX 80
#define DEVLEN 4
#define PORT 8080 
#define SA struct sockaddr
#define LEN_BYTE_BUF 4

void writeAxisFifo(char* device_str, char* command_str) {

	int seq_fd;
	char axis_fifo_path[] = "/dev/axis_fifo_0x000000008000";
	char* device_path_c = new char[strlen(device_str) + strlen(axis_fifo_path) + 1];
	strcpy(device_path_c, axis_fifo_path);
	strcat(device_path_c, device_str);

	printf("device_path = %s\n", device_path_c);

	seq_fd = open(device_path_c, O_RDWR);

	if (seq_fd == -1)
	{
		printf("Cannot open sequencer device");
		exit(1);
	}

	char byte_buf[LEN_BYTE_BUF];
	byte_buf[3] = 0x01; //enable for DIO
	byte_buf[2] = 0x00;
	byte_buf[1] = 0x00; //address 0x00
	byte_buf[0] = 0x00; //address 0x00
	write(seq_fd, &byte_buf, LEN_BYTE_BUF);

	byte_buf[3] = 0x00; //timestamp (32-bit)
	byte_buf[2] = 0x00; //timestamp (32-bit)
	byte_buf[1] = 0x00; //timestamp (32-bit)
	byte_buf[0] = 0x01; //timestamp (32-bit)
	write(seq_fd, &byte_buf, LEN_BYTE_BUF);

	byte_buf[3] = 0x00; //output (32-bit)
	byte_buf[2] = 0x00; //output (32-bit)
	byte_buf[1] = 0x00; //output (32-bit)
	byte_buf[0] = 0x02; //output (32-bit)
	write(seq_fd, &byte_buf, LEN_BYTE_BUF);

	// TERMINATION
	byte_buf[3] = 0x01; //enable for DIO
	byte_buf[2] = 0x00;
	byte_buf[1] = 0x00; //address 0x03
	byte_buf[0] = 0x04; //address 0x03
	write(seq_fd, &byte_buf, LEN_BYTE_BUF);

	byte_buf[3] = 0x00; //timestamp (32-bit)
	byte_buf[2] = 0x00; //timestamp (32-bit)
	byte_buf[1] = 0x00; //timestamp (32-bit)
	byte_buf[0] = 0x00; //timestamp (32-bit)
	write(seq_fd, &byte_buf, LEN_BYTE_BUF);

	byte_buf[3] = 0x00; //output (32-bit)
	byte_buf[2] = 0x00; //output (32-bit)
	byte_buf[1] = 0x00; //output (32-bit)
	byte_buf[0] = 0x00; //output (32-bit)
	write(seq_fd, &byte_buf, LEN_BYTE_BUF);

	close(seq_fd);
}

void writeDIO(int sockfd, int numSnapshots)
{
	int seq_fd;
	char axis_fifo_path[] = "/dev/axis_fifo_0x0000000080004000";

	seq_fd = open(axis_fifo_path, O_RDWR);

	if (seq_fd == -1)
	{
		printf("Cannot open sequencer device");
		exit(1);
	}

	printf("sequencer device open\n");

	char *byte_buf = new char[LEN_BYTE_BUF * 3 * numSnapshots];

	readBuffer(sockfd, byte_buf, sizeof(byte_buf));

	for (int i = 0; i < LEN_BYTE_BUF * 3 * numSnapshots; ++i)
	{
		std::bitset<8> bitbuf(byte_buf[i]);
		std::cout << "char " << i << " = " << bitbuf << "\n";
	}

	int n;
	n = write(seq_fd, &byte_buf, LEN_BYTE_BUF * 3 * numSnapshots);
	printf("%u bytes written to sequencer.\n", n);

	close(seq_fd);
	printf("sequencer device closed\n");

	delete[] byte_buf;
}

std::string readBuffer(int socket, char* buffer, int bufferSize)
{
	int bytesRead = 0;

	printf("reading buffer... \n");
	while (bytesRead < bufferSize)
	{
		bytesRead += read(socket, buffer + bytesRead, bufferSize - bytesRead);
	}
	std::string buffer_str(buffer);
	return buffer_str;
}


// Function that reads commands from Chimera and passes them to the axis-fifo interface of the Zynq FPGA
int chimeraInterface(int sockfd)
{
	char buffer[MAX];
	char *pch;
	std::vector<char*> dev_pch;
	std::string buffer_str, dev;
	std::string delim = "_";
	std::stringstream sstr;
	int numSnapshots, len, connfd;
	struct sockaddr_in cli;

	for (;;)
	{
		// Now server is ready to listen and verification 
		if ((listen(sockfd, 5)) != 0) {
			printf("Listen failed...\n");
			exit(0);
		}
		else
			printf("Server listening..\n");
		len = sizeof(cli);

		// Accept the data packet from client and verification 
		connfd = accept(sockfd, (SA*)&cli, (socklen_t*)&len);
		if (connfd < 0) {
			printf("server acccept failed...\n");
			exit(0);
		}
		else
			printf("server acccepted the client...\n");

		bzero(buffer, MAX);
		//printf("buffer: %p\n", buffer);
		buffer_str = readBuffer(connfd, buffer, sizeof(buffer));
		std::cout << buffer_str << "\n";
		dev = buffer_str.substr(0, buffer_str.find("_"));
		std::cout << dev << "\n";
		numSnapshots = std::stoi(buffer_str.substr(buffer_str.find("_") + 1));
		std::cout << numSnapshots << "\n";

		if (dev == "DIO" && numSnapshots > 0)
		{
			writeDIO(connfd, numSnapshots);
		}
	}

	return connfd;
}

// Driver function 
int main()
{
	int sockfd, connfd, len;
	struct sockaddr_in servaddr, cli;

	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		printf("socket creation failed...\n");
		exit(0);
	}
	else
		printf("Socket successfully created..\n");
	bzero(&servaddr, sizeof(servaddr));

	// assign IP, PORT 
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(PORT);

	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
		printf("socket bind failed...\n");
		exit(0);
	}
	else
		printf("Socket successfully binded..\n");


	// Function for reading DIO, DAC, and DDS commands from Chimera
	connfd = chimeraInterface(sockfd);

	// After chatting close the socket 
	if (connfd != -1)
	{
		close(connfd);
	}

	return 0;
}
