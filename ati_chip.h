#ifndef _ATI_CHIP_SORTED_H_
#define _ATI_CHIP_SORTED_H_
#include <pciaccess.h>
#include <string>
#include <vector>

enum class AtiChipFamily
{
	UNKNOWN = 0,
	#define PCI_CHIP_FAMILY(name) name,
	#define PCI_CHIP(family, sub, code)
	#include "ati_pciids_gen.def"
	#undef PCI_CHIP_FAMILY
	#undef PCI_CHIP
};

struct AtiDeviceData
{
	AtiChipFamily family;
	std::string vendorName;
	std::string deviceName;
	std::string busid;
	std::string devpath;
};

bool isAtiGPU(pci_device* device);
AtiChipFamily getAtiChipFamily(pci_device* device);
std::string getAtiChipFamilyString(pci_device* device);

std::vector<AtiDeviceData> getAllAtiDevices();


#endif
