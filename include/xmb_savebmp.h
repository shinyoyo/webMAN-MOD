#ifdef EXTRA_FEAT

#include "vsh/system_plugin.h"

static void saveBMP()
{
	if(View_Find("game_plugin")==0) //XMB
	{
		system_interface = (system_plugin_interface *)plugin_GetInterface(View_Find("system_plugin"),1); // 1=regular xmb, 3=ingame xmb (doesnt work)

		CellRtcDateTime t;
		cellRtcGetCurrentClockLocalTime(&t);

		cellFsMkdir((char*)"/dev_hdd0/PICTURE", MODE);

		char bmp[0x50];
		vsh_sprintf(bmp,"/dev_hdd0/PICTURE/ScreenShot_%04d.%02d.%02d_%02d_%02d_%02d.bmp",t.year,t.month,t.day,t.hour,t.minute,t.second);
		system_interface->saveBMP(bmp);

		show_msg(bmp);
	}
}

#endif
