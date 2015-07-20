/*****************************************************************************
 Name        : ping.cpp
 Author      : sotter
 Date        : 2015年6月23日
 Description : 
 ******************************************************************************/
#include <signal.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
//#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <setjmp.h>
#include <errno.h>
#include "my_icmp.h"
#include "ping.h"

namespace cppnetwork
{

#define SEND_LEN 64

static unsigned short get_check_sum(unsigned short *addr, int len);

static int pack_icmp(int pack_no, struct icmp* icmp);

static bool unpack_icmp(char *buf, int len, struct IcmpEchoReply *reply);

static struct timeval tv_sub(struct timeval &recv, struct timeval &send);

static std::string result2json(PingResult &result);

Ping::Ping()
{
	_max_packet_size = 1;
	_nsend = 0;
	_nrecv = 0;
	_icmp_seq = 0;
}

bool Ping::getsockaddr(const char * host, struct sockaddr_in* sockaddr)
{
	memset(static_cast<void *>(sockaddr), 0, sizeof(sockaddr));
	sockaddr->sin_family = AF_INET;

	bool rc = true;
	if (host == NULL || host[0] == '\0')
	{
		sockaddr->sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else
	{
		char c;
		const char *p = host;
		bool is_ipaddr = true;
		while ((c = (*p++)) != '\0')
		{
			if ((c != '.') && (!((c >= '0') && (c <= '9'))))
			{
				is_ipaddr = false;
				break;
			}
		}

		if (is_ipaddr)
		{
			sockaddr->sin_addr.s_addr = inet_addr(host);
		}
		else
		{
			struct hostent *hostname = gethostbyname(host);
			if (hostname != NULL)
			{
				memcpy(&(sockaddr->sin_addr), *(hostname->h_addr_list), sizeof(struct in_addr));
			}
			else
			{
				rc = false;
			}
		}
	}

	return rc;
}

/*发送三个ICMP报文*/
bool Ping::send_packet()
{
	size_t packetsize;
	while (_nsend < _max_packet_size)
	{
		_nsend++;
		_icmp_seq++;
		packetsize = pack_icmp(_icmp_seq, (struct icmp*) _send_packet); /*设置ICMP报头*/

		if (sendto(_sockfd, _send_packet, packetsize, 0, (struct sockaddr *) &_dest_addr, sizeof(_dest_addr)) < 0)
		{
			//perror("sendto error");
			continue;
		}
	}
	return true;
}

/*接收所有ICMP报文*/
bool Ping::recv_packet(PingResult &result)
{
	int len = 0;
	extern int errno;
	struct IcmpEchoReply echo_reply;
	int maxfds = _sockfd + 1;
	int nfd = 0;
	fd_set rset;
	FD_ZERO(&rset);

	socklen_t fromlen = sizeof(_from_addr);

	struct timeval timeout;
	timeout.tv_sec = 4;
	timeout.tv_usec = 0;

	for (int recv_cnt = 0; recv_cnt < _max_packet_size; recv_cnt++)
	{
		FD_SET(_sockfd, &rset);
		if ((nfd = select(maxfds, &rset, NULL, NULL, &timeout)) == -1)
		{
			//printf("select error \n");
			continue;
		}
		if (nfd == 0)
		{
			echo_reply.is_reply = false;
			result.error = "request timeout";
			result.icmp_echo_replys.push_back(echo_reply);
			continue;
		}
		if (FD_ISSET(_sockfd, &rset))
		{
			if ((len = (int)recvfrom(_sockfd, _recv_packet, sizeof(_recv_packet), 0, (struct sockaddr *) &_from_addr,
					&fromlen)) < 0)
			{
//				if (errno == EINTR)
//					continue;
				//perror("recvfrom error");
				continue;
			}

			echo_reply.from_addr = inet_ntoa(_from_addr.sin_addr);
			if (echo_reply.from_addr != result.ip)
			{
				//printf("invalid address, discard\n");
				recv_cnt--;
				continue;
			}
		}
		//todo: 注意修改的跟原来不一样了
		if (!unpack_icmp(_recv_packet, len, &echo_reply))
		{
			//retry again
			recv_cnt--;
			continue;
		}

		echo_reply.is_reply = true;
		result.icmp_echo_replys.push_back(echo_reply);
		_nrecv++;
	}

	return true;
}

std::string Ping::ping(std::string host)
{
	PingResult result;

	struct protoent *protocol;
	IcmpEchoReply echo_reply;

	_nsend = 0;
	_nrecv = 0;
	result.icmp_echo_replys.clear();
	_pid = getpid();

	result.data_len = SEND_LEN - 8;

	if ((protocol = getprotobyname("icmp")) == NULL)
	{
		//perror("getprotobyname");
		return result2json(result);
	}

#ifdef __APPLE__
	int type = SOCK_DGRAM;
#else
	int type = SOCK_RAW;
#endif

	/*生成使用ICMP的原始套接字,这种套接字只有root才能生成*/
	if ((_sockfd = socket(AF_INET, type, protocol->p_proto)) < 0)
	{
		result.error = strerror(errno);
		return result2json(result);
	}

	/*扩大套接字接收缓冲区到50K这样做主要为了减小接收缓冲区溢出的
	 的可能性,若无意中ping一个广播地址或多播地址,将会引来大量应答*/
	int size = 4 * 1024;
	setsockopt(_sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));

	/*获取main的进程id,用于设置ICMP的标志符*/
	if (!getsockaddr(host.c_str(), &_dest_addr))
	{
		result.error = "unknow host " + host;
		return result2json(result);
	}

	result.ip = inet_ntoa(_dest_addr.sin_addr);
	send_packet(); /*发送所有ICMP报文*/
	recv_packet(result); /*接收所有ICMP报文*/

	result.nsend = _nsend;
	result.nreceived = _nrecv;
	close(_sockfd);

	return result2json(result);
}


/*校验和算法*/
unsigned short get_check_sum(unsigned short *addr, int len)
{
	int left = len;
	int sum = 0;
	unsigned short *w = addr;
	unsigned short answer = 0;

	/*把ICMP报头二进制数据以2字节为单位累加起来*/
	while (left > 1)
	{
		sum += *w++;
		left -= 2;
	}

	/*若ICMP报头为奇数个字节，会剩下最后一字节。把最后一个字节视为一个2字节数据的高字节，这个2字节数据的低字节为0，继续累加*/
	if (left == 1)
	{
		*(unsigned char *) (&answer) = *(unsigned char *) w;
		sum += answer;
	}

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	answer = ~sum;

	return answer;
}

/*设置ICMP报头*/
int pack_icmp(int pack_no, struct icmp* icmp)
{
	int packsize;
	struct icmp *picmp;
	struct timeval *tval;

	picmp = icmp;
	picmp->icmp_type = ICMP_ECHO;
	picmp->icmp_code = 0;
	picmp->icmp_cksum = 0;
	picmp->icmp_seq = pack_no;
	picmp->icmp_id = getpid();
	packsize = SEND_LEN;
	tval = (struct timeval *) icmp->icmp_data;

	gettimeofday(tval, NULL); /*记录发送时间*/

	picmp->icmp_cksum = get_check_sum((unsigned short *) icmp, packsize);

	return packsize;
}

// 剥去ICMP报头
bool unpack_icmp(char *buf, int len, struct IcmpEchoReply *reply)
{
	int iphdrlen;
	struct ip *ip;
	struct icmp *icmp;
	struct timeval *tvsend, tvrecv, tvresult;
	double rtt;

	ip = (struct ip *) buf;
	//求ip报头长度,即ip报头的长度标志乘4
	iphdrlen = ip->ip_hl << 2;
	//越过ip报头,指向ICMP报头
	icmp = (struct icmp *) (buf + iphdrlen);
	//ICMP报头及ICMP数据报的总长度
	len -= iphdrlen;

	if (len < 8)
	{
		printf("ICMP packets\'s length is less than 8\n");
		return false;
	}

	// 确保所接收的是我所发的的ICMP的回应
	if (icmp->icmp_type == ICMP_ECHOREPLY /* && (icmp->icmp_id == _pid*/)
	{
		tvsend = (struct timeval *) icmp->icmp_data;
		gettimeofday(&tvrecv, NULL);
		tvresult = tv_sub(tvrecv, *tvsend);
		rtt = tvresult.tv_sec * 1000 + tvresult.tv_usec / 1000;
		reply->rtt = rtt;
		reply->icmpseq = icmp->icmp_seq;
		reply->ip_ttl = ip->ip_ttl;
		reply->icmp_len = len;
		return true;
	}
	else
	{
		return false;
	}
}

/*两个timeval结构相减*/
struct timeval tv_sub(struct timeval &recv, struct timeval &send)
{
	struct timeval result;

	result.tv_sec = recv.tv_sec - send.tv_sec;

	if(recv.tv_usec < send.tv_sec)
	{
		result.tv_sec --;
		result.tv_usec = recv.tv_usec + 1 * 1000 * 1000 - send.tv_usec;
	}
	else
	{
		result.tv_usec = recv.tv_usec - send.tv_usec;
	}

	return result;
}

std::string result2json(PingResult &result)
{
	char buffer[128] = { 0 };

	IcmpEchoReply reply;
	if (!result.icmp_echo_replys.empty())
	{
		reply = result.icmp_echo_replys[0];
	}

	snprintf(buffer, sizeof(buffer) - 1, "{\"result\":%d,\"ip\":\"%s\",\"recvbytes\":%d,\"ttl\":%d,\"time\":%.3f,\"error\":\"%s\"}",
					reply.is_reply ? 0 : 1,
					result.ip.c_str(),
					reply.icmp_len,
					reply.ip_ttl,
					reply.rtt,
					result.error.c_str());

	std::string str(buffer);
	return str;
}

} /* namespace cppnetwork */
