#ifndef GUARD_BATTLE_AI_SWITCH_H
#define GUARD_BATTLE_AI_SWITCH_H

// Switching of the trainer AI ported from pokefirered-expansion, for trainers
// with AI_SWITCH_* steps (level 5+, see constants/battle_ai.h). Weaker
// trainers keep the vanilla switching in battle_ai_switch_items.c.

enum SwitchType
{
    SWITCH_AFTER_KO,            // the AI's mon fainted
    SWITCH_MID_BATTLE_FORCED,   // the AI has to send something in (Baton Pass, Perish Song)
    SWITCH_MID_BATTLE_OPTIONAL, // the AI may switch if it finds a good switch-in
};

// Decides at action selection whether gActiveBattler switches out. If it
// does, sends the switch action to the battle and returns TRUE.
bool32 AI_TrySwitchWithLogic(u8 battler);

// The party slot to send in when the battle asks the AI for a mon, or
// PARTY_SIZE if no mon can come in.
u8 AI_ChooseMonToSendIn(u8 battler);

// Would the AI switch out in this battler's place (used to predict the
// player's switches)? Sets AI_LOGIC_DATA->mostSuitableMonId[battler] to the
// mon it expects to come in, among the party mons it has seen.
bool32 AI_WouldBattlerSwitch(u8 battler);

// Is this party mon the ace, kept for last (AI_SWITCH_ACE_POKEMON)?
bool32 IsAceMon(u8 battler, u8 monPartyId);

#endif // GUARD_BATTLE_AI_SWITCH_H
