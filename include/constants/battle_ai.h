#ifndef GUARD_CONSTANTS_BATTLE_AI_H
#define GUARD_CONSTANTS_BATTLE_AI_H

// battlers
#define AI_TARGET 0
#define AI_USER 1
#define AI_TARGET_PARTNER 2
#define AI_USER_PARTNER 3

// get_type command
#define AI_TYPE1_TARGET 0
#define AI_TYPE1_USER 1
#define AI_TYPE2_TARGET 2
#define AI_TYPE2_USER 3
#define AI_TYPE_MOVE 4

// type effectiveness
#define AI_EFFECTIVENESS_x4     160
#define AI_EFFECTIVENESS_x2     80
#define AI_EFFECTIVENESS_x1     40
#define AI_EFFECTIVENESS_x0_5   20
#define AI_EFFECTIVENESS_x0_25  10
#define AI_EFFECTIVENESS_x0     0

// ai weather
#define AI_WEATHER_SUN       0
#define AI_WEATHER_RAIN      1
#define AI_WEATHER_SANDSTORM 2
#define AI_WEATHER_HAIL      3

// get_how_powerful_move_is
#define MOVE_POWER_DISCOURAGED  0
#define MOVE_NOT_MOST_POWERFUL  1
#define MOVE_MOST_POWERFUL      2

// script's table id to bit
#define AI_SCRIPT_CHECK_BAD_MOVE (1 << 0)
#define AI_SCRIPT_CHECK_VIABILITY (1 << 1)
#define AI_SCRIPT_TRY_TO_FAINT (1 << 2)
#define AI_SCRIPT_SETUP_FIRST_TURN (1 << 3)
#define AI_SCRIPT_RISKY (1 << 4)
#define AI_SCRIPT_PREFER_STRONGEST_MOVE (1 << 5)
#define AI_SCRIPT_PREFER_BATON_PASS (1 << 6)
#define AI_SCRIPT_DOUBLE_BATTLE (1 << 7)
#define AI_SCRIPT_HP_AWARE (1 << 8)
#define AI_SCRIPT_UNKNOWN (1 << 9)
// 10 - 23 are not used
// 24 - 27 hold the trainer's AI level (see AI_LEVEL below)
// 28 is not used
#define AI_SCRIPT_ROAMING (1 << 29)
#define AI_SCRIPT_SAFARI (1 << 30)
#define AI_SCRIPT_FIRST_BATTLE (1 << 31)

// Trainer AI level, from 1 (weakest, like vanilla route trainers) to 10 (strongest).
// Set it in a trainer's aiFlags with AI_LEVEL(n), e.g. `.aiFlags = AI_LEVEL(5)`.
// The level decides which AI behaviour the trainer gets. It can be combined
// with AI_SCRIPT_* flags, which are then added on top of the level's behaviour.
// Trainers without a level keep their AI_SCRIPT_* flags exactly as given.
#define AI_LEVEL_SHIFT 24
#define AI_LEVEL_MASK  (0xF << AI_LEVEL_SHIFT)
#define AI_LEVEL_NONE  0
#define AI_LEVEL_MIN   1
#define AI_LEVEL_MAX   10
#define AI_LEVEL(n)    (((n) << AI_LEVEL_SHIFT) & AI_LEVEL_MASK)
#define GET_AI_LEVEL(flags) (((flags) & AI_LEVEL_MASK) >> AI_LEVEL_SHIFT)

// From this level on, moves are scored by the AI logic ported from
// pokefirered-expansion (src/battle_ai_main.c) instead of the vanilla scripts.
#define AI_LEVEL_SMART_SCORING 4

// Move scoring steps of the ported AI, run in this order (expansion's AI_FLAG_*).
// The trainer's level picks them; see SetAiFlagsAndLevel.
#define AI_LOGIC_CHECK_BAD_MOVE         (1 << 0)  // avoid moves that fail or do nothing
#define AI_LOGIC_TRY_TO_FAINT           (1 << 1)  // go for KOs, use priority when about to be KO'd
#define AI_LOGIC_CHECK_VIABILITY        (1 << 2)  // score move effects and pick the best damaging move
#define AI_LOGIC_FORCE_SETUP_FIRST_TURN (1 << 3)  // prefer setup moves on the first turn
#define AI_LOGIC_RISKY                  (1 << 4)  // prefer high-damage, risky moves; assumes high damage rolls
#define AI_LOGIC_TRY_TO_2HKO            (1 << 5)  // bonus for moves that KO in one or two hits
#define AI_LOGIC_PREFER_BATON_PASS      (1 << 6)  // set up stats, then Baton Pass
#define AI_LOGIC_DOUBLE_BATTLE          (1 << 7)  // work with the partner in double battles (added automatically there)
#define AI_LOGIC_HP_AWARE               (1 << 8)  // choose effects by its own and the target's HP
#define AI_LOGIC_PREDICT_SWITCH         (1 << 9)  // when it expects the player to switch, prefer moves that punish it

// Switching steps of the ported AI (src/battle_ai_switch.c), picked by level.
// Levels 1-4 use the vanilla switching.
#define AI_SWITCH_SMART_MON_CHOICES (1 << 0) // level 5+: send in the mon that wins the matchup
#define AI_SWITCH_SMART_SWITCHING   (1 << 1) // level 6+: switch out of bad matchups and bad situations
#define AI_SWITCH_ACE_POKEMON       (1 << 2) // level 8+: the last mon in the party comes out last

// What the ported AI knows about the player's side and what it predicts,
// picked by level (src/battle_ai_engine.c). Without AI_KNOW_OMNISCIENT it only
// knows the moves the player's mons have used, the abilities and items the
// battle has revealed, and the party mons it has seen.
#define AI_KNOW_ASSUME_STAB     (1 << 0) // level 8: expects a typical attack of each type on moves it hasn't seen
#define AI_KNOW_OMNISCIENT      (1 << 1) // level 9+: knows the player's moves, items, abilities and party
#define AI_PREDICT_MOVE         (1 << 2) // level 7+: predicts the player's move from what it knows
#define AI_PREDICT_SWITCH       (1 << 3) // level 7+: predicts when the player will switch
#define AI_PREDICT_INCOMING_MON (1 << 4) // level 10: scores its moves against the mon it expects to come in

#endif // GUARD_CONSTANTS_BATTLE_AI_H
