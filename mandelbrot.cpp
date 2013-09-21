#include <iostream>
#include <cstdio>
#include <string>
#include <time.h>
#include <assert.h>
#include "code_helper.h"
#include "compute_interface.hpp"

using namespace std;

struct uchar4
{
	uint8_t r, g, b, a;
}__attribute__((packed));

void imageToFile(ComputeInterface& compute, gpu_buffer* buffer, int mx, int my, std::string fname)
{
	vector<uchar4> image(mx*my);
	compute.transferFromGPU(buffer, 0, image);
	
	FILE *f = fopen(fname.c_str(), "w");
	
	fprintf(f, "P6\n%i %i\n255\n", mx, my);
	
	for (auto val : image)
	{
		cout << *(uint32_t*)&val << endl;
		fwrite(&val.r, 1, 1, f);
		fwrite(&val.g, 1, 1, f);
		fwrite(&val.b, 1, 1, f);
	}
	
	fclose(f);
}

void set_program(unsigned* p, int mx, int my)
{
	s_nop(p);
	s_nop(p);
	s_nop(p);
	s_nop(p);
	s_nop(p);
	
	v_mov_imm32(p, 8, 0xFFFFFF);
// 	v_mov_imm32(p, 8, 0x0);
	
	v_mov_b32(p, 5, 4); //s4
	v_mul_i32_i24(p, 5, 5, 255); p[0]=256; p++;
	v_add_i32(p, 6/*cx*/, 0, 256+5);
	v_mov_b32(p, 8/*cy*/, 5); //s5
	v_mul_i32_i24(p, 0, 8, 255); p[0]=mx; p++;
	v_add_i32(p, 0, 0, 256+6);
	
	///convert x and y to float
	v_cvt_f32_i32(p, 6, 256+6);
	v_cvt_f32_i32(p, 8, 256+8);
	v_add_f32(p, 6, 6, 255); p[0] = floatconv(-mx/2); p++;
	v_add_f32(p, 8, 8, 255); p[0] = floatconv(-my/2); p++;
	v_mul_f32(p, 6, 6, 255); p[0] = floatconv(1.0/float(mx/2)); p++;
	v_mul_f32(p, 8, 8, 255); p[0] = floatconv(1.0/float(my/2)); p++;
	
	//////////////////////////////////////////////////////////////////////////////
	///x:v6 y:v8 float32
	///pixelcolor:v10, 0xBBGGRR
	
	v_mul_f32(p, 6, 6, 256+6);
	v_mul_f32(p, 8, 8, 256+8);
	
 	v_add_f32(p, 10, 6, 256+8);
	
	v_sqrt_f32(p, 10, 256+10);
	
	v_mul_f32(p, 10, 10, 255); p[0] = floatconv(float(mx/2)); p++;
	
	
	///-------------------
	s_mov_b64(p, 12, 126); //SAVE exec to s12-s13
	s_mov_imm32(p, 8, 0); //s8 = 0;
	v_mov_imm32(p, 10, floatconv(0));
	
	unsigned* eleje = p; //eleje:
	
	s_nop(p);
	s_nop(p);
	s_nop(p);
	
	v_add_f32(p, 10, 10, 242); //v10 = v10 + 1;
	s_add_i32(p, 8, 8, 129); //s8 = s8 + 1;
	
	v_cmpx_gt_f32(p, 10, 255); p[0]=floatconv(-10); p++;
	s_nop(p);
	
	s_cbranch_execz(p, 3);//Exit loop if vector unit is idle
	s_cmp_lt_i32(p, 8, 255); p[0] = 10000; p++;
	s_cbranch_scc0(p, eleje-p-1); //if (s8 <= 100) goto eleje;
	s_mov_b64(p, 126, 12); //restore exec from s12-s13
	
	s_mov_imm32(p, 126, 0xFFFFFFFF);
	s_mov_imm32(p, 127, 0xFFFFFFFF);
	
	v_cvt_i32_f32(p, 10, 256+10);
	v_mov_b32(p, 10, 8);
 	//v_mul_lo_i32(p, 10, 256+10, 256+10);
	
// 	v_mov_b32(p, 8, 256+9);
// 	v_mul_i32_i24(p, 8, 8, 255); p[0]=16; p++;
	
	//////////////////////////////////////////////////////////////////////////////
	
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
		10,//int vdata,
		0//int vaddr
	);
	
  s_waitcnt(p);

	s_nop(p);
	s_nop(p);
	s_nop(p);
	s_nop(p);
	s_endpgm(p);
}

int main()
{
	int mx = 256;
	int my = 256;
	
	ComputeInterface compute("/dev/dri/card0");
	
	int code_size_max = 1024*4;
	
	gpu_buffer* program_code = compute.bufferAlloc(code_size_max);
	gpu_buffer* data = compute.bufferAlloc(mx*my*sizeof(uchar4)+1024*4);
	
	compute.transferToGPU(data, 0, vector<uchar4>(mx*my)); ///zero out GPU memory
	
	buffer_resource bufres;
	vector<uint32_t> user_data;
	
	bufres.base_addr = compute.getVirtualAddress(data);
	bufres.stride = 4;
	bufres.num_records = mx*my+1024;
	bufres.dst_sel_x = 4;
	bufres.dst_sel_y = 4;
	bufres.dst_sel_z = 4;
	bufres.dst_sel_w = 4;
	bufres.num_format = 4;
	bufres.data_format = 10;
	bufres.element_size = 1;
	bufres.add_tid_en = 0;
	
	user_data.push_back(bufres.data[0]);
	user_data.push_back(bufres.data[1]);
	user_data.push_back(bufres.data[2]);
	user_data.push_back(bufres.data[3]);

	uint32_t *code = new uint32_t[code_size_max/4];

	set_program(code, mx, my);
	
	assert(mx > 0 and mx%256 == 0);
	
	compute.transferToGPU(program_code, 0, code, code_size_max);
	compute.launch(user_data, {0, 0, 0}, {size_t(mx/256), size_t(my), 1}, {256, 1, 1}, program_code);
	
	imageToFile(compute, data, mx, my, "ki.ppm");
}
