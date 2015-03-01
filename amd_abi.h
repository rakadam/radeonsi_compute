#ifndef AMDABI_H
#define AMDABI_H
#include <string>
#include <vector>
#include <map>
#include <set>

class AMDABI
{
public:
	struct VectorRegister
	{
		int vreg;
		int sizeInDWords;
	};
	
	struct ScalarRegister
	{
		int sreg;
		int sizeInDWords;
	};
	
	struct ScalarMemoryReadTuple
	{
		ScalarMemoryReadTuple();
		
		int targetSreg; ///-1 if we don't specify it
		int sregBase;
		bool bufferResourceAtSregBase; ///< if it's true, then sregBase is a buffer resource descriptor, if it is false, then it is a raw pointer
		int offset;
		int sizeInDWords;
	};
	
	struct KernelArgument
	{
		KernelArgument(std::string name, std::string ctypeName, bool readOnly = false);
		void parseCTypeName();
		
		std::string name;
		std::string ctypeName;
		bool readOnly;
		std::string shortTypeName; ///< pointed type name if pointer
		std::string oclTypeName;
		int vectorLength;
		bool isPointer;
		int sizeInBytes; ///< pointed size in bytes if it is a pointer
		int startOffsetInArgTable;
		int usedUAV;
		bool localPointer;
		
		int getSizeInArgumentTable() const;
	};
	
	struct UserElementDescriptor
	{
		std::string name;
		int dataClass;
		int apiSlot;
		int startSReg;
		int regCount;
	};
	
	int dim;
	std::string kernelName;
	int privateMemSize;
	int localMemSize;
	std::map<std::string, int> KernelArgumentNameToIndex;
	std::vector<KernelArgument> kernelArguments;
	std::set<int> usedUAVs;
	
	std::map<std::string, int> userElementNameToIndex;
	std::vector<UserElementDescriptor> userElementTable;
	
	ScalarRegister privateMemoryBufres;
	std::vector< AMDABI::ScalarMemoryReadTuple > abiIntro;
	int sgprCount;
	int vgprCount;
	std::map<int, ScalarRegister> UAVBufresInRegs; ///< argindex To bufres scalar register
private:
	void computeKernelArgumentTableLayout();
	void numberUAVs();
	void allocateUserElements();
	void makeABIIntro();
	std::string makeRSRC2Table() const;
	std::string makeUAVListTable() const;
	std::string makeUserElementTable() const;
	std::string makeRegisterResourceTable() const;
	
	std::string makeMetaKernelArgTable() const;
	std::string makeMetaMisc() const;
	std::string makeMetaReflectionTable() const;
	
	ScalarMemoryReadTuple readUAVBufresForKernelArgument(int index) const;
	ScalarMemoryReadTuple readUAVBufresForKernelArgument(std::string argName) const;
	
	void align(int& value, int divisor) const;
public:
	AMDABI(std::string kernelName);
	
	void setDimension(int dim);
	void setPrivateMemorySizePerItem(int sizeInBytes);
	void setLocalMemorySize(int sizeInBytes);
	void addKernelArgument(std::string name, std::string ctypeName, int customSizeInBytes = -1);
	void addKernelArgumentLocalMemory(std::string name, std::string ctypeName);
	
	/**
	 * \brief Set the Register counts used by the kernel, these counts includes the predefined regs also
	 */
	void setRegUse(int sgprCount, int vgprCount);
	
	void buildInternalData();
	
	ScalarMemoryReadTuple get_local_size(int dim) const;
	ScalarMemoryReadTuple get_global_size(int dim) const;
	ScalarMemoryReadTuple get_num_groups(int dim) const;
	ScalarMemoryReadTuple get_global_offset(int dim) const;
	VectorRegister get_local_id(int dim) const;
	ScalarRegister get_group_id(int dim) const;
	
	ScalarMemoryReadTuple getKernelArgument(int index) const;
	ScalarMemoryReadTuple getKernelArgument(std::string name) const;
	
	std::vector<ScalarMemoryReadTuple> getABIIntro() const;
	int getAllocatedUserRegCount() const;
	int getPredefinedUserRegCount() const;
	int getFirstFreeSRegAfterABIIntro() const;
	int getFirstFreeVRegAfterABIIntro() const;
	
	ScalarRegister getPrivateMemoryOffsetRegsiter() const;
	ScalarRegister getPrivateMemoryResourceDescriptorRegister() const;
	
	ScalarRegister getUAVBufresForKernelArgument(int index) const;
	ScalarRegister getUAVBufresForKernelArgument(std::string argName) const;
	
	std::string makeInnerMetaData() const;
	std::string makeMetaData() const;
	
	void generateIntoDirectory(std::string baseDir, std::vector<uint32_t> bytecode) const;
};

#endif // AMDABI_H
