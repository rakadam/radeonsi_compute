#ifndef _CODE_HELPER_H_
#define _CODE_HELPER_H_

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

void s_mov_b64(unsigned *&p, int sdst, int src)
{
  p[0] = 0xBE800000;
  
  unsigned ssrc0 = src;
  unsigned op = 4; //MOV_B32
  
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

void sop1(unsigned *&p, unsigned op, int vdst, int src0)
{
	p[0] = 0x7E000000;

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

void s_trap(unsigned *&p, uint8_t trapID)
{
	p[0] = 0xBF800000 | (18 << 16) | trapID;
	p++;
}

void s_nop(unsigned *&p)
{
	p[0] = 0xBF800000;
	p++;
}

void s_cbranch_scc0(unsigned *&p, int16_t imm)
{
 p[0] = 0xBF800000 | (4 << 16) | uint32_t(imm) & 0xFFFF;
 p++;
}

void s_cbranch_execz(unsigned *&p, int16_t imm)
{
 p[0] = 0xBF800000 | (8 << 16) | uint32_t(imm) & 0xFFFF;
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

void s_and_b32(unsigned *&p, unsigned sdst, unsigned ssrc1, unsigned ssrc0)
{
 sop2(p, 14, sdst, ssrc1, ssrc0);
}

void s_and_b64(unsigned *&p, unsigned sdst, unsigned ssrc1, unsigned ssrc0)
{
 sop2(p, 15, sdst, ssrc1, ssrc0);
}

void s_rfe_b64(unsigned *&p)
{
	p[0] = 0xBE800000 | (34 << 8);
	p++;
}

void v_cvt_i32_f32(unsigned *&p, unsigned vdst, unsigned src0)
{
	sop1(p, 8, vdst, src0);
}

void v_cvt_f32_i32(unsigned *&p, unsigned vdst, unsigned src0)
{
	sop1(p, 5, vdst, src0);
}

void vop3a(unsigned *&p, unsigned vdst, unsigned abs, unsigned clamp, unsigned op, unsigned src0, unsigned src1, unsigned src2, unsigned omod, unsigned neg)
{
	p[0] = 0xD0000000;
	p[1] = 0x00000000;
	
	p[0] |= vdst << 0;
	p[0] |= abs << 8;
	p[0] |= clamp << 11;
	p[0] |= op << 17;
	
	p[1] |= src0 << 0;
	p[1] |= src1 << 9;
	p[1] |= src2 << 18;
	p[1] |= omod << 27;
	p[1] |= neg << 29;

	p+=2;
}

void v_mul_lo_i32(unsigned *&p, unsigned vdst, unsigned src0, unsigned src1)
{
	vop3a(p, vdst, 0, 0, 363, src0, src1, 0, 0, 0);
}

void v_cmp_lt_f32(unsigned *&p, unsigned vsrc1, unsigned src0)
{
	unsigned op = 0x01; //cmp_lt
	
	p[0] = 0x7C000000;
	
	p[0] |= src0 << 0;
	p[0] |= vsrc1 << 8;
	p[0] |= op << 17;
	
	p++;
}

void v_cmpx_lt_f32(unsigned *&p, unsigned vsrc1, unsigned src0)
{
	unsigned op = 0x11; //cmp_lt
	
	p[0] = 0x7C000000;
	
	p[0] |= src0 << 0;
	p[0] |= vsrc1 << 8;
	p[0] |= op << 17;
	
	p++;
}

void v_cmp_gt_f32(unsigned *&p, unsigned vsrc1, unsigned src0)
{
	unsigned op = 0x04; //cmp_lt
	
	p[0] = 0x7C000000;
	
	p[0] |= src0 << 0;
	p[0] |= vsrc1 << 8;
	p[0] |= op << 17;
	
	p++;
}

void v_cmpx_gt_f32(unsigned *&p, unsigned vsrc1, unsigned src0)
{
	unsigned op = 0x14; //cmp_lt
	
	p[0] = 0x7C000000;
	
	p[0] |= src0 << 0;
	p[0] |= vsrc1 << 8;
	p[0] |= op << 17;
	
	p++;
}

#endif
