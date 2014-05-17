
//================================================================
// HexChat plugin to execute lua irc scripts
//
// version 2 ! (pretty much done)
//
// how it works:
//	- one instance of LuaGlobal.lua is generated upon addon load
//	- when a channel is joined, an instance of Lua#[channel_name].lua is created (if it exists), and l_Init() is called
//	- the addon then dispatches basic events (message,action,highlight,clock) to every running lua instance
//		^the global instance will receive messages from every channels, as well as private messages!
//
// All lua files should be located on root_of_HexChat/LuaPlugin/*
// file names should be like: LuaGlobal.lua , lua#somechannel.lua , lua#otherchannel.lua, ...
// The channel instances should only contain independant stuff that require no synchronization whatsoever.
// Anything that require some kind of sync between several channels should be located in LuaGlobal.lua.
// If you need to have a feature running in several channels independantly, you can and should use require("feature.lua");
// instead of duplicating code in different lua#chan.lua files.
//
// possible improvements :
//	- add a way for LuaGlobal to have some control over channel instances
//		^that would help a lot for making a global enable/disable (on/off) for example.
//
// on Windows, you can compile this into a HexChat addon that way:
//	- install lua (5.1 is good !)
//	- copy lua headers to this folder ( Lua/5.1/include/lua.h,lauxlib.h,luaconf.h,lualib.h )
//	- copy lua lib to this folder ( Lua/5.1/lib/lua5.1.lib )
//		^NOTE: if you use another version, make sure to change every occurence of it
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

#include <stdarg.h>

#include <windows.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "hexchat-plugin.h"

#pragma comment(lib, "lua5.1.lib")		// CHECK VERSION

#define MAX_CHANNELS 16

static hexchat_plugin *ph;

//================================================================
// Structures
//================================================================

struct s_channel
{
	char name[32];
	lua_State *L;
	struct s_channel* next;
};
struct s_channel* channels = NULL;

lua_State *LuaGlobal = NULL;

struct s_channel* current_chan(void)
{
	const char* name = hexchat_get_info(ph, "channel");
	struct s_channel* chan;
	for ( chan=channels; chan!=NULL; chan=chan->next )
		if ( strcmp(chan->name, name) == 0 )
			return chan;

	return NULL;
}

void lua_err(struct s_channel* chan, char* fmt, ...)
{
	char err[1024];
	va_list args;

	va_start(args, fmt);
	vsprintf(err, fmt, args);
	va_end(args);

	if ( chan != NULL )
	{
		hexchat_context *ctx = hexchat_find_context(ph, NULL, chan->name);
		if ( ctx != NULL )
			hexchat_set_context(ph, ctx);

		hexchat_printf(ph, "\00304[Lua Error] - lua%s.lua, %s\n", chan->name, err);
	}
	else
		hexchat_printf(ph, "\00304[Lua Error] - LuaGlobal.lua, %s\n", err);
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
		hexchat_printf(ph, "%s\n", lua_tostring(L,-1));
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

// lua lacks directory management :<
// not necessary for this basic template
// but I need it personnally, so here it is :D
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

void reset_channel_lua(struct s_channel* chan)
{
	char buff[50];
	FILE* f;

	if ( chan != NULL )
	{
		if ( chan->L != NULL )
			lua_close(chan->L);

		sprintf(buff, "LuaPlugin/lua%s.lua", chan->name);
		f = fopen(buff, "r");
		if ( f != NULL )
		{
			fclose(f);
			chan->L = lua_open();
			luaL_openlibs(chan->L);
			lua_register(chan->L, "c_Print", c_Print);
			lua_register(chan->L, "c_Command", c_Command);
			lua_register(chan->L, "c_ListDir", c_ListDir);
			luaL_loadfile(chan->L, buff);
			if ( !lua_pcall(chan->L, 0, 0, 0) )
			{
				lua_getglobal(chan->L, "l_Init");
				lua_pushstring(chan->L, chan->name);
				if ( lua_pcall(chan->L, 1, 0, 0) )
					lua_err(chan, "l_Init('%s')", chan->name);
			}
			else
			{
				lua_err(chan, "load");
				lua_close(chan->L);
				chan->L = NULL;
			}
		}
		if ( LuaGlobal != NULL )
		{
			lua_getglobal(LuaGlobal, "l_ircEvent");
			lua_pushstring(LuaGlobal, chan->name);
			lua_pushstring(LuaGlobal, "OnChannel");
			if ( lua_pcall(LuaGlobal, 2, 0, 0) )
				lua_err(NULL, "ircEvent(OnChannel, '%s')", chan->name);
		}
	}
}

static int on_channel(char *word[], void *userdata)
{
	struct s_channel* chan;

	for ( chan=channels; chan!=NULL; chan=chan->next )
		if ( strcmp(chan->name, word[2]) == 0 )
			return HEXCHAT_EAT_NONE;

	chan = malloc(sizeof(struct s_channel));
	strcpy(chan->name, word[2]);
	chan->next = channels;
	channels = chan;

	chan->L = NULL;
	reset_channel_lua(chan);

	return HEXCHAT_EAT_NONE;
}

void dispatch_print(char* event, char* arg1, char* arg2)
{
	struct s_channel* chan = current_chan();
	if ( chan != NULL )
	{
		if ( LuaGlobal != NULL )
		{
			int i;
			lua_getglobal(LuaGlobal, "l_ircEvent");
			lua_pushstring(LuaGlobal, chan->name);	// LuaGlobal also needs the channel
			lua_pushstring(LuaGlobal, event);
			lua_pushstring(LuaGlobal, arg1);
			lua_pushstring(LuaGlobal, arg2);
			if ( lua_pcall(LuaGlobal, 4, 0, 0) )
				lua_err(NULL, "ircEvent('%s', '%s', '%s', '%s')", chan->name, event, arg1, arg2);
		}
		if ( chan->L != NULL )
		{
			int i;
			lua_getglobal(chan->L, "l_ircEvent");
			lua_pushstring(chan->L, event);
			lua_pushstring(chan->L, arg1);
			lua_pushstring(chan->L, arg2);
			if ( lua_pcall(chan->L, 3, 0, 0) )
				lua_err(chan, "ircEvent('%s', '%s', '%s')", event, arg1, arg2);
		}
	}
}

static int on_message(char *word[], void *userdata)
{
	dispatch_print("OnMessage", word[1], word[2]);
	return HEXCHAT_EAT_NONE;
}

static int on_message_hilight(char *word[], void *userdata)
{
	dispatch_print("OnMessageHilight", word[1], word[2]);
	return HEXCHAT_EAT_NONE;
}
static int my_message(char *word[], void *userdata)
{
	dispatch_print("MyMessage", word[1], word[2]);
	return HEXCHAT_EAT_NONE;
}

static int on_action(char *word[], void *userdata)
{
	dispatch_print("OnAction", word[1], word[2]);
	return HEXCHAT_EAT_NONE;
}
static int on_action_hilight(char *word[], void *userdata)
{
	dispatch_print("OnActionHilight", word[1], word[2]);
	return HEXCHAT_EAT_NONE;
}

static int my_action(char *word[], void *userdata)
{
	dispatch_print("MyAction", word[1], word[2]);
	return HEXCHAT_EAT_NONE;
}

static int on_timer(void *userdata)
{
	struct s_channel* chan;
	if ( LuaGlobal != NULL )
	{
		lua_getglobal(LuaGlobal, "l_ircEvent");
		lua_pushnil(LuaGlobal);
		lua_pushstring(LuaGlobal, "OnTick");
		lua_pushnil(LuaGlobal);
		lua_pushnil(LuaGlobal);
		if ( lua_pcall(LuaGlobal, 4, 0, 0) )
			lua_err(NULL, "l_ircEvent(nil, 'OnTick', nil, nil)");
	}
	for ( chan=channels; chan!=NULL; chan=chan->next )
	{
		if ( chan->L != NULL )
		{
			lua_getglobal(chan->L, "l_ircEvent");
			lua_pushstring(chan->L, "OnTick");
			lua_pushnil(chan->L);
			lua_pushnil(chan->L);
			if ( lua_pcall(chan->L, 3, 0, 0) )
				lua_err(chan, "l_ircEvent('OnTick', nil, nil)");
		}
	}
	hexchat_hook_timer(ph, 500, on_timer, NULL);
	return HEXCHAT_EAT_NONE;
}

static int on_slash_lua(char *word[], char *word_eol[], void *userdata)
{
	struct s_channel* chan = current_chan();
	if ( chan != NULL )
	{
		if ( LuaGlobal != NULL )
		{
			lua_getglobal(LuaGlobal, "l_Slash");
			lua_pushstring(LuaGlobal, chan->name);
			lua_pushstring(LuaGlobal, word_eol[2]);
			if ( lua_pcall(LuaGlobal, 2, 0, 0) )
				lua_err(NULL, "l_Slash('%s', '%s')", chan->name, word_eol[2]);
		}
		if ( chan->L != NULL )
		{
			lua_getglobal(chan->L, "l_Slash");
			lua_pushstring(chan->L, word_eol[2]);
			if ( lua_pcall(chan->L, 1, 0, 0) )
				lua_err(chan, "l_Slash('%s')", word_eol[2]);
		}
	}
	return HEXCHAT_EAT_NONE;
}

static int on_reset_chan(char *word[], char *word_eol[], void *userdata)
{
	struct s_channel* chan = current_chan();
	if ( chan == NULL )
	{
		chan = malloc(sizeof(struct s_channel));
		strcpy(chan->name, hexchat_get_info(ph, "channel"));
		chan->next = channels;
		channels = chan;
		chan->L = NULL;
	}
	reset_channel_lua(current_chan());
	return HEXCHAT_EAT_NONE;
}

static int on_reset_global(char *word[], char *word_eol[], void *userdata)
{
	FILE* f = fopen("LuaPlugin/LuaGlobal.lua", "r");
	if ( f != NULL )
	{
		fclose(f);
		LuaGlobal = lua_open();
		luaL_openlibs(LuaGlobal);
		lua_register(LuaGlobal, "c_Print", c_Print);
		lua_register(LuaGlobal, "c_Command", c_Command);
		lua_register(LuaGlobal, "c_ListDir", c_ListDir);
		luaL_loadfile(LuaGlobal, "LuaPlugin/LuaGlobal.lua");
		if ( !lua_pcall(LuaGlobal, 0, 0, 0) )
		{
			struct s_channel* chan;
			const char* cur_chan = hexchat_get_info(ph, "channel");

			lua_getglobal(LuaGlobal, "l_Init");
			lua_pushstring(LuaGlobal, cur_chan);
			if ( lua_pcall(LuaGlobal, 1, 0, 0) )
				lua_err(NULL, "l_Init('%s')", cur_chan);

			// send all channels registered
			for ( chan=channels; chan!=NULL; chan=chan->next )
			{
				lua_getglobal(LuaGlobal, "l_ircEvent");
				lua_pushstring(LuaGlobal, chan->name);
				lua_pushstring(LuaGlobal, "OnChannel");
				if ( lua_pcall(LuaGlobal, 2, 0, 0) )
					lua_err(NULL, "ircEvent(OnChannel, '%s')", chan->name);
			}
		}
		else
		{
			lua_err(NULL, "load");
			lua_close(LuaGlobal);
			LuaGlobal = NULL;
		}
	}
	else
		hexchat_print(ph, "\00307[LUA Warning] - file LuaGlobal.lua not found!\n");
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
	*plugin_version = "2.0";

	hexchat_hook_print(ph, "You Join", HEXCHAT_PRI_NORM, on_channel, 0);

	hexchat_hook_print(ph, "Channel Message", HEXCHAT_PRI_NORM, on_message, 0);
	hexchat_hook_print(ph, "Channel Msg Hilight", HEXCHAT_PRI_NORM, on_message_hilight, 0);
	hexchat_hook_print(ph, "Your Message", HEXCHAT_PRI_NORM, my_message, 0);

	hexchat_hook_print(ph, "Channel Action", HEXCHAT_PRI_NORM, on_action, 0);
	hexchat_hook_print(ph, "Channel Action Hilight", HEXCHAT_PRI_NORM, on_action_hilight, 0);
	hexchat_hook_print(ph, "Your Action", HEXCHAT_PRI_NORM, my_action, 0);

	hexchat_hook_command(ph, "lua", HEXCHAT_PRI_NORM, on_slash_lua, NULL, NULL);
	hexchat_hook_command(ph, "rc", HEXCHAT_PRI_NORM, on_reset_chan, NULL, NULL);
	hexchat_hook_command(ph, "rg", HEXCHAT_PRI_NORM, on_reset_global, NULL, NULL);

	hexchat_hook_timer(ph, 500, on_timer, NULL);

	hexchat_print(ph, "\00313[LuaBridge loaded successfully!]\n");

	on_reset_global(NULL, NULL, NULL);

	return 1;
}

void hexchat_plugin_get_info(char **name, char **desc, char **version, void **reserved)
{
	*name = "LuaBridge";
	*desc = "-- Lua Bridge --";
	*version = "2.0";
}
