#include "global.h"
#include "battle.h"
#include "battle_ai_engine.h"
#include "battle_ai_script_commands.h"
#include "battle_anim.h"
#include "battle_interface.h"
#include "battle_main.h"
#include "battle_util.h"
#include "event_data.h"
#include "item.h"
#include "malloc.h"
#include "pokedex.h"
#include "pokemon.h"
#include "random.h"
#include "util.h"
#include "constants/abilities.h"
#include "constants/battle_ai.h"
#include "constants/battle_move_effects.h"
#include "constants/hold_effects.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/species.h"
#if AI_DEBUG_CHECKS
#include "gba/isagbprint.h"
#endif

// See include/battle_ai_engine.h for what this file is for.

// Tables owned by the battle engine, so the AI uses exactly the same numbers.
struct StatFractions
{
    u8 dividend;
    u8 divisor;
};
extern const struct StatFractions sAccuracyStageRatios[];  // battle_script_commands.c
extern const u8 sFlailHpScaleToPowerTable[];               // battle_script_commands.c
extern const u16 sWeightToDamageTable[];                   // battle_script_commands.c
extern const u16 sSoundMovesTable[];                       // battle_util.c
#define SOUND_MOVES_END 0xFFFF
#define FLAIL_TABLE_SIZE 12 // bytes in sFlailHpScaleToPowerTable (6 pairs)

// Battle globals that CalculateBaseDamage and the ability checks read or write.
// The damage simulation saves and restores them so the battle never notices.
struct AiSavedBattleState
{
    s32 battleMoveDamage;
    u16 currentMove;
    u16 battleMovePower;
    u16 dynamicBasePower;
    u8 battlerAttacker;
    u8 battlerTarget;
    u8 critMultiplier;
    u8 dmgMultiplier;
    u8 dynamicMoveType;
    u8 lastUsedAbility;
};

static void SaveBattleState(struct AiSavedBattleState *state)
{
    state->battleMoveDamage = gBattleMoveDamage;
    state->currentMove = gCurrentMove;
    state->battleMovePower = gBattleMovePower;
    state->dynamicBasePower = gDynamicBasePower;
    state->battlerAttacker = gBattlerAttacker;
    state->battlerTarget = gBattlerTarget;
    state->critMultiplier = gCritMultiplier;
    state->dmgMultiplier = gBattleScripting.dmgMultiplier;
    state->dynamicMoveType = gBattleStruct->dynamicMoveType;
    state->lastUsedAbility = gLastUsedAbility;
}

static void RestoreBattleState(const struct AiSavedBattleState *state)
{
    gBattleMoveDamage = state->battleMoveDamage;
    gCurrentMove = state->currentMove;
    gBattleMovePower = state->battleMovePower;
    gDynamicBasePower = state->dynamicBasePower;
    gBattlerAttacker = state->battlerAttacker;
    gBattlerTarget = state->battlerTarget;
    gCritMultiplier = state->critMultiplier;
    gBattleScripting.dmgMultiplier = state->dmgMultiplier;
    gBattleStruct->dynamicMoveType = state->dynamicMoveType;
    gLastUsedAbility = state->lastUsedAbility;
}

// WEATHER_HAS_EFFECT, without leaving gLastUsedAbility changed.
static bool32 AI_WeatherHasEffect(void)
{
    u8 lastUsedAbility = gLastUsedAbility;
    bool32 hasEffect = WEATHER_HAS_EFFECT;

    gLastUsedAbility = lastUsedAbility;
    return hasEffect;
}

// ---------------------------------------------------------------------------
// Move data
// ---------------------------------------------------------------------------

u8 GetMoveEffect(u16 move)
{
    return gBattleMoves[move].effect;
}

u8 GetMovePower(u16 move)
{
    return gBattleMoves[move].power;
}

u8 GetMoveType(u16 move)
{
    return gBattleMoves[move].type;
}

u8 GetMoveAccuracy(u16 move)
{
    return gBattleMoves[move].accuracy;
}

u8 GetMoveSecondaryEffectChance(u16 move)
{
    return gBattleMoves[move].secondaryEffectChance;
}

u8 GetMoveTargetType(u16 move)
{
    return gBattleMoves[move].target;
}

s8 GetMovePriority(u16 move)
{
    return gBattleMoves[move].priority;
}

bool32 MoveMakesContact(u16 move)
{
    return (gBattleMoves[move].flags & FLAG_MAKES_CONTACT) != 0;
}

bool32 IsSoundMove(u16 move)
{
    s32 i;

    for (i = 0; sSoundMovesTable[i] != SOUND_MOVES_END; i++)
    {
        if (sSoundMovesTable[i] == move)
            return TRUE;
    }
    return FALSE;
}

// Gen 3 has no status category: a move without base power deals no direct damage.
bool32 IsBattleMoveStatus(u16 move)
{
    return gBattleMoves[move].power == 0;
}

// Same formula as Cmd_hiddenpowercalc.
static u8 GetHiddenPowerType(u8 battler)
{
    struct BattlePokemon *mon = &gBattleMons[battler];
    s32 typeBits, type;

    typeBits = ((mon->hpIV & 1) << 0)
             | ((mon->attackIV & 1) << 1)
             | ((mon->defenseIV & 1) << 2)
             | ((mon->speedIV & 1) << 3)
             | ((mon->spAttackIV & 1) << 4)
             | ((mon->spDefenseIV & 1) << 5);

    type = ((NUMBER_OF_MON_TYPES - 3) * typeBits) / 63 + 1;
    if (type >= TYPE_MYSTERY)
        type++;
    return type;
}

static u16 GetHiddenPowerPower(u8 battler)
{
    struct BattlePokemon *mon = &gBattleMons[battler];
    s32 powerBits;

    powerBits = ((mon->hpIV & 2) >> 1)
              | ((mon->attackIV & 2) << 0)
              | ((mon->defenseIV & 2) << 1)
              | ((mon->speedIV & 2) << 2)
              | ((mon->spAttackIV & 2) << 3)
              | ((mon->spDefenseIV & 2) << 4);

    return (40 * powerBits) / 63 + 30;
}

// Same rules as Cmd_setweatherballtype.
static u8 GetWeatherBallType(void)
{
    if (AI_WeatherHasEffect())
    {
        if (gBattleWeather & B_WEATHER_RAIN)
            return TYPE_WATER;
        else if (gBattleWeather & B_WEATHER_SANDSTORM)
            return TYPE_ROCK;
        else if (gBattleWeather & B_WEATHER_SUN)
            return TYPE_FIRE;
        else if (gBattleWeather & B_WEATHER_HAIL)
            return TYPE_ICE;
    }
    return TYPE_NORMAL;
}

u8 GetBattleMoveType(u16 move, u8 battlerAtk)
{
    switch (gBattleMoves[move].effect)
    {
    case EFFECT_HIDDEN_POWER:
        return GetHiddenPowerType(battlerAtk);
    case EFFECT_WEATHER_BALL:
        return GetWeatherBallType();
    }
    return gBattleMoves[move].type;
}

// Gen 3 split: the move's type decides physical or special (IS_TYPE_PHYSICAL).
u8 GetBattleMoveCategory(u16 move, u8 battlerAtk)
{
    u8 type;

    if (IsBattleMoveStatus(move))
        return DAMAGE_CATEGORY_STATUS;

    type = GetBattleMoveType(move, battlerAtk);
    if (IS_TYPE_PHYSICAL(type))
        return DAMAGE_CATEGORY_PHYSICAL;
    if (IS_TYPE_SPECIAL(type))
        return DAMAGE_CATEGORY_SPECIAL;
    return DAMAGE_CATEGORY_STATUS; // ??? type
}

bool32 IsBattleMovePhysical(u16 move, u8 battlerAtk)
{
    return GetBattleMoveCategory(move, battlerAtk) == DAMAGE_CATEGORY_PHYSICAL;
}

bool32 IsBattleMoveSpecial(u16 move, u8 battlerAtk)
{
    return GetBattleMoveCategory(move, battlerAtk) == DAMAGE_CATEGORY_SPECIAL;
}

// ---------------------------------------------------------------------------
// What the AI believes about each battler
// ---------------------------------------------------------------------------

bool32 IsBattlerAlive(u8 battler)
{
    if (battler >= gBattlersCount)
        return FALSE;
    if (gAbsentBattlerFlags & gBitTable[battler])
        return FALSE;
    return gBattleMons[battler].hp != 0;
}

static bool32 AiKnowledgeReady(void)
{
    return gBattleResources != NULL && AI_LOGIC_DATA != NULL && AI_LOGIC_DATA->ready;
}

// The ability the AI assumes the battler has. AI_SetLogicDataForTurn decides it;
// before that runs it is the real ability.
u8 AI_GetBattlerAbility(u8 battler)
{
    if (AiKnowledgeReady())
        return AI_LOGIC_DATA->abilities[battler];
    return gBattleMons[battler].ability;
}

u16 AI_GetBattlerItem(u8 battler)
{
    if (AiKnowledgeReady())
        return AI_LOGIC_DATA->items[battler];
    return gBattleMons[battler].item;
}

static u8 GetHoldEffectOfItem(u8 battler, u16 item)
{
    if (item == ITEM_ENIGMA_BERRY)
        return gEnigmaBerries[battler].holdEffect;
    return ItemId_GetHoldEffect(item);
}

static u8 GetHoldEffectParamOfItem(u8 battler, u16 item)
{
    if (item == ITEM_ENIGMA_BERRY)
        return gEnigmaBerries[battler].holdEffectParam;
    return ItemId_GetHoldEffectParam(item);
}

u8 AI_GetBattlerHoldEffect(u8 battler)
{
    if (AiKnowledgeReady())
        return AI_LOGIC_DATA->holdEffects[battler];
    return GetHoldEffectOfItem(battler, gBattleMons[battler].item);
}

u8 AI_GetBattlerHoldEffectParam(u8 battler)
{
    if (AiKnowledgeReady())
        return AI_LOGIC_DATA->holdEffectParams[battler];
    return GetHoldEffectParamOfItem(battler, gBattleMons[battler].item);
}

bool32 AI_BattlerHasType(u8 battler, u8 type)
{
    return IS_BATTLER_OF_TYPE(battler, type);
}

// ---------------------------------------------------------------------------
// What the AI knows about the player's side
// ---------------------------------------------------------------------------

// The attack a level 8 trainer expects of each type on a move it hasn't seen:
// a common move of 60-80 power. Only used to estimate damage and to predict.
static const u16 sAssumedStabMoves[NUMBER_OF_MON_TYPES] =
{
    [TYPE_NORMAL]   = MOVE_STRENGTH,
    [TYPE_FIGHTING] = MOVE_BRICK_BREAK,
    [TYPE_FLYING]   = MOVE_DRILL_PECK,
    [TYPE_POISON]   = MOVE_SLUDGE,
    [TYPE_GROUND]   = MOVE_BONE_CLUB,
    [TYPE_ROCK]     = MOVE_ROCK_SLIDE,
    [TYPE_BUG]      = MOVE_SIGNAL_BEAM,
    [TYPE_GHOST]    = MOVE_SHADOW_BALL,
    [TYPE_STEEL]    = MOVE_STEEL_WING,
    [TYPE_FIRE]     = MOVE_FIRE_PUNCH,
    [TYPE_WATER]    = MOVE_WATERFALL,
    [TYPE_GRASS]    = MOVE_GIGA_DRAIN,
    [TYPE_ELECTRIC] = MOVE_THUNDER_PUNCH,
    [TYPE_PSYCHIC]  = MOVE_PSYBEAM,
    [TYPE_ICE]      = MOVE_ICE_PUNCH,
    [TYPE_DRAGON]   = MOVE_DRAGON_CLAW,
    [TYPE_DARK]     = MOVE_BITE,
};

// The AI knows everything about its own side. A level 9+ trainer also knows
// the player's side; others only what the battle has shown.
bool32 AI_KnowsBattler(u8 battler)
{
    if (GetBattlerSide(battler) != B_SIDE_PLAYER)
        return TRUE;
    return AI_LOGIC_DATA != NULL && (AI_LOGIC_DATA->knowledgeFlags & AI_KNOW_OMNISCIENT);
}

static struct AiPartyKnowledge *GetPartyKnowledge(u8 battler, u8 partyIndex)
{
    if (AI_LOGIC_DATA == NULL || GetBattlerSide(battler) != B_SIDE_PLAYER || partyIndex >= PARTY_SIZE)
        return NULL;
    return &AI_LOGIC_DATA->playerParty[partyIndex];
}

static struct AiPartyKnowledge *GetBattlerKnowledge(u8 battler)
{
    return GetPartyKnowledge(battler, gBattlerPartyIndexes[battler]);
}

void AI_RecordAbility(u8 battler, u8 ability)
{
    struct AiPartyKnowledge *mon = GetBattlerKnowledge(battler);

    if (mon != NULL && ability != ABILITY_NONE)
        mon->ability = ability;
}

void AI_RecordItem(u8 battler)
{
    struct AiPartyKnowledge *mon = GetBattlerKnowledge(battler);

    if (mon != NULL)
        mon->item = gBattleMons[battler].item;
}

// At every decision: the player's mons on the field have been seen, and so
// has the move each used last (gLastMoves only holds moves of earlier turns).
static void RecordWhatTheAiSees(void)
{
    struct AiPartyKnowledge *mon;
    u16 move;
    u8 battler, i;

    for (battler = 0; battler < gBattlersCount; battler++)
    {
        mon = GetBattlerKnowledge(battler);
        if (mon == NULL || gBattleMons[battler].species == SPECIES_NONE || (gAbsentBattlerFlags & gBitTable[battler]))
            continue;
        mon->seen = TRUE;
        move = gLastMoves[battler];
        if (move == MOVE_NONE || move == MOVE_UNAVAILABLE)
            continue;
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            if (gBattleMons[battler].moves[i] == move)
            {
                mon->moves[i] = move;
                break;
            }
        }
    }
}

// The guess for an ability the battle hasn't shown: one of the species'
// abilities, picked again every turn.
static u8 GuessAbility(struct AiPartyKnowledge *mon, u16 species, u8 ability)
{
    u8 ability0 = gSpeciesInfo[species].abilities[0];
    u8 ability1 = gSpeciesInfo[species].abilities[1];
    u8 turn = gBattleResults.battleTurnCounter + 1;

    if (mon->ability != ABILITY_NONE)
        return ability;
    // Only one ability possible, or it changed in plain sight (Trace, Skill Swap, Role Play).
    if (ability1 == ABILITY_NONE || ability1 == ability0 || (ability != ability0 && ability != ability1))
        return ability;
    // Like the expansion AI: the player notices when it can't switch out.
    if (ability == ABILITY_SHADOW_TAG || ability == ABILITY_MAGNET_PULL || ability == ABILITY_ARENA_TRAP)
        return ability;
    if (mon->guessTurn != turn)
    {
        mon->guessedAbility = (Random() & 1) ? ability1 : ability0;
        mon->guessTurn = turn;
    }
    return mon->guessedAbility;
}

static u8 GetKnownAbility(u8 battler)
{
    struct AiPartyKnowledge *mon = GetBattlerKnowledge(battler);

    if (AI_KnowsBattler(battler) || mon == NULL)
        return gBattleMons[battler].ability;
    return GuessAbility(mon, gBattleMons[battler].species, gBattleMons[battler].ability);
}

static u16 GetKnownItem(u8 battler)
{
    struct AiPartyKnowledge *mon = GetBattlerKnowledge(battler);

    if (AI_KnowsBattler(battler) || mon == NULL)
        return gBattleMons[battler].item;
    if (mon->item != ITEM_NONE && mon->item == gBattleMons[battler].item)
        return mon->item;
    return ITEM_NONE;
}

// The moves the AI believes a battler it doesn't fully know has.
static void SetKnownMoves(u8 battler)
{
    struct AiPartyKnowledge *mon = GetBattlerKnowledge(battler);
    u16 *known = AI_LOGIC_DATA->knownMoves[battler];
    u16 assumed;
    u8 types[2], i, j;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        known[i] = MOVE_NONE;
        if (mon != NULL && mon->moves[i] != MOVE_NONE && mon->moves[i] == gBattleMons[battler].moves[i])
            known[i] = mon->moves[i];
    }

    if (!(AI_LOGIC_DATA->knowledgeFlags & AI_KNOW_ASSUME_STAB))
        return;

    // Expect an attack of each of its types it hasn't shown one of.
    types[0] = gBattleMons[battler].type1;
    types[1] = gBattleMons[battler].type2;
    for (j = 0; j < 2; j++)
    {
        if ((j == 1 && types[1] == types[0]) || types[j] >= NUMBER_OF_MON_TYPES)
            continue;
        assumed = sAssumedStabMoves[types[j]];
        if (assumed == MOVE_NONE)
            continue;
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            if (known[i] != MOVE_NONE && gBattleMoves[known[i]].power != 0 && gBattleMoves[known[i]].type == types[j])
                break;
        }
        if (i < MAX_MON_MOVES)
            continue;
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            if (known[i] == MOVE_NONE)
            {
                known[i] = assumed;
                break;
            }
        }
    }
}

u16 *AI_GetMovesArray(u8 battler)
{
    if (AI_LOGIC_DATA != NULL && !AI_KnowsBattler(battler))
        return AI_LOGIC_DATA->knownMoves[battler];
    return gBattleMons[battler].moves;
}

bool32 AI_CanSeePartyMon(u8 battler, u8 partyIndex)
{
    struct AiPartyKnowledge *mon = GetPartyKnowledge(battler, partyIndex);

    return AI_KnowsBattler(battler) || mon == NULL || mon->seen;
}

u16 AI_GetPartyMonMove(u8 battler, u8 partyIndex, u8 slot)
{
    struct Pokemon *party = (GetBattlerSide(battler) == B_SIDE_PLAYER) ? gPlayerParty : gEnemyParty;
    struct AiPartyKnowledge *mon = GetPartyKnowledge(battler, partyIndex);
    u16 move = GetMonData(&party[partyIndex], MON_DATA_MOVE1 + slot);

    if (AI_KnowsBattler(battler) || mon == NULL || mon->moves[slot] == move)
        return move;
    return MOVE_NONE;
}

u8 AI_GetPartyMonAbility(u8 battler, u8 partyIndex)
{
    struct Pokemon *party = (GetBattlerSide(battler) == B_SIDE_PLAYER) ? gPlayerParty : gEnemyParty;
    struct AiPartyKnowledge *mon = GetPartyKnowledge(battler, partyIndex);
    u8 ability = GetMonAbility(&party[partyIndex]);

    if (AI_KnowsBattler(battler) || mon == NULL)
        return ability;
    return GuessAbility(mon, GetMonData(&party[partyIndex], MON_DATA_SPECIES), ability);
}

bool32 AI_MoveSlotHasPP(u8 battler, u8 slot)
{
    if (AI_KnowsBattler(battler))
        return gBattleMons[battler].pp[slot] != 0;
    return TRUE;
}

// Disable, Torment, Taunt, Imprison, Encore and a known Choice Band, for the
// moves the AI believes the battler has. The AI can't see PP or hidden items.
static u8 GetVisibleMoveLimitations(u8 battler)
{
    u16 *moves = AI_LOGIC_DATA->knownMoves[battler];
    u16 choicedMove = gBattleStruct->choicedMove[battler];
    u8 limitations = 0;
    u8 i;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] == MOVE_NONE)
            continue;
        if (moves[i] == gDisableStructs[battler].disabledMove
         || (moves[i] == gLastMoves[battler] && (gBattleMons[battler].status2 & STATUS2_TORMENT))
         || (gDisableStructs[battler].tauntTimer && gBattleMoves[moves[i]].power == 0)
         || GetImprisonedMovesCount(battler, moves[i])
         || (gDisableStructs[battler].encoreTimer && gDisableStructs[battler].encoredMove != moves[i])
         || (AI_LOGIC_DATA->holdEffects[battler] == HOLD_EFFECT_CHOICE_BAND
          && choicedMove != MOVE_NONE && choicedMove != MOVE_UNAVAILABLE && choicedMove != moves[i]))
            limitations |= gBitTable[i];
    }
    return limitations;
}

u16 AI_GetWeather(void)
{
    if (!AI_WeatherHasEffect())
        return 0;
    return gBattleWeather;
}

// Is any battler on the field known to have this ability?
bool32 AI_IsAbilityOnField(u8 ability)
{
    s32 i;

    for (i = 0; i < gBattlersCount; i++)
    {
        if (IsBattlerAlive(i) && AI_GetBattlerAbility(i) == ability)
            return TRUE;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Type matchups (Gen 3 type chart, same loop as Cmd_typecalc)
// ---------------------------------------------------------------------------

uq4_12_t AI_GetTypeEffectiveness(u8 moveType, u8 battlerDef)
{
    s32 i = 0;
    u32 effectiveness = AI_EFFECTIVENESS_1;
    u8 type1 = gBattleMons[battlerDef].type1;
    u8 type2 = gBattleMons[battlerDef].type2;

    while (TYPE_EFFECT_ATK_TYPE(i) != TYPE_ENDTABLE)
    {
        if (TYPE_EFFECT_ATK_TYPE(i) == TYPE_FORESIGHT)
        {
            if (gBattleMons[battlerDef].status2 & STATUS2_FORESIGHT)
                break;
            i += 3;
            continue;
        }
        if (TYPE_EFFECT_ATK_TYPE(i) == moveType)
        {
            if (TYPE_EFFECT_DEF_TYPE(i) == type1)
                effectiveness = effectiveness * TYPE_EFFECT_MULTIPLIER(i) / 10;
            if (TYPE_EFFECT_DEF_TYPE(i) == type2 && type1 != type2)
                effectiveness = effectiveness * TYPE_EFFECT_MULTIPLIER(i) / 10;
        }
        i += 3;
    }
    return effectiveness;
}

// Abilities that make the move fail outright before any damage is dealt.
bool32 AI_IsMoveBlockedByAbility(u16 move, u8 battlerAtk, u8 battlerDef)
{
    u8 abilityDef = AI_GetBattlerAbility(battlerDef);
    u8 moveType = GetBattleMoveType(move, battlerAtk);

    switch (abilityDef)
    {
    case ABILITY_VOLT_ABSORB:
        if (moveType == TYPE_ELECTRIC && gBattleMoves[move].power != 0)
            return TRUE;
        break;
    case ABILITY_WATER_ABSORB:
        if (moveType == TYPE_WATER && gBattleMoves[move].power != 0)
            return TRUE;
        break;
    case ABILITY_FLASH_FIRE:
        if (moveType == TYPE_FIRE && !(gBattleMons[battlerDef].status1 & STATUS1_FREEZE))
            return TRUE;
        break;
    case ABILITY_SOUNDPROOF:
        if (IsSoundMove(move))
            return TRUE;
        break;
    }

    if (gBattleMoves[move].effect == EFFECT_EXPLOSION && AI_IsAbilityOnField(ABILITY_DAMP))
        return TRUE;

    return FALSE;
}

// Effectiveness after the target's ability: Levitate, Wonder Guard and the
// absorbing/blocking abilities all count as no effect.
uq4_12_t AI_GetMoveEffectiveness(u16 move, u8 battlerAtk, u8 battlerDef)
{
    uq4_12_t effectiveness;
    u8 moveType;

    if (move == MOVE_NONE || move == MOVE_STRUGGLE)
        return AI_EFFECTIVENESS_1;

    // Moves without power only check types when their battle script runs
    // typecalc: of those, only Thunder Wave, Stun Spore and Glare. Moves on
    // the user or the field are never blocked by the target.
    if (gBattleMoves[move].power == 0 && gBattleMoves[move].effect != EFFECT_PARALYZE)
    {
        if (gBattleMoves[move].target & (MOVE_TARGET_USER | MOVE_TARGET_OPPONENTS_FIELD))
            return AI_EFFECTIVENESS_1;
        if (AI_IsMoveBlockedByAbility(move, battlerAtk, battlerDef))
            return AI_EFFECTIVENESS_0;
        return AI_EFFECTIVENESS_1;
    }

    moveType = GetBattleMoveType(move, battlerAtk);

    if (AI_GetBattlerAbility(battlerDef) == ABILITY_LEVITATE && moveType == TYPE_GROUND)
        return AI_EFFECTIVENESS_0;
    if (AI_IsMoveBlockedByAbility(move, battlerAtk, battlerDef))
        return AI_EFFECTIVENESS_0;

    effectiveness = AI_GetTypeEffectiveness(moveType, battlerDef);

    if (AI_GetBattlerAbility(battlerDef) == ABILITY_WONDER_GUARD
     && gBattleMoves[move].power != 0
     && effectiveness <= AI_EFFECTIVENESS_1)
        return AI_EFFECTIVENESS_0;

    return effectiveness;
}

// ---------------------------------------------------------------------------
// Damage
// ---------------------------------------------------------------------------

// Damage after Cmd_typecalc's STAB and type multipliers, same rounding.
static s32 ApplyStabAndTypes(s32 damage, u8 moveType, u8 battlerAtk, u8 battlerDef)
{
    s32 i = 0;
    u8 type1 = gBattleMons[battlerDef].type1;
    u8 type2 = gBattleMons[battlerDef].type2;
    u8 multiplier;

    if (IS_BATTLER_OF_TYPE(battlerAtk, moveType))
        damage = damage * 15 / 10;

    while (TYPE_EFFECT_ATK_TYPE(i) != TYPE_ENDTABLE)
    {
        if (TYPE_EFFECT_ATK_TYPE(i) == TYPE_FORESIGHT)
        {
            if (gBattleMons[battlerDef].status2 & STATUS2_FORESIGHT)
                break;
            i += 3;
            continue;
        }
        if (TYPE_EFFECT_ATK_TYPE(i) == moveType)
        {
            multiplier = TYPE_EFFECT_MULTIPLIER(i);
            if (TYPE_EFFECT_DEF_TYPE(i) == type1)
            {
                damage = damage * multiplier / 10;
                if (damage == 0 && multiplier != 0)
                    damage = 1;
            }
            if (TYPE_EFFECT_DEF_TYPE(i) == type2 && type1 != type2)
            {
                damage = damage * multiplier / 10;
                if (damage == 0 && multiplier != 0)
                    damage = 1;
            }
        }
        i += 3;
    }
    return damage;
}

// One hit's damage before the 85-100% roll, following Cmd_damagecalc then
// Cmd_typecalc. The battle state must already be saved by the caller.
static s32 CalcHitDamageBeforeRoll(u16 move, u8 battlerAtk, u8 battlerDef, u16 power, u8 moveType, u8 dmgMultiplier, bool32 isCrit)
{
    struct BattlePokemon attacker = gBattleMons[battlerAtk];
    struct BattlePokemon defender = gBattleMons[battlerDef];
    u16 powerOverride = 0;
    u8 typeOverride = 0;
    s32 damage;

    // The AI's view of abilities and held items.
    attacker.ability = AI_GetBattlerAbility(battlerAtk);
    attacker.item = AI_GetBattlerItem(battlerAtk);
    defender.ability = AI_GetBattlerAbility(battlerDef);
    defender.item = AI_GetBattlerItem(battlerDef);

    if (power != gBattleMoves[move].power)
        powerOverride = power;
    if (moveType != gBattleMoves[move].type)
        typeOverride = moveType | F_DYNAMIC_TYPE_1 | F_DYNAMIC_TYPE_2;

    gCurrentMove = move; // Explosion and Solar Beam are checked through gCurrentMove
    gBattlerAttacker = battlerAtk;
    gBattlerTarget = battlerDef;
    gCritMultiplier = isCrit ? 2 : 1;

    damage = CalculateBaseDamage(&attacker, &defender, move, gSideStatuses[GET_BATTLER_SIDE(battlerDef)],
                                 powerOverride, typeOverride, battlerAtk, battlerDef);
    damage = damage * gCritMultiplier * dmgMultiplier;

    if ((gStatuses3[battlerAtk] & STATUS3_CHARGED_UP) && gBattleMoves[move].type == TYPE_ELECTRIC)
        damage *= 2;
    if (gProtectStructs[battlerAtk].helpingHand)
        damage = damage * 15 / 10;

    if (move != MOVE_STRUGGLE)
        damage = ApplyStabAndTypes(damage, moveType, battlerAtk, battlerDef);

    return damage;
}

// Same as ApplyRandomDmgMultiplier with a fixed roll.
static s32 ApplyRoll(s32 damage, u32 rollPercent)
{
    if (damage == 0)
        return 0;
    damage = damage * rollPercent / 100;
    if (damage == 0)
        damage = 1;
    return damage;
}

static u16 ClampDamage(s32 damage)
{
    if (damage < 0)
        return 0;
    if (damage > 0xFFFF)
        return 0xFFFF;
    return damage;
}

static void SetSimulatedDamage(struct SimulatedDamage *dmg, s32 minimum, s32 median, s32 maximum)
{
    dmg->minimum = ClampDamage(minimum);
    dmg->median = ClampDamage(median);
    dmg->maximum = ClampDamage(maximum);
}

// Damage from moves that ignore the damage formula. Returns FALSE for other moves.
static bool32 TryCalcFixedDamage(u16 move, u8 battlerAtk, u8 battlerDef, struct SimulatedDamage *dmg)
{
    u8 level = gBattleMons[battlerAtk].level;
    s32 fixed;

    switch (gBattleMoves[move].effect)
    {
    case EFFECT_LEVEL_DAMAGE:
        fixed = level;
        break;
    case EFFECT_DRAGON_RAGE:
        fixed = 40;
        break;
    case EFFECT_SONICBOOM:
        fixed = 20;
        break;
    case EFFECT_SUPER_FANG:
        fixed = gBattleMons[battlerDef].hp / 2;
        if (fixed == 0)
            fixed = 1;
        break;
    case EFFECT_PSYWAVE: // level * (50% to 150%), see Cmd_psywavedamageeffect
        SetSimulatedDamage(dmg, level * 50 / 100, level, level * 150 / 100);
        return TRUE;
    case EFFECT_ENDEAVOR:
        if (gBattleMons[battlerDef].hp > gBattleMons[battlerAtk].hp)
            fixed = gBattleMons[battlerDef].hp - gBattleMons[battlerAtk].hp;
        else
            fixed = 0;
        break;
    case EFFECT_OHKO:
        if (level >= gBattleMons[battlerDef].level && AI_GetBattlerAbility(battlerDef) != ABILITY_STURDY)
            fixed = gBattleMons[battlerDef].hp;
        else
            fixed = 0;
        break;
    case EFFECT_COUNTER:     // depend on damage taken this turn, scored by the AI logic itself
    case EFFECT_MIRROR_COAT:
    case EFFECT_BIDE:
    case EFFECT_FUTURE_SIGHT: // hits two turns later
        fixed = 0;
        break;
    default:
        return FALSE;
    }

    SetSimulatedDamage(dmg, fixed, fixed, fixed);
    return TRUE;
}

// Flail and Reversal, see Cmd_remaininghptopower.
static u16 GetFlailPower(u8 battler)
{
    s32 i;
    s32 hpFraction = GetScaledHPFraction(gBattleMons[battler].hp, gBattleMons[battler].maxHP, 48);

    for (i = 0; i < FLAIL_TABLE_SIZE - 2; i += 2)
    {
        if (hpFraction <= sFlailHpScaleToPowerTable[i])
            break;
    }
    return sFlailHpScaleToPowerTable[i + 1];
}

// Low Kick, see Cmd_weightdamagecalculation.
static u16 GetLowKickPower(u8 battlerDef)
{
    s32 i;
    u16 weight = GetPokedexHeightWeight(SpeciesToNationalPokedexNum(gBattleMons[battlerDef].species), 1);

    for (i = 0; sWeightToDamageTable[i] != 0xFFFF; i += 2)
    {
        if (sWeightToDamageTable[i] > weight)
            break;
    }
    if (sWeightToDamageTable[i] != 0xFFFF)
        return sWeightToDamageTable[i + 1];
    return 120;
}

// Rollout and Ice Ball, the power of the next hit. See Cmd_rolloutdamagecalculation.
static u16 GetRolloutPower(u16 move, u8 battlerAtk)
{
    u16 power = gBattleMoves[move].power;

    if (gBattleMons[battlerAtk].status2 & STATUS2_MULTIPLETURNS)
        power <<= (5 - gDisableStructs[battlerAtk].rolloutTimer);
    if (gBattleMons[battlerAtk].status2 & STATUS2_DEFENSE_CURL)
        power *= 2;
    return power;
}

// Fury Cutter, the power of the next hit. See Cmd_furycuttercalc.
static u16 GetFuryCutterPower(u16 move, u8 battlerAtk)
{
    u8 counter = gDisableStructs[battlerAtk].furyCutterCounter;

    if (counter != 5)
        counter++;
    return gBattleMoves[move].power << (counter - 1);
}

// Power, extra multiplier and number of hits of a damaging move in the current
// situation. Power for random moves (Magnitude, Present) is their average.
static u16 GetMovePowerForCalc(u16 move, u8 battlerAtk, u8 battlerDef, u8 *dmgMultiplier)
{
    struct BattlePokemon *atk = &gBattleMons[battlerAtk];
    u16 power = gBattleMoves[move].power;

    *dmgMultiplier = 1;

    switch (gBattleMoves[move].effect)
    {
    case EFFECT_FLAIL:
        power = GetFlailPower(battlerAtk);
        break;
    case EFFECT_RETURN:
        power = 10 * atk->friendship / 25;
        break;
    case EFFECT_FRUSTRATION:
        power = 10 * (255 - atk->friendship) / 25;
        break;
    case EFFECT_HIDDEN_POWER:
        power = GetHiddenPowerPower(battlerAtk);
        break;
    case EFFECT_WEATHER_BALL:
        if (AI_WeatherHasEffect() && (gBattleWeather & B_WEATHER_ANY))
            *dmgMultiplier = 2;
        break;
    case EFFECT_ERUPTION:
        power = atk->hp * power / atk->maxHP;
        if (power == 0)
            power = 1;
        break;
    case EFFECT_LOW_KICK:
        power = GetLowKickPower(battlerDef);
        break;
    case EFFECT_MAGNITUDE:
        power = 70;
        if (gStatuses3[battlerDef] & STATUS3_UNDERGROUND)
            *dmgMultiplier = 2;
        break;
    case EFFECT_EARTHQUAKE:
        if (gStatuses3[battlerDef] & STATUS3_UNDERGROUND)
            *dmgMultiplier = 2;
        break;
    case EFFECT_PRESENT:
        power = 80;
        break;
    case EFFECT_ROLLOUT:
        power = GetRolloutPower(move, battlerAtk);
        break;
    case EFFECT_FURY_CUTTER:
        power = GetFuryCutterPower(move, battlerAtk);
        break;
    case EFFECT_FACADE:
        if (atk->status1 & (STATUS1_POISON | STATUS1_BURN | STATUS1_PARALYSIS | STATUS1_TOXIC_POISON))
            *dmgMultiplier = 2;
        break;
    case EFFECT_SMELLINGSALT:
        if (gBattleMons[battlerDef].status1 & STATUS1_PARALYSIS)
            *dmgMultiplier = 2;
        break;
    case EFFECT_FLINCH_MINIMIZE_HIT:
        if (gStatuses3[battlerDef] & STATUS3_MINIMIZED)
            *dmgMultiplier = 2;
        break;
    case EFFECT_GUST:
    case EFFECT_TWISTER:
        if (gStatuses3[battlerDef] & STATUS3_ON_AIR)
            *dmgMultiplier = 2;
        break;
    case EFFECT_HIT:
        if (move == MOVE_SURF && (gStatuses3[battlerDef] & STATUS3_UNDERWATER))
            *dmgMultiplier = 2;
        break;
    case EFFECT_TRAP:
        if (move == MOVE_WHIRLPOOL && (gStatuses3[battlerDef] & STATUS3_UNDERWATER))
            *dmgMultiplier = 2;
        break;
    }
    return power;
}

// Beat Up: one typeless hit per healthy party member, see Cmd_trydobeatup.
static void CalcBeatUpDamage(u16 move, u8 battlerAtk, u8 battlerDef, bool32 isCrit, struct SimulatedDamage *dmg)
{
    struct Pokemon *party = (GetBattlerSide(battlerAtk) == B_SIDE_PLAYER) ? gPlayerParty : gEnemyParty;
    s32 i, hit, minimum = 0, median = 0, maximum = 0;
    u16 species;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        species = GetMonData(&party[i], MON_DATA_SPECIES_OR_EGG);
        if (GetMonData(&party[i], MON_DATA_HP) == 0 || species == SPECIES_NONE || species == SPECIES_EGG
         || GetMonData(&party[i], MON_DATA_STATUS) != 0)
            continue;

        hit = gSpeciesInfo[species].baseAttack;
        hit *= gBattleMoves[move].power;
        hit *= (GetMonData(&party[i], MON_DATA_LEVEL) * 2 / 5 + 2);
        hit /= gSpeciesInfo[gBattleMons[battlerDef].species].baseDefense;
        hit = (hit / 50) + 2;
        if (gProtectStructs[battlerAtk].helpingHand)
            hit = hit * 15 / 10;
        if (isCrit)
            hit *= 2;

        minimum += ApplyRoll(hit, MIN_ROLL_PERCENTAGE);
        median += ApplyRoll(hit, DMG_ROLL_PERCENTAGE);
        maximum += hit;
    }
    SetSimulatedDamage(dmg, minimum, median, maximum);
}

// Simulated damage of a move. Multi-hit moves count every hit: the minimum
// assumes the fewest hits, the median the average, the maximum the most.
// Nothing in the battle changes.
struct SimulatedDamage AI_CalcDamage(u16 move, u8 battlerAtk, u8 battlerDef, uq4_12_t *effectiveness, bool32 isCrit)
{
    struct SimulatedDamage dmg = {0, 0, 0};
    struct AiSavedBattleState saved;
    uq4_12_t moveEffectiveness;
    u8 effect = gBattleMoves[move].effect;
    u8 moveType, dmgMultiplier;
    u16 power;
    s32 hit, minimum, median, maximum;

    SaveBattleState(&saved);

    moveEffectiveness = AI_GetMoveEffectiveness(move, battlerAtk, battlerDef);
    if (effectiveness != NULL)
        *effectiveness = moveEffectiveness;

    if (move == MOVE_NONE || gBattleMoves[move].power == 0 || moveEffectiveness == AI_EFFECTIVENESS_0)
    {
        RestoreBattleState(&saved);
        return dmg;
    }

    if (TryCalcFixedDamage(move, battlerAtk, battlerDef, &dmg))
    {
        RestoreBattleState(&saved);
        return dmg;
    }

    moveType = GetBattleMoveType(move, battlerAtk);

    switch (effect)
    {
    case EFFECT_BEAT_UP:
        CalcBeatUpDamage(move, battlerAtk, battlerDef, isCrit, &dmg);
        break;
    case EFFECT_TRIPLE_KICK: // three hits of 10, 20 and 30 power
        minimum = median = maximum = 0;
        for (power = 10; power <= 30; power += 10)
        {
            hit = CalcHitDamageBeforeRoll(move, battlerAtk, battlerDef, power, moveType, 1, isCrit);
            minimum += ApplyRoll(hit, MIN_ROLL_PERCENTAGE);
            median += ApplyRoll(hit, DMG_ROLL_PERCENTAGE);
            maximum += hit;
        }
        SetSimulatedDamage(&dmg, minimum, median, maximum);
        break;
    case EFFECT_SPIT_UP: // stockpile multiplies the damage, no random roll
        if (gDisableStructs[battlerAtk].stockpileCounter == 0)
            break;
        gCurrentMove = move;
        gBattlerAttacker = battlerAtk;
        gBattlerTarget = battlerDef;
        gCritMultiplier = 1;
        {
            struct BattlePokemon attacker = gBattleMons[battlerAtk];
            struct BattlePokemon defender = gBattleMons[battlerDef];

            attacker.ability = AI_GetBattlerAbility(battlerAtk);
            attacker.item = AI_GetBattlerItem(battlerAtk);
            defender.ability = AI_GetBattlerAbility(battlerDef);
            defender.item = AI_GetBattlerItem(battlerDef);
            hit = CalculateBaseDamage(&attacker, &defender, move, gSideStatuses[GET_BATTLER_SIDE(battlerDef)],
                                      0, 0, battlerAtk, battlerDef) * gDisableStructs[battlerAtk].stockpileCounter;
        }
        if (gProtectStructs[battlerAtk].helpingHand)
            hit = hit * 15 / 10;
        hit = ApplyStabAndTypes(hit, moveType, battlerAtk, battlerDef);
        SetSimulatedDamage(&dmg, hit, hit, hit);
        break;
    default:
        power = GetMovePowerForCalc(move, battlerAtk, battlerDef, &dmgMultiplier);
        hit = CalcHitDamageBeforeRoll(move, battlerAtk, battlerDef, power, moveType, dmgMultiplier, isCrit);
        minimum = ApplyRoll(hit, MIN_ROLL_PERCENTAGE);
        median = ApplyRoll(hit, DMG_ROLL_PERCENTAGE);
        maximum = hit;

        switch (effect)
        {
        case EFFECT_MULTI_HIT: // 2 to 5 hits
            minimum *= 2;
            median *= 3;
            maximum *= 5;
            break;
        case EFFECT_DOUBLE_HIT:
        case EFFECT_TWINEEDLE:
            minimum *= 2;
            median *= 2;
            maximum *= 2;
            break;
        }
        SetSimulatedDamage(&dmg, minimum, median, maximum);
        break;
    }

    RestoreBattleState(&saved);
    return dmg;
}

// Expected damage of a move slot, from the per-decision cache when it is ready.
// A risky AI expects high rolls on its attacks and low rolls on the attacks it
// takes. Everyone else expects the median roll.
u32 AI_GetDamage(u8 battlerAtk, u8 battlerDef, u8 moveIndex, u8 calcContext)
{
    struct SimulatedDamage dmg;
    bool32 risky = (gBattleResources != NULL && gBattleResources->ai != NULL
                 && (gBattleResources->ai->aiLogicFlags & AI_LOGIC_RISKY));

    if (AiKnowledgeReady())
        dmg = AI_LOGIC_DATA->simulatedDmg[battlerAtk][battlerDef][moveIndex];
    else
        dmg = AI_CalcDamage(AI_GetMovesArray(battlerAtk)[moveIndex], battlerAtk, battlerDef, NULL, FALSE);

    if (risky && calcContext == AI_ATTACKING && GetBattlerSide(battlerAtk) == B_SIDE_OPPONENT)
        return dmg.maximum;
    if (risky && calcContext == AI_DEFENDING && GetBattlerSide(battlerDef) == B_SIDE_OPPONENT)
        return dmg.minimum;
    return dmg.median;
}

u32 GetNoOfHitsToKO(u32 dmg, s32 hp)
{
    if (dmg == 0)
        return 0;
    return hp / dmg + (hp % dmg != 0);
}

u32 GetNoOfHitsToKOBattler(u8 battlerAtk, u8 battlerDef, u8 moveIndex, u8 calcContext, u8 considerEndure)
{
    return GetNoOfHitsToKO(AI_GetDamage(battlerAtk, battlerDef, moveIndex, calcContext), gBattleMons[battlerDef].hp);
}

// ---------------------------------------------------------------------------
// Accuracy (Cmd_accuracycheck and AccuracyCalcHelper)
// ---------------------------------------------------------------------------

bool32 AI_CanHitSemiInvulnerable(u16 move, u8 battlerDef)
{
    u8 effect = gBattleMoves[move].effect;

    if (gStatuses3[battlerDef] & STATUS3_ON_AIR)
        return effect == EFFECT_GUST || effect == EFFECT_TWISTER || effect == EFFECT_THUNDER || effect == EFFECT_SKY_UPPERCUT;
    if (gStatuses3[battlerDef] & STATUS3_UNDERGROUND)
        return effect == EFFECT_EARTHQUAKE || effect == EFFECT_MAGNITUDE;
    if (gStatuses3[battlerDef] & STATUS3_UNDERWATER)
        return move == MOVE_SURF || move == MOVE_WHIRLPOOL;
    return TRUE;
}

// Moves that hit without an accuracy roll (unless the target is out of reach).
static bool32 MoveSkipsAccuracyCheck(u16 move, u8 battlerAtk, u8 battlerDef)
{
    u8 effect = gBattleMoves[move].effect;

    if (!AI_CanHitSemiInvulnerable(move, battlerDef))
        return FALSE;
    if ((gStatuses3[battlerDef] & STATUS3_ALWAYS_HITS) && gDisableStructs[battlerDef].battlerWithSureHit == battlerAtk)
        return TRUE;
    if (effect == EFFECT_OHKO)
        return FALSE;
    if (effect == EFFECT_ALWAYS_HIT || effect == EFFECT_VITAL_THROW || gBattleMoves[move].accuracy == 0)
        return TRUE;
    if (effect == EFFECT_THUNDER && AI_WeatherHasEffect() && (gBattleWeather & B_WEATHER_RAIN))
        return TRUE;
    return FALSE;
}

u8 AI_GetMoveAccuracy(u8 battlerAtk, u8 battlerDef, u16 move)
{
    u8 effect = gBattleMoves[move].effect;
    u8 moveAcc = gBattleMoves[move].accuracy;
    u8 holdEffect, param;
    s8 buff;
    u32 calc;

    if ((gStatuses3[battlerDef] & STATUS3_ALWAYS_HITS) && gDisableStructs[battlerDef].battlerWithSureHit == battlerAtk)
        return 100;
    if (!AI_CanHitSemiInvulnerable(move, battlerDef))
        return 0;
    if (effect == EFFECT_ALWAYS_HIT || effect == EFFECT_VITAL_THROW || moveAcc == 0)
        return 100;
    if (effect == EFFECT_THUNDER && AI_WeatherHasEffect() && (gBattleWeather & B_WEATHER_RAIN))
        return 100;

    if (effect == EFFECT_OHKO) // see Cmd_tryKO
    {
        if (gBattleMons[battlerAtk].level < gBattleMons[battlerDef].level)
            return 0;
        calc = moveAcc + (gBattleMons[battlerAtk].level - gBattleMons[battlerDef].level);
        return calc > 100 ? 100 : calc;
    }

    if (gBattleMons[battlerDef].status2 & STATUS2_FORESIGHT)
        buff = gBattleMons[battlerAtk].statStages[STAT_ACC];
    else
        buff = gBattleMons[battlerAtk].statStages[STAT_ACC] + DEFAULT_STAT_STAGE - gBattleMons[battlerDef].statStages[STAT_EVASION];
    if (buff < MIN_STAT_STAGE)
        buff = MIN_STAT_STAGE;
    if (buff > MAX_STAT_STAGE)
        buff = MAX_STAT_STAGE;

    if (effect == EFFECT_THUNDER && AI_WeatherHasEffect() && (gBattleWeather & B_WEATHER_SUN))
        moveAcc = 50;

    calc = sAccuracyStageRatios[buff].dividend * moveAcc;
    calc /= sAccuracyStageRatios[buff].divisor;

    if (AI_GetBattlerAbility(battlerAtk) == ABILITY_COMPOUND_EYES)
        calc = (calc * 130) / 100;
    if (AI_GetBattlerAbility(battlerDef) == ABILITY_SAND_VEIL && AI_WeatherHasEffect() && (gBattleWeather & B_WEATHER_SANDSTORM))
        calc = (calc * 80) / 100;
    if (AI_GetBattlerAbility(battlerAtk) == ABILITY_HUSTLE && IS_TYPE_PHYSICAL(GetBattleMoveType(move, battlerAtk)))
        calc = (calc * 80) / 100;

    holdEffect = AI_GetBattlerHoldEffect(battlerDef);
    param = AI_GetBattlerHoldEffectParam(battlerDef);
    if (holdEffect == HOLD_EFFECT_EVASION_UP)
        calc = (calc * (100 - param)) / 100;

    return calc > 100 ? 100 : calc;
}

// ---------------------------------------------------------------------------
// Speed and turn order (GetWhoStrikesFirst without Quick Claw)
// ---------------------------------------------------------------------------

u32 AI_GetBattlerSpeed(u8 battler)
{
    u8 ability = AI_GetBattlerAbility(battler);
    u8 holdEffect = AI_GetBattlerHoldEffect(battler);
    u32 multiplier = 1;
    u32 speed;

    if (AI_WeatherHasEffect()
     && ((ability == ABILITY_SWIFT_SWIM && (gBattleWeather & B_WEATHER_RAIN))
      || (ability == ABILITY_CHLOROPHYLL && (gBattleWeather & B_WEATHER_SUN))))
        multiplier = 2;

    speed = (gBattleMons[battler].speed * multiplier)
          * (gStatStageRatios[gBattleMons[battler].statStages[STAT_SPEED]][0])
          / (gStatStageRatios[gBattleMons[battler].statStages[STAT_SPEED]][1]);

    if (!(gBattleTypeFlags & BATTLE_TYPE_LINK) && FlagGet(FLAG_BADGE03_GET) && GetBattlerSide(battler) == B_SIDE_PLAYER)
        speed = (speed * 110) / 100;
    if (holdEffect == HOLD_EFFECT_MACHO_BRACE)
        speed /= 2;
    if (gBattleMons[battler].status1 & STATUS1_PARALYSIS)
        speed /= 4;

    return speed;
}

// Whether the AI's battler moves before the other one with these moves.
// Pass MOVE_NONE for a battler that is not using a move. A speed tie counts
// as slower, so the AI never relies on winning it.
s32 AI_WhoStrikesFirst(u8 battlerAi, u8 battler, u16 aiMove, u16 otherMove, bool32 considerPriority)
{
    u32 speedAi, speedOther;

    if (considerPriority)
    {
        s8 priorityAi = gBattleMoves[aiMove].priority;
        s8 priorityOther = gBattleMoves[otherMove].priority;

        if (priorityAi > priorityOther)
            return AI_IS_FASTER;
        if (priorityAi < priorityOther)
            return AI_IS_SLOWER;
    }

    if (AiKnowledgeReady())
    {
        speedAi = AI_LOGIC_DATA->speedStats[battlerAi];
        speedOther = AI_LOGIC_DATA->speedStats[battler];
    }
    else
    {
        speedAi = AI_GetBattlerSpeed(battlerAi);
        speedOther = AI_GetBattlerSpeed(battler);
    }

    if (speedAi > speedOther)
        return AI_IS_FASTER;
    return AI_IS_SLOWER;
}

bool32 AI_IsFaster(u8 battlerAi, u8 battler, u16 aiMove, u16 otherMove, bool32 considerPriority)
{
    return AI_WhoStrikesFirst(battlerAi, battler, aiMove, otherMove, considerPriority) == AI_IS_FASTER;
}

bool32 AI_IsSlower(u8 battlerAi, u8 battler, u16 aiMove, u16 otherMove, bool32 considerPriority)
{
    return AI_WhoStrikesFirst(battlerAi, battler, aiMove, otherMove, considerPriority) == AI_IS_SLOWER;
}

// ---------------------------------------------------------------------------
// Per-decision cache
// ---------------------------------------------------------------------------

void AI_ClearLogicData(void)
{
    if (gBattleResources != NULL && AI_LOGIC_DATA != NULL)
        AI_LOGIC_DATA->ready = FALSE;
}

// What the AI knows about one battler: ability, item, HP, speed and so on.
static void SetBattlerLogicData(u8 battler)
{
    struct AiLogicData *data = AI_LOGIC_DATA;

    data->abilities[battler] = GetKnownAbility(battler);
    data->items[battler] = GetKnownItem(battler);
    data->holdEffects[battler] = GetHoldEffectOfItem(battler, data->items[battler]);
    data->holdEffectParams[battler] = GetHoldEffectParamOfItem(battler, data->items[battler]);
    // gLastMoves only holds moves from earlier turns while the AI decides.
    data->lastUsedMove[battler] = (gLastMoves[battler] == MOVE_UNAVAILABLE) ? MOVE_NONE : gLastMoves[battler];
    SetKnownMoves(battler);
}

static void SetBattlerStateData(u8 battler)
{
    struct AiLogicData *data = AI_LOGIC_DATA;

    if (IsBattlerAlive(battler))
    {
        data->hpPercents[battler] = gBattleMons[battler].hp * 100 / gBattleMons[battler].maxHP;
        if (AI_KnowsBattler(battler))
            data->moveLimitations[battler] = CheckMoveLimitations(battler, 0, 0xFF);
        else
            data->moveLimitations[battler] = GetVisibleMoveLimitations(battler);
        data->speedStats[battler] = AI_GetBattlerSpeed(battler);
    }
    else
    {
        data->hpPercents[battler] = 0;
        data->moveLimitations[battler] = 0;
        data->speedStats[battler] = 0;
    }
}

// Damage, effectiveness and accuracy of the attacker's moves on the target.
static void CalcBattlerMovesData(u8 atk, u8 def)
{
    struct AiLogicData *data = AI_LOGIC_DATA;
    u16 move;
    u8 i;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        data->simulatedDmg[atk][def][i].minimum = 0;
        data->simulatedDmg[atk][def][i].median = 0;
        data->simulatedDmg[atk][def][i].maximum = 0;
        data->effectiveness[atk][def][i] = AI_EFFECTIVENESS_0;
        data->moveAccuracy[atk][def][i] = 0;

        if (atk == def || !IsBattlerAlive(atk) || !IsBattlerAlive(def))
            continue;
        move = AI_GetMovesArray(atk)[i];
        if (move == MOVE_NONE)
            continue;

        data->simulatedDmg[atk][def][i] = AI_CalcDamage(move, atk, def, &data->effectiveness[atk][def][i], FALSE);
        if (MoveSkipsAccuracyCheck(move, atk, def))
            data->moveAccuracy[atk][def][i] = AI_ACCURACY_SURE_HIT;
        else
            data->moveAccuracy[atk][def][i] = AI_GetMoveAccuracy(atk, def, move);
    }
}

// Fills the AI's view of the battle once per decision: what it believes each
// battler's ability and item are, then damage, effectiveness and accuracy of
// every move slot against every other battler.
void AI_SetLogicDataForTurn(u8 battlerAi)
{
    u8 atk, def, i;

    if (AI_LOGIC_DATA == NULL)
        return;

    AI_LOGIC_DATA->ready = FALSE;
    AI_LOGIC_DATA->knowledgeFlags = BattleAI_GetKnowledgeFlags();
    AI_LOGIC_DATA->predictingMove = FALSE;   // AI_SetupPredictions turns them on
    AI_LOGIC_DATA->predictingSwitch = FALSE;
    RecordWhatTheAiSees();
    for (i = 0; i < MAX_BATTLERS_COUNT; i++)
        SetBattlerLogicData(i);
    AI_LOGIC_DATA->ready = TRUE;

    for (i = 0; i < MAX_BATTLERS_COUNT; i++)
        SetBattlerStateData(i);

    for (atk = 0; atk < MAX_BATTLERS_COUNT; atk++)
    {
        for (def = 0; def < MAX_BATTLERS_COUNT; def++)
            CalcBattlerMovesData(atk, def);
    }
}

// ---------------------------------------------------------------------------
// Switch-in candidates: put a party mon in a battler's place to see how it
// would do. Everything is restored afterwards.
// ---------------------------------------------------------------------------

// Puts back a saved AI view, keeping what the AI has learned since.
static void RestoreLogicData(const struct AiLogicData *saved)
{
    struct AiPartyKnowledge playerParty[PARTY_SIZE];

    memcpy(playerParty, AI_LOGIC_DATA->playerParty, sizeof(playerParty));
    *AI_LOGIC_DATA = *saved;
    memcpy(AI_LOGIC_DATA->playerParty, playerParty, sizeof(playerParty));
}

struct AiSwitchinBackup
{
    struct AiLogicData logicData;
    struct BattlePokemon battleMon;
    struct DisableStruct disableStruct;
    u32 statuses3;
    u16 choicedMove;
    u8 partyIndex;
    u8 absentBattlerFlags;
};

// The battle data a party mon gets when it switches in (Cmd_switchindataupdate).
static void PartyMonToBattleMon(struct Pokemon *mon, struct BattlePokemon *bmon)
{
    s32 i;

    bmon->species = GetMonData(mon, MON_DATA_SPECIES);
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        bmon->moves[i] = GetMonData(mon, MON_DATA_MOVE1 + i);
        bmon->pp[i] = GetMonData(mon, MON_DATA_PP1 + i);
    }
    bmon->ppBonuses = GetMonData(mon, MON_DATA_PP_BONUSES);
    bmon->friendship = GetMonData(mon, MON_DATA_FRIENDSHIP);
    bmon->experience = GetMonData(mon, MON_DATA_EXP);
    bmon->hpIV = GetMonData(mon, MON_DATA_HP_IV);
    bmon->attackIV = GetMonData(mon, MON_DATA_ATK_IV);
    bmon->defenseIV = GetMonData(mon, MON_DATA_DEF_IV);
    bmon->speedIV = GetMonData(mon, MON_DATA_SPEED_IV);
    bmon->spAttackIV = GetMonData(mon, MON_DATA_SPATK_IV);
    bmon->spDefenseIV = GetMonData(mon, MON_DATA_SPDEF_IV);
    bmon->personality = GetMonData(mon, MON_DATA_PERSONALITY);
    bmon->status1 = GetMonData(mon, MON_DATA_STATUS);
    bmon->level = GetMonData(mon, MON_DATA_LEVEL);
    bmon->hp = GetMonData(mon, MON_DATA_HP);
    bmon->maxHP = GetMonData(mon, MON_DATA_MAX_HP);
    bmon->attack = GetMonData(mon, MON_DATA_ATK);
    bmon->defense = GetMonData(mon, MON_DATA_DEF);
    bmon->speed = GetMonData(mon, MON_DATA_SPEED);
    bmon->spAttack = GetMonData(mon, MON_DATA_SPATK);
    bmon->spDefense = GetMonData(mon, MON_DATA_SPDEF);
    bmon->isEgg = GetMonData(mon, MON_DATA_IS_EGG);
    bmon->abilityNum = GetMonData(mon, MON_DATA_ABILITY_NUM);
    bmon->otId = GetMonData(mon, MON_DATA_OT_ID);
    bmon->item = GetMonData(mon, MON_DATA_HELD_ITEM);
    bmon->ability = GetAbilityBySpecies(bmon->species, bmon->abilityNum);
    bmon->type1 = gSpeciesInfo[bmon->species].types[0];
    bmon->type2 = gSpeciesInfo[bmon->species].types[1];
    for (i = 0; i < NUM_BATTLE_STATS; i++)
        bmon->statStages[i] = DEFAULT_STAT_STAGE;
    bmon->status2 = 0;
}

void *AI_BeginSwitchinSimulation(u8 battler)
{
    struct AiSwitchinBackup *backup = Alloc(sizeof(struct AiSwitchinBackup));

    if (backup == NULL)
        return NULL;
    backup->logicData = *AI_LOGIC_DATA;
    backup->battleMon = gBattleMons[battler];
    backup->disableStruct = gDisableStructs[battler];
    backup->statuses3 = gStatuses3[battler];
    backup->choicedMove = gBattleStruct->choicedMove[battler];
    backup->partyIndex = gBattlerPartyIndexes[battler];
    backup->absentBattlerFlags = gAbsentBattlerFlags;
    return backup;
}

// The AI's view of the battle with the party mon in the battler's place, as
// on the turn it switches in.
void AI_SetSwitchinCandidate(u8 battler, struct Pokemon *mon, u8 partyIndex)
{
    u8 i;

    PartyMonToBattleMon(mon, &gBattleMons[battler]);
    memset(&gDisableStructs[battler], 0, sizeof(struct DisableStruct));
    gStatuses3[battler] = 0;
    gBattleStruct->choicedMove[battler] = MOVE_NONE;
    gBattlerPartyIndexes[battler] = partyIndex;
    gAbsentBattlerFlags &= ~gBitTable[battler];

    SetBattlerLogicData(battler);
    AI_LOGIC_DATA->lastUsedMove[battler] = MOVE_NONE;
    SetBattlerStateData(battler);
    for (i = 0; i < MAX_BATTLERS_COUNT; i++)
    {
        if (i == battler || !IsBattlerAlive(i))
            continue;
        SetBattlerStateData(i); // speed comparisons can change
        CalcBattlerMovesData(battler, i);
        CalcBattlerMovesData(i, battler);
    }
}

void AI_EndSwitchinSimulation(u8 battler, void *backupPtr)
{
    struct AiSwitchinBackup *backup = backupPtr;

    if (backup == NULL)
        return;
    RestoreLogicData(&backup->logicData);
    gBattleMons[battler] = backup->battleMon;
    gDisableStructs[battler] = backup->disableStruct;
    gStatuses3[battler] = backup->statuses3;
    gBattleStruct->choicedMove[battler] = backup->choicedMove;
    gBattlerPartyIndexes[battler] = backup->partyIndex;
    gAbsentBattlerFlags = backup->absentBattlerFlags;
    Free(backup);
}

// ---------------------------------------------------------------------------
// Thinking as another battler, to predict what the player will do. The
// battler gets the moves, item and ability the AI believes it has, so the
// prediction can't use anything the AI doesn't know.
// ---------------------------------------------------------------------------

struct AiPlayAsBackup
{
    struct AiLogicData logicData;
    struct AI_ThinkingStruct thinking;
    struct BattlePokemon battleMon;
    u8 battler;
};

void *AI_BeginPlayAsBattler(u8 battler)
{
    struct AiPlayAsBackup *backup = Alloc(sizeof(struct AiPlayAsBackup));
    u16 *moves;
    u8 i;

    if (backup == NULL)
        return NULL;
    backup->logicData = *AI_LOGIC_DATA;
    backup->thinking = *gBattleResources->ai;
    backup->battleMon = gBattleMons[battler];
    backup->battler = battler;

    if (!AI_KnowsBattler(battler))
    {
        moves = AI_LOGIC_DATA->knownMoves[battler];
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            gBattleMons[battler].moves[i] = moves[i];
            gBattleMons[battler].pp[i] = (moves[i] != MOVE_NONE) ? 1 : 0; // PP the AI can't see
        }
        gBattleMons[battler].item = AI_LOGIC_DATA->items[battler];
        gBattleMons[battler].ability = AI_LOGIC_DATA->abilities[battler];
    }
    return backup;
}

void AI_EndPlayAsBattler(void *backupPtr)
{
    struct AiPlayAsBackup *backup = backupPtr;

    if (backup == NULL)
        return;
    RestoreLogicData(&backup->logicData);
    *gBattleResources->ai = backup->thinking;
    gBattleMons[backup->battler] = backup->battleMon;
    Free(backup);
}

// ---------------------------------------------------------------------------
// Debug checks: compare predictions with what really happens
// ---------------------------------------------------------------------------

#if AI_DEBUG_CHECKS

// Effects whose damage depends on state the battle changes right before the
// hit, or on a random roll other than the damage roll.
static bool32 IsDamageCheckSkipped(u8 effect)
{
    switch (effect)
    {
    case EFFECT_ROLLOUT:
    case EFFECT_FURY_CUTTER:
    case EFFECT_MAGNITUDE:
    case EFFECT_PRESENT:
    case EFFECT_TRIPLE_KICK:
    case EFFECT_BEAT_UP:
    case EFFECT_REVENGE:
    case EFFECT_PURSUIT:
    case EFFECT_FUTURE_SIGHT:
        return TRUE;
    }
    return FALSE;
}

// Called by adjustnormaldamage right after the random roll: gBattleMoveDamage
// is one real hit of gCurrentMove. Logs whether it is inside the AI's range.
void AI_DebugCheckDamage(void)
{
    struct AiSavedBattleState saved;
    bool8 wasReady = FALSE;
    u8 effect = gBattleMoves[gCurrentMove].effect;
    u8 atk = gBattlerAttacker, def = gBattlerTarget;
    u8 moveType, dmgMultiplier;
    u16 power;
    s32 real = gBattleMoveDamage, hit, minimum;

    // A confused battler hitting itself uses its own formula (atk == def).
    // A move that missed or had no effect (e.g. Levitate) deals no damage, even
    // though gBattleMoveDamage still holds a number.
    if (gCurrentMove == MOVE_NONE || atk == def || IsDamageCheckSkipped(effect)
     || (gMoveResultFlags & (MOVE_RESULT_MISSED | MOVE_RESULT_DOESNT_AFFECT_FOE | MOVE_RESULT_FAILED)))
    {
        DebugPrintfLevel(MGBA_LOG_INFO, "AIDMG SKIP move=%d", gCurrentMove);
        return;
    }

    // Check the formula with the real abilities and items.
    if (AiKnowledgeReady())
    {
        wasReady = TRUE;
        AI_LOGIC_DATA->ready = FALSE;
    }

    SaveBattleState(&saved);
    moveType = GetBattleMoveType(gCurrentMove, atk);
    power = GetMovePowerForCalc(gCurrentMove, atk, def, &dmgMultiplier);
    hit = CalcHitDamageBeforeRoll(gCurrentMove, atk, def, power, moveType, dmgMultiplier, saved.critMultiplier == 2);
    RestoreBattleState(&saved);

    if (wasReady)
        AI_LOGIC_DATA->ready = TRUE;

    minimum = ApplyRoll(hit, MIN_ROLL_PERCENTAGE);
    if (real >= minimum && real <= hit)
        DebugPrintfLevel(MGBA_LOG_INFO, "AIDMG OK move=%d atk=%d def=%d real=%d min=%d max=%d", gCurrentMove, atk, def, real, minimum, hit);
    else
        DebugPrintfLevel(MGBA_LOG_WARN, "AIDMG MISMATCH move=%d atk=%d def=%d real=%d min=%d max=%d crit=%d", gCurrentMove, atk, def, real, minimum, hit, saved.critMultiplier);
}

// Called once the turn order is set. Reads the moves both sides chose, which
// is fine for a debug check but never done by the AI itself.
void AI_DebugCheckTurnOrder(void)
{
    s32 i, j;
    u8 b1, b2;
    u16 move1, move2;
    s32 predicted;

    // The moves chosen this turn, to check the AI's predictions and knowledge.
    for (i = 0; i < gBattlersCount; i++)
    {
        if (gChosenActionByBattler[i] == B_ACTION_USE_MOVE && IsBattlerAlive(i))
            DebugPrintfLevel(MGBA_LOG_INFO, "AIMOVEUSED turn=%d battler=%d species=%d move=%d", gBattleResults.battleTurnCounter,
                i, gBattleMons[i].species, gBattleMons[i].moves[gBattleStruct->chosenMovePositions[i]]);
    }

    for (i = 0; i < gBattlersCount; i++)
    {
        for (j = i + 1; j < gBattlersCount; j++)
        {
            b1 = gBattlerByTurnOrder[i];
            b2 = gBattlerByTurnOrder[j];
            if (gChosenActionByBattler[b1] != B_ACTION_USE_MOVE || gChosenActionByBattler[b2] != B_ACTION_USE_MOVE)
                continue;
            if (!IsBattlerAlive(b1) || !IsBattlerAlive(b2))
                continue;
            if (GetHoldEffectOfItem(b1, gBattleMons[b1].item) == HOLD_EFFECT_QUICK_CLAW
             || GetHoldEffectOfItem(b2, gBattleMons[b2].item) == HOLD_EFFECT_QUICK_CLAW)
                continue;

            move1 = gBattleMons[b1].moves[gBattleStruct->chosenMovePositions[b1]];
            move2 = gBattleMons[b2].moves[gBattleStruct->chosenMovePositions[b2]];
            if (gBattleMoves[move1].priority == gBattleMoves[move2].priority
             && AI_GetBattlerSpeed(b1) == AI_GetBattlerSpeed(b2))
                continue; // speed tie, decided at random

            predicted = AI_WhoStrikesFirst(b1, b2, move1, move2, CONSIDER_PRIORITY);
            if (predicted == AI_IS_FASTER)
                DebugPrintfLevel(MGBA_LOG_INFO, "AIORDER OK first=%d second=%d", b1, b2);
            else
                DebugPrintfLevel(MGBA_LOG_WARN, "AIORDER MISMATCH first=%d second=%d spd=%d/%d", b1, b2, AI_GetBattlerSpeed(b1), AI_GetBattlerSpeed(b2));
        }
    }
}

#endif // AI_DEBUG_CHECKS
