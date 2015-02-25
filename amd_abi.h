#ifndef AMDABI_H
#define AMDABI_H
#include <string>
#include <vector>
#include <map>
#include <set>

class AMDABI
{
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
		KernelArgument(std::string name, std::string ctypeName);
		void parseCTypeName();
		
		std::string name;
		std::string ctypeName;
		std::string shortTypeName; ///< pointed type name if pointer
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
	
public:
	AMDABI(std::string kernelName);
	
	void setDimension(int dim);
	void setPrivateMemorySizePerItem(int sizeInBytes);
	void setLocalMemorySize(int sizeInBytes);
	void addKernelArgument(std::string name, std::string ctypeName, int customSizeInBytes = -1);
	void addKernelArgumentLocalMemory(std::string name, std::string ctypeName);
	void computeKernelArgumentTableLayout();
	void numberUAVs();
	void allocateUserElements();
	
	ScalarMemoryReadTuple get_local_size(int dim) const;
	ScalarMemoryReadTuple get_global_size(int dim) const;
	ScalarMemoryReadTuple get_num_groups(int dim) const;
	ScalarMemoryReadTuple get_global_offset(int dim) const;
	VectorRegister get_local_id(int dim) const;
	ScalarRegister get_group_id(int dim) const;
	
	ScalarMemoryReadTuple getKernelArgument(int index) const;
	ScalarMemoryReadTuple getKernelArgument(std::string name) const;
	
	std::vector<ScalarMemoryReadTuple> getABIIntro() const;
	int getFirstFreeSRegAfterABIIntro() const;
	
	ScalarRegister getPrivateMemoryOffsetRegsiter() const;
	ScalarRegister getPrivateMemoryResourceDescriptorRegister() const;
	
	std::string makeRSRC2Table();
	std::string makeUAVListTable();
	std::string makeUserElementTable();
	std::string makeRegisterResourceTable();
};

#endif // AMDABI_H
