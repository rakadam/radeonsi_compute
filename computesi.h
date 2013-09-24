#ifndef COMPUTESI_H
#define COMPUTESI_H
#include <stdio.h>
#include <stdlib.h>
#include <drm.h>
#include <xf86drm.h>
#include <radeon_cs_gem.h>
#include <radeon_bo_gem.h>
#include "sid.h"

struct compute_context;
struct cs_reloc_gem;

struct gpu_buffer
{
	struct compute_context* ctx;

	uint64_t alignment;
	uint32_t handle;
	uint32_t domain;
	uint32_t flags;
	uint64_t size;
	
	uint64_t va;
	uint64_t va_size;
	
	uint32_t fragment_number; ///for fragmented allocation
};

struct pool_node
{
	uint64_t va;
	uint64_t size;
	struct gpu_buffer* parent_bo; ///main fragment
	struct gpu_buffer* bo;
	struct pool_node* prev;
	struct pool_node* next;
};

struct compute_context
{
	int fd; ///opened DRM interface
	struct pool_node* vm_pool;
	
};

struct compute_relocs
{
	size_t reloc_num;
	struct cs_reloc_gem* relocs;
};

struct compute_state
{
	int id;
	unsigned user_data[16]; ///shader user data, mapped to SGPRs
	int user_data_length; /// in dwords
	int dim[3];
	int start[3];
	int num_thread[3];
	
	int sgpr_num;
	int vgpr_num;
	int priority;
	
	int debug_mode; ///BOOL
	int priv_mode; ///BOOL
	int trap_en; ///BOOL
	
	int ieee_mode;
	
	int scratch_en;
	int lds_size;
	int excp_en;
	
	int waves_per_sh;
	int thread_groups_per_cu;
	int lock_threshold;
	int simd_dest_cntl;
	
	int se0_sh0_cu_en;
	int se0_sh1_cu_en;
	int se1_sh0_cu_en;
	int se1_sh1_cu_en;
	
	int tmpring_waves;
	int tmpring_wavesize;
	
	struct gpu_buffer* binary;
};

enum radeon_bo_domain
{
	RADEON_DOMAIN_GTT  = 2,
	RADEON_DOMAIN_VRAM = 4
};

struct compute_context* compute_create_context(const char* drm_devfile);
void compute_free_context(struct compute_context* ctx);

void compute_flush_caches(const struct compute_context* ctx);
void compute_pool_alloc(struct compute_context* ctx, struct gpu_buffer* bo);
void compute_pool_free(struct compute_context* ctx, struct gpu_buffer* bo);

int compute_copy_to_gpu(struct gpu_buffer* bo, size_t gpu_offset, const void* src, size_t size);
int compute_copy_from_gpu(struct gpu_buffer* bo, size_t gpu_offset, void* dst, size_t size);

int compute_send_sync_dma_req(struct compute_context* ctx, struct gpu_buffer* dst_bo, size_t dst_offset, struct gpu_buffer* src_bo, size_t src_offset, size_t size, int sync_flag, int raw_wait_flag, int use_pfp_engine);

int compute_send_async_dma_req(struct compute_context* ctx, struct gpu_buffer* dst_bo, size_t dst_offset, struct gpu_buffer* src_bo, size_t src_offset, size_t size);
int compute_send_dma_fence(struct compute_context* ctx, struct gpu_buffer* bo);

void compute_free_gpu_buffer(struct gpu_buffer* bo);
struct gpu_buffer* compute_alloc_gpu_buffer(struct compute_context* ctx, size_t size, int domain, int alignment);
int compute_emit_compute_state(const struct compute_context* ctx, const struct compute_state* state);
int compute_emit_compute_state_manual_relocs(const struct compute_context* ctx, const struct compute_state* state, int reloc_num, struct cs_reloc_gem* relocs);
struct cs_reloc_gem* compute_allocate_reloc_array(int reloc_num);
void compute_set_reloc(struct cs_reloc_gem* relocs, int index, struct gpu_buffer* bo);
int compute_bo_wait(struct gpu_buffer *boi);


#endif
