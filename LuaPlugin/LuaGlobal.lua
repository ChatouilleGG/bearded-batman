
function import(file) require("LuaPlugin/"..(string.match(file, "^(.*)[.]lua") or file)); end

--================================================================
-- init
--================================================================

CHANS = {};		-- global, for requires
local BINDS;

function l_Init(init_chan)
	CHANS[init_chan] = {};

	BINDS = {
		OnChannel = {OnChannel},
		OnTick = {OnTick},
		MyMessage = {OnMessage},
		OnMessage = {OnMessage},
		OnMessageHilight = {OnMessage},
		MyAction = {OnAction},
		OnAction = {OnAction},
		OnActionHilight = {OnAction},
	}

	c_Print(init_chan, "\00313[LuaGlobal loaded !]");
end

--================================================================
-- Lua -> C
--================================================================

-- WARNING: this instance is not bound to a channel!
-- a channel name has to be specified everytime you need to print/command

--@override
function print(data)
end

-- c_Print(chan, text)
-- c_Command(chan, cmd)

--================================================================
-- C -> Lua
--================================================================

function l_ircEvent(chan, event, arg1, arg2)
	if ( BINDS[event] ~= nil ) then
		for i=1,#BINDS[event] do
			if ( BINDS[event][i](chan, arg1, arg2) ) then
				break;
			end
		end
	end
end

SLASH = {};		-- also global, for requires

function l_Slash(chan, cmdline)
	local cmd,rest = string.match(cmdline, "^ *([^ ]+) ?(.*)$");
	if ( cmd ~= nil and SLASH[cmd] ~= nil ) then
		local args = {text=rest};
		for word in string.gmatch(args.text, "[^ ]+") do
			table.insert(args, word);
		end
		SLASH[cmd](chan, args);
	end
end

--================================================================
-- Main
--================================================================

bDisabled = false;		-- global (need to find a way to pass it to sub instances...)

function SLASH.goff(chan, args)
	bDisabled = true;
	c_Print(chan, "\00313[LuaGlobal disabled!]");
end

function SLASH.gon(chan, args)
	bDisabled = false;
	c_Print(chan, "\00313[LuaGlobal enabled!]");
end

function SLASH.getchans(chan, args)
	local str = "channels: ";
	for k,v in pairs(CHANS) do
		str = str .. k .. ", ";
	end
	c_Print(chan, str);
end

--================================================================
-- OnChannel
--================================================================

function OnChannel(chan)
	if ( string.sub(chan,1,1) == "#" ) then
		CHANS[chan] = {};
		c_Print(chan, "\00313[LuaGlobal registered this channel!]");
	end
end

--================================================================
-- OnTick
--================================================================

function OnTick()
	-- 500 ms
end

--================================================================
-- Stuff
--================================================================

function OnMessage(chan, nick, msg)
	if ( msg == "!test" ) then
		c_Command(chan, "say 06YAAAAY!");
	end
end

function OnAction(chan, nick, msg)
	-- ...
end
