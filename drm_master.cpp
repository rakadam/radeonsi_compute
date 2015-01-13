#include <va/va_dri2.h>
#include <drm.h>
#include <X11/Xlib.h>
#include <xf86drm.h>
#include <libudev.h>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdio>
#include <cstdlib>
#include "ati_chip.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


int main()
{
	std::vector<AtiDeviceData> devices = getAllAtiDevices();
	
	for (AtiDeviceData devData : devices)
	{
		std::cout << devData.vendorName << " : " << devData.deviceName << " : " << devData.busid << " " << devData.devpath << std::endl;
	}
	
	std::map<std::string, int> drms;
	
	for (AtiDeviceData devData : devices)
	{
		if (devData.devpath.empty())
		{
			continue;
		}
		
		int ret;
		int fd = open(devData.devpath.c_str(), O_RDWR, 0);
		
		ret = drmSetMaster(fd);
		
		if (ret < 0)
		{
			std::cerr << "Failed to set Master on: " << devData.vendorName << " : " << devData.deviceName << " : " << devData.busid << " " << devData.devpath << std::endl;
			std::cerr << "Reason: " << strerror(errno) << std::endl;
		}
		
		drms[devData.busid] = fd;
	}
	
	for (auto p : drms)
	{
		close(p.second);
	}
	
	return 0;
	int sockfd,n;
	struct sockaddr_in servaddr,cliaddr;
	socklen_t len;
	char mesg[1000];
	
	sockfd=socket(AF_INET,SOCK_DGRAM, 0);
	
	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
	servaddr.sin_port=htons(37463);
	bind(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr));
	
	for (;;)
	{
		len = sizeof(cliaddr);
		n = recvfrom(sockfd,mesg,1000,0,(struct sockaddr *)&cliaddr,&len);
		sendto(sockfd,mesg,n,0,(struct sockaddr *)&cliaddr,sizeof(cliaddr));
		printf("-------------------------------------------------------\n");
		mesg[n] = 0;
		printf("Received the following:\n");
		printf("%s",mesg);
		printf("-------------------------------------------------------\n");
	}
}