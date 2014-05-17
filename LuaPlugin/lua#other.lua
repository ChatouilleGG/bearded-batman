
require("string");
require("table");
require("math");

function import(file) require("LuaPlugin/"..(string.match(file, "^(.*)[.]lua") or file)); end

import("general.lua");

--================================================================
-- init
--================================================================

function l_Init(chan_name)
	CHAN = chan_name;

	BINDS = {
		OnTick = 			{},
		MyMessage = 		{Thing_OnMessage,},
		OnMessage = 		{Thing_OnMessage,},
		OnMessageHilight = 	{Thing_OnMessage,},
		MyAction = 			{},
		OnAction =			{},
		OnActionHilight =	{},
	};

	c_Print(CHAN, "\00313[Lua Plugin loaded for "..CHAN.."]");
end

--================================================================
-- Stuff
--================================================================

import("thing.lua");
