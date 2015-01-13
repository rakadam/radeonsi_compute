#include <stdexcept>
#include <assert.h>
#include <iostream>
#include "compute_interface.hpp"

extern "C" {
#include "computesi.h"
};

ComputeInterface::ComputeInterface(std::string driName, std::string busid)
{
	context = compute_create_context(driName.c_str(), busid.c_str());
	
	if (!context)
	{
		throw std::runtime_error("Could not open DRI interface: " + driName);
	}
}

ComputeInterface::~ComputeInterface()
{
	compute_free_context(context);
}

gpu_buffer* ComputeInterface::bufferAlloc(size_t size)
{
	return compute_alloc_gpu_buffer(context, size, RADEON_DOMAIN_VRAM, 8*1024);
}

gpu_buffer* ComputeInterface::bufferAllocGTT(size_t size)
{
	return compute_alloc_gpu_buffer(context, size, RADEON_DOMAIN_GTT, 8*1024);
}

void ComputeInterface::bufferFree(gpu_buffer* buf)
{
	compute_free_gpu_buffer(buf);
}

uint64_t ComputeInterface::getVirtualAddress(gpu_buffer* buf)
{
	return buf->va;
}

void ComputeInterface::syncDMACopy(gpu_buffer* dst, size_t dst_offset, gpu_buffer* src, size_t src_offset, size_t size)
{
	assert(src);
	assert(dst);
	size_t fragmentSize = 512*1024;
	
	for (size_t i = 0; i < size; i += std::min(fragmentSize, size-i))
	{
		size_t curSize = std::min(fragmentSize, size-i);
		
		compute_send_sync_dma_req(context, dst, i+dst_offset, src, i+src_offset, curSize, i+curSize >= size, i == 0, 1);
	}
}

void ComputeInterface::asyncDMACopy(gpu_buffer* dst, size_t dst_offset, gpu_buffer* src, size_t src_offset, size_t size)
{
	assert(src);
	assert(dst);
	size_t fragmentSize = 512*1024;
	
	for (size_t i = 0; i < size; i += std::min(fragmentSize, size-i))
	{
		size_t curSize = std::min(fragmentSize, size-i);
		
		compute_send_async_dma_req(context, dst, i+dst_offset, src, i+src_offset, curSize);
	}
}

void ComputeInterface::asyncDMAFence(gpu_buffer* bo)
{
	compute_send_dma_fence(context, bo);
}

void ComputeInterface::transferToGPU(gpu_buffer* buf, size_t offset, const void* data, size_t size, EventDependence evd)
{
	compute_copy_to_gpu(buf, offset, data, size);
}

void ComputeInterface::transferFromGPU(gpu_buffer* buf, size_t offset, void* data, size_t size, EventDependence evd)
{
	compute_copy_from_gpu(buf, offset, data, size);
}

void ComputeInterface::launch(std::vector<uint32_t> userData, std::vector<size_t> threadOffset, std::vector<size_t> blockDim, std::vector<size_t> localSize, gpu_buffer* code,
															const std::vector<gpu_buffer*>& usedMemories,
															int localMemSize, int vgprnum, int sgprnum, EventDependence evd)
{
	assert(localMemSize <= 32*1024);
	
	assert(vgprnum < 257);
	assert(sgprnum < 129);
	assert(localSize.size() == blockDim.size());
	assert(localSize.size() <= 3);
	assert(localSize.size() > 0);
	assert(userData.size() <= 16);
	
	vgprnum = vgprnum == 0 ? 256 : vgprnum;
	sgprnum = sgprnum == 0 ? 104 : sgprnum;
	
	localSize.resize(3, 1);
	blockDim.resize(3, 1);
	threadOffset.resize(3, 0);
	
	compute_state state;
	
	state.id = 0;
	state.user_data_length = userData.size();

	for (unsigned i = 0; i < userData.size(); i++)
	{
		state.user_data[i] = userData[i];
	}
	
	state.dim[0] = blockDim[0];
	state.dim[1] = blockDim[1];
	state.dim[2] = blockDim[2];
	state.start[0] = threadOffset[0];
	state.start[1] = threadOffset[1];
	state.start[2] = threadOffset[2];
	state.num_thread[0] = localSize[0];
	state.num_thread[1] = localSize[1];
	state.num_thread[2] = localSize[2];
	
	state.sgpr_num = (sgprnum+7)/8-1;
	state.vgpr_num = (vgprnum+3)/4-1;
	state.priority = 0;
	state.debug_mode = 0;
	state.priv_mode = 0;
	state.trap_en = 0;
	state.excp_en = 0;
	state.ieee_mode = 1;
	state.scratch_en = 0;
	state.lds_size = (localMemSize+255) / 256;
	state.waves_per_sh = 0; ///zero means automatic maximum, practically limited by registers
	state.thread_groups_per_cu = 0; ///zero means automatic maximum, practically limited by local memory and registers
	state.lock_threshold = 0;
	state.simd_dest_cntl = 0;
	state.se0_sh0_cu_en = 0xFF;
	state.se0_sh1_cu_en = 0xFF;
	state.se1_sh0_cu_en = 0xFF;
	state.se1_sh1_cu_en = 0xFF;
	state.tmpring_waves = 0;
	state.tmpring_wavesize = 0;
	state.binary = code;

	struct compute_relocs crelocs;
	compute_init_relocs(&crelocs);
	
	for (auto bo : usedMemories)
	{
		compute_push_reloc(&crelocs, bo);
	}
	
	int ret = compute_emit_compute_state_manual_relocs(context, &state, crelocs);

	free(crelocs.relocs);
	
	if (ret != 0)
	{
		throw std::runtime_error("Error while running kernel: " + std::string(strerror(errno)));
	}
	
//
}

void ComputeInterface::waitBuffer(gpu_buffer* buf)
{
	compute_bo_wait(buf);
	compute_flush_caches(context);
}
