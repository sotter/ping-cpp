/*****************************************************************************
Name        : demo.cpp
Author      : sotter
Date        : 2015年7月20日
Description : 
******************************************************************************/

#include "ping.h"


int main()
{
	cppnetwork::Ping ping;

	//ping本机
	std::string result1 = ping.ping("127.0.0.1");
	printf("result1:%s\n", result1.c_str());
	//ping域名机器
	std::string result2 = ping.ping("www.baidu.com");
	printf("result2:%s\n", result2.c_str());
	//ping不存在的IP
	std::string result3 = ping.ping("198.168.1.1");
	printf("result3:%s\n", result3.c_str());

	return 0;
}



