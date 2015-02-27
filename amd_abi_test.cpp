#include <iostream>
#include "amd_abi.h"

int main()
{
	AMDABI abi("testkernel");
	
	abi.addKernelArgument("array", "const unsigned int*");
	abi.addKernelArgument("array2", "const unsigned int*");
	abi.addKernelArgument("array3", "unsigned int*");
	abi.setDimension(1);
	abi.setLocalMemorySize(1024);
	abi.setRegUse(64, 250);
	abi.setPrivateMemorySizePerItem(8000);
	
	abi.buildInternalData();
	
// 	std::cout << abi.makeInnerMetaData() << std::endl;
// 	std::cout << abi.makeMetaData() << std::endl;
	
	std::vector<uint32_t> bytecode;
	
	for (int i = 0; i < 1024; i++)
	{
		bytecode.push_back(0xBF800000 | (0x1 << 16));
	}
	
	abi.generateIntoDirectory(".", bytecode);
}
