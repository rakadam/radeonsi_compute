#include <iostream>
#include <cstdio>
#include <string>
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
	gpu_buffer* data = compute.bufferAlloc(mx*my*sizeof(uchar4));
	
	buffer_resource bufres;
	vector<uint32_t> user_data;
	
	bufres.base_addr = compute.getVirtualAddress(data);
	bufres.stride = 4;
	bufres.num_records = mx*my;
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
	
	compute.transferToGPU(program_code, 0, code, code_size_max);
	compute.launch(user_data, {0, 0, 0}, {1, 1, 1}, {64, 1, 1}, program_code);
	
	imageToFile(compute, data, mx, my, "ki.ppm");
}
