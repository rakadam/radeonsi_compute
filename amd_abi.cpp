#include <stdexcept>
#include "amd_abi.h"

const int firstUsableUAV = 12;

AMDABI::AMDABI(std::string kernelName) : kernelName(kernelName)
{
	dim = 0;
	privateMemSize = 0;
	localMemSize = 0;
}

void AMDABI::setDimension(int dim)
{
 this->dim = dim;
}

void AMDABI::setPrivateMemorySizePerItem(int sizeInBytes)
{
	if (sizeInBytes % 32 != 0)
	{
		sizeInBytes += (32 - sizeInBytes % 32);
	}
	
	privateMemSize = sizeInBytes;
}

void AMDABI::setLocalMemorySize(int sizeInBytes)
{
	if (sizeInBytes % 256 != 0)
	{
		sizeInBytes += (256 - sizeInBytes % 256);
	}
	
	localMemSize = sizeInBytes;
}

void AMDABI::addKernelArgument(std::string name, std::string ctypeName, int customSizeInBytes)
{
	KernelArgument arg(name, ctypeName);
	
	if (customSizeInBytes > -1)
	{
		arg.sizeInBytes = customSizeInBytes;
	}
	
	KernelArgumentNameToIndex[name] = kernelArguments.size();
	kernelArguments.push_back(arg);
}

void AMDABI::addKernelArgumentLocalMemory(std::string name, std::string ctypeName)
{
	KernelArgument arg(name, ctypeName);
	
	arg.isPointer = true;
	arg.localPointer = true;
	KernelArgumentNameToIndex[name] = kernelArguments.size();
	kernelArguments.push_back(arg);
}

void AMDABI::computeKernelArgumentTableLayout()
{
	int tableSize = 0;
	
	for (KernelArgument& argument : kernelArguments)
	{
		argument.startOffsetInArgTable = tableSize;
		tableSize += argument.getSizeInArgumentTable();
	}
}

void AMDABI::numberUAVs()
{
	int UAVid = firstUsableUAV;
	
	for (KernelArgument& argument : kernelArguments)
	{
		if (argument.isPointer and not argument.localPointer)
		{
			argument.usedUAV = UAVid;
			UAVid++;
		}
	}
}

void AMDABI::allocateUserElements()
{
	if (privateMemSize > 0)
	{
		//private memory, spill space, raw pointer to the bufres
		UserElementDescriptor elem;
		
		elem.dataClass = 24;
		elem.apiSlot = 0;
		elem.regCount = 2;
		
		userElementNameToIndex["privateMemoryBufresPtr"] = userElementTable.size();
		userElementTable.push_back(elem);
	}
	
	{
		//raw pointer to the UAV table
		UserElementDescriptor elem;
		elem.dataClass = 23;
		elem.apiSlot = 0;
		elem.regCount = 2;
		
		userElementNameToIndex["UAVTablePtr"] = userElementTable.size();
		userElementTable.push_back(elem);
	}
	
	{
		//kernel size table buffer resource descriptor
		UserElementDescriptor elem;
		elem.dataClass = 2;
		elem.apiSlot = 0;
		elem.regCount = 4;
		
		userElementNameToIndex["kernelSizeTableBufres"] = userElementTable.size();
		userElementTable.push_back(elem);
	}
	
	{
		//kernel argument table buffer resource descriptor
		UserElementDescriptor elem;
		elem.dataClass = 2;
		elem.apiSlot = 1;
		elem.regCount = 4;
		
		userElementNameToIndex["kernelArgumentTableBufres"] = userElementTable.size();
		userElementTable.push_back(elem);
	}
	
	int index = 0;
	
	for (UserElementDescriptor& elem : userElementTable)
	{
		elem.startSReg = index;
		index += elem.regCount;
	}
}

AMDABI::ScalarMemoryReadTuple AMDABI::get_local_size(int dim) const
{
	AMDABI::ScalarMemoryReadTuple result;
	
	result.sregBase = userElementTable.at(userElementNameToIndex.at("kernelSizeTableBufres")).startSReg;
	result.bufferResourceAtSregBase = true;
	result.offset = 0x04 + dim;
	result.sizeInDWords = 1;
	result.targetSreg = -1;
	
	return result;
}

AMDABI::ScalarMemoryReadTuple AMDABI::get_global_size(int dim) const
{
	AMDABI::ScalarMemoryReadTuple result;
	
	result.sregBase = userElementTable.at(userElementNameToIndex.at("kernelSizeTableBufres")).startSReg;
	result.bufferResourceAtSregBase = true;
	result.offset = 0x00 + dim;
	result.sizeInDWords = 1;
	result.targetSreg = -1;
	
	return result;
}

AMDABI::ScalarMemoryReadTuple AMDABI::get_num_groups(int dim) const
{
	AMDABI::ScalarMemoryReadTuple result;
	
	result.sregBase = userElementTable.at(userElementNameToIndex.at("kernelSizeTableBufres")).startSReg;
	result.bufferResourceAtSregBase = true;
	result.offset = 0x08 + dim;
	result.sizeInDWords = 1;
	result.targetSreg = -1;
	
	return result;
}

AMDABI::ScalarMemoryReadTuple AMDABI::get_global_offset(int dim) const
{
	AMDABI::ScalarMemoryReadTuple result;
	
	result.sregBase = userElementTable.at(userElementNameToIndex.at("kernelSizeTableBufres")).startSReg;
	result.bufferResourceAtSregBase = true;
	result.offset = 0x18 + dim;
	result.sizeInDWords = 1;
	result.targetSreg = -1;
	
	return result;
}

AMDABI::VectorRegister AMDABI::get_local_id(int dim) const
{
	VectorRegister reg;
	
	reg.vreg = dim;
	reg.sizeInDWords = 1;
	
	return reg;
}

AMDABI::ScalarRegister AMDABI::get_group_id(int dim) const
{
	ScalarRegister reg;
	int base = userElementTable.back().startSReg + userElementTable.back().regCount;
	
	if (privateMemSize > 0)
	{
		base++;
	}
	
	reg.sreg = base + dim;
	reg.sizeInDWords = 1;
	
	return reg;
}

AMDABI::ScalarMemoryReadTuple AMDABI::getKernelArgument(std::string name) const
{
	return getKernelArgument(KernelArgumentNameToIndex.at(name));
}

AMDABI::ScalarMemoryReadTuple AMDABI::getKernelArgument(int index) const
{
	ScalarMemoryReadTuple result;
	KernelArgument arg = kernelArguments.at(index);
	
	result.sregBase = userElementTable.at(userElementNameToIndex.at("kernelArgumentTableBufres")).startSReg;
	result.offset = arg.startOffsetInArgTable;
	result.sizeInDWords = (arg.sizeInBytes+3) / 4;
	result.bufferResourceAtSregBase = true;
	result.targetSreg = -1;
	
	return result;
}


AMDABI::ScalarMemoryReadTuple::ScalarMemoryReadTuple() : targetSreg(-1), sregBase(-1), bufferResourceAtSregBase(false), offset(0), sizeInDWords(-1)
{
}


AMDABI::KernelArgument::KernelArgument(std::string name, std::string ctypeName) : name(name), ctypeName(ctypeName), vectorLength(1), isPointer(false), sizeInBytes(0), startOffsetInArgTable(0), usedUAV(0), localPointer(false)
{
	parseCTypeName();
}

void AMDABI::KernelArgument::parseCTypeName()
{
	std::string type = ctypeName;
	
	if (type.back() == '*')
	{
		type.resize(type.length()-1);
		isPointer = true;
	}
	
	if (type.find("*") != std::string::npos)
	{
		throw std::runtime_error("Type is too complex:" + ctypeName);
	}
	
	if (type.find("struct") != std::string::npos)
	{
		shortTypeName = "opaque";
		sizeInBytes = 4; ///should set it manually!
		return;
	}
	
	if (type.back() == '2')
	{
		type.resize(type.length()-1);
		vectorLength = 2;
	}
	
	if (type.back() == '4')
	{
		type.resize(type.length()-1);
		vectorLength = 4;
	}
	
	if (type.back() == '8')
	{
		type.resize(type.length()-1);
		vectorLength = 8;
	}
	
	if (type == "double")
	{
		sizeInBytes = 8;
		shortTypeName = "double";
		return;
	}
	
	if (type == "float")
	{
		sizeInBytes = 4;
		shortTypeName = "float";
		return;
	}
	
	if (type == "char")
	{
		sizeInBytes = 1;
		shortTypeName = "i8";
		return;
	}
	
	if (type == "unsigned char")
	{
		sizeInBytes = 1;
		shortTypeName = "u8";
		return;
	}
	
	if (type == "short")
	{
		sizeInBytes = 2;
		shortTypeName = "i16";
		return;
	}
	
	if (type == "unsigned short")
	{
		sizeInBytes = 2;
		shortTypeName = "u16";
		return;
	}
	
	if (type == "int")
	{
		sizeInBytes = 4;
		shortTypeName = "i32";
		return;
	}
	
	if (type == "unsigned int")
	{
		sizeInBytes = 4;
		shortTypeName = "u32";
		return;
	}
	
	if (type == "long")
	{
		sizeInBytes = 8;
		shortTypeName = "i64";
		return;
	}
	
	if (type == "unsigned long")
	{
		sizeInBytes = 8;
		shortTypeName = "u64";
		return;
	}
	
	throw std::runtime_error("Failed to recognize type name: " + ctypeName);
}

int AMDABI::KernelArgument::getSizeInArgumentTable() const
{
	int minSize = 16;
	
	if (isPointer)
	{
		return minSize;
	}
	
	int dataSize = sizeInBytes * vectorLength;
	
	return std::max(minSize, dataSize);
}
