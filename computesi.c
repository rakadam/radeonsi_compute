#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include "computesi.h"
#include <va/va_dri2.h>

#define PKT3C(a, b, c) (PKT3(a, b, c) | 1 << 1)

#define set_compute_reg(reg, val) do {\
	assert(reg >= SI_SH_REG_OFFSET && reg <= SI_SH_REG_END); \
	buf[cdw++] = PKT3C(PKT3_SET_SH_REG, 1, 0); \
	buf[cdw++] = (reg - SI_SH_REG_OFFSET) >> 2; \
	buf[cdw++] = val; \
	}while (0)

#ifndef RADEON_VA_MAP
	
#define RADEON_VA_MAP               1
#define RADEON_VA_UNMAP             2
			
#define RADEON_VA_RESULT_OK         0
#define RADEON_VA_RESULT_ERROR      1
#define RADEON_VA_RESULT_VA_EXIST   2
			
#define RADEON_VM_PAGE_VALID        (1 << 0)
#define RADEON_VM_PAGE_READABLE     (1 << 1)
#define RADEON_VM_PAGE_WRITEABLE    (1 << 2)
#define RADEON_VM_PAGE_SYSTEM       (1 << 3)
#define RADEON_VM_PAGE_SNOOPED      (1 << 4)

#define RADEON_CHUNK_ID_FLAGS       0x03
#define RADEON_CS_USE_VM            0x02
#define RADEON_CS_RING_GFX          0
#define RADEON_CS_RING_COMPUTE      1
#define RADEON_CS_RING_DMA          2
#define RADEON_CS_RING_UVD          3
/* query if a RADEON_CS_RING_* submission is supported */
#define RADEON_INFO_RING_WORKING 0x15


struct drm_radeon_gem_va {
		uint32_t    handle;
		uint32_t    operation;
		uint32_t    vm_id;
		uint32_t    flags;
		uint64_t    offset;
};

#define DRM_RADEON_GEM_VA   0x2b
#endif

#ifndef RADEON_INFO_VA_START
	#define RADEON_INFO_VA_START          0x0e
	#define RADEON_INFO_IB_VM_MAX_SIZE    0x0f
#endif

#define DMA_PACKET(cmd, sub_cmd, n) ((((cmd) & 0xF) << 28) |    \
                                    (((sub_cmd) & 0xFF) << 20) |\
                                    (((n) & 0xFFFFF) << 0))

/* async DMA Packet types */
#define DMA_PACKET_WRITE                        0x2
#define DMA_PACKET_COPY                         0x3
#define DMA_PACKET_INDIRECT_BUFFER              0x4
#define DMA_PACKET_SEMAPHORE                    0x5
#define DMA_PACKET_FENCE                        0x6
#define DMA_PACKET_TRAP                         0x7
#define DMA_PACKET_SRBM_WRITE                   0x9
#define DMA_PACKET_CONSTANT_FILL                0xd
#define DMA_PACKET_NOP                          0xf

#define FRAGMENT_SIZE (64*1024*1024)

struct cs_reloc_gem {
		uint32_t    handle;
		uint32_t    read_domain;
		uint32_t    write_domain;
		uint32_t    flags;
};


struct compute_context* compute_create_context(const char* drm_devfile)
{
	int ret = 0;
	struct drm_radeon_info ginfo;
	assert(drmAvailable());
	struct compute_context* ctx = malloc(sizeof(struct compute_context));
	
	if (strstr(drm_devfile, "pci:"))
	{
		ctx->fd = drmOpen(NULL, drm_devfile);
	}
	else
	{
		ctx->fd = open(drm_devfile, O_RDWR, 0);
	}
	
	if (ctx->fd < 1)
	{
		printf("Open Error: %s\n", drm_devfile);
		free(ctx);
		return NULL;
	}
	
	uint64_t reserved_mem = 0;
	uint64_t max_vm_size = 0;
	uint64_t ring_working = 0;
	
	ctx->display = NULL;
	ctx->window = NULL;
	
	memset(&ginfo, 0, sizeof(ginfo));
	ginfo.request = RADEON_INFO_RING_WORKING;
	ginfo.value = (uintptr_t)&ring_working;
	
	if (drmCommandWriteRead(ctx->fd, DRM_RADEON_INFO, &ginfo, sizeof(ginfo))) ///We only need to auth if we are disallowed to access the DRM
	{
		drm_magic_t magic = 0;
		
		if(ret=drmGetMagic(ctx->fd, &magic))
		{
			printf("Failed to perform drmGetMagic on %s, error: %s\n", drm_devfile, strerror(-ret));
			close(ctx->fd);
			free(ctx);
			return NULL;
		}
		
		ctx->display = XOpenDisplay(NULL);
		ctx->window = DefaultRootWindow(ctx->display);
		
		printf("display: %p window: %p\n", ctx->display, ctx->window);
		
		ret=VA_DRI2Authenticate(ctx->display, ctx->window, magic);
	}
	
	
	memset(&ginfo, 0, sizeof(ginfo));
	ginfo.request = RADEON_INFO_RING_WORKING;
	ginfo.value = (uintptr_t)&ring_working;

	if ((ret=drmCommandWriteRead(ctx->fd, DRM_RADEON_INFO, &ginfo, sizeof(ginfo))))
	{
		printf("Failed to perform DRM_RADEON_INFO on %s, error: %s\n", drm_devfile, strerror(-ret));
		if (ctx->display)
		{
			XCloseDisplay(ctx->display);
		}
		close(ctx->fd);
		free(ctx);
		return NULL;
	}
	
	if (!ring_working)
	{
		printf("Compute ring is not ready\n");
		return NULL;
	}
	
	memset(&ginfo, 0, sizeof(ginfo));
	ginfo.request = RADEON_INFO_VA_START;
	ginfo.value = (uintptr_t)&reserved_mem;
	
	if ((ret=drmCommandWriteRead(ctx->fd, DRM_RADEON_INFO, &ginfo, sizeof(ginfo))))
	{
		printf("Failed to perform drmCommandWriteRead on %d, error: %s\n", ctx->fd, strerror(-ret));
		if (ctx->display)
		{
			XCloseDisplay(ctx->display);
		}
		close(ctx->fd);
		free(ctx);
		return NULL;
	}
	
	ginfo.request = RADEON_INFO_IB_VM_MAX_SIZE;
	ginfo.value = (uintptr_t)&max_vm_size;
	
	if ((ret=drmCommandWriteRead(ctx->fd, DRM_RADEON_INFO, &ginfo, sizeof(ginfo))))
	{
		printf("Failed to perform drmCommandWriteRead on %d, error: %s\n", ctx->fd, strerror(-ret));
		if (ctx->display)
		{
			XCloseDisplay(ctx->display);
		}
		close(ctx->fd);
		free(ctx);
		return NULL;
	}
	
	printf("reserved mem: 0x%lx vm size: 0x%lx pages\n", reserved_mem, max_vm_size);
	
	ctx->vm_pool = malloc(sizeof(struct pool_node));
	ctx->vm_pool->va = 0;
	ctx->vm_pool->size = reserved_mem+4096; ///reserved VM area by the driver 
	ctx->vm_pool->prev = NULL;
	ctx->vm_pool->next = NULL;

	return ctx;
}

void compute_free_context(struct compute_context* ctx)
{
	while (ctx->vm_pool->next)
	{
		compute_free_gpu_buffer(ctx->vm_pool->next->bo);
	}
	
	if (ctx->display)
	{
		XCloseDisplay(ctx->display);
	}
	
	free(ctx->vm_pool);
	close(ctx->fd);
	free(ctx);
}

void compute_pool_alloc(struct compute_context* ctx, struct gpu_buffer* bo)
{
	struct pool_node *n;
	assert((bo->va_size & 4095) == 0);
	assert(bo->fragment_number > 0);

// 	printf("pool dump:\n");
// 	for (n = ctx->vm_pool; n; n = n->next)
// 	{
// 		printf("pool %p, va: %lX, size: %lX,\n", n, n->va, n->size);
// 	}
// 	printf("\n");
	
	for (n = ctx->vm_pool; n; n = n->next)
	{
		if (n->next && bo->fragment_number == 1)
		{
			if ((int64_t)n->next->va - n->va - n->size > bo->va_size && bo->alignment <= 4096)
			{
				struct pool_node* n2 = malloc(sizeof(struct pool_node));
				
				n2->parent_bo = bo;
				n2->bo = bo;
				n2->va = n->va + n->size;
				n2->size = bo->va_size;
				n2->prev = n;
				n2->next = n->next;
				n->next->prev = n2;
				n->next = n2;
				
				bo->va = n2->va;
				
				return;
			}
		}

		if (n->next == NULL)
		{
			unsigned i = 0;
			
			for (i = 0; i < bo->fragment_number; i++)
			{
				struct pool_node* n2 = malloc(sizeof(struct pool_node));
				struct gpu_buffer* buf = bo + i;
				
				n2->parent_bo = bo;
				n2->bo = buf;
				n2->va = n->va + n->size;
				n2->size = buf->va_size;
				n->next = n2;
				n2->prev = n;
				n2->next = NULL;
				
				buf->va = n2->va;
// 				printf("pool %p, va: %lX, handle: %i fragment: %i, frag_size: %lX, prev_va: %lX prev_pool %p\n", n2, n2->va, buf->handle, i, n2->size, n->va, n);
				
				n = n2;
			}
			
			return;
		}
	}
	
	assert(0 && "unreachable");
}

void compute_pool_free(struct compute_context* ctx, struct gpu_buffer* bo)
{
	struct pool_node *n, *next;
	int found_bo_num = 0;
	
	for (n = ctx->vm_pool; n; n = next)
	{
		next = n->next;
		
		if (n->parent_bo == bo)
		{
			n->prev->next = n->next;
			
			if (n->next)
			{
				n->next->prev = n->prev;
			}
			
			found_bo_num++;
			free(n);
		}
	}
	
	assert(found_bo_num != 0 && "internal error attempted to free a non allocated vm block");
}

static int compute_vm_map(struct compute_context* ctx, struct gpu_buffer* bo, int vm_id, int flags)
{
	unsigned i;
	
	for (i = 0; i < bo->fragment_number; i++)
	{
		struct drm_radeon_gem_va va;
		memset(&va, 0, sizeof(va));
		
		va.handle = bo[i].handle;
		va.vm_id = vm_id;
		va.operation = RADEON_VA_MAP;
		va.flags = flags |
							RADEON_VM_PAGE_READABLE |
							RADEON_VM_PAGE_WRITEABLE;
							
		va.offset = bo[i].va;
		
// 		fprintf(stderr, "mapping: handle:%i va_addr:%lX size:%lX fragment:%i\n", bo[i].handle, bo[i].va, bo[i].size, i);
		
		int r = drmCommandWriteRead(ctx->fd, DRM_RADEON_GEM_VA, &va, sizeof(va));
		
		if (r && va.operation == RADEON_VA_RESULT_ERROR)
		{
			fprintf(stderr, "radeon: Failed to map buffer: %x\n", bo[i].handle);
			return -1;
		}
		
		if (va.operation == RADEON_VA_RESULT_VA_EXIST)
		{
			fprintf(stderr, "double map?\n");
			return -1;
		}
	}
	
	return 0;
}

int compute_vm_remap(struct gpu_buffer* bo)
{
	return compute_vm_map(bo->ctx, bo, 0, RADEON_VM_PAGE_SNOOPED);
}

static int compute_vm_unmap(struct compute_context* ctx, uint64_t vm_addr, uint32_t handle, int vm_id)
{
	return 0; //WARNING
	struct drm_radeon_gem_va va;
	int r;
	
	memset(&va, 0, sizeof(va));
	
	va.handle = handle;
	va.vm_id = vm_id;
	va.operation = RADEON_VA_UNMAP;
	va.flags = RADEON_VM_PAGE_SNOOPED; ///BUG: kernel should ignore this flag for unmap,but still asserts it

	va.offset = vm_addr;
	
	r = drmCommandWriteRead(ctx->fd, DRM_RADEON_GEM_VA, &va, sizeof(va));
	
	if (r && va.operation == RADEON_VA_RESULT_ERROR)
	{
		fprintf(stderr, "radeon: Failed to unmap buffer: %x\n", handle);
		return -1;
	}
	
	return 0;
}

// struct cs_reloc_gem* compute_allocate_reloc_array(int reloc_num)
// {
// 	struct cs_reloc_gem* relocs = calloc(reloc_num, sizeof(struct cs_reloc_gem));
// 	
// 	if (relocs)
// 	{
// 		memset(relocs, 0, reloc_num*sizeof(struct cs_reloc_gem));
// 	}
// 	
// 	return relocs;
// }
// 
// void compute_set_reloc(struct cs_reloc_gem* relocs, int index, struct gpu_buffer* bo)
// {
// 	struct cs_reloc_gem *reloc = &relocs[index];
// 	
// 	memset(reloc, 0, sizeof(struct cs_reloc_gem));
// 	
// 	reloc->handle = bo->handle;
// 	reloc->read_domain = bo->domain;
// 	reloc->write_domain = bo->domain;
// 	reloc->flags = 0;
// }

void compute_init_relocs(struct compute_relocs* crelocs)
{
	crelocs->reloc_num = 0;
	crelocs->relocs = NULL;
}

static void compute_push_reloc_unfragmented(struct compute_relocs* crelocs, const struct gpu_buffer* bo)
{
	if (crelocs->reloc_num == 0)
	{
		crelocs->reloc_num = 1;
		crelocs->relocs = malloc(sizeof(struct cs_reloc_gem));
	}
	else
	{
		crelocs->reloc_num++;
		crelocs->relocs = realloc(crelocs->relocs, crelocs->reloc_num*sizeof(struct cs_reloc_gem));
	}
	
	struct cs_reloc_gem *reloc = &crelocs->relocs[crelocs->reloc_num-1];
	
	memset(reloc, 0, sizeof(struct cs_reloc_gem));
	
	reloc->handle = bo->handle;
	reloc->read_domain = bo->domain;
	reloc->write_domain = bo->domain;
	reloc->flags = 0;
}

void compute_push_reloc(struct compute_relocs* crelocs, const struct gpu_buffer* bo)
{
	unsigned i;
	
	for (i = 0; i < bo->fragment_number; i++)
	{
		compute_push_reloc_unfragmented(crelocs, bo+i);
	}
}

static void compute_create_reloc_table(const struct compute_context* ctx, struct compute_relocs* crelocs)
{
	struct pool_node *n;

	compute_init_relocs(crelocs);
	
	for (n = ctx->vm_pool->next; n; n = n->next)
	{
		compute_push_reloc(crelocs, n->bo);
	}
}

int compute_send_sync_dma_req(struct compute_context* ctx, struct gpu_buffer* dst_bo, size_t dst_offset, struct gpu_buffer* src_bo, size_t src_offset, size_t size, int sync_flag, int raw_wait_flag, int use_pfp_engine)
{
	struct drm_radeon_cs cs;
	unsigned buf[64];
	int cdw = 0;
	uint64_t chunk_array[5];
	struct drm_radeon_cs_chunk chunks[5];
	uint32_t flags[3];
	
	assert(size);
	assert((size & ((1<<21)-1)) == size);
	
	size_t src_va = src_bo->va + src_offset;
	size_t dst_va = dst_bo->va + dst_offset;
	
	buf[cdw++] = PKT3C(PKT3_CP_DMA, 4, 0);
	buf[cdw++] = src_va & 0xFFFFFFFF;
	buf[cdw++] = (sync_flag ? PKT3_CP_DMA_CP_SYNC : 0) | PKT3_CP_DMA_ENGINE((use_pfp_engine ? 1 : 0))| ((src_va >> 32) & 0xFFFF);
	buf[cdw++] = dst_va & 0xFFFFFFFF;
	buf[cdw++] = (dst_va >> 32) & 0xffff;
	buf[cdw++] = (raw_wait_flag ? PKT3_CP_DMA_CMD_RAW_WAIT : 0) | size;
	
	flags[0] = RADEON_CS_USE_VM;
	flags[1] = RADEON_CS_RING_COMPUTE;
	
	chunks[0].chunk_id = RADEON_CHUNK_ID_FLAGS;
	chunks[0].length_dw = 2;
	chunks[0].chunk_data =  (uint64_t)(uintptr_t)&flags[0];
	
	#define RELOC_SIZE (sizeof(struct cs_reloc_gem) / sizeof(uint32_t))

	struct compute_relocs crelocs;
	compute_init_relocs(&crelocs);

	compute_push_reloc(&crelocs, src_bo);
	compute_push_reloc(&crelocs, dst_bo);
	
	chunks[1].chunk_id = RADEON_CHUNK_ID_RELOCS;
	chunks[1].length_dw = crelocs.reloc_num*RELOC_SIZE;
	chunks[1].chunk_data =  (uint64_t)(uintptr_t)crelocs.relocs;

	chunks[2].chunk_id = RADEON_CHUNK_ID_IB;
	chunks[2].length_dw = cdw;
	chunks[2].chunk_data =  (uint64_t)(uintptr_t)&buf[0];  

	chunk_array[0] = (uint64_t)(uintptr_t)&chunks[0];
	chunk_array[1] = (uint64_t)(uintptr_t)&chunks[1];
	chunk_array[2] = (uint64_t)(uintptr_t)&chunks[2];
	
	cs.num_chunks = 3;
	cs.chunks = (uint64_t)(uintptr_t)chunk_array;
	cs.cs_id = 1;
	
	int r = drmCommandWriteRead(ctx->fd, DRM_RADEON_CS, &cs, sizeof(struct drm_radeon_cs));
	
	if (sync_flag)
	{
		compute_bo_wait(dst_bo);
		compute_bo_wait(src_bo);
	}
	
	free(crelocs.relocs);
	
	return r;
}

int compute_send_async_dma_req(struct compute_context* ctx, struct gpu_buffer* dst_bo, size_t dst_offset, struct gpu_buffer* src_bo, size_t src_offset, size_t size)
{
	struct drm_radeon_cs cs;
	unsigned buf[64];
	int cdw = 0;
	uint64_t chunk_array[5];
	struct drm_radeon_cs_chunk chunks[5];
	uint32_t flags[3];
	
	assert(size);
	assert((size & ((1<<21)-1)) == size);
	
	size_t src_va = src_bo->va + src_offset;
	size_t dst_va = dst_bo->va + dst_offset;
	
	assert((src_va >> 40) == 0);
	assert((dst_va >> 40) == 0);
	
	if ((src_va & 3) == 0 && (dst_va & 3) == 0)
	{
		buf[cdw++] = DMA_PACKET(DMA_PACKET_COPY, 0x00/*DW aligned L2L*/, size/4);
	}
	else
	{
		buf[cdw++] = DMA_PACKET(DMA_PACKET_COPY, 0x40/*byte aligned L2L*/, size);
	}
	
	buf[cdw++] = dst_va & 0xFFFFFFFF;
	buf[cdw++] = src_va & 0xFFFFFFFF;
	buf[cdw++] = ((dst_va >> 32) & 0xFF) | (0/*swap*/ << 8);
	buf[cdw++] = ((src_va >> 32) & 0xFF) | (0/*swap*/ << 8);
	
	flags[0] = RADEON_CS_USE_VM;
	flags[1] = RADEON_CS_RING_DMA;
	
	chunks[0].chunk_id = RADEON_CHUNK_ID_FLAGS;
	chunks[0].length_dw = 2;
	chunks[0].chunk_data =  (uint64_t)(uintptr_t)&flags[0];
	
	#define RELOC_SIZE (sizeof(struct cs_reloc_gem) / sizeof(uint32_t))
	
	struct compute_relocs crelocs;
	compute_init_relocs(&crelocs);
	
	compute_push_reloc(&crelocs, src_bo);
	compute_push_reloc(&crelocs, dst_bo);
	
	chunks[1].chunk_id = RADEON_CHUNK_ID_RELOCS;
	chunks[1].length_dw = crelocs.reloc_num*RELOC_SIZE;
	chunks[1].chunk_data =  (uint64_t)(uintptr_t)crelocs.relocs;

	chunks[2].chunk_id = RADEON_CHUNK_ID_IB;
	chunks[2].length_dw = cdw;
	chunks[2].chunk_data =  (uint64_t)(uintptr_t)&buf[0];  

	chunk_array[0] = (uint64_t)(uintptr_t)&chunks[0];
	chunk_array[1] = (uint64_t)(uintptr_t)&chunks[1];
	chunk_array[2] = (uint64_t)(uintptr_t)&chunks[2];
	
	cs.num_chunks = 3;
	cs.chunks = (uint64_t)(uintptr_t)chunk_array;
	cs.cs_id = 1;
	
	int r = drmCommandWriteRead(ctx->fd, DRM_RADEON_CS, &cs, sizeof(struct drm_radeon_cs));
	
	return r;
}

int compute_send_dma_fence(struct compute_context* ctx, struct gpu_buffer* bo)
{
	int r = 0;
	
	/*
	struct drm_radeon_cs cs;
	unsigned buf[64];
	int cdw = 0;
	uint64_t chunk_array[5];
	struct drm_radeon_cs_chunk chunks[5];
	uint32_t flags[3];
	int i;
	
	buf[cdw++] = DMA_PACKET(DMA_PACKET_FENCE, 0, 0);
	buf[cdw++] = bo->va & 0xFFFFFFFF;
	buf[cdw++] = (bo->va >> 32) & 0xFF;
	
	flags[0] = RADEON_CS_USE_VM;
	flags[1] = RADEON_CS_RING_DMA;
	
	chunks[0].chunk_id = RADEON_CHUNK_ID_FLAGS;
	chunks[0].length_dw = 2;
	chunks[0].chunk_data =  (uint64_t)(uintptr_t)&flags[0];
	
	#define RELOC_SIZE (sizeof(struct cs_reloc_gem) / sizeof(uint32_t))
	
	struct cs_reloc_gem* relocs = calloc(1, sizeof(struct cs_reloc_gem));

	compute_set_reloc(&relocs[0], bo);
	
	chunks[1].chunk_id = RADEON_CHUNK_ID_RELOCS;
	chunks[1].length_dw = 1*RELOC_SIZE;
	chunks[1].chunk_data =  (uint64_t)(uintptr_t)relocs;

	chunks[2].chunk_id = RADEON_CHUNK_ID_IB;
	chunks[2].length_dw = cdw;
	chunks[2].chunk_data =  (uint64_t)(uintptr_t)&buf[0];  

	chunk_array[0] = (uint64_t)(uintptr_t)&chunks[0];
	chunk_array[1] = (uint64_t)(uintptr_t)&chunks[1];
	chunk_array[2] = (uint64_t)(uintptr_t)&chunks[2];
	
	cs.num_chunks = 3;
	cs.chunks = (uint64_t)(uintptr_t)chunk_array;
	cs.cs_id = 1;
	
	
	r = drmCommandWriteRead(ctx->fd, DRM_RADEON_CS, &cs, sizeof(struct drm_radeon_cs));
	
	printf("%s\n", strerror(-r));
	*/
	
	compute_bo_wait(bo);
	
	return r;
}

void compute_free_gpu_buffer(struct gpu_buffer* bo)
{
	struct drm_gem_close args;
	
	if (bo->va)
	{
		compute_pool_free(bo->ctx, bo);
		compute_vm_unmap(bo->ctx, bo->va, bo->handle, 0);
	}
	
	memset(&args, 0, sizeof(args));
	args.handle = bo->handle;
	drmIoctl(bo->ctx->fd, DRM_IOCTL_GEM_CLOSE, &args); 
	free(bo);
}

static struct gpu_buffer* compute_alloc_fragmented_buffer(struct compute_context* ctx, size_t whole_size, int domain, int alignment)
{
	unsigned i;
	size_t fragment_num = (whole_size+FRAGMENT_SIZE-1) / FRAGMENT_SIZE;
	size_t size_alloced = 0;
	struct gpu_buffer* buf = calloc(fragment_num, sizeof(struct gpu_buffer));
	
	for (i = 0; i < fragment_num; i++)
	{
		size_t size;
		struct drm_radeon_gem_create args;
		
		if (size_alloced + FRAGMENT_SIZE <= whole_size)
		{
			size = FRAGMENT_SIZE;
		}
		else
		{
			size = whole_size-size_alloced;
		}
		
		memset(&args, 0, sizeof(args));
		args.size = size;
		args.alignment = alignment;
		args.initial_domain = domain;
		
		if (drmCommandWriteRead(ctx->fd, DRM_RADEON_GEM_CREATE, &args, sizeof(args)))
		{
			fprintf(stderr, "radeon: Failed to allocate a buffer:\n");
			fprintf(stderr, "radeon:    size      : %ld bytes\n", size);
			fprintf(stderr, "radeon:    alignment : %d bytes\n", alignment);
			fprintf(stderr, "radeon:    domains   : %d\n", domain);
			return NULL;
		}
		
// 		fprintf(stderr, "handle: %i\n", args.handle);
		
		buf[i].fragment_number = (i == 0) ? fragment_num : 0;
		buf[i].ctx = ctx;
		buf[i].alignment = args.alignment;
		buf[i].handle = args.handle;
		buf[i].domain = args.initial_domain;
		buf[i].flags = 0;
		buf[i].size = size;
		buf[i].va_size = ((int)((size + 4095) / 4096)) * 4096;
		size_alloced += size;
	}
	
	assert(whole_size == size_alloced);
	
	return buf;
}

struct gpu_buffer* compute_alloc_gpu_buffer(struct compute_context* ctx, size_t size, int domain, int alignment)
{
// 	struct drm_radeon_gem_create args;
// 	struct gpu_buffer* buf = calloc(1, sizeof(struct gpu_buffer));
// 	
// 	memset(&args, 0, sizeof(args));
// 	args.size = size;
// 	args.alignment = alignment;
// 	args.initial_domain = domain;
// 	
// 	if (drmCommandWriteRead(ctx->fd, DRM_RADEON_GEM_CREATE, &args, sizeof(args)))
// 	{
// 		fprintf(stderr, "radeon: Failed to allocate a buffer:\n");
// 		fprintf(stderr, "radeon:    size      : %d bytes\n", size);
// 		fprintf(stderr, "radeon:    alignment : %d bytes\n", alignment);
// 		fprintf(stderr, "radeon:    domains   : %d\n", domain);
// 		return NULL;
// 	}
// 	
// 	fprintf(stderr, "handle: %i\n", args.handle);
// 	
// 	buf->ctx = ctx;
// 	buf->alignment = args.alignment;
// 	buf->handle = args.handle;
// 	buf->domain = args.initial_domain;
// 	buf->flags = 0;
// 	buf->size = size;
// 	buf->fragment_number = 1;
// 	
// 	buf->va_size = ((int)((size + 4095) / 4096)) * 4096;
	
	struct gpu_buffer* buf = compute_alloc_fragmented_buffer(ctx, size, domain, alignment);
	
	compute_pool_alloc(ctx, buf);

	if (compute_vm_map(ctx, buf, 0, RADEON_VM_PAGE_SNOOPED))
	{
		compute_pool_free(ctx, buf);
		buf->va = 0;
		compute_free_gpu_buffer(buf);
		return NULL;
	}
	
	return buf;
}

static int compute_bo_wait_unfragmented(struct gpu_buffer *boi)
{
	struct drm_radeon_gem_wait_idle args;
	int ret;
	
	/* Zero out args to make valgrind happy */
	memset(&args, 0, sizeof(args));
	args.handle = boi->handle;
	do {
		ret = drmCommandWriteRead(boi->ctx->fd, DRM_RADEON_GEM_WAIT_IDLE, &args, sizeof(args));
	} while (ret == -EBUSY);
	return ret;
}

int compute_bo_wait(struct gpu_buffer *boi)
{
	unsigned i;
	
	for (i = 0; i < boi->fragment_number; i++)
	{
		int ret = compute_bo_wait_unfragmented(boi + i);
		
		if (ret)
		{
			return ret;
		}
	}
	
	return 0;
}

int compute_flush_caches(const struct compute_context* ctx)
{
	struct drm_radeon_cs cs;
	unsigned buf[1024];
	int cdw = 0;
	uint64_t chunk_array[5];
	struct drm_radeon_cs_chunk chunks[5];
	uint32_t flags[3];

	buf[cdw++] = PKT3C(PKT3_SURFACE_SYNC, 3, 0);
	buf[cdw++] = S_0085F0_TCL1_ACTION_ENA(1) |
							S_0085F0_SH_ICACHE_ACTION_ENA(1) |
							S_0085F0_SH_KCACHE_ACTION_ENA(1) |
							S_0085F0_TC_ACTION_ENA(1);

	buf[cdw++] = 0xffffffff;
	buf[cdw++] = 0;
	buf[cdw++] = 0xA;

	flags[0] = RADEON_CS_USE_VM;
	flags[1] = RADEON_CS_RING_COMPUTE;
	
	chunks[0].chunk_id = RADEON_CHUNK_ID_FLAGS;
	chunks[0].length_dw = 2;
	chunks[0].chunk_data =  (uint64_t)(uintptr_t)&flags[0];

	#define RELOC_SIZE (sizeof(struct cs_reloc_gem) / sizeof(uint32_t))
	
	struct compute_relocs crelocs;
	compute_create_reloc_table(ctx, &crelocs);
	
	chunks[1].chunk_id = RADEON_CHUNK_ID_RELOCS;
	chunks[1].length_dw = crelocs.reloc_num*RELOC_SIZE;
	chunks[1].chunk_data =  (uint64_t)(uintptr_t)crelocs.relocs;

	chunks[2].chunk_id = RADEON_CHUNK_ID_IB;
	chunks[2].length_dw = cdw;
	chunks[2].chunk_data =  (uint64_t)(uintptr_t)&buf[0];  

// 	printf("cdw: %i\n", cdw);

	chunk_array[0] = (uint64_t)(uintptr_t)&chunks[0];
	chunk_array[1] = (uint64_t)(uintptr_t)&chunks[1];
	chunk_array[2] = (uint64_t)(uintptr_t)&chunks[2];
	
	cs.num_chunks = 3;
	cs.chunks = (uint64_t)(uintptr_t)chunk_array;
	cs.cs_id = 1;
	
	int r = drmCommandWriteRead(ctx->fd, DRM_RADEON_CS, &cs, sizeof(struct drm_radeon_cs));

	return r;
}

int compute_emit_compute_state(const struct compute_context* ctx, const struct compute_state* state)
{
	struct compute_relocs crelocs;
	
	compute_create_reloc_table(ctx, &crelocs);
	
	int ret = compute_emit_compute_state_manual_relocs(ctx, state, crelocs);
	
	free(crelocs.relocs);

	return ret;
}

int compute_emit_compute_state_manual_relocs(const struct compute_context* ctx, const struct compute_state* state, struct compute_relocs crelocs)
{
	struct drm_radeon_cs cs;
	int i, r;
	unsigned buf[1024];
	int cdw = 0;
	uint64_t chunk_array[5];
	struct drm_radeon_cs_chunk chunks[5];
	uint32_t flags[3];
	
	set_compute_reg(R_00B804_COMPUTE_DIM_X,         state->dim[0]); ///global_size/local_size
	set_compute_reg(R_00B808_COMPUTE_DIM_Y,         state->dim[1]);
	set_compute_reg(R_00B80C_COMPUTE_DIM_Z,         state->dim[2]);
	set_compute_reg(R_00B810_COMPUTE_START_X,       state->start[0]); ///UNKNOWN kind of offset
	set_compute_reg(R_00B814_COMPUTE_START_Y,       state->start[1]);
	set_compute_reg(R_00B818_COMPUTE_START_Z,       state->start[2]);
	
	set_compute_reg(R_00B81C_COMPUTE_NUM_THREAD_X,  S_00B81C_NUM_THREAD_FULL(state->num_thread[0])); ///local_size
	set_compute_reg(R_00B820_COMPUTE_NUM_THREAD_Y,  S_00B820_NUM_THREAD_FULL(state->num_thread[1]));
	set_compute_reg(R_00B824_COMPUTE_NUM_THREAD_Z,  S_00B824_NUM_THREAD_FULL(state->num_thread[2]));
	
	set_compute_reg(R_00B82C_COMPUTE_MAX_WAVE_ID,   S_00B82C_MAX_WAVE_ID(0xF00)); ///WTF is this field?
	
	set_compute_reg(R_00B830_COMPUTE_PGM_LO,        state->binary->va >> 8); ///compute code start address
	set_compute_reg(R_00B834_COMPUTE_PGM_HI,        state->binary->va >> 40);
	
	set_compute_reg(R_00B848_COMPUTE_PGM_RSRC1,
		S_00B848_VGPRS(state->vgpr_num) |  S_00B848_SGPRS(state->sgpr_num) |  S_00B848_PRIORITY(state->priority) |
		S_00B848_FLOAT_MODE(0) | S_00B848_PRIV(state->priv_mode) | S_00B848_DX10_CLAMP(0) |
		S_00B848_DEBUG_MODE(state->debug_mode) | S_00B848_IEEE_MODE(state->ieee_mode)
	);
	
	///it was really elusive, took random bit flips to find it!!!
	#define S_00B84C_TRAP_EN(x) (((x) & 0x1) << 6)
	#define G_00B84C_TRAP_EN(x) (((x) >> 6) & 0x1)

	set_compute_reg(R_00B84C_COMPUTE_PGM_RSRC2,
		S_00B84C_SCRATCH_EN(state->scratch_en) | S_00B84C_USER_SGPR(state->user_data_length) |
		S_00B84C_TRAP_EN(state->trap_en) |
		S_00B84C_TGID_X_EN(1) | S_00B84C_TGID_Y_EN(1) | S_00B84C_TGID_Z_EN(1) |
		S_00B84C_TG_SIZE_EN(1) |
		S_00B84C_TIDIG_COMP_CNT(0) |
		S_00B84C_LDS_SIZE(state->lds_size) |
		S_00B84C_EXCP_EN(state->excp_en)
	);
	
	set_compute_reg(R_00B854_COMPUTE_RESOURCE_LIMITS,
		S_00B854_WAVES_PER_SH(state->waves_per_sh) | S_00B854_TG_PER_CU(state->thread_groups_per_cu) |
		S_00B854_LOCK_THRESHOLD(state->lock_threshold) | S_00B854_SIMD_DEST_CNTL(state->simd_dest_cntl) 
	);
	
	///Activate Compute Units:
	set_compute_reg(R_00B858_COMPUTE_STATIC_THREAD_MGMT_SE0,
		S_00B858_SH0_CU_EN(state->se0_sh0_cu_en) | S_00B858_SH1_CU_EN(state->se0_sh1_cu_en)
	);
	
	set_compute_reg(R_00B85C_COMPUTE_STATIC_THREAD_MGMT_SE1,
		S_00B85C_SH0_CU_EN(state->se1_sh0_cu_en) | S_00B85C_SH1_CU_EN(state->se1_sh1_cu_en)
	);
	
	
	if (state->user_data_length)
	{
		buf[cdw++] = PKT3C(PKT3_SET_SH_REG, state->user_data_length, 0);
		buf[cdw++] = (R_00B900_COMPUTE_USER_DATA_0 - SI_SH_REG_OFFSET) >> 2;

		for (i = 0; i < state->user_data_length; i++)
		{
			buf[cdw++] = state->user_data[i];
		}
	}


	///This is a cache flush
	buf[cdw++] = PKT3C(PKT3_SURFACE_SYNC, 3, 0);
	buf[cdw++] = S_0085F0_TCL1_ACTION_ENA(1) |
							S_0085F0_SH_ICACHE_ACTION_ENA(1) |
							S_0085F0_SH_KCACHE_ACTION_ENA(1) |
							S_0085F0_TC_ACTION_ENA(1);

	buf[cdw++] = 0xffffffff;
	buf[cdw++] = 0;
	buf[cdw++] = 0xA;
	///end cache flush
	
	///launch compute code
	set_compute_reg(R_00B800_COMPUTE_DISPATCH_INITIATOR,
		S_00B800_COMPUTE_SHADER_EN(1) | S_00B800_PARTIAL_TG_EN(0) |
		S_00B800_FORCE_START_AT_000(0) | S_00B800_ORDERED_APPEND_ENBL(0) 
	);
	
	set_compute_reg(R_00B800_COMPUTE_DISPATCH_INITIATOR, 0);

	flags[0] = RADEON_CS_USE_VM;
	flags[1] = RADEON_CS_RING_COMPUTE;
	
	chunks[0].chunk_id = RADEON_CHUNK_ID_FLAGS;
	chunks[0].length_dw = 2;
	chunks[0].chunk_data =  (uint64_t)(uintptr_t)&flags[0];

	#define RELOC_SIZE (sizeof(struct cs_reloc_gem) / sizeof(uint32_t))
		
	chunks[1].chunk_id = RADEON_CHUNK_ID_RELOCS;
	chunks[1].length_dw = crelocs.reloc_num*RELOC_SIZE;
	chunks[1].chunk_data =  (uint64_t)(uintptr_t)crelocs.relocs;

	chunks[2].chunk_id = RADEON_CHUNK_ID_IB;
	chunks[2].length_dw = cdw;
	chunks[2].chunk_data =  (uint64_t)(uintptr_t)&buf[0];  

//   printf("cdw: %i\n", cdw);

	chunk_array[0] = (uint64_t)(uintptr_t)&chunks[0];
	chunk_array[1] = (uint64_t)(uintptr_t)&chunks[1];
	chunk_array[2] = (uint64_t)(uintptr_t)&chunks[2];
	
	cs.num_chunks = 3;
	cs.chunks = (uint64_t)(uintptr_t)chunk_array;
	cs.cs_id = state->id;
	
	r = drmCommandWriteRead(ctx->fd, DRM_RADEON_CS, &cs, sizeof(struct drm_radeon_cs));

//   printf("ret:%i\n", r);
	
// 	compute_bo_wait(state->binary); ///to see if it hangs
	
	return r;
}

static int compute_copy_to_gpu_unfragmented(struct gpu_buffer* bo, size_t gpu_offset, const void* src, size_t size)
{
	struct drm_radeon_gem_mmap args;
	int r;
	void* ptr;
	
	if (size > bo->size + gpu_offset)
	{
		return -1;
	}
	
	memset(&args, 0, sizeof(args));
	
	args.handle = bo->handle;
	args.offset = gpu_offset;
	args.size = (uint64_t)size;
	r = drmCommandWriteRead(bo->ctx->fd,
													DRM_RADEON_GEM_MMAP,
													&args,
													sizeof(args));
	if (r)
	{
		fprintf(stderr, "error mapping %p 0x%08X (error = %d)\n", bo, bo->handle, r);
		return -2;
	}
	
	ptr = mmap(0, args.size, PROT_READ|PROT_WRITE, MAP_SHARED, bo->ctx->fd, args.addr_ptr);

	if (ptr == MAP_FAILED)
	{
		fprintf(stderr, "mmap failed: %s\n", strerror(errno));
			return -3;
	}

	memcpy(ptr, src, size);
	munmap(ptr, args.size);

	return 0;
}

int compute_copy_to_gpu(struct gpu_buffer* bo, size_t gpu_offset, const void* src_, size_t size)
{
	const char* src = (const char*)src_;
	size_t start_gpu = gpu_offset;
	size_t start_fragment = gpu_offset / FRAGMENT_SIZE;
	unsigned i;
	
	assert(start_fragment < bo->fragment_number);
	
	for (i = start_fragment; i < bo->fragment_number; i++)
	{
		size_t local_size;
		size_t local_offset;
		size_t size_left = size - start_gpu;
		
		if (size_left < FRAGMENT_SIZE)
		{
			local_size = size_left;
		}
		else
		{
			local_size = FRAGMENT_SIZE;
		}
		
		if (local_size == 0)
		{
			break;
		}
		
		local_offset = start_gpu % FRAGMENT_SIZE;
		local_size -= local_offset;
		
		int ret = compute_copy_to_gpu_unfragmented(&bo[i], local_offset, src, local_size);
		
		if (ret)
		{
			return ret;
		}
		
		src += local_size;
		start_gpu += local_size;
	}
	
	assert(start_gpu = size);
	
	return 0;
}

int compute_copy_from_gpu_unfragmented(struct gpu_buffer* bo, size_t gpu_offset, void* dst, size_t size)
{
	struct drm_radeon_gem_mmap args;
	int r;
	void* ptr;
	
	if (size > bo->size + gpu_offset)
	{
		return -1;
	}
	
	memset(&args, 0, sizeof(args));
	
	args.handle = bo->handle;
	args.offset = gpu_offset;
	args.size = (uint64_t)size;
	r = drmCommandWriteRead(bo->ctx->fd,
													DRM_RADEON_GEM_MMAP,
													&args,
													sizeof(args));
	if (r)
	{
		fprintf(stderr, "error mapping %p 0x%08X (error = %d)\n", bo, bo->handle, r);
		return -2;
	}
	
	ptr = mmap(0, args.size, PROT_READ|PROT_WRITE, MAP_SHARED, bo->ctx->fd, args.addr_ptr);

	if (ptr == MAP_FAILED)
	{
		fprintf(stderr, "mmap failed: %s\n", strerror(errno));
			return -3;
	}

	memcpy(dst, ptr, size);
	munmap(ptr, args.size);

	return 0;
}

int compute_copy_from_gpu(struct gpu_buffer* bo, size_t gpu_offset, void* dst_, size_t size)
{
	char* dst = (char*)dst_;
	size_t start_gpu = gpu_offset;
	size_t start_fragment = gpu_offset / FRAGMENT_SIZE;
	unsigned i;
	
	assert(start_fragment < bo->fragment_number);
	
	for (i = start_fragment; i < bo->fragment_number; i++)
	{
		size_t local_size;
		size_t local_offset;
		size_t size_left = size - start_gpu;
		
		if (size_left < FRAGMENT_SIZE)
		{
			local_size = size_left;
		}
		else
		{
			local_size = FRAGMENT_SIZE;
		}
		
		if (local_size == 0)
		{
			break;
		}
		
		local_offset = start_gpu % FRAGMENT_SIZE;
		local_size -= local_offset;
		
		int ret = compute_copy_from_gpu_unfragmented(&bo[i], local_offset, dst, local_size);
		
		if (ret)
		{
			return ret;
		}
		
		dst += local_size;
		start_gpu += local_size;
	}
	
	assert(start_gpu = size);
	
	return 0;
}
