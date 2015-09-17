#include <sdk_version.h>
#include <cellstatus.h>
#include <cell/cell_fs.h>
#include <cell/rtc.h>
#include <cell/gcm.h>
#include <cell/pad.h>
#include <sys/vm.h>
#include <sysutil/sysutil_common.h>

#include <sys/prx.h>
#include <sys/ppu_thread.h>
#include <sys/event.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/memory.h>
#include <sys/timer.h>
#include <sys/process.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netex/net.h>
#include <netex/errno.h>
#include <netex/libnetctl.h>
#include <netex/sockinfo.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "flags.h"

#include "types.h"
#include "common.h"
#include "cobra/cobra.h"
#include "cobra/storage.h"
#include "vsh/game_plugin.h"
#include "vsh/netctl_main.h"
#include "vsh/xregistry.h"

#define _game_TitleID  _game_info+0x04
#define _game_Title    _game_info+0x14

static char _game_info[0x120];
static char search_url[50] = "http://google.com/search?q=";

#ifdef COBRA_ONLY
 #include "cobra/netiso.h"

 #ifdef LITE_EDITION
    #define EDITION " [Lite]"
 #else
  #ifdef PS3MAPI
	#ifdef REX_ONLY
		#define EDITION " [Rebug-PS3MAPI]"
	#else
		#define EDITION " [PS3MAPI]"
	#endif
  #else
   #ifdef REX_ONLY
    #define EDITION " [Rebug]"
   #else
    #define EDITION ""
   #endif
  #endif
 #endif
#else
 #ifdef CCAPI
    #define EDITION " [CCAPI]"
 #else
    #define EDITION " [nonCobra]"
 #endif
#endif

SYS_MODULE_INFO(WWWD, 0, 1, 0);
SYS_MODULE_START(wwwd_start);
SYS_MODULE_STOP(wwwd_stop);

#define VSH_MODULE_PATH 	"/dev_blind/vsh/module/"
#define VSH_ETC_PATH		"/dev_blind/vsh/etc/"
#define PS2_EMU_PATH		"/dev_blind/ps2emu/"
#define REBUG_COBRA_PATH	"/dev_blind/rebug/cobra/"
#define HABIB_COBRA_PATH	"/dev_blind/habib/cobra/"
#define SYS_COBRA_PATH		"/dev_blind/sys/"
#define PS2_CLASSIC_TOGGLER "/dev_hdd0/classic_ps2"
#define REBUG_TOOLBOX		"/dev_hdd0/game/RBGTLBOX2/USRDIR/"
#define COLDBOOT_PATH		"/dev_blind/vsh/resource/coldboot.raf"
#define ORG_LIBFS_PATH		"/dev_flash/sys/external/libfs.sprx"
#define NEW_LIBFS_PATH		"/dev_hdd0/tmp/libfs.sprx"

#define PS2_CLASSIC_PLACEHOLDER  "/dev_hdd0/game/PS2U10000/USRDIR"
#define PS2_CLASSIC_ISO_PATH     "/dev_hdd0/game/PS2U10000/USRDIR/ISO.BIN.ENC"
#define PS2_CLASSIC_ISO_ICON     "/dev_hdd0/game/PS2U10000/ICON0.PNG"

#define WM_VERSION			"1.43.04 MOD"						// webMAN version
#define MM_ROOT_STD			"/dev_hdd0/game/BLES80608/USRDIR"	// multiMAN root folder
#define MM_ROOT_SSTL		"/dev_hdd0/game/NPEA00374/USRDIR"	// multiman SingStar® Stealth root folder
#define MM_ROOT_STL			"/dev_hdd0/tmp/game_repo/main"		// stealthMAN root folder

#define WMCONFIG			"/dev_hdd0/tmp/wmconfig.bin"		// webMAN config file
#define WMTMP				"/dev_hdd0/tmp/wmtmp"				// webMAN work/temp folder
#define WM_ICONS_PATH		"/dev_hdd0/tmp/wm_icons/"			// webMAN icons path
#define WMNOSCAN			"/dev_hdd0/tmp/wm_noscan"			// webMAN config file

#ifdef WEB_CHAT
 #define DELETE_TURNOFF		{do_umount(false); cellFsUnlink((char*)"/dev_hdd0/tmp/turnoff"); cellFsUnlink((char*)WMCHATFILE);}
#else
 #define DELETE_TURNOFF		{do_umount(false); cellFsUnlink((char*)"/dev_hdd0/tmp/turnoff");}
#endif

#define THREAD_NAME 		"wwwdt"
#define THREAD_NAME_FTP 	"ftpdt"
#define THREAD_NAME_NET 	"netiso"
#define THREAD_NAME_NTFS 	"ntfsd"

#define STOP_THREAD_NAME 	"wwwds"

////////////

#define HTML_BASE_PATH			"/dev_hdd0/xmlhost/game_plugin"

#define FB_XML					HTML_BASE_PATH "/fb.xml"
#define MY_GAMES_XML			HTML_BASE_PATH "/mygames.xml"
#define MOBILE_HTML				HTML_BASE_PATH "/mobile.html"
#define GAMELIST_JS				HTML_BASE_PATH "/gamelist.js"

#define DELETE_CACHED_GAMES		{cellFsUnlink((char*)WMTMP "/games.html"); cellFsUnlink((char*)GAMELIST_JS);}

////////////

#define SC_COBRA_SYSCALL8				(8)

#define SC_GET_FREE_MEM 				(352)
#define SC_GET_PLATFORM_INFO			(387)
#define SC_RING_BUZZER  				(392)

#define SC_FS_MOUNT  					(837)
#define SC_FS_UMOUNT 					(838)
#define SC_GET_CONSOLE_TYPE				(985)

#define SC_GET_PRX_MODULE_BY_ADDRESS	(461)
#define SC_STOP_PRX_MODULE 				(482)
#define SC_UNLOAD_PRX_MODULE 			(483)
#define SC_PPU_THREAD_EXIT				(41)

#define SC_SYS_POWER 					(379)
#define SYS_SOFT_REBOOT 				0x0200
#define SYS_HARD_REBOOT					0x1200
#define SYS_REBOOT						0x8201 /*load LPAR id 1*/
#define SYS_SHUTDOWN					0x1100

#define SYS_NET_EURUS_POST_COMMAND		(726)
#define CMD_GET_MAC_ADDRESS				0x103f

#define SYSCALL8_OPCODE_GET_MAMBA		0x7FFFULL

#define BEEP1 { system_call_3(SC_RING_BUZZER, 0x1004, 0x4,   0x6); }
#define BEEP2 { system_call_3(SC_RING_BUZZER, 0x1004, 0x7,  0x36); }
#define BEEP3 { system_call_3(SC_RING_BUZZER, 0x1004, 0xa, 0x1b6); }

#define WWWPORT			(80)
#define FTPPORT			(21)

#define KB			   1024UL
#define   _4KB_		   4096UL
#define   _8KB_		   8192UL
#define  _32KB_		  32768UL
#define  _64KB_		  65536UL
#define _128KB_		 131072UL
#define _192KB_		 196608UL
#define _256KB_		 262144UL
#define  _1MB_		1048576UL
#define _32MB_		33554432UL

#define MIN_MEM		_192KB_

#define MODE		0777

#define LINELEN			512 // file listing
#define MAX_LINE_LEN	640 // html games
#define MAX_PATH_LEN	512 // do not change!

#define FAILED		-1

#define HTML_RECV_SIZE 2048

static u32 BUFFER_SIZE_FTP	= ( _128KB_);

static u32 BUFFER_SIZE		= ( 448*KB);
static u32 BUFFER_SIZE_PSX	= ( 160*KB);
static u32 BUFFER_SIZE_PSP	= (  _32KB_);
static u32 BUFFER_SIZE_PS2	= (  _64KB_);
static u32 BUFFER_SIZE_DVD	= ( _192KB_);
static u32 BUFFER_SIZE_ALL	= ( 896*KB);

#ifdef COBRA_ONLY
 #ifndef LITE_EDITION
 static sys_ppu_thread_t thread_id_net	=-1;
 #endif
 static sys_ppu_thread_t thread_id_ntfs	=-1;
#endif
static sys_ppu_thread_t thread_id_poll	=-1;
static sys_ppu_thread_t thread_id_ftp	=-1;
static sys_ppu_thread_t thread_id		=-1;

#define SUFIX(a)	((a==1)? "_1" :(a==2)? "_2" :(a==3)? "_3" :(a==4)?"_4":"")
#define SUFIX2(a)	((a==1)?" (1)":(a==2)?" (2)":(a==3)?" (3)":(a==4)?" (4)":"")
#define SUFIX3(a)	((a==1)?" (1).ntfs[":(a==2)?" (2).ntfs[":(a==3)?" (3).ntfs[":(a==4)?" (4).ntfs[":"")

#define IS_ISO_FOLDER (f1>1 && f1<10)
#define IS_PS3_FOLDER (f1<3 || f1>=10)
#define IS_BLU_FOLDER (f1==3)
#define IS_DVD_FOLDER (f1==4)
#define IS_PS2_FOLDER (f1==5)
#define IS_PSX_FOLDER (f1==6 || f1==7)
#define IS_PSP_FOLDER (f1==8 || f1==9)

#define PS3 (1<<0)
#define PS2 (1<<1)
#define PS1 (1<<2)
#define PSP (1<<3)
#define BLU (1<<4)
#define DVD (1<<5)

#define MIN(a, b)	((a) <= (b) ? (a) : (b))
#define ABS(a)		(((a) < 0) ? -(a) : (a))
#define RANGE(a, b, c)	((a) <= (b) ? (b) : (a) >= (c) ? (c) : (a))

#define START_DAEMON		(0xC0FEBABE)
#define REFRESH_CONTENT		(0xC0FEBAB0)

typedef struct {
	uint32_t total;
	uint32_t avail;
} _meminfo;

//static bool is_rebug = false;
static u8 profile = 0;

static u8 loading_html = 0;
static u8 loading_games = 0;
static u8 init_running = 0;

#define NTFS 		 	(10)

static bool show_info_popup = false;

#ifdef USE_DEBUG
 static int debug_s=-1;
 static char debug[256];
#endif
static volatile u8 wm_unload_combo = 0;
static volatile u8 working = 1;
static u8 cobra_mode=0;
static u8 max_mapped=0;

static float c_firmware=0.0f;
static u8 dex_mode=0;

static u64 SYSCALL_TABLE = 0;

typedef struct
{
	uint8_t usb0;
	uint8_t usb1;
	uint8_t usb2;
	uint8_t usb3;
	uint8_t usb6;
	uint8_t usb7;
	uint8_t netd0;
	uint8_t lastp;
	uint8_t autob;
	uint8_t delay;
	uint8_t bootd;
	uint8_t boots;
	uint8_t blind;
	uint8_t nogrp;
	uint8_t noset;
	uint8_t cmask;
	uint32_t netp0;
	char neth0[16];
	uint8_t poll;
	uint8_t ftpd;
	uint8_t warn;
	uint8_t fanc;
	uint8_t temp1;
	uint8_t rxvid;
	uint8_t bind;
	uint8_t refr;
	uint8_t manu;
	uint8_t temp0;
	uint8_t netd1;
	uint32_t netp1;
	char neth1[16];
	uint8_t foot;
	uint8_t nopad;
	uint8_t nocov;
	uint8_t nospoof;
	uint8_t ps2temp;
	uint8_t pspl;
	uint8_t minfan;
	uint16_t combo;
	uint8_t sidps;
	uint8_t spsid;
	uint8_t spp;
	uint8_t lang;
	char vIDPS1[17];
	char vIDPS2[17];
	char vPSID1[17];
	char vPSID2[17];
	uint8_t tid;
	uint8_t wmdn;
	char autoboot_path[256];
	uint8_t ps2l;
	uint32_t combo2;
	uint8_t homeb;
	char home_url[256];
	uint8_t netd2;
	uint32_t netp2;
	char neth2[16];
	uint8_t profile;
	char uaccount[9];
	char allow_ip[16];
	uint8_t noss;
	uint8_t fixgame;
	uint8_t bus;
	uint8_t dev_sd;
	uint8_t dev_ms;
	uint8_t dev_cf;
	uint8_t ps1emu;
} __attribute__((packed)) WebmanCfg;

#define AUTOBOOT_PATH            "/dev_hdd0/PS3ISO/AUTOBOOT.ISO"

#ifdef COBRA_ONLY
 #define DEFAULT_AUTOBOOT_PATH   "/dev_hdd0/PS3ISO/AUTOBOOT.ISO"
#else
 #define DEFAULT_AUTOBOOT_PATH   "/dev_hdd0/GAMES/AUTOBOOT"
#endif

#define ISO_EXTENSIONS           ".iso.cue.img.mdf.bin"

static CellRtcTick rTick, gTick;

static int set_gamedata_status(u8 status, bool do_mount);
static void set_buffer_sizes(int footprint);

#ifdef COBRA_ONLY
static void select_ps1emu(void);
#endif

int extcmp(const char *s1, const char *s2, size_t n);
int extcasecmp(const char *s1, const char *s2, size_t n);
char *strcasestr(const char *s1, const char *s2);

static void reset_settings(void);
static int save_settings(void);
static u64 backup[6];

static u8 wmconfig[sizeof(WebmanCfg)];
static WebmanCfg *webman_config = (WebmanCfg*) wmconfig;

static bool gmobile_mode = false;

static char ftp_password[20]="";
static char html_base_path[MAX_PATH_LEN]="";

static char smonth[12][4]={"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static char drives[14][12]={"/dev_hdd0", "/dev_usb000", "/dev_usb001", "/dev_usb002", "/dev_usb003", "/dev_usb006", "/dev_usb007", "/net0", "/net1", "/net2", "/ext", "/dev_sd", "/dev_ms", "/dev_cf"};
static char paths [11][12]={"GAMES", "GAMEZ", "PS3ISO", "BDISO", "DVDISO", "PS2ISO", "PSXISO", "PSXGAMES", "PSPISO", "ISO", "video"};

static char wm_icons[12][60]={WM_ICONS_PATH "icon_wm_album_ps3.png", //024.png  [0]
                              WM_ICONS_PATH "icon_wm_album_psx.png", //026.png  [1]
                              WM_ICONS_PATH "icon_wm_album_ps2.png", //025.png  [2]
                              WM_ICONS_PATH "icon_wm_album_psp.png", //022.png  [3]
                              WM_ICONS_PATH "icon_wm_album_dvd.png", //023.png  [4]

                              WM_ICONS_PATH "icon_wm_ps3.png",       //024.png  [5]
                              WM_ICONS_PATH "icon_wm_psx.png",       //026.png  [6]
                              WM_ICONS_PATH "icon_wm_ps2.png",       //025.png  [7]
                              WM_ICONS_PATH "icon_wm_psp.png",       //022.png  [8]
                              WM_ICONS_PATH "icon_wm_dvd.png",       //023.png  [9]

                              WM_ICONS_PATH "icon_wm_settings.png",  //icon/icon_home.png  [10]
                              WM_ICONS_PATH "icon_wm_eject.png"      //icon/icon_home.png  [11]
                             };

static bool covers_exist[7];
static char local_ip[16] = "127.0.0.1";

//uint64_t find_syscall();
//uint64_t search64(uint64_t val);
//uint64_t find_syscall_table();

#define SYSCALLS_UNAVAILABLE    0xFFFFFFFF80010003ULL

#include "include/peek_poke.h"
#include "include/led.h"
#include "include/socket.h"
#include "include/html.h"
#include "include/language.h"
#include "include/fancontrol.h"

#include "include/ps2_classic.h"
#include "include/vpad.h"
#include "include/idps.h"
#include "include/singstar.h"
#include "include/xmb_savebmp.h"

int wwwd_start(uint64_t arg);
int wwwd_stop(void);
static void stop_prx_module(void);
static void unload_prx_module(void);

static void detect_firmware(void);

static void handleclient(u64 conn_s_p);

static void do_umount(bool clean);
static void mount_autoboot(void);
static bool mount_with_mm(const char *_path, u8 do_eject);

#ifdef COBRA_ONLY
static void do_umount_iso(void);
#endif

static bool from_reboot = false;
static bool is_busy = false;
static bool is_mounting = false;

#ifdef COPY_PS3
static char current_file[MAX_PATH_LEN];
#endif

#include "include/vsh.h"
#include "include/webchat.h"
#include "include/file.h"
#include "include/fix_game.h"
#include "include/ps2_disc.h"

#ifndef COBRA_ONLY
static void string_to_lv2(char* path, u64 addr);
static void add_to_map(char *path1, char *path2);
#endif

static inline void _sys_ppu_thread_exit(uint64_t val)
{
	system_call_1(SC_PPU_THREAD_EXIT, val); // prxloader = mandatory; cobra = optional; ccapi = don't use !!!
}

static inline sys_prx_id_t prx_get_module_id_by_address(void *addr)
{
	system_call_1(SC_GET_PRX_MODULE_BY_ADDRESS, (uint64_t)(uint32_t)addr);
	return (int)p1;
}

#include "include/eject_insert.h"

#ifdef COBRA_ONLY

#include "include/rawseciso.h"
#include "include/netsvr.h"

#endif //#ifdef COBRA_ONLY

#include "include/gamedata.h"
#include "include/psxemu.h"

#include "include/debug_mem.h"
#include "include/ftp.h"
#include "include/ps3mapi.h"
#include "include/video_rec.h"
#include "include/stealth.h"

#include "include/games_html.h"
#include "include/games_xml.h"

#include "include/cpursx.h"
#include "include/setup.h"
#include "include/file_manager.h"
#include "include/_mount.h"

static void http_response(int conn_s, char *header, char *param, int code, char *msg)
{
	char text[MAX_LINE_LEN]; if(msg[0]=='/') sprintf(text, "%s : OK", msg+1); else sprintf(text, "%s", msg);

	sprintf(header, "HTTP/1.1 %i OK\r\n"
					"X-PS3-Info: [%s]\r\n"
					"Content-Type: text/html\r\n"
					"Content-Length: %i\r\n\r\n"
					"<body bgcolor=\"#101010\" text=\"#c0c0c0\">"
					"<font face=\"Courier New\">"
					"webMAN MOD " WM_VERSION "<hr><h2>%s</h2>"
					"</font></body>",
					 code, param, 113+strlen(text), text);

	ssend(conn_s, header);
	sclose(&conn_s);

	if(msg[0]=='/') {show_msg((char*)text); sys_timer_sleep(1); }
}

static void prepare_html(char *buffer, char *templn, char *param, u8 is_ps3_http, u8 is_cpursx, bool mount_ps3)
{
	strcpy(buffer,  "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">"
					"<html xmlns=\"http://www.w3.org/1999/xhtml\">"
					"<meta http-equiv=\"Content-type\" content=\"text/html;charset=UTF-8\">"
					"<meta http-equiv=\"cache-control\" content=\"no-cache\">"
					"<meta content='target-densitydpi=device-dpi; width=device-width; initial-scale=0.6; maximum-scale=1.0;' name='viewport'>");

	if(is_cpursx) strcat(buffer, "<meta http-equiv=\"refresh\" content=\"6;URL=/cpursx.ps3\">");
	if(mount_ps3) {	strcat(buffer, "<body bgcolor=\"#101010\">"); return;}

	strcat(buffer,	"<head><title>webMAN MOD</title>"
					"<style type=\"text/css\"><!--\r\n"

					"a.s:active{color:#F0F0F0;}"
					"a:link{color:#909090;text-decoration:none;}"

					"a.f:active{color:#F8F8F8;}"
					"a,a.f:link,a:visited{color:#D0D0D0;}");

	if(!is_cpursx)
	strcat(buffer,	"a.d:link{color:#D0D0D0;background-position:0px 2px;background-image:url('data:image/gif;base64,R0lGODlhEAAMAIMAAOenIumzLbmOWOuxN++9Me+1Pe+9QvDAUtWxaffKXvPOcfTWc/fWe/fWhPfckgAAACH5BAMAAA8ALAAAAAAQAAwAAARQMI1Agzk4n5Sa+84CVNUwHAz4KWzLMo3SzDStOkrHMO8O2zmXsAXD5DjIJEdxyRie0KfzYChYr1jpYVAweb/cwrMbAJjP54AXwRa433A2IgIAOw==');padding:0 0 0 20px;background-repeat:no-repeat;margin-left:auto;margin-right: auto;}"
					"a.w:link{color:#D0D0D0;background-image:url('data:image/gif;base64,R0lGODlhDgAQAIMAAAAAAOfn5+/v7/f39////////////////////////////////////////////wAAACH5BAMAAA8ALAAAAAAOABAAAAQx8D0xqh0iSHl70FxnfaDohWYloOk6papEwa5g37gt5/zO475fJvgDCW8gknIpWToDEQA7');padding:0 0 0 20px;background-repeat:no-repeat; margin-left:auto; margin-right:auto;}");

	strcat(buffer,	"a:active,a:active:hover,a:visited:hover,a:link:hover{color:#FFFFFF;}"
					".list{display:inline;}"
					"input:focus{border:2px solid #0099FF;}"
					".gc{float:left;overflow:hidden;position:relative;text-align:center;width:280px;height:260px;margin:3px;border:1px dashed grey;}"
					".ic{position:absolute;top:5px;right:5px;left:5px;bottom:40px;}"
					".propfont{font-family:\"Courier New\",Courier,monospace;}"
					"body,a.s,td,th{color:#F0F0F0;white-space:nowrap;");

	if(is_ps3_http==2)
		strcat(buffer, "width:800px;}");
	else
		strcat(buffer, "}");

	if(!strstr(param, "/setup.ps3")) strcat(buffer, "td+td{text-align:right;white-space:nowrap}");

	if(is_ps3_http==1)
		strcat(buffer, ".gi{height:210px;width:267px;");
	else
		strcat(buffer, ".gi{position:absolute;max-height:210px;max-width:260px;");

	strcat(buffer, "position:absolute;bottom:0px;top:0px;left:0px;right:0px;margin:auto;}"
				   ".gn{position:absolute;height:38px;bottom:0px;right:7px;left:7px;text-align:center;}--></style>");

	if(param[1]!=NULL) {sprintf(templn, "<base href=\"%s/\">", param); strcat(buffer, templn);}

	strcat(buffer,	"</head>"
					"<body bgcolor=\"#101010\">"
					"<font face=\"Courier New\"><b>");

#ifndef ENGLISH_ONLY
    if(strlen(STR_TRADBY)==0) language("STR_TRADBY", STR_TRADBY); //strcpy(STR_TRADBY, "<br>");
    if(strlen(STR_HOME  )==0) language("STR_HOME"  , STR_HOME  ); //strcpy(STR_HOME  , "<br>");
#else
    strcpy(STR_HOME, "Home");
#endif

#ifdef PS3MAPI
	#ifdef WEB_CHAT
		sprintf(templn, "webMAN " WM_VERSION " %s <font style=\"font-size:18px\">[<a href=\"/\">%s</a>] [<a href=\"/index.ps3\">%s</a>] [<a href=\"/games.ps3\">Slider</a>] [<a href=\"/chat.ps3\">Chat</a>] [<a href=\"/home.ps3mapi\">PS3MAPI</a>] [<a href=\"/setup.ps3\">%s</a>]</b>", STR_TRADBY, STR_FILES, STR_GAMES, STR_SETUP); strcat(buffer, templn);
	#else
		sprintf(templn, "webMAN " WM_VERSION " %s <font style=\"font-size:18px\">[<a href=\"/\">%s</a>] [<a href=\"/index.ps3\">%s</a>] [<a href=\"/games.ps3\">Slider</a>] [<a href=\"/home.ps3mapi\">PS3MAPI</a>] [<a href=\"/setup.ps3\">%s</a>]</b>", STR_TRADBY, STR_FILES, STR_GAMES, STR_SETUP ); strcat(buffer, templn);
	#endif
#else
	#ifdef WEB_CHAT
		sprintf(templn, "webMAN " WM_VERSION " %s <font style=\"font-size:18px\">[<a href=\"/\">%s</a>] [<a href=\"/index.ps3\">%s</a>] [<a href=\"/games.ps3\">Slider</a>] [<a href=\"/chat.ps3\">Chat</a>] [<a href=\"/setup.ps3\">%s</a>]</b>", STR_TRADBY, STR_FILES, STR_GAMES, STR_SETUP); strcat(buffer, templn);
	#else
		sprintf(templn, "webMAN " WM_VERSION " %s <font style=\"font-size:18px\">[<a href=\"/\">%s</a>] [<a href=\"/index.ps3\">%s</a>] [<a href=\"/games.ps3\">Slider</a>] [<a href=\"/setup.ps3\">%s</a>]</b>", STR_TRADBY, STR_FILES, STR_GAMES, STR_SETUP ); strcat(buffer, templn);
	#endif
#endif

}

static void handleclient(u64 conn_s_p)
{
	int conn_s = (int)conn_s_p; // main communications socket


	char param[HTML_RECV_SIZE];
	struct CellFsStat buf;
	int fd;

	if(conn_s_p==START_DAEMON || conn_s_p==REFRESH_CONTENT)
	{

#ifndef ENGLISH_ONLY
		update_language();
#endif

		if(conn_s_p==START_DAEMON)
		{
			if(profile || (!(webman_config->wmdn) && strlen(STR_WMSTART)>0))
			{
				sys_timer_sleep(10);
				sprintf(param, "%s%s", STR_WMSTART, SUFIX2(profile));
				show_msg((char*)param);
			}
		}
		else //if(conn_s_p==REFRESH_CONTENT)
		{
			{DELETE_CACHED_GAMES} // refresh XML will force "refresh HTML" to rebuild the cache file
		}

		init_running = 1;

		cellFsMkdir((char*)WMTMP, MODE);

		//identify covers folders to be scanned
#ifndef ENGLISH_ONLY
													covers_exist[0]=isDir(COVERS_PATH);
#endif
		sprintf(param, "%s/covers", MM_ROOT_STD) ;	covers_exist[1]=isDir(param);
		sprintf(param, "%s/covers", MM_ROOT_STL) ;	covers_exist[2]=isDir(param);
		sprintf(param, "%s/covers", MM_ROOT_SSTL);	covers_exist[3]=isDir(param);
													covers_exist[4]=isDir("/dev_hdd0/GAMES/covers");
													covers_exist[5]=isDir("/dev_hdd0/GAMEZ/covers");
													covers_exist[6]=isDir(WMTMP);

		for(u8 i=0; i<12; i++)
		{
			if(cellFsStat(wm_icons[i], &buf)!=CELL_FS_SUCCEEDED)
			{
				sprintf(param, "/dev_flash/vsh/resource/explore/icon/%s", wm_icons[i] + 23); strcpy(wm_icons[i], param);
				if(cellFsStat(param, &buf)==CELL_FS_SUCCEEDED) continue;
				else
				if(i==0 || i==5) strcpy(wm_icons[i] + 32, "user/024.png\0"); else //ps3
				if(i==1 || i==6) strcpy(wm_icons[i] + 32, "user/026.png\0"); else //psx
				if(i==2 || i==7) strcpy(wm_icons[i] + 32, "user/025.png\0"); else //ps2
				if(i==3 || i==8) strcpy(wm_icons[i] + 32, "user/022.png\0"); else //psp
				if(i==4 || i==9) strcpy(wm_icons[i] + 32, "user/023.png\0"); else //dvd
								 strcpy(wm_icons[i] + 37, "icon_home.png\0"); //setup / eject
			}
		}

#ifdef NOSINGSTAR
		if(webman_config->noss) no_singstar_icon();
#endif

#ifdef COBRA_ONLY
		//if(cobra_mode)
		{
			u8 cconfig[15];
			CobraConfig *cobra_config = (CobraConfig*) cconfig;
			memset(cobra_config, 0, 15);
			cobra_read_config(cobra_config);

			if(webman_config->nospoof)
			{
				cobra_config->spoof_version=0;
				cobra_config->spoof_revision=0;
			}
			else
			{   // cobra spoofer not working on 4.53
    			if(c_firmware!=4.53f)
				{
					cobra_config->spoof_version=0x0476;
					cobra_config->spoof_revision=65514;
				}
			}

			if( cobra_config->ps2softemu == 0 && cobra_get_ps2_emu_type()==PS2_EMU_SW )
				cobra_config->ps2softemu =  1;

			cobra_write_config(cobra_config);

		}
#endif

#ifdef SPOOF_CONSOLEID
		spoof_idps_psid();
#endif

#ifdef COBRA_ONLY
	#ifdef REMOVE_SYSCALLS
		if(webman_config->spp & 1) //remove syscalls & history
        {
			sys_timer_sleep(5);

			remove_cfw_syscalls();
			delete_history(true);
		}
		else
	#endif
		if(webman_config->spp & 2) //remove history only
		{
			delete_history(false);
			if(c_firmware<=4.70f) block_online_servers();
		}
#endif

		if(update_mygames_xml(conn_s_p)) mount_autoboot();

		init_running = 0;
		sys_ppu_thread_exit(0);
	}

#ifdef USE_DEBUG
	ssend(debug_s, "waiting...");
#endif
	if(loading_html>10) loading_html=0;
	//while((init_running/* || loading_html>3*/) && working) sys_timer_usleep(10000);

	sys_net_sockinfo_t conn_info_main;
	sys_net_get_sockinfo(conn_s, &conn_info_main, 1);

	char ip_address[16];
	sprintf(ip_address, "%s", inet_ntoa(conn_info_main.remote_adr));
	if( webman_config->bind && ((conn_info_main.local_adr.s_addr!=conn_info_main.remote_adr.s_addr) && strncmp(ip_address, webman_config->allow_ip, strlen(webman_config->allow_ip))!=0))
	{
		sclose(&conn_s);
		loading_html--;
		sys_ppu_thread_exit(0);
	}

	if(!webman_config->netd0 && !webman_config->neth0[0]) strcpy(webman_config->neth0, ip_address);
	if(!webman_config->bind) strcpy(webman_config->allow_ip, ip_address);

	set_buffer_sizes(webman_config->foot);

	_meminfo meminfo;
	u8 retries=0;
again3:
	{system_call_1(SC_GET_FREE_MEM, (uint64_t)(u32) &meminfo);}
	if((meminfo.avail)<( (_64KB_) + MIN_MEM)) //leave if less than min memory
	{
#ifdef USE_DEBUG
	ssend(debug_s, "!!! NOT ENOUGH MEMORY!\r\n");
#endif
		retries++;
		sys_timer_sleep(1);
		if(retries<5) goto again3;
		init_running = 0;
		sclose(&conn_s);
		loading_html--;
		sys_ppu_thread_exit(0);
	}

	sys_addr_t sysmem=0;

	u8 is_binary = 0, served=0;	// served http requests
	u64 c_len = 0;
	char cmd[16], header[HTML_RECV_SIZE];

	u8 is_ps3_http=0;
	u8 is_cpursx=0;
	u8 is_popup=0;

	struct timeval tv;
	tv.tv_usec = 0;
	tv.tv_sec = 3;
	setsockopt(conn_s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	while(!served && working)
	{
		served++;
		header[0]=0;

#ifdef USE_DEBUG
	ssend(debug_s, "ACC - ");
#endif
		if(recv(conn_s, header, HTML_RECV_SIZE, 0) > 0 && header[0]=='G' && header[4]=='/') // serve only GET /xxx requests
		{
			if(strstr(header, "Mozilla/5.0 (PLAYSTATION 3;")) is_ps3_http=1; else
			if(strstr(header, "Gecko/36")) is_ps3_http=2; else is_ps3_http=0;

			header[strcspn(header, "\n")] = '\0';
			header[strcspn(header, "\r")] = '\0';

			ssplit(header, cmd, 15, header, (HTML_RECV_SIZE-1));
			ssplit(header, param, (HTML_RECV_SIZE-1), cmd, 15);

			bool allow_retry_response=true, small_alloc = true, mobile_mode = false;

#ifdef USE_DEBUG
	ssend(debug_s, param);
	ssend(debug_s, "\r\n");
#endif
			//url decode (unescape)
			if(strstr(param, "%"))
			{
				strcpy(header, param);

				u16 pos=0, len=strlen(param);
				for(u16 i=0;i<len;i++, pos++)
				{
					if(header[i]!='%')
						param[pos]=header[i];
					else
					{
						i++;
						if(header[i]>='0' && header[i]<='9') param[pos]=(header[i]-0x30)*0x10; else
						if(header[i]>='A' && header[i]<='F') param[pos]=(header[i]-0x37)*0x10; else
						if(header[i]>='a' && header[i]<='f') param[pos]=(header[i]-0x57)*0x10;

						i++;
						if(header[i]>='0' && header[i]<='9') param[pos]+=header[i]-0x30; else
						if(header[i]>='A' && header[i]<='F') param[pos]+=header[i]-0x37; else
						if(header[i]>='a' && header[i]<='f') param[pos]+=header[i]-0x57;
					}
				}
				param[pos]=0;
			}

 #ifdef VIRTUAL_PAD
			if(strstr(param, "/pad.ps3"))
			{
				parse_pad_command(param);

				is_binary=0;
				http_response(conn_s, header, param, 200, param+9);
				loading_html--;
				sys_ppu_thread_exit(0);
				return;
			}
#endif

#ifndef LITE_EDITION
 #ifdef WEB_CHAT
			if(strstr(param, "/chat.ps3"))
			{
				is_popup=1; is_binary=0;
				goto html_response;
			}
 #endif
			if(strstr(param, "/popup.ps3"))
			{
				if(param[10]==0) show_info_popup = true; else is_popup=1;
				is_binary=0;
				goto html_response;
			}
			if(strstr(param, "/dev_blind"))
			{
				is_binary=2;
				goto html_response;
			}
#endif
			if(strstr(param, "quit.ps3"))
			{
				http_response(conn_s, header, param, 200, param);
#ifdef LOAD_PRX
quit:
#endif
				if(!webman_config->fanc || strstr(param, "?0") || webman_config->ps2temp<33)
					restore_fan(0); //restore syscon fan control mode
				else
					restore_fan(1); //set ps2 fan control mode

				working = 0;
				sclose(&conn_s);
				if(sysmem) sys_memory_free(sysmem);
				loading_html=0;

				stop_prx_module();
				sys_ppu_thread_exit(0);
				break;
			}

			if(strstr(param, "shutdown.ps3"))
			{
				http_response(conn_s, header, param, 200, param);
				working = 0;
				{ DELETE_TURNOFF } { BEEP1 }

				if(strstr(param, "?"))
					vshmain_87BB0001(1); // shutdown using VSH
				else
					{system_call_4(SC_SYS_POWER, SYS_SHUTDOWN, 0, 0, 0);}

				sys_ppu_thread_exit(0);
				break;
			}
			if(strstr(param, "rebuild.ps3"))
			{
				http_response(conn_s, header, param, 200, param);

				cmd[0] = cmd[1] = 0; cmd[2] = 0x03; cmd[3] = 0xE9; // 00 00 03 E9
				savefile((char*)"/dev_hdd0/mms/db.err", cmd, 4);
				goto restart;
			}
			if(strstr(param, "recovery.ps3"))
			{
				#define SC_UPDATE_MANAGER_IF				863
				#define UPDATE_MGR_PACKET_ID_READ_EPROM		0x600B
				#define UPDATE_MGR_PACKET_ID_WRITE_EPROM	0x600C
				#define RECOVER_MODE_FLAG_OFFSET			0x48C61

				http_response(conn_s, header, param, 200, param);

				{system_call_7(SC_UPDATE_MANAGER_IF, UPDATE_MGR_PACKET_ID_WRITE_EPROM, RECOVER_MODE_FLAG_OFFSET, 0x00, 0, 0, 0, 0);} // set recovery mode
				goto reboot; // hard reboot
			}
			if(strstr(param, "restart.ps3"))
			{
				http_response(conn_s, header, param, 200, param);
restart:
				working = 0;
				{ DELETE_TURNOFF } { BEEP2 }
				if(strstr(param,"?0")==NULL) savefile((char*)WMNOSCAN, NULL, 0);

				vshmain_87BB0001(2); // VSH reboot

				sys_ppu_thread_exit(0);
				break;
			}
			if(strstr(param, "reboot.ps3"))
			{
				http_response(conn_s, header, param, 200, param);
reboot:
				working = 0;
				{ DELETE_TURNOFF } { BEEP2 }

				if(strstr(param, "?v"))
					vshmain_87BB0001(2); // VSH reboot
				else
				if(strstr(param, "?q"))
					{system_call_3(SC_SYS_POWER, SYS_REBOOT, NULL, 0);}
				else
				if(strstr(param, "?s"))
					{system_call_3(SC_SYS_POWER, SYS_SOFT_REBOOT, NULL, 0);}
				else
					{system_call_3(SC_SYS_POWER, SYS_HARD_REBOOT, NULL, 0);}

				sys_ppu_thread_exit(0);
				break;
			}

#ifdef FIX_GAME
			if(strstr(param, "/fixgame.ps3"))
			{
				// fix game folder
                char *game_path = param + 12;
				if(cellFsStat((char*)game_path, &buf)==CELL_FS_SUCCEEDED)
				{
					fix_in_progress=true; fix_aborted=false;
#ifdef COBRA_ONLY
					if(strcasestr(game_path, ".iso"))
						fix_iso(game_path, 0x100000UL, false);
					else
#endif //#ifdef COBRA_ONLY
						fix_game(game_path);

					fix_in_progress=false;

					is_popup=1; is_binary=0;
					goto html_response;
				}
			}
#endif

			if(strstr(param, "/games.ps3"))
			{
mobile_response:
				mobile_mode = false;

				if(cellFsStat((char*)MOBILE_HTML, &buf)!=CELL_FS_SUCCEEDED)
					sprintf(param, "/index.ps3%s", param+10);
				else if(strstr(param, "?g="))
					sprintf(param, "%s", MOBILE_HTML);
				else if(strstr(param, "?"))
					{sprintf(param, "/index.ps3%s", param+10); mobile_mode = true;}
				else if(cellFsStat((char*)GAMELIST_JS, &buf)!=CELL_FS_SUCCEEDED)
					sprintf(param, "%s", "index.ps3?mobile");
				else
					sprintf(param, "%s", MOBILE_HTML);
			}
			else mobile_mode = false;

			if(strstr(param, "index.ps3")!=NULL) small_alloc=false;

			if(!is_busy && (strstr(param, "index.ps3?")  ||
#ifdef DEBUG_MEM
							strstr(param, "peek.lv2?")   ||
							strstr(param, "poke.lv2?")   ||
							strstr(param, "find.lv2?")   ||
							strstr(param, "peek.lv1?")   ||
							strstr(param, "poke.lv1?")   ||
							strstr(param, "find.lv1?")   ||
							strstr(param, "dump.ps3")    ||
#endif

#ifndef LITE_EDITION
							strstr(param, "delete.ps3")  ||
							strstr(param, "delete_ps3")  ||
#endif

#ifdef PS3MAPI
							strstr(param, "home.ps3mapi")     ||
							strstr(param, "setmem.ps3mapi")   ||
							strstr(param, "getmem.ps3mapi")   ||
							strstr(param, "led.ps3mapi")      ||
							strstr(param, "buzzer.ps3mapi")   ||
							strstr(param, "notify.ps3mapi")   ||
							strstr(param, "syscall.ps3mapi")  ||
							strstr(param, "syscall8.ps3mapi") ||
							strstr(param, "setidps.ps3mapi")  ||
							strstr(param, "vshplugin.ps3mapi")  ||
							strstr(param, "gameplugin.ps3mapi") ||
#endif

#ifdef COPY_PS3
							strstr(param, "copy.ps3/") ||
#endif
							strstr(param, "refresh.ps3")
			))
				is_binary=0;
			else if(strstr(param, "cpursx.ps3")  ||
					strstr(param, "index.ps3")   ||
					strstr(param, "setup.ps3")   ||
					strstr(param, "mount_ps3/")  ||
					strstr(param, "mount.ps3/")  ||
#ifdef PS2_DISC
					strstr(param, "mount.ps2/")  ||
					strstr(param, "mount_ps2/")  ||
#endif

#ifdef VIDEO_REC
					strstr(param, "videorec.ps3") ||
#endif

#ifdef EXT_GDATA
					strstr(param, "extgd.ps3")   ||
#endif

#ifdef SYS_BGM
					strstr(param, "sysbgm.ps3")  ||
#endif

#ifdef LOAD_PRX
					strstr(param, "loadprx.ps3") ||
#endif

					strstr(param, "eject.ps3")   ||
					strstr(param, "insert.ps3"))
				is_binary=0;
			else if(param[1]=='n' && param[2]=='e' && param[3]=='t' && (param[4]>='0' && param[4]<='2')) //net0/net1/net2
			{
				is_binary=2; small_alloc = false;
			}
			else
			{
				is_binary=(cellFsStat(param, &buf)==CELL_FS_SUCCEEDED);

				if(!is_binary) {strcpy(header, param);  sprintf(param, "%s/%s", html_base_path, header); is_binary=(cellFsStat(param, &buf)==CELL_FS_SUCCEEDED);} // use html path (if path is omitted)

				if(is_binary)
				{
					c_len=buf.st_size;
					if((buf.st_mode & S_IFDIR) != 0) {is_binary=2; small_alloc = false;} // folder listing
				}
				else
				{
					c_len=0;
					is_binary=0;
					http_response(conn_s, header, param, is_busy ? 503:400, is_busy ? (char*)"503 Server is Busy":(char*)"400 Bad Request");
					loading_html--;
					sys_ppu_thread_exit(0);
				}
			}

html_response:
			prepare_header(header, param, is_binary);
			char templn[1024];
			{u16 ulen=strlen(param); if(ulen>1 && param[ulen-1]=='/') param[ulen-1]=0;}
			//sprintf(templn, "X-PS3-Info: %llu [%s]\r\n", (unsigned long long)c_len, param); strcat(header, templn);

			//-- select content profile
			if(strstr(param, ".ps3?"))
			{
				u8 uprofile=profile; char url[10]; bool is_index_ps3 = (strstr(param, "index.ps3?")!=NULL);

				for(u8 i=0;i<5;i++)
				{
					sprintf(url, "?%i", i); if(strstr(param, url)) {profile=i; break;}
					sprintf(url, "usr=%i", i); if(strstr(param, url)) {profile=i; break;}
					if(is_index_ps3) {sprintf(url, "_%i", i); if(strstr(param, url)) {profile=i; break;}}
				}

				if(uprofile!=profile) {webman_config->profile=profile; save_settings();}
				if((uprofile!=profile) || is_index_ps3) {DELETE_CACHED_GAMES}
			}
			//--

			if(is_binary==1) //file
			{
				sprintf(templn, "Content-Length: %llu\r\n\r\n", (unsigned long long)c_len); strcat(header, templn);
				ssend(conn_s, header);
				if(!sysmem && sys_memory_allocate(_64KB_, SYS_MEMORY_PAGE_SIZE_64K, &sysmem)!=0)
				{
					sclose(&conn_s);
					loading_html--;
					sys_ppu_thread_exit(0);
				}
				char *buffer= (char*)sysmem;
				if(cellFsOpen(param, CELL_FS_O_RDONLY, &fd, NULL, 0) == CELL_FS_SUCCEEDED)
				{
					u64 read_e = 0, pos;
					cellFsLseek(fd, 0, CELL_FS_SEEK_SET, &pos);

					while(working)
					{
						//sys_timer_usleep(500);
						if(cellFsRead(fd, (void *)buffer, _64KB_, &read_e)==CELL_FS_SUCCEEDED)
						{
							if(read_e>0)
							{
								if(send(conn_s, buffer, (size_t)read_e, 0)<0) break;
							}
							else
								break;
						}
						else
							break;
					}
					cellFsClose(fd);
				}
				sys_memory_free(sysmem);
				sclose(&conn_s);
				loading_html--;
				sys_ppu_thread_exit(0);

			}
			if(strstr(param, "cpursx.ps3") || show_info_popup)
			{
				if(!sysmem && sys_memory_allocate(_64KB_, SYS_MEMORY_PAGE_SIZE_64K, &sysmem)!=0)
				{
					sclose(&conn_s);
					loading_html--;
					sys_ppu_thread_exit(0);
				}
				is_cpursx=1;
			}
			else
			{
				if(!small_alloc)
				{
					set_buffer_sizes(webman_config->foot);

					_meminfo meminfo;
					{system_call_1(SC_GET_FREE_MEM, (uint64_t)(u32) &meminfo);}

					if((meminfo.avail)<( (BUFFER_SIZE_ALL) + MIN_MEM)) set_buffer_sizes(3); //MIN+
					if((meminfo.avail)<( (BUFFER_SIZE_ALL) + MIN_MEM)) set_buffer_sizes(1); //MIN
				}

				if(!sysmem && sys_memory_allocate(small_alloc?_64KB_:BUFFER_SIZE_ALL, SYS_MEMORY_PAGE_SIZE_64K, &sysmem)!=0)
				{
					sclose(&conn_s);
					loading_html--;
					sys_ppu_thread_exit(0);
				}
			}

			char *buffer= (char*)sysmem;
			//else	// text page
			{
				if(is_binary!=2 && strstr(param, "setup.ps3?"))
				{
					setup_parse_settings(param);
				}

				bool mount_ps3 = !is_popup && (strstr(param, "mount_ps3")!=NULL), forced_mount = false;

				if(mount_ps3 && View_Find("game_plugin")) {mount_ps3=false; forced_mount=true;}

				prepare_html(buffer, templn, param, is_ps3_http, is_cpursx, mount_ps3);

				if(is_cpursx)
				{
					cpu_rsx_stats(buffer, templn, param);

					is_cpursx = 0; goto send_response;

					//CellGcmConfig config; cellGcmGetConfiguration(&config);
					//sprintf(templn, "localAddr: %x", (u32) config.localAddress); strcat(buffer, templn);
				}
				else if(!mount_ps3)
				{
					u32 t1=0, t2=0;
					get_temperature(0, &t1); // 3E030000 -> 3E.03°C -> 62.(03/256)°C
					get_temperature(1, &t2);
					t1=t1>>24;
					t2=t2>>24;

					sprintf(templn, " [<a href=\"/cpursx.ps3\">CPU: %i°C | RSX: %i°C</a>]<hr>"
									"<form action=\"\">", t1, t2); strcat(buffer, templn);

					if((webman_config->homeb) && (strlen(webman_config->home_url)>0))
					{sprintf(templn, HTML_BUTTON_FMT, HTML_BUTTON, STR_HOME, HTML_ONCLICK, webman_config->home_url); strcat(buffer, templn);}

					sprintf(templn, HTML_BUTTON_FMT
									HTML_BUTTON_FMT
									HTML_BUTTON_FMT
#ifdef EXT_GDATA
									HTML_BUTTON_FMT
#endif
									, HTML_BUTTON, STR_EJECT, HTML_ONCLICK, "/eject.ps3"
									, HTML_BUTTON, STR_INSERT, HTML_ONCLICK, "/insert.ps3"
									, HTML_BUTTON, STR_UNMOUNT, HTML_ONCLICK, "/mount.ps3/unmount"
#ifdef EXT_GDATA
									, HTML_BUTTON, "gameDATA", HTML_ONCLICK, "/extgd.ps3"
#endif
                    ); strcat(buffer, templn);
#ifdef COPY_PS3
					if(((strstr(param, "/dev_") && strlen(param)>12 && !strstr(param,"?")) || strstr(param, "/dev_bdvd")) && !strstr(param,".ps3/") && !strstr(param,".ps3?"))
					{sprintf(templn, "%s%s\" onclick='window.location.href=\"/copy.ps3%s\";'\">", HTML_BUTTON, STR_COPY, param); strcat(buffer, templn);}
#endif
					sprintf(templn,  "%s%s XML%s\" %s'%s';\"> "
									 "%s%s HTML%s\" %s'%s';\">"
									 HTML_BUTTON_FMT
									 HTML_BUTTON_FMT
									 "</form><hr>",
									 HTML_BUTTON, STR_REFRESH, SUFIX2(profile), HTML_ONCLICK, "/refresh.ps3",
									 HTML_BUTTON, STR_REFRESH, SUFIX2(profile), HTML_ONCLICK, "/index.ps3?html",
									 HTML_BUTTON, STR_SHUTDOWN, HTML_ONCLICK, "/shutdown.ps3",
									 HTML_BUTTON, STR_RESTART, HTML_ONCLICK, "/restart.ps3"); strcat(buffer, templn);
				}

				char tempstr[_4KB_];

#ifndef LITE_EDITION
				if(is_popup)
				{
#ifdef WEB_CHAT
					if(strstr(param, "/chat.ps3"))
					{
						webchat(buffer, templn, param, tempstr, conn_info_main);
					}
					else
#endif
#ifdef FIX_GAME
					if(strstr(param, "/fixgame.ps3"))
					{
						char *game_path = param + 12;
						sprintf(templn, "Fixed: %s", game_path);
						templn[211]=0; //limit message to 200 characters
						show_msg((char*)templn);
						sprintf(templn, "Fixed: <a href=\"%s\">%s</a>", game_path, game_path); strcat(buffer, templn);
					}
					else
#endif
					{
						param[211]=0; //limit message to 200 characters
						show_msg((char*)(param+11));
						sprintf(templn, "Message sent: %s", param+11); strcat(buffer, templn);
					}

					is_popup=0; goto send_response;
				}
#endif
				if(is_binary==2) // folder listing
				{
					if(folder_listing(buffer, templn, param, conn_s, tempstr, header) == false)
					{
						if(sysmem) sys_memory_free(sysmem);
						loading_html--;
						sys_ppu_thread_exit(0);
					}
				}
				else
				{
					is_busy=true;

					if(strstr(param, "refresh.ps3") && init_running==0)
					{
						init_running = 1;
						refresh_xml(templn);
						sprintf(templn,  "<br>%s", STR_XMLRF); strcat(buffer, templn);
					}
					else
					if(strstr(param, "eject.ps3"))
					{
						eject_insert(1, 0);
						strcat(buffer, STR_EJECTED);
					}
					else
					if(strstr(param, "insert.ps3"))
					{
						eject_insert(0, 1);
						strcat(buffer, STR_LOADED);
					}
#ifdef LOAD_PRX
					else
					if(strstr(param, "loadprx.ps3"))
					{
						char *pos; unsigned int slot=6; bool prx_found;

						pos=strstr(param, "slot=");
						if(pos)
						{
							get_value(templn, pos + 5, 2);
							slot=RANGE((unsigned int)val(templn), 1, 6);
						}

						if(param[12]=='/') sprintf(templn, "%s", param+12);
						else
						{
							sprintf(templn, "/dev_hdd0/plugins/webftp_server.sprx");
							if(cellFsStat(templn, &buf)!=CELL_FS_SUCCEEDED) sprintf(templn, "/dev_hdd0/webftp_server.sprx");

							pos=strstr(param, "prx=");
							if(pos) get_value(templn, pos + 4, MAX_PATH_LEN);
						}

						prx_found = (cellFsStat(templn, &buf)==CELL_FS_SUCCEEDED);

						if(prx_found)
							sprintf(param, "slot: %i<br>load prx: %s", slot, templn);
						else
							sprintf(param, "unload slot: %i", slot);

						strcat(buffer, param); strcat(buffer, "</font></body></html>");

						cobra_unload_vsh_plugin(slot);

						if(prx_found)
							{cobra_load_vsh_plugin(slot, templn, NULL, 0); if(strstr(templn, "/webftp_server")) goto quit;}
					}
#endif

#ifdef VIDEO_REC
					else
					if(strstr(param, "videorec.ps3"))
					{
						toggle_video_rec();
						strcat(buffer,	"<a class=\"f\" href=\"/dev_hdd0\">/dev_hdd0/</a><a href=\"/dev_hdd0/VIDEO\">VIDEO</a>:<p>"
										"Video recording: <a href=\"/videorec.ps3\">");
						strcat(buffer, recording?STR_ENABLED:STR_DISABLED);
						strcat(buffer, "</a>");
					}
#endif

#ifdef EXT_GDATA
					else
					if(strstr(param, "extgd.ps3"))
					{
						if(strstr(param,"?s" /*status */ )); else
						if(strstr(param,"?e" /*enable */ ) || strstr(param, "?1"))  extgd=1; else
						if(strstr(param,"?d" /*disable*/ ) || strstr(param, "?0"))  extgd=0; else
																					extgd=extgd^1;

						strcat(buffer, "External Game DATA: ");
						if(set_gamedata_status(extgd, true))
							strcat(buffer, STR_ERROR);
						else
							strcat(buffer, extgd?STR_ENABLED:STR_DISABLED);
					}
#endif

#ifdef SYS_BGM
					else
					if(strstr(param, "sysbgm.ps3"))
					{
						static int system_bgm=0;

						if(system_bgm==0)
						{
							system_bgm=-1;
							BgmPlaybackEnable  = (void*)((int)getNIDfunc("vshmain", 0xEDAB5E5E, 16*2));
							BgmPlaybackDisable = (void*)((int)getNIDfunc("vshmain", 0xEDAB5E5E, 17*2));
						}

						if(strstr(param, "?1") || strstr(param, "?e")) system_bgm=-1; //enable
						if(strstr(param, "?0") || strstr(param, "?d")) system_bgm=1;  //disable

						if(strstr(param, "?s")) goto bgm_status;

						int * arg2;
						if(system_bgm<0)
						{
							BgmPlaybackEnable(0, &arg2); system_bgm=1;
						}
						else
						{
							BgmPlaybackDisable(0, &arg2); system_bgm=-1;
						}

bgm_status:
						sprintf(templn, "System BGM: %s", (system_bgm>0)?STR_ENABLED:STR_DISABLED);
						strcat(buffer, templn);
						show_msg(templn);
					}
#endif

#ifdef DEBUG_MEM
					else
					if(strstr(param, "dump.ps3"))
					{
						ps3mapi_mem_dump(buffer, templn, param);
					}
					else
					if(strstr(param, "peek.lv") || strstr(param, "poke.lv") || strstr(param, "find.lv"))
					{
						ps3mapi_find_peek_poke(buffer, templn, param);
					}
#endif
					else
					if(strstr(param, "setup.ps3?"))
					{
						if(strstr(param, "&")==NULL)
						{
							cellFsUnlink(WMCONFIG);
							reset_settings();
						}
						if(save_settings() == CELL_FS_SUCCEEDED)
						{
							sprintf(templn, "<br> %s", STR_SETTINGSUPD); strcat(buffer, templn);
						}
						else
							strcat(buffer, STR_ERROR);
					}
					else
					if(strstr(param, "setup.ps3"))
					{
                        setup_form(buffer, templn);
					}
#ifndef LITE_EDITION
					else
					if(strstr(param, "/delete.ps3") || strstr(param, "/delete_ps3"))
					{
						bool is_reset = false;
						if(strstr(param, "?wmreset")) is_reset=true;
						if(is_reset || strstr(param, "?wmconfig")) {cellFsUnlink(WMCONFIG); reset_settings(); sprintf(param, "/delete_ps3%s", WMCONFIG);}
						if(is_reset || strstr(param, "?wmtmp")) strcpy(param, "/delete_ps3/dev_hdd0/tmp/wmtmp\0");

						if(strstr(param, "?history"))
							{delete_history(true); sprintf(templn, "%s : history", STR_DELETE);}
						else if(strstr(param, "?uninstall"))
						{
							if(cellFsStat((char*)"/dev_hdd0/boot_plugins.txt", &buf)==CELL_FS_SUCCEEDED && buf.st_size<40) cellFsUnlink((char*)"/dev_hdd0/boot_plugins.txt");
							cellFsUnlink((char*)"/dev_hdd0/webftp_server.sprx");
							cellFsUnlink((char*)"/dev_hdd0/plugins/webftp_server.sprx");
							cellFsUnlink((char*)WMCONFIG);
							del((char*)WMTMP, true);
							del((char*)"/dev_hdd0/xmlhost", true);
							del((char*)"/dev_hdd0/tmp/wm_lang", true);
							del((char*)"/dev_hdd0/tmp/wm_icons", true);
							http_response(conn_s, header, param, 200, param);
							goto restart;
						}
						else if(del(param+11, (strstr(param, "/delete.ps3")!=NULL)))
							sprintf(templn, "%s %s : <a href=\"%s\">%s</a><br>", STR_DELETE, STR_ERROR, param+11, param+11);
						else
							sprintf(templn, "%s : <a href=\"%s\">%s</a><br>", STR_DELETE, param+11, param+11);

						strcat(buffer, templn);
					}
#endif

#ifdef PS3MAPI
					else
					if(strstr(param, "home.ps3mapi"))
					{
						ps3mapi_home(buffer, templn);
					}
					else
					if(strstr(param, "buzzer.ps3mapi"))
					{
						ps3mapi_buzzer(buffer, templn, param);
					}
					else
					if(strstr(param, "led.ps3mapi"))
					{
						ps3mapi_led(buffer, templn, param);
					}
					else
					if(strstr(param, "notify.ps3mapi"))
					{
						ps3mapi_notify(buffer, templn, param);
					}
					else
					if(strstr(param, "syscall.ps3mapi"))
					{
						ps3mapi_syscall(buffer, templn, param);
					}
					else
					if(strstr(param, "syscall8.ps3mapi"))
					{
						ps3mapi_syscall8(buffer, templn, param);
					}
					else
					if(strstr(param, "getmem.ps3mapi"))
					{
						ps3mapi_getmem(buffer, templn, param);
					}
					else
					if(strstr(param, "setmem.ps3mapi"))
					{
						ps3mapi_setmem(buffer, templn, param);
					}
					else
					if(strstr(param, "setidps.ps3mapi"))
					{
						ps3mapi_setidps(buffer, templn, param);
					}
					else
					if(strstr(param, "vshplugin.ps3mapi"))
					{
						ps3mapi_vshplugin(buffer, templn, param);
					}
					else
					if(strstr(param,"gameplugin.ps3mapi"))
					{
						ps3mapi_gameplugin(buffer, templn, param);
					}
#endif

					else
#ifdef PS2_DISC
					if(mount_ps3 || forced_mount || !memcmp(param, "/mount.ps3", 10) || !memcmp(param, "/mount.ps2", 10) || !memcmp(param, "/mount_ps2", 10) || !memcmp(param, "/copy.ps3", 9))
#else
					if(mount_ps3 || forced_mount || !memcmp(param, "/mount.ps3", 10) || !memcmp(param, "/copy.ps3", 9))
#endif
					{
                        game_mount(buffer, templn, param, tempstr, is_binary, mount_ps3, forced_mount);
					}
					else
					{
						mobile_mode|=(strstr(param, "?mob")!=NULL || strstr(param, "&mob")!=NULL);

						if(game_listing(buffer, templn, param, conn_s, tempstr, mobile_mode) == false)
						{
							is_busy=false;
							if(sysmem) sys_memory_free(sysmem);
							loading_html--;
							sys_ppu_thread_exit(0);
							break;
                        }
					}

					is_busy=false;
				}

send_response:
				if(mobile_mode && allow_retry_response) {allow_retry_response=false; goto mobile_response;}

				if(mount_ps3)
					strcat(buffer, "<script type=\"text/javascript\">window.close(this);</script>"); //auto-close
				else
					strcat(buffer, "</font></body></html>"); //end-html

				sprintf(templn, "Content-Length: %llu\r\n\r\n", (unsigned long long)strlen(buffer)); strcat(header, templn);
				ssend(conn_s, header);
				ssend(conn_s, buffer);
				buffer[0]=0;
			}
		}

		break;
	}

#ifdef USE_DEBUG
	ssend(debug_s, "Request served.\r\n");
#endif

	sclose(&conn_s);
	if(sysmem) sys_memory_free(sysmem);
	loading_html--;
	sys_ppu_thread_exit(0);
}

/*
	u64 sa=0x03300000ULL;
	u64 offset=0;
	for(u64 i=0;i<0x7700000;i+=8)
	{
		if( peek(0x8000000000000000ULL+i+sa	 )==0xFFFFFF00FFFFFF00ULL &&
			peek(0x8000000000000000ULL+i+sa+8)==0xFFFFFF00FFFFFF00ULL )
		{
			show_msg((char*)"Found match!");
			offset=i+sa+16;
			for(u64 i2=offset;i2<(offset+320*4*90);i2+=8)
				poke(0x8000000000000000ULL+i2, 0x30405060708090A0ULL);
		}
	}
*/

static void poll_thread(uint64_t poll)
{
	/*u8 d0[157];
	u8 d1[157];
	u8 d0t[157];
	u8 d1t[157];
	int u0=0, u1=0;
	char un[128];

	while(working)
	{
		if(u0<128)
		{
			int fd;
			u64 read_e;
			bool toupd0, toupd1;
			cellFsOpen((char*)"/dev_hdd0/vsh/task/00000001/d0.pdb", CELL_FS_O_RDONLY, &fd, NULL, 0);
			cellFsRead(fd, (void *)&d0, 157, &read_e); cellFsClose(fd);
			cellFsOpen((char*)"/dev_hdd0/vsh/task/00000001/d1.pdb", CELL_FS_O_RDONLY, &fd, NULL, 0);
			cellFsRead(fd, (void *)&d1, 157, &read_e); cellFsClose(fd);
			toupd0=0;
			toupd1=0;
			for(u8 b=0;b<157;b++)
			{
				if(d0[b]!=d0t[b]) toupd0=1;
				if(d1[b]!=d1t[b]) toupd1=1;
				d0t[b]=d0[b];
				d1t[b]=d1[b];
			}
			if(toupd0)
			{
				u0++;
				sprintf(un, "/dev_hdd0/tmp/d0-%03i.bin", u0);
				cellFsOpen(un, CELL_FS_O_CREAT|CELL_FS_O_WRONLY, &fd, NULL, 0);
				cellFsWrite(fd, (void *)d0, 157, &read_e);
				cellFsClose(fd);
			}
			if(toupd1)
			{
				u1++;
				sprintf(un, "/dev_hdd0/tmp/d1-%03i.bin", u1);
				cellFsOpen(un, CELL_FS_O_CREAT|CELL_FS_O_WRONLY, &fd, NULL, 0);
				cellFsWrite(fd, (void *)d1, 157, &read_e);
				cellFsClose(fd);
			}
		}
	}
	*/

	u8 to=0;
	u8 sec=0;
	u8 step=3;
	u32 t1=0, t2=0;
	u8 lasttemp=0;
	old_fan=0;
	u8 stall=0;
	u8 step_up=5;
	//u8 step_down=2;
	u8 smoothstep=0;
	int delta=0;
	uint32_t usb_handle = -1;
	uint8_t msg[200];

	uint32_t r;

	CellPadData data;

	old_fan=0;
	while(working)
	{
		if(max_temp)
		{
			t1=0;
			get_temperature(0, &t1); // 3E030000 -> 3E.03°C -> 62.(03/256)°C
			t2=t1;
			sys_timer_usleep(300000);

			get_temperature(1, &t2); // 3E030000 -> 3E.03°C -> 62.(03/256)°C
			sys_timer_usleep(200000);

			t1=t1>>24;
			t2=t2>>24;

			if(t2>t1) t1=t2;

			if(!lasttemp) lasttemp=t1;

			delta=(lasttemp-t1);

			lasttemp=t1;

			if(t1>=max_temp || t1>84)
			{
				if(delta< 0) fan_speed+=2;
				if(delta==0 && t1!=(max_temp-1)) fan_speed++;
				if(delta==0 && t1>=(max_temp+1)) fan_speed+=(2+(t1-max_temp));
				if(delta> 0)
				{
					smoothstep++;
					if(smoothstep>1)
					{
						fan_speed--;
						smoothstep=0;
					}
				}
				if(t1>84)	 fan_speed+=step_up;
				if(delta< 0 && (t1-max_temp)>=2) fan_speed+=step_up;
			}
			else
			{
				if(delta< 0 && t1>=(max_temp-1)) fan_speed+=2;
				if(delta==0 && t1<=(max_temp-2))
				{
					smoothstep++;
					if(smoothstep>1)
					{
						fan_speed--;
						if(t1<=(max_temp-3)) {fan_speed--; if(fan_speed>0xA8) fan_speed--;} // 66%
						if(t1<=(max_temp-5)) {fan_speed--; if(fan_speed>0x80) fan_speed--;} // 50%
						smoothstep=0;
					}
				}
				//if(delta==0 && t1>=(max_temp-1)) fan_speed++;
				if(delta> 0)
				{
					//smoothstep++;
					//if(smoothstep)
					{
						fan_speed--;
						if(t1<=(max_temp-3)) {fan_speed--; if(fan_speed>0xA8) fan_speed--;} // 66%
						if(t1<=(max_temp-5)) {fan_speed--; if(fan_speed>0x80) fan_speed--;} // 50%
						smoothstep=0;
					}
				}
			}

			if(t1>76 && old_fan<0x43) fan_speed++;
			if(t1>84 && fan_speed<0xB0) {old_fan=0; fan_speed=0xB0;}

			if(fan_speed<((webman_config->minfan*255)/100)) fan_speed=(webman_config->minfan*255)/100;
			if(fan_speed>MAX_FANSPEED) fan_speed=MAX_FANSPEED;

			//sprintf(debug, "OFAN: %x | CFAN: %x | TEMP: %i | STALL: %i\r\n", old_fan, fan_speed, t1, stall);	ssend(data_s, mytxt);
			//if(abs(old_fan-fan_speed)>=0x0F || stall>35 || (abs(old_fan-fan_speed) /*&& webman_config->aggr*/))
			if(old_fan!=fan_speed || stall>35)
			{
				//if(t1>76 && fan_speed<0x50) fan_speed=0x50;
				//if(t1>77 && fan_speed<0x58) fan_speed=0x58;
				if(t1>78 && fan_speed<0x50) fan_speed+=2;
				if(old_fan!=fan_speed)
				{
				old_fan=fan_speed;
				fan_control(fan_speed, 1);
				//sprintf(debug, "OFAN: %x | CFAN: %x | TEMP: %i | SPEED APPLIED!\r\n", old_fan, fan_speed, t1); ssend(data_s, mytxt);
				stall=0;
			}
			}
			else
				if( old_fan>fan_speed && (old_fan-fan_speed)>8 && t1<(max_temp-3) )
					stall++;
		}

/*
 FAIL SAFE    : SELECT+L3+L2+R2
 RESET SAFE   : SELECT+R3+L2+R2

 REFRESH XML  : SELECT+L3 (+R2=profile1, +L2=profile2)
 UNLOAD WM    : L3+R2+R3

 PREV GAME    : SELECT+L1
 NEXT GAME    : SELECT+R1
 SHUTDOWN     : L3+R2+X
 RESTART      : L3+R2+O

 FAN CNTRL    : L3+R2+START
 SHOW TEMP    : SELECT+R3 / SELECT+START
 DYNAMIC TEMP : SELECT+LEFT/RIGHT
 MANUAL TEMP  : SELECT+UP/DOWN

 SYSCALLS     : R2+TRIANGLE
 SHOW IDPS    : R2+O
 OFFLINE MODE : R2+口

 EXT GAME DATA: SELECT+口
 MOUNT net0/  : SELECT+R2+口
 MOUNT net1/  : SELECT+L2+口

 TOGGLE PS2CLASSIC    : SELECT+L2+TRIANGLE
 SWITCH PS2EMU        : SELECT+L2+R2

 COBRA TOGGLE         : L3+L2+TRIANGLE
 REBUG  Mode Switcher : L3+L2+口
 Normal Mode Switcher : L3+L2+O
 DEBUG  Menu Switcher : L3+L2+X
*/
		bool reboot = false;

#ifdef COBRA_ONLY
		struct CellFsStat s;
#endif

		for(u8 n=0;n<10;n++)
		{
			if(show_info_popup) {show_info_popup = false; goto show_popup;}

			if(!webman_config->nopad)
			{
				data.len=0;
				if(cellPadGetData(0, &data) != CELL_PAD_OK || data.len == 0)
					if(cellPadGetData(1, &data) != CELL_PAD_OK || data.len == 0)
						if(cellPadGetData(2, &data) != CELL_PAD_OK) {sys_timer_usleep(300000); continue;}

				if(data.len > 0)
				{
					if((data.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_SELECT))
					{
						if( !(webman_config->combo2 & (EXTGAMDAT | MOUNTNET0 | MOUNTNET1))         // Toggle External Game Data
                            && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_SQUARE)) // SELECT+SQUARE
						{
#ifndef LITE_EDITION
							if(!(webman_config->combo2 & MOUNTNET0) &&
								(data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R2))
							{if(webman_config->netp0 && webman_config->neth0[0]) mount_with_mm((char*)"/net0", 1);}
							else
							if(!(webman_config->combo2 & MOUNTNET1) &&
								(data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L2))
							{if(webman_config->netp1 && webman_config->neth1[0]) mount_with_mm((char*)"/net1", 1);}
							else
#endif

#ifdef EXT_GDATA
							set_gamedata_status(extgd^1, true);
#endif
							sys_timer_sleep(2);
							break;
						}
						else
						if( !(webman_config->combo & FAIL_SAFE)
							&& (data.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_L3)
							&& (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L2) // fail-safe mode
							&& (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R2) // SELECT+L3+L2+R2
							)
						{
							cellFsUnlink((char*)"/dev_hdd0/boot_plugins.txt");
							goto reboot; // vsh reboot
						}
						else
						if( (data.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_R3)
							&& (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L2) // reset-safe mode
							&& (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R2) // SELECT+R3+L2+R2
							)
						{
							cellFsUnlink(WMCONFIG);
							{ BEEP1 }
							show_msg((char*)STR_RMVWMCFG);
							sys_timer_sleep(2);
							show_msg((char*)STR_RMVWMCFGOK);
							sys_timer_sleep(3);
							goto reboot; // vsh reboot
						}
#ifdef COBRA_ONLY
						else
						if( !(webman_config->combo2 & PS2TOGGLE)
							&& (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L2)
							&& (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_TRIANGLE) // SELECT+L2+TRIANGLE
							&& (c_firmware>=4.65f) )
						{
							bool classic_ps2_enabled = (cellFsStat((char*)PS2_CLASSIC_TOGGLER, &s)==CELL_FS_SUCCEEDED);

							if(classic_ps2_enabled)
							{
								disable_classic_ps2_mode();
							}
							else
							{
								enable_classic_ps2_mode();
							}

							sprintf((char*) msg, (char*)"PS2 Classic %s", classic_ps2_enabled ? STR_DISABLED : STR_ENABLED);
							show_msg((char*) msg);
							sys_timer_sleep(3);
						}
						else
						if( !(webman_config->combo2 & PS2SWITCH)
							&& (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L2) // Clone ps2emu habib's switcher
							&& (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R2) // SELECT+L2+R2
							&& (c_firmware>=4.53f) )
						{
								enable_dev_blind((char*)"Swapping ps2emu activated!");
 #ifdef REX_ONLY
								if(cellFsStat((char*)REBUG_TOOLBOX "ps2_netemu.self", &s)==CELL_FS_SUCCEEDED)
								{
									uint64_t size1, size2;

									// ---- Backup PS2Emus to Rebug Toolbox folder ----
									if( cellFsStat( (char*)REBUG_TOOLBOX "ps2_netemu.self.cobra", &s)!=CELL_FS_SUCCEEDED )
										  filecopy( (char*)PS2_EMU_PATH  "ps2_netemu.self",
													(char*)REBUG_TOOLBOX "ps2_netemu.self.cobra", COPY_WHOLE_FILE);

									if( cellFsStat( (char*)REBUG_TOOLBOX "ps2_gxemu.self.cobra", &s)!=CELL_FS_SUCCEEDED )
										  filecopy( (char*)PS2_EMU_PATH  "ps2_gxemu.self",
													(char*)REBUG_TOOLBOX "ps2_gxemu.self.cobra", COPY_WHOLE_FILE);

									if( cellFsStat( (char*)REBUG_TOOLBOX "ps2_emu.self.cobra", &s)!=CELL_FS_SUCCEEDED )
										  filecopy( (char*)PS2_EMU_PATH  "ps2_emu.self",
													(char*)REBUG_TOOLBOX "ps2_emu.self.cobra", COPY_WHOLE_FILE);

									// ---- Swap ps2_netemu.self ----
									size1 = size2 = 0;
									if( cellFsStat((char*)PS2_EMU_PATH  "ps2_netemu.self", &s)==CELL_FS_SUCCEEDED) size1 = s.st_size;
									if( cellFsStat((char*)REBUG_TOOLBOX "ps2_netemu.self", &s)==CELL_FS_SUCCEEDED) size2 = s.st_size;

									show_msg((size1==size2) ?   (char*)"Restoring original Cobra ps2emu...":
																(char*)"Switching to custom ps2emu...");

									if(size1>0 && size2>0)
										filecopy((size1==size2) ?   (char*)REBUG_TOOLBOX "ps2_netemu.self.cobra":
																	(char*)REBUG_TOOLBOX "ps2_netemu.self",
																	(char*)PS2_EMU_PATH  "ps2_netemu.self", COPY_WHOLE_FILE);

									// ---- Swap ps2_gxemu.self ----
									size1 = size2 = 0;
									if( cellFsStat((char*)PS2_EMU_PATH  "ps2_gxemu.self", &s)==CELL_FS_SUCCEEDED) size1 = s.st_size;
									if( cellFsStat((char*)REBUG_TOOLBOX "ps2_gxemu.self", &s)==CELL_FS_SUCCEEDED) size2 = s.st_size;

									if(size1>0 && size2>0)
										filecopy((size1==size2) ?   (char*)REBUG_TOOLBOX "ps2_gxemu.self.cobra":
																	(char*)REBUG_TOOLBOX "ps2_gxemu.self",
																	(char*)PS2_EMU_PATH  "ps2_gxemu.self", COPY_WHOLE_FILE);

									// ---- Swap ps2_emu.self ----
									size1 = size2 = 0;
									if( cellFsStat((char*)PS2_EMU_PATH  "ps2_emu.self", &s)==CELL_FS_SUCCEEDED) size1 = s.st_size;
									if( cellFsStat((char*)REBUG_TOOLBOX "ps2_emu.self", &s)==CELL_FS_SUCCEEDED) size2 = s.st_size;

									if(size1>0 && size2>0)
										filecopy((size1==size2) ?   (char*)REBUG_TOOLBOX "ps2_emu.self.cobra":
																	(char*)REBUG_TOOLBOX "ps2_emu.self",
																	(char*)PS2_EMU_PATH  "ps2_emu.self", COPY_WHOLE_FILE);
								}
								else
 #endif //#ifdef REX_ONLY
								if(cellFsStat((char*)PS2_EMU_PATH "ps2_netemu.self.swap", &s)==CELL_FS_SUCCEEDED)
								{
									show_msg((char*)"Switch to custom ps2emu...");

									cellFsRename(PS2_EMU_PATH "ps2_netemu.self"     , PS2_EMU_PATH "ps2_netemu.tmp");
									cellFsRename(PS2_EMU_PATH "ps2_netemu.self.swap", PS2_EMU_PATH "ps2_netemu.self");

									cellFsRename(PS2_EMU_PATH "ps2_gxemu.self"      , PS2_EMU_PATH "ps2_gxemu.tmp");
									cellFsRename(PS2_EMU_PATH "ps2_gxemu.self.swap" , PS2_EMU_PATH "ps2_gxemu.self");

									cellFsRename(PS2_EMU_PATH "ps2_emu.self"        , PS2_EMU_PATH "ps2_emu.tmp");
									cellFsRename(PS2_EMU_PATH "ps2_emu.self.swap"   , PS2_EMU_PATH "ps2_emu.self");
								}
								else if(cellFsStat((char*)PS2_EMU_PATH "ps2_netemu.self.sp", &s)==CELL_FS_SUCCEEDED)
								{
									show_msg((char*)"Switching to custom ps2emu...");

									cellFsRename(PS2_EMU_PATH "ps2_netemu.self"   , PS2_EMU_PATH "ps2_netemu.tmp");
									cellFsRename(PS2_EMU_PATH "ps2_netemu.self.sp", PS2_EMU_PATH "ps2_netemu.self");

									cellFsRename(PS2_EMU_PATH "ps2_gxemu.self"    , PS2_EMU_PATH "ps2_gxemu.tmp");
									cellFsRename(PS2_EMU_PATH "ps2_gxemu.self.sp" , PS2_EMU_PATH "ps2_gxemu.self");

									cellFsRename(PS2_EMU_PATH "ps2_emu.self"      , PS2_EMU_PATH "ps2_emu.tmp");
									cellFsRename(PS2_EMU_PATH "ps2_emu.self.sp"   , PS2_EMU_PATH "ps2_emu.self");
								}
								else if(cellFsStat(PS2_EMU_PATH "ps2_netemu.tmp", &s)==CELL_FS_SUCCEEDED)
								{
									show_msg((char*)"Restoring original ps2emu...");

									if(c_firmware>=4.65f)
									{
										cellFsRename(PS2_EMU_PATH "ps2_netemu.self", PS2_EMU_PATH "ps2_netemu.self.swap");
										cellFsRename(PS2_EMU_PATH "ps2_netemu.tmp" , PS2_EMU_PATH "ps2_netemu.self");

										cellFsRename(PS2_EMU_PATH "ps2_gxemu.self" , PS2_EMU_PATH "ps2_gxemu.self.swap");
										cellFsRename(PS2_EMU_PATH "ps2_gxemu.tmp"  , PS2_EMU_PATH "ps2_gxemu.self");

										cellFsRename(PS2_EMU_PATH "ps2_emu.self"   , PS2_EMU_PATH "ps2_emu.self.swap");
										cellFsRename(PS2_EMU_PATH "ps2_emu.tmp"    , PS2_EMU_PATH "ps2_emu.self");
									}
									else
									{
										cellFsRename(PS2_EMU_PATH "ps2_netemu.self", PS2_EMU_PATH "ps2_netemu.self.sp");
										cellFsRename(PS2_EMU_PATH "ps2_netemu.tmp" , PS2_EMU_PATH "ps2_netemu.self");

										cellFsRename(PS2_EMU_PATH "ps2_gxemu.self" , PS2_EMU_PATH "ps2_gxemu.self.sp");
										cellFsRename(PS2_EMU_PATH "ps2_gxemu.tmp"  , PS2_EMU_PATH "ps2_gxemu.self");

										cellFsRename(PS2_EMU_PATH "ps2_emu.self"   , PS2_EMU_PATH "ps2_emu.self.sp");
										cellFsRename(PS2_EMU_PATH "ps2_emu.tmp"    , PS2_EMU_PATH "ps2_emu.self");
									}
								}
						}
#endif //#ifdef COBRA_ONLY

						else
						if(!(webman_config->combo2 & XMLREFRSH) && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_L3) ) // SELECT+L3 refresh XML
						{
							if(data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] == CELL_PAD_CTRL_R2) profile=1; else
							if(data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] == CELL_PAD_CTRL_L2) profile=2; else
							if(data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] == CELL_PAD_CTRL_R1) profile=3; else
							if(data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] == CELL_PAD_CTRL_L1) profile=4; else profile=0;

							refresh_xml((char*)msg);
						}
                        else
						if( (!(webman_config->combo & SHOW_TEMP) && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & (CELL_PAD_CTRL_R3 | CELL_PAD_CTRL_START)))) // SELECT+START show temperatures / hdd space
						{
#ifdef VIDEO_REC
							if(!(webman_config->combo2 & VIDRECORD) && data.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_R3) // SELECT + R3
							{
								toggle_video_rec();
								sys_timer_sleep(2);
								break;
							}
#endif

#ifdef EXTRA_FEAT
							if(data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] == (CELL_PAD_CTRL_R2 | CELL_PAD_CTRL_L2) )
								saveBMP();
							else
#endif
							{
								CellRtcTick pTick; u32 dd, hh, mm, ss; char tmp[200];
show_popup:
								cellRtcGetCurrentTick(&pTick);
								get_temperature(0, &t1);
								get_temperature(1, &t2);

								uint32_t blockSize;
								uint64_t freeSize;
								cellFsGetFreeSize((char*)"/dev_hdd0", &blockSize, &freeSize);

								u8 st, mode, speed, unknown;
								if(get_fan_policy_offset)
								{
									if(!webman_config->fanc)
									{
										backup[5]=peekq(get_fan_policy_offset);
										lv2poke32(get_fan_policy_offset, 0x38600001); // sys 409 get_fan_policy  4.55/4.60/4.65/4.70/4.75/4.76
									}

									sys_sm_get_fan_policy(0, &st, &mode, &speed, &unknown);

									if(!webman_config->fanc)
									{
										pokeq(get_fan_policy_offset, backup[5]); // sys 409 get_fan_policy  4.55/4.60/4.65/4.70/4.75/4.76
									}
								}
								_meminfo meminfo;
								{system_call_1(SC_GET_FREE_MEM, (uint64_t)(u32) &meminfo);}

								// detect aprox. time when a game is launched
								if(View_Find("game_plugin")==0) gTick=rTick; else if(gTick.tick==rTick.tick) cellRtcGetCurrentTick(&gTick);

								bool R2 = (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] == CELL_PAD_CTRL_R2), bb;

								///// startup/play time /////
								bb = (!R2 && gTick.tick>rTick.tick); // show play time
								ss = (u32)((pTick.tick-(bb?gTick.tick:rTick.tick))/1000000); dd = (u32)(ss / 86400);
								if(dd>100) {bb=false; ss = (u32)((pTick.tick-rTick.tick)/1000000); dd = (u32)(ss / 86400);}
								ss = ss % 86400; hh = (u32)(ss / 3600); ss = ss % 3600; mm = (u32)(ss / 60); ss = ss % 60;
								/////////////////////////////

								net_info info;
								memset(&info, 0, sizeof(net_info));
								xsetting_F48C0548()->sub_44A47C(&info);

								char net_type[8] = "";
								if (info.device == 0) strcpy(net_type, "LAN"); else
								if (info.device == 1) strcpy(net_type, "WLAN");

								int32_t ip_size = 0x10;
								char ip[ip_size];
								netctl_main_9A528B81(ip_size, ip);

								char cfw_info[20];
#ifdef COBRA_ONLY
								bool is_mamba; {system_call_1(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_GET_MAMBA); is_mamba = ((int)p1 ==0x666);}

								uint16_t cobra_version; sys_get_version2(&cobra_version);

								char cobra_ver[8];
								if((cobra_version & 0x0F) == 0)
									sprintf(cobra_ver, "%X.%X", cobra_version>>8, (cobra_version & 0xFF) >> 4);
								else
									sprintf(cobra_ver, "%X.%02X", cobra_version>>8, (cobra_version & 0xFF));

								sprintf(cfw_info, "%s %s: %s", dex_mode ? "DEX" : "CEX", is_mamba ? "Mamba" : "Cobra", cobra_ver);
#else
								sprintf(cfw_info, "%s", dex_mode ? "DEX" : "CEX");
#endif
								sprintf((char*)tmp, "CPU: %i°C  RSX: %i°C  FAN: %i%%   \r\n"
													"%s: %id %02d:%02d:%02d\r\n"
													"Firmware : %i.%02i %s\r\n"
													"IP: %s  %s",
													t1>>24, t2>>24, (int)(((int)speed*100)/255),
													bb?"Play":"Startup", dd, hh, mm, ss,
													(int)c_firmware, ((u32)(c_firmware * 1000.0f) % 1000) / 10, cfw_info, ip, net_type);

								sprintf((char*)msg, "%s\r\n%s: %'i %s\r\n"
													"%s: %'i %s\r\n", tmp,
													STR_STORAGE, (int)((blockSize*freeSize)>>20), STR_MBFREE,
													STR_MEMORY, meminfo.avail>>10, STR_KBFREE);

								if(R2 && gTick.tick>rTick.tick)
								{
									////// play time //////
									ss = (u32)((pTick.tick-gTick.tick)/1000000);
									dd = (u32)(ss / 86400); ss = ss % 86400; hh = (u32)(ss / 3600); ss = ss % 3600; mm = (u32)(ss / 60); ss = ss % 60;

									if(dd<100) {char gname[200]; get_game_info(); sprintf(gname, "%s %s\r\n\r\n", _game_TitleID, _game_Title); sprintf((char*) msg, "%sPlay: %id %02d:%02d:%02d\r\n%s", gname, dd, hh, mm, ss, tmp); }
								}

								show_msg((char*) msg);
								sys_timer_sleep(2);

								/////////////////////////////
#ifdef COPY_PS3
								if(copy_in_progress)
								{
									sprintf((char*) msg, "<hr>%s %s (%i %s)", STR_COPYING, current_file, copied_count, STR_FILES);
									show_msg((char*) msg);
									sys_timer_sleep(2);
								}
								else
								if(fix_in_progress)
								{
									sprintf((char*) msg, "%s %s", STR_FIXING, current_file);
									show_msg((char*) msg);
									sys_timer_sleep(2);
								}
#endif
								/////////////////////////////
							}
						}
						else
						if(webman_config->fanc && !(webman_config->combo & MANUALFAN) && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_UP) ) // SELECT+UP increase TEMP/FAN
						{
							if(max_temp) //auto mode
							{
								if(data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R2) max_temp+=5; else max_temp+=1;
								if(max_temp>85) max_temp=85;
								webman_config->temp1=max_temp;
								sprintf((char*) msg, "%s\r\n%s %i°C", STR_FANCH0, STR_FANCH1, max_temp);
							}
							else
							{
								if(data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R2) webman_config->manu+=5; else webman_config->manu+=1;
								webman_config->manu=RANGE(webman_config->manu, 20, 99); //%
								webman_config->temp0= (u8)(((float)webman_config->manu * 255.f)/100.f);
								webman_config->temp0=RANGE(webman_config->temp0, 0x33, MAX_FANSPEED);
								fan_control(webman_config->temp0, 0);
								sprintf((char*) msg, "%s\r\n%s %i%%", STR_FANCH0, STR_FANCH2, webman_config->manu);
							}
							save_settings();
							show_msg((char*) msg);
							sys_timer_sleep(2);
						}
						else
						if(webman_config->fanc && !(webman_config->combo & MANUALFAN) && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_DOWN) ) // SELECT+DOWN increase TEMP/FAN
						{
							if(max_temp) //auto mode
							{
								if(max_temp>30) {if(data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R2) max_temp-=5; else max_temp-=1;}
								webman_config->temp1=max_temp;
								sprintf((char*) msg, "%s\r\n%s %i°C", STR_FANCH0, STR_FANCH1, max_temp);
							}
							else
							{
								if(webman_config->manu>20) {if(data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R2) webman_config->manu-=5; else webman_config->manu-=1;}
								webman_config->temp0= (u8)(((float)webman_config->manu * 255.f)/100.f);
								if(webman_config->temp0<0x33) webman_config->temp0=0x33;
								if(webman_config->temp0>MAX_FANSPEED) webman_config->temp0=MAX_FANSPEED;
								fan_control(webman_config->temp0, 0);
								sprintf((char*) msg, "%s\r\n%s %i%%", STR_FANCH0, STR_FANCH2, webman_config->manu);
							}
							save_settings();
							show_msg((char*) msg);
							sys_timer_sleep(2);
						}
						else
						if(webman_config->minfan && !(webman_config->combo & MINDYNFAN) && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_LEFT) ) // SELECT+LEFT decrease Minfan
						{
							if(webman_config->minfan-5>=MIN_FANSPEED) webman_config->minfan-=5;
							sprintf((char*) msg, "%s\r\n%s %i%%", STR_FANCH0, STR_FANCH3, webman_config->minfan);

							save_settings();
							show_msg((char*) msg);
							sys_timer_sleep(2);
						}
						else
						if(webman_config->minfan && !(webman_config->combo & MINDYNFAN) && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_RIGHT) ) // SELECT+RIGHT increase Minfan
						{
							if(webman_config->minfan+5<100) webman_config->minfan+=5;
							sprintf((char*) msg, "%s\r\n%s %i%%", STR_FANCH0, STR_FANCH3, webman_config->minfan);

							save_settings();
							show_msg((char*) msg);
							sys_timer_sleep(2);
						}
						else
						if(!(webman_config->combo & PREV_GAME) && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] == CELL_PAD_CTRL_L1) ) // SELECT+L1 (previous title)
						{
							led(GREEN, BLINK_FAST);
							mount_with_mm((char*)"_prev", 1);
							sys_timer_sleep(3);
							led(GREEN, ON);
						}
						else
						if(!(webman_config->combo & NEXT_GAME) && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] == CELL_PAD_CTRL_R1) ) // SELECT+R1 (next title)
						{
							led(GREEN, BLINK_FAST);
							mount_with_mm((char*)"_next", 1);
							sys_timer_sleep(3);
							led(GREEN, ON);
						}
						else
						if(!(webman_config->combo & UMNT_GAME) && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] == CELL_PAD_CTRL_CIRCLE) ) // SELECT+O (unmount)
						{
							do_umount(true);
						}
					}
					else
					if((data.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_L3) && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R2))
					{
						if(!(webman_config->combo & SHUT_DOWN) && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CROSS) ) // L3+R2+X (shutdown)
						{
							// power off
							working = 0;
							{ DELETE_TURNOFF } { BEEP1 }

							{system_call_4(SC_SYS_POWER, SYS_SHUTDOWN, 0, 0, 0);}

							sys_ppu_thread_exit(0);
						}
						else if(!(webman_config->combo & RESTARTPS) && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CIRCLE) ) // L3+R2+O (lpar restart)
						{
							// lpar restart
							working = 0;
							{ DELETE_TURNOFF }{ BEEP2 }

							{system_call_3(SC_SYS_POWER, SYS_REBOOT, NULL, 0);}

							sys_ppu_thread_exit(0);
						}
						else if(!(webman_config->combo & UNLOAD_WM) && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_R3) ) // L3+R3+R2 (quit webMAN)
						{
							if(!webman_config->fanc || webman_config->ps2temp<33)
								restore_fan(0); //restore syscon fan control mode
							else
								restore_fan(1); //set ps2 fan control mode

							working = 0;
							wm_unload_combo = 1;

							stop_prx_module();
							sys_ppu_thread_exit(0);
							break;
						}
						else if(!(webman_config->combo & DISABLEFC) && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_START) ) // L3+R2+START (enable/disable fancontrol)
						{
							webman_config->fanc = (webman_config->fanc ? 0 : 1);

							max_temp=0;
							if(webman_config->fanc)
							{
								if(webman_config->temp0==0) max_temp=webman_config->temp1; else max_temp=0;
								fan_control(webman_config->temp0, 0);
								sprintf((char*) msg, "%s %s", STR_FANCTRL3, STR_ENABLED);
							}
							else
							{
								restore_fan(0); //syscon
								sprintf((char*) msg, "%s %s", STR_FANCTRL3, STR_DISABLED);
							}
							save_settings();
							show_msg((char*) msg);
							sys_timer_sleep(2);
							break;
						}
					}
					else
					if(!(webman_config->combo & RESTARTPS) && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_L3) && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R1))
					{
						if(data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CROSS) // L3+R1+X (vsh shutdown)
						{
							// vsh shutdown
							working = 0;
							{ DELETE_TURNOFF }{ BEEP1 }

							vshmain_87BB0001(1); // VSH shutdown

							sys_ppu_thread_exit(0);
						}
						else
						if(data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CIRCLE) // L3+R1+O (vsh restart)
						{
							// vsh reboot
							working = 0;
							{ DELETE_TURNOFF }{ BEEP2 }

							vshmain_87BB0001(2); // VSH reboot

							sys_ppu_thread_exit(0);
						}
					}
					else
					if(data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R2)
					{
						if((copy_in_progress || fix_in_progress) && data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CIRCLE) // R2+O Abort copy process
						{
							fix_aborted=copy_aborted=true;
						}
#ifdef REMOVE_SYSCALLS
						else
						if(!(webman_config->combo & DISABLESH) && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_TRIANGLE) ) // R2+TRIANGLE Disable CFW Sycalls
						{
							if(peekq(0x8000000000003000ULL)==SYSCALLS_UNAVAILABLE) {
								{ BEEP2 }
								show_msg((char*)STR_CFWSYSALRD);
								sys_timer_sleep(2);
							} else {
								show_msg((char*)STR_CFWSYSRIP);
								remove_cfw_syscalls();
								delete_history(true);
								if(peekq(0x8000000000003000ULL)==SYSCALLS_UNAVAILABLE) {
									{ BEEP1 }
									show_msg((char*)STR_RMVCFWSYS);
									sys_timer_sleep(2);
								} else {
									{ BEEP2 }
									show_msg((char*)STR_RMVCFWSYSF);
									sys_timer_sleep(2);
								}
							}
						}
#endif
						else
						if(!(webman_config->combo2 & BLOCKSVRS) && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_SQUARE) && (c_firmware<=4.70f) ) // R2+SQUARE
						{
							show_msg((char*)"Blocking servers");
							block_online_servers();
							show_msg((char*)"Servers blocked");
						}
						else
						if(!(webman_config->combo & SHOW_IDPS) && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CIRCLE) ) // R2+O Show IDPS EID0+LV2
						{
							vshmain_is_ss_enabled = (void*)((int)getNIDfunc("vshmain", 0x981D7E9F, 0)); //is screenshot enabled?


							if(vshmain_is_ss_enabled()==0)
							{
								set_SSHT_ = (uint32_t*)&opd;
								memcpy(set_SSHT_, vshmain_is_ss_enabled, 8);
								opd[0] -= 0x2C; // Sub before vshmain_981D7E9F sets Screenshot Flag
								set_SSHT_(1);	// enable screenshot

								show_msg((char*)"Screenshot enabled");
								sys_timer_sleep(2);
							}

							uint64_t eid0_idps[2], buffer[0x40], start_sector;
							uint32_t read;
							sys_device_handle_t source;
							if(sys_storage_open(0x100000000000004ULL, 0, &source, 0)!=0)
							{
								start_sector = 0x204;
								sys_storage_close(source);
								sys_storage_open(0x100000000000001ULL, 0, &source, 0);
							}
							else start_sector = 0x178;
							sys_storage_read(source, 0, start_sector, 1, buffer, &read, 0);
							sys_storage_close(source);

							eid0_idps[0]=buffer[0x0E];
							eid0_idps[1]=buffer[0x0F];

							get_idps_psid();

							#define SEP "\n                  "
							sprintf((char*) msg, "IDPS EID0 : %016llX" SEP
															 "%016llX\n"
												 "IDPS LV2  : %016llX" SEP
															 "%016llX\r\n"
												 "PSID LV2 : %016llX" SEP
															"%016llX", eid0_idps[0], eid0_idps[1], IDPS[0], IDPS[1], PSID[0], PSID[1]);
							show_msg((char*) msg);
							sys_timer_sleep(2);
						}
					}
					else
					if((data.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_L3) && (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L2))
					{
#ifdef COBRA_ONLY
						if(!(webman_config->combo & DISACOBRA)
							&& (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_TRIANGLE))
						{ // L3+L2+TRIANGLE COBRA Toggle
							enable_dev_blind((char*)"COBRA Toggle activated!");
 #ifdef REX_ONLY
							if( (cellFsStat((char*) REBUG_COBRA_PATH "stage2.cex", &s)==CELL_FS_SUCCEEDED) /* &&
								(cellFsStat((char*) REBUG_COBRA_PATH "stage2.dex", &s)==CELL_FS_SUCCEEDED) */)
							{
								show_msg((char*)"REBUG COBRA is active!\r\nDeactivating COBRA...");

								cellFsRename(REBUG_COBRA_PATH "stage2.cex", REBUG_COBRA_PATH "stage2.cex.bak");
								cellFsRename(REBUG_COBRA_PATH "stage2.dex", REBUG_COBRA_PATH "stage2.dex.bak");
								reboot=true; // vsh reboot
							}
							else if((cellFsStat((char*) REBUG_COBRA_PATH "stage2.cex.bak", &s)==CELL_FS_SUCCEEDED) /* &&
									(cellFsStat((char*) REBUG_COBRA_PATH "stage2.dex.bak", &s)==CELL_FS_SUCCEEDED) */)
							{
								show_msg((char*)"REBUG COBRA is inactive!\r\nActivating COBRA...");

								cellFsRename(REBUG_COBRA_PATH "stage2.cex.bak", REBUG_COBRA_PATH "stage2.cex");
								cellFsRename(REBUG_COBRA_PATH "stage2.dex.bak", REBUG_COBRA_PATH "stage2.dex");
								reboot=true; // vsh reboot
							}
 #else

							if(cellFsStat((char*)HABIB_COBRA_PATH "stage2.cex", &s)==CELL_FS_SUCCEEDED)
							{
								show_msg((char*)"COBRA is active!\r\nDeactivating COBRA...");

								cellFsRename(HABIB_COBRA_PATH "stage2.cex", HABIB_COBRA_PATH "stage2_disabled.cex");

								reboot=true; // vsh reboot
							}
							else if(cellFsStat((char*)HABIB_COBRA_PATH "stage2_disabled.cex", &s)==CELL_FS_SUCCEEDED)
							{
								show_msg((char*)"COBRA is inactive!\r\nActivating COBRA...");

								cellFsRename(HABIB_COBRA_PATH "stage2_disabled.cex", HABIB_COBRA_PATH "stage2.cex");

								reboot=true; // vsh reboot
							}

							if(cellFsStat((char*)SYS_COBRA_PATH "stage2.bin", &s)==CELL_FS_SUCCEEDED)
							{
								show_msg((char*)"COBRA is active!\r\nDeactivating COBRA...");

								cellFsRename(SYS_COBRA_PATH "stage2.bin", SYS_COBRA_PATH "stage2_disabled.bin");

								if(cellFsStat((char*)COLDBOOT_PATH ".normal", &s)==CELL_FS_SUCCEEDED)
								{
									cellFsRename(COLDBOOT_PATH          , COLDBOOT_PATH ".cobra");
									cellFsRename(COLDBOOT_PATH ".normal", COLDBOOT_PATH);
								}

								reboot=true; // vsh reboot
							}
							else if(cellFsStat((char*)SYS_COBRA_PATH "stage2_disabled.bin", &s)==CELL_FS_SUCCEEDED)
							{
								show_msg((char*)"COBRA is inactive!\r\nActivating COBRA...");

								cellFsRename(SYS_COBRA_PATH "stage2_disabled.bin", SYS_COBRA_PATH "stage2.bin");

								if(cellFsStat((char*)COLDBOOT_PATH ".cobra", &s)==CELL_FS_SUCCEEDED)
								{
									cellFsRename(COLDBOOT_PATH         , COLDBOOT_PATH ".normal");
									cellFsRename(COLDBOOT_PATH ".cobra", COLDBOOT_PATH);
								}

								reboot=true; // vsh reboot
							}
 #endif //#ifdef REX_ONLY
						}
#endif //#ifdef COBRA_ONLY

#ifdef REX_ONLY
						if(!(webman_config->combo2 & REBUGMODE)
							&& (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_SQUARE))
						{ // L3+L2+口 REBUG Mode Switcher
							enable_dev_blind((char*)"REBUG Mode Switcher activated!");

							if(cellFsStat((char*) VSH_MODULE_PATH "vsh.self.swp", &s)==CELL_FS_SUCCEEDED)
							{
								show_msg((char*)"Normal Mode detected!\r\nSwitch to REBUG Mode Debug XMB...");
								sys_timer_sleep(3);

								cellFsRename(VSH_ETC_PATH "index.dat", VSH_ETC_PATH "index.dat.nrm");
								cellFsRename(VSH_ETC_PATH "index.dat.swp", VSH_ETC_PATH "index.dat");

								cellFsRename(VSH_ETC_PATH "version.txt", VSH_ETC_PATH "version.txt.nrm");
								cellFsRename(VSH_ETC_PATH "version.txt.swp", VSH_ETC_PATH "version.txt");

								cellFsRename(VSH_MODULE_PATH "vsh.self", VSH_MODULE_PATH "vsh.self.nrm");
								cellFsRename(VSH_MODULE_PATH "vsh.self.swp", VSH_MODULE_PATH "vsh.self");

								reboot=true; // vsh reboot
							}
							else
							if((cellFsStat((char*) VSH_MODULE_PATH "vsh.self.nrm", &s)==CELL_FS_SUCCEEDED)
							&& (cellFsStat((char*) VSH_MODULE_PATH "vsh.self.cexsp", &s)==CELL_FS_SUCCEEDED))
							{
								show_msg((char*)"REBUG Mode Debug XMB detected!\r\nSwitch to Retail XMB...");
								sys_timer_sleep(3);

								cellFsRename(VSH_MODULE_PATH "vsh.self", VSH_MODULE_PATH "vsh.self.dexsp");
								cellFsRename(VSH_MODULE_PATH "vsh.self.cexsp", VSH_MODULE_PATH "vsh.self");

								reboot=true; // vsh reboot
							}
							else
							if(cellFsStat((char*) VSH_MODULE_PATH "vsh.self.dexsp", &s)==CELL_FS_SUCCEEDED)
							{
								show_msg((char*)"REBUG Mode Retail XMB detected!\r\nSwitch to Debug XMB...");
								sys_timer_sleep(3);

								cellFsRename(VSH_MODULE_PATH "vsh.self", VSH_MODULE_PATH "vsh.self.cexsp");
								cellFsRename(VSH_MODULE_PATH "vsh.self.dexsp", VSH_MODULE_PATH "vsh.self");

								reboot=true; // vsh reboot
							}
						}
						else
						if(!(webman_config->combo2 & NORMAMODE)
							&& (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CIRCLE))
						{ // L3+L2+O Normal Mode Switcher
							enable_dev_blind((char*)"Normal Mode Switcher activated!");

							if((cellFsStat((char*) VSH_MODULE_PATH "vsh.self.nrm", &s)==CELL_FS_SUCCEEDED)
							&& (cellFsStat(VSH_MODULE_PATH "vsh.self.cexsp", &s)==CELL_FS_SUCCEEDED))
							{
								show_msg((char*)"REBUG Mode Debug XMB detected!\r\nSwitch to Normal Mode...");

								cellFsRename(VSH_ETC_PATH "index.dat", VSH_ETC_PATH "index.dat.swp");
								cellFsRename(VSH_ETC_PATH "index.dat.nrm", VSH_ETC_PATH "index.dat");

								cellFsRename(VSH_ETC_PATH "version.txt", VSH_ETC_PATH "version.txt.swp");
								cellFsRename(VSH_ETC_PATH "version.txt.nrm", VSH_ETC_PATH "version.txt");

								cellFsRename(VSH_MODULE_PATH "vsh.self", VSH_MODULE_PATH "vsh.self.swp");
								cellFsRename(VSH_MODULE_PATH "vsh.self.nrm", VSH_MODULE_PATH "vsh.self");

								reboot=true; // vsh reboot
							}
							else
							if(cellFsStat((char*) VSH_MODULE_PATH "vsh.self.dexsp", &s)==CELL_FS_SUCCEEDED)
							{
								show_msg((char*)"REBUG Mode Retail XMB detected!\r\nSwitch to Normal Mode...");

								cellFsRename(VSH_ETC_PATH "index.dat", VSH_ETC_PATH "index.dat.swp");
								cellFsRename(VSH_ETC_PATH "index.dat.nrm", VSH_ETC_PATH "index.dat");

								cellFsRename(VSH_ETC_PATH "version.txt", VSH_ETC_PATH "version.txt.swp");
								cellFsRename(VSH_ETC_PATH "version.txt.nrm", VSH_ETC_PATH "version.txt");

								cellFsRename(VSH_MODULE_PATH "vsh.self.dexsp", VSH_MODULE_PATH "vsh.self.swp");
								cellFsRename(VSH_MODULE_PATH "vsh.self", VSH_MODULE_PATH "vsh.self.cexsp");
								cellFsRename(VSH_MODULE_PATH "vsh.self.nrm", VSH_MODULE_PATH "vsh.self");

								reboot=true; // vsh reboot
							}
							else
							if(cellFsStat((char*) VSH_MODULE_PATH "vsh.self.swp", &s)==CELL_FS_SUCCEEDED)
							{
								show_msg((char*)"Normal Mode detected!\r\nNo need to switch!");
								sys_timer_sleep(3);
								{system_call_3(SC_FS_UMOUNT, (u64)(char*)"/dev_blind", 0, 1);}
								break;
							}
						}
						else
						if(!(webman_config->combo2 & DEBUGMENU)
							&& (data.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CROSS))
						{ // L3+L2+X DEBUG Menu Switcher
							enable_dev_blind((char*)"Debug Menu Switcher activated!");

							if(cellFsStat((char*) VSH_MODULE_PATH "sysconf_plugin.sprx.dex", &s)==CELL_FS_SUCCEEDED)
							{
								show_msg((char*)"CEX QA Menu is active!\r\nSwitch to DEX Debug Menu...");

								cellFsRename(VSH_MODULE_PATH "sysconf_plugin.sprx", VSH_MODULE_PATH "sysconf_plugin.sprx.cex");
								cellFsRename(VSH_MODULE_PATH "sysconf_plugin.sprx.dex", VSH_MODULE_PATH "sysconf_plugin.sprx");
							}
							else
							if(cellFsStat((char*) VSH_MODULE_PATH "sysconf_plugin.sprx.cex", &s)==CELL_FS_SUCCEEDED)
							{
								show_msg((char*)"DEX Debug Menu is active!\r\nSwitch to CEX QA Menu...");

								cellFsRename(VSH_MODULE_PATH "sysconf_plugin.sprx", VSH_MODULE_PATH "sysconf_plugin.sprx.dex");
								cellFsRename(VSH_MODULE_PATH "sysconf_plugin.sprx.cex", VSH_MODULE_PATH "sysconf_plugin.sprx");
							}
							sys_timer_sleep(1);
							{system_call_3(SC_FS_UMOUNT, (u64)(char*)"/dev_blind", 0, 1);}
						}
#endif //#ifdef REX_ONLY
					}
				}

				if(reboot)
				{
					sys_timer_sleep(1);
					// reboot
					show_msg((char*)"Switching successful! Reboot now...");
					sys_timer_sleep(3);
					{system_call_3(SC_FS_UMOUNT, (u64)(char*)"/dev_blind", 0, 1);}
reboot:
					// vsh reboot
					working = 0;
					{ DELETE_TURNOFF }
					savefile((char*)WMNOSCAN, NULL, 0);

					vshmain_87BB0001(2); // VSH reboot

					sys_ppu_thread_exit(0);
				}

			}
			//sys_timer_sleep(step);
			sys_timer_usleep(300000);
		}

		to++;
		if(to==20)
		{
			get_temperature(0, &t1);
			get_temperature(1, &t2);
			t1>>=24; t2>>=24;
			if(t1>83 || t2>83)
			{
				if(!webman_config->warn)
				{
					sprintf((char*) msg, "%s\r\n CPU: %i°C   RSX: %i°C", STR_OVERHEAT, t1, t2);
					show_msg((char*) msg);
					sys_timer_sleep(2);
				}
				if(t1>85 || t2>85)
				{
					if(!max_temp) max_temp=82;
					if(fan_speed<0xB0) fan_speed=0xB0;
					else
						if(fan_speed<MAX_FANSPEED) fan_speed+=8;

					old_fan=fan_speed;
					fan_control(fan_speed, 0);
					show_msg((char*)STR_OVERHEAT2);
				}
			}
		}
		if(to>40) to=0;

		// detect aprox. time when a game is launched
		if((sec % 10)==0) {if(View_Find("game_plugin")==0) gTick=rTick; else if(gTick.tick==rTick.tick) cellRtcGetCurrentTick(&gTick);}

		if(poll==0 && sec>=120) // check USB drives each 120 seconds
		{
			uint8_t tmp[2048];
			for(u8 f0=0; f0<8; f0++)
			{
				if(sys_storage_open(((f0<6)?USB_MASS_STORAGE_1(f0):USB_MASS_STORAGE_2(f0)), 0, &usb_handle, 0)==0)
				{
					sys_storage_read(usb_handle, 0, to, 1, tmp, &r, 0);
					sys_storage_close(usb_handle);
					//sprintf(tmp, "/dev_usb00%i: Read %i sectors @ %i offset", f0, r, to); show_msg((char*)tmp);
				}
			}
			sec=0;
		}
		sec+=step;
	}

	sys_ppu_thread_exit(0);
}

static void set_buffer_sizes(int footprint)
{
	if(footprint==1) //MIN
	{
#ifndef LITE_EDITION
		BUFFER_SIZE_ALL = ( 320*KB);
#else
		BUFFER_SIZE_ALL = ( _256KB_);
#endif
		BUFFER_SIZE_FTP	= ( _128KB_);
		//BUFFER_SIZE	= ( _128KB_);
		BUFFER_SIZE_PSX	= (  _32KB_);
		BUFFER_SIZE_PSP	= (  _32KB_);
		BUFFER_SIZE_PS2	= (  _64KB_);
		BUFFER_SIZE_DVD	= (  _64KB_);
	}
	else
	if(footprint==2) //MAX
	{
		BUFFER_SIZE_ALL = ( 1280*KB);
		BUFFER_SIZE_FTP	= ( _256KB_);
		//BUFFER_SIZE	= ( 512*KB);
		BUFFER_SIZE_PSX	= ( _192KB_);
		BUFFER_SIZE_PSP	= (  _64KB_);
		BUFFER_SIZE_PS2	= ( _128KB_);
		BUFFER_SIZE_DVD	= ( _256KB_);

		if((webman_config->cmask & PS1)) BUFFER_SIZE_PSX	= (_64KB_);
		if((webman_config->cmask & PS2)) BUFFER_SIZE_PS2	= (_64KB_);
		if((webman_config->cmask & (BLU | DVD)) == (BLU | DVD)) BUFFER_SIZE_DVD = (_64KB_);
	}
	else
	if(footprint==3) //MIN+
	{
		BUFFER_SIZE_ALL = ( 512*KB);
		BUFFER_SIZE_FTP	= ( _128KB_);
		//BUFFER_SIZE	= ( 320*KB);
		BUFFER_SIZE_PSX	= (  _32KB_);
		BUFFER_SIZE_PSP	= (  _32KB_);
		BUFFER_SIZE_PS2	= (  _64KB_);
		BUFFER_SIZE_DVD	= (  _64KB_);
	}
	else
	if(footprint==4) //MAX+
	{
		BUFFER_SIZE_ALL = ( 1280*KB);
		BUFFER_SIZE_FTP	= ( _128KB_);
		//BUFFER_SIZE	= ( 1088*KB);
		BUFFER_SIZE_PSX	= (  _32KB_);
		BUFFER_SIZE_PSP	= (  _32KB_);
		BUFFER_SIZE_PS2	= (  _64KB_);
		BUFFER_SIZE_DVD	= (  _64KB_);
	}
	else	//STANDARD
	{
		BUFFER_SIZE_ALL = ( 896*KB);
		BUFFER_SIZE_FTP	= ( _128KB_);
		//BUFFER_SIZE	= ( 448*KB);
		BUFFER_SIZE_PSX	= ( 160*KB);
		BUFFER_SIZE_PSP	= (  _32KB_);
		BUFFER_SIZE_PS2	= (  _64KB_);
		BUFFER_SIZE_DVD	= ( _192KB_);

		if((webman_config->cmask & PS1)) BUFFER_SIZE_PSX	= (_32KB_);
		if((webman_config->cmask & (BLU | DVD)) == (BLU | DVD)) BUFFER_SIZE_DVD = (_64KB_);
	}

	BUFFER_SIZE = BUFFER_SIZE_ALL - (BUFFER_SIZE_PSX + BUFFER_SIZE_PSP + BUFFER_SIZE_PS2 + BUFFER_SIZE_DVD);
}

static void wwwd_thread(uint64_t arg)
{

//	sys_timer_sleep(8);
//	u32 mode=0;
//	if(in_cobra(&mode)==0) cobra_mode=1;

	backup[0]=0;

	detect_firmware();

#ifdef COBRA_ONLY
	//cobra_lib_init();
	cobra_mode=1;
#else
	cobra_mode=0;
#endif

	View_Find = (void*)((int)getNIDfunc("paf", 0xF21655F3, 0));
	plugin_GetInterface = (void*)((int)getNIDfunc("paf", 0x23AFB290, 0));
#ifdef EXTRA_FEAT
	vsh_sprintf = (void*)((int)getNIDfunc("stdc", 0x273B9711, 0)); // sprintf
#endif

	//pokeq(0x8000000000003560ULL, 0x386000014E800020ULL); // li r3, 0 / blr
	//pokeq(0x8000000000003D90ULL, 0x386000014E800020ULL); // li r3, 0 / blr

	led(YELLOW, BLINK_FAST);

	WebmanCfg *webman_config = (WebmanCfg*) wmconfig;
	reset_settings();


	{struct CellFsStat buf; from_reboot = (cellFsStat((char*)WMNOSCAN, &buf)==CELL_FS_SUCCEEDED);} //is_rebug=isDir("/dev_flash/rebug");

	if(webman_config->blind) enable_dev_blind(NULL);

	{sys_map_path((char*)"/app_home", NULL);}

	set_buffer_sizes(webman_config->foot);

	init_running = 1;

	sys_ppu_thread_t id2;
	sys_ppu_thread_create(&id2, handleclient, (u64)START_DAEMON, -0x1d8, 0x20000, 0, "wwwd2");

	if(!webman_config->ftpd)
		sys_ppu_thread_create(&thread_id_ftp, ftpd_thread, NULL, -0x1d8,  0x2000, SYS_PPU_THREAD_CREATE_JOINABLE, THREAD_NAME_FTP);

#ifdef PS3MAPI
	///////////// PS3MAPI BEGIN //////////////
		sys_ppu_thread_create(&thread_id_ps3mapi, ps3mapi_thread, NULL, -0x1d8, 0x2000, SYS_PPU_THREAD_CREATE_JOINABLE, THREAD_NAME_PS3MAPI);
	///////////// PS3MAPI END //////////////
#endif

	led(YELLOW, OFF);
	sys_timer_sleep(5);

#ifdef USE_DEBUG
	u8 d_retries=0;
again_debug:
	debug_s=connect_to_server((char*)"192.168.100.209", 38009);
	if(debug_s<0) {d_retries++; sys_timer_sleep(2); if(d_retries<10) goto again_debug;}
	if(debug_s>=0) ssend(debug_s, "Connected...\r\n");
	sprintf(debug, "FC=%i T0=%i T1=%i\r\n", webman_config->fanc, webman_config->temp0, webman_config->temp1);
	ssend(debug_s, debug);
#endif

	max_temp=0;

	if(webman_config->fanc)
	{
		if(webman_config->temp0==0) max_temp=webman_config->temp1; else max_temp=0;
		fan_control(webman_config->temp0, 0);
	}

    sys_ppu_thread_create(&thread_id_poll, poll_thread, (u64)webman_config->poll, -0x1d8, 0x20000, SYS_PPU_THREAD_CREATE_JOINABLE, "poll_thread");

	while(init_running && working) sys_timer_usleep(100000);

	led(GREEN, ON);

//	{ DELETE_TURNOFF }

	int list_s = FAILED;

relisten:
#ifdef USE_DEBUG
	ssend(debug_s, "Listening on port 80...");
#endif
	if(working) list_s = slisten(WWWPORT, 4);
	else goto end;

	if((list_s<0) && working)
	{
		sys_timer_sleep(2);
		if(working) goto relisten;
		else goto end;
	}

	if((list_s >= 0) && working)
	{
#ifdef USE_DEBUG
		ssend(debug_s, " OK!\r\n");
#endif

		while(working)
		{
			sys_timer_usleep(10000);
			while(loading_html>2 && working)
			{
#ifdef USE_DEBUG
	sprintf(debug, "THREADS: %i\r\n", loading_html);
	ssend(debug_s, debug);
#endif
				sys_timer_usleep(300000);
			}
			int conn_s;
			if(!working) goto end;
			else
			if(working && (conn_s = accept(list_s, NULL, NULL)) > 0)
			{
				loading_html++;
				#ifdef USE_DEBUG
				ssend(debug_s, "*** Incoming connection... ");
				#endif
				sys_ppu_thread_t id;
				if(working) sys_ppu_thread_create(&id, handleclient, (u64)conn_s, -0x1d8, 0x20000, 0, "wwwd");
				else {sclose(&conn_s); break;}
			}
			else
			if((sys_net_errno==SYS_NET_EBADF) || (sys_net_errno==SYS_NET_ENETDOWN))
			{
				sclose(&list_s);
				list_s=FAILED;
				if(working) goto relisten;
				else break;
			}
		}

	}
end:
	sclose(&list_s);
	sys_ppu_thread_exit(0);
}


int wwwd_start(uint64_t arg)
{
	cellRtcGetCurrentTick(&rTick); gTick=rTick;

	sys_ppu_thread_create(&thread_id, wwwd_thread, NULL, -0x1d8, 0x4000, SYS_PPU_THREAD_CREATE_JOINABLE, THREAD_NAME);
#ifndef CCAPI
	_sys_ppu_thread_exit(0); // remove for ccapi compatibility
#endif
	return SYS_PRX_RESIDENT;
}

static void wwwd_stop_thread(uint64_t arg)
{
	while(init_running) sys_timer_usleep(500000); //Prevent unload too fast

	restore_fan(1); //restore & set static fan speed for ps2

	working = 0;
	sys_timer_usleep(500000);

	uint64_t exit_code;

/*
	sys_ppu_thread_t t;
 #ifndef LITE_EDITION
	sys_ppu_thread_create(&t, netiso_stop_thread, 0, 0, 0x2000, SYS_PPU_THREAD_CREATE_JOINABLE, STOP_THREAD_NAME);
	sys_ppu_thread_join(t, &exit_code);
 #endif
	sys_ppu_thread_create(&t, rawseciso_stop_thread, 0, 0, 0x2000, SYS_PPU_THREAD_CREATE_JOINABLE, STOP_THREAD_NAME);
	sys_ppu_thread_join(t, &exit_code);

	while(netiso_loaded || rawseciso_loaded) {sys_timer_usleep(100000);}
*/
	//if(thread_id != (sys_ppu_thread_t)-1)
		sys_ppu_thread_join(thread_id, &exit_code);

	if(thread_id_ftp != (sys_ppu_thread_t)-1)
		sys_ppu_thread_join(thread_id_ftp, &exit_code);

#ifdef PS3MAPI
	///////////// PS3MAPI BEGIN //////////////
	if(thread_id_ps3mapi != (sys_ppu_thread_t)-1)
		sys_ppu_thread_join(thread_id_ps3mapi, &exit_code);
	///////////// PS3MAPI END //////////////
#endif

	if(wm_unload_combo !=1)
	{
		if(thread_id_poll != (sys_ppu_thread_t)-1)
			sys_ppu_thread_join(thread_id_poll, &exit_code);
	}

	sys_ppu_thread_exit(0);
}

static void stop_prx_module(void)
{
	show_msg((char*)STR_WMUNL);

	sys_prx_id_t prx = prx_get_module_id_by_address(stop_prx_module);
	int *result = NULL;

	{system_call_6(SC_STOP_PRX_MODULE, (u64)(u32)prx, 0, NULL, (u64)(u32)result, 0, NULL);}
}

static void unload_prx_module(void)
{

	sys_prx_id_t prx = prx_get_module_id_by_address(unload_prx_module);

	{system_call_3(SC_UNLOAD_PRX_MODULE, (u64)prx, 0, NULL);}

}

int wwwd_stop(void)
{
	sys_ppu_thread_t t;
	uint64_t exit_code;

	int ret = sys_ppu_thread_create(&t, wwwd_stop_thread, 0, 0, 0x2000, SYS_PPU_THREAD_CREATE_JOINABLE, STOP_THREAD_NAME);
	if (ret == 0) sys_ppu_thread_join(t, &exit_code);

	sys_timer_usleep(500000);

	unload_prx_module();

//#ifndef CCAPI
	_sys_ppu_thread_exit(0); // remove for ccapi compatibility ???
//#endif

	return SYS_PRX_STOP_OK;
}
