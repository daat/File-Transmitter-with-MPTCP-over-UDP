#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>

#define CWND_THRESHOLD 2
#define TIMEOUT 3

struct sockaddr_in myAddress, destAddress;
struct sockaddr_in *agentAddress;
int agentNum;

struct tcp_packet
{
	unsigned char dest_address[4];
	int port;
	int seg_num; //also ack_num if ack is set
	int rwnd;
	int flag; //0: none, 1:ack, 2:syn, 3 ack/syn, 4: fin
	int len; //data length
	unsigned char data[1000];
};

unsigned char *serialize_tcp_packet(unsigned char *buffer, struct tcp_packet tp)
{
	unsigned char *ptr = buffer;
	ptr[0] = tp.dest_address[0];
	ptr[1] = tp.dest_address[1];
	ptr[2] = tp.dest_address[2];
	ptr[3] = tp.dest_address[3];
	ptr += 4;

	ptr = serialize_int(ptr, tp.port);
	ptr = serialize_int(ptr, tp.seq_num);
	ptr = serialize_int(ptr, tp.len);
	ptr = serialize_int(ptr, tp.rwnd);
	ptr = serialize_int(ptr, tp.flag);
}

int mpsocket(int port)
{
	int connfd;
	socklen_t length;
	connfd = socket(AF_INET, SOCK_DGRAM, 0);
	bzero(&myAddress, sizeof(myAddress));
	myAddress.sin_family = AF_INET;
	myAddress.sin_port = htons(port);
	myAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(connfd, (struct sockaddr *)&myAddress, sizeof(myAddress));
	/*handshake with agents TBD*/
	return connfd;
}


int mpsend(int connfd, int fd, string recv_address, int port)
{
	struct tcp_packet *dg;
	struct tcp_packet fb;

	unsigned char buffer[1024];
	unsigned char data[1000];
	memset(buffer, 0, 1024);
	memset(data, 0, 1000);
	int len, send_base = 1, next_seq_num = 1;
	int cwnd = 1, i;

	while()
	{
		dg = (struct tcp_packet *)calloc(cwnd, sizeof(struct tcp_packet));
		/*read from file and send data*/
		for(i = 0;i<cwnd, i++)
		{
			if((len = read(fd, data, 1000)) > 0)
			{
				memcpy(dg[i].dest_address, gethostbyname(recv_adress)->h_addr_list[0], 4);
				dg[i].port = port;
				dg[i].seq_num = next_seq_num;
				dg[i].flag = 0;
				dg[i].rwnd = 0;
				dg[i].len = len;
				memset(dg[i].data, 0, 1000);
				memcpy(dg[i].data, data, 1000);
				memset(data, 0, 1000);
				memset(buffer, 0, 1024);
				sendto(connfd, &dg[i], 1024, 0, agentAddress[0]);
				next_seq_num += len;
				if(i == 0)
				{
					alarm(TIMEOUT);
				}
			}
			else if(len == 0)break;
			else printf("wrong\n");
		}
		/*wait for acks*/
		while()
		{
			memset(fb.data, 0, 1000);
			if(recvfrom(connfd, &fb, 1024)<=0)break;
			
		}
		free(dg);
	}

}
