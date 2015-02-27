#include <iostream>
#include "code_helper.h"
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
	
	unsigned int *p = bytecode.data();
	
	AMDABI::ScalarMemoryReadTuple rr = abi.getUAVBufresForKernelArgument("array3");
	
	int array3bufresBase = abi.getFirstFreeSRegAfterABIIntro();
	
	while (array3bufresBase % 4)
	{
		array3bufresBase++;
	}
	
	s_load_dwordx4(p, rr.sregBase/2, array3bufresBase, rr.offset, 1);
	s_waitcnt(p);
	mtbuf(p,
				4,//int nfmt,
				4,//int dfmt,
				TBUFFER_STORE_FORMAT_X,//int op,
				0,//int addr64,
				0,//int glc,
				0,//int idxen,
				0,//int offen,
				0,//int offset,
				128,//int soffset, set to zero
				0,//int tfe,
				0,//int slc,
				array3bufresBase/4,//int srsrc,
				0,//int vdata,
				0//int vaddr
	);
	s_waitcnt(p);
	
	
	s_endpgm(p);
	
	abi.generateIntoDirectory(".", bytecode);
}
