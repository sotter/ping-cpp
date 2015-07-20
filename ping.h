/*****************************************************************************
 Name        : ping.h
 Author      : sotter
 Date        : 2015年6月23日
 Description : 
 ******************************************************************************/

#ifndef LIBNETWORK_1_0_CPPNETWORK_PING_H_
#define LIBNETWORK_1_0_CPPNETWORK_PING_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>

namespace cppnetwork
{
#define PACKET_SIZE     4096
#define MAX_WAIT_TIME   5
#define MAX_NO_PACKETS  3

struct IcmpEchoReply
{
	int icmpseq;
	int icmp_len;
	int ip_ttl;
	double rtt;
	std::string from_addr;
	bool is_reply;

	IcmpEchoReply()
	{
		icmpseq = 0;
		icmp_len = 0;
		ip_ttl = 0;
		rtt = 0;
		is_reply = false;
	}
};

struct PingResult
{
	int data_len;
	int nsend;
	int nreceived;
	std::string ip;
	std::string error;
	std::vector<IcmpEchoReply> icmp_echo_replys;

	PingResult()
	{
		data_len = 0;
		nsend = 0;
		nreceived = 0;
	}
};

class Ping
{
public:

	Ping();

	std::string ping(std::string host);

private:

	bool getsockaddr(const char * host, sockaddr_in* sockaddr);

	bool send_packet();

	bool recv_packet(PingResult &result);

private:

	char _send_packet[PACKET_SIZE];

	char _recv_packet[PACKET_SIZE];

	int _max_packet_size;

	int _sockfd;

	int _nsend;

	int _nrecv;

	int _icmp_seq;

	struct sockaddr_in _dest_addr;

	struct sockaddr_in _from_addr;

	pid_t _pid;
};
} /* namespace cppnetwork */

#endif /* LIBNETWORK_1_0_CPPNETWORK_PING_H_ */
