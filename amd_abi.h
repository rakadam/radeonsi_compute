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
	
	std::string makeInnerMetaData() const;
	std::string makeMetaData() const;
public:
	AMDABI(std::string kernelName);
	
	/**
	 * \brief Dimension of the thread ids, [0..3]
	 */
	void setDimension(int dim);
	
	/**
	 * \brief Set the size of the private memory in bytes used by the kernel
	 * 
	 * This can be used for explicitly or for register spilling
	 */
	void setPrivateMemorySizePerItem(int sizeInBytes);
	
	/**
	 * \brief Set the size of the local memory statically used by the kernel
	 */
	void setLocalMemorySize(int sizeInBytes);
	
	/**
	 * \brief Add a kernel argument with a unique name
	 * 
	 * \param customSizeInBytes If the argument is an opaque type, the actualy size needs to be set manually
	 */
	void addKernelArgument(std::string name, std::string ctypeName, int customSizeInBytes = -1);
	
	/**
	 * \brief Add a kernel argument representing a dynamical local memory allocation
	 */
	void addKernelArgumentLocalMemory(std::string name, std::string ctypeName);
	
	/**
	 * \brief Set the Register counts used by the kernel, these counts includes the predefined regs also
	 */
	void setRegUse(int sgprCount, int vgprCount);
	
	/**
	 * \brief Build all the internal data structures after everything was set
	 * 
	 * This must be called before querying anything
	 */
	void buildInternalData();
	
	/**
	 * \brief Query information about the memory where the local size is stored
	 * 
	 * Seel also in OpenCL standard
	 */
	ScalarMemoryReadTuple get_local_size(int dim) const;
	
	/**
	 * \brief Query information about the memory where the global size is stored
	 * 
	 * Seel also in OpenCL standard
	 */
	ScalarMemoryReadTuple get_global_size(int dim) const;
	
	/**
	 * \brief Query information about the memory where the number of groups is stored
	 * 
	 * Seel also in OpenCL standard
	 */
	ScalarMemoryReadTuple get_num_groups(int dim) const;
	
	/**
	 * \brief Query information about the memory where the global offset is stored
	 * 
	 * Seel also in OpenCL 2.0 standard
	 */
	ScalarMemoryReadTuple get_global_offset(int dim) const;
	
	/**
	 * \brief Query information about the vector register where the local id is stored
	 * 
	 * Seel also in OpenCL standard
	 */
	VectorRegister get_local_id(int dim) const;
	
	/**
	 * \brief Query information about the scalar register where the group id is stored
	 * 
	 * Seel also in OpenCL standard
	 */
	ScalarRegister get_group_id(int dim) const;
	
	/**
	 * \brief Query information about the memory where the value of the kernel argument is stored
	 * 
	 * \param index Index of the kernel argument
	 */
	ScalarMemoryReadTuple getKernelArgument(int index) const;
	
	/**
	 * \brief Query information about the memory where the value of the kernel argument is stored
	 * 
	 * \param name Name of the kernel argument
	 */
	ScalarMemoryReadTuple getKernelArgument(std::string name) const;
	
	/**
	 * \brief Query information about the ABI specific memory reads which need to happen at the beginning of the kernel executable
	 * 
	 * This is mandatory for correct operation
	 */
	std::vector<ScalarMemoryReadTuple> getABIIntro() const;
	
	/**
	 * \brief Get the number of user scalar registers (max 16) which will be pre-initialized before the kernel execution
	 * 
	 * These registers are numbered continuously from zero
	 * See \ref{getFirstFreeSRegAfterABIIntro} and \ref{getFirstFreeVRegAfterABIIntro}
	 */
	int getAllocatedUserRegCount() const;
	
	/**
	 * \brief Get the number of scalar registers which will be pre-initialized by the hardware or the software before kernel execution
	 * 
	 * These registers are numbered continuously from zero
	 * See \ref{getFirstFreeSRegAfterABIIntro} and \ref{getFirstFreeVRegAfterABIIntro}
	 */
	int getPredefinedUserRegCount() const;
	
	/**
	 * \brief Index of the first scalar register which is not used by the ABI.
	 * 
	 * The kernel scalar register allocation should be started from this
	 */
	int getFirstFreeSRegAfterABIIntro() const;
	
	/**
	 * \brief Index of the first vector register which is not used by the ABI
	 * 
	 * The kernel scalar register allocation could be started from this offset,
	 * optionally these vector registers can be reused if local_id is no longer necessary
	 */
	int getFirstFreeVRegAfterABIIntro() const;
	
	/**
	 * \brief Query the scalar register which stores the private memory offset of the current vector core
	 * 
	 * This offset is calculated by the hardware based on the running wavefronts
	 */
	ScalarRegister getPrivateMemoryOffsetRegsiter() const;
	
	/**
	 * \brief Query the scalar registers which store the buffer resource descriptor for the private memory
	 */
	ScalarRegister getPrivateMemoryResourceDescriptorRegister() const;
	
	/**
	 * \brief Query the scalar registers which store the buffer resource descriptor associated with the kernel argument if the kernel argument is a pointer
	 * 
	 * This only makes sense for global pointer kernel arguments
	 * \param index Index of the kernel argument
	 */
	ScalarRegister getUAVBufresForKernelArgument(int index) const;
	
	/**
	 * \brief Query the scalar registers which store the buffer resource descriptor associated with the kernel argument if the kernel argument is a pointer
	 * 
	 * This only makes sense for global pointer kernel arguments
	 * \param name Name of the kernel argument
	 */
	ScalarRegister getUAVBufresForKernelArgument(std::string argName) const;
	
	/**
	 * \brief Generate the executable file
	 */
	void generateIntoDirectory(std::string baseDir, std::vector<uint32_t> bytecode) const;
};

#endif // AMDABI_H
