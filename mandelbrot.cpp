#include <iostream>
#include <cstdio>
#include <string>
#include <time.h>
#include <sys/time.h>
#include <assert.h>
#include <cmath>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "code_helper.h"
#include "compute_interface.hpp"

using namespace std;

struct uchar4
{
	uchar4() : r(0), g(0), b(0), a(0) {}
	uchar4(int val) : r(val), g(val), b(val), a(val) {}
	
	uint8_t r, g, b, a;
}__attribute__((packed));

int64_t get_time_usec()
{
		struct timeval tv;
		struct timezone tz;

		gettimeofday(&tv, &tz);

		return int64_t(tv.tv_sec) * 1000000 + int64_t(tv.tv_usec);
}

void imageToFile(ComputeInterface& compute, gpu_buffer* buffer, int mx, int my, std::string fname)
{
	vector<uchar4> image(mx*my);

	{
		int64_t start_time = get_time_usec();
		
		compute.transferFromGPU(buffer, 0, image);
		
		int64_t stop_time = get_time_usec();
		
		std::cout << "GTT-CPU transfer time down: " << double(stop_time-start_time)/1000.0 << "ms" << std::endl;
		std::cout << "GTT-CPU Bandwidth down: " << double(sizeof(image[0])*image.size()) / double(stop_time-start_time) << "Mbyte/s" << std::endl;
	}
	
	FILE *f = fopen(fname.c_str(), "w");
	
	fprintf(f, "P6\n%i %i\n255\n", mx, my);
	
	for (auto val : image)
	{
// 		cout << *(uint32_t*)&val << endl;
		fwrite(&val.r, 1, 1, f);
		fwrite(&val.g, 1, 1, f);
		fwrite(&val.b, 1, 1, f);
	}
	
	fclose(f);
}

void imageToFrameBuffer(ComputeInterface& compute, gpu_buffer* buffer, int mx, int my, std::string fb_name)
{
	struct stat sb;
	off_t len;
	char *p;
	int fd;

	fd = open (fb_name.c_str(), O_RDWR);
	
	if (fd == -1)
	{
		perror ("open");
		return;
	}

	if (fstat (fd, &sb) == -1)
	{
		perror ("fstat");
		return;
	}
	
	size_t size = mx*my*4;
	
	p = (char*)mmap (0, size, PROT_WRITE, MAP_SHARED, fd, 0);
	
	
	{
		int64_t start_time = get_time_usec();
		
		compute.transferFromGPU(buffer, 0, p, size);
		
		int64_t stop_time = get_time_usec();
		
		std::cout << "GTT-CPU transfer time down: " << double(stop_time-start_time)/1000.0 << "ms" << std::endl;
		std::cout << "GTT-CPU Bandwidth down: " << double(size) / double(stop_time-start_time) << "Mbyte/s" << std::endl;
	}

	if (p == MAP_FAILED)
	{
		perror ("mmap");
		return;
	}

	close(fd);
	
	if (munmap (p, size) == -1)
	{
		perror ("munmap");
		return;
	}
	
}

void set_program(unsigned* p, int mx, int my, double image_scale=1.0, double offset_x=0, double offset_y=0)
{
	float N;

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
	
	v_mul_f32(p, 6, 6, 255); p[0] = floatconv(1.0/float(mx/2)/image_scale); p++;
	v_mul_f32(p, 8, 8, 255); p[0] = floatconv(1.0/float(my/2)/image_scale); p++;
	
	v_add_f32(p, 6, 6, 255); p[0] = floatconv(offset_x); p++;
	v_add_f32(p, 8, 8, 255); p[0] = floatconv(offset_y); p++;
	
	//////////////////////////////////////////////////////////////////////////////
	///x:v6 y:v8 float32
	///pixelcolor:v10, 0xBBGGRR
	
	s_mov_b64(p, 12, 126); //SAVE exec to s12-s13
	s_mov_imm32(p, 8, 0); //s8 = 0;
	v_mov_imm32(p, 10, floatconv(0));
	v_mov_imm32(p, 12, floatconv(0));
	v_mov_imm32(p, 35, floatconv(0));  //zero out xact
	v_mov_imm32(p, 36, floatconv(0));  //zero out yact
	
	
	unsigned* eleje = p; //start label
	
	v_add_f32(p, 10, 10, 242); //v10 = v10 + 1; iteration counter for pixel color
	s_add_i32(p, 8, 8, 129); //s8 = s8 + 1; iteration counter for the scalar unit
	
	///dummy computation:
	///v_sin_f32(p, 12, 256+12);
	///v_mul_f32(p, 12, 12, 255); p[0] = floatconv(7.01); p++;
	///v_mul_f32(p, 14, 6, 256+6);
	///v_add_f32(p, 12, 12, 256+14);
	///v_mul_f32(p, 14, 6, 256+8);
	///v_add_f32(p, 12, 12, 256+14);

	///mandelbrot calculation
	// A     := v32
	// B     := v33
	// C     := v34
	// xact  := v35
	// yact  := v36
	// xval  := v6
	// yval  := v8
	// xtemp := v40

	//float xtemp = xact * xact - yact * yact + xval;
	v_mul_f32(p, 32, 35, 256+35);
	v_add_f32(p, 32, 32, 256+6); 
	v_mul_f32(p, 33, 36, 256+36);
	v_sub_f32(p, 40, 33, 256+32); 

	//yact = 2 * xact * yact + yval;
	v_mul_f32(p, 32, 35, 255); p[0] = floatconv(2.0); p++;
	v_mul_f32(p, 33, 32, 256+36);
	v_add_f32(p, 36, 33, 256+8); 
	
	//xact = xtemp;
	v_mov_b32(p, 35, 256+40);

	//STOP_EXPR_1
	v_mul_f32(p, 32, 35, 256+35);
	v_mul_f32(p, 33, 36, 256+36);
	v_add_f32(p, 12, 32, 256+33);
	
	//setting the radius 
	N = sqrt(25);
	v_cmpx_gt_f32(p, 12, 255); p[0]=floatconv(N*N); p++; //while(r12 < 7.0)
	
	s_cbranch_execz(p, 3);//Exit loop if vector unit is idle
	
	s_cmp_lt_i32(p, 8, 255); p[0] = 0xFFF; p++;
	s_cbranch_scc0(p, eleje-p-1); //if (s8 <= 0x1FFF) goto eleje;
	
	s_mov_b64(p, 126, 12); //restore exec from s12-s13
	
	//continuous coloring
	float scaleFactor = 1/(log2(N));
	v_sqrt_f32(p, 12, 256+12);
	v_log_f32(p, 12, 256+12);
	v_mul_f32(p, 12, 12, 255); p[0]=floatconv(scaleFactor); p++;
	v_log_f32(p, 12, 256+12);
	v_sub_f32(p, 10, 12, 256+10);
	
	float cscale = 5.0;
	
	v_mul_f32(p, 11, 10, 255); p[0] = floatconv(1.0/cscale); p++;
	v_add_f32(p, 11, 11, 255); p[0] = floatconv(-1.0/3.0); p++;
	v_sin_f32(p, 11, 256+11);
	v_add_f32(p, 11, 11, 255); p[0] = floatconv(1.0001); p++;
	v_mul_f32(p, 11, 11, 255); p[0] = floatconv(127.0); p++;
	
	v_mul_f32(p, 12, 10, 255); p[0] = floatconv(1.0/cscale); p++;
	v_add_f32(p, 12, 12, 255); p[0] = floatconv(0.0); p++;
	v_sin_f32(p, 12, 256+12);
	v_add_f32(p, 12, 12, 255); p[0] = floatconv(1.0001); p++;
	v_mul_f32(p, 12, 12, 255); p[0] = floatconv(127.0); p++;
	
	v_mul_f32(p, 13, 10, 255); p[0] = floatconv(1.0/cscale); p++;
	v_add_f32(p, 13, 13, 255); p[0] = floatconv(1.0/3.0); p++;
	v_sin_f32(p, 13, 256+13);
	v_add_f32(p, 13, 13, 255); p[0] = floatconv(1.0001); p++;
	v_mul_f32(p, 13, 13, 255); p[0] = floatconv(127.0); p++;
	
	v_cvt_i32_f32(p, 11, 256+11); //convert to int
	v_cvt_i32_f32(p, 12, 256+12); //convert to int
	v_cvt_i32_f32(p, 13, 256+13); //convert to int
	
	v_mul_i32_i24(p, 12, 12, 255); p[0]=256; p++;
	v_mul_i32_i24(p, 13, 13, 255); p[0]=256*256; p++;
	
	v_mov_b32(p, 10, 256+11);
	v_add_i32(p, 10, 10, 256+12);
	v_add_i32(p, 10, 10, 256+13);
	
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

void animationZoom(double offset_x, double offset_y, double zoom_step, int iternum)
{
	double zoom = 0.05;

	int mx = 1920;
	int my = 1080;
	int code_size_max = 1024*4;
	
	ComputeInterface compute("/dev/dri/card1");
	gpu_buffer* program_code = compute.bufferAlloc(code_size_max);
	gpu_buffer* data = compute.bufferAllocGTT(mx*my*sizeof(uchar4)+1024*4);
	uint32_t *code = new uint32_t[code_size_max/4];
	
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


	for (int i = 0; i < iternum; i++)
	{
		set_program(code, mx, my, zoom, offset_x, offset_y);
		
		compute.transferToGPU(program_code, 0, code, code_size_max);
		
		compute.launch(user_data, {0, 0, 0}, {size_t(mx/256), size_t(my), 1}, {256, 1, 1}, program_code, {program_code, data});

		imageToFrameBuffer(compute, data, mx, my, "/dev/fb1");
		zoom += zoom_step;
	}
	
	compute.bufferFree(program_code);
	compute.bufferFree(data);
}

int main()
{
	
	animationZoom(0.3, 0.45, 0.1, 200);
	
	return 0;
	int mx = 1920;
	int my = 1080;
	
	
	ComputeInterface compute("/dev/dri/card0");
	
	int code_size_max = 1024*4;
	
	gpu_buffer* program_code = compute.bufferAlloc(code_size_max);
	gpu_buffer* data = compute.bufferAlloc(mx*my*sizeof(uchar4)+1024*4);
	gpu_buffer* cpu_data = compute.bufferAllocGTT(mx*my*sizeof(uchar4)+1024*4);
	gpu_buffer* data2 = compute.bufferAllocGTT(mx*my*sizeof(uchar4)+1024*4);
	
	compute.transferToGPU(cpu_data, 0, vector<uchar4>(mx*my, uchar4(128))); ///zero out CPU memory
	compute.transferToGPU(data, 0, vector<uchar4>(mx*my, uchar4(128))); ///zero out GPU memory
	
	{
		int64_t start_time = get_time_usec();
		compute.transferToGPU(data, 0, vector<uchar4>(mx*my));
		int64_t stop_time = get_time_usec();
		
		std::cout << "transfer time up: " << double(stop_time-start_time)/1000.0 << "ms" << std::endl;
		std::cout << "Bandwidth up: " << double(mx*my*sizeof(uchar4)) / double(stop_time-start_time) << "Mbyte/s" << std::endl;
	}

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

	set_program(code, mx, my, 0.7, 0, 0);
	
// 	assert(mx > 0 and mx%256 == 0);
	
	compute.transferToGPU(program_code, 0, code, code_size_max);
	
	int64_t start_time = get_time_usec();
	
// 	compute.asyncDMACopy(cpu_data, 0, data2, 0, mx*my*sizeof(uchar4));
	compute.launch(user_data, {0, 0, 0}, {size_t(mx/256), size_t(my), 1}, {256, 1, 1}, program_code, {program_code, data});
	
	int64_t stop_time = get_time_usec();
	
	cout << "Runtime: " << double(stop_time-start_time) / 1000.0 << "ms" << endl;

	{
		int64_t start_time = get_time_usec();
		
		compute.asyncDMACopy(cpu_data, 0, data, 0, mx*my*sizeof(uchar4));
		compute.asyncDMAFence(cpu_data);
		
		int64_t stop_time = get_time_usec();
		
		std::cout << "DMA time: " << double(stop_time-start_time)/1000.0 << "ms" << std::endl;
		std::cout << "DMA Bandwidth: " << double(mx*my*sizeof(uchar4)) / double(stop_time-start_time) << "Mbyte/s" << std::endl;
	}
	
	imageToFile(compute, cpu_data, mx, my, "ki.ppm");
	imageToFrameBuffer(compute, cpu_data, mx, my, "/dev/fb1");
	{
		FILE *f = fopen("ki.bin", "w");
		fwrite(code, 1, code_size_max, f);
		fclose(f);
	}
}
