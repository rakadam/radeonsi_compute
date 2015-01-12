#include "ati_chip.h"
#include <cstdio>
#include <assert.h>
#include <mutex>
#include <iostream>
#include <errno.h>
#include <string.h>

#define PCI_VENDOR_ATI 0x1002

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
			
			if (int(devData.family))
			{
				devices.push_back(devData);
			}
		}
	}
	
	pci_system_cleanup();
	
	return devices;
}
