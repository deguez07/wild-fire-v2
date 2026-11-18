PewterCity_Mart_MapScripts::
	.byte 0

PewterCity_Mart_EventScript_Youngster::
	msgbox PewterCity_Mart_Text_BoughtWeirdFishFromShadyGuy, MSGBOX_NPC
	end

PewterCity_Mart_EventScript_Boy::
	msgbox PewterCity_Mart_Text_GoodThingsIfRaiseMonsDiligently, MSGBOX_NPC
	end

PewterCity_Mart_EventScript_Clerk::
	goto_if_questlog EventScript_ReleaseEnd
	lock
	faceplayer
	message Text_MayIHelpYou
	waitmessage
	pokemart PewterCity_Mart_Items
	msgbox Text_PleaseComeAgain
	release
	end

	.align 2
PewterCity_Mart_Items::
	.2byte ITEM_POKE_BALL
	.2byte ITEM_GREAT_BALL
	.2byte ITEM_POTION
	.2byte ITEM_SUPER_POTION
	.2byte ITEM_ANTIDOTE
	.2byte ITEM_PARALYZE_HEAL
	.2byte ITEM_BURN_HEAL
	.2byte ITEM_TM01
	.2byte ITEM_TM03
	.2byte ITEM_TM11
	.2byte ITEM_TM16
	.2byte ITEM_TM18
	.2byte ITEM_TM19
	.2byte ITEM_TM28
	.2byte ITEM_TM33
	.2byte ITEM_TM34
	.2byte ITEM_TM37
	.2byte ITEM_TM39
	.2byte ITEM_TM40
	.2byte ITEM_TM44
	.2byte ITEM_TM48
	.2byte ITEM_NONE
	release
	end
