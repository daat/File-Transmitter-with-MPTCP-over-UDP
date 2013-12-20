#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define LOSS_RATE 0.01

int main(int argc, char *argv[])
{
	int connfd, port, seq_num, flag;
	int sockaddr_len = sizeof(struct sockaddr);
	struct sockaddr_in myAddress, tAddress, sAddress;
	unsigned char buffer[1024], addr[4];
	double loss = 0.0, total = 0.0;
	int threshold = (int)(LOSS_RATE*100);

	connfd = socket(AF_INET, SOCK_DGRAM, 0);
	bzero(&myAddress, sizeof(myAddress));
	myAddress.sin_family = AF_INET;
	myAddress.sin_port = htons(atoi(argv[1]));
	myAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(connfd, (struct sockaddr *)&myAddress, sizeof(myAddress));

	memset(buffer, 0, 1024);

	tAddress.sin_family = AF_INET;

	srand((unsigned)time(NULL));

	while(recvfrom(connfd, buffer, 1024, 0, (struct sockaddr *)&sAddress, &sockaddr_len) > 0)
	{
		memcpy(addr, buffer, 4);
		port = buffer[4]<<24 | buffer[5]<<16 | buffer[6]<<8 | buffer[7];
		seq_num = buffer[8]<<24 | buffer[9]<<16 | buffer[10]<<8 | buffer[11];
		flag = buffer[20]<<24 | buffer[21]<<16 | buffer[22]<<8 | buffer[23];

		if(flag == 0)
		{
			total += 1.0;
			if((rand()%100) < threshold)
			{
				loss +=1.0;
				printf("get DATA #%d\n, loss rate: %lf\n", seq_num/1000+1, loss/total);
				printf("drop DATA #%d\n", seq_num/1000+1);
				continue;
			}
			printf("get DATA #%d\n, loss rate %lf\n", seq_num/1000+1, loss/total);
			printf("fwd DATA #%d\n", seq_num/1000+1);
		}
		else if(flag == 1)
		{
			printf("get ACK #%d\n", seq_num/1000);
			printf("fwd ACK #%d\n", seq_num/1000);
		}

		tAddress.sin_port = htons(port);
		tAddress.sin_addr = *(struct in_addr*)addr;
		memcpy(buffer, &(sAddress.sin_addr.s_addr), 4);
		port = ntohs(sAddress.sin_port);
		buffer[4] = port>>24;
		buffer[5] = port>>16;
		buffer[6] = port>>8;
		buffer[7] = port;
		sendto(connfd, buffer, 1024, 0, (struct sockaddr *)&tAddress, sizeof(struct sockaddr));
		memset(buffer, 0, 1024);
	}

	return 0;
}
