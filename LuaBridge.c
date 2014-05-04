
//================================================================
// HexChat plugin to execute lua irc scripts
//
// how it works:
// 	- when a channel is joined, a new LuaPlugin.lua is instanced and l_Init(channel) is called
//	- if l_Init(channel) returns 0, that instance is destroyed (aka. no lua running on this channel)
//	- the addon dispatches basic events (message,action,highlight) to every active instance of LuaPlugin.lua
//
// Note that despite having one instance per channel, there is one and only one LuaPlugin.lua
// (maybe it is not a good idea I don't know, it helps factorize code a lot since
//	most of your scripts will be running on multiple channels.)
// The current implementation however will not let you synchronize two (or more) instances.
// Also, lua can #require to factorize code, so the previous decision about having
// one file for all channels was stupid (my LuaPlugin.lua file is pretty messy right now...)
//
// I believe a good idea to improve this would be :
//	- have one instance shared between all the channels, running a file such as LuaGlobal.lua
//		+ ofcourse, add a command to reload the global script, just like /rui in a channel reloads this channel's instance
//	- have one file and one instance per channel, running something like Lua#[channel_name].lua
//
// Note: LuaPlugin.lua should be located in HexChat root folder
//
// on Windows, you can compile this into a HexChat addon that way:
//	- create a shortcut with the following target (you'll have locate your vcvarsall.bat yourself!):
//		%comspec% /k ""D:\VS2010\VCExpress\VC\vcvarsall.bat"" x86
//	- execute the shortcut
//	- navigate to this script's folder with the CD command
//	- execute: cl -O1 -MD -DWIN32 -c LuaBridge.c
//	- execute: link /DLL /out:LuaBridge.dll /SUBSYSTEM:WINDOWS LuaBridge.obj /def:LuaBridge.def /base:0x00d40000
// 	- paste the LuaBridge.dll into HexChat's plugins folder
//	- make a batch script to do it automatically
//
//================================================================

#include <windows.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "hexchat-plugin.h"

#pragma comment(lib, "lua5.1.lib")

#define MAX_CHANNELS 16

static hexchat_plugin *ph;

//================================================================
// Structures
//================================================================

struct s_channel
{
	char name[24];
	int enabled;
	lua_State *L;
};
struct s_channel channels[MAX_CHANNELS];
int chan_count = 0;

struct s_channel* get_channel(void)
{
	int i;
	const char* chan = hexchat_get_info(ph, "channel");
	for ( i=0; i<chan_count; i++ )
		if ( strcmp(channels[i].name, chan) == 0 )
			return &channels[i];

	return NULL;
}

lua_State* get_lua(void)
{
	struct s_channel *chan = get_channel();

	if ( chan != NULL && chan->enabled )
		return chan->L;

	return NULL;
}

//================================================================
// Lua -> C
//================================================================

static int c_Print(lua_State *L)
{
	hexchat_context *chan = hexchat_find_context(ph, NULL, lua_tostring(L,-2));
	if ( chan != NULL )
	{
		hexchat_set_context(ph, chan);
		hexchat_print(ph, lua_tostring(L,-1));
	}
	return 0;
}

static int c_Command(lua_State *L)
{
	hexchat_context *chan = hexchat_find_context(ph, NULL, lua_tostring(L,-2));
	if ( chan != NULL )
	{
		hexchat_set_context(ph, chan);
		hexchat_command(ph, lua_tostring(L,-1));
	}
	return 0;
}

static int c_RUI(lua_State *L)
{
	return 0;
}

// lua lacks directory management :<
// not necessary for this basic template
static int c_ListDir(lua_State *L)
{
	WIN32_FIND_DATA fdFile;
	HANDLE hFind = NULL;

	char sPath[256];

	char filename[256];
	char result[2048];

	result[0] = '\0';
	sprintf(sPath, "%s\\*.*", lua_tostring(L,-1));

	if ( (hFind = FindFirstFile(sPath, &fdFile)) == INVALID_HANDLE_VALUE )
	{
		lua_pushnil(L);
		return 1;
	}
	do
	{
		if ( strcmp(fdFile.cFileName, ".") != 0 && strcmp(fdFile.cFileName, "..") != 0 ) 
		{
			if ( fdFile.dwFileAttributes &FILE_ATTRIBUTE_DIRECTORY )
			{
				strcat(result, "/");
				strcat(result, fdFile.cFileName);
				strcat(result, "/ ");
			}
			else
			{
				strcat(result, "'");
				strcat(result, fdFile.cFileName);
				strcat(result, "' ");
			}
		}
	} while ( FindNextFile(hFind, &fdFile) );

	FindClose(hFind);
	lua_pushstring(L, result);
	return 1;
}

//================================================================
// C -> Lua
//================================================================

int init_lua(lua_State **L, char* chan)
{
	*L = lua_open();
	luaL_openlibs(*L);
	lua_register(*L, "c_Print", c_Print);
	lua_register(*L, "c_Command", c_Command);
	lua_register(*L, "c_RUI", c_RUI);
	lua_register(*L, "c_ListDir", c_ListDir);
	luaL_loadfile(*L, "LuaPlugin.lua");
	lua_pcall(*L, 0, 0, 0);
	lua_getglobal(*L, "l_Init");
	lua_pushstring(*L, chan);
	lua_pcall(*L, 1, 1, 0);
	if ( ! lua_tonumber(*L, -1) )
	{
		lua_close(*L);
		*L = NULL;
		return 0;
	}
	return 1;
}

static int on_channel(char *word[], void *userdata)
{
	int i;
	for ( i=0; i<chan_count; i++ )
		if ( strcmp(channels[chan_count].name, word[2]) == 0 )
			return HEXCHAT_EAT_NONE;

	if ( chan_count == MAX_CHANNELS )
		return HEXCHAT_EAT_NONE;

	strcpy(channels[chan_count].name, word[2]);
	channels[chan_count].enabled = init_lua(&channels[chan_count].L, channels[chan_count].name);
	chan_count ++;
	return HEXCHAT_EAT_NONE;
}

static int on_message(char *word[], void *userdata)
{
	lua_State *L = get_lua();
	if ( L != NULL )
	{
		lua_getglobal(L, "l_ircEvent");
		lua_pushstring(L, "OnMessage");
		lua_pushstring(L, word[1]);
		lua_pushstring(L, word[2]);
		lua_pcall(L, 3, 0, 0);
	}
	return HEXCHAT_EAT_NONE;
}

static int on_message_hilight(char *word[], void *userdata)
{
	lua_State *L = get_lua();
	if ( L != NULL )
	{
		lua_getglobal(L, "l_ircEvent");
		lua_pushstring(L, "OnMessageHilight");
		lua_pushstring(L, word[1]);
		lua_pushstring(L, word[2]);
		lua_pcall(L, 3, 0, 0);
	}
	return HEXCHAT_EAT_NONE;
}
static int my_message(char *word[], void *userdata)
{
	lua_State *L = get_lua();
	if ( L != NULL )
	{
		lua_getglobal(L, "l_ircEvent");
		lua_pushstring(L, "MyMessage");
		lua_pushstring(L, word[1]);
		lua_pushstring(L, word[2]);
		lua_pcall(L, 3, 0, 0);
	}
	return HEXCHAT_EAT_NONE;
}

static int on_action(char *word[], void *userdata)
{
	lua_State *L = get_lua();
	if ( L != NULL )
	{
		lua_getglobal(L, "l_ircEvent");
		lua_pushstring(L, "OnAction");
		lua_pushstring(L, word[1]);
		lua_pushstring(L, word[2]);
		lua_pcall(L, 3, 0, 0);
	}
	return HEXCHAT_EAT_NONE;
}
static int on_action_hilight(char *word[], void *userdata)
{
	lua_State *L = get_lua();
	if ( L != NULL )
	{
		lua_getglobal(L, "l_ircEvent");
		lua_pushstring(L, "OnActionHilight");
		lua_pushstring(L, word[1]);
		lua_pushstring(L, word[2]);
		lua_pcall(L, 3, 0, 0);
	}
	return HEXCHAT_EAT_NONE;
}

static int my_action(char *word[], void *userdata)
{
	lua_State *L = get_lua();
	if ( L != NULL )
	{
		lua_getglobal(L, "l_ircEvent");
		lua_pushstring(L, "MyAction");
		lua_pushstring(L, word[1]);
		lua_pushstring(L, word[2]);
		lua_pcall(L, 3, 0, 0);
	}
	return HEXCHAT_EAT_NONE;
}

static int on_timer(void *userdata)
{
	int i;
	for ( i=0; i<chan_count; i++ )
	{
		if ( channels[i].enabled )
		{
			lua_getglobal(channels[i].L, "l_ircEvent");
			lua_pushstring(channels[i].L, "OnTick");
			lua_pcall(channels[i].L, 1, 0, 0);
		}
	}
	hexchat_hook_timer(ph, 500, on_timer, NULL);
	return HEXCHAT_EAT_NONE;
}

static int lua_exec(char *word[], char *word_eol[], void *userdata)
{
	lua_State *L = get_lua();
	if ( L != NULL )
	{
		lua_getglobal(L, "l_Slash");
		lua_pushstring(L, word_eol[2]);
		lua_pcall(L, 1, 0, 0);
	}
	return HEXCHAT_EAT_NONE;
}

static int lua_reset(char *word[], char *word_eol[], void *userdata)
{
	struct s_channel *channel = get_channel();
	if ( channel == NULL && chan_count < MAX_CHANNELS )
	{
		channel = &channels[chan_count];
		strcpy(channel->name, hexchat_get_info(ph, "channel"));
		channel->enabled = 0;
		chan_count ++;
	}
	if ( channel != NULL )
	{
		if ( channel->enabled )
			lua_close(channel->L);

		channel->enabled = init_lua(&channel->L, channel->name);
	}
	return HEXCHAT_EAT_NONE;
}

//================================================================
// HexChat
//================================================================

int hexchat_plugin_init(hexchat_plugin *plugin_handle, char **plugin_name, char **plugin_desc, char **plugin_version, char *arg)
{
	ph = plugin_handle;
	*plugin_name = "LuaBridge";
	*plugin_desc = "-- Lua Bridge --";
	*plugin_version = "1.0";

	hexchat_hook_print(ph, "You Join", HEXCHAT_PRI_NORM, on_channel, 0);

	hexchat_hook_print(ph, "Channel Message", HEXCHAT_PRI_NORM, on_message, 0);
	hexchat_hook_print(ph, "Channel Msg Hilight", HEXCHAT_PRI_NORM, on_message_hilight, 0);
	hexchat_hook_print(ph, "Your Message", HEXCHAT_PRI_NORM, my_message, 0);

	hexchat_hook_print(ph, "Channel Action", HEXCHAT_PRI_NORM, on_action, 0);
	hexchat_hook_print(ph, "Channel Action Hilight", HEXCHAT_PRI_NORM, on_action_hilight, 0);
	hexchat_hook_print(ph, "Your Action", HEXCHAT_PRI_NORM, my_action, 0);

	hexchat_hook_command(ph, "lua", HEXCHAT_PRI_NORM, lua_exec, NULL, NULL);
	hexchat_hook_command(ph, "rui", HEXCHAT_PRI_NORM, lua_reset, NULL, NULL);

	hexchat_hook_timer(ph, 500, on_timer, NULL);

	hexchat_print(ph, "\00313[LuaBridge loaded successfully!]\n");

	return 1;
}

void hexchat_plugin_get_info(char **name, char **desc, char **version, void **reserved)
{
	*name = "LuaBridge";
	*desc = "-- Lua Bridge --";
	*version = "1.0";
}
