
#include "compute_interface.hpp"
#include "computesi.h"
#include "code_helper.h"

#include <iostream>

class AMDBinaryOperationRunner
{
	unsigned mx,my;
	ComputeInterface compute;
public:
	AMDBinaryOperationRunner():compute("/dev/dri/card0"){}

template<typename T,typename C>
std::vector<T> testOperation(const std::vector<uint32_t>& binary, const std::vector<T>& input1, const std::vector<C>& input2)
{
	// create seq
	// generate code
	mx=1024;
	my=1;

	// temp
	int code_size_max = 1024*4;
	uint32_t* code = new uint32_t[code_size_max/4];
	generateTempCode(code, binary, sizeof(T));
	
	
	gpu_buffer* cpu_data1 	= compute.bufferAlloc(input1.size()*sizeof(T));
	gpu_buffer* cpu_data2 	= compute.bufferAlloc(input2.size()*sizeof(T));
	
	compute.transferToGPU(cpu_data1, 0, input1);
	compute.transferToGPU(cpu_data2, 0, input2);
	
	runCode(code, cpu_data1, cpu_data2, sizeof(T));
	
	std::vector<T> result(input2.size(),static_cast<T>( 0));
	compute.transferFromGPU(cpu_data2, 0, result.data(), result.size()*sizeof(T));
	
	return result;
}

template<typename T>
std::vector<T> testOperation(const std::vector<uint32_t>& binary, const std::vector<T>& input1, const std::vector<T>& input2)
{
	return testOperation<T,T>(binary, input1, input2);
}


void runCode(uint32_t* code, gpu_buffer* data1, gpu_buffer* data2, std::size_t typeSize);
/**
 * @brief Generates necessary code to be able to run the operations provided in binary.
 * 
 * @param code a simple pointer to the finaly binary code with necessary loads and stores and end program instruction
 * @param binary the actual little code to be embedded into code
 * @param typeSize is the data type size in bytes for the operations like 4 for in32, float32 and 8 for float64 and int64
 * @return void
 * 
 * The vector registera has the following layout in the setup code: v0: TID, V6:offset of the work-group, V7: global ID
 * So v2-v3 and v4-v5 can be used for double operands (64 data must be even alligned!).
 */
void generateTempCode(uint32_t* code, const std::vector<uint32_t>& binary, std::size_t typeSize);
};

void AMDBinaryOperationRunner::generateTempCode(uint32_t* code, const std::vector<uint32_t>& binary, std::size_t typeSize)
{
	mubuf_op LoadType;
	mubuf_op StoreType;
	
	if(typeSize == 1)
	{
		LoadType = BUFFER_LOAD_UBYTE;
		StoreType = BUFFER_STORE_BYTE;
	}
	else if(typeSize == 2)
	{
		LoadType = BUFFER_LOAD_USHORT;
		StoreType = BUFFER_STORE_SHORT;
	}
	else if(typeSize == 4)
	{
		LoadType = BUFFER_LOAD_DWORD;
		StoreType = BUFFER_STORE_DWORD;
	}
	else if(typeSize == 8)
	{
		LoadType = BUFFER_LOAD_DWORDX2;
		StoreType = BUFFER_STORE_DWORDX2;
	}
	else if(typeSize)
	{
		LoadType = BUFFER_LOAD_DWORDX4;
		StoreType = BUFFER_STORE_DWORDX4;
	}
	else
	{
		std::cout << "Invalid type size!" << std::endl;
	}
	
	std::cout << "typeSize: " << typeSize<< std::endl; 
	
	s_nop(code);
	s_nop(code);
	s_nop(code);
	s_nop(code);
	s_nop(code);

	const int SRSRC_SIZE = 8;
	v_mov_b32(code, 6, SRSRC_SIZE); //s4 V6 = S4 aka GROUP_ID
	v_mul_i32_i24(code, 6, 6, 255); code[0]=256; code++; // V6 = V6 * 256 aka GROUP_ID * GROUP_SIZE
	v_add_i32(code, 7/*cx*/, 0, 256+6); // V7=V0+V6 aka GLOBAL_ID
	//v_mul_i32_i24(code, 7, 7, 255); code[0]=typeSize; code++; //
	
	//unsigned *&p, int soffset, int tfe, int slc, int srsrc, int vdata, int vaddr, int op, int lds, int addr64, int glc, int idxen, int offen, int offset
	mubuf(code,
		128,//int soffset, set to zero
	   0,//int tfe,
	   0,//int slc,
	   0,//int srsrc,
	   2,//int vdata, V1 data is loaded here
	   7,//int vaddr
	   LoadType, // as required 8,16,32,64 or 128 bit; see above
	   0,// int lds , no local data store
	   0,//int addr64,
	   0,//int glc,
	   1,//int idxen,
	   0,//int offen,
	   0//int offset,
	);

	mubuf(code,
		128,//int soffset, set to zero
	   0,//int tfe,
	   0,//int slc,
	   1,//int srsrc,
	   4,//int vdata, V2 data is loaded here
	   7,//int vaddr
	   LoadType, // as required 8,16,32,64 or 128 bit; see above
	   0,// int lds , no local data store
	   0,//int addr64,
	   0,//int glc,
	   1,//int idxen,
	   0,//int offen,
	   0//int offset,
	);
	s_waitcnt(code);

	// Here is the actual code
	//v_add_i32(code,2,1,256+2);
	for(size_t i = 0; i < binary.size(); i++)
	{
		code[0] = binary[i];
		code++;
	}

	mubuf(code,
		128,//int soffset, set to zero
	   0,//int tfe,
	   0,//int slc,
	   1,//int srsrc,
	   4,//int vdata, result data is stored from here
	   7,//int vaddr
	   StoreType, // as required 8,16,32,64 or 128 bit; see above
	   0,// int lds , no local data store
	   0,//int addr64,
	   0,//int glc,
	   1,//int idxen,
	   0,//int offen,
	   0//int offset,
	);
	s_waitcnt(code);

	s_nop(code);
	s_nop(code);
	s_nop(code);
	s_nop(code);
	s_nop(code);
	s_endpgm(code);
}

int main()
{
	AMDBinaryOperationRunner opRunner;
	
	// float32
	std::vector<float> vec_input1(1024,1.1f);
	std::vector<float> vec_input2(1024,2.4f);
	std::vector<float> vec_result;
	std::vector<float> vec_resultRef(1024,1.1f/2.4f);
	
	std::vector<uint32_t> binaryOperation(2,0);
	uint32_t* p = &binaryOperation[0];
	
	//make the division
	v_rcp_f32(p,3,2);
	v_mul_f32(p,4,4,256+3);
	
	vec_result = opRunner.testOperation<float>(binaryOperation,vec_input1,vec_input2);
	
	return 0;
}