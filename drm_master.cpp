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
	
	std::cout << "Devices found: " << std::endl;
	
	for (AtiDeviceData devData : devices)
	{
		std::cout << devData.vendorName << " : " << devData.deviceName << " : " << devData.busid << " " << devData.devpath << std::endl;
	}
	
	std::cout << std::endl;
	
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
			
			if (getuid() == 0)
			{
				std::cerr << "drm already has a Master" << std::endl;
			}
			else
			{
				std::cerr << "Reason: " << strerror(errno) << std::endl;
			}
		}
		else
		{
			drms[devData.busid] = fd;
		}
	}
	
// 	for (auto p : drms)
// 	{
// 		close(p.second);
// 	}
	
	int sockfd,n;
	struct sockaddr_in servaddr,cliaddr;
	socklen_t len;
	
	sockfd=socket(AF_INET,SOCK_DGRAM, 0);
	
	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
	servaddr.sin_port=htons(DRM_LOCAL_MASTER_UDP_PORT);
	bind(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr));
	
	while(true)
	{
		char buf[1024];
		len = sizeof(cliaddr);
		n = recvfrom(sockfd, buf, sizeof(buf)-1, 0, (struct sockaddr *)&cliaddr, &len);
		buf[n] = 0;
		char busid[1025];
		long int magic = 0;
		int ret = sscanf(buf, "%s %li", busid, &magic);
		std::cout << "recv: " << buf << std::endl;
		if (ret == 2)
		{
			if (drms.count(busid))
			{
				drmAuthMagic(drms.at(busid), magic);
			}
			else
			{
				std::cerr << "DRM not found for busid: " << busid << std::endl;
			}
		}
		else
		{
			std::cerr << "Error while parsing incoming data: " << buf << std::endl;
		}
// 		sendto(sockfd,mesg,n,0,(struct sockaddr *)&cliaddr,sizeof(cliaddr));
	}
}