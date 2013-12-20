#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <netdb.h>
#include <signal.h>

#define INIT_CWND_THRESHOLD 32
#define TIMEOUT 3
#define RECV_BUFFER 64


struct sockaddr_in myAddress, destAddress;
struct sockaddr_in *agentAddress;
int agentNum, timeout = 0;

struct tcp_packet
{
	unsigned char dest_address[4];
	int port;
	int seq_num; //also ack_num if ack is set
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

	ptr = serialize_int(ptr, tp.port);
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

	ptr = deserialize_int(ptr, &(tp->port));
	ptr = deserialize_int(ptr, &(tp->seq_num));
	ptr = deserialize_int(ptr, &(tp->len));
	ptr = deserialize_int(ptr, &(tp->rwnd));
	ptr = deserialize_int(ptr, &(tp->flag));
	memcpy(tp->data, ptr, 1000);
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
	memcpy(dg->dest_address, recv_addr, 4);
	dg->port = port;
	dg->seq_num = seq_num;
	dg->flag = flag;
	dg->rwnd = rwnd;
	dg->len = len;
}

static void time_out(int signo)
{
	timeout = 1;
	printf("time out  ");
}

int mpsend(int connfd, int fd, char *recv_address, int port, char *fname)
{
	printf("mpsend\n");
	struct tcp_packet *dg;
	struct tcp_packet fb;

	unsigned char buffer[1024], buffer2[1024];
	memset(buffer, 0, 1024);
	int len, send_base = 1, next_seq_num = 1;
	int cwnd = 1, i, eof = 0, threshold = INIT_CWND_THRESHOLD;
	int rwnd = threshold*2;
	int prev_next_seq_num = 1;
	int sockaddr_len = sizeof(struct sockaddr);

	struct hostent *he;
	he = gethostbyname(recv_address);

	struct sigaction *act;
	act->sa_handler = time_out;
	act->sa_flags = 0 | SA_INTERRUPT;
	sigaction(SIGALRM, act, NULL);

	srand((unsigned)time(NULL));

	/*send SYN*/
	dg = (struct tcp_packet*)calloc(1, sizeof(struct tcp_packet));
	/*while(1)
	{*/
		make_header(&dg[0], (he->h_addr_list)[0], port, 0, 2, 0, 1);
		memset(dg[0].data, 0, 1000);
		memcpy(dg[0].data, fname, strlen(fname)+1);
		serialize_tcp_packet(buffer, dg[0]);

		sendto(connfd, buffer, 1024, 0, (struct sockaddr*)&agentAddress[rand()%agentNum], sizeof(struct sockaddr));
		/*alarm(3);
		memset(buffer2, 0, 1024);

		recvfrom(connfd, buffer2, 1024, 0, (struct sockaddr*)&destAddress, &sockaddr_len);

		if(timeout)
		{
			timeout = 0;
			continue;
		}
		memset(fb.data, 0, 1000);
		deserialize_tcp_packet(buffer2, &fb);
		if(fb.flag == 2) break;
		
	}*/
	alarm(0);
	free(dg);

	/*send*/
	while((!eof) || send_base != next_seq_num)
	{
		dg = (struct tcp_packet *)calloc(cwnd, sizeof(struct tcp_packet));
		/*read from file and send data*/
		lseek(fd, send_base-1, SEEK_SET);
		prev_next_seq_num = next_seq_num;
		next_seq_num = send_base;
		for(i = 0;i<cwnd; i++)
		{
			memset(dg[i].data, 0, 1000);
			if((len = read(fd, dg[i].data, 1000)) > 0)
			{
				make_header(&dg[i], (he->h_addr_list)[0], port, next_seq_num, 0, 0, len);
				memset(buffer, 0, 1024);
				serialize_tcp_packet(buffer, dg[i]);
				if(next_seq_num >= prev_next_seq_num)
					printf("send DATA #%d, winSize = %d\n", next_seq_num/1000+1, cwnd);
				else
					printf("resnd DATA #%d, winSize = %d\n", next_seq_num/1000+1, cwnd);

				sendto(connfd, buffer, 1024, 0, (struct sockaddr*)&agentAddress[rand()%agentNum], sizeof(struct sockaddr));
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
		while(send_base < next_seq_num)
		{
			memset(fb.data, 0, 1000);
			memset(buffer, 0, 1024);
			
			if(!timeout)
			{
				if((i = recvfrom(connfd, buffer, 1024, 0, (struct sockaddr*)&destAddress, &sockaddr_len))<=0)
				{
					if(!timeout)return 0;
				}

			}
			/*if time out, resend packet*/
			if(timeout)
			{
				if(cwnd > 1) threshold = cwnd/2;
				else threshold = 1;
				printf(", threshold = %d\n", threshold);
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
				printf("recv ACK #%d\n", fb.seq_num/1000);
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
	sendto(connfd, buffer, 1024, 0, (struct sockaddr*)&destAddress, sizeof(struct sockaddr));

	return 1;
}

struct tcp_packet *add_to_buffer(struct tcp_packet *head, struct tcp_packet *rp, int *window_len)
{
	struct tcp_packet *now = head, *prev = NULL;
	rp->next = NULL;
	if(now == NULL)
	{
		*window_len = *window_len+1;
		printf("recv DATA #%d\n", rp->seq_num/1000+1);
		return rp;
	}

	while(1)
	{
		if(now == NULL)
		{
			prev->next = rp;
			*window_len = *window_len + 1;
			printf("recv DATA #%d\n", rp->seq_num/1000+1);
			break;
		}
		if(now->seq_num == rp->seq_num)
		{
			printf("ignr DATA #%d\n", rp->seq_num/1000+1);
			break;
		}
		else if(now->seq_num < rp->seq_num)
		{
			prev = now;
			now = now->next;
		}
		else
		{
			printf("recv DATA #%d\n", rp->seq_num/1000+1);
			if(prev == NULL)
			{
				rp->next = now;
				*window_len = *window_len + 1;
				return rp;
			}
			else
			{
				prev->next = rp;
				rp->next = now;
				*window_len = *window_len + 1;
				break;
			}
		}
	}
	return head;
}

int next_expect_seq_num(struct tcp_packet *head, int num)
{
	struct tcp_packet *now = head;

	while(now != NULL)
	{
		if(now->seq_num == num)
		{
			return next_expect_seq_num(now->next, num + now->len);
		}
		else if(now->seq_num > num)
		{
			break;
		}
		else
		{
			now = now->next;
		}
	}
	return num;
}

void recv_buffer_flush(struct tcp_packet **head, int expect_seq_num, int fd, int *window_len)
{
	struct tcp_packet *now = *head, *tmp;
	printf("flush\n");
	while(1)
	{
		if(now == NULL)break;
		if(now->seq_num < expect_seq_num)
		{
			write(fd, now->data, now->len);
			*window_len = *window_len - 1;
			tmp = now;
			now = now->next;
			free(tmp);
		}
		else break;
	}
	*head = now;
}

int mprecv(int connfd, int fd)
{
	unsigned char buffer[1024];
	struct tcp_packet *rp, *fb;
	int window_len = 0, expect_seq_num = 1, t;
	int sockaddr_len = sizeof(struct sockaddr);
	struct tcp_packet *window_head = NULL;

	printf("mprecv\n");
	while(1)
	{
		memset(buffer, 0, 1024);
		if(t = (recvfrom(connfd, buffer, 1024, 0, (struct sockaddr*)&destAddress, &sockaddr_len)) <= 0)
		{
			printf("read over\n");
			return fd;
		}

		rp = (struct tcp_packet*)malloc(1024);
		deserialize_tcp_packet(buffer, rp);
		
		if(rp->flag == 0)//normal packet
		{
			if(window_len >= RECV_BUFFER)//buffer is full, flush
			{
				printf("drop DATA #%d\n", rp->seq_num/1000+1);
				recv_buffer_flush(&window_head, expect_seq_num, fd, &window_len);
				continue;
			}
			if(rp->seq_num == expect_seq_num)
			{
				expect_seq_num = next_expect_seq_num(window_head, expect_seq_num + rp->len);
				window_head = add_to_buffer(window_head, rp, &window_len);
			}
			else if(rp->seq_num > expect_seq_num)
			{
				window_head = add_to_buffer(window_head, rp, &window_len);
			}

			/*send ACK*/
			printf("send ACK #%d\n", expect_seq_num/1000);
			memset(buffer, 0, 1024);
			fb = (struct tcp_packet*)malloc(1024);
			memset(fb->data, 0, 1000);
			memcpy(fb->dest_address, rp->dest_address, 4);
			fb->port = rp->port;
			fb->len = 0;
			fb->flag = 1;
			fb->seq_num = expect_seq_num;
			fb->rwnd = RECV_BUFFER;
			serialize_tcp_packet(buffer, *fb);
			sendto(connfd, buffer, 1024, 0, (struct sockaddr*)&destAddress, sizeof(struct sockaddr));
			
			free(fb);
		}
		else if(rp->flag == 2)//SYN
		{
			int j;
			for(j = strlen(rp->data)-1; j >=0;j--)
			{
				if(rp->data[j] =='/')break;
			}
			fd = open(&(rp->data[j+1]), O_CREAT | O_WRONLY | O_EXCL, 0666);
			if(fd < 0)
			{
				fd = open(strcat(&(rp->data[j+1]), "(1)"), O_CREAT | O_WRONLY | O_TRUNC, 0666);
			}
			rp->flag = 2; //set SYN flag
			rp->len = 0;
			memset(rp->data, 0, 1000);
			serialize_tcp_packet(buffer, *rp);
			sendto(connfd, buffer, 1024, 0, (struct sockaddr*)&destAddress, sizeof(struct sockaddr));
		}
		else if(rp->flag == 4)//FIN
		{
			recv_buffer_flush(&window_head, expect_seq_num, fd, &window_len);
			rp->flag = 4; //set FIN flag
			rp->len = 0;
			memset(rp->data, 0, 1000);
			serialize_tcp_packet(buffer, *rp);
			sendto(connfd, buffer, 1024, 0, (struct sockaddr*)&destAddress, sizeof(struct sockaddr));

			free(rp);
			printf("FIN\n");
			break;
		}


	}
	recv_buffer_flush(&window_head, expect_seq_num, fd, &window_len);

	return fd;
}

int main(int argc, char *argv[])
{
	int connfd, fd, i, aport;
	struct hostent *he;
	char addr[128];
	if(strcmp(argv[1], "s")==0)
	{
		connfd = mpsocket(5123);
		if((fd = open(argv[2], O_RDONLY)) < 0)
		{
			printf("wrong file\n");
			return 0;
		}
		bzero(&destAddress, sizeof(destAddress));
		he = gethostbyname(argv[3]);
		destAddress.sin_family = AF_INET;
		destAddress.sin_port = htons(atoi(argv[4]));
		destAddress.sin_addr = *(struct in_addr*)((he->h_addr_list)[0]);
		printf("Num of agents: ");
		scanf("%d", &agentNum);
		if(agentNum <= 0)return 0;
		agentAddress = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in)*agentNum);
		for(i=0;i<agentNum;i++)
		{
			memset(addr, 0, 128);
			printf("agent #%d address: ", i);
			scanf("%s", addr);
			printf("agent #%d port: ", i);
			scanf("%d", &aport);
			agentAddress[i].sin_family = AF_INET;
			agentAddress[i].sin_port = htons(aport);
			agentAddress[i].sin_addr = *(struct in_addr*)((he->h_addr_list)[0]);
		}

		mpsend(connfd, fd, argv[3], atoi(argv[4]), argv[2]);
	}
	else if(strcmp(argv[1], "r") == 0)
	{
		fd = open("recvfile", O_WRONLY, O_CREAT, O_TRUNC, 0666);
		connfd = mpsocket(5124);
		if(mprecv(connfd, fd) != fd)unlink("recvfile");
	}
	close(connfd);
	close(fd);
	return 0;
}
