#include "ati_chip.h"
#include <cstdio>
#include <assert.h>
#include <mutex>
#include <iostream>
#include <errno.h>
#include <string.h>
#include <libudev.h>
#include <map>
#include <drm.h>
#include <xf86drm.h>
#include <X11/Xlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define PCI_VENDOR_ATI 0x1002

std::map<std::string, std::string> getBusidLookup()
{
	std::map<std::string, std::string> result;
	
	udev* dev = udev_new();
	
	udev_enumerate* en = udev_enumerate_new(dev);
	
	udev_enumerate_add_match_subsystem(en, "drm");
	udev_enumerate_add_match_sysname(en, "card*");
	udev_enumerate_scan_devices(en);
	udev_list_entry* first = udev_enumerate_get_list_entry(en);
	
	for (udev_list_entry* entry = first; entry; entry = udev_list_entry_get_next(entry))
	{
		const char* path = udev_list_entry_get_name(entry);
		udev_device* device = udev_device_new_from_syspath(dev, path);
		
		const char* type = udev_device_get_devtype(device);
		const char* devnode = udev_device_get_devnode(device);
		const char* psysname = udev_device_get_sysname(udev_device_get_parent(device));
		
		if (type and strcmp(type, "drm_minor") == 0 and devnode and psysname)
		{
// 			printf("Device Node Path: %s\n", udev_device_get_devnode(device));
// 			printf("Device type: %s\n", udev_device_get_devtype(device));
// 			printf("Device driver: %s\n", udev_device_get_driver(device));
// 			printf("Device sysname: %s\n", udev_device_get_sysname(device));
// 			printf("Device parent sysname: %s\n", udev_device_get_sysname(udev_device_get_parent(device)));
			result[psysname] = devnode;
		}
		
		udev_device_unref(device);
	}
	
	udev_enumerate_unref(en);
	udev_unref(dev);
	
	return result;
}

bool isAtiGPU(pci_device* device)
{
	#define PCI_VENDOR_ATI 0x1002
	return device->vendor_id == PCI_VENDOR_ATI;
}

AtiChipFamily getAtiChipFamily(pci_device* device)
{
	assert(device);
	assert(isAtiGPU(device));
	
	#define PCI_CHIP_FAMILY(name)
	#define PCI_CHIP(family, sub, code) if (code == device->device_id) return AtiChipFamily:: family;
	#include "ati_pciids_gen.def"
	#undef PCI_CHIP_FAMILY
	#undef PCI_CHIP
	
	return AtiChipFamily::UNKNOWN;
}

std::string getAtiChipFamilyString(pci_device* device)
{
	assert(device);
	assert(isAtiGPU(device));
	
	#define PCI_CHIP_FAMILY(name)
	#define PCI_CHIP(family, sub, code) if (code == device->device_id) return #family ;
	#include "ati_pciids_gen.def"
	#undef PCI_CHIP_FAMILY
	#undef PCI_CHIP
	
	return "";
}


static std::mutex pci_mutex;

std::vector< AtiDeviceData > getAllAtiDevices()
{
	std::map<std::string, std::string> busidTable = getBusidLookup();
	
	std::lock_guard<std::mutex> lock(pci_mutex);
	std::vector<AtiDeviceData> devices;
	
	pci_system_init();
	
	#define PCI_VENDOR_ATI 0x1002
	struct pci_device *device = NULL;
	struct pci_device_iterator *device_iter;
	
	device_iter = pci_slot_match_iterator_create(NULL);
	while ((device = pci_device_next(device_iter)) != NULL)
	{
		if (device->vendor_id == PCI_VENDOR_ATI)
		{
			pci_device_probe(device);
// 			std::cout << pci_device_get_vendor_name(device) << " : " << pci_device_get_device_name(device) << " : " << device->device_id  << " " << getAtiChipFamilyString(device) << std::endl;
// 			printf("%x\n", device->device_id);
			
// 			device->
			AtiDeviceData devData;
			
			devData.family = getAtiChipFamily(device);
			devData.vendorName = pci_device_get_vendor_name(device);
			
			if (pci_device_get_subdevice_name(device))
			{
				devData.deviceName = pci_device_get_subdevice_name(device);
			}
			else if (pci_device_get_device_name(device))
			{
				devData.deviceName = pci_device_get_device_name(device);
			}
			
			char buf[1024];
			sprintf(buf, "pci:%04x:%02x:%02x.%d", device->domain, device->bus, device->dev, device->func);
			devData.busid = buf;
			
			for (auto p: busidTable)
			{
				int domain = 0;
				int bus = 0;
				int dev = 0;
				int func = 0;
				int ret = 0;
				
				//std::cout << p.first << " " << p.second << std::endl;
				ret = sscanf(p.first.c_str(), "%x:%x:%x.%d", &domain, &bus, &dev, &func);
				assert(ret == 3 or ret == 4);
				
				if (ret == 3)
				{
					domain = 0;
					ret = sscanf(p.first.c_str(), "%d:%d:%d", &bus, &dev, &func);
					assert(ret == 3);
				}
				
				if (domain == device->domain and bus == device->bus and dev == device->dev and func == device->func)
				{
					devData.devpath = p.second;
					break;
				}
			}
			
			if (int(devData.family) and not devData.devpath.empty())
			{
				devices.push_back(devData);
			}
		}
	}
	
	pci_system_cleanup();
	
	return devices;
}

void authWithLocalMaster(long int magic, const char* busid)
{
	int sockfd,n;
	struct sockaddr_in servaddr,cliaddr;
	char sendline[1000];
	char recvline[1000];
	
	sockfd=socket(AF_INET,SOCK_DGRAM,0);
	
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 100000;
	
	assert(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv,sizeof(tv)) >= 0);
	
	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
	servaddr.sin_port=htons(DRM_LOCAL_MASTER_UDP_PORT);
	
	char buf[1024];
	
	snprintf(buf, sizeof(buf), "%s %li", busid, magic);
	int ret = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&servaddr,sizeof(servaddr));
	
	if (ret < 0)
	{
		return;
	}
	
	socklen_t len = sizeof(servaddr);
	ret = recvfrom(sockfd, buf, 1023, 0, (struct sockaddr *)&servaddr, &len);
	
	close(sockfd);
}
