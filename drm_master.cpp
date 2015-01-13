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
#include <syslog.h>
#include <sstream>

int main()
{
	if (getuid() == 0)
	{
		system("modprobe radeon");
		sleep(1);
	}
	
	if (not drmAvailable())
	{
		std::cerr << "ERROR: DRM not available" << std::endl;
		return 1;
	}
	
	std::vector<AtiDeviceData> devices = getAllAtiDevices();
	
	std::cout << "Devices found: " << std::endl;
	
	for (AtiDeviceData devData : devices)
	{
		std::cout << devData.vendorName << " : " << devData.deviceName << " : " << devData.busid << " " << devData.devpath << std::endl;
	}
	
	std::cout << std::endl;
	std::cout << "entering daemon mode" << std::endl;
	
//	daemon(0, 0);
	
	openlog ("drm_master", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
	
	std::map<std::string, int> drms;
	
	for (AtiDeviceData devData : devices)
	{
		if (devData.devpath.empty())
		{
			continue;
		}
		
		int ret;
		int fd = open(devData.devpath.c_str(), O_RDWR, 0);
		
		drmSetBusid(fd, devData.busid.c_str());
		
		ret = drmSetMaster(fd);
		
		if (ret < 0)
		{
			std::stringstream ss;
			
			ss << "Failed to set Master on: " << devData.vendorName << " : " << devData.deviceName << " : " << devData.busid << " " << devData.devpath << " ";
			
			if (getuid() == 0)
			{
				ss << "drm already has a Master";
			}
			else
			{
				ss << "Reason: " << strerror(errno);
			}
			
			syslog(LOG_WARNING, ss.str().c_str());
		}
		else
		{
			std::stringstream ss;
			ss << "set Master on:" << devData.vendorName << " : " << devData.deviceName << " : " << devData.busid << " " << devData.devpath << " fd: " << fd;
			std::cerr << ss.str() << std::endl;
			syslog(LOG_INFO, ss.str().c_str());
			
			drms[devData.busid] = fd;
		}
	}
	
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
		
// 		std::cout << "recv: " << buf << std::endl;
		bool ok = false;
		std::stringstream ss;
		
		if (ret == 2)
		{
			if (drms.count(busid))
			{
				if (drmAuthMagic(drms.at(busid), magic))
				{
					ss << "drmAuthMagic failed: " << strerror(errno) << std::endl;
				}
				else
				{
					std::stringstream ss;
					ss << "Authenticated: " << busid << " with magic: " << magic;
					syslog(LOG_INFO, ss.str().c_str());
					ok = true;
					std::cout << ss.str() << std::endl;
				}
			}
			else
			{
				ss << "DRM not found for busid: " << busid;
			}
		}
		else
		{
			ss << "Error while parsing incoming data: " << buf;
		}
		
		if (not ss.str().empty())
		{
			std::cerr << ss.str() << std::endl;
			syslog(LOG_WARNING, ss.str().c_str());
		}
		
		const char* ans;
		
		if (ok)
		{
			ans = "ACK";
		}
		else
		{
			ans = "NACK";
		}
		
		sendto(sockfd, ans, strlen(ans), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
	}
}
