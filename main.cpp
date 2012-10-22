#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

extern "C" {
#include "computesi.h"
};


using namespace std;

enum mtbuf_op{
  TBUFFER_LOAD_FORMAT_X     = 0,
  TBUFFER_LOAD_FORMAT_XY    = 1,
  TBUFFER_LOAD_FORMAT_XYZ   = 2,
  TBUFFER_LOAD_FORMAT_XYZW  = 3,

  TBUFFER_STORE_FORMAT_X    = 4,
  TBUFFER_STORE_FORMAT_XY   = 5,
  TBUFFER_STORE_FORMAT_XYZ  = 6,
  TBUFFER_STORE_FORMAT_XYZW = 7
};

struct buffer_resource{
  buffer_resource()
  {
    data[0] = 0;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
  }
  
  union{
    struct {
      uint64_t base_addr    : 48;
      unsigned stride       : 14;
      unsigned c_swizzle    : 1;
      unsigned swizzle_en   : 1;
      unsigned num_records  : 32;
      unsigned dst_sel_x    : 3;
      unsigned dst_sel_y    : 3;
      unsigned dst_sel_z    : 3;
      unsigned dst_sel_w    : 3;
      unsigned num_format   : 3;
      unsigned data_format  : 4;
      unsigned element_size : 2;
      unsigned idx_stride   : 2;
      unsigned add_tid_en   : 1;
      unsigned reserved     : 1;
      unsigned hash_en      : 1;
      unsigned heap         : 1;
      unsigned unused       : 3;
      unsigned zero         : 2;
    } __attribute__((packed));
    
    uint32_t data[4];
  };
};

void mtbuf(unsigned *&p, int nfmt, int dfmt, int op, int addr64, int glc, int idxen, int offen, int offset, int soffset, int tfe, int slc, int srsrc, int vdata, int vaddr)
{
  p[0] = 0xE8000000;
  p[1] = 0;
  
  p[0] |= offset; //12bit
  p[0] |= offen << 12;
  p[0] |= idxen << 13;
  p[0] |= glc << 14;
  p[0] |= addr64 << 15;
  p[0] |= op << 16; //3bit
  p[0] |= dfmt << 19; //4bit
  p[0] |= nfmt << 23; //3bit

  p[1] |= vaddr; //8bit
  p[1] |= vdata << 8; //8bit
  p[1] |= srsrc << 16; //5bit
  //1bit reserved
  p[1] |= slc << 22;
  p[1] |= tfe << 23;
  p[1] |= soffset << 24; //8bit
  
  p += 2;
}

void s_load_dword(unsigned *&p, int sbase, int sdst, int offset, int imm)
{
  p[0] = 0xC0000000; //SRMD
  
  unsigned op = 0; ///S_LOAD_DWORD
  
  p[0] |= offset;
  p[0] |= imm << 8;
  p[0] |= sbase << 9;
  p[0] |= sdst<< 15;
  p[0] |= op << 22;
  
  p += 1;
}

void s_mov_imm32(unsigned *&p, int sdst, unsigned imm)
{
  p[0] = 0xBE800000;
  
  unsigned ssrc0 = 255; //literal constant
  unsigned op = 2; //MOV_B32
  
  p[0] |= ssrc0;
  p[0] |= op << 8;
  p[0] |= sdst << 16;
  
  p[1] = imm;
  
  p += 2;
}

void v_mov_b32(unsigned *&p, int vdst, int src0)
{
  p[0] = 0x7E000000;
  
  unsigned op = 1;
  
  p[0] |= src0;
  p[0] |= op << 9;
  p[0] |= vdst << 17;
  
  p++;
}

void v_mov_imm32(unsigned *&p, int vdst, unsigned imm)
{
  v_mov_b32(p, vdst, 255);
  p[0] = imm;
  p++;
}

void s_waitcnt(unsigned*&p)
{
  unsigned op = 12;
  p[0] = 0xBF800000 | op << 16;
  p++;
}

void s_endpgm(unsigned*&p)
{
  unsigned op = 1;
  p[0] = 0xBF800000 | op << 16;
  p++;
}

int main()
{
//   {
//   int fd = open("/dev/dri/card0", O_RDWR, 0);
//   
//   radeon_cs *cs;
//   radeon_cs_manager *gem = radeon_cs_manager_gem_ctor(fd);
//   assert(gem);
//   cs = radeon_cs_create(gem, RADEON_BUFFER_SIZE/4);
//   assert(cs);
//   radeon_bo_manager *bom = radeon_bo_manager_gem_ctor(fd);
// 
//   int ndw = 2;
//   radeon_bo* bo = radeon_bo_open(bom, 0, 4096, 4096, RADEON_DOMAIN_VRAM, 0);
//   
//   radeon_cs_begin(cs, ndw, __FILE__, __func__, __LINE__);
//   radeon_cs_write_reloc(cs, bo, RADEON_DOMAIN_VRAM, 0, 0);
//   radeon_cs_end(cs, __FILE__, __func__, __LINE__);
//   
//   radeon_cs_space_add_persistent_bo(cs, bo, RADEON_DOMAIN_VRAM, 0);
//   radeon_cs_space_check(cs);
// 
//   radeon_cs_emit(cs);
//   radeon_cs_erase(cs);
// 
//   return 0;
//   }
  compute_context* ctx = compute_create_context("/dev/dri/card0");
  
  gpu_buffer* code_bo = compute_alloc_gpu_buffer(ctx, 4096, RADEON_DOMAIN_VRAM, 4096);
  gpu_buffer* data_bo = compute_alloc_gpu_buffer(ctx, 4096, RADEON_DOMAIN_VRAM, 4096);
  
  unsigned prog[256];
  
  
  for (int i = 0; i < 256; i++)
  {
    prog[i] = 0xBF800000; //sopp: NOP
  }
  
  unsigned *p = &prog[2];
  
  buffer_resource bufres;
  
  bufres.base_addr = data_bo->va;
  bufres.stride = 4;
  bufres.num_records = 16;
  bufres.dst_sel_x = 4;
  bufres.dst_sel_y = 4;
  bufres.dst_sel_z = 4;
  bufres.dst_sel_w = 4;
  bufres.num_format = 4;
  bufres.data_format = 4;
  bufres.element_size = 1;
  
  printf("%.8x %.8x %.8x %.8x\n", bufres.data[0], bufres.data[1], bufres.data[2], bufres.data[3]);
  
  s_mov_imm32(p, 0, bufres.data[0]);
  s_mov_imm32(p, 1, bufres.data[1]);
  s_mov_imm32(p, 2, bufres.data[2]);
  s_mov_imm32(p, 3, bufres.data[3]);
  
  v_mov_imm32(p, 0, 0x00000000);
  v_mov_imm32(p, 1, 0x00000000);
  v_mov_imm32(p, 2, 0x00000000);
  v_mov_imm32(p, 3, 0x00000000);
  v_mov_imm32(p, 4, 0x00000000);
  v_mov_imm32(p, 5, 0x00000000);
  v_mov_imm32(p, 6, 0x00000000);
  v_mov_imm32(p, 7, 0x00000000);

  mtbuf(p,
           4,//int nfmt,
           4,//int dfmt,
           TBUFFER_STORE_FORMAT_X,//int op,
           0,//int addr64,
           0,//int glc,
           0,//int idxen,
           0,//int offen,
           0,//int offset,
           0,//int soffset,
           0,//int tfe,
           0,//int slc,
           0,//int srsrc,
           2,//int vdata,
           4//int vaddr
          );
  
  s_waitcnt(p);
  
//  prog[100] = 0xBF800000 | (0x2 << 16) | 0x0001; //sopp: JUMP next
//  prog[101] = 0xBF800000 | (0x2 << 16) | 0xFFFF; //sopp: JUMP self
  
//   prog[255] = 0xBF800000 | (0x1 << 16); //sopp: ENDPGM
  
  s_endpgm(p);
  
  compute_copy_to_gpu(code_bo, 0, &prog[0], sizeof(prog));
  
  unsigned test_data[1024];
  
  for (int i = 0; i < 1024; i++)
  {
    test_data[i] = 0xDEADBEEF;
  }
  
  compute_copy_to_gpu(data_bo, 0, &test_data[0], sizeof(test_data));
  
  compute_state state;
  
  printf("%lx\n", code_bo->va);
  
  state.id = 1;
  state.user_data_length = 4;
  
  state.user_data[0] = bufres.data[0];
  state.user_data[1] = bufres.data[1];
  state.user_data[2] = bufres.data[2];
  state.user_data[3] = bufres.data[3];
  
  state.dim[0] = 1;
  state.dim[1] = 1;
  state.dim[2] = 1;
  state.start[0] = 0;
  state.start[1] = 0;
  state.start[2] = 0;
  state.num_thread[0] = 1;
  state.num_thread[1] = 1;
  state.num_thread[2] = 1;
  
  state.sgpr_num = 16;
  state.vgpr_num = 32;
  state.priority = 0;
  state.debug_mode = 0;
  state.ieee_mode = 0;
  state.scratch_en = 0;
  state.lds_size = 0;
  state.excp_en = 0;
  state.waves_per_sh = 1;
  state.thread_groups_per_cu = 1;
  state.lock_threshold = 4;
  state.simd_dest_cntl = 0;
  state.se0_sh0_cu_en = 1;
  state.se0_sh1_cu_en = 1;
  state.se1_sh0_cu_en = 1;
  state.se1_sh1_cu_en = 1;
  state.tmpring_waves = 0;
  state.tmpring_wavesize = 0;
  state.binary = code_bo;

  int e = compute_emit_compute_state(ctx, &state);
  
  cout << e << " " << strerror(errno) << endl;
  
  compute_copy_from_gpu(data_bo, 0, &test_data[0], sizeof(test_data));
  
  for (int i = 0; i < 16; i++)
  {
    printf("%i : %.8x\n", i, test_data[i]);
  }
  
  compute_free_context(ctx);
}


