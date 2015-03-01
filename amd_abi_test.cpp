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
	
	for (AMDABI::ScalarMemoryReadTuple r : abi.getABIIntro())
	{
		if (r.sizeInDWords == 1)
		{
			if (r.bufferResourceAtSregBase)
			{
				s_buffer_load_dword(p, r.sregBase/2, r.targetSreg, r.offset, 1);
			}
			else
			{
				s_load_dword(p, r.sregBase/2, r.targetSreg, r.offset, 1);
			}
			
			s_waitcnt(p);
		}
		else if (r.sizeInDWords == 4)
		{
			if (r.bufferResourceAtSregBase)
			{
				s_buffer_load_dwordx4(p, r.sregBase/2, r.targetSreg, r.offset, 1);
			}
			else
			{
				s_load_dwordx4(p, r.sregBase/2, r.targetSreg, r.offset, 1);
			}
			
			s_waitcnt(p);
		}
	}
		
	int array3Ptr =  abi.getFirstFreeSRegAfterABIIntro();
	
	{
		AMDABI::ScalarMemoryReadTuple rr = abi.getKernelArgument("array3");
		s_load_dword(p, rr.sregBase/2, array3Ptr, rr.offset, 1);
		s_waitcnt(p);
	}
	
	int localSize = array3Ptr+1;
	
	{
		AMDABI::ScalarMemoryReadTuple rr = abi.get_local_size(0);
		s_buffer_load_dword(p, rr.sregBase/2, localSize, rr.offset, 1);
		s_waitcnt(p);
	}
	
	int gidBase = localSize+1;
	
	s_mul_i32(p, gidBase, abi.get_group_id(0).sreg, localSize);
	v_add_i32(p, 4, abi.get_local_id(0).vreg, gidBase);
	v_mul_lo_i32(p, 4, 256 + 4, 129+3);
	v_add_i32(p, 4, 4, array3Ptr);
	
	v_mov_imm32(p, 5, 0xFFFF);
	mtbuf(p,
				4,//int nfmt,
				4,//int dfmt,
				TBUFFER_STORE_FORMAT_X,//int op,
				0,//int addr64,
				0,//int glc,
				0,//int idxen,
				1,//int offen,
				0,//int offset,
				128,//int soffset, set to zero
				0,//int tfe,
				0,//int slc,
				abi.getUAVBufresForKernelArgument("array3").sreg/4,//int srsrc,
				5,//int vdata,
				4//int vaddr
	);
	s_waitcnt(p);
	
	s_endpgm(p);
	
	abi.generateIntoDirectory(".", bytecode);
}
