
--================================================================
-- init
--================================================================

local CHAN;   -- store channel name

local BINDS;  -- bind events to functions/features, depending on the channel

function l_Init(chan_name)

	CHAN = chan_name;

	if ( CHAN == "#channelexample" ) then
		BINDS = {
			OnMessage = {SomeFeature_OnMessage},
			OnMessageHilight = {SomeFeature_OnMessage},
			MyMessage = {Admin_OnMessage, SomeFeature_OnMessage},
		};

	elseif ( CHAN == "#somechan" ) then
		BINDS = {
			OnMessage = {OtherFeature_OnMessage},
			OnMessageHilight = {OtherFeature_OnMessage},
			MyMessage = {Admin_OnMessage, OtherFeature_OnMessage},
			OnTick = {SomeChan_OnTick},
		};

  -- no '#' is the channel of private messages !
	elseif ( string.sub(CHAN,1,1) ~= "#" ) then
		BINDS = {
			OnMessage = {SomeFeature_OnMessage, OtherFeature_OnMessage},
			OnMessageHilight = {SomeFeature_OnMessage, OtherFeature_OnMessage},
			MyMessage = {Admin_OnMessage, SomeFeature_OnMessage, OtherFeature_OnMessage},
			OnTick = {PrivChan_OnTick},
		};

	else
		BINDS = {};
		return 0;   -- no lua running on this channel
	end

	c_Print(CHAN, "\00313[LuaPlugin loaded in "..CHAN.."]");
	return 1;
end

--================================================================
-- Lua -> C
--================================================================

-- override lua default print, to print in IRC window
local function print(data)
	c_Print(CHAN, tostring(data));
end

function send(cmd)
	c_Command(CHAN, cmd);
end

function say(data)
	c_Command(CHAN, "say "..tostring(data));
end

--================================================================
-- C -> Lua
--================================================================

-- use binds structures to dispatch events
-- a function returning TRUE will 'eat' the event (next functions won't receive it)
function l_ircEvent(event, arg1, arg2)
	if ( BINDS[event] ~= nil ) then
		for i=1,#BINDS[event] do
			if ( BINDS[event][i](arg1, arg2) ) then
				break;
			end
		end
	end
end

--[[ handle slash commands for more awesomeness !
if you use /lua in irc, the C plugin will capture and dispatch the command to active lua scripts.
This function grabs the first parameter and calls the function with that name if it exists.
for instance, if you type "/lua stuff 10 bleh" , it will automatically call the function SLASH.stuff(args),
where args = { 1="10", 2="bleh", text="10 bleh" } so you can use args[1], args[2], or args.text.
--]]

local SLASH = {};

function l_Slash(cmdline)
	local cmd,rest = string.match(cmdline, "^ *([^ ]+) ?(.*)$");
	if ( cmd ~= nil and SLASH[cmd] ~= nil ) then
		local args = {text=rest};
		for word in string.gmatch(args.text, "[^ ]+") do
			table.insert(args, word);
		end
		SLASH[cmd](args);
	end
end

--================================================================
-- Main
--================================================================

local bDisabled = false;

function SLASH.off(args)
	bDisabled = true;
	print("\00313[LuaPlugin disabled!]");
end

function SLASH.on(args)
	bDisabled = false;
	print("\00313[LuaPlugin enabled!]");
end

function Admin_OnMessage(nick, msg)
	if ( msg == "!off" ) then
		SLASH.off(nil);
	elseif ( msg == "!on" ) then
		SLASH.on(nil);
	else
		return false;
	end
	return true;
end

--================================================================
-- Some Feature
--================================================================

function SomeFeature_OnMessage(nick, msg)
  -- ...
  return false;
end

--================================================================
-- Other Feature
--================================================================

function OtherFeature_OnMessage(nick, msg)
  -- ...
  return false;
end

SomeChan_OnTick()
  -- ...
  return false;
end

--================================================================
-- Private Messages Channel
--================================================================

function PrivChan_OnTick()
  -- ...
  return false;
end

--================================================================
-- Stuff
--================================================================

function SLASH.stuff(args)
  -- ...
end
