Route22_NorthEntrance_MapScripts::
	.byte 0

Route22_NorthEntrance_EventScript_BoulderBadgeGuard::
	lock
	faceplayer
	setvar VAR_TEMP_1, 1
	bufferstdstring STR_VAR_1, STDSTRING_BOULDER_BADGE
	goto Route22_NorthEntrance_EventScript_BadgeGuard
	end

Route22_NorthEntrance_EventScript_BoulderBadgeGuardTrigger::
	lockall
	setvar VAR_TEMP_1, 1
	setvar VAR_0x8009, 1
	bufferstdstring STR_VAR_1, STDSTRING_BOULDER_BADGE
	goto Route23_EventScript_BadgeGuardTrigger
	end

Route22_NorthEntrance_EventScript_SafariZoneAttendant::
	lock
	faceplayer
	msgbox Route22_NorthEntrance_Text_NewSafariZoneEntrance
	goto_if_set FLAG_SYS_GAME_CLEAR, Route22_NorthEntrance_EventScript_SafariZoneAttendantEnd
	msgbox Route22_NorthEntrance_Text_FuchsiaEntranceClosed
Route22_NorthEntrance_EventScript_SafariZoneAttendantEnd::
	release
	end
