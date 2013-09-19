#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <map>
#include <vector>
#include <set>
#include <sys/time.h>

extern "C" {
#include "computesi.h"
};


using namespace std;
unsigned floatconv(float f)
{
 union 
 {
   float f;
   uint32_t u;
 } conv;

 conv.f = f;

 return conv.u;
}

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

#define BUFFER_ATOMIC_ADD 50

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

void mubuf(unsigned *&p, int soffset, int tfe, int slc, int srsrc, int vdata, int vaddr, int op, int lds, int addr64, int glc, int idxen, int offen, int offset)
{
  p[0] = 0xE0000000;
  p[1] = 0;

  p[0] |= offset;
  p[0] |= offen << 12;
  p[0] |= idxen << 13;
  p[0] |= glc << 14;
  p[0] |= addr64 << 15;
  p[0] |= lds << 16;
//r
  p[0] |= op << 18;

  p[1] |= vaddr;
  p[1] |= vdata << 8;
  p[1] |= srsrc << 16;
//r
  p[1] |= slc << 22;
  p[1] |= tfe << 23;
  p[1] |= soffset << 24;

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
  unsigned op = 3; //MOV_B32
  
  p[0] |= ssrc0;
  p[0] |= op << 8;
  p[0] |= sdst << 16;
  
  p[1] = imm;
  
  p += 2;
}

void s_mov_b32(unsigned *&p, int sdst, int src)
{
  p[0] = 0xBE800000;
  
  unsigned ssrc0 = src;
  unsigned op = 3; //MOV_B32
  
  p[0] |= ssrc0;
  p[0] |= op << 8;
  p[0] |= sdst << 16;
  
  p += 1;
}

void s_getreg_b32(unsigned *&p, int sdst, unsigned size, unsigned offset, unsigned hwregid)
{
  p[0] = 0xB0000000;
  
  unsigned op = 18;
  
  p[0] |= size << 11| offset << 6 | hwregid;
  p[0] |= op << 23;
  p[0] |= sdst << 16;
  
  p += 1;
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

void v_sin_f32(unsigned *&p, int vdst, int src0)
{
  p[0] = 0x7E000000;
  
  unsigned op = 53;
  
  p[0] |= src0;
  p[0] |= op << 9;
  p[0] |= vdst << 17;
  
  p++;
}

void v_rcp_f32(unsigned *&p, int vdst, int src0)
{
  p[0] = 0x7E000000;
  
  unsigned op = 42;
  
  p[0] |= src0;
  p[0] |= op << 9;
  p[0] |= vdst << 17;
  
  p++;
}

void v_rcp_f64(unsigned *&p, int vdst, int src0)
{
  p[0] = 0x7E000000;
  
  unsigned op = 47;
  
  p[0] |= src0;
  p[0] |= op << 9;
  p[0] |= vdst << 17;
  
  p++;
}

void v_sqrt_f32(unsigned *&p, int vdst, int src0)
{
  p[0] = 0x7E000000;
  
  unsigned op = 51;
  
  p[0] |= src0;
  p[0] |= op << 9;
  p[0] |= vdst << 17;
  
  p++;
}

void v_sqrt_f64(unsigned *&p, int vdst, int src0)
{
  p[0] = 0x7E000000;
  
  unsigned op = 52;
  
  p[0] |= src0;
  p[0] |= op << 9;
  p[0] |= vdst << 17;
  
  p++;
}

void v_bfrev_b32(unsigned *&p, int vdst, int src0)
{
  p[0] = 0x7E000000;
  
  unsigned op = 56;
  
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

void s_getpc_b64(unsigned*&p, unsigned sdst)
{
  unsigned op = 31;
  p[0] = 0xBE800000 | op << 8 | sdst << 16;
  p++;
}

void s_swappc_b64(unsigned*&p, unsigned sdst, unsigned ssrc0)
{
  unsigned op = 33;
  p[0] = 0xBE800000 | op << 8 | sdst << 16 | ssrc0;
  p++;
}

void s_jump(unsigned *&p, int rel)
{
  p[0] = 0xBF800000 | (0x2 << 16) | rel;
  p++;
}

void smrd(unsigned *&p, unsigned op, unsigned sdst, unsigned sbase, unsigned imm, unsigned offset)
{
  p[0] = 0xC0000000 | (op << 22) | (sdst << 15) | (sbase << 9) | (imm << 8) | (offset << 0);
  p++;
}

void s_memtime(unsigned *&p, unsigned sdst)
{
  smrd(p, 30, sdst, 0, 0, 0);
}

void v_mul_i32_i24(unsigned *&p, unsigned vdst, unsigned vsrc1, unsigned src0)
{
  unsigned op = 9;

  p[0] = 0;
  p[0] |= src0;
  p[0] |= vsrc1 << 9;
  p[0] |= vdst << 17;
  p[0] |= op << 25;
  p++;
}

void v_mul_f32(unsigned *&p, unsigned vdst, unsigned vsrc1, unsigned src0)
{
  unsigned op = 8;

  p[0] = 0;
  p[0] |= src0;
  p[0] |= vsrc1 << 9;
  p[0] |= vdst << 17;
  p[0] |= op << 25;
  p++;
}

void v_add_f32(unsigned *&p, unsigned vdst, unsigned vsrc1, unsigned src0)
{
  unsigned op = 3;

  p[0] = 0;
  p[0] |= src0;
  p[0] |= vsrc1 << 9;
  p[0] |= vdst << 17;
  p[0] |= op << 25;
  p++;
}

void v_mul_i32_i24_imm32(unsigned *&p, unsigned vdst, unsigned vsrc1, unsigned imm32)
{
  v_mul_i32_i24(p, vdst, vsrc1, 255);
  p[0] = imm32;
  p++;
}

void v_mul_f32_imm32(unsigned *&p, unsigned vdst, unsigned vsrc1, float f)
{
  v_mul_f32(p, vdst, vsrc1, 255);
  p[0] = floatconv(f);
  p++;
}

void v_add_f32_imm32(unsigned *&p, unsigned vdst, unsigned vsrc1, float f)
{
  v_add_f32(p, vdst, vsrc1, 255);
  p[0] = floatconv(f);
  p++;
}

void v_add_i32(unsigned *&p, unsigned vdst, unsigned vsrc1, unsigned src0)
{
  unsigned op = 37;

  p[0] = 0;
  p[0] |= src0;
  p[0] |= vsrc1 << 9;
  p[0] |= vdst << 17;
  p[0] |= op << 25;
  p++;
}

void v_add_i32_imm32(unsigned *&p, unsigned vdst, unsigned vsrc1, unsigned imm32)
{
  v_add_i32(p, vdst, vsrc1, 255);
  p[0] = imm32;
  p++;
}

void sopc(unsigned *&p, unsigned op, unsigned ssrc1, unsigned ssrc0)
{
 p[0] = 0xBF000000 | (ssrc0) | (ssrc1 << 8) | (op << 16);
 p++;
}

void s_cmp_lt_i32(unsigned *&p, unsigned ssrc1, unsigned ssrc0)
{
 sopc(p, 4, ssrc1, ssrc0);
}

void s_cmp_gt_i32(unsigned *&p, unsigned ssrc1, unsigned ssrc0)
{
 sopc(p, 2, ssrc1, ssrc0);
}


void s_cbranch_scc0(unsigned *&p, int16_t imm)
{
 p[0] = 0xBF800000 | (4 << 16) | uint32_t(imm) & 0xFFFF;
 p++;
}

void s_branch(unsigned *&p, int16_t imm)
{
 p[0] = 0xBF800000 | (2 << 16)| uint32_t(imm) & 0xFFFF;
 p++;
}

void sop2(unsigned *&p, unsigned op, unsigned sdst, unsigned ssrc1, unsigned ssrc0)
{
 p[0] = 0x80000000 | (op << 23) | (sdst << 16) | (ssrc1 << 8) | (ssrc0 << 0);
 p++;
}

void s_add_i32(unsigned *&p, unsigned sdst, unsigned ssrc1, unsigned ssrc0)
{
 sop2(p, 2, sdst, ssrc1, ssrc0);
}

int64_t get_time_usec()
{
    struct timeval tv;
    struct timezone tz;

    gettimeofday(&tv, &tz);

    return int64_t(tv.tv_sec) * 1000000 + int64_t(tv.tv_usec);
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
  compute_context* ctx = compute_create_context("/dev/dri/card1");
  
  int test_data_size = 1024*1024*16;
  gpu_buffer* code_bo = compute_alloc_gpu_buffer(ctx, 1024*1024*4, RADEON_DOMAIN_VRAM, 4096);
  gpu_buffer* data_bo = compute_alloc_gpu_buffer(ctx, test_data_size*4, RADEON_DOMAIN_VRAM, 4096);
  
  unsigned prog[1024*1024*1];
  
  
  for (int i = 0; i < sizeof(prog) / sizeof(prog[0]); i++)
  {
    prog[i] = 0xBF800000; //sopp: NOP
  }
  
  unsigned *p = &prog[0];
  
  buffer_resource bufres;
  
  bufres.base_addr = data_bo->va;
  bufres.stride = 4;
  bufres.num_records = 1024*100;
  bufres.dst_sel_x = 4;
  bufres.dst_sel_y = 4;
  bufres.dst_sel_z = 4;
  bufres.dst_sel_w = 4;
  bufres.num_format = 4;
  bufres.data_format = 4;
  bufres.element_size = 1;
  bufres.add_tid_en = 0;

  printf("buf addr%x, code addr: %x\n", data_bo->va, code_bo->va);
//  printf("buf addr%p\n", data_bo->va >> 11);

/*
rak_adam: bit 0-4 are the fault type (bit 0 - range, bit 1 - pde, bit 2 - valid, bit 3 - read, bit 4 - write), 19:12 is the GPU client id, bit 24 is client r/w bit (0 - read, 1 - write), and 28-25 is the vmid
rak_adam: 0x48 is the TC (texture cache)

*/
  printf("resource: %.8x %.8x %.8x %.8x\n", bufres.data[0], bufres.data[1], bufres.data[2], bufres.data[3]);
  

/*
  s_mov_imm32(p, 4, bufres.data[0]);
  s_mov_imm32(p, 5, bufres.data[1]);
  s_mov_imm32(p, 6, bufres.data[2]);
  s_mov_imm32(p, 7, bufres.data[3]);
  */
//  s_getreg_b32(p, 4, 31, 0, 4);

//  v_mov_imm32(p, 0, 0x00000006);
//  v_mov_b32(p, 0, 4);
//  s_getpc_b64(p, 4);
//  v_mov_b32(p, 0, 4);

/*  v_mov_imm32(p, 2, 0x00000002);
  v_mov_imm32(p, 3, 0x00000042);
  v_mov_imm32(p, 4, 0x00000000);
  v_mov_imm32(p, 5, 0x00000000);
  v_mov_imm32(p, 6, 0x00000000);
  v_mov_imm32(p, 7, 0x00000000);

  s_mov_imm32(p, 0, code_bo->va+(1+start+21+1)*4);
*/

  v_mov_imm32(p, 1, 0x0002);
  v_mov_imm32(p, 2, 0x0001);

  s_mov_imm32(p, 126, 0x00000001); //EXECLO
  s_mov_imm32(p, 127, 0x00000000); //EXECHI

//  s_mov_imm32(p, 0, data_bo->va);
  //s_getreg_b32(p, 4, 3, 0, 4);
  v_mov_b32(p, 1, 4);
  v_mov_b32(p, 2, 4);
  v_mul_i32_i24_imm32(p, 1, 1, 64);

/*
  mtbuf(p,
           4,//int nfmt,
           4,//int dfmt,
           TBUFFER_STORE_FORMAT_X,//int op,
           0,//int addr64,
           0,//int glc,
           1,//int idxen,
           0,//int offen,
           0,//int offset,
           128,//int soffset, set to zero
           0,//int tfe,
           0,//int slc,
           0,//int srsrc,
           2,//int vdata,
           1//int vaddr
          );
*/

  v_mov_imm32(p, 1, 0x00000000);
  v_mov_imm32(p, 2, 0x00000006);

  mubuf(p, 
    128, //int soffset, 
    0,//int tfe, 
    0,//int slc, 
    0,//int srsrc, 
    2,//int vdata, 
    1,//int vaddr,
    BUFFER_ATOMIC_ADD,//int op, 
    0, //int lds, 
    0, //int addr64, 
    1, //int glc, 
    0, //int idxen, 
    0, //int offen, 
    0 //int offset
  );
  
  s_waitcnt(p);

//  s_getreg_b32(p, 4, 31, 0, 4);
  v_mov_b32(p, 1, 4);

  s_mov_imm32(p, 6, 0x42);
  s_memtime(p, 6);
  s_waitcnt(p);
  v_mov_b32(p, 4, 6);
  v_mov_b32(p, 5, 7);

  mtbuf(p,
           4,//int nfmt,
           11,//int dfmt,
           TBUFFER_STORE_FORMAT_XY,//int op,
           0,//int addr64,
           0,//int glc,
           1,//int idxen,
           0,//int offen,
           0,//int offset,
           128,//int soffset, set to zero
           0,//int tfe,
           0,//int slc,
           0,//int srsrc,
           4,//int vdata,
           2//int vaddr
          );
  s_waitcnt(p);
////////////////////////////////////////
  s_mov_imm32(p, 126, 0xFFFFFFFF); //EXECLO
  s_mov_imm32(p, 127, 0xFFFFFFFF); //EXECHI

  
  int iternum = 1000;

  s_mov_imm32(p, 8, 0);

  v_mov_imm32(p, 4, floatconv(2));
  v_mov_imm32(p, 5, floatconv(1));

  unsigned * eleje = p;

  int ii2 = 1024*(8)*0.25;

  for (int i = 0; i < ii2; i++)
  {
		
//    s_mov_b32(p, 6, 6);
//    v_sin_f32(p, 4, 256+4);
      v_add_f32(p, 4, 4, 256+4);
//    v_add_f32_imm32(p, 4, 4, 1.1);
//    v_sqrt_f64(p, 4, 256+4);
//    v_bfrev(p, 4, 256+4);
//    v_rcp_f64(p, 4, 256+4);
//    v_mov_b32(p, 5, 256+4);
  }

  if (iternum > 1)
	{
		s_add_i32(p, 8, 8, 255); p[0] = 1; p++;
		s_cmp_lt_i32(p, 8, 255); p[0] = iternum; p++;
		s_cbranch_scc0(p, eleje-p);
	}
	
  s_mov_imm32(p, 126, 0x00000001); //EXECLO
  s_mov_imm32(p, 127, 0x00000000); //EXECHI
///////////////////////////////////////
  s_memtime(p, 6);
  s_waitcnt(p);

  s_getreg_b32(p, 8, 31, 0, 4); //hwid
  s_getreg_b32(p, 8, 31, 0, 2); //status
//   printf("getreg: %08X\n", p[-1]);

  v_mov_b32(p, 4, 6);
  v_mov_b32(p, 5, 7);
  v_mov_b32(p, 6, 4);
  v_mov_b32(p, 7, 8);
  v_add_i32_imm32(p, 2, 2, 2);

  mtbuf(p,
           4,//int nfmt,
           14,//int dfmt,
           TBUFFER_STORE_FORMAT_XYZW,//int op,
           0,//int addr64,
           0,//int glc,
           1,//int idxen,
           0,//int offen,
           0,//int offset,
           128,//int soffset, set to zero
           0,//int tfe,
           0,//int slc,
           0,//int srsrc,
           4,//int vdata,
           2//int vaddr
          );

  s_waitcnt(p);

//  prog[100] = 0xBF800000 | (0x2 << 16) | 0x0001; //sopp: JUMP next
//  prog[101] = 0xBF800000 | (0x2 << 16) | 0xFFFF; //sopp: JUMP self
  
//   prog[255] = 0xBF800000 | (0x1 << 16); //sopp: ENDPGM
  
  s_endpgm(p);
  
/*  printf("code:\n");

  for (unsigned* i = &prog[0]; i != p; i++)
  {
    printf(">%.8X\n", *i);
  }*/

/*
  for (unsigned *i = &prog[0]; i < p; i++)
  {
    printf("%.8x\n", *i);
  }
*/
  compute_copy_to_gpu(code_bo, 0, &prog[0], sizeof(prog));
  
  unsigned* test_data = new unsigned[test_data_size];
  
  for (int i = 0; i < 1024; i++)
  {
    test_data[i] = 0xDEADBEEF;
  }
  
  test_data[0] = 1;

  compute_copy_to_gpu(data_bo, 0, &test_data[0], test_data_size*4);
  
  compute_state state;
  
  printf("%lx\n", code_bo->va);
 
  state.id = 0;
  state.user_data_length = 4;
  
  state.user_data[0] = bufres.data[0];
  state.user_data[1] = bufres.data[1];
  state.user_data[2] = bufres.data[2];
  state.user_data[3] = bufres.data[3];
  
  state.dim[0] = 32;
  state.dim[1] = 1;
  state.dim[2] = 1;
  state.start[0] = 0;
  state.start[1] = 0;
  state.start[2] = 0;
  state.num_thread[0] = 64*4;
  state.num_thread[1] = 1;
  state.num_thread[2] = 1;
  
  state.sgpr_num = 11;
  state.vgpr_num = 10;
  state.priority = 0;
  state.debug_mode = 0;
	state.priv_mode = 0;
  state.ieee_mode = 0;
  state.scratch_en = 0;
  state.lds_size = 128; ///32K
  state.excp_en = 0;
  state.waves_per_sh = 0;//256 / ((state.vgpr_num+1)*4);
  state.thread_groups_per_cu = 1;
  state.lock_threshold = 0;
  state.simd_dest_cntl = 0;
  state.se0_sh0_cu_en = 0xFF;
  state.se0_sh1_cu_en = 0xFF;
  state.se1_sh0_cu_en = 0xFF;
  state.se1_sh1_cu_en = 0xFF;
  state.tmpring_waves = 0;
  state.tmpring_wavesize = 0;
  state.binary = code_bo;

  int e;

  int64_t start_time = get_time_usec();

  e = compute_emit_compute_state(ctx, &state);

  int64_t stop_time = get_time_usec();

  cout << e << " " << strerror(errno) << endl;
  
  compute_flush_caches(ctx);

  compute_copy_from_gpu(data_bo, 0, &test_data[0], test_data_size*4);
  
  for (int i = 1; i < 64*4; i++)
  {
    printf("%i : %.8x\n", i, test_data[i]);
  }


  int global_size = state.dim[0]*state.dim[1]*state.dim[2]*state.num_thread[0]*state.num_thread[1]*state.num_thread[2];
  int datalen = global_size / 64 * 6;

  std::cout << test_data[0] << " " << datalen << std::endl;

  datalen = test_data[0];

  uint64_t firststart = *(uint64_t*)&test_data[1];

  uint64_t laststop = firststart;

  for (int i = 1; i < datalen; i += 6)
  {
    uint64_t start = *(uint64_t*)&test_data[i];
    uint64_t stop = *(uint64_t*)&test_data[i+2];
    int group_id = test_data[i+4];

    firststart = std::min(firststart, start);
    laststop = std::max(laststop, stop);
  }

  std::set<std::pair<std::vector<uint32_t>, std::string> > ordered;

  for (int i = 1; i < datalen; i += 6)
  {
    uint64_t start = *(uint64_t*)&test_data[i];
    uint64_t stop = *(uint64_t*)&test_data[i+2];
    int group_id = test_data[i+4];
    uint32_t hwid = test_data[i+5];

    int cu_id = (hwid >> 8) & 15;
    int sh_id = (hwid >> 12) & 1;
    int se_id = (hwid >> 13) & 3;

    char buf[64*1024];
    sprintf(buf, "start: %10lu diff:%10lu clocks gid:%3i wave_id: %i simd_id: %i cu_id: %i, sh_id: %i se_id: %i cu_num:%3i tg_id: %i hwid:%08X\n", 
            start-firststart, stop-start, group_id, hwid & 15, (hwid >> 4) & 3, cu_id, sh_id, se_id,  cu_id+sh_id*8+se_id*16, (hwid >> 16) & 15, hwid);
    ordered.insert(make_pair(vector<uint32_t>{(hwid >> 13) & 3, (hwid >> 12) & 1, (hwid >> 8) & 15, (hwid >> 4) & 3}, std::string(buf)));
  }

  for (auto n : ordered)
  {
    cout << n.second;
  }

  cout << "run time: " << double(stop_time-start_time)/1000.0 << "ms" << endl;
  cout << "run cycles: " << double(laststop - firststart) << endl;
  cout << "Core freq: " << double(laststop - firststart)/double(stop_time-start_time) << "MHz" << endl;
  cout << double(ii2*iternum)*global_size / double(stop_time-start_time) * 1E-3 << "Giter/s" << endl;
  cout << double(laststop - firststart) / double(ii2*iternum) << " cycles / iter" << endl;
  compute_free_context(ctx);
}


