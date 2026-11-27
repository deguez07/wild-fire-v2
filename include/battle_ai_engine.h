#ifndef GUARD_BATTLE_AI_ENGINE_H
#define GUARD_BATTLE_AI_ENGINE_H

// Connects the trainer AI to the vanilla FireRed battle engine.
//
// The AI logic ported from pokefirered-expansion asks the engine questions
// such as "how much damage does this move do", "who moves first" or "how
// accurate is this move". This file answers them with vanilla's own Gen 3
// rules: the vanilla damage formula, per-type physical/special split, Gen 3
// type chart and Gen 3 turn order. Function names follow the expansion AI
// where possible, so the ported code reads the same.
//
// Nothing here changes the battle. Every function only reads battle state,
// and the damage simulation restores every global it touches.
//
// Fairness: these functions see the AI's view of each battler (its moves,
// ability and held item, see AI_GetMovesArray / AI_GetBattlerAbility /
// AI_GetBattlerItem), never the move or switch the player has selected this
// turn. Below level 9 the AI's view of the player's side holds only what the
// battle has shown (struct AiPartyKnowledge).

#include "global.h"
#include "battle.h"

// Set to TRUE to log the AI's damage and turn-order predictions against what
// really happens in battle. Needs a debug build: in include/config.h comment
// out NDEBUG and set LOG_HANDLER to LOG_HANDLER_MGBA_PRINT. Leave FALSE for
// normal builds.
#define AI_DEBUG_CHECKS FALSE

// Damage categories. In Gen 3 the category comes from the move's type.
#define DAMAGE_CATEGORY_PHYSICAL 0
#define DAMAGE_CATEGORY_SPECIAL  1
#define DAMAGE_CATEGORY_STATUS   2

// Type effectiveness as a 4.12 fixed-point multiplier, like the expansion AI.
typedef u16 uq4_12_t;
#define UQ_4_12_SHIFT 12
#define AI_EFFECTIVENESS_0     0
#define AI_EFFECTIVENESS_0_25  (1 << (UQ_4_12_SHIFT - 2))
#define AI_EFFECTIVENESS_0_5   (1 << (UQ_4_12_SHIFT - 1))
#define AI_EFFECTIVENESS_1     (1 << UQ_4_12_SHIFT)
#define AI_EFFECTIVENESS_2     (1 << (UQ_4_12_SHIFT + 1))
#define AI_EFFECTIVENESS_4     (1 << (UQ_4_12_SHIFT + 2))

// Damage rolls. Gen 3 multiplies damage by a random 85-100%.
#define MIN_ROLL_PERCENTAGE 85
#define MAX_ROLL_PERCENTAGE 100
#define DMG_ROLL_PERCENTAGE ((MAX_ROLL_PERCENTAGE + MIN_ROLL_PERCENTAGE + 1) / 2)

// Return values of AI_WhoStrikesFirst.
#define AI_IS_FASTER   1
#define AI_IS_SLOWER  -1

#define DONT_CONSIDER_PRIORITY 0
#define CONSIDER_PRIORITY      1

// Whose damage AI_GetDamage estimates: the AI's own attacks, or attacks it
// expects to take. A risky AI assumes high rolls for its attacks and low rolls
// for the attacks it takes; otherwise the median roll is used.
#define AI_ATTACKING 0
#define AI_DEFENDING 1

// For GetNoOfHitsToKOBattler, as in the expansion AI. Gen 3 has nothing that
// surely survives a KO hit (Focus Band is random), so both give the same result.
#define DONT_CONSIDER_ENDURE 0
#define CONSIDER_ENDURE      1

// moveAccuracy value for moves that skip the accuracy check, so they beat
// moves with 100% accuracy when the AI compares them.
#define AI_ACCURACY_SURE_HIT 101

// What the AI has seen of one of the player's party mons in this battle.
struct AiPartyKnowledge
{
    u16 moves[MAX_MON_MOVES]; // moves it has used, in their move slot; MOVE_NONE if not seen
    u16 item;                 // held item the battle revealed, ITEM_NONE if not seen
    u8 ability;               // ability the battle revealed, ABILITY_NONE if not seen
    u8 guessedAbility;        // the AI's guess while the ability isn't known
    u8 guessTurn;             // turn of that guess, plus 1 (0: no guess yet)
    bool8 seen;               // it has been on the field
};

struct SimulatedDamage
{
    u16 minimum;
    u16 median;
    u16 maximum;
};

// AI data computed once per AI decision. Indexes are [attacker][target][move slot].
struct AiLogicData
{
    u8 abilities[MAX_BATTLERS_COUNT];  // ability the AI assumes each battler has
    u16 items[MAX_BATTLERS_COUNT];     // held item the AI assumes each battler has
    u8 holdEffects[MAX_BATTLERS_COUNT];
    u8 holdEffectParams[MAX_BATTLERS_COUNT];
    u8 hpPercents[MAX_BATTLERS_COUNT];
    u8 moveLimitations[MAX_BATTLERS_COUNT];
    u32 speedStats[MAX_BATTLERS_COUNT];
    struct SimulatedDamage simulatedDmg[MAX_BATTLERS_COUNT][MAX_BATTLERS_COUNT][MAX_MON_MOVES];
    uq4_12_t effectiveness[MAX_BATTLERS_COUNT][MAX_BATTLERS_COUNT][MAX_MON_MOVES];
    u8 moveAccuracy[MAX_BATTLERS_COUNT][MAX_BATTLERS_COUNT][MAX_MON_MOVES]; // AI_ACCURACY_SURE_HIT if it can't miss
    u16 lastUsedMove[MAX_BATTLERS_COUNT]; // last move each battler used (previous turns only)
    u16 partnerMove;                      // move the AI's partner chose this turn (doubles)
    s32 moveScores[MAX_MON_MOVES];        // scores of the ported AI while it decides
    u16 prevTurnSpecies[MAX_BATTLERS_COUNT]; // species on the field at the AI's last switch decision
    u8 mostSuitableMonId[MAX_BATTLERS_COUNT]; // best switch-in found by the switching AI, PARTY_SIZE if none
    bool8 shouldConsiderExplosion;        // rolled once per decision, more likely at low HP
    bool8 ready; // TRUE once AI_SetLogicDataForTurn has run for this decision
    u8 knowledgeFlags;                    // AI_KNOW_* and AI_PREDICT_* of the trainer
    u16 knownMoves[MAX_BATTLERS_COUNT][MAX_MON_MOVES]; // AI_GetMovesArray of battlers the AI doesn't fully know

    // Predictions about the player's side, made once per turn (AI_SetupPredictions).
    u16 predictedMove[MAX_BATTLERS_COUNT];   // the move the AI expects, MOVE_NONE if it can't tell
    u8 predictedSwitchin[MAX_BATTLERS_COUNT]; // the mon the AI expects to come in, PARTY_SIZE if none
    u8 predictedSwitches;                  // bit per battler the AI expects to switch out
    bool8 predictSwitchRoll;               // this turn's roll to act on predicted switches
    bool8 predictingMove;                  // the AI acts on predictedMove this turn
    bool8 predictingSwitch;                // the AI acts on predictedSwitches this turn
    bool8 predictionInProgress;            // TRUE while the AI plays the player's side
    u8 predictionTurn;                     // turn of the predictions, plus 1 (0: none)
    u8 predictionBattler;                  // AI battler the predictions were made for

    // Kept for the whole battle; never restored by the simulations above.
    struct AiPartyKnowledge playerParty[PARTY_SIZE];
};

#define AI_LOGIC_DATA (gBattleResources->aiLogicData)

// Move data
u8 GetMoveEffect(u16 move);
u8 GetMovePower(u16 move);
u8 GetMoveType(u16 move);
u8 GetMoveAccuracy(u16 move);
u8 GetMoveSecondaryEffectChance(u16 move);
u8 GetMoveTargetType(u16 move);
s8 GetMovePriority(u16 move);
bool32 MoveMakesContact(u16 move);
bool32 IsSoundMove(u16 move);
bool32 IsBattleMoveStatus(u16 move);

// Type and category of a move when this battler uses it (Hidden Power, Weather Ball).
u8 GetBattleMoveType(u16 move, u8 battlerAtk);
u8 GetBattleMoveCategory(u16 move, u8 battlerAtk);
bool32 IsBattleMovePhysical(u16 move, u8 battlerAtk);
bool32 IsBattleMoveSpecial(u16 move, u8 battlerAtk);

// What the AI believes about a battler
bool32 IsBattlerAlive(u8 battler);
u8 AI_GetBattlerAbility(u8 battler);
u16 AI_GetBattlerItem(u8 battler);
u8 AI_GetBattlerHoldEffect(u8 battler);
u8 AI_GetBattlerHoldEffectParam(u8 battler);
bool32 AI_BattlerHasType(u8 battler, u8 type);

// Type matchups
uq4_12_t AI_GetTypeEffectiveness(u8 moveType, u8 battlerDef);
uq4_12_t AI_GetMoveEffectiveness(u16 move, u8 battlerAtk, u8 battlerDef);
bool32 AI_IsMoveBlockedByAbility(u16 move, u8 battlerAtk, u8 battlerDef);
bool32 AI_IsAbilityOnField(u8 ability);

// Weather that currently has an effect (0 under Cloud Nine / Air Lock).
u16 AI_GetWeather(void);

// The moves the AI considers a battler to have. The AI's own side and level 9+
// trainers see the real moves; otherwise the moves the battler has used, plus
// typical attacks of its types at level 8 (MOVE_NONE where it knows nothing).
u16 *AI_GetMovesArray(u8 battler);

// What the AI knows about the player's side
bool32 AI_KnowsBattler(u8 battler);                         // AI's own side, or a level 9+ trainer
bool32 AI_CanSeePartyMon(u8 battler, u8 partyIndex);        // known, or has been on the field
u16 AI_GetPartyMonMove(u8 battler, u8 partyIndex, u8 slot); // MOVE_NONE if the AI hasn't seen it
u8 AI_GetPartyMonAbility(u8 battler, u8 partyIndex);
bool32 AI_MoveSlotHasPP(u8 battler, u8 slot);               // always TRUE for moves whose PP the AI can't see
void AI_RecordAbility(u8 battler, u8 ability);              // the battle showed the ability
void AI_RecordItem(u8 battler);                             // the battle showed the held item

// Damage
struct SimulatedDamage AI_CalcDamage(u16 move, u8 battlerAtk, u8 battlerDef, uq4_12_t *effectiveness, bool32 isCrit);
u32 AI_GetDamage(u8 battlerAtk, u8 battlerDef, u8 moveIndex, u8 calcContext);
u32 GetNoOfHitsToKO(u32 dmg, s32 hp);
u32 GetNoOfHitsToKOBattler(u8 battlerAtk, u8 battlerDef, u8 moveIndex, u8 calcContext, u8 considerEndure);

// Accuracy, from 0 to 100
u8 AI_GetMoveAccuracy(u8 battlerAtk, u8 battlerDef, u16 move);
bool32 AI_CanHitSemiInvulnerable(u16 move, u8 battlerDef);

// Speed and turn order (Quick Claw is never counted on)
u32 AI_GetBattlerSpeed(u8 battler);
s32 AI_WhoStrikesFirst(u8 battlerAi, u8 battler, u16 aiMove, u16 otherMove, bool32 considerPriority);
bool32 AI_IsFaster(u8 battlerAi, u8 battler, u16 aiMove, u16 otherMove, bool32 considerPriority);
bool32 AI_IsSlower(u8 battlerAi, u8 battler, u16 aiMove, u16 otherMove, bool32 considerPriority);

// Per-decision cache
void AI_SetLogicDataForTurn(u8 battlerAi);
void AI_ClearLogicData(void);

// Saves the battlers' data, the AI's view and the AI's thinking, puts the
// battler's moves, item and ability as the AI sees them in gBattleMons (so the
// AI can think as that battler) and restores everything at the end. The
// knowledge in playerParty is kept.
void *AI_BeginPlayAsBattler(u8 battler);
void AI_EndPlayAsBattler(void *backup);

// Switch-in simulation. Begin saves the battler's data, SetSwitchinCandidate
// puts a party mon in its place (and updates the cache), End restores
// everything. Only call SetSwitchinCandidate between Begin and End.
void *AI_BeginSwitchinSimulation(u8 battler);
void AI_SetSwitchinCandidate(u8 battler, struct Pokemon *mon, u8 partyIndex);
void AI_EndSwitchinSimulation(u8 battler, void *backup);

#if AI_DEBUG_CHECKS
void AI_DebugCheckDamage(void);
void AI_DebugCheckTurnOrder(void);
#endif

#endif // GUARD_BATTLE_AI_ENGINE_H
