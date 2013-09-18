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
    std::vector<compute_context*> ctx;
    bool isDriAvailable(std::string fname);
public:
    ComputeInterface();
    ~ComputeInterface();

    size_t devNum() { return ctx.size(); }

    gpu_buffer* bufferAlloc(size_t size);
    void bufferFree(gpu_buffer* buf);

    void transferToGPU(gpu_buffer* buf, void* data, size_t size, EventDependence evd = EventDependence());
    void transferFromGPU(gpu_buffer* buf, void* data, size_t size, EventDependence evd = EventDependence());

    template<typename T>
    void transferToGPU(gpu_buffer* buf, const T& data, EventDependence evd = EventDependence())
    {
        transferToGPU(buf, &data[0], data.size()*sizeof(data[0]), evd);
    }

    template<typename T>
    void transferFromGPU(gpu_buffer* buf, const T& data, EventDependence evd = EventDependence())
    {
        transferFromGPU(buf, &data[0], data.size()*sizeof(data[0]), evd);
    }

    void launch(const std::vector<uint32_t>& userData, const std::vector<size_t>& threadOffset, const std::vector<size_t>& blockDim, const std::vector<size_t>& localSize, gpu_buffer* code, EventDependence evd = EventDependence());
};

#endif
