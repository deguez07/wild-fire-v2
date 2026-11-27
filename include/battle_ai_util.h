#ifndef GUARD_BATTLE_AI_UTIL_H
#define GUARD_BATTLE_AI_UTIL_H

// Helpers for the move scoring in battle_ai_main.c, ported from
// pokefirered-expansion's battle_ai_util.c and trimmed to Gen 1-3: only the
// moves, abilities and items FireRed has, with Gen 3 mechanics.
//
// They only read the battle. Information about the player's side comes from
// the AI's view in AI_LOGIC_DATA (battle_ai_engine.h), never from the move or
// switch the player picked this turn.

#include "battle_ai_engine.h"

#define AI_THINKING_STRUCT (gBattleResources->ai)

#define LEFT_FOE(battler)  ((BATTLE_OPPOSITE(battler)) & BIT_SIDE)
#define RIGHT_FOE(battler) (((BATTLE_OPPOSITE(battler)) & BIT_SIDE) | BIT_FLANK)

#define UNKNOWN_NO_OF_HITS 0xFFFFFFFF

// Tuning, same values as expansion's include/config/ai.h.
#define LOW_ACCURACY_THRESHOLD          75  // moves at or below this accuracy count as inaccurate
#define STATUS_MOVE_FOCUS_PUNCH_CHANCE  50  // chance to avoid a status move when the target's best move is Focus Punch
#define SHOULD_RECOVER_CHANCE           50  // chance to heal when below ENABLE_RECOVERY_THRESHOLD and not in danger
#define ENABLE_RECOVERY_THRESHOLD       60
#define PRIORITIZE_LAST_CHANCE_CHANCE   50  // chance to prefer a priority move when about to be KO'd
#define EXPLOSION_LOWER_HP_THRESHOLD    10  // at or below this HP%, 90% chance to consider exploding
#define EXPLOSION_HIGHER_HP_THRESHOLD   90  // at or above this HP%, never considers exploding
#define EXPLOSION_MINIMUM_CHANCE        0
#define EXPLOSION_MAXIMUM_CHANCE        90

// Scores added by the move effect checks.
enum AIScore
{
    NO_INCREASE = 0,
    WEAK_EFFECT = 1,
    DECENT_EFFECT = 2,
    GOOD_EFFECT = 3,
    BEST_EFFECT = 4,
    PERFECT_EFFECT = 10,
    BAD_EFFECT = -1,
    AWFUL_EFFECT = -3,
    WORST_EFFECT = -10
};

#define BEST_DAMAGE_MOVE     1   // move with the fewest hits to KO and the best accuracy/effect
#define NO_DAMAGE_OR_FAILS   -20 // move fails or does no damage

// AI_TryToFaint
#define FAST_KILL   6 // AI is faster and KOs the target
#define SLOW_KILL   4 // AI is slower and KOs the target
#define LAST_CHANCE 2 // target KOs the AI first: prefer a priority move

// AI_Risky
#define STRONG_RISKY_EFFECT  3
#define AVERAGE_RISKY_EFFECT 2

// Stat raises, for IncreaseStatUpScore.
enum StatChange
{
    STAT_CHANGE_ATK,
    STAT_CHANGE_DEF,
    STAT_CHANGE_SPEED,
    STAT_CHANGE_SPATK,
    STAT_CHANGE_SPDEF,
    STAT_CHANGE_ATK_2,
    STAT_CHANGE_DEF_2,
    STAT_CHANGE_SPEED_2,
    STAT_CHANGE_SPATK_2,
    STAT_CHANGE_SPDEF_2,
    STAT_CHANGE_ATK_MAX,
    STAT_CHANGE_ACC,
    STAT_CHANGE_EVASION
};

enum MoveComparisonResult
{
    MOVE_NEUTRAL_COMPARISON,
    MOVE_WON_COMPARISON,
    MOVE_LOST_COMPARISON,
};

// A move's secondary effect. Gen 3 stores it in the move's battle script, so
// GetMoveSecondaryEffect maps each move effect to it.
struct AiSecondaryEffect
{
    u8 moveEffect; // MOVE_EFFECT_*, 0 if none
    bool8 self;    // affects the user
    bool8 certain; // always happens when the move hits
};

bool32 RandomPercentage(u32 percent);

// Battlers and sides
bool32 IsTargetingPartner(u8 battlerAtk, u8 battlerDef);
bool32 HasPartner(u8 battler);
bool32 HasTwoOpponents(u8 battler);
bool32 IsBattle1v1(void);
struct Pokemon *AI_GetBattlerParty(u8 battler);
s32 CountUsablePartyMons(u8 battler);
bool32 AI_IsAbilityOnSide(u8 battler, u8 ability);
bool32 AI_BattlerAtMaxHp(u8 battler);
bool32 AreBattlersOfOppositeGender(u8 battler1, u8 battler2);

// Moves
bool32 IsMoveUnusable(u8 moveIndex, u16 move, u8 moveLimitations);
u8 GetMoveIndex(u8 battler, u16 move);
struct AiSecondaryEffect GetMoveSecondaryEffect(u16 move);
bool32 MoveEffectIsGuaranteed(u8 battlerAtk, u16 move, const struct AiSecondaryEffect *secondary);
bool32 IsExplosionMove(u16 move);
bool32 IsSelfSacrificeEffect(u16 move);
bool32 IsTwoTurnNotSemiInvulnerableMove(u8 battlerAtk, u16 move);
bool32 IsStatRaisingEffect(u8 effect);
bool32 IsStatLoweringEffect(u8 effect);
u8 GetMoveNonVolatileStatus(u16 move);
bool32 HasMove(u8 battler, u16 move);
bool32 HasMoveWithEffect(u8 battler, u8 effect);
bool32 HasMoveWithCategory(u8 battler, u8 category);
bool32 HasMoveWithType(u8 battler, u8 type);
bool32 HasDamagingMove(u8 battler);
bool32 HasDamagingMoveOfType(u8 battler, u8 type);
bool32 HasMoveWithFlag(u8 battler, u8 flag);
bool32 HasNonVolatileMoveEffect(u8 battler, u8 moveEffect);
bool32 HasMoveWithLowAccuracy(u8 battlerAtk, u8 battlerDef, u32 accCheck, bool32 ignoreStatus);
bool32 HasThawingMove(u8 battler);
bool32 HasUsableWhileAsleepMove(u8 battler);
bool32 HasMoveWithFlinchEffect(u8 battler);

// Damage and KOs
bool32 CanIndexMoveFaintTarget(u8 battlerAtk, u8 battlerDef, u8 moveIndex, u8 calcContext);
bool32 CanTargetFaintAi(u8 battlerDef, u8 battlerAtk);
bool32 CanTargetFaintAiWithMod(u8 battlerDef, u8 battlerAtk, s32 hpMod, s32 dmgMod);
bool32 CanTargetMoveFaintAi(u16 move, u8 battlerDef, u8 battlerAtk, u32 numHits);
bool32 CanAIFaintTarget(u8 battlerAtk, u8 battlerDef, u32 numHits);
u32 NoOfHitsForTargetToFaintBattler(u8 battlerDef, u8 battlerAtk);
u32 NoOfHitsForTargetToFaintBattlerWithMod(u8 battlerDef, u8 battlerAtk, s32 hpMod);
u32 GetBestDmgFromBattler(u8 battler, u8 battlerTarget, u8 calcContext);
void GetBestDmgMovesFromBattler(u8 battlerAtk, u8 battlerDef, u8 calcContext, u16 *bestMoves);
bool32 IsBestDmgMove(u8 battlerAtk, u8 battlerDef, u8 calcContext, u16 move);
bool32 BestDmgMoveHasEffect(u8 battlerAtk, u8 battlerDef, u8 calcContext, u8 moveEffect);
bool32 HasPhysicalBestMove(u8 battlerAtk, u8 battlerDef, u8 calcContext);
u32 GetBattlerSecondaryDamage(u8 battler);
bool32 BattlerWillFaintFromWeather(u8 battler, u8 ability);
bool32 BattlerWillFaintFromSecondaryDamage(u8 battler, u8 ability);
bool32 ShouldConsiderSelfSacrificeDamageEffect(u8 battlerAtk, u8 battlerDef, u16 move, bool32 aiIsFaster);
u32 GetAIExplosionChanceFromHP(u32 hpPercent);
enum MoveComparisonResult CompareMoveEffects(u16 move1, u16 move2, u8 battlerAtk, u8 battlerDef, s32 noOfHitsToKo);

// Stats
bool32 BattlerStatCanRise(u8 battler, u8 ability, u8 stat);
bool32 AreBattlersStatsMaxed(u8 battler);
bool32 AnyStatIsRaised(u8 battler);
u32 CountPositiveStatStages(u8 battler);
u32 CountNegativeStatStages(u8 battler);
bool32 CanLowerStat(u8 battlerAtk, u8 battlerDef, u8 stat);
enum AIScore IncreaseStatUpScore(u8 battlerAtk, u8 battlerDef, u8 statChange);
enum AIScore IncreaseStatDownScore(u8 battlerAtk, u8 battlerDef, u8 stat);
s32 AI_TryToClearStats(u8 battlerAtk, u8 battlerDef, bool32 isDoubleBattle);
bool32 AI_ShouldCopyStatChanges(u8 battlerAtk, u8 battlerDef);

// Status conditions
bool32 DoesSubstituteBlockMove(u8 battlerAtk, u8 battlerDef, u16 move);
bool32 AI_CanBeSlept(u8 battler, u8 ability);
bool32 AI_CanBePoisoned(u8 battlerDef, u8 defAbility);
bool32 AI_CanBeBurned(u8 battlerDef, u8 defAbility);
bool32 AI_CanBeParalyzed(u8 battlerDef, u8 defAbility);
bool32 AI_CanBeFrozen(u8 battlerDef, u8 defAbility);
bool32 AI_CanPutToSleep(u8 battlerAtk, u8 battlerDef, u8 defAbility, u16 move);
bool32 AI_CanPoison(u8 battlerAtk, u8 battlerDef, u8 defAbility, u16 move);
bool32 AI_CanParalyze(u8 battlerAtk, u8 battlerDef, u8 defAbility, u16 move);
bool32 AI_CanBurn(u8 battlerAtk, u8 battlerDef, u8 defAbility, u16 move);
bool32 AI_CanConfuse(u8 battlerAtk, u8 battlerDef, u8 defAbility, u16 move);
bool32 AI_CanBeInfatuated(u8 battlerAtk, u8 battlerDef, u8 defAbility);
bool32 ShouldPoison(u8 battlerAtk, u8 battlerDef);
bool32 ShouldBurn(u8 battlerAtk, u8 battlerDef, u8 abilityDef);
bool32 ShouldParalyze(u8 battlerAtk, u8 battlerDef, u8 abilityDef);
bool32 ShouldCureStatus(u8 battlerAtk, u8 battlerDef);
bool32 AnyPartyMemberStatused(u8 battler, bool32 checkSoundproof);
bool32 IsBattlerIncapacitated(u8 battler, u8 ability);
bool32 AI_IsBattlerAsleep(u8 battler);
bool32 IsWakeupTurn(u8 battler);
bool32 IsBattlerTrapped(u8 battlerAtk, u8 battlerDef);
bool32 HasChoiceEffect(u8 battler);
void IncreasePoisonScore(u8 battlerAtk, u8 battlerDef, u16 move, s32 *score);
void IncreaseBurnScore(u8 battlerAtk, u8 battlerDef, u16 move, s32 *score);
void IncreaseParalyzeScore(u8 battlerAtk, u8 battlerDef, u16 move, s32 *score);
void IncreaseSleepScore(u8 battlerAtk, u8 battlerDef, u16 move, s32 *score);
void IncreaseConfusionScore(u8 battlerAtk, u8 battlerDef, u16 move, s32 *score);

// Situational checks
bool32 ShouldTryOHKO(u8 battlerAtk, u8 battlerDef, u8 atkAbility, u8 defAbility, u16 move);
bool32 ShouldTryToFlinch(u8 battlerAtk, u8 battlerDef, u8 atkAbility, u8 defAbility, u16 move);
bool32 IsFlinchGuaranteed(u8 battlerAtk, u8 battlerDef, u16 move);
bool32 ShouldTrap(u8 battlerAtk, u8 battlerDef, u16 move);
bool32 ShouldAbsorb(u8 battlerAtk, u8 battlerDef, u16 move);
bool32 ShouldRecover(u8 battlerAtk, u8 battlerDef, u16 move, u32 healPercent);
bool32 ShouldSetScreen(u8 battlerAtk, u8 battlerDef, u8 moveEffect);
bool32 ShouldUseWishAromatherapy(u8 battlerAtk, u8 battlerDef, u16 move);
bool32 AI_ShouldSetUpHazards(u8 battlerAtk, u8 battlerDef, u16 move);
bool32 HasHPForDamagingSetup(u8 battlerAtk, u8 battlerDef, u32 hpThreshold);
s32 ProtectChecks(u8 battlerAtk, u8 battlerDef, u16 move, u16 predictedMove);
u32 IncreaseSubstituteMoveScore(u8 battlerAtk, u8 battlerDef, u16 move);
s32 CalcWeatherScore(u8 battlerAtk, u8 battlerDef, u16 move);

#endif // GUARD_BATTLE_AI_UTIL_H
