#include <iostream>
#include "amd_abi.h"

int main()
{
	AMDABI abi("testkernel");
	
	abi.addKernelArgument("array", "const int*");
	abi.addKernelArgument("array2", "const int*");
	abi.addKernelArgument("array3", "int*");
	abi.setDimension(1);
	abi.setLocalMemorySize(1024);
	abi.setRegUse(64, 256);
	abi.setPrivateMemorySizePerItem(1024);
	
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
