#ifndef AMDABI_H
#define AMDABI_H
#include <string>
#include <vector>
#include <map>

class AMDABI
{
	struct VectorRegister
	{
		int vreg;
		int sizeInDWords;
	};
	
	struct ScalarRegister
	{
		int vreg;
		int sizeInDWords;
	};
	
	struct ScalarMemoryReadTuple
	{
		int targetSreg; ///-1 if we don't specify it
		int sregBase;
		bool bufferResourceAtSregBase;
		int offset;
		int sizeInDWords;
	};
	
	struct KernelArgument
	{
		KernelArgument(std::string name, std::string ctypeName);
		
		std::string name;
		std::string ctypeName;
		std::string shortTypeName; ///< pointed type name if pointer
		bool isPointer;
		int sizeInBytes; ///< pointed size in bytes if it is a pointer
		int startOffsetInArgTable;
		int usedUAV;
		bool localPointer;
	};
	
	std::string kernelName;
	std::map<std::string, int> KernelArgumentNameToIndex;
	std::vector<KernelArgument> kernelArguments;
	
public:
	AMDABI(std::string kernelName);
	
	void setDimension(int dim);
	void setPrivateMemorySizePerItem(int sizeInBytes);
	void setLocalMemorySize(int sizeInBytes);
	void addKernelArgument(std::string name, std::string ctypeName);
	void addKernelArgumentLocalMemory(std::string name, std::string ctypeName);
	
	ScalarMemoryReadTuple get_local_size(int dim);
	ScalarMemoryReadTuple get_global_size(int dim);
	ScalarMemoryReadTuple get_num_groups(int dim);
	ScalarMemoryReadTuple get_global_offset(int dim);
	VectorRegister get_local_id(int dim);
	ScalarRegister get_group_id(int dim);
	
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
