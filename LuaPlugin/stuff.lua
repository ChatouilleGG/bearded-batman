--================================================================
-- Stuff
--================================================================

local stuff = {
	run = false,
};

function SLASH.stuff(args)
	stuff.run = not(stuff.run);
	if ( stuff.run ) then
		print("stuff running!");
	else
		print("stuff stopped!");
	end
end

function Stuff_OnMessage(nick, msg)
	if ( stuff.run ) then
		local str = string.match(msg, "^[!]say (.*)$");
		if ( str ~= nil and string.len(str) > 0 ) then
			say(str);
			return true;
		end
	end
	return false;
end
