#include "../vsh/vshmain.h"

#ifdef VIDEO_REC

#include "../vsh/vsh.h"
#include "../vsh/rec_plugin.h"

#define PLUGIN_NAME "rec_plugin"

void toggle_video_rec(void);

bool recording = false;

uint32_t *recOpt = NULL;              // recording utility vsh options struct
int32_t (*reco_open)(int32_t) = NULL; // base pointer

bool rec_start(void);

bool rec_start()
{
	recOpt[1] = 0x4660;//CELL_REC_PARAM_VIDEO_FMT_M4HD_HD720_5000K_30FPS | 0x2100; //CELL_REC_PARAM_VIDEO_FMT_AVC_BL_MIDDLE_512K_30FPS
	recOpt[2] = 0x0000; //CELL_REC_PARAM_AUDIO_FMT_AAC_96K
	recOpt[5] = (vsh_memory_container_by_id(1) == -1 ) ? vsh_memory_container_by_id(0) : vsh_memory_container_by_id(1);
	recOpt[0x208] = 0x80; // 0x90 show XMB || reduce memsize // 0x80; // allow show XMB

	CellRtcDateTime t;
	cellRtcGetCurrentClockLocalTime(&t);

	char g[0x120];
	game_interface = (game_plugin_interface *)plugin_GetInterface(View_Find("game_plugin"), 1);

	game_interface->gameInfo(g);

	cellFsMkdir((char*)"/dev_hdd0/VIDEO", 0777);
	sprintf((char*)&recOpt[0x6], "/dev_hdd0/VIDEO/%s_%04d.%02d.%02d_%02d_%02d_%02d.mp4",
								   g+0x04, t.year, t.month, t.day, t.hour, t.minute, t.second);

	reco_open(-1); // memory container
	sys_timer_sleep(4);


	if(View_Find("rec_plugin") != 0)
	{
		rec_interface = (rec_plugin_interface *)plugin_GetInterface(View_Find("rec_plugin"), 1);

		if(rec_interface != 0)
		{
			rec_interface->start();
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		reco_open(-1); //reco_open((vsh_memory_container_by_id(1) == -1 ) ? vsh_memory_container_by_id(0) : vsh_memory_container_by_id(1));
		sys_timer_sleep(3);

		if(View_Find("rec_plugin") != 0)
		{
			rec_interface = (rec_plugin_interface *)plugin_GetInterface(View_Find("rec_plugin"), 1);

			rec_interface->start();
			return true;
		}
		else
		{
			show_msg((char*)"No rec_plugin view found.");
			return false;
		}
	}
}

void toggle_video_rec(void)
{
	if(View_Find("game_plugin") != 0)    // if game_plugin is loaded -> there is a game/app running and we can recording...
	{
		if(!reco_open)
		{
			// get functions pointer for sub_163EB0() aka reco_open()
			reco_open = vshmain_BEF63A14; // base pointer, the export nearest to sub_163EB0()

			reco_open -= (50*8); // reco_open_opd (50 opd's above vshmain_BEF63A14_opd)

			// fetch recording utility vsh options struct (build address from instructions...)
			uint32_t addr = (*(uint32_t*)(*(uint32_t*)reco_open+0xC) & 0x0000FFFF) -1;
			recOpt = (uint32_t*)((addr << 16) + ((*(uint32_t*)(*(uint32_t*)reco_open+0x14)) & 0x0000FFFF)); // (uint32_t*)0x72EEC0;
		}

		if(recording == false)
		{
		  // not recording yet
			show_msg((char*)"Recording started");

			if(rec_start() == false)
			{
				show_msg((char*)"Recording Error!");
			}
			else
			{
				recording = true;
			}
		}
		else
		{
			// we are already recording
			rec_interface->stop();
			rec_interface->close(0);
			show_msg((char*)"Recording finished");
			recording = false;
		}
	}
	else recording = false;
}
#endif // #ifdef VIDEO_REC
