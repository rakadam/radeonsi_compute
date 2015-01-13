#include <va/va_dri2.h>
#include <drm.h>
#include <X11/Xlib.h>
#include <xf86drm.h>
#include <libudev.h>
#include <iostream>
#include <cstdio>

int main()
{
	udev* dev = udev_new();
	
	udev_enumerate* en = udev_enumerate_new(dev);
	
	udev_enumerate_add_match_subsystem(en, "drm");
// 	udev_enumerate_add_match_sysattr(en, "DEVTYPE", "drm_minor");
	udev_enumerate_scan_devices(en);
// 	udev_enumerate_scan_subsystems(en);
	udev_list_entry* first = udev_enumerate_get_list_entry(en);
	
	std::cout << (void*)first << std::endl;
	
	for (udev_list_entry* entry = first; entry; entry = udev_list_entry_get_next(entry))
	{
		std::cout << udev_list_entry_get_name(entry) << std::endl;
		
		const char* path = udev_list_entry_get_name(entry);
		udev_device* device = udev_device_new_from_syspath(dev, path);
		
		printf("Device Node Path: %s\n", udev_device_get_devnode(device));
		
		
		for (udev_list_entry* i = udev_device_get_sysattr_list_entry(device); i; i = udev_list_entry_get_next(i))
		{
			printf("%s : %s\n", udev_list_entry_get_name(i), udev_device_get_sysattr_value(device, udev_list_entry_get_name(i)));
		}
		
		for (udev_list_entry* i = udev_device_get_tags_list_entry(device); i; i = udev_list_entry_get_next(i))
		{
			printf("%s\n", udev_list_entry_get_name(i));
		}
		
	}
	
	std::cout << std::endl;
	udev_enumerate_unref(en);
	
	udev_unref(dev);
}