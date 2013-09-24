#ifndef _COMPUTE_INTERFACE_HPP_
#define _COMPUTE_INTERFACE_HPP_
#include <vector>
#include <string>

struct gpu_buffer;
struct compute_context;

class EventDependence
{
};

class ComputeInterface
{
	compute_context* context;
public:
	ComputeInterface(std::string driName);
	~ComputeInterface();

	gpu_buffer* bufferAlloc(size_t size);
	gpu_buffer* bufferAllocGTT(size_t size);
	void bufferFree(gpu_buffer* buf);

	uint64_t getVirtualAddress(gpu_buffer* buf);
	void transferToGPU(gpu_buffer* buf, size_t offset, const void* data, size_t size, EventDependence evd = EventDependence());
	void transferFromGPU(gpu_buffer* buf, size_t offset, void* data, size_t size, EventDependence evd = EventDependence());

	template<typename T>
	void transferToGPU(gpu_buffer* buf, size_t offset, const T& data, EventDependence evd = EventDependence())
	{
		transferToGPU(buf, offset, &data[0], data.size()*sizeof(data[0]), evd);
	}

	template<typename T>
	void transferFromGPU(gpu_buffer* buf, size_t offset, T& data, EventDependence evd = EventDependence())
	{
		transferFromGPU(buf, offset, &data[0], data.size()*sizeof(data[0]), evd);
	}

	void launch(std::vector<uint32_t> userData, std::vector<size_t> threadOffset, std::vector<size_t> blockDim, std::vector<size_t> localSize, gpu_buffer* code, int localMemSize = 32*1024, int vgprnum=0, int sgprnum=0, EventDependence evd = EventDependence());
};

#endif
