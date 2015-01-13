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
#include "code_helper.h"
#include "ati_chip.h"
extern "C" {
#include "computesi.h"
};

using namespace std;

int64_t get_time_usec()
{
		struct timeval tv;
		struct timezone tz;

		gettimeofday(&tv, &tz);

		return int64_t(tv.tv_sec) * 1000000 + int64_t(tv.tv_usec);
}

int main(int argc, char* argv[])
{
	std::vector<AtiDeviceData> devices = getAllAtiDevices();
	
	for (AtiDeviceData devData : devices)
	{
		std::cout << devData.vendorName << " : " << devData.deviceName << " : " << devData.busid << " " << devData.devpath << std::endl;
	}
	
	if (devices.empty())
	{
		std::cerr << "No available GPU devices" << std::endl;
		return 1;
	}
	
	compute_context* ctx = nullptr;
	
	if (argc == 1)
	{
		ctx = compute_create_context(devices.front().devpath.c_str(), devices.front().busid.c_str());
	}
	else
	{
		for (AtiDeviceData devData : devices)
		{
			if (argv[1] == devData.devpath)
			{
				ctx = compute_create_context(argv[1], devData.busid.c_str());
				break;
			}
		}
	}
	
	if (not ctx)
	{
		std::cerr << "Cannot create GPU context" << std::endl;
		return 1;
	}
	
	int test_data_size = 1024*1024*16;
	gpu_buffer* code_bo = compute_alloc_gpu_buffer(ctx, 1024*1024*4, RADEON_DOMAIN_GTT, 4096);
	gpu_buffer* data_bo = compute_alloc_gpu_buffer(ctx, test_data_size*4, RADEON_DOMAIN_GTT, 4096);
	
	uint32_t prog[1024*1024*1];
	unsigned *p;
	for (unsigned i = 0; i < sizeof(prog) / sizeof(prog[0]); i++)
	{
		prog[i] = 0xBF800000; //sopp: NOP
	}
	
	p = &prog[0];
		
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

	printf("buf addr%lx, code addr: %lx\n", data_bo->va, code_bo->va);

	v_mov_imm32(p, 1, 0x0002);
	v_mov_imm32(p, 2, 0x0001);

	s_mov_imm32(p, 126, 0x00000001); //EXECLO
	s_mov_imm32(p, 127, 0x00000000); //EXECHI

	v_mov_b32(p, 1, 4);
	v_mov_b32(p, 2, 4);
	v_mul_i32_i24_imm32(p, 1, 1, 64);

	v_mov_imm32(p, 1, 0x00000000);
	v_mov_imm32(p, 2, 0x00000007);

	mubuf(p, 
		128, //int soffset, 
		0,//int tfe, 
		0,//int slc, 
		0,//int srsrc, 
		2,//int vdata, 
		0,//int vaddr, UNUSED HERE
		BUFFER_ATOMIC_ADD,//int op, 
		0, //int lds, 
		0, //int addr64, 
		1, //int glc, 
		0, //int idxen, NO index in vaddr
		0, //int offen, NO offset in vaddr
		0 //int offset
	);
	
	s_waitcnt(p);

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
	int iternum = 1000;
	int elem_flopcount = 2;
	int ii2 = 1024*1;
	s_mov_imm32(p, 126, 0xFFFFFFFF); //EXECLO
	s_mov_imm32(p, 127, 0xFFFFFFFF); //EXECHI

	s_mov_imm32(p, 8, 0);

	v_mov_imm32(p, 4, floatconv(2));
	v_mov_imm32(p, 5, floatconv(1));

	unsigned * eleje = p;

	for (int i = 0; i < ii2; i++)
	{
		v_fma_f32(p, 4, 0, 0, 256+5, 256+4, 256+4, 0, 0);
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

	
	s_getreg_b32(p, 9, 31, 0, 5); //status
	
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
	
	v_mov_b32(p, 4, 9);
	v_add_i32_imm32(p, 2, 2, 4);

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
					4,//int vdata,
					2//int vaddr
					);
	s_waitcnt(p);

	s_endpgm(p);

	compute_copy_to_gpu(code_bo, 0, &prog[0], sizeof(prog));
	
	unsigned* test_data = new unsigned[test_data_size];
	
	for (int i = 0; i < 1024; i++)
	{
		test_data[i] = 0xDEADBEEF;
	}
	
	test_data[0] = 1;

	compute_copy_to_gpu(data_bo, 0, &test_data[0], test_data_size*4);
	
	compute_state state;
	
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
	state.num_thread[0] = 256*7;
	state.num_thread[1] = 1;
	state.num_thread[2] = 1;
	
	state.sgpr_num = 5; //8x
	state.vgpr_num = 5; //4x
	state.priority = 0;
	state.debug_mode = 0;
	state.priv_mode = 0;
	state.trap_en = 0;
	state.ieee_mode = 0;
	state.scratch_en = 0;
	state.lds_size = 0;
	state.excp_en = 0;
	state.waves_per_sh = 0;
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

	compute_bo_wait(code_bo);
	int64_t stop_time = get_time_usec();

	cout << e << " " << strerror(errno) << endl;
	
	compute_flush_caches(ctx);
	
	compute_copy_from_gpu(data_bo, 0, &test_data[0], test_data_size*4);
	
	int global_size = state.dim[0]*state.dim[1]*state.dim[2]*state.num_thread[0]*state.num_thread[1]*state.num_thread[2];
	int datalen = global_size / 64 * 7;

	std::cout << test_data[0] << " " << datalen << std::endl;

	datalen = test_data[0];

	uint64_t firststart = *(uint64_t*)&test_data[1];

	uint64_t laststop = firststart;

	for (int i = 1; i < datalen; i += 7)
	{
		uint64_t start = *(uint64_t*)&test_data[i];
		uint64_t stop = *(uint64_t*)&test_data[i+2];
// 		int group_id = test_data[i+4];

		firststart = std::min(firststart, start);
		laststop = std::max(laststop, stop);
	}

	std::set<std::pair<std::vector<uint32_t>, std::string> > ordered;

	for (int i = 1; i < datalen; i += 7)
	{
		uint64_t start = *(uint64_t*)&test_data[i];
		uint64_t stop = *(uint64_t*)&test_data[i+2];
		int group_id = test_data[i+4];
		uint32_t hwid = test_data[i+5];
		uint32_t odata = test_data[i+6];
		
		int cu_id = (hwid >> 8) & 15;
		int sh_id = (hwid >> 12) & 1;
		int se_id = (hwid >> 13) & 3;

		char buf[64*1024];
		sprintf(buf, "start: %10lu diff:%10lu clocks gid:%3i wave_id: %i simd_id: %i cu_id: %i, sh_id: %i se_id: %i cu_num:%3i tg_id: %i hwid:%08X data:%08X", 
						start-firststart, stop-start, group_id, hwid & 15, (hwid >> 4) & 3, cu_id, sh_id, se_id,  cu_id+sh_id*8+se_id*16, (hwid >> 16) & 15, hwid, odata);
		ordered.insert(make_pair(vector<uint32_t>{(hwid >> 13) & 3, (hwid >> 12) & 1, (hwid >> 8) & 15, (hwid >> 4) & 3}, std::string(buf)));
	}

	for (auto p : ordered)
	{
		cout << p.second << std::endl;
	}
	
	cout << "run time: " << double(stop_time-start_time)/1000.0 << "ms" << endl;
	cout << "run cycles: " << double(laststop - firststart) << endl;
	cout << "Core freq: " << double(laststop - firststart)/double(stop_time-start_time) << "MHz" << endl;
	cout << double(ii2*iternum)*global_size / double(stop_time-start_time) * 1E-3 << "Giter/s" << endl;
	cout << double(ii2*iternum)*global_size / double(stop_time-start_time) * 1E-3 * double(elem_flopcount)<< "Gflops" << endl;
	cout << double(laststop - firststart) / double(ii2*iternum) << " cycles / iter" << endl;
	compute_free_context(ctx);
}


