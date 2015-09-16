#ifdef COBRA_ONLY
static void select_ps1emu(void)
{
	CellPadData pad_data;
	pad_data.len=0;

	for(u8 n=0;n<10;n++)
	{
		if(cellPadGetData(0, &pad_data) != CELL_PAD_OK || pad_data.len == 0)
			if(cellPadGetData(1, &pad_data) != CELL_PAD_OK || pad_data.len == 0)
					cellPadGetData(2, &pad_data);

		if(pad_data.len > 0) break;
		sys_timer_usleep(100000);
	}

	if(pad_data.len>0)
    {
		if(pad_data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R2) {webman_config->ps1emu=1; save_settings();} else
		if(pad_data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L2) {webman_config->ps1emu=0; save_settings();}
    }

	char msg[100];

	if(webman_config->ps1emu)
	{
		sys_map_path((char*)"/dev_flash/ps1emu/ps1_netemu.self", (char*)"//dev_flash/ps1emu/ps1_emu.self");
		sys_map_path((char*)"/dev_flash/ps1emu/ps1_emu.self"   , (char*)"//dev_flash/ps1emu/ps1_netemu.self");

		sprintf(msg, "ps1_netemu.self %s", STR_ENABLED);
	}
	else
	{
		sys_map_path((char*)"/dev_flash/ps1emu/ps1_netemu.self", (char*)"//dev_flash/ps1emu/ps1_netemu.self");
		sys_map_path((char*)"/dev_flash/ps1emu/ps1_emu.self"   , (char*)"//dev_flash/ps1emu/ps1_emu.self");

		sprintf(msg, "ps1_emu.self %s",    STR_ENABLED);
	}

	show_msg((char*) msg);
}
#endif
