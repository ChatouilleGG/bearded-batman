
--================================================================
-- Usual stuff
--================================================================

CHAN = "";
BINDS = {};

--================================================================
-- Lua -> C
--================================================================

--@override
function print(data)
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

function l_ircEvent(event, arg1, arg2)
	if ( bEnabled and BINDS[event] ~= nil ) then
		for i=1,#BINDS[event] do
			if ( BINDS[event][i](arg1, arg2) ) then
				break;
			end
		end
	end
end

SLASH = {};		-- also global, for requires

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

bEnabled = true;	-- global

function SLASH.off(args)
	bEnabled = false;
	print("\00313[Lua"..CHAN.." disabled!]");
end

function SLASH.on(args)
	bEnabled = true;
	print("\00313[Lua"..CHAN.." enabled!]");
end

