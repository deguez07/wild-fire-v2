#include "global.h"
#include "battle.h"
#include "battle_ai_engine.h"
#include "battle_ai_util.h"
#include "battle_anim.h"
#include "battle_util.h"
#include "pokemon.h"
#include "random.h"
#include "util.h"
#include "constants/abilities.h"
#include "constants/battle_ai.h"
#include "constants/battle_move_effects.h"
#include "constants/hold_effects.h"
#include "constants/items.h"
#include "constants/moves.h"

// See include/battle_ai_util.h. Ported from pokefirered-expansion's
// battle_ai_util.c with only the Gen 1-3 parts kept.

#define AI_DATA (AI_LOGIC_DATA)

bool32 RandomPercentage(u32 percent)
{
    if (percent == 0)
        return FALSE;
    if (percent >= 100)
        return TRUE;
    return (Random() % 100) < percent;
}

// ---------------------------------------------------------------------------
// Battlers and sides
// ---------------------------------------------------------------------------

bool32 IsTargetingPartner(u8 battlerAtk, u8 battlerDef)
{
    return IsDoubleBattle() && battlerDef == BATTLE_PARTNER(battlerAtk);
}

bool32 HasPartner(u8 battler)
{
    return IsDoubleBattle() && IsBattlerAlive(BATTLE_PARTNER(battler));
}

bool32 HasTwoOpponents(u8 battler)
{
    return IsDoubleBattle() && IsBattlerAlive(LEFT_FOE(battler)) && IsBattlerAlive(RIGHT_FOE(battler));
}

bool32 IsBattle1v1(void)
{
    if (IsDoubleBattle()
     && ((IsBattlerAlive(GetBattlerAtPosition(B_POSITION_PLAYER_LEFT)) && IsBattlerAlive(GetBattlerAtPosition(B_POSITION_PLAYER_RIGHT)))
      || (IsBattlerAlive(GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT)) && IsBattlerAlive(GetBattlerAtPosition(B_POSITION_OPPONENT_RIGHT)))))
        return FALSE;
    return TRUE;
}

struct Pokemon *AI_GetBattlerParty(u8 battler)
{
    return (GetBattlerSide(battler) == B_SIDE_PLAYER) ? gPlayerParty : gEnemyParty;
}

// Party members that could still be sent in (alive and not on the field).
// The number of mons the player has left is shown by the party balls.
s32 CountUsablePartyMons(u8 battler)
{
    struct Pokemon *party = AI_GetBattlerParty(battler);
    u8 onField1, onField2;
    s32 i, count = 0;

    onField1 = gBattlerPartyIndexes[battler];
    if (IsDoubleBattle())
        onField2 = gBattlerPartyIndexes[BATTLE_PARTNER(battler)];
    else
        onField2 = onField1;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (i == onField1 || i == onField2)
            continue;
        if (GetMonData(&party[i], MON_DATA_HP) != 0
         && GetMonData(&party[i], MON_DATA_SPECIES_OR_EGG) != SPECIES_NONE
         && GetMonData(&party[i], MON_DATA_SPECIES_OR_EGG) != SPECIES_EGG)
            count++;
    }
    return count;
}

bool32 AI_IsAbilityOnSide(u8 battler, u8 ability)
{
    if (IsBattlerAlive(battler) && AI_DATA->abilities[battler] == ability)
        return TRUE;
    if (IsDoubleBattle() && IsBattlerAlive(BATTLE_PARTNER(battler)) && AI_DATA->abilities[BATTLE_PARTNER(battler)] == ability)
        return TRUE;
    return FALSE;
}

bool32 AI_BattlerAtMaxHp(u8 battler)
{
    return AI_DATA->hpPercents[battler] == 100 || gBattleMons[battler].hp == gBattleMons[battler].maxHP;
}

static u8 GetBattlerGender(u8 battler)
{
    return GetGenderFromSpeciesAndPersonality(gBattleMons[battler].species, gBattleMons[battler].personality);
}

bool32 AreBattlersOfOppositeGender(u8 battler1, u8 battler2)
{
    u8 gender1 = GetBattlerGender(battler1);
    u8 gender2 = GetBattlerGender(battler2);

    return gender1 != MON_GENDERLESS && gender2 != MON_GENDERLESS && gender1 != gender2;
}

// ---------------------------------------------------------------------------
// Moves
// ---------------------------------------------------------------------------

bool32 IsMoveUnusable(u8 moveIndex, u16 move, u8 moveLimitations)
{
    return move == MOVE_NONE || move == MOVE_UNAVAILABLE || (moveLimitations & gBitTable[moveIndex]);
}

u8 GetMoveIndex(u8 battler, u16 move)
{
    u16 *moves = AI_GetMovesArray(battler);
    u8 i;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] == move)
            return i;
    }
    return MAX_MON_MOVES;
}

// The MOVE_EFFECT_* each move effect's battle script sets (data/battle_scripts_1.s).
struct AiSecondaryEffect GetMoveSecondaryEffect(u16 move)
{
    struct AiSecondaryEffect secondary = {0, FALSE, FALSE};

    switch (gBattleMoves[move].effect)
    {
    case EFFECT_POISON_HIT:
    case EFFECT_POISON_TAIL:
    case EFFECT_TWINEEDLE:
        secondary.moveEffect = MOVE_EFFECT_POISON;
        break;
    case EFFECT_POISON_FANG:
        secondary.moveEffect = MOVE_EFFECT_TOXIC;
        break;
    case EFFECT_BURN_HIT:
    case EFFECT_BLAZE_KICK:
    case EFFECT_THAW_HIT:
        secondary.moveEffect = MOVE_EFFECT_BURN;
        break;
    case EFFECT_FREEZE_HIT:
        secondary.moveEffect = MOVE_EFFECT_FREEZE;
        break;
    case EFFECT_PARALYZE_HIT:
    case EFFECT_THUNDER:
        secondary.moveEffect = MOVE_EFFECT_PARALYSIS;
        break;
    case EFFECT_FLINCH_HIT:
    case EFFECT_FLINCH_MINIMIZE_HIT:
    case EFFECT_TWISTER:
    case EFFECT_SNORE:
    case EFFECT_SKY_ATTACK:
        secondary.moveEffect = MOVE_EFFECT_FLINCH;
        break;
    case EFFECT_FAKE_OUT:
        secondary.moveEffect = MOVE_EFFECT_FLINCH;
        secondary.certain = TRUE;
        break;
    case EFFECT_CONFUSE_HIT:
        secondary.moveEffect = MOVE_EFFECT_CONFUSION;
        break;
    case EFFECT_TRI_ATTACK:
        secondary.moveEffect = MOVE_EFFECT_TRI_ATTACK;
        break;
    case EFFECT_ATTACK_DOWN_HIT:
        secondary.moveEffect = MOVE_EFFECT_ATK_MINUS_1;
        break;
    case EFFECT_DEFENSE_DOWN_HIT:
        secondary.moveEffect = MOVE_EFFECT_DEF_MINUS_1;
        break;
    case EFFECT_SPEED_DOWN_HIT:
        secondary.moveEffect = MOVE_EFFECT_SPD_MINUS_1;
        break;
    case EFFECT_SPECIAL_ATTACK_DOWN_HIT:
        secondary.moveEffect = MOVE_EFFECT_SP_ATK_MINUS_1;
        break;
    case EFFECT_SPECIAL_DEFENSE_DOWN_HIT:
        secondary.moveEffect = MOVE_EFFECT_SP_DEF_MINUS_1;
        break;
    case EFFECT_ACCURACY_DOWN_HIT:
        secondary.moveEffect = MOVE_EFFECT_ACC_MINUS_1;
        break;
    case EFFECT_ATTACK_UP_HIT:
        secondary.moveEffect = MOVE_EFFECT_ATK_PLUS_1;
        secondary.self = TRUE;
        break;
    case EFFECT_DEFENSE_UP_HIT:
        secondary.moveEffect = MOVE_EFFECT_DEF_PLUS_1;
        secondary.self = TRUE;
        break;
    case EFFECT_ALL_STATS_UP_HIT:
        secondary.moveEffect = MOVE_EFFECT_ALL_STATS_UP;
        secondary.self = TRUE;
        break;
    case EFFECT_TRAP:
        secondary.moveEffect = MOVE_EFFECT_WRAP;
        break;
    case EFFECT_OVERHEAT:
        secondary.moveEffect = MOVE_EFFECT_SP_ATK_TWO_DOWN;
        secondary.self = TRUE;
        secondary.certain = TRUE;
        break;
    case EFFECT_SUPERPOWER:
        secondary.moveEffect = MOVE_EFFECT_ATK_DEF_DOWN;
        secondary.self = TRUE;
        secondary.certain = TRUE;
        break;
    case EFFECT_RECHARGE:
        secondary.moveEffect = MOVE_EFFECT_RECHARGE;
        secondary.self = TRUE;
        secondary.certain = TRUE;
        break;
    case EFFECT_RAPID_SPIN:
        secondary.moveEffect = MOVE_EFFECT_RAPIDSPIN;
        secondary.self = TRUE;
        secondary.certain = TRUE;
        break;
    case EFFECT_SMELLINGSALT:
        secondary.moveEffect = MOVE_EFFECT_REMOVE_PARALYSIS;
        secondary.certain = TRUE;
        break;
    default:
        break;
    }
    return secondary;
}

// The effect happens whenever the move hits (Serene Grace doubles the chance).
bool32 MoveEffectIsGuaranteed(u8 battlerAtk, u16 move, const struct AiSecondaryEffect *secondary)
{
    u32 chance = gBattleMoves[move].secondaryEffectChance;

    if (secondary->moveEffect == 0)
        return FALSE;
    if (secondary->certain)
        return TRUE;
    if (AI_DATA->abilities[battlerAtk] == ABILITY_SERENE_GRACE)
        chance *= 2;
    return chance >= 100;
}

bool32 IsExplosionMove(u16 move)
{
    return gBattleMoves[move].effect == EFFECT_EXPLOSION;
}

bool32 IsSelfSacrificeEffect(u16 move)
{
    return IsExplosionMove(move) || gBattleMoves[move].effect == EFFECT_MEMENTO;
}

// Charging moves that leave the user open on the first turn.
bool32 IsTwoTurnNotSemiInvulnerableMove(u8 battlerAtk, u16 move)
{
    switch (gBattleMoves[move].effect)
    {
    case EFFECT_SOLAR_BEAM:
        return !(AI_GetWeather() & B_WEATHER_SUN);
    case EFFECT_RAZOR_WIND:
    case EFFECT_SKY_ATTACK:
    case EFFECT_SKULL_BASH:
        return TRUE;
    default:
        return FALSE;
    }
}

bool32 IsStatRaisingEffect(u8 effect)
{
    switch (effect)
    {
    case EFFECT_ATTACK_UP:
    case EFFECT_ATTACK_UP_2:
    case EFFECT_DEFENSE_UP:
    case EFFECT_DEFENSE_UP_2:
    case EFFECT_SPEED_UP:
    case EFFECT_SPEED_UP_2:
    case EFFECT_SPECIAL_ATTACK_UP:
    case EFFECT_SPECIAL_ATTACK_UP_2:
    case EFFECT_SPECIAL_DEFENSE_UP:
    case EFFECT_SPECIAL_DEFENSE_UP_2:
    case EFFECT_ACCURACY_UP:
    case EFFECT_ACCURACY_UP_2:
    case EFFECT_EVASION_UP:
    case EFFECT_EVASION_UP_2:
    case EFFECT_MINIMIZE:
    case EFFECT_DEFENSE_CURL:
    case EFFECT_CALM_MIND:
    case EFFECT_COSMIC_POWER:
    case EFFECT_DRAGON_DANCE:
    case EFFECT_BULK_UP:
        return TRUE; // Gen 3 Stockpile doesn't raise stats
    default:
        return FALSE;
    }
}

bool32 IsStatLoweringEffect(u8 effect)
{
    switch (effect)
    {
    case EFFECT_ATTACK_DOWN:
    case EFFECT_DEFENSE_DOWN:
    case EFFECT_SPEED_DOWN:
    case EFFECT_SPECIAL_ATTACK_DOWN:
    case EFFECT_SPECIAL_DEFENSE_DOWN:
    case EFFECT_ACCURACY_DOWN:
    case EFFECT_EVASION_DOWN:
    case EFFECT_ATTACK_DOWN_2:
    case EFFECT_DEFENSE_DOWN_2:
    case EFFECT_SPEED_DOWN_2:
    case EFFECT_SPECIAL_ATTACK_DOWN_2:
    case EFFECT_SPECIAL_DEFENSE_DOWN_2:
    case EFFECT_ACCURACY_DOWN_2:
    case EFFECT_EVASION_DOWN_2:
    case EFFECT_TICKLE:
    case EFFECT_MEMENTO:
        return TRUE;
    default:
        return FALSE;
    }
}

// Status condition a status move inflicts (expansion's EFFECT_NON_VOLATILE_STATUS).
u8 GetMoveNonVolatileStatus(u16 move)
{
    switch (gBattleMoves[move].effect)
    {
    case EFFECT_SLEEP:
        return MOVE_EFFECT_SLEEP;
    case EFFECT_POISON:
        return MOVE_EFFECT_POISON;
    case EFFECT_TOXIC:
        return MOVE_EFFECT_TOXIC;
    case EFFECT_PARALYZE:
        return MOVE_EFFECT_PARALYSIS;
    case EFFECT_WILL_O_WISP:
        return MOVE_EFFECT_BURN;
    default:
        return 0;
    }
}

bool32 HasMove(u8 battler, u16 move)
{
    u16 *moves = AI_GetMovesArray(battler);
    u8 i;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] == move && move != MOVE_NONE)
            return TRUE;
    }
    return FALSE;
}

bool32 HasMoveWithEffect(u8 battler, u8 effect)
{
    u16 *moves = AI_GetMovesArray(battler);
    u8 i;

    if (!IsBattlerAlive(battler))
        return FALSE;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] != MOVE_NONE && moves[i] != MOVE_UNAVAILABLE && gBattleMoves[moves[i]].effect == effect)
            return TRUE;
    }
    return FALSE;
}

bool32 HasMoveWithCategory(u8 battler, u8 category)
{
    u16 *moves = AI_GetMovesArray(battler);
    u8 moveLimitations = AI_DATA->moveLimitations[battler];
    u8 i;

    if (!IsBattlerAlive(battler))
        return FALSE;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (IsMoveUnusable(i, moves[i], moveLimitations))
            continue;
        if (GetBattleMoveCategory(moves[i], battler) == category)
            return TRUE;
    }
    return FALSE;
}

bool32 HasMoveWithType(u8 battler, u8 type)
{
    u16 *moves = AI_GetMovesArray(battler);
    u8 moveLimitations = AI_DATA->moveLimitations[battler];
    u8 i;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (IsMoveUnusable(i, moves[i], moveLimitations))
            continue;
        if (GetBattleMoveType(moves[i], battler) == type)
            return TRUE;
    }
    return FALSE;
}

bool32 HasDamagingMove(u8 battler)
{
    u16 *moves = AI_GetMovesArray(battler);
    u8 moveLimitations = AI_DATA->moveLimitations[battler];
    u8 i;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (!IsMoveUnusable(i, moves[i], moveLimitations) && gBattleMoves[moves[i]].power != 0)
            return TRUE;
    }
    return FALSE;
}

bool32 HasDamagingMoveOfType(u8 battler, u8 type)
{
    u16 *moves = AI_GetMovesArray(battler);
    u8 moveLimitations = AI_DATA->moveLimitations[battler];
    u8 i;

    if (!IsBattlerAlive(battler))
        return FALSE;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (!IsMoveUnusable(i, moves[i], moveLimitations)
         && gBattleMoves[moves[i]].power != 0
         && GetBattleMoveType(moves[i], battler) == type)
            return TRUE;
    }
    return FALSE;
}

// FLAG_MAGIC_COAT_AFFECTED, FLAG_SNATCH_AFFECTED, ...
bool32 HasMoveWithFlag(u8 battler, u8 flag)
{
    u16 *moves = AI_GetMovesArray(battler);
    u8 moveLimitations = AI_DATA->moveLimitations[battler];
    u8 i;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (!IsMoveUnusable(i, moves[i], moveLimitations) && (gBattleMoves[moves[i]].flags & flag))
            return TRUE;
    }
    return FALSE;
}

bool32 HasNonVolatileMoveEffect(u8 battler, u8 moveEffect)
{
    u16 *moves = AI_GetMovesArray(battler);
    u8 moveLimitations = AI_DATA->moveLimitations[battler];
    u8 i;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (!IsMoveUnusable(i, moves[i], moveLimitations) && GetMoveNonVolatileStatus(moves[i]) == moveEffect)
            return TRUE;
    }
    return FALSE;
}

bool32 HasMoveWithLowAccuracy(u8 battlerAtk, u8 battlerDef, u32 accCheck, bool32 ignoreStatus)
{
    u16 *moves = AI_GetMovesArray(battlerAtk);
    u8 moveLimitations = AI_DATA->moveLimitations[battlerAtk];
    u8 i, target;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (IsMoveUnusable(i, moves[i], moveLimitations))
            continue;
        if (ignoreStatus && IsBattleMoveStatus(moves[i]))
            continue;
        if (!IsBattleMoveStatus(moves[i]) && gBattleMoves[moves[i]].accuracy == 0)
            continue;
        target = gBattleMoves[moves[i]].target;
        if (target & (MOVE_TARGET_USER | MOVE_TARGET_OPPONENTS_FIELD))
            continue;
        if (AI_DATA->moveAccuracy[battlerAtk][battlerDef][i] <= accCheck)
            return TRUE;
    }
    return FALSE;
}

// Flame Wheel and Sacred Fire thaw the user out.
bool32 HasThawingMove(u8 battler)
{
    return HasMoveWithEffect(battler, EFFECT_THAW_HIT);
}

bool32 HasUsableWhileAsleepMove(u8 battler)
{
    return HasMoveWithEffect(battler, EFFECT_SLEEP_TALK) || HasMoveWithEffect(battler, EFFECT_SNORE);
}

// A move that can flinch, except Fake Out.
bool32 HasMoveWithFlinchEffect(u8 battler)
{
    u16 *moves = AI_GetMovesArray(battler);
    u8 i;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] == MOVE_NONE || moves[i] == MOVE_UNAVAILABLE || gBattleMoves[moves[i]].effect == EFFECT_FAKE_OUT)
            continue;
        if (GetMoveSecondaryEffect(moves[i]).moveEffect == MOVE_EFFECT_FLINCH)
            return TRUE;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Damage and KOs
// ---------------------------------------------------------------------------

bool32 CanIndexMoveFaintTarget(u8 battlerAtk, u8 battlerDef, u8 moveIndex, u8 calcContext)
{
    s32 dmg;

    if (IsDoubleBattle() && battlerDef == BATTLE_PARTNER(battlerAtk))
        dmg = AI_DATA->simulatedDmg[battlerAtk][battlerDef][moveIndex].maximum; // attacking the partner: be careful
    else
        dmg = AI_GetDamage(battlerAtk, battlerDef, moveIndex, calcContext);

    return gBattleMons[battlerDef].hp <= dmg;
}

// Can the target KO the AI with one of its moves?
bool32 CanTargetFaintAi(u8 battlerDef, u8 battlerAtk)
{
    u16 *moves = AI_GetMovesArray(battlerDef);
    u8 moveLimitations = AI_DATA->moveLimitations[battlerDef];
    u8 i;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (IsMoveUnusable(i, moves[i], moveLimitations))
            continue;
        if (AI_GetDamage(battlerDef, battlerAtk, i, AI_DEFENDING) >= gBattleMons[battlerAtk].hp)
            return TRUE;
    }
    return FALSE;
}

bool32 CanTargetFaintAiWithMod(u8 battlerDef, u8 battlerAtk, s32 hpMod, s32 dmgMod)
{
    u16 *moves = AI_GetMovesArray(battlerDef);
    u8 moveLimitations = AI_DATA->moveLimitations[battlerDef];
    u32 hpCheck = gBattleMons[battlerAtk].hp + hpMod;
    s32 dmg;
    u8 i;

    if (hpCheck > gBattleMons[battlerAtk].maxHP)
        hpCheck = gBattleMons[battlerAtk].maxHP;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (IsMoveUnusable(i, moves[i], moveLimitations))
            continue;
        dmg = AI_GetDamage(battlerDef, battlerAtk, i, AI_DEFENDING);
        if (dmgMod)
            dmg *= dmgMod;
        if (dmg >= hpCheck)
            return TRUE;
    }
    return FALSE;
}

bool32 CanTargetMoveFaintAi(u16 move, u8 battlerDef, u8 battlerAtk, u32 numHits)
{
    u8 moveIndex = GetMoveIndex(battlerDef, move);
    u32 dmg;

    if (moveIndex == MAX_MON_MOVES)
        return FALSE;
    dmg = AI_GetDamage(battlerDef, battlerAtk, moveIndex, AI_DEFENDING);
    if (numHits)
        dmg *= numHits;
    return dmg >= gBattleMons[battlerAtk].hp;
}

// Can the AI KO the target with any of its moves? With numHits > 1, can it
// KO the target with that many hits of one move?
bool32 CanAIFaintTarget(u8 battlerAtk, u8 battlerDef, u32 numHits)
{
    u16 *moves = gBattleMons[battlerAtk].moves;
    u8 moveLimitations = AI_DATA->moveLimitations[battlerAtk];
    s32 dmg;
    u8 i;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (IsMoveUnusable(i, moves[i], moveLimitations))
            continue;
        dmg = AI_GetDamage(battlerAtk, battlerDef, i, AI_ATTACKING);
        if (numHits)
            dmg *= numHits;
        if (gBattleMons[battlerDef].hp <= dmg)
            return TRUE;
    }
    return FALSE;
}

u32 NoOfHitsForTargetToFaintBattler(u8 battlerDef, u8 battlerAtk)
{
    u32 hits, leastHits = UNKNOWN_NO_OF_HITS;
    u8 i;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        hits = GetNoOfHitsToKOBattler(battlerDef, battlerAtk, i, AI_DEFENDING, CONSIDER_ENDURE);
        if (hits != 0 && hits < leastHits)
            leastHits = hits;
    }
    return leastHits;
}

u32 NoOfHitsForTargetToFaintBattlerWithMod(u8 battlerDef, u8 battlerAtk, s32 hpMod)
{
    u32 hits, leastHits = UNKNOWN_NO_OF_HITS;
    u32 hpCheck = gBattleMons[battlerAtk].hp + hpMod;
    u32 dmg;
    u8 i;

    if (hpCheck > gBattleMons[battlerAtk].maxHP)
        hpCheck = gBattleMons[battlerAtk].maxHP;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        dmg = AI_GetDamage(battlerDef, battlerAtk, i, AI_DEFENDING);
        if (dmg == 0)
            continue;
        hits = hpCheck / (dmg + 1) + 1;
        if (hits < leastHits)
            leastHits = hits;
    }
    return leastHits;
}

u32 GetBestDmgFromBattler(u8 battler, u8 battlerTarget, u8 calcContext)
{
    u16 *moves = AI_GetMovesArray(battler);
    u8 moveLimitations = AI_DATA->moveLimitations[battler];
    u32 dmg, bestDmg = 0;
    u8 i;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (IsMoveUnusable(i, moves[i], moveLimitations))
            continue;
        dmg = AI_GetDamage(battler, battlerTarget, i, calcContext);
        if (bestDmg < dmg)
            bestDmg = dmg;
    }
    return bestDmg;
}

// The moves that KO the target or, if none does, deal the most damage.
// bestMoves must hold MAX_MON_MOVES moves, cleared to MOVE_NONE.
void GetBestDmgMovesFromBattler(u8 battlerAtk, u8 battlerDef, u8 calcContext, u16 *bestMoves)
{
    u16 *moves = AI_GetMovesArray(battlerAtk);
    u8 moveLimitations = AI_DATA->moveLimitations[battlerAtk];
    u32 dmg, bestDmg = 0;
    u8 i, count = 0;

    if (CanAIFaintTarget(battlerAtk, battlerDef, 1))
    {
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            if (!IsMoveUnusable(i, moves[i], moveLimitations) && CanIndexMoveFaintTarget(battlerAtk, battlerDef, i, AI_ATTACKING))
                bestMoves[count++] = moves[i];
        }
        return;
    }

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (IsMoveUnusable(i, moves[i], moveLimitations) || gBattleMoves[moves[i]].power == 0)
            continue;
        dmg = AI_GetDamage(battlerAtk, battlerDef, i, calcContext);
        if (dmg == 0)
            continue;
        if (bestDmg < dmg)
        {
            count = 0;
            bestDmg = dmg;
            bestMoves[count++] = moves[i];
        }
        else if (bestDmg == dmg)
        {
            bestMoves[count++] = moves[i];
        }
    }
    for (i = count; i < MAX_MON_MOVES; i++)
        bestMoves[i] = MOVE_NONE;
}

bool32 IsBestDmgMove(u8 battlerAtk, u8 battlerDef, u8 calcContext, u16 move)
{
    u16 bestMoves[MAX_MON_MOVES] = {MOVE_NONE};
    u8 index = GetMoveIndex(battlerAtk, move);
    u8 i;

    if (index < MAX_MON_MOVES && CanIndexMoveFaintTarget(battlerAtk, battlerDef, index, AI_ATTACKING))
        return TRUE;

    GetBestDmgMovesFromBattler(battlerAtk, battlerDef, calcContext, bestMoves);
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (bestMoves[i] == move)
            return TRUE;
    }
    return FALSE;
}

bool32 BestDmgMoveHasEffect(u8 battlerAtk, u8 battlerDef, u8 calcContext, u8 moveEffect)
{
    u16 bestMoves[MAX_MON_MOVES] = {MOVE_NONE};
    u8 i;

    GetBestDmgMovesFromBattler(battlerAtk, battlerDef, calcContext, bestMoves);
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (bestMoves[i] == MOVE_NONE)
            break;
        if (gBattleMoves[bestMoves[i]].effect == moveEffect)
            return TRUE;
    }
    return FALSE;
}

bool32 HasPhysicalBestMove(u8 battlerAtk, u8 battlerDef, u8 calcContext)
{
    u16 bestMoves[MAX_MON_MOVES] = {MOVE_NONE};
    u8 i;

    GetBestDmgMovesFromBattler(battlerAtk, battlerDef, calcContext, bestMoves);
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (bestMoves[i] == MOVE_NONE)
            break;
        if (IsBattleMovePhysical(bestMoves[i], battlerAtk))
            return TRUE;
    }
    return FALSE;
}

static u32 GetFractionOfMaxHp(u8 battler, u32 divisor)
{
    u32 damage = gBattleMons[battler].maxHP / divisor;

    return damage == 0 ? 1 : damage;
}

static bool32 DoesBattlerTakeSandstormDamage(u8 battler, u8 ability)
{
    if (!(AI_GetWeather() & B_WEATHER_SANDSTORM))
        return FALSE;
    if (IS_BATTLER_OF_TYPE(battler, TYPE_ROCK) || IS_BATTLER_OF_TYPE(battler, TYPE_GROUND) || IS_BATTLER_OF_TYPE(battler, TYPE_STEEL))
        return FALSE;
    return ability != ABILITY_SAND_VEIL;
}

static bool32 DoesBattlerTakeHailDamage(u8 battler, u8 ability)
{
    if (!(AI_GetWeather() & B_WEATHER_HAIL))
        return FALSE;
    return !IS_BATTLER_OF_TYPE(battler, TYPE_ICE);
}

// Damage the battler takes at the end of the turn (Gen 3 amounts).
u32 GetBattlerSecondaryDamage(u8 battler)
{
    u8 ability = AI_DATA->abilities[battler];
    u32 damage = 0, status1Temp;

    if (gStatuses3[battler] & STATUS3_LEECHSEED)
        damage += GetFractionOfMaxHp(battler, 8);
    if ((gBattleMons[battler].status2 & STATUS2_NIGHTMARE) && (gBattleMons[battler].status1 & STATUS1_SLEEP))
        damage += GetFractionOfMaxHp(battler, 4);
    if (gBattleMons[battler].status2 & STATUS2_CURSED)
        damage += GetFractionOfMaxHp(battler, 4);
    if (gBattleMons[battler].status2 & STATUS2_WRAPPED)
        damage += GetFractionOfMaxHp(battler, 16);
    if (gBattleMons[battler].status1 & STATUS1_POISON)
    {
        damage += GetFractionOfMaxHp(battler, 8);
    }
    else if (gBattleMons[battler].status1 & STATUS1_TOXIC_POISON)
    {
        status1Temp = gBattleMons[battler].status1;
        if ((status1Temp & STATUS1_TOXIC_COUNTER) != STATUS1_TOXIC_TURN(15))
            status1Temp += STATUS1_TOXIC_TURN(1);
        damage += GetFractionOfMaxHp(battler, 16) * ((status1Temp & STATUS1_TOXIC_COUNTER) >> 8);
    }
    if (!(gStatuses3[battler] & (STATUS3_UNDERGROUND | STATUS3_UNDERWATER))
     && (DoesBattlerTakeSandstormDamage(battler, ability) || DoesBattlerTakeHailDamage(battler, ability)))
        damage += GetFractionOfMaxHp(battler, 16);

    return damage;
}

bool32 BattlerWillFaintFromWeather(u8 battler, u8 ability)
{
    if ((DoesBattlerTakeSandstormDamage(battler, ability) || DoesBattlerTakeHailDamage(battler, ability))
     && gBattleMons[battler].hp <= GetFractionOfMaxHp(battler, 16))
        return TRUE;
    return FALSE;
}

bool32 BattlerWillFaintFromSecondaryDamage(u8 battler, u8 ability)
{
    if (GetBattlerSecondaryDamage(battler) != 0
     && gBattleMons[battler].hp <= GetFractionOfMaxHp(battler, 16))
        return TRUE;
    return FALSE;
}

bool32 ShouldConsiderSelfSacrificeDamageEffect(u8 battlerAtk, u8 battlerDef, u16 move, bool32 aiIsFaster)
{
    if (!IsDoubleBattle() && IsExplosionMove(move) && AI_DATA->shouldConsiderExplosion)
        return TRUE;
    return FALSE;
}

u32 GetAIExplosionChanceFromHP(u32 hpPercent)
{
    if (hpPercent >= EXPLOSION_HIGHER_HP_THRESHOLD)
        return EXPLOSION_MINIMUM_CHANCE;
    if (hpPercent <= EXPLOSION_LOWER_HP_THRESHOLD)
        return EXPLOSION_MAXIMUM_CHANCE;
    return EXPLOSION_HIGHER_HP_THRESHOLD - hpPercent;
}

static bool32 IsSecondaryEffectBlocked(u8 battlerDef)
{
    return AI_DATA->abilities[battlerDef] == ABILITY_SHIELD_DUST;
}

static bool32 IsStatDropEffect(u8 moveEffect)
{
    return (moveEffect >= MOVE_EFFECT_ATK_MINUS_1 && moveEffect <= MOVE_EFFECT_EVS_MINUS_1)
        || (moveEffect >= MOVE_EFFECT_ATK_MINUS_2 && moveEffect <= MOVE_EFFECT_EVS_MINUS_2)
        || moveEffect == MOVE_EFFECT_ATK_DEF_DOWN
        || moveEffect == MOVE_EFFECT_SP_ATK_TWO_DOWN;
}

static bool32 IsStatRiseEffect(u8 moveEffect)
{
    return (moveEffect >= MOVE_EFFECT_ATK_PLUS_1 && moveEffect <= MOVE_EFFECT_EVS_PLUS_1)
        || (moveEffect >= MOVE_EFFECT_ATK_PLUS_2 && moveEffect <= MOVE_EFFECT_EVS_PLUS_2)
        || moveEffect == MOVE_EFFECT_ALL_STATS_UP;
}

// Does the move have a side effect that helps the AI (for moves with equal hits to KO)?
static bool32 AI_IsMoveEffectInPlus(u8 battlerAtk, u8 battlerDef, u16 move, s32 noOfHitsToKo)
{
    u8 abilityAtk = AI_DATA->abilities[battlerAtk];
    u8 abilityDef = AI_DATA->abilities[battlerDef];
    bool32 aiIsFaster = AI_IsFaster(battlerAtk, battlerDef, move, MOVE_NONE, CONSIDER_PRIORITY);
    struct AiSecondaryEffect secondary;
    u8 stat;

    switch (gBattleMoves[move].effect)
    {
    case EFFECT_ABSORB:
    case EFFECT_DREAM_EATER:
        if (!AI_BattlerAtMaxHp(battlerAtk) || !aiIsFaster)
            return TRUE;
        break;
    case EFFECT_PURSUIT:
        if (noOfHitsToKo == 1)
            return TRUE;
        break;
    default:
        break;
    }

    secondary = GetMoveSecondaryEffect(move);
    if (secondary.moveEffect == 0)
        return FALSE;

    if (secondary.self)
    {
        if (secondary.moveEffect == MOVE_EFFECT_ALL_STATS_UP)
        {
            for (stat = STAT_ATK; stat < NUM_BATTLE_STATS; stat++)
            {
                if (BattlerStatCanRise(battlerAtk, abilityAtk, stat))
                    return TRUE;
            }
        }
        else if (secondary.moveEffect >= MOVE_EFFECT_ATK_PLUS_1 && secondary.moveEffect <= MOVE_EFFECT_EVS_PLUS_1)
        {
            if (BattlerStatCanRise(battlerAtk, abilityAtk, STAT_ATK + secondary.moveEffect - MOVE_EFFECT_ATK_PLUS_1))
                return TRUE;
        }
        return FALSE;
    }

    if (IsSecondaryEffectBlocked(battlerDef))
        return FALSE;

    switch (secondary.moveEffect)
    {
    case MOVE_EFFECT_POISON:
    case MOVE_EFFECT_TOXIC:
        if (AI_CanPoison(battlerAtk, battlerDef, abilityDef, move))
            return TRUE;
        break;
    case MOVE_EFFECT_BURN:
        if (AI_CanBurn(battlerAtk, battlerDef, abilityDef, move))
            return TRUE;
        break;
    case MOVE_EFFECT_FREEZE:
        if (AI_CanBeFrozen(battlerDef, abilityDef))
            return TRUE;
        break;
    case MOVE_EFFECT_PARALYSIS:
        if (AI_CanParalyze(battlerAtk, battlerDef, abilityDef, move))
            return TRUE;
        break;
    case MOVE_EFFECT_CONFUSION:
        if (AI_CanConfuse(battlerAtk, battlerDef, abilityDef, move))
            return TRUE;
        break;
    case MOVE_EFFECT_FLINCH:
        if (ShouldTryToFlinch(battlerAtk, battlerDef, abilityAtk, abilityDef, move))
            return TRUE;
        break;
    default:
        if (secondary.moveEffect >= MOVE_EFFECT_ATK_MINUS_1 && secondary.moveEffect <= MOVE_EFFECT_EVS_MINUS_1)
        {
            if (CanLowerStat(battlerAtk, battlerDef, STAT_ATK + secondary.moveEffect - MOVE_EFFECT_ATK_MINUS_1) && noOfHitsToKo > 1)
                return TRUE;
        }
        break;
    }
    return FALSE;
}

// Does the move have a side effect that hurts the AI?
static bool32 AI_IsMoveEffectInMinus(u8 battlerAtk, u8 battlerDef, u16 move, s32 noOfHitsToKo)
{
    u8 abilityAtk = AI_DATA->abilities[battlerAtk];
    u8 abilityDef = AI_DATA->abilities[battlerDef];
    struct AiSecondaryEffect secondary;

    if (IsExplosionMove(move))
        return TRUE;

    switch (gBattleMoves[move].effect)
    {
    case EFFECT_RECOIL:
    case EFFECT_DOUBLE_EDGE:
    case EFFECT_RECOIL_IF_MISS:
        if (abilityAtk != ABILITY_ROCK_HEAD)
            return TRUE;
        break;
    case EFFECT_ABSORB:
        if (abilityDef == ABILITY_LIQUID_OOZE)
            return TRUE;
        break;
    default:
        secondary = GetMoveSecondaryEffect(move);
        if (secondary.moveEffect == MOVE_EFFECT_RECHARGE)
            return TRUE;
        if (secondary.self && IsStatDropEffect(secondary.moveEffect))
            return TRUE;
        // Raising the target's stats is bad, unless the hit KOs it anyway.
        if (!secondary.self && IsStatRiseEffect(secondary.moveEffect) && noOfHitsToKo > 1)
            return TRUE;
        break;
    }
    return FALSE;
}

// For two damaging moves with the same number of hits to KO, which side
// effects are better?
enum MoveComparisonResult CompareMoveEffects(u16 move1, u16 move2, u8 battlerAtk, u8 battlerDef, s32 noOfHitsToKo)
{
    bool32 effect1minus, effect1plus, effect2minus, effect2plus;
    u8 savedIndex = AI_THINKING_STRUCT->movesetIndex;

    // Rough Skin hurts on contact.
    if (AI_DATA->abilities[battlerDef] == ABILITY_ROUGH_SKIN)
    {
        bool32 moveContact1 = MoveMakesContact(move1);
        bool32 moveContact2 = MoveMakesContact(move2);

        if (moveContact1 && !moveContact2)
            return MOVE_LOST_COMPARISON;
        if (moveContact2 && !moveContact1)
            return MOVE_WON_COMPARISON;
    }

    AI_THINKING_STRUCT->movesetIndex = GetMoveIndex(battlerAtk, move1);
    effect1minus = AI_IsMoveEffectInMinus(battlerAtk, battlerDef, move1, noOfHitsToKo);
    effect1plus = AI_IsMoveEffectInPlus(battlerAtk, battlerDef, move1, noOfHitsToKo);

    AI_THINKING_STRUCT->movesetIndex = GetMoveIndex(battlerAtk, move2);
    effect2plus = AI_IsMoveEffectInPlus(battlerAtk, battlerDef, move2, noOfHitsToKo);
    effect2minus = AI_IsMoveEffectInMinus(battlerAtk, battlerDef, move2, noOfHitsToKo);

    AI_THINKING_STRUCT->movesetIndex = savedIndex;

    if (effect2minus && !effect1minus)
        return MOVE_WON_COMPARISON;
    if (effect1minus && !effect2minus)
        return MOVE_LOST_COMPARISON;
    if (effect2plus && !effect1plus)
        return MOVE_LOST_COMPARISON;
    if (effect1plus && !effect2plus)
        return MOVE_WON_COMPARISON;
    return MOVE_NEUTRAL_COMPARISON;
}

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------

bool32 BattlerStatCanRise(u8 battler, u8 ability, u8 stat)
{
    return gBattleMons[battler].statStages[stat] < MAX_STAT_STAGE;
}

bool32 AreBattlersStatsMaxed(u8 battler)
{
    u8 stat;

    for (stat = STAT_ATK; stat < NUM_BATTLE_STATS; stat++)
    {
        if (gBattleMons[battler].statStages[stat] < MAX_STAT_STAGE)
            return FALSE;
    }
    return TRUE;
}

bool32 AnyStatIsRaised(u8 battler)
{
    u8 stat;

    for (stat = STAT_ATK; stat < NUM_BATTLE_STATS; stat++)
    {
        if (gBattleMons[battler].statStages[stat] > DEFAULT_STAT_STAGE)
            return TRUE;
    }
    return FALSE;
}

u32 CountPositiveStatStages(u8 battler)
{
    u32 count = 0;
    u8 stat;

    for (stat = STAT_ATK; stat < NUM_BATTLE_STATS; stat++)
    {
        if (gBattleMons[battler].statStages[stat] > DEFAULT_STAT_STAGE)
            count++;
    }
    return count;
}

u32 CountNegativeStatStages(u8 battler)
{
    u32 count = 0;
    u8 stat;

    for (stat = STAT_ATK; stat < NUM_BATTLE_STATS; stat++)
    {
        if (gBattleMons[battler].statStages[stat] < DEFAULT_STAT_STAGE)
            count++;
    }
    return count;
}

bool32 CanLowerStat(u8 battlerAtk, u8 battlerDef, u8 stat)
{
    u16 move = AI_THINKING_STRUCT->moveConsidered;

    if (gBattleMons[battlerDef].statStages[stat] == MIN_STAT_STAGE)
        return FALSE;
    if (gSideStatuses[GetBattlerSide(battlerDef)] & SIDE_STATUS_MIST)
        return FALSE;

    switch (AI_DATA->abilities[battlerDef])
    {
    case ABILITY_HYPER_CUTTER:
        if (stat == STAT_ATK)
            return FALSE;
        break;
    case ABILITY_KEEN_EYE:
        if (stat == STAT_ACC)
            return FALSE;
        break;
    case ABILITY_CLEAR_BODY:
    case ABILITY_WHITE_SMOKE:
        return FALSE;
    case ABILITY_SHIELD_DUST:
        if (!IsBattleMoveStatus(move))
            return FALSE;
        break;
    default:
        break;
    }

    // If the AI is faster and has no mons left, lowering speed doesn't help.
    if (stat == STAT_SPEED)
        return !(AI_IsFaster(battlerAtk, battlerDef, move, MOVE_NONE, DONT_CONSIDER_PRIORITY)
              && CountUsablePartyMons(battlerAtk) == 0);
    return TRUE;
}

// Does the target have a move that makes setting up pointless (priority, or
// lowering the AI's speed when the AI relies on moving first)?
static bool32 HasMoveThatChangesKOThreshold(u8 battler, u32 noOfHitsToFaint, bool32 aiIsFaster)
{
    u16 *moves = AI_GetMovesArray(battler);
    struct AiSecondaryEffect secondary;
    u8 i;

    if (noOfHitsToFaint > 2)
        return FALSE;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] == MOVE_NONE || moves[i] == MOVE_UNAVAILABLE)
            continue;
        if (gBattleMoves[moves[i]].priority > 0)
            return TRUE;
        secondary = GetMoveSecondaryEffect(moves[i]);
        if (aiIsFaster && !secondary.self && secondary.moveEffect == MOVE_EFFECT_SPD_MINUS_1)
            return TRUE;
    }
    return FALSE;
}

static bool32 ShouldRaiseAnyStat(u8 battlerAtk, u8 battlerDef)
{
    if (AreBattlersStatsMaxed(battlerAtk))
        return FALSE;
    // Don't raise stats when about to fall asleep from Yawn.
    if ((gStatuses3[battlerAtk] & STATUS3_YAWN) && AI_CanBeSlept(battlerAtk, AI_DATA->abilities[battlerAtk]))
        return FALSE;
    // Don't set up if the AI faints to residual damage.
    if (GetBattlerSecondaryDamage(battlerAtk) >= gBattleMons[battlerAtk].hp)
        return FALSE;
    // Don't raise stats into a target that used Haze.
    if (gBattleMoves[AI_DATA->lastUsedMove[battlerDef]].effect == EFFECT_HAZE)
        return FALSE;
    // Don't keep raising stats if the target can reset them.
    if (CountPositiveStatStages(battlerAtk) > 0
     && (HasMoveWithEffect(battlerDef, EFFECT_HAZE) || HasMoveWithEffect(battlerDef, EFFECT_ROAR)))
        return FALSE;
    return TRUE;
}

static u8 GetStatBeingChanged(u8 statChange)
{
    switch (statChange)
    {
    case STAT_CHANGE_ATK:
    case STAT_CHANGE_ATK_2:
    case STAT_CHANGE_ATK_MAX:
        return STAT_ATK;
    case STAT_CHANGE_DEF:
    case STAT_CHANGE_DEF_2:
        return STAT_DEF;
    case STAT_CHANGE_SPEED:
    case STAT_CHANGE_SPEED_2:
        return STAT_SPEED;
    case STAT_CHANGE_SPATK:
    case STAT_CHANGE_SPATK_2:
        return STAT_SPATK;
    case STAT_CHANGE_SPDEF:
    case STAT_CHANGE_SPDEF_2:
        return STAT_SPDEF;
    case STAT_CHANGE_ACC:
        return STAT_ACC;
    case STAT_CHANGE_EVASION:
        return STAT_EVASION;
    }
    return 0;
}

static u32 GetStagesOfStatChange(u8 statChange)
{
    switch (statChange)
    {
    case STAT_CHANGE_ATK_2:
    case STAT_CHANGE_DEF_2:
    case STAT_CHANGE_SPEED_2:
    case STAT_CHANGE_SPATK_2:
    case STAT_CHANGE_SPDEF_2:
        return 2;
    case STAT_CHANGE_ATK_MAX:
        return 6;
    default:
        return 1;
    }
}

// How useful raising this stat is now.
enum AIScore IncreaseStatUpScore(u8 battlerAtk, u8 battlerDef, u8 statChange)
{
    enum AIScore tempScore = NO_INCREASE;
    u32 noOfHitsToFaint = NoOfHitsForTargetToFaintBattler(battlerDef, battlerAtk);
    bool32 aiIsFaster = AI_IsFaster(battlerAtk, battlerDef, MOVE_NONE, MOVE_NONE, DONT_CONSIDER_PRIORITY);
    bool32 shouldSetUp = ((noOfHitsToFaint >= 2 && aiIsFaster) || (noOfHitsToFaint >= 3 && !aiIsFaster) || noOfHitsToFaint == UNKNOWN_NO_OF_HITS);
    u8 stat = GetStatBeingChanged(statChange);
    u32 stages = GetStagesOfStatChange(statChange);

    if (!ShouldRaiseAnyStat(battlerAtk, battlerDef))
        return NO_INCREASE;
    // Don't raise a stat that is already at +4 or more.
    if (gBattleMons[battlerAtk].statStages[stat] >= MAX_STAT_STAGE - 2)
        return NO_INCREASE;
    // Don't raise stats below 70% HP when the AI doesn't know how hard it gets hit.
    if (AI_DATA->hpPercents[battlerAtk] < 70 && noOfHitsToFaint == UNKNOWN_NO_OF_HITS)
        return NO_INCREASE;
    if (HasMoveThatChangesKOThreshold(battlerDef, noOfHitsToFaint, aiIsFaster))
        return NO_INCREASE;

    switch (stat)
    {
    case STAT_ATK:
        if (HasMoveWithCategory(battlerAtk, DAMAGE_CATEGORY_PHYSICAL) && shouldSetUp)
        {
            if (stages == 1)
                tempScore += DECENT_EFFECT;
            else if (stages == 6)
                tempScore += BEST_EFFECT;
            else
                tempScore += GOOD_EFFECT;
        }
        break;
    case STAT_DEF:
        if (HasMoveWithCategory(battlerDef, DAMAGE_CATEGORY_PHYSICAL) || !HasMoveWithCategory(battlerDef, DAMAGE_CATEGORY_SPECIAL))
        {
            if (stages == 1)
                tempScore += WEAK_EFFECT;
            else
                tempScore += DECENT_EFFECT;
        }
        break;
    case STAT_SPEED:
        if ((noOfHitsToFaint >= 3 && !aiIsFaster) || noOfHitsToFaint == UNKNOWN_NO_OF_HITS)
        {
            if (stages == 1)
                tempScore += DECENT_EFFECT;
            else
                tempScore += GOOD_EFFECT;
        }
        break;
    case STAT_SPATK:
        if (HasMoveWithCategory(battlerAtk, DAMAGE_CATEGORY_SPECIAL) && shouldSetUp)
        {
            if (stages == 1)
                tempScore += DECENT_EFFECT;
            else
                tempScore += GOOD_EFFECT;
        }
        break;
    case STAT_SPDEF:
        if (HasMoveWithCategory(battlerDef, DAMAGE_CATEGORY_SPECIAL) || !HasMoveWithCategory(battlerDef, DAMAGE_CATEGORY_PHYSICAL))
        {
            if (stages == 1)
                tempScore += WEAK_EFFECT;
            else
                tempScore += DECENT_EFFECT;
        }
        break;
    case STAT_ACC:
        if (gBattleMons[battlerAtk].statStages[stat] <= DEFAULT_STAT_STAGE - 3)
            tempScore += DECENT_EFFECT;
        break;
    case STAT_EVASION:
        if (noOfHitsToFaint > 3 || noOfHitsToFaint == UNKNOWN_NO_OF_HITS)
            tempScore += GOOD_EFFECT;
        else
            tempScore += DECENT_EFFECT;
        break;
    }
    return tempScore;
}

// How useful lowering the target's stat is now.
enum AIScore IncreaseStatDownScore(u8 battlerAtk, u8 battlerDef, u8 stat)
{
    s32 tempScore = NO_INCREASE;

    // Don't lower a stat that is already at -3 or less.
    if (stat != STAT_SPEED && gBattleMons[battlerDef].statStages[stat] <= DEFAULT_STAT_STAGE - 3)
        return NO_INCREASE;
    // Don't bother if the target faints to residual damage.
    if (GetBattlerSecondaryDamage(battlerDef) >= gBattleMons[battlerDef].hp)
        return NO_INCREASE;

    switch (stat)
    {
    case STAT_ATK:
        if (HasMoveWithCategory(battlerDef, DAMAGE_CATEGORY_PHYSICAL))
            tempScore += DECENT_EFFECT;
        break;
    case STAT_DEF:
        if (HasMoveWithCategory(battlerAtk, DAMAGE_CATEGORY_PHYSICAL)
         || HasMoveWithCategory(BATTLE_PARTNER(battlerAtk), DAMAGE_CATEGORY_PHYSICAL))
            tempScore += DECENT_EFFECT;
        break;
    case STAT_SPEED:
        if (AI_IsSlower(battlerAtk, battlerDef, MOVE_NONE, MOVE_NONE, DONT_CONSIDER_PRIORITY)
         || (HasPartner(battlerAtk) && AI_IsSlower(BATTLE_PARTNER(battlerAtk), battlerDef, MOVE_NONE, MOVE_NONE, DONT_CONSIDER_PRIORITY)))
            tempScore += DECENT_EFFECT;
        break;
    case STAT_SPATK:
        if (HasMoveWithCategory(battlerDef, DAMAGE_CATEGORY_SPECIAL))
            tempScore += DECENT_EFFECT;
        break;
    case STAT_SPDEF:
        if (HasMoveWithCategory(battlerAtk, DAMAGE_CATEGORY_SPECIAL)
         || HasMoveWithCategory(BATTLE_PARTNER(battlerAtk), DAMAGE_CATEGORY_SPECIAL))
            tempScore += DECENT_EFFECT;
        break;
    case STAT_ACC:
        tempScore += WEAK_EFFECT;
        if (IsBattlerTrapped(battlerAtk, battlerDef))
            tempScore += DECENT_EFFECT;
        if (gStatuses3[battlerDef] & STATUS3_LEECHSEED)
            tempScore += WEAK_EFFECT;
        if (gBattleMons[battlerDef].status2 & STATUS2_CURSED)
            tempScore += WEAK_EFFECT;
        break;
    case STAT_EVASION:
        if (gBattleMons[battlerDef].status1 & STATUS1_PSN_ANY)
            tempScore += WEAK_EFFECT;
        if (gStatuses3[battlerDef] & STATUS3_LEECHSEED)
            tempScore += WEAK_EFFECT;
        if (gStatuses3[battlerDef] & STATUS3_ROOTED)
            tempScore += WEAK_EFFECT;
        if (gBattleMons[battlerDef].status2 & STATUS2_CURSED)
            tempScore += WEAK_EFFECT;
        break;
    }
    return (tempScore > BEST_EFFECT) ? BEST_EFFECT : tempScore;
}

s32 AI_TryToClearStats(u8 battlerAtk, u8 battlerDef, bool32 isDoubleBattle)
{
    u32 count;

    if (isDoubleBattle)
    {
        count = CountPositiveStatStages(battlerDef) + CountPositiveStatStages(BATTLE_PARTNER(battlerDef));
        return min(count, 7);
    }
    count = CountPositiveStatStages(battlerDef);
    return min(count, 4);
}

bool32 AI_ShouldCopyStatChanges(u8 battlerAtk, u8 battlerDef)
{
    u8 stat;

    // Copy only stat changes that help.
    for (stat = STAT_ATK; stat < NUM_BATTLE_STATS; stat++)
    {
        if (gBattleMons[battlerDef].statStages[stat] > gBattleMons[battlerAtk].statStages[stat])
        {
            switch (stat)
            {
            case STAT_ATK:
                return HasMoveWithCategory(battlerAtk, DAMAGE_CATEGORY_PHYSICAL);
            case STAT_SPATK:
                return HasMoveWithCategory(battlerAtk, DAMAGE_CATEGORY_SPECIAL);
            case STAT_ACC:
                return HasMoveWithLowAccuracy(battlerAtk, battlerDef, LOW_ACCURACY_THRESHOLD, FALSE);
            case STAT_EVASION:
            case STAT_SPEED:
                return TRUE;
            case STAT_DEF:
            case STAT_SPDEF:
                return FALSE;
            }
        }
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Status conditions (Gen 3 rules)
// ---------------------------------------------------------------------------

bool32 DoesSubstituteBlockMove(u8 battlerAtk, u8 battlerDef, u16 move)
{
    if (!(gBattleMons[battlerDef].status2 & STATUS2_SUBSTITUTE) || battlerAtk == battlerDef)
        return FALSE;
    return !(gBattleMoves[move].target & MOVE_TARGET_USER);
}

static bool32 IsUproarOnField(void)
{
    u8 i;

    for (i = 0; i < gBattlersCount; i++)
    {
        if (IsBattlerAlive(i) && (gBattleMons[i].status2 & STATUS2_UPROAR))
            return TRUE;
    }
    return FALSE;
}

bool32 AI_CanBeSlept(u8 battler, u8 ability)
{
    if (gBattleMons[battler].status1 & STATUS1_ANY)
        return FALSE;
    if (ability == ABILITY_INSOMNIA || ability == ABILITY_VITAL_SPIRIT)
        return FALSE;
    if (gSideStatuses[GetBattlerSide(battler)] & SIDE_STATUS_SAFEGUARD)
        return FALSE;
    if (IsUproarOnField())
        return FALSE;
    return TRUE;
}

bool32 AI_CanBePoisoned(u8 battlerDef, u8 defAbility)
{
    if (gBattleMons[battlerDef].status1 & STATUS1_ANY)
        return FALSE;
    if (IS_BATTLER_OF_TYPE(battlerDef, TYPE_POISON) || IS_BATTLER_OF_TYPE(battlerDef, TYPE_STEEL))
        return FALSE;
    if (defAbility == ABILITY_IMMUNITY)
        return FALSE;
    if (gSideStatuses[GetBattlerSide(battlerDef)] & SIDE_STATUS_SAFEGUARD)
        return FALSE;
    return TRUE;
}

bool32 AI_CanBeBurned(u8 battlerDef, u8 defAbility)
{
    if (gBattleMons[battlerDef].status1 & STATUS1_ANY)
        return FALSE;
    if (IS_BATTLER_OF_TYPE(battlerDef, TYPE_FIRE) || defAbility == ABILITY_WATER_VEIL)
        return FALSE;
    if (gSideStatuses[GetBattlerSide(battlerDef)] & SIDE_STATUS_SAFEGUARD)
        return FALSE;
    return TRUE;
}

bool32 AI_CanBeParalyzed(u8 battlerDef, u8 defAbility)
{
    if (gBattleMons[battlerDef].status1 & STATUS1_ANY)
        return FALSE;
    if (defAbility == ABILITY_LIMBER)
        return FALSE;
    if (gSideStatuses[GetBattlerSide(battlerDef)] & SIDE_STATUS_SAFEGUARD)
        return FALSE;
    return TRUE;
}

bool32 AI_CanBeFrozen(u8 battlerDef, u8 defAbility)
{
    if (gBattleMons[battlerDef].status1 & STATUS1_ANY)
        return FALSE;
    if (IS_BATTLER_OF_TYPE(battlerDef, TYPE_ICE) || defAbility == ABILITY_MAGMA_ARMOR)
        return FALSE;
    if (AI_GetWeather() & B_WEATHER_SUN)
        return FALSE;
    if (gSideStatuses[GetBattlerSide(battlerDef)] & SIDE_STATUS_SAFEGUARD)
        return FALSE;
    return TRUE;
}

static bool32 IsConsideredMoveImmune(u8 battlerAtk, u8 battlerDef)
{
    return AI_DATA->effectiveness[battlerAtk][battlerDef][AI_THINKING_STRUCT->movesetIndex] == AI_EFFECTIVENESS_0;
}

bool32 AI_CanPutToSleep(u8 battlerAtk, u8 battlerDef, u8 defAbility, u16 move)
{
    if (!AI_CanBeSlept(battlerDef, defAbility) || DoesSubstituteBlockMove(battlerAtk, battlerDef, move))
        return FALSE;
    return TRUE;
}

bool32 AI_CanPoison(u8 battlerAtk, u8 battlerDef, u8 defAbility, u16 move)
{
    if (!AI_CanBePoisoned(battlerDef, defAbility)
     || IsConsideredMoveImmune(battlerAtk, battlerDef)
     || DoesSubstituteBlockMove(battlerAtk, battlerDef, move))
        return FALSE;
    return TRUE;
}

bool32 AI_CanParalyze(u8 battlerAtk, u8 battlerDef, u8 defAbility, u16 move)
{
    if (!AI_CanBeParalyzed(battlerDef, defAbility)
     || IsConsideredMoveImmune(battlerAtk, battlerDef)
     || DoesSubstituteBlockMove(battlerAtk, battlerDef, move))
        return FALSE;
    return TRUE;
}

bool32 AI_CanBurn(u8 battlerAtk, u8 battlerDef, u8 defAbility, u16 move)
{
    if (!AI_CanBeBurned(battlerDef, defAbility)
     || IsConsideredMoveImmune(battlerAtk, battlerDef)
     || DoesSubstituteBlockMove(battlerAtk, battlerDef, move))
        return FALSE;
    return TRUE;
}

static bool32 AI_CanBeConfused(u8 battlerAtk, u8 battlerDef, u16 move, u8 defAbility)
{
    if ((gBattleMons[battlerDef].status2 & STATUS2_CONFUSION)
     || defAbility == ABILITY_OWN_TEMPO
     || (gSideStatuses[GetBattlerSide(battlerDef)] & SIDE_STATUS_SAFEGUARD)
     || DoesSubstituteBlockMove(battlerAtk, battlerDef, move))
        return FALSE;
    return TRUE;
}

bool32 AI_CanConfuse(u8 battlerAtk, u8 battlerDef, u8 defAbility, u16 move)
{
    // Teeter Dance also hits the partner: don't use it if only the partner gets confused.
    if ((gBattleMoves[move].target & MOVE_TARGET_FOES_AND_ALLY)
     && AI_CanBeConfused(battlerAtk, battlerDef, move, defAbility)
     && HasPartner(battlerAtk)
     && !AI_CanBeConfused(battlerAtk, BATTLE_PARTNER(battlerDef), move, AI_DATA->abilities[BATTLE_PARTNER(battlerDef)]))
        return FALSE;
    return AI_CanBeConfused(battlerAtk, battlerDef, move, defAbility);
}

bool32 AI_CanBeInfatuated(u8 battlerAtk, u8 battlerDef, u8 defAbility)
{
    if ((gBattleMons[battlerDef].status2 & STATUS2_INFATUATION)
     || IsConsideredMoveImmune(battlerAtk, battlerDef)
     || defAbility == ABILITY_OBLIVIOUS
     || !AreBattlersOfOppositeGender(battlerAtk, battlerDef))
        return FALSE;
    return TRUE;
}

// Marvel Scale, Guts and Facade make a status condition a benefit.
static bool32 DoesBattlerBenefitFromAllVolatileStatus(u8 battler, u8 ability)
{
    if (ability == ABILITY_MARVEL_SCALE
     || (ability == ABILITY_GUTS && HasMoveWithCategory(battler, DAMAGE_CATEGORY_PHYSICAL))
     || HasMoveWithEffect(battler, EFFECT_FACADE))
        return TRUE;
    return FALSE;
}

bool32 ShouldPoison(u8 battlerAtk, u8 battlerDef)
{
    u8 abilityDef = AI_DATA->abilities[battlerDef];

    if (AI_CanBePoisoned(battlerDef, abilityDef) && DoesBattlerBenefitFromAllVolatileStatus(battlerDef, abilityDef))
        return battlerAtk == battlerDef;
    return battlerAtk != battlerDef;
}

bool32 ShouldBurn(u8 battlerAtk, u8 battlerDef, u8 abilityDef)
{
    if (AI_CanBeBurned(battlerDef, abilityDef) && DoesBattlerBenefitFromAllVolatileStatus(battlerDef, abilityDef))
        return battlerAtk == battlerDef;
    return battlerAtk != battlerDef;
}

bool32 ShouldParalyze(u8 battlerAtk, u8 battlerDef, u8 abilityDef)
{
    if (AI_CanBeParalyzed(battlerDef, abilityDef) && DoesBattlerBenefitFromAllVolatileStatus(battlerDef, abilityDef))
        return battlerAtk == battlerDef;
    return battlerAtk != battlerDef;
}

// Is curing battlerDef's status condition good for the AI?
bool32 ShouldCureStatus(u8 battlerAtk, u8 battlerDef)
{
    bool32 targetingSelf = (battlerAtk == battlerDef);
    bool32 targetingAlly = IsTargetingPartner(battlerAtk, battlerDef);
    u32 status = gBattleMons[battlerDef].status1;
    bool32 isHarmless;

    if (status & STATUS1_SLEEP)
    {
        if ((targetingAlly || targetingSelf) && !HasUsableWhileAsleepMove(battlerDef))
            return targetingAlly;
        return FALSE;
    }
    if (status & STATUS1_FREEZE)
    {
        if ((targetingAlly || targetingSelf) && !HasThawingMove(battlerDef))
            return targetingAlly;
        return FALSE;
    }

    isHarmless = DoesBattlerBenefitFromAllVolatileStatus(battlerDef, AI_DATA->abilities[battlerDef]);
    if (isHarmless)
        return !(targetingSelf || targetingAlly);
    return targetingSelf || targetingAlly;
}

// Does the AI's side have a status condition worth curing with Heal Bell or
// Aromatherapy? In Gen 3, Heal Bell doesn't cure a Soundproof Pokémon.
bool32 AnyPartyMemberStatused(u8 battler, bool32 checkSoundproof)
{
    struct Pokemon *party = AI_GetBattlerParty(battler);
    u8 onField1, onField2;
    bool32 hasStatusToCure = FALSE;
    s32 i;

    onField1 = gBattlerPartyIndexes[battler];
    if (HasPartner(battler))
    {
        onField2 = gBattlerPartyIndexes[BATTLE_PARTNER(battler)];
        if ((AI_DATA->abilities[BATTLE_PARTNER(battler)] != ABILITY_SOUNDPROOF || !checkSoundproof)
         && gBattleMons[BATTLE_PARTNER(battler)].status1 != 0
         && ShouldCureStatus(battler, BATTLE_PARTNER(battler)))
            hasStatusToCure = TRUE;
    }
    else
    {
        onField2 = onField1;
    }

    if ((AI_DATA->abilities[battler] != ABILITY_SOUNDPROOF || !checkSoundproof)
     && gBattleMons[battler].status1 != 0
     && ShouldCureStatus(battler, battler))
        hasStatusToCure = TRUE;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (i == onField1 || i == onField2)
            continue;
        if (GetMonData(&party[i], MON_DATA_HP) == 0 || GetMonData(&party[i], MON_DATA_SPECIES_OR_EGG) == SPECIES_NONE)
            continue;
        if (checkSoundproof && GetMonAbility(&party[i]) == ABILITY_SOUNDPROOF)
            continue;
        if (GetMonData(&party[i], MON_DATA_STATUS) != 0)
            return TRUE;
    }
    return hasStatusToCure;
}

bool32 IsBattlerIncapacitated(u8 battler, u8 ability)
{
    if ((gBattleMons[battler].status1 & STATUS1_FREEZE) && !HasThawingMove(battler))
        return TRUE; // with a thawing move it will surely use it
    if ((gBattleMons[battler].status1 & STATUS1_SLEEP) && !HasMoveWithEffect(battler, EFFECT_SLEEP_TALK))
        return TRUE;
    if ((gBattleMons[battler].status2 & STATUS2_RECHARGE) || (ability == ABILITY_TRUANT && gDisableStructs[battler].truantCounter != 0))
        return TRUE;
    return FALSE;
}

bool32 AI_IsBattlerAsleep(u8 battler)
{
    return (gBattleMons[battler].status1 & STATUS1_SLEEP) != 0;
}

bool32 IsWakeupTurn(u8 battler)
{
    u32 sleepTurns = gBattleMons[battler].status1 & STATUS1_SLEEP;

    if (sleepTurns == 0)
        return FALSE;
    // Early Bird lowers the sleep counter twice as fast.
    if (AI_DATA->abilities[battler] == ABILITY_EARLY_BIRD)
        return sleepTurns <= 2;
    return sleepTurns <= 1;
}

static bool32 AI_IsBattlerGrounded(u8 battler)
{
    return !IS_BATTLER_OF_TYPE(battler, TYPE_FLYING) && AI_DATA->abilities[battler] != ABILITY_LEVITATE;
}

// Can battlerDef not switch out? (Gen 3: no Shed Shell, ghosts can be trapped.)
bool32 IsBattlerTrapped(u8 battlerAtk, u8 battlerDef)
{
    if (gBattleMons[battlerDef].status2 & (STATUS2_WRAPPED | STATUS2_ESCAPE_PREVENTION))
        return TRUE;
    if (gStatuses3[battlerDef] & STATUS3_ROOTED)
        return TRUE;
    if (AI_IsAbilityOnSide(battlerAtk, ABILITY_SHADOW_TAG))
        return TRUE;
    if (AI_IsAbilityOnSide(battlerAtk, ABILITY_ARENA_TRAP) && AI_IsBattlerGrounded(battlerDef))
        return TRUE;
    if (AI_IsAbilityOnSide(battlerAtk, ABILITY_MAGNET_PULL) && IS_BATTLER_OF_TYPE(battlerDef, TYPE_STEEL))
        return TRUE;
    if ((gBattleTypeFlags & BATTLE_TYPE_TRAINER) && CountUsablePartyMons(battlerDef) == 0)
        return TRUE;
    return FALSE;
}

bool32 HasChoiceEffect(u8 battler)
{
    return AI_DATA->holdEffects[battler] == HOLD_EFFECT_CHOICE_BAND;
}

void IncreasePoisonScore(u8 battlerAtk, u8 battlerDef, u16 move, s32 *score)
{
    u8 holdEffectDef = AI_DATA->holdEffects[battlerDef];

    if (((AI_THINKING_STRUCT->aiLogicFlags & AI_LOGIC_TRY_TO_FAINT) && CanAIFaintTarget(battlerAtk, battlerDef, 0))
     || holdEffectDef == HOLD_EFFECT_CURE_PSN || holdEffectDef == HOLD_EFFECT_CURE_STATUS)
        return;

    if (AI_CanPoison(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], move) && AI_DATA->hpPercents[battlerDef] > 20)
    {
        if (!HasDamagingMove(battlerDef))
            *score += DECENT_EFFECT;
        *score += WEAK_EFFECT;
    }
}

void IncreaseBurnScore(u8 battlerAtk, u8 battlerDef, u16 move, s32 *score)
{
    u16 defBestMoves[MAX_MON_MOVES] = {MOVE_NONE};
    u8 holdEffectDef = AI_DATA->holdEffects[battlerDef];
    bool32 hasPhysical = FALSE;
    u8 i;

    if (((AI_THINKING_STRUCT->aiLogicFlags & AI_LOGIC_TRY_TO_FAINT) && CanAIFaintTarget(battlerAtk, battlerDef, 0))
     || holdEffectDef == HOLD_EFFECT_CURE_BRN || holdEffectDef == HOLD_EFFECT_CURE_STATUS)
        return;

    if (AI_CanBurn(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], move)
     && HasMoveWithCategory(battlerDef, DAMAGE_CATEGORY_PHYSICAL))
    {
        GetBestDmgMovesFromBattler(battlerDef, battlerAtk, AI_DEFENDING, defBestMoves);
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            if (defBestMoves[i] == MOVE_NONE)
                break;
            if (IsBattleMovePhysical(defBestMoves[i], battlerDef))
            {
                hasPhysical = TRUE;
                break;
            }
        }
        if (hasPhysical)
            *score += DECENT_EFFECT;
        else
            *score += WEAK_EFFECT;
    }
}

void IncreaseParalyzeScore(u8 battlerAtk, u8 battlerDef, u16 move, s32 *score)
{
    u8 holdEffectDef = AI_DATA->holdEffects[battlerDef];
    u32 atkSpeed, defSpeed;

    if (((AI_THINKING_STRUCT->aiLogicFlags & AI_LOGIC_TRY_TO_FAINT) && CanAIFaintTarget(battlerAtk, battlerDef, 0))
     || holdEffectDef == HOLD_EFFECT_CURE_PAR || holdEffectDef == HOLD_EFFECT_CURE_STATUS)
        return;

    if (AI_CanParalyze(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], move))
    {
        atkSpeed = AI_DATA->speedStats[battlerAtk];
        defSpeed = AI_DATA->speedStats[battlerDef];
        // Gen 3 paralysis divides speed by 4.
        if ((defSpeed >= atkSpeed && defSpeed / 4 < atkSpeed) // the AI moves first after paralyzing
         || HasMoveWithFlinchEffect(battlerAtk)
         || (gBattleMons[battlerDef].status2 & (STATUS2_INFATUATION | STATUS2_CONFUSION)))
            *score += GOOD_EFFECT;
        else
            *score += DECENT_EFFECT;
    }
}

void IncreaseSleepScore(u8 battlerAtk, u8 battlerDef, u16 move, s32 *score)
{
    u16 bestMoves[MAX_MON_MOVES] = {MOVE_NONE};
    u8 holdEffectDef = AI_DATA->holdEffects[battlerDef];
    u8 i;

    if (holdEffectDef == HOLD_EFFECT_CURE_SLP || holdEffectDef == HOLD_EFFECT_CURE_STATUS)
        return;

    // Don't bother with sleep when a KO is available (Focus Punch aside).
    if ((AI_THINKING_STRUCT->aiLogicFlags & AI_LOGIC_TRY_TO_FAINT) && CanAIFaintTarget(battlerAtk, battlerDef, 0))
    {
        GetBestDmgMovesFromBattler(battlerAtk, battlerDef, AI_ATTACKING, bestMoves);
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            if (bestMoves[i] != MOVE_NONE && gBattleMoves[bestMoves[i]].effect != EFFECT_FOCUS_PUNCH)
                return;
        }
    }

    if (!AI_CanPutToSleep(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], move))
        return;
    *score += DECENT_EFFECT;

    if ((HasMoveWithEffect(battlerAtk, EFFECT_DREAM_EATER) || HasMoveWithEffect(battlerAtk, EFFECT_NIGHTMARE))
     && !HasUsableWhileAsleepMove(battlerDef))
        *score += WEAK_EFFECT;
}

void IncreaseConfusionScore(u8 battlerAtk, u8 battlerDef, u16 move, s32 *score)
{
    u8 holdEffectDef = AI_DATA->holdEffects[battlerDef];

    if (((AI_THINKING_STRUCT->aiLogicFlags & AI_LOGIC_TRY_TO_FAINT) && CanAIFaintTarget(battlerAtk, battlerDef, 0))
     || holdEffectDef == HOLD_EFFECT_CURE_CONFUSION || holdEffectDef == HOLD_EFFECT_CURE_STATUS)
        return;

    if (AI_CanConfuse(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], move))
    {
        if ((gBattleMons[battlerDef].status1 & STATUS1_PARALYSIS)
         || (gBattleMons[battlerDef].status2 & STATUS2_INFATUATION)
         || (AI_DATA->abilities[battlerAtk] == ABILITY_SERENE_GRACE && HasMoveWithFlinchEffect(battlerAtk)))
            *score += GOOD_EFFECT;
        else
            *score += DECENT_EFFECT;
    }
}

// ---------------------------------------------------------------------------
// Situational checks
// ---------------------------------------------------------------------------

bool32 ShouldTryOHKO(u8 battlerAtk, u8 battlerDef, u8 atkAbility, u8 defAbility, u16 move)
{
    u8 holdEffect = AI_DATA->holdEffects[battlerDef];
    u32 accuracy = AI_DATA->moveAccuracy[battlerAtk][battlerDef][AI_THINKING_STRUCT->movesetIndex];
    u32 odds;
    bool32 sureHit;

    // Focus Band would probably activate.
    if (holdEffect == HOLD_EFFECT_FOCUS_BAND && (Random() % 100) < AI_DATA->holdEffectParams[battlerDef])
        return FALSE;
    if (defAbility == ABILITY_STURDY)
        return FALSE;
    if (gBattleMons[battlerAtk].level < gBattleMons[battlerDef].level)
        return FALSE;

    sureHit = (gStatuses3[battlerDef] & STATUS3_ALWAYS_HITS) && gDisableStructs[battlerDef].battlerWithSureHit == battlerAtk;
    if (sureHit)
        return TRUE;

    odds = accuracy;
    return (Random() % 100 + 1) < odds;
}

bool32 ShouldTryToFlinch(u8 battlerAtk, u8 battlerDef, u8 atkAbility, u8 defAbility, u16 move)
{
    if (defAbility == ABILITY_SHIELD_DUST || defAbility == ABILITY_INNER_FOCUS
     || DoesSubstituteBlockMove(battlerAtk, battlerDef, move)
     || AI_IsSlower(battlerAtk, battlerDef, move, MOVE_NONE, CONSIDER_PRIORITY)) // the target moves first
        return FALSE;

    if (atkAbility == ABILITY_SERENE_GRACE
     || (gBattleMons[battlerDef].status1 & STATUS1_PARALYSIS)
     || (gBattleMons[battlerDef].status2 & (STATUS2_INFATUATION | STATUS2_CONFUSION))
     || (AI_IsFaster(battlerAtk, battlerDef, move, MOVE_NONE, CONSIDER_PRIORITY) && CanTargetFaintAi(battlerDef, battlerAtk)))
        return TRUE;
    return FALSE;
}

bool32 IsFlinchGuaranteed(u8 battlerAtk, u8 battlerDef, u16 move)
{
    struct AiSecondaryEffect secondary = GetMoveSecondaryEffect(move);

    if (secondary.moveEffect != MOVE_EFFECT_FLINCH || !MoveEffectIsGuaranteed(battlerAtk, move, &secondary))
        return FALSE;
    if (AI_IsSlower(battlerAtk, battlerDef, move, MOVE_NONE, CONSIDER_PRIORITY))
        return FALSE;
    if (DoesSubstituteBlockMove(battlerAtk, battlerDef, move)
     || AI_DATA->abilities[battlerDef] == ABILITY_SHIELD_DUST
     || AI_DATA->abilities[battlerDef] == ABILITY_INNER_FOCUS)
        return FALSE;
    return TRUE;
}

bool32 ShouldTrap(u8 battlerAtk, u8 battlerDef, u16 move)
{
    if (IsBattlerTrapped(battlerAtk, battlerDef))
        return FALSE;
    // Trap a target that is about to faint from residual damage.
    return BattlerWillFaintFromSecondaryDamage(battlerDef, AI_DATA->abilities[battlerDef]);
}

static bool32 RecoveryEnablesWinning1v1(u8 battlerAtk, u8 battlerDef, u16 move, bool32 aiIsFaster, u32 healAmount)
{
    if (aiIsFaster)
    {
        // The target KOs the AI unless it heals.
        if (CanTargetFaintAi(battlerDef, battlerAtk) && !CanTargetFaintAiWithMod(battlerDef, battlerAtk, healAmount, 0))
            return TRUE;
    }
    else
    {
        // The target can't KO the AI and deals less damage than the AI heals.
        if (!CanTargetFaintAi(battlerDef, battlerAtk)
         && GetBestDmgFromBattler(battlerDef, battlerAtk, AI_DEFENDING) < healAmount
         && NoOfHitsForTargetToFaintBattler(battlerDef, battlerAtk) < NoOfHitsForTargetToFaintBattlerWithMod(battlerDef, battlerAtk, healAmount))
            return TRUE;
    }
    // The target can't KO the AI at all: generally safe to heal.
    if (!CanTargetFaintAi(battlerDef, battlerAtk)
     && AI_DATA->hpPercents[battlerAtk] < ENABLE_RECOVERY_THRESHOLD
     && RandomPercentage(SHOULD_RECOVER_CHANCE))
        return TRUE;
    return FALSE;
}

static bool32 ShouldDrainHPToWithstandHit(u8 battlerAtk, u8 battlerDef, u32 currHP, u32 healAmount)
{
    u32 bestDamageFromTarget = GetBestDmgFromBattler(battlerDef, battlerAtk, AI_DEFENDING);

    if (bestDamageFromTarget >= gBattleMons[battlerAtk].maxHP + healAmount)
        return FALSE;
    if (bestDamageFromTarget >= currHP && currHP + healAmount > bestDamageFromTarget)
        return TRUE;
    return FALSE;
}

bool32 ShouldAbsorb(u8 battlerAtk, u8 battlerDef, u16 move)
{
    u32 maxHP = gBattleMons[battlerAtk].maxHP;
    u32 currHP = gBattleMons[battlerAtk].hp;
    u32 healAmount = AI_GetDamage(battlerAtk, battlerDef, AI_THINKING_STRUCT->movesetIndex, AI_ATTACKING) / 2;
    bool32 aiIsFaster = AI_IsFaster(battlerAtk, battlerDef, move, MOVE_NONE, CONSIDER_PRIORITY);

    if (healAmount == 0)
        healAmount = 1;
    if (healAmount + currHP > maxHP)
        healAmount = maxHP - currHP;

    if (AI_DATA->abilities[battlerDef] == ABILITY_LIQUID_OOZE)
        return FALSE;
    if (AI_BattlerAtMaxHp(battlerAtk) && aiIsFaster)
        return FALSE;
    if (RecoveryEnablesWinning1v1(battlerAtk, battlerDef, move, aiIsFaster, healAmount))
        return TRUE;
    if (ShouldDrainHPToWithstandHit(battlerAtk, battlerDef, currHP, healAmount))
        return TRUE;
    return FALSE;
}

bool32 ShouldRecover(u8 battlerAtk, u8 battlerDef, u16 move, u32 healPercent)
{
    u32 maxHP = gBattleMons[battlerAtk].maxHP;
    u32 currHP = gBattleMons[battlerAtk].hp;
    u32 healAmount = (healPercent * maxHP) / 100;
    bool32 aiIsFaster = AI_IsFaster(battlerAtk, battlerDef, move, MOVE_NONE, CONSIDER_PRIORITY);

    if (healAmount + currHP > maxHP)
        healAmount = maxHP - currHP;
    if (AI_BattlerAtMaxHp(battlerAtk))
        return FALSE;
    return RecoveryEnablesWinning1v1(battlerAtk, battlerDef, move, aiIsFaster, healAmount);
}

bool32 ShouldSetScreen(u8 battlerAtk, u8 battlerDef, u8 moveEffect)
{
    u8 atkSide = GetBattlerSide(battlerAtk);

    // Don't waste a turn if Brick Break will break the screen.
    if (HasMoveWithEffect(battlerDef, EFFECT_BRICK_BREAK))
        return FALSE;

    switch (moveEffect)
    {
    case EFFECT_REFLECT:
        if (HasMoveWithCategory(battlerDef, DAMAGE_CATEGORY_PHYSICAL) && !(gSideStatuses[atkSide] & SIDE_STATUS_REFLECT))
            return TRUE;
        break;
    case EFFECT_LIGHT_SCREEN:
        if (HasMoveWithCategory(battlerDef, DAMAGE_CATEGORY_SPECIAL) && !(gSideStatuses[atkSide] & SIDE_STATUS_LIGHTSCREEN))
            return TRUE;
        break;
    }
    return FALSE;
}

bool32 ShouldUseWishAromatherapy(u8 battlerAtk, u8 battlerDef, u16 move)
{
    struct Pokemon *party = AI_GetBattlerParty(battlerAtk);
    bool32 hasStatus = AnyPartyMemberStatused(battlerAtk, move == MOVE_HEAL_BELL);
    bool32 needHealing = FALSE;
    u32 currHp, maxHp;
    s32 i;

    // Don't heal the team when this is the last mon and it's about to faint.
    if (CountUsablePartyMons(battlerAtk) == 0
     && (CanTargetFaintAi(battlerDef, battlerAtk) || BattlerWillFaintFromSecondaryDamage(battlerAtk, AI_DATA->abilities[battlerAtk])))
        return FALSE;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        currHp = GetMonData(&party[i], MON_DATA_HP);
        maxHp = GetMonData(&party[i], MON_DATA_MAX_HP);
        if (!GetMonData(&party[i], MON_DATA_IS_EGG) && currHp > 0 && maxHp > 0 && (currHp * 100) / maxHp < 65)
            needHealing = TRUE;
    }

    if (IsBattle1v1())
    {
        switch (gBattleMoves[move].effect)
        {
        case EFFECT_WISH:
            return needHealing;
        case EFFECT_HEAL_BELL:
            return hasStatus;
        }
    }
    else
    {
        switch (gBattleMoves[move].effect)
        {
        case EFFECT_WISH:
            return ShouldRecover(battlerAtk, battlerDef, move, 50); // assume the partner isn't the one to heal
        case EFFECT_HEAL_BELL:
            return hasStatus;
        }
    }
    return FALSE;
}

bool32 AI_ShouldSetUpHazards(u8 battlerAtk, u8 battlerDef, u16 move)
{
    if (CountUsablePartyMons(battlerDef) == 0 || HasMoveWithEffect(battlerDef, EFFECT_RAPID_SPIN))
        return FALSE;
    if (HasMoveWithEffect(battlerDef, EFFECT_MAGIC_COAT))
        return FALSE;
    return TRUE;
}

bool32 HasHPForDamagingSetup(u8 battlerAtk, u8 battlerDef, u32 hpThreshold)
{
    return GetBestDmgFromBattler(battlerDef, battlerAtk, AI_DEFENDING) < (hpThreshold * gBattleMons[battlerAtk].maxHP) / 100;
}

static bool32 IsBattlerDamagedByStatus(u8 battler)
{
    return (gBattleMons[battler].status1 & (STATUS1_PSN_ANY | STATUS1_BURN))
        || (gBattleMons[battler].status2 & (STATUS2_WRAPPED | STATUS2_NIGHTMARE | STATUS2_CURSED))
        || (gStatuses3[battler] & (STATUS3_LEECHSEED | STATUS3_PERISH_SONG));
}

s32 ProtectChecks(u8 battlerAtk, u8 battlerDef, u16 move, u16 predictedMove)
{
    s32 score = 0;
    u32 uses = gDisableStructs[battlerAtk].protectUses;

    if (uses == 0)
    {
        if (predictedMove != MOVE_NONE && predictedMove != MOVE_UNAVAILABLE && !IsBattleMoveStatus(predictedMove))
            score += DECENT_EFFECT;
        else if (Random() % 256 < 100)
            score += WEAK_EFFECT;
    }
    else
    {
        if (!IsBattle1v1())
            score -= 2 * min(uses, 3);
        else
            score -= min(uses, 3);
    }

    if (IsBattlerDamagedByStatus(battlerAtk))
        score -= 1;
    if (IsBattlerDamagedByStatus(battlerDef))
        score += DECENT_EFFECT;
    return score;
}

u32 IncreaseSubstituteMoveScore(u8 battlerAtk, u8 battlerDef, u16 move)
{
    u32 scoreIncrease = 0;

    if (GetBestDmgFromBattler(battlerDef, battlerAtk, AI_DEFENDING) < gBattleMons[battlerAtk].maxHP / 4)
        scoreIncrease += GOOD_EFFECT;
    if (gStatuses3[battlerDef] & STATUS3_PERISH_SONG)
        scoreIncrease += GOOD_EFFECT;
    if (gBattleMons[battlerDef].status1 & STATUS1_SLEEP)
        scoreIncrease += GOOD_EFFECT;
    else if (gBattleMons[battlerDef].status1 & (STATUS1_PSN_ANY | STATUS1_BURN))
        scoreIncrease += DECENT_EFFECT;

    if (HasNonVolatileMoveEffect(battlerDef, MOVE_EFFECT_SLEEP)
     || HasNonVolatileMoveEffect(battlerDef, MOVE_EFFECT_TOXIC)
     || HasNonVolatileMoveEffect(battlerDef, MOVE_EFFECT_PARALYSIS)
     || HasNonVolatileMoveEffect(battlerDef, MOVE_EFFECT_BURN)
     || HasMoveWithEffect(battlerDef, EFFECT_CONFUSE)
     || HasMoveWithEffect(battlerDef, EFFECT_LEECH_SEED))
        scoreIncrease += GOOD_EFFECT;

    if (AI_DATA->hpPercents[battlerAtk] > 70)
        scoreIncrease += WEAK_EFFECT;
    return scoreIncrease;
}

// ---------------------------------------------------------------------------
// Weather (expansion's battle_ai_field_statuses.c, Gen 3 parts)
// ---------------------------------------------------------------------------

enum FieldEffectOutcome
{
    FIELD_EFFECT_POSITIVE,
    FIELD_EFFECT_NEUTRAL,
    FIELD_EFFECT_NEGATIVE,
};

static bool32 DoesAbilityBenefitFromWeather(u8 ability, u16 weather)
{
    switch (ability)
    {
    case ABILITY_FORECAST:
        return (weather & (B_WEATHER_RAIN | B_WEATHER_SUN | B_WEATHER_HAIL)) != 0;
    case ABILITY_SAND_VEIL:
        return (weather & B_WEATHER_SANDSTORM) != 0;
    case ABILITY_RAIN_DISH:
    case ABILITY_SWIFT_SWIM:
        return (weather & B_WEATHER_RAIN) != 0;
    case ABILITY_CHLOROPHYLL:
        return (weather & B_WEATHER_SUN) != 0;
    }
    return FALSE;
}

// Moves boosted by sun and weakened by the other weathers.
static bool32 HasLightSensitiveMove(u8 battler)
{
    return HasMoveWithEffect(battler, EFFECT_SOLAR_BEAM)
        || HasMoveWithEffect(battler, EFFECT_MORNING_SUN)
        || HasMoveWithEffect(battler, EFFECT_SYNTHESIS)
        || HasMoveWithEffect(battler, EFFECT_MOONLIGHT);
}

static u8 BenefitsFromSun(u8 battler)
{
    if (DoesAbilityBenefitFromWeather(AI_DATA->abilities[battler], B_WEATHER_SUN)
     || HasLightSensitiveMove(battler)
     || HasDamagingMoveOfType(battler, TYPE_FIRE)
     || HasMoveWithEffect(battler, EFFECT_WEATHER_BALL))
        return FIELD_EFFECT_POSITIVE;
    if (HasMoveWithEffect(battler, EFFECT_THUNDER) || HasDamagingMoveOfType(battler, TYPE_WATER))
        return FIELD_EFFECT_NEGATIVE;
    return FIELD_EFFECT_NEUTRAL;
}

static u8 BenefitsFromSandstorm(u8 battler)
{
    if (DoesAbilityBenefitFromWeather(AI_DATA->abilities[battler], B_WEATHER_SANDSTORM)
     || IS_BATTLER_OF_TYPE(battler, TYPE_ROCK)
     || HasMoveWithEffect(battler, EFFECT_WEATHER_BALL))
        return FIELD_EFFECT_POSITIVE;
    if (IS_BATTLER_OF_TYPE(battler, TYPE_GROUND) || IS_BATTLER_OF_TYPE(battler, TYPE_STEEL))
    {
        u8 foe = LEFT_FOE(battler);

        if (!IS_BATTLER_OF_TYPE(foe, TYPE_ROCK) && !IS_BATTLER_OF_TYPE(foe, TYPE_GROUND) && !IS_BATTLER_OF_TYPE(foe, TYPE_STEEL)
         && !DoesAbilityBenefitFromWeather(AI_DATA->abilities[foe], B_WEATHER_SANDSTORM))
            return FIELD_EFFECT_POSITIVE;
        return FIELD_EFFECT_NEUTRAL;
    }
    return FIELD_EFFECT_NEGATIVE;
}

static u8 BenefitsFromHail(u8 battler)
{
    if (DoesAbilityBenefitFromWeather(AI_DATA->abilities[battler], B_WEATHER_HAIL)
     || IS_BATTLER_OF_TYPE(battler, TYPE_ICE)
     || HasMoveWithEffect(battler, EFFECT_WEATHER_BALL))
        return FIELD_EFFECT_POSITIVE;
    return FIELD_EFFECT_NEGATIVE; // hail hurts everything that isn't Ice
}

static u8 BenefitsFromRain(u8 battler)
{
    if (DoesAbilityBenefitFromWeather(AI_DATA->abilities[battler], B_WEATHER_RAIN)
     || HasMoveWithEffect(battler, EFFECT_THUNDER)
     || HasDamagingMoveOfType(battler, TYPE_WATER)
     || HasMoveWithEffect(battler, EFFECT_WEATHER_BALL))
        return FIELD_EFFECT_POSITIVE;
    if (HasLightSensitiveMove(battler) || HasDamagingMoveOfType(battler, TYPE_FIRE))
        return FIELD_EFFECT_NEGATIVE;
    return FIELD_EFFECT_NEUTRAL;
}

static bool32 WeatherChecker(u8 battler, u16 weather, u8 desiredResult)
{
    u8 result = FIELD_EFFECT_NEUTRAL;
    u8 firstResult = FIELD_EFFECT_NEUTRAL;
    u8 battlersOnSide = HasPartner(battler) ? 2 : 1;
    u8 i;

    for (i = 0; i < battlersOnSide; i++)
    {
        if (weather & B_WEATHER_RAIN)
            result = BenefitsFromRain(battler);
        else if (weather & B_WEATHER_SUN)
            result = BenefitsFromSun(battler);
        else if (weather & B_WEATHER_SANDSTORM)
            result = BenefitsFromSandstorm(battler);
        else if (weather & B_WEATHER_HAIL)
            result = BenefitsFromHail(battler);

        battler = BATTLE_PARTNER(battler);
        // Damaging weather: both mons on the side have to agree.
        if (result != FIELD_EFFECT_NEUTRAL && (weather & (B_WEATHER_SANDSTORM | B_WEATHER_HAIL)) && i == 0 && battlersOnSide == 2)
            firstResult = result;
    }

    if (firstResult != FIELD_EFFECT_NEUTRAL)
        return firstResult == result && result == desiredResult;
    return result == desiredResult;
}

static bool32 ShouldSetWeather(u8 battler, u16 weather)
{
    if (AI_GetWeather() & weather)
        return FALSE;
    return WeatherChecker(battler, weather, FIELD_EFFECT_POSITIVE);
}

s32 CalcWeatherScore(u8 battlerAtk, u8 battlerDef, u16 move)
{
    s32 score = 0;
    u8 partnerDef = BATTLE_PARTNER(battlerDef);

    switch (gBattleMoves[move].effect)
    {
    case EFFECT_RAIN_DANCE:
        if (ShouldSetWeather(battlerAtk, B_WEATHER_RAIN))
        {
            score += DECENT_EFFECT;
            if (HasMoveWithEffect(battlerAtk, EFFECT_WEATHER_BALL))
                score += WEAK_EFFECT;
            if (HasLightSensitiveMove(battlerDef))
                score += WEAK_EFFECT;
            if (HasDamagingMoveOfType(battlerDef, TYPE_FIRE) || (IsDoubleBattle() && HasDamagingMoveOfType(partnerDef, TYPE_FIRE)))
                score += WEAK_EFFECT;
        }
        break;
    case EFFECT_SUNNY_DAY:
        if (ShouldSetWeather(battlerAtk, B_WEATHER_SUN))
        {
            score += DECENT_EFFECT;
            if (HasMoveWithEffect(battlerAtk, EFFECT_WEATHER_BALL))
                score += WEAK_EFFECT;
            if (HasDamagingMoveOfType(battlerDef, TYPE_WATER) || (IsDoubleBattle() && HasDamagingMoveOfType(partnerDef, TYPE_WATER)))
                score += WEAK_EFFECT;
            if (HasMoveWithEffect(battlerDef, EFFECT_THUNDER) || (IsDoubleBattle() && HasMoveWithEffect(partnerDef, EFFECT_THUNDER)))
                score += WEAK_EFFECT;
        }
        break;
    case EFFECT_SANDSTORM:
        if (ShouldSetWeather(battlerAtk, B_WEATHER_SANDSTORM))
        {
            score += DECENT_EFFECT;
            if (HasMoveWithEffect(battlerAtk, EFFECT_WEATHER_BALL))
                score += WEAK_EFFECT;
            if (HasMoveWithEffect(battlerDef, EFFECT_MORNING_SUN)
             || HasMoveWithEffect(battlerDef, EFFECT_SYNTHESIS)
             || HasMoveWithEffect(battlerDef, EFFECT_MOONLIGHT))
                score += WEAK_EFFECT;
        }
        break;
    case EFFECT_HAIL:
        if (ShouldSetWeather(battlerAtk, B_WEATHER_HAIL))
        {
            score += DECENT_EFFECT;
            if (HasMoveWithEffect(battlerAtk, EFFECT_WEATHER_BALL))
                score += WEAK_EFFECT;
            if (HasMoveWithEffect(battlerDef, EFFECT_MORNING_SUN)
             || HasMoveWithEffect(battlerDef, EFFECT_SYNTHESIS)
             || HasMoveWithEffect(battlerDef, EFFECT_MOONLIGHT))
                score += WEAK_EFFECT;
        }
        break;
    }
    return score;
}
