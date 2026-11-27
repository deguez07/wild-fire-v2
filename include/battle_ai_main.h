#ifndef GUARD_BATTLE_AI_MAIN_H
#define GUARD_BATTLE_AI_MAIN_H

// Move scoring of the trainer AI ported from pokefirered-expansion, for
// trainers at AI_LEVEL_SMART_SCORING and above (see constants/battle_ai.h).
// Weaker trainers keep the vanilla AI scripts.

// Scores the attacker's moves (gBattlerAttacker) against gBattlerTarget with
// the AI_LOGIC_* steps set up by BattleAI_SetupAIData, and returns the chosen
// move slot. Never reads the move or switch the player picked this turn.
u8 BattleAI_ChooseMoveIndexWithLogic(void);

// Levels 7+: predicts the move of each of the player's mons and, in single
// battles, whether it switches and to what. The AI scores the player's side
// as if it were the AI, with only the moves, items and abilities it knows of.
// Call after AI_SetLogicDataForTurn; predictions are made once per turn.
void AI_SetupPredictions(u8 battlerAi);

#endif // GUARD_BATTLE_AI_MAIN_H
