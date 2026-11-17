ViridianCity_House_MapScripts::
	.byte 0

@ ---------------- Old Rod fisherman ----------------

ViridianCity_House_EventScript_FishingGuru::
	lock
	faceplayer
	goto_if_set FLAG_GOT_OLD_ROD, ViridianCity_House_EventScript_AlreadyGotOldRod
	msgbox ViridianCity_House_Text_ImFishingGuruDoYouLikeToFish, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, YES, ViridianCity_House_EventScript_GiveOldRod
	msgbox ViridianCity_House_Text_OhThatsSoDisappointing
	release
	end

ViridianCity_House_EventScript_AlreadyGotOldRod::
	msgbox ViridianCity_House_Text_HowAreTheFishBiting
	release
	end

ViridianCity_House_EventScript_GiveOldRod::
	checkitemspace ITEM_OLD_ROD
	goto_if_eq VAR_RESULT, FALSE, ViridianCity_House_EventScript_NoRoomForOldRod
	additem ITEM_OLD_ROD
	msgbox ViridianCity_House_Text_TakeThisAndFish
	msgreceiveditem ViridianCity_House_Text_ReceivedOldRodFromFishingGuru, ITEM_OLD_ROD
	setflag FLAG_GOT_OLD_ROD
	msgbox ViridianCity_House_Text_FishingIsAWayOfLife
	release
	end

ViridianCity_House_EventScript_NoRoomForOldRod::
	msgbox ViridianCity_House_Text_NoRoomForNiceGift
	release
	end

@ ---------------- Good Rod fisherman ----------------

ViridianCity_House_EventScript_FishingGurusBrother::
	lock
	faceplayer
	goto_if_set FLAG_GOT_GOOD_ROD, ViridianCity_House_EventScript_AlreadyGotGoodRod
	msgbox ViridianCity_House_Text_DoYouLikeToFishOlderBrother, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, YES, ViridianCity_House_EventScript_GiveGoodRod
	msgbox ViridianCity_House_Text_OhThatsSoDisappointing
	release
	end

ViridianCity_House_EventScript_AlreadyGotGoodRod::
	msgbox ViridianCity_House_Text_HowAreTheFishBiting
	release
	end

ViridianCity_House_EventScript_GiveGoodRod::
	msgbox ViridianCity_House_Text_LikeYourStyleTakeThis
	checkitemspace ITEM_GOOD_ROD
	goto_if_eq VAR_RESULT, FALSE, ViridianCity_House_EventScript_NoRoomForGoodRod
	giveitem_msg ViridianCity_House_Text_ReceivedGoodRod, ITEM_GOOD_ROD
	msgbox ViridianCity_House_Text_GoodRodCanCatchBetterMons
	setflag FLAG_GOT_GOOD_ROD
	release
	end

ViridianCity_House_EventScript_NoRoomForGoodRod::
	msgbox ViridianCity_House_Text_YouHaveNoRoomForGift
	release
	end

@ ---------------- Super Rod fisherman ----------------

ViridianCity_House_EventScript_FishingGuruBrother::
	lock
	faceplayer
	goto_if_set FLAG_GOT_SUPER_ROD, ViridianCity_House_EventScript_CheckMagikarpRecord
	msgbox ViridianCity_House_Text_DoYouLikeToFishYoungerBrother, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, YES, ViridianCity_House_EventScript_GiveSuperRod
	msgbox ViridianCity_House_Text_OhThatsDisappointing
	release
	end

ViridianCity_House_EventScript_GiveSuperRod::
	checkitemspace ITEM_SUPER_ROD
	goto_if_eq VAR_RESULT, FALSE, ViridianCity_House_EventScript_NoRoomForSuperRod
	additem ITEM_SUPER_ROD
	msgbox ViridianCity_House_Text_TakeThisAndFishYoungFriend
	msgreceiveditem ViridianCity_House_Text_ReceivedSuperRod, ITEM_SUPER_ROD
	setflag FLAG_GOT_SUPER_ROD
	msgbox ViridianCity_House_Text_IfYouCatchBigMagikarpShowMe
	release
	end

ViridianCity_House_EventScript_NoRoomForSuperRod::
	msgbox ViridianCity_House_Text_NoRoomForGift
	release
	end

ViridianCity_House_EventScript_CheckMagikarpRecord::
	goto_if_questlog EventScript_ReleaseEnd
	special QuestLog_CutRecording
	setvar VAR_0x8004, SPECIES_MAGIKARP
	specialvar VAR_RESULT, DoesPlayerPartyContainSpecies
	goto_if_eq VAR_RESULT, FALSE, ViridianCity_House_EventScript_NoMagikarpInParty
	special GetMagikarpSizeRecordInfo
	msgbox ViridianCity_House_Text_OhMagikarpAllowMeToSee
	special ChoosePartyMon
	waitstate
	copyvar VAR_RESULT, VAR_0x8004
	goto_if_ge VAR_RESULT, PARTY_SIZE, ViridianCity_House_EventScript_CancelShowMon
	special CompareMagikarpSize
	goto_if_eq VAR_RESULT, 1, ViridianCity_House_EventScript_NotMagikarp
	goto_if_eq VAR_RESULT, 2, ViridianCity_House_EventScript_NotRecordMagikarp
	goto_if_eq VAR_RESULT, 3, ViridianCity_House_EventScript_NewRecordMagikarp
	goto_if_eq VAR_RESULT, 4, ViridianCity_House_EventScript_TieRecordMagikarp
	release
	end

ViridianCity_House_EventScript_NoMagikarpInParty::
	msgbox ViridianCity_House_Text_TryFishingBringMeMagikarp
	release
	end

ViridianCity_House_EventScript_CancelShowMon::
	release
	end

ViridianCity_House_EventScript_NotMagikarp::
	msgbox ViridianCity_House_Text_DoesntLookLikeMagikarp
	release
	end

ViridianCity_House_EventScript_NotRecordMagikarp::
	goto_if_unset FLAG_GOT_RECORD_SETTING_MAGIKARP, ViridianCity_House_EventScript_NewRecordMagikarp
	msgbox ViridianCity_House_Text_HmmXInchesDoesntMeasureUp
	release
	end

ViridianCity_House_EventScript_TieRecordMagikarp::
	goto_if_unset FLAG_GOT_RECORD_SETTING_MAGIKARP, ViridianCity_House_EventScript_NewRecordMagikarp
	msgbox ViridianCity_House_Text_HuhXInchesSameSizeAsLast
	release
	end

ViridianCity_House_EventScript_NewRecordMagikarp::
	setflag FLAG_GOT_RECORD_SETTING_MAGIKARP
	msgbox ViridianCity_House_Text_WhoaXInchesTakeThis
	giveitem ITEM_NET_BALL
	goto_if_eq VAR_RESULT, FALSE, ViridianCity_House_EventScript_NoRoomForNetBall
	msgbox ViridianCity_House_Text_LookForwardToGreaterRecords
	release
	end

ViridianCity_House_EventScript_NoRoomForNetBall::
	msgbox ViridianCity_House_Text_NoRoomForGift
	release
	end

ViridianCity_House_EventScript_MagikarpRecordSign::
	lockall
	goto_if_set FLAG_GOT_RECORD_SETTING_MAGIKARP, ViridianCity_House_EventScript_MagikarpRecordSignRecordSet
	msgbox ViridianCity_House_Text_BlankChartOfSomeSort
	releaseall
	end

ViridianCity_House_EventScript_MagikarpRecordSignRecordSet::
	special GetMagikarpSizeRecordInfo
	msgbox ViridianCity_House_Text_MostGiganticMagikarpXInches
	releaseall
	end
