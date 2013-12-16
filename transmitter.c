#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>

#define INIT_CWND_THRESHOLD 2
#define TIMEOUT 3
#define RECV_BUFFER 16

/***************************
*TBD: 
*agent handshaking
*congestion control
*add_to_buffer, next_expect_seq_num, recv_buffer_flush
*agents
****************************/

struct sockaddr_in myAddress, destAddress;
struct sockaddr_in *agentAddress;
int agentNum, timeout = 0;

struct tcp_packet
{
	unsigned char dest_address[4];
	int port;
	int seg_num; //also ack_num if ack is set
	int len; //data length
	int rwnd;
	int flag; //0: none, 1:ack, 2:syn, 3 ack/syn, 4: fin
	unsigned char data[1000];

	struct tcp_packet *next;
};

unsigned char *deserialize_int(unsigned char *ptr, int *value)
{
	*value =  (ptr[0]<<24) | (ptr[1]<<16) | (ptr[2]<<8) | ptr[3];
	return ptr+4;
}

unsigned char *serialize_int(unsigned char *ptr, int value)
{
	ptr[0] = value>>24;
	ptr[1] = value>>16;
	ptr[2] = value>>8;
	ptr[3] = value;
	return ptr+4;
}

unsigned char *serialize_tcp_packet(unsigned char *buffer, struct tcp_packet tp)
{
	unsigned char *ptr = buffer;
	ptr[0] = tp.dest_address[0];
	ptr[1] = tp.dest_address[1];
	ptr[2] = tp.dest_address[2];
	ptr[3] = tp.dest_address[3];
	ptr += 4;

	ptr = serialize_int(ptr, tp.port:;
	ptr = serialize_int(ptr, tp.seq_num);
	ptr = serialize_int(ptr, tp.len);
	ptr = serialize_int(ptr, tp.rwnd);
	ptr = serialize_int(ptr, tp.flag);
	memcpy(ptr, tp.data, 1000);
	return buffer;
}

unsigned char *deserialize_tcp_packet(unsigned char *buffer, struct tcp_packet *tp)
{
	unsigned char *ptr = buffer;
	tp->dest_address[0] = ptr[0];
	tp->dest_address[1] = ptr[1];
	tp->dest_address[2] = ptr[2];
	tp->dest_address[3] = ptr[3];
	ptr += 4;

	ptr = deserialize_int(ptr, &(tp->port))
	ptr = deserialize_int(ptr, &(tp->seq_num))
	ptr = deserialize_int(ptr, &(tp->len))
	ptr = deserialize_int(ptr, &(tp->rwnd))
	ptr = deserialize_int(ptr, &(tp->flag))
	return buffer;
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
void make_header(struct tcp_packet *dg, char *recv_addr, int port, int seq_num, int flag, int rwnd, int len)
{
	memcpy(dg[i].dest_address, recv_addr, 4);
	dg[i].port = port;
	dg[i].seq_num = seq_num;
	dg[i].flag = flag;
	dg[i].rwnd = rwnd;
	dg[i].len = len;
}



int mpsend(int connfd, int fd, string recv_address, int port)
{
	struct tcp_packet *dg;
	struct tcp_packet fb;

	unsigned char buffer[1024];
	memset(buffer, 0, 1024);
	int len, send_base = 1, next_seq_num = 1;
	int cwnd = 1, i, eof = 0, threshold = INIT_CWND_THRESHOLD;
	int rwnd = threshold*2;
	/*TBD send syn*/
	while(!eof)
	{
		dg = (struct tcp_packet *)calloc(cwnd, sizeof(struct tcp_packet));
		/*read from file and send data*/
		lseek(fd, send_base-1, SEEK_SET);
		for(i = 0;i<cwnd; i++)
		{
			memset(dg[i].data, 0, 1000);
			if((len = read(fd, dg[i].data, 1000)) > 0)
			{
				make_header(&dg[i], gethostbyname(recv_adress)->h_addr_list[0], port, next_seq_num, 0, 0, len);
				memset(buffer, 0, 1024);
				serialize_tcp_packet(buffer, dg[i]);
				sendto(connfd, buffer, 1024, 0, (struct sockaddr*)&agentAddress[0], sizeof(struct sockaddr));
				next_seq_num += len;
				if(i == 0)
				{
					alarm(TIMEOUT);
				}
			}
			else if(len == 0)
			{
				eof = 1;
				break;
			}
			else 
			{
				printf("read file wrong\n");
				eof = 1;

				return 0;
			}
		}

		/*set cwnd*/
		if(cwnd < threshold) cwnd *= 2;
		else cwnd++;
		if(cwnd > rwnd) cwnd = rwnd;

		/*wait for acks*/
		int sockaddr_len = sizeof(struct sockaddr);
		while(send_base < next_seq_num)
		{
			memset(fb.data, 0, 1000);
			memset(buffer, 0, 1024);
			
			if(!timeout)
			{
				if(recvfrom(connfd, buffer, 1024, 0, (struct sockaddr*)&destAddress, &sockaddr_len)<=0)return 0;
			}
			/*if time out, resend packet*/
			if(timeout)
			{
				if(threshold > 1) threshold /= 2;
				cwnd = 1;
				timeout = 0;
				break;

				/*for(i = 0;i<cwnd;i++)
				{
					if(dg[i].seq_num >= send_base)
					{
						memset(buffer, 0, 1024);
						serialize_tcp_packet(buffer, dg[i]);
						sendto(connfd, buffer, 1024, 0, (struct sockaddt*)&agentAddress[0], sizeof(struct sockaddr));
						alarm(TIMEOUT);
						break;
					}
				}*/
					
			}

			deserialize_tcp_packet(buffer, &fb);
			if(fb.flag == 1)
			{
				if(fb.seq_num > send_base)
				{
					send_base = fb.seq_num;
					rwnd = fb.rwnd;
					alarm(TIMEOUT);
				}
			}
		}
		alarm(0);
		free(dg);
	}

	/*send FIN*/
	fb.flag = 4;
	fb.len = 0;
	memset(fb.data, 0, 1000);
	serialize_tcp_packet(buffer, fb);
	sendto(connfd, buffer, 1024, 0, (struct sockaddt*)&agentAddress[0], sizeof(struct sockaddr));

	return 1;
}

int mprecv(int connfd, int fd)
{
	unsigned char buffer[1024];
	struct tcp_packet *rp;
	int window_len = 0, expect_seq_num = 1;
	int sockaddr_len = sizeof(struct sockaddr);
	struct tcp_packet *window_head = NULL;

	while(1)
	{
		memset(buffer, 0, 1024);
		if((recvfrom(connfd, buffer, 1024, 0, (struct sockaddr*)&destAddress, &sockaddr_len)) <= 0)return 0;

		rp = (struct tcp_packet*)malloc(1024);
		deserialize_tcp_packet(buffer, rp);
		
		if(rp->flag == 0)//normal packet
		{
			if(window_len >= RECV_BUFFER)//buffer is full, flush
			{
				recv_buffer_flush(&window_head, fd);
				continue;
			}
			if(rp->seq_num == expect_seq_num)
			{
				expect_seq_num = next_expect_seq_num(expect_seq_num + rp->len);
				add_to_buffer(&window_head, rp);
			}
			else if(rp->seq_num > expect_seq_num)
			{
				add_to_buffer(&window_head, rp);
			}

			/*send ACK*/
			memset(buffer, 0, 1024);
			memset(rp->data, 0, 1000);
			rp->len = 0;
			rp->flag = 1;
			rp->seq_num = expect_seq_num;
			rp->rwnd = RECV_BUFFER;
			serialize_tcp_packet(buffer, *rp);
			sendto(connfd, buffer, 1024, 0, (struct sockaddr*)&destAddress, sizeof(struct sockaddr));

		}
		else if(rp->flag == 2)//SYN
		{
		}
		else if(rp->flag == 4)//FIN
		{
			recv_buffer_flush(&window_head, fd);
			rp->flag = 4; //set FIN flag
			rp->len = 0;
			memset(rp->data, 0, 1000);
			serialize_tcp_packet(buffer, *rp);
			sendto(connfd, buffer, 1024, 0, (struct sockaddr*)&destAddress, sizeof(struct sockaddr));

			free(rp);
			break;
		}

		free(rp);

	}

	return 1;
}

int main(int argc, char *argv[])
{
	close(connfd);
	close(fd);
	return 0;
}
