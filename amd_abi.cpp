#include <stdexcept>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>
#include <fstream>
#include "amd_abi.h"

const int firstUsableUAV = 12;

AMDABI::AMDABI(std::string kernelName) : kernelName(kernelName)
{
	dim = 0;
	privateMemSize = 0;
	localMemSize = 0;
	sgprCount = 16;
	vgprCount = 4;
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
			usedUAVs.insert(UAVid);
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
		while (index % elem.regCount != 0) ///< we need to align the start indexes
		{
			index++;
		}
		
		elem.startSReg = index;
		index += elem.regCount;
	}
}

void AMDABI::setRegUse(int sgprCount, int vgprCount)
{
	while (sgprCount % 8 != 0)
	{
		sgprCount++;
	}
	
	while (vgprCount % 4 != 0)
	{
		vgprCount++;
	}
	
	this->sgprCount = sgprCount;
	this->vgprCount = vgprCount;
}

void AMDABI::buildInternalData()
{
	numberUAVs();
	computeKernelArgumentTableLayout();
	allocateUserElements();
	makeABIIntro();
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
	if (dim >= this->dim)
	{
		throw std::runtime_error("get_group_id dimension error" + std::to_string(dim) + " is bigger than the available maximum");
	}
	
	ScalarRegister reg;
	int base = getAllocatedUserRegCount();
	
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

void AMDABI::makeABIIntro()
{
	abiIntro.clear();
	
	if (privateMemSize > 0)
	{
		privateMemoryBufres.sizeInDWords = 4;
		privateMemoryBufres.sreg = getPredefinedUserRegCount();
		
		ScalarMemoryReadTuple readPrivateMemoryBufres;
		
		readPrivateMemoryBufres.sregBase = userElementTable.at(userElementNameToIndex.at("privateMemoryBufresPtr")).startSReg;;
		readPrivateMemoryBufres.bufferResourceAtSregBase = false;
		readPrivateMemoryBufres.offset = 0x04;
		readPrivateMemoryBufres.sizeInDWords = 4;
		readPrivateMemoryBufres.targetSreg = privateMemoryBufres.sreg;
		
		abiIntro.push_back(readPrivateMemoryBufres);
	}
}

std::vector< AMDABI::ScalarMemoryReadTuple > AMDABI::getABIIntro() const
{
	return abiIntro;
}

int AMDABI::getAllocatedUserRegCount() const
{
	int number = userElementTable.back().startSReg + userElementTable.back().regCount;
	
	return number;
}

int AMDABI::getPredefinedUserRegCount() const
{
	int base = getAllocatedUserRegCount();
	
	base += dim; ///< group_ids written by the hardware
	
	if (privateMemSize > 0)
	{
		base += 1; ///< scratch offset computed by the hardware
	}
	
	return base;
}

int AMDABI::getFirstFreeSRegAfterABIIntro() const
{
	int base = getPredefinedUserRegCount();
	
	for (ScalarMemoryReadTuple read : abiIntro)
	{
		int n = read.targetSreg + read.sizeInDWords;
		base = std::max(base, n);
	}
	
	return base;
}

int AMDABI::getFirstFreeVRegAfterABIIntro() const
{
	return dim;
}

AMDABI::ScalarRegister AMDABI::getPrivateMemoryOffsetRegsiter() const
{
	if (privateMemSize == 0)
	{
		throw std::runtime_error("Private memory is turned off");
	}
	
	ScalarRegister privateMemoryOffset;
	
	privateMemoryOffset.sizeInDWords = 1;
	privateMemoryOffset.sreg = getPredefinedUserRegCount()-1;
	
	return privateMemoryOffset;
}

AMDABI::ScalarRegister AMDABI::getPrivateMemoryResourceDescriptorRegister() const
{
	return privateMemoryBufres;
}

std::string AMDABI::makeRegisterResourceTable() const
{
	std::stringstream ss;
	
	ss << "num_vgprs " << vgprCount << std::endl;
	ss << "num_sgprs " << sgprCount << std::endl;
	ss << "float_mode 192" << std::endl;
	ss << "ieee_mode 0" << std::endl;
	
	return ss.str();
}

std::string AMDABI::makeRSRC2Table() const
{
	std::stringstream ss;
	
	ss << "rsrc2_scrach_en      "  << (privateMemSize > 0) << std::endl;
	ss << "rsrc2_user_sgpr      "  << getAllocatedUserRegCount() << std::endl;
	ss << "rsrc2_trap_present   0" << std::endl;
	ss << "rsrc2_tgid_x_en      "  << (dim > 0) << std::endl;
	ss << "rsrc2_tgid_y_en      "  << (dim > 1) << std::endl;
	ss << "rsrc2_tgid_z_en      "  << (dim > 2) << std::endl;
	ss << "rsrc2_tg_size_en     0" << std::endl;
	ss << "rsrc2_tidig_comp_cnt "  << (dim > 0 ? dim-1 : 0) << std::endl;
	ss << "rsrc2_excp_en_msb    0" << std::endl;
	ss << "rsrc2_lds_size       "  << localMemSize / 256 << std::endl;
	ss << "rsrc2_excp_en        0" << std::endl;
	ss << "rsrc2_unknown1       0" << std::endl;
	
	return ss.str();
}

std::string AMDABI::makeUAVListTable() const
{
	std::stringstream ss;
	
	for (int UAVID : usedUAVs)
	{
		ss << "uav " << UAVID << " 4 0 5" << std::endl;
	}
	
	if (privateMemSize > 0)
	{
		ss << "uav 8 3 0 5" << std::endl; ///implicit UAV
	}
	
	return ss.str();
}

std::string AMDABI::makeUserElementTable() const
{
	std::stringstream ss;
	
	for (UserElementDescriptor elem : userElementTable)
	{
		ss << "user_element " << elem.dataClass << " " << elem.apiSlot << " " << elem.startSReg << " " << elem.regCount << std::endl;
	}
	
	return ss.str();
}

std::string AMDABI::makeMetaKernelArgTable() const
{
	std::stringstream ss;
	
	for (const KernelArgument& arg : kernelArguments)
	{
		ss << ";";
		
		if (arg.isPointer)
		{
			ss << "pointer:";
		}
		else
		{
			ss << "value:";
		}
		
		ss << arg.name << ":" << arg.shortTypeName << ":" << arg.vectorLength << ":" << 1 << ":" << arg.startOffsetInArgTable;
		
		if (arg.isPointer and arg.usedUAV > 0)
		{
			ss << ":" << "uav" << ":" << arg.usedUAV << ":" << arg.sizeInBytes << ":" << (arg.readOnly ? "RO" : "RW") << ":" << 0 << ":" << 0;
		}
		else if (arg.isPointer and arg.localPointer)
		{
			ss << ":" << "hl" << ":" << 1 << ":" << arg.sizeInBytes << ":" << (arg.readOnly ? "RO" : "RW") << ":" << 0 << ":" << 0;
		}
		
		ss << std::endl;
	}
	
	return ss.str();
}

std::string AMDABI::makeMetaMisc() const
{
	std::stringstream ss;
	
	ss << ";memory:datareqd" << std::endl;
	ss << ";function:1:1033" << std::endl;
	ss << ";uavid:11" << std::endl;
	ss << ";printfid:9" << std::endl;
	ss << ";cbid:10" << std::endl;
	ss << ";privateid:8" << std::endl;
	
	return ss.str();
}

std::string AMDABI::makeMetaReflectionTable() const
{
	std::stringstream ss;
	
	int index = 0;
	
	for (const KernelArgument& arg : kernelArguments)
	{
		ss << ";";
		
		ss << "reflection" << ":" << index << ":" << arg.oclTypeName;
		ss << std::endl;
		index++;
	}
	
	return ss.str();
}

std::string AMDABI::makeInnerMetaData() const
{
	std::stringstream ss;
	
	ss << "machine 26 4 0" << std::endl; //Tahiti GPU
	
	ss << makeUAVListTable();
	ss << std::endl;
	ss << "cb 0 0" << std::endl;
	ss << "cb 1 0" << std::endl;
	ss << std::endl;
	ss << makeUserElementTable();
	ss << std::endl;
	ss << makeRegisterResourceTable();
	ss << std::endl;
	ss << makeRSRC2Table();
	ss << std::endl;
	
	ss << "float_consts_begin" << std::endl;
	
	for (int i = 0; i < 256; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			ss << 0 << " #[" << i << "][" << j << "]" << std::endl;
		}
	}
	
	ss << "float_consts_end" << std::endl;
	ss << std::endl;
	ss << "int_consts_begin" << std::endl;
	
	for (int i = 0; i < 32; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			ss << 0 << " #[" << i << "][" << j << "]" << std::endl;
		}
	}
	
	ss << "int_consts_end" << std::endl;
	ss << std::endl;
	ss << "bool_consts_begin" << std::endl;
	
	for (int i = 0; i < 32; i++)
	{
		ss << 0 << " #[" << i << "]" << std::endl;
	}
	
	ss << "bool_consts_end" << std::endl;
	ss << std::endl;
	
	return ss.str();
}

std::string AMDABI::makeMetaData() const
{
	std::stringstream ss;
	
	ss << ";ARGSTART:__OpenCL_" + kernelName << "_kernel" << std::endl;
	ss << ";version:3:1:111" << std::endl;
	ss << ";device:tahiti" << std::endl;
	ss << ";uniqueid:1024" << std::endl;
	ss << ";memory:uavprivate:" << privateMemSize << std::endl;
	ss << ";memory:hwlocal:" << localMemSize << std::endl;
	ss << ";memory:hwregion:0" << std::endl;
	
	ss << makeMetaKernelArgTable();
	ss << makeMetaMisc();
	ss << makeMetaReflectionTable();
	
	ss << ";ARGEND:__OpenCL_" << kernelName << "_kernel" << std::endl;
	
	return ss.str();
}

void AMDABI::generateIntoDirectory(std::string baseDir, std::vector< uint32_t > bytecode) const
{
	int retval = 0;
	std::string dirname = baseDir + "/" + kernelName;
	
	retval = mkdir(dirname.c_str(), 0755);
	
	if (retval)
	{
		throw std::runtime_error(dirname + " : " + strerror(errno));
	}
	
	std::ofstream(dirname + "/" + kernelName + ".metadata") << makeMetaData() << std::endl;
	std::ofstream(dirname + "/" + kernelName + "_inner_26" + ".encoding") << makeInnerMetaData() << std::endl;
	
	std::vector<uint8_t> dummyHeader(32);
	dummyHeader[20] = 1;
	std::ofstream(dirname + "/" + kernelName + ".header", std::ofstream::binary).write((const char*)dummyHeader.data(), dummyHeader.size());
	std::ofstream(dirname + "/" + kernelName + "_inner_26" + ".bytecode", std::ofstream::binary).write((const char*)bytecode.data(), bytecode.size()*sizeof(bytecode[0]));
	
	system(("elf_build_inner " + dirname + " " + kernelName + " " + dirname + "/" + kernelName + ".kernel").c_str());
	system(("elf_build " + dirname + " " + kernelName + " " + baseDir + "/" + kernelName + ".bin").c_str());
}

AMDABI::ScalarMemoryReadTuple::ScalarMemoryReadTuple() : targetSreg(-1), sregBase(-1), bufferResourceAtSregBase(false), offset(0), sizeInDWords(-1)
{
}

AMDABI::KernelArgument::KernelArgument(std::string name, std::string ctypeName, bool readOnly)
 : name(name), ctypeName(ctypeName), readOnly(readOnly), vectorLength(1), isPointer(false), sizeInBytes(0), startOffsetInArgTable(0), usedUAV(0), localPointer(false)
{
	parseCTypeName();
	
	if (shortTypeName != "opaque")
	{
		if (vectorLength > 1)
		{
			oclTypeName += std::to_string(vectorLength);
		}
		
		if (isPointer)
		{
			oclTypeName += "*";
		}
	}
}

void AMDABI::KernelArgument::parseCTypeName()
{
	std::string type = ctypeName;
	
	if (type.find("const") != std::string::npos)
	{
		std::string str = "const";
		readOnly = true;
		type.erase(type.find(str), str.length());
	}
	
	while (type.length() and type.front() == ' ')
	{
		type.erase(0, 1);
	}
	
	while (type.length() and type.back() == ' ')
	{
		type.resize(type.length()-1);
	}
	
	if (type.back() == '*')
	{
		type.resize(type.length()-1);
		isPointer = true;
	}
	
	if (type.find("*") != std::string::npos)
	{
		throw std::runtime_error("Type is too complex:" + ctypeName);
	}
	
	while (type.length() and type.front() == ' ')
	{
		type.erase(0, 1);
	}
	
	while (type.length() and type.back() == ' ')
	{
		type.resize(type.length()-1);
	}
	
	if (type.find("struct") != std::string::npos)
	{
		shortTypeName = "opaque";
		oclTypeName = ctypeName;
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
		oclTypeName = "double";
		return;
	}
	
	if (type == "float")
	{
		sizeInBytes = 4;
		shortTypeName = "float";
		oclTypeName = "float";
		return;
	}
	
	if (type == "char")
	{
		sizeInBytes = 1;
		shortTypeName = "i8";
		oclTypeName = "char";
		return;
	}
	
	if (type == "unsigned char")
	{
		sizeInBytes = 1;
		shortTypeName = "u8";
		oclTypeName = "uchar";
		return;
	}
	
	if (type == "short")
	{
		sizeInBytes = 2;
		shortTypeName = "i16";
		oclTypeName = "short";
		return;
	}
	
	if (type == "unsigned short")
	{
		sizeInBytes = 2;
		shortTypeName = "u16";
		oclTypeName = "ushort";
		return;
	}
	
	if (type == "int")
	{
		sizeInBytes = 4;
		shortTypeName = "i32";
		oclTypeName = "int";
		return;
	}
	
	if (type == "unsigned int")
	{
		sizeInBytes = 4;
		shortTypeName = "u32";
		oclTypeName = "uint";
		return;
	}
	
	if (type == "long")
	{
		sizeInBytes = 8;
		shortTypeName = "i64";
		oclTypeName = "long";
		return;
	}
	
	if (type == "unsigned long")
	{
		sizeInBytes = 8;
		shortTypeName = "u64";
		oclTypeName = "ulong";
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
