--================================================================
-- Thing
--================================================================

function Thing_OnMessage(nick, msg)
	if ( msg == "!thing" ) then
		say(nick..": This is the thing !!!");
		return true;
	end
	return false;
end
