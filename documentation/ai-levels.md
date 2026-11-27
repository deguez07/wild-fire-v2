# Trainer AI levels

Every trainer has an AI level from 1 (weakest) to 10 (strongest), set in
`src/data/trainers.h`:

```c
.aiFlags = AI_LEVEL(5),
```

To change a trainer's difficulty, change the number. Extra `AI_SCRIPT_*` flags
can still be added on top, e.g. `AI_LEVEL(5) | AI_SCRIPT_PREFER_BATON_PASS`.
Trainers without `AI_LEVEL` keep their `AI_SCRIPT_*` flags exactly as before
(only unused trainers are left that way).

## What each level does

Levels 1-3 use the vanilla FireRed AI scripts. From level 4 on, moves are
scored by the AI logic ported from pokefirered-expansion (Gen 1-3 moves and
Gen 3 rules only), which is built on exact damage, speed and accuracy
estimates for every move.

| Level | Now (stage 4) | Planned |
|---|---|---|
| 1 | Avoids moves that would fail (vanilla route trainers) | Same |
| 2 | + goes for KOs | Same |
| 3 | + scores moves by usefulness (vanilla leaders) | Same |
| 4 | Ported move scoring: avoids useless moves, goes for KOs, picks the best damaging move, scores status and setup moves by the situation, considers its own and your HP | Same |
| 5 | + bonus for moves that KO in one or two hits; sends in the mon that wins the matchup (after a KO, or when it switches) | Same |
| 6 | + smart switching: leaves bad matchups, Perish Song, Yawn, bad status, Encore and stat drops; switches to absorbers and trappers | Same |
| 7 | Same as 6 | + predicts your moves and switches |
| 8 | + keeps its ace (last mon in the party) for last | + guesses your likely moves |
| 9 | Same as 8 | + knows your moves, items and abilities |
| 10 | Same as 8 | Everything, no deliberate mistakes |

Levels 1-4 switch like vanilla FireRed. Level 5 uses the expansion AI's basic
switching checks (Wonder Guard, Perish Song, Natural Cure, resisting the last
hit). In double battles, level 5+ picks switch-ins by type matchup and damage;
the full matchup check is for single battles.

From level 4 on, extra flags turn on more of the ported AI:
`AI_SCRIPT_SETUP_FIRST_TURN` (sets up on turn 1), `AI_SCRIPT_RISKY` (prefers
risky high-damage moves, assumes high rolls), `AI_SCRIPT_PREFER_STRONGEST_MOVE`
(the 2-hit KO bonus) and `AI_SCRIPT_PREFER_BATON_PASS` (sets up, then passes).

Until stage 5, the level 4+ AI sees your moves, held item and ability. Stage 5
limits that to levels 9-10. No level reads the move or switch you picked that
turn: the AI only uses the move you used last turn.

## How the starting levels were chosen

- Route and dungeon trainers by badges you normally have there: 0 badges = 2,
  1-2 = 3, 3-5 = 4, 6-7 = 5, Victory Road and post-game Sevii = 6.
- Gym trainers (and the Saffron Dojo): one above the route trainers of that point.
- Gym leaders: Brock and Misty 6, Surge, Erika and Koga 7, Sabrina, Blaine and Giovanni 8.
- Rival: Oak's Lab, Route 22 (first) and Cerulean 6; S.S. Anne and Pokémon Tower 7;
  Silph Co. and Route 22 (second) 8.
- Giovanni: Rocket Hideout 7, Silph Co. 8. Rocket Admins (Five Island) 7.
- Elite Four 9, Champion 10 (first battle and rematches).
- VS Seeker rematches: one above the first battle, at most 6.

## Trainers by level

| Level | Role | Trainer | Map |
|---|---|---|---|
| 10 | Champion | `TRAINER_CHAMPION_FIRST_BULBASAUR` | PokemonLeague_ChampionsRoom |
| 10 | Champion | `TRAINER_CHAMPION_FIRST_CHARMANDER` | PokemonLeague_ChampionsRoom |
| 10 | Champion | `TRAINER_CHAMPION_FIRST_SQUIRTLE` | PokemonLeague_ChampionsRoom |
| 10 | Champion | `TRAINER_CHAMPION_REMATCH_BULBASAUR` | PokemonLeague_ChampionsRoom |
| 10 | Champion | `TRAINER_CHAMPION_REMATCH_CHARMANDER` | PokemonLeague_ChampionsRoom |
| 10 | Champion | `TRAINER_CHAMPION_REMATCH_SQUIRTLE` | PokemonLeague_ChampionsRoom |
| 9 | Elite Four | `TRAINER_ELITE_FOUR_AGATHA` | PokemonLeague_AgathasRoom |
| 9 | Elite Four | `TRAINER_ELITE_FOUR_AGATHA_2` | PokemonLeague_AgathasRoom |
| 9 | Elite Four | `TRAINER_ELITE_FOUR_BRUNO` | PokemonLeague_BrunosRoom |
| 9 | Elite Four | `TRAINER_ELITE_FOUR_BRUNO_2` | PokemonLeague_BrunosRoom |
| 9 | Elite Four | `TRAINER_ELITE_FOUR_LANCE` | PokemonLeague_LancesRoom |
| 9 | Elite Four | `TRAINER_ELITE_FOUR_LANCE_2` | PokemonLeague_LancesRoom |
| 9 | Elite Four | `TRAINER_ELITE_FOUR_LORELEI` | PokemonLeague_LoreleisRoom |
| 9 | Elite Four | `TRAINER_ELITE_FOUR_LORELEI_2` | PokemonLeague_LoreleisRoom |
| 8 | Boss | `TRAINER_BOSS_GIOVANNI_2` | SilphCo_11F |
| 8 | Boss | `TRAINER_LEADER_GIOVANNI` | ViridianCity_Gym |
| 8 | Gym leader | `TRAINER_LEADER_BLAINE` | CinnabarIsland_Gym |
| 8 | Gym leader | `TRAINER_LEADER_SABRINA` | SaffronCity_Gym |
| 8 | Rival | `TRAINER_RIVAL_ROUTE22_LATE_BULBASAUR` | Route22 |
| 8 | Rival | `TRAINER_RIVAL_ROUTE22_LATE_CHARMANDER` | Route22 |
| 8 | Rival | `TRAINER_RIVAL_ROUTE22_LATE_SQUIRTLE` | Route22 |
| 8 | Rival | `TRAINER_RIVAL_SILPH_BULBASAUR` | SilphCo_7F |
| 8 | Rival | `TRAINER_RIVAL_SILPH_CHARMANDER` | SilphCo_7F |
| 8 | Rival | `TRAINER_RIVAL_SILPH_SQUIRTLE` | SilphCo_7F |
| 7 | Boss | `TRAINER_BOSS_GIOVANNI` | RocketHideout_B4F |
| 7 | Boss | `TRAINER_TEAM_ROCKET_ADMIN` | FiveIsland_RocketWarehouse |
| 7 | Boss | `TRAINER_TEAM_ROCKET_ADMIN_2` | FiveIsland_RocketWarehouse |
| 7 | Gym leader | `TRAINER_LEADER_ERIKA` | CeladonCity_Gym |
| 7 | Gym leader | `TRAINER_LEADER_KOGA` | FuchsiaCity_Gym |
| 7 | Gym leader | `TRAINER_LEADER_LT_SURGE` | VermilionCity_Gym |
| 7 | Rival | `TRAINER_RIVAL_POKEMON_TOWER_BULBASAUR` | PokemonTower_2F |
| 7 | Rival | `TRAINER_RIVAL_POKEMON_TOWER_CHARMANDER` | PokemonTower_2F |
| 7 | Rival | `TRAINER_RIVAL_POKEMON_TOWER_SQUIRTLE` | PokemonTower_2F |
| 7 | Rival | `TRAINER_RIVAL_SS_ANNE_BULBASAUR` | SSAnne_2F_Corridor |
| 7 | Rival | `TRAINER_RIVAL_SS_ANNE_CHARMANDER` | SSAnne_2F_Corridor |
| 7 | Rival | `TRAINER_RIVAL_SS_ANNE_SQUIRTLE` | SSAnne_2F_Corridor |
| 6 | Gym leader | `TRAINER_LEADER_BROCK` | PewterCity_Gym |
| 6 | Gym leader | `TRAINER_LEADER_MISTY` | CeruleanCity_Gym |
| 6 | Gym trainer | `TRAINER_BLACK_BELT_ATSUSHI` | ViridianCity_Gym |
| 6 | Gym trainer | `TRAINER_BLACK_BELT_KIYO` | ViridianCity_Gym |
| 6 | Gym trainer | `TRAINER_BLACK_BELT_TAKASHI` | ViridianCity_Gym |
| 6 | Gym trainer | `TRAINER_BURGLAR_DUSTY` | CinnabarIsland_Gym |
| 6 | Gym trainer | `TRAINER_BURGLAR_QUINN` | CinnabarIsland_Gym |
| 6 | Gym trainer | `TRAINER_BURGLAR_RAMON` | CinnabarIsland_Gym |
| 6 | Gym trainer | `TRAINER_COOLTRAINER_SAMUEL` | ViridianCity_Gym |
| 6 | Gym trainer | `TRAINER_COOLTRAINER_WARREN` | ViridianCity_Gym |
| 6 | Gym trainer | `TRAINER_COOLTRAINER_YUJI` | ViridianCity_Gym |
| 6 | Gym trainer | `TRAINER_SUPER_NERD_AVERY` | CinnabarIsland_Gym |
| 6 | Gym trainer | `TRAINER_SUPER_NERD_DEREK` | CinnabarIsland_Gym |
| 6 | Gym trainer | `TRAINER_SUPER_NERD_ERIK` | CinnabarIsland_Gym |
| 6 | Gym trainer | `TRAINER_SUPER_NERD_ZAC` | CinnabarIsland_Gym |
| 6 | Gym trainer | `TRAINER_TAMER_COLE` | ViridianCity_Gym |
| 6 | Gym trainer | `TRAINER_TAMER_JASON` | ViridianCity_Gym |
| 6 | Post-game trainer | `TRAINER_AROMA_LADY_MIAH` | SevenIsland_SevaultCanyon_Entrance |
| 6 | Post-game trainer | `TRAINER_AROMA_LADY_ROSE` | SixIsland_WaterPath |
| 6 | Post-game trainer | `TRAINER_BIRD_KEEPER_CHAZ` | FiveIsland_MemorialPillar |
| 6 | Post-game trainer | `TRAINER_BIRD_KEEPER_HAROLD` | FiveIsland_MemorialPillar |
| 6 | Post-game trainer | `TRAINER_BIRD_KEEPER_MILO` | FiveIsland_MemorialPillar |
| 6 | Post-game trainer | `TRAINER_BUG_CATCHER_GARRET` | SixIsland_PatternBush |
| 6 | Post-game trainer | `TRAINER_BUG_CATCHER_JONAH` | SixIsland_PatternBush |
| 6 | Post-game trainer | `TRAINER_BUG_CATCHER_VANCE` | SixIsland_PatternBush |
| 6 | Post-game trainer | `TRAINER_CAMPER_RILEY` | SixIsland_PatternBush |
| 6 | Post-game trainer | `TRAINER_COOLTRAINER_LEROY` | SevenIsland_SevaultCanyon |
| 6 | Post-game trainer | `TRAINER_COOLTRAINER_MICHELLE` | SevenIsland_SevaultCanyon |
| 6 | Post-game trainer | `TRAINER_COOL_COUPLE_LEX_NYA` | SevenIsland_SevaultCanyon |
| 6 | Post-game trainer | `TRAINER_CRUSH_GIRL_CYNDY` | SevenIsland_SevaultCanyon |
| 6 | Post-game trainer | `TRAINER_FISHERMAN_TYLOR` | SixIsland_OutcastIsland |
| 6 | Post-game trainer | `TRAINER_GENTLEMAN_CLIFFORD` | SevenIsland_TanobyRuins |
| 6 | Post-game trainer | `TRAINER_HIKER_DARYL` | SixIsland_RuinValley |
| 6 | Post-game trainer | `TRAINER_HIKER_EARL` | SixIsland_WaterPath |
| 6 | Post-game trainer | `TRAINER_JUGGLER_EDWARD` | SixIsland_WaterPath |
| 6 | Post-game trainer | `TRAINER_JUGGLER_MASON` | SevenIsland_SevaultCanyon_Entrance |
| 6 | Post-game trainer | `TRAINER_LADY_GILLIAN` | FiveIsland_ResortGorgeous |
| 6 | Post-game trainer | `TRAINER_LADY_JACKI` | FiveIsland_ResortGorgeous |
| 6 | Post-game trainer | `TRAINER_LADY_SELPHY` | FiveIsland_LostCave_Room10 |
| 6 | Post-game trainer | `TRAINER_LASS_DALIA` | SixIsland_PatternBush |
| 6 | Post-game trainer | `TRAINER_LASS_JOANA` | SixIsland_PatternBush |
| 6 | Post-game trainer | `TRAINER_PAINTER_CELINA` | FiveIsland_ResortGorgeous |
| 6 | Post-game trainer | `TRAINER_PAINTER_DAISY` | FiveIsland_ResortGorgeous |
| 6 | Post-game trainer | `TRAINER_PAINTER_EDNA` | SevenIsland_TanobyRuins |
| 6 | Post-game trainer | `TRAINER_PAINTER_RAYNA` | FiveIsland_ResortGorgeous |
| 6 | Post-game trainer | `TRAINER_PICNICKER_MARCY` | SixIsland_PatternBush |
| 6 | Post-game trainer | `TRAINER_PKMN_BREEDER_ALIZE` | FiveIsland_WaterLabyrinth |
| 6 | Post-game trainer | `TRAINER_PKMN_BREEDER_ALLISON` | SixIsland_PatternBush |
| 6 | Post-game trainer | `TRAINER_PKMN_BREEDER_BETHANY` | SixIsland_PatternBush |
| 6 | Post-game trainer | `TRAINER_PKMN_RANGER_JACKSON` | SevenIsland_SevaultCanyon |
| 6 | Post-game trainer | `TRAINER_PKMN_RANGER_KATELYN` | SevenIsland_SevaultCanyon |
| 6 | Post-game trainer | `TRAINER_PKMN_RANGER_MADELINE` | SevenIsland_SevaultCanyon_Entrance |
| 6 | Post-game trainer | `TRAINER_PKMN_RANGER_NICOLAS` | SevenIsland_SevaultCanyon_Entrance |
| 6 | Post-game trainer | `TRAINER_POKEMANIAC_HECTOR` | SixIsland_RuinValley |
| 6 | Post-game trainer | `TRAINER_PSYCHIC_DARIO` | SevenIsland_TrainerTower |
| 6 | Post-game trainer | `TRAINER_PSYCHIC_JACLYN` | SixIsland_GreenPath |
| 6 | Post-game trainer | `TRAINER_PSYCHIC_LAURA` | FiveIsland_LostCave_Room4 |
| 6 | Post-game trainer | `TRAINER_PSYCHIC_RODETTE` | SevenIsland_TrainerTower |
| 6 | Post-game trainer | `TRAINER_RUIN_MANIAC_BENJAMIN` | SevenIsland_TanobyRuins |
| 6 | Post-game trainer | `TRAINER_RUIN_MANIAC_BRANDON` | SevenIsland_TanobyRuins |
| 6 | Post-game trainer | `TRAINER_RUIN_MANIAC_FOSTER` | SixIsland_RuinValley |
| 6 | Post-game trainer | `TRAINER_RUIN_MANIAC_LARRY` | SixIsland_RuinValley |
| 6 | Post-game trainer | `TRAINER_RUIN_MANIAC_LAWSON` | FiveIsland_LostCave_Room1 |
| 6 | Post-game trainer | `TRAINER_RUIN_MANIAC_LAYTON` | SixIsland_PatternBush |
| 6 | Post-game trainer | `TRAINER_RUIN_MANIAC_STANLY` | SixIsland_RuinValley |
| 6 | Post-game trainer | `TRAINER_SCIENTIST_GIDEON` | FiveIsland_RocketWarehouse |
| 6 | Post-game trainer | `TRAINER_SIS_AND_BRO_AVA_GEB` | SixIsland_OutcastIsland |
| 6 | Post-game trainer | `TRAINER_SWIMMER_FEMALE_DENISE` | SixIsland_WaterPath |
| 6 | Post-game trainer | `TRAINER_SWIMMER_FEMALE_NICOLE` | SixIsland_OutcastIsland |
| 6 | Post-game trainer | `TRAINER_SWIMMER_MALE_MYMO` | SixIsland_OutcastIsland |
| 6 | Post-game trainer | `TRAINER_SWIMMER_MALE_SAMIR` | SixIsland_WaterPath |
| 6 | Post-game trainer | `TRAINER_SWIMMER_MALE_TOBY` | FiveIsland_ResortGorgeous |
| 6 | Post-game trainer | `TRAINER_TAMER_EVAN` | SevenIsland_SevaultCanyon |
| 6 | Post-game trainer | `TRAINER_TEAM_ROCKET_GRUNT_42` | FiveIsland_RocketWarehouse |
| 6 | Post-game trainer | `TRAINER_TEAM_ROCKET_GRUNT_45` | FourIsland_IcefallCave_Back |
| 6 | Post-game trainer | `TRAINER_TEAM_ROCKET_GRUNT_46` | SixIsland_OutcastIsland |
| 6 | Post-game trainer | `TRAINER_TEAM_ROCKET_GRUNT_47` | FiveIsland_RocketWarehouse |
| 6 | Post-game trainer | `TRAINER_TEAM_ROCKET_GRUNT_48` | FiveIsland_RocketWarehouse |
| 6 | Post-game trainer | `TRAINER_TEAM_ROCKET_GRUNT_49` | FiveIsland_Meadow |
| 6 | Post-game trainer | `TRAINER_TEAM_ROCKET_GRUNT_50` | FiveIsland_Meadow |
| 6 | Post-game trainer | `TRAINER_TEAM_ROCKET_GRUNT_51` | FiveIsland_Meadow |
| 6 | Post-game trainer | `TRAINER_TWINS_MIU_MIA` | SixIsland_WaterPath |
| 6 | Post-game trainer | `TRAINER_YOUNGSTER_CORDELL` | SixIsland_PatternBush |
| 6 | Post-game trainer | `TRAINER_YOUNGSTER_DESTIN` | FiveIsland_ResortGorgeous |
| 6 | Post-game trainer | `TRAINER_YOUNGSTER_NASH` | SixIsland_PatternBush |
| 6 | Post-game trainer | `TRAINER_YOUNG_COUPLE_EVE_JON` | SevenIsland_SevaultCanyon_Entrance |
| 6 | Rematch of BIRD_KEEPER_CHAZ | `TRAINER_BIRD_KEEPER_CHAZ_2` | (VS Seeker) |
| 6 | Rematch of BIRD_KEEPER_HAROLD | `TRAINER_BIRD_KEEPER_HAROLD_2` | (VS Seeker) |
| 6 | Rematch of BIRD_KEEPER_MILO | `TRAINER_BIRD_KEEPER_MILO_2` | (VS Seeker) |
| 6 | Rematch of BLACK_BELT_HUGH | `TRAINER_BLACK_BELT_HUGH_2` | (VS Seeker) |
| 6 | Rematch of BLACK_BELT_HUGH | `TRAINER_BLACK_BELT_HUGH_3` | (VS Seeker) |
| 6 | Rematch of BLACK_BELT_SHEA | `TRAINER_BLACK_BELT_SHEA_2` | (VS Seeker) |
| 6 | Rematch of BLACK_BELT_SHEA | `TRAINER_BLACK_BELT_SHEA_3` | (VS Seeker) |
| 6 | Rematch of COOLTRAINER_LEROY | `TRAINER_COOLTRAINER_LEROY_2` | (VS Seeker) |
| 6 | Rematch of COOLTRAINER_MICHELLE | `TRAINER_COOLTRAINER_MICHELLE_2` | (VS Seeker) |
| 6 | Rematch of COOL_COUPLE_LEX_NYA | `TRAINER_COOL_COUPLE_LEX_NYA_2` | (VS Seeker) |
| 6 | Rematch of CRUSH_GIRL_CYNDY | `TRAINER_CRUSH_GIRL_CYNDY_2` | (VS Seeker) |
| 6 | Rematch of CRUSH_GIRL_SHARON | `TRAINER_CRUSH_GIRL_SHARON_2` | (VS Seeker) |
| 6 | Rematch of CRUSH_GIRL_SHARON | `TRAINER_CRUSH_GIRL_SHARON_3` | (VS Seeker) |
| 6 | Rematch of CRUSH_GIRL_TANYA | `TRAINER_CRUSH_GIRL_TANYA_2` | (VS Seeker) |
| 6 | Rematch of CRUSH_GIRL_TANYA | `TRAINER_CRUSH_GIRL_TANYA_3` | (VS Seeker) |
| 6 | Rematch of CRUSH_KIN_MIK_KIA | `TRAINER_CRUSH_KIN_MIK_KIA_2` | (VS Seeker) |
| 6 | Rematch of CRUSH_KIN_MIK_KIA | `TRAINER_CRUSH_KIN_MIK_KIA_3` | (VS Seeker) |
| 6 | Rematch of FISHERMAN_WADE | `TRAINER_FISHERMAN_WADE_2` | (VS Seeker) |
| 6 | Rematch of HIKER_EARL | `TRAINER_HIKER_EARL_2` | (VS Seeker) |
| 6 | Rematch of JUGGLER_MASON | `TRAINER_JUGGLER_MASON_2` | (VS Seeker) |
| 6 | Rematch of PAINTER_RAYNA | `TRAINER_PAINTER_RAYNA_2` | (VS Seeker) |
| 6 | Rematch of PICNICKER_MISSY | `TRAINER_PICNICKER_MISSY_2` | (VS Seeker) |
| 6 | Rematch of PICNICKER_MISSY | `TRAINER_PICNICKER_MISSY_3` | (VS Seeker) |
| 6 | Rematch of PKMN_BREEDER_ALIZE | `TRAINER_PKMN_BREEDER_ALIZE_2` | (VS Seeker) |
| 6 | Rematch of PKMN_RANGER_JACKSON | `TRAINER_PKMN_RANGER_JACKSON_2` | (VS Seeker) |
| 6 | Rematch of PKMN_RANGER_KATELYN | `TRAINER_PKMN_RANGER_KATELYN_2` | (VS Seeker) |
| 6 | Rematch of PKMN_RANGER_MADELINE | `TRAINER_PKMN_RANGER_MADELINE_2` | (VS Seeker) |
| 6 | Rematch of PKMN_RANGER_NICOLAS | `TRAINER_PKMN_RANGER_NICOLAS_2` | (VS Seeker) |
| 6 | Rematch of POKEMANIAC_HECTOR | `TRAINER_POKEMANIAC_HECTOR_2` | (VS Seeker) |
| 6 | Rematch of PSYCHIC_DARIO | `TRAINER_PSYCHIC_DARIO_2` | (VS Seeker) |
| 6 | Rematch of PSYCHIC_JACLYN | `TRAINER_PSYCHIC_JACLYN_2` | (VS Seeker) |
| 6 | Rematch of PSYCHIC_RODETTE | `TRAINER_PSYCHIC_RODETTE_2` | (VS Seeker) |
| 6 | Rematch of RUIN_MANIAC_LARRY | `TRAINER_RUIN_MANIAC_LARRY_2` | (VS Seeker) |
| 6 | Rematch of SIS_AND_BRO_LIL_IAN | `TRAINER_SIS_AND_BRO_LIL_IAN_2` | (VS Seeker) |
| 6 | Rematch of SIS_AND_BRO_LIL_IAN | `TRAINER_SIS_AND_BRO_LIL_IAN_3` | (VS Seeker) |
| 6 | Rematch of SWIMMER_FEMALE_ALICE | `TRAINER_SWIMMER_FEMALE_ALICE_2` | (VS Seeker) |
| 6 | Rematch of SWIMMER_FEMALE_MELISSA | `TRAINER_SWIMMER_FEMALE_MELISSA_2` | (VS Seeker) |
| 6 | Rematch of SWIMMER_FEMALE_NICOLE | `TRAINER_SWIMMER_FEMALE_NICOLE_2` | (VS Seeker) |
| 6 | Rematch of SWIMMER_MALE_DARRIN | `TRAINER_SWIMMER_MALE_DARRIN_2` | (VS Seeker) |
| 6 | Rematch of SWIMMER_MALE_FINN | `TRAINER_SWIMMER_MALE_FINN_2` | (VS Seeker) |
| 6 | Rematch of SWIMMER_MALE_JACK | `TRAINER_SWIMMER_MALE_JACK_2` | (VS Seeker) |
| 6 | Rematch of SWIMMER_MALE_MATTHEW | `TRAINER_SWIMMER_MALE_MATTHEW_2` | (VS Seeker) |
| 6 | Rematch of SWIMMER_MALE_SAMIR | `TRAINER_SWIMMER_MALE_SAMIR_2` | (VS Seeker) |
| 6 | Rematch of SWIMMER_MALE_TONY | `TRAINER_SWIMMER_MALE_TONY_2` | (VS Seeker) |
| 6 | Rematch of TAMER_EVAN | `TRAINER_TAMER_EVAN_2` | (VS Seeker) |
| 6 | Rematch of TUBER_AMIRA | `TRAINER_TUBER_AMIRA_2` | (VS Seeker) |
| 6 | Rematch of TWINS_JOY_MEG | `TRAINER_TWINS_JOY_MEG_2` | (VS Seeker) |
| 6 | Rematch of YOUNGSTER_DESTIN | `TRAINER_YOUNGSTER_DESTIN_2` | (VS Seeker) |
| 6 | Rival | `TRAINER_RIVAL_CERULEAN_BULBASAUR` | CeruleanCity |
| 6 | Rival | `TRAINER_RIVAL_CERULEAN_CHARMANDER` | CeruleanCity |
| 6 | Rival | `TRAINER_RIVAL_CERULEAN_SQUIRTLE` | CeruleanCity |
| 6 | Rival | `TRAINER_RIVAL_OAKS_LAB_BULBASAUR` | PalletTown_ProfessorOaksLab |
| 6 | Rival | `TRAINER_RIVAL_OAKS_LAB_CHARMANDER` | PalletTown_ProfessorOaksLab |
| 6 | Rival | `TRAINER_RIVAL_OAKS_LAB_SQUIRTLE` | PalletTown_ProfessorOaksLab |
| 6 | Rival | `TRAINER_RIVAL_ROUTE22_EARLY_BULBASAUR` | Route22 |
| 6 | Rival | `TRAINER_RIVAL_ROUTE22_EARLY_CHARMANDER` | Route22 |
| 6 | Rival | `TRAINER_RIVAL_ROUTE22_EARLY_SQUIRTLE` | Route22 |
| 6 | Trainer | `TRAINER_BLACK_BELT_DAISUKE` | VictoryRoad_2F |
| 6 | Trainer | `TRAINER_COOLTRAINER_ALEXA` | VictoryRoad_3F |
| 6 | Trainer | `TRAINER_COOLTRAINER_CAROLINE` | VictoryRoad_3F |
| 6 | Trainer | `TRAINER_COOLTRAINER_COLBY` | VictoryRoad_3F |
| 6 | Trainer | `TRAINER_COOLTRAINER_GEORGE` | VictoryRoad_3F |
| 6 | Trainer | `TRAINER_COOLTRAINER_NAOMI` | VictoryRoad_1F |
| 6 | Trainer | `TRAINER_COOLTRAINER_ROLANDO` | VictoryRoad_1F |
| 6 | Trainer | `TRAINER_COOL_COUPLE_RAY_TYRA` | VictoryRoad_3F |
| 6 | Trainer | `TRAINER_JUGGLER_GREGORY` | VictoryRoad_2F |
| 6 | Trainer | `TRAINER_JUGGLER_NELSON` | VictoryRoad_2F |
| 6 | Trainer | `TRAINER_POKEMANIAC_DAWSON` | VictoryRoad_2F |
| 6 | Trainer | `TRAINER_TAMER_VINCENT` | VictoryRoad_2F |
| 5 | Gym trainer | `TRAINER_BEAUTY_BRIDGET` | CeladonCity_Gym |
| 5 | Gym trainer | `TRAINER_BEAUTY_LORI` | CeladonCity_Gym |
| 5 | Gym trainer | `TRAINER_BEAUTY_TAMIA` | CeladonCity_Gym |
| 5 | Gym trainer | `TRAINER_BLACK_BELT_AARON` | SaffronCity_Dojo |
| 5 | Gym trainer | `TRAINER_BLACK_BELT_HIDEKI` | SaffronCity_Dojo |
| 5 | Gym trainer | `TRAINER_BLACK_BELT_HITOSHI` | SaffronCity_Dojo |
| 5 | Gym trainer | `TRAINER_BLACK_BELT_KOICHI` | SaffronCity_Dojo |
| 5 | Gym trainer | `TRAINER_BLACK_BELT_MIKE` | SaffronCity_Dojo |
| 5 | Gym trainer | `TRAINER_CHANNELER_AMANDA` | SaffronCity_Gym |
| 5 | Gym trainer | `TRAINER_CHANNELER_STACY` | SaffronCity_Gym |
| 5 | Gym trainer | `TRAINER_CHANNELER_TASHA` | SaffronCity_Gym |
| 5 | Gym trainer | `TRAINER_COOLTRAINER_MARY` | CeladonCity_Gym |
| 5 | Gym trainer | `TRAINER_JUGGLER_KAYDEN` | FuchsiaCity_Gym |
| 5 | Gym trainer | `TRAINER_JUGGLER_KIRK` | FuchsiaCity_Gym |
| 5 | Gym trainer | `TRAINER_JUGGLER_NATE` | FuchsiaCity_Gym |
| 5 | Gym trainer | `TRAINER_JUGGLER_SHAWN` | FuchsiaCity_Gym |
| 5 | Gym trainer | `TRAINER_LASS_KAY` | CeladonCity_Gym |
| 5 | Gym trainer | `TRAINER_LASS_LISA` | CeladonCity_Gym |
| 5 | Gym trainer | `TRAINER_PICNICKER_TINA` | CeladonCity_Gym |
| 5 | Gym trainer | `TRAINER_PSYCHIC_CAMERON` | SaffronCity_Gym |
| 5 | Gym trainer | `TRAINER_PSYCHIC_JOHAN` | SaffronCity_Gym |
| 5 | Gym trainer | `TRAINER_PSYCHIC_PRESTON` | SaffronCity_Gym |
| 5 | Gym trainer | `TRAINER_PSYCHIC_TYRON` | SaffronCity_Gym |
| 5 | Gym trainer | `TRAINER_TAMER_EDGAR` | FuchsiaCity_Gym |
| 5 | Gym trainer | `TRAINER_TAMER_PHIL` | FuchsiaCity_Gym |
| 5 | Rematch of BEAUTY_GRACE | `TRAINER_BEAUTY_GRACE_2` | (VS Seeker) |
| 5 | Rematch of BEAUTY_SHEILA | `TRAINER_BEAUTY_SHEILA_2` | (VS Seeker) |
| 5 | Rematch of BIKER_JAREN | `TRAINER_BIKER_JAREN_2` | (VS Seeker) |
| 5 | Rematch of BIKER_JAXON | `TRAINER_BIKER_JAXON_2` | (VS Seeker) |
| 5 | Rematch of BIKER_LUKAS | `TRAINER_BIKER_LUKAS_2` | (VS Seeker) |
| 5 | Rematch of BIKER_RUBEN | `TRAINER_BIKER_RUBEN_2` | (VS Seeker) |
| 5 | Rematch of BIRD_KEEPER_BENNY | `TRAINER_BIRD_KEEPER_BENNY_2` | (VS Seeker) |
| 5 | Rematch of BIRD_KEEPER_BENNY | `TRAINER_BIRD_KEEPER_BENNY_3` | (VS Seeker) |
| 5 | Rematch of BIRD_KEEPER_CHESTER | `TRAINER_BIRD_KEEPER_CHESTER_2` | (VS Seeker) |
| 5 | Rematch of BIRD_KEEPER_CHESTER | `TRAINER_BIRD_KEEPER_CHESTER_3` | (VS Seeker) |
| 5 | Rematch of BIRD_KEEPER_JACOB | `TRAINER_BIRD_KEEPER_JACOB_2` | (VS Seeker) |
| 5 | Rematch of BIRD_KEEPER_JACOB | `TRAINER_BIRD_KEEPER_JACOB_3` | (VS Seeker) |
| 5 | Rematch of BIRD_KEEPER_MARLON | `TRAINER_BIRD_KEEPER_MARLON_2` | (VS Seeker) |
| 5 | Rematch of BIRD_KEEPER_MARLON | `TRAINER_BIRD_KEEPER_MARLON_3` | (VS Seeker) |
| 5 | Rematch of BIRD_KEEPER_ROBERT | `TRAINER_BIRD_KEEPER_ROBERT_2` | (VS Seeker) |
| 5 | Rematch of BIRD_KEEPER_ROBERT | `TRAINER_BIRD_KEEPER_ROBERT_3` | (VS Seeker) |
| 5 | Rematch of CAMPER_CHRIS | `TRAINER_CAMPER_CHRIS_2` | (VS Seeker) |
| 5 | Rematch of CAMPER_CHRIS | `TRAINER_CAMPER_CHRIS_3` | (VS Seeker) |
| 5 | Rematch of CAMPER_CHRIS | `TRAINER_CAMPER_CHRIS_4` | (VS Seeker) |
| 5 | Rematch of CRUSH_KIN_RON_MYA | `TRAINER_CRUSH_KIN_RON_MYA_2` | (VS Seeker) |
| 5 | Rematch of CRUSH_KIN_RON_MYA | `TRAINER_CRUSH_KIN_RON_MYA_3` | (VS Seeker) |
| 5 | Rematch of CRUSH_KIN_RON_MYA | `TRAINER_CRUSH_KIN_RON_MYA_4` | (VS Seeker) |
| 5 | Rematch of CUE_BALL_CAMRON | `TRAINER_CUE_BALL_CAMRON_2` | (VS Seeker) |
| 5 | Rematch of CUE_BALL_COREY | `TRAINER_CUE_BALL_COREY_2` | (VS Seeker) |
| 5 | Rematch of CUE_BALL_ISAIAH | `TRAINER_CUE_BALL_ISAIAH_2` | (VS Seeker) |
| 5 | Rematch of FISHERMAN_ELLIOT | `TRAINER_FISHERMAN_ELLIOT_2` | (VS Seeker) |
| 5 | Rematch of GAMER_RICH | `TRAINER_GAMER_RICH_2` | (VS Seeker) |
| 5 | Rematch of HIKER_JEREMY | `TRAINER_HIKER_JEREMY_2` | (VS Seeker) |
| 5 | Rematch of HIKER_TRENT | `TRAINER_HIKER_TRENT_2` | (VS Seeker) |
| 5 | Rematch of LASS_MEGAN | `TRAINER_LASS_MEGAN_2` | (VS Seeker) |
| 5 | Rematch of LASS_MEGAN | `TRAINER_LASS_MEGAN_3` | (VS Seeker) |
| 5 | Rematch of PICNICKER_ALICIA | `TRAINER_PICNICKER_ALICIA_2` | (VS Seeker) |
| 5 | Rematch of PICNICKER_ALICIA | `TRAINER_PICNICKER_ALICIA_3` | (VS Seeker) |
| 5 | Rematch of PICNICKER_ALICIA | `TRAINER_PICNICKER_ALICIA_4` | (VS Seeker) |
| 5 | Rematch of PICNICKER_BECKY | `TRAINER_PICNICKER_BECKY_2` | (VS Seeker) |
| 5 | Rematch of PICNICKER_BECKY | `TRAINER_PICNICKER_BECKY_3` | (VS Seeker) |
| 5 | Rematch of PICNICKER_BECKY | `TRAINER_PICNICKER_BECKY_4` | (VS Seeker) |
| 5 | Rematch of PICNICKER_SUSIE | `TRAINER_PICNICKER_SUSIE_2` | (VS Seeker) |
| 5 | Rematch of PICNICKER_SUSIE | `TRAINER_PICNICKER_SUSIE_3` | (VS Seeker) |
| 5 | Rematch of PICNICKER_SUSIE | `TRAINER_PICNICKER_SUSIE_4` | (VS Seeker) |
| 5 | Rematch of POKEMANIAC_HERMAN | `TRAINER_POKEMANIAC_HERMAN_2` | (VS Seeker) |
| 5 | Rematch of POKEMANIAC_HERMAN | `TRAINER_POKEMANIAC_HERMAN_3` | (VS Seeker) |
| 5 | Rematch of POKEMANIAC_MARK | `TRAINER_POKEMANIAC_MARK_2` | (VS Seeker) |
| 5 | Rematch of POKEMANIAC_MARK | `TRAINER_POKEMANIAC_MARK_3` | (VS Seeker) |
| 5 | Rematch of ROCKER_LUCA | `TRAINER_ROCKER_LUCA_2` | (VS Seeker) |
| 5 | Rematch of SUPER_NERD_GLENN | `TRAINER_SUPER_NERD_GLENN_2` | (VS Seeker) |
| 5 | Rematch of TWINS_ELI_ANNE | `TRAINER_TWINS_ELI_ANNE_2` | (VS Seeker) |
| 5 | Rematch of YOUNG_COUPLE_GIA_JES | `TRAINER_YOUNG_COUPLE_GIA_JES_2` | (VS Seeker) |
| 5 | Rematch of YOUNG_COUPLE_GIA_JES | `TRAINER_YOUNG_COUPLE_GIA_JES_3` | (VS Seeker) |
| 5 | Trainer | `TRAINER_AROMA_LADY_NIKKI` | ThreeIsland_BondBridge |
| 5 | Trainer | `TRAINER_AROMA_LADY_VIOLET` | ThreeIsland_BondBridge |
| 5 | Trainer | `TRAINER_BIKER_GOON` | ThreeIsland |
| 5 | Trainer | `TRAINER_BIKER_GOON_2` | ThreeIsland |
| 5 | Trainer | `TRAINER_BIKER_GOON_3` | ThreeIsland |
| 5 | Trainer | `TRAINER_BIRD_KEEPER_ROGER` | Route20 |
| 5 | Trainer | `TRAINER_BLACK_BELT_HUGH` | OneIsland_KindleRoad |
| 5 | Trainer | `TRAINER_BLACK_BELT_SHEA` | OneIsland_KindleRoad |
| 5 | Trainer | `TRAINER_BURGLAR_ARNIE` | PokemonMansion_2F |
| 5 | Trainer | `TRAINER_BURGLAR_LEWIS` | PokemonMansion_B1F |
| 5 | Trainer | `TRAINER_BURGLAR_SIMON` | PokemonMansion_3F |
| 5 | Trainer | `TRAINER_CAMPER_BRYCE` | OneIsland_KindleRoad |
| 5 | Trainer | `TRAINER_CRUSH_GIRL_JOCELYN` | MtEmber_Exterior |
| 5 | Trainer | `TRAINER_CRUSH_GIRL_SHARON` | OneIsland_KindleRoad |
| 5 | Trainer | `TRAINER_CRUSH_GIRL_TANYA` | OneIsland_KindleRoad |
| 5 | Trainer | `TRAINER_CRUSH_KIN_MIK_KIA` | OneIsland_KindleRoad |
| 5 | Trainer | `TRAINER_CUE_BALL_PAXTON` | ThreeIsland |
| 5 | Trainer | `TRAINER_FISHERMAN_CLAUDE` | Route21_South |
| 5 | Trainer | `TRAINER_FISHERMAN_NOLAN` | Route21_South |
| 5 | Trainer | `TRAINER_FISHERMAN_RONALD` | Route21_North |
| 5 | Trainer | `TRAINER_FISHERMAN_TOMMY` | OneIsland_KindleRoad |
| 5 | Trainer | `TRAINER_FISHERMAN_WADE` | Route21_North |
| 5 | Trainer | `TRAINER_PICNICKER_CLAIRE` | OneIsland_KindleRoad |
| 5 | Trainer | `TRAINER_PICNICKER_IRENE` | Route20 |
| 5 | Trainer | `TRAINER_PICNICKER_MISSY` | Route20 |
| 5 | Trainer | `TRAINER_PKMN_RANGER_BETH` | MtEmber_Exterior |
| 5 | Trainer | `TRAINER_PKMN_RANGER_LOGAN` | MtEmber_Exterior |
| 5 | Trainer | `TRAINER_SCIENTIST_BRAYDON` | PokemonMansion_3F |
| 5 | Trainer | `TRAINER_SCIENTIST_IVAN` | PokemonMansion_B1F |
| 5 | Trainer | `TRAINER_SCIENTIST_TED` | PokemonMansion_1F |
| 5 | Trainer | `TRAINER_SIS_AND_BRO_LIA_LUC` | Route19 |
| 5 | Trainer | `TRAINER_SIS_AND_BRO_LIL_IAN` | Route21_North |
| 5 | Trainer | `TRAINER_SWIMMER_FEMALE_ABIGAIL` | OneIsland_KindleRoad |
| 5 | Trainer | `TRAINER_SWIMMER_FEMALE_ALICE` | Route19 |
| 5 | Trainer | `TRAINER_SWIMMER_FEMALE_AMARA` | OneIsland_TreasureBeach |
| 5 | Trainer | `TRAINER_SWIMMER_FEMALE_ANYA` | Route19 |
| 5 | Trainer | `TRAINER_SWIMMER_FEMALE_CONNIE` | Route19 |
| 5 | Trainer | `TRAINER_SWIMMER_FEMALE_MARIA` | OneIsland_KindleRoad |
| 5 | Trainer | `TRAINER_SWIMMER_FEMALE_MELISSA` | Route20 |
| 5 | Trainer | `TRAINER_SWIMMER_FEMALE_NORA` | Route20 |
| 5 | Trainer | `TRAINER_SWIMMER_FEMALE_SHIRLEY` | Route20 |
| 5 | Trainer | `TRAINER_SWIMMER_FEMALE_TIFFANY` | Route20 |
| 5 | Trainer | `TRAINER_SWIMMER_FEMALE_TISHA` | ThreeIsland_BondBridge |
| 5 | Trainer | `TRAINER_SWIMMER_MALE_AXLE` | Route19 |
| 5 | Trainer | `TRAINER_SWIMMER_MALE_BARRY` | Route20 |
| 5 | Trainer | `TRAINER_SWIMMER_MALE_DARRIN` | Route20 |
| 5 | Trainer | `TRAINER_SWIMMER_MALE_DAVID` | Route19 |
| 5 | Trainer | `TRAINER_SWIMMER_MALE_DEAN` | Route20 |
| 5 | Trainer | `TRAINER_SWIMMER_MALE_DOUGLAS` | Route19 |
| 5 | Trainer | `TRAINER_SWIMMER_MALE_FINN` | OneIsland_KindleRoad |
| 5 | Trainer | `TRAINER_SWIMMER_MALE_GARRETT` | OneIsland_KindleRoad |
| 5 | Trainer | `TRAINER_SWIMMER_MALE_JACK` | Route21_South |
| 5 | Trainer | `TRAINER_SWIMMER_MALE_JEROME` | Route21_South |
| 5 | Trainer | `TRAINER_SWIMMER_MALE_MATTHEW` | Route19 |
| 5 | Trainer | `TRAINER_SWIMMER_MALE_REECE` | Route19 |
| 5 | Trainer | `TRAINER_SWIMMER_MALE_RICHARD` | Route19 |
| 5 | Trainer | `TRAINER_SWIMMER_MALE_ROLAND` | Route21_South |
| 5 | Trainer | `TRAINER_SWIMMER_MALE_SPENCER` | Route21_North |
| 5 | Trainer | `TRAINER_SWIMMER_MALE_TONY` | Route19 |
| 5 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_43` | MtEmber_Exterior |
| 5 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_44` | MtEmber_Exterior |
| 5 | Trainer | `TRAINER_TUBER_ALEXIS` | ThreeIsland_BondBridge |
| 5 | Trainer | `TRAINER_TUBER_AMIRA` | ThreeIsland_BondBridge |
| 5 | Trainer | `TRAINER_TWINS_JOY_MEG` | ThreeIsland_BondBridge |
| 5 | Trainer | `TRAINER_YOUNGSTER_JOHNSON` | PokemonMansion_1F |
| 4 | Gym trainer | `TRAINER_ENGINEER_BAILY` | VermilionCity_Gym |
| 4 | Gym trainer | `TRAINER_GENTLEMAN_TUCKER` | VermilionCity_Gym |
| 4 | Gym trainer | `TRAINER_PICNICKER_DIANA` | CeruleanCity_Gym |
| 4 | Gym trainer | `TRAINER_SAILOR_DWAYNE` | VermilionCity_Gym |
| 4 | Gym trainer | `TRAINER_SWIMMER_MALE_LUIS` | CeruleanCity_Gym |
| 4 | Rematch of BUG_CATCHER_COLTON | `TRAINER_BUG_CATCHER_COLTON_2` | (VS Seeker) |
| 4 | Rematch of BUG_CATCHER_COLTON | `TRAINER_BUG_CATCHER_COLTON_3` | (VS Seeker) |
| 4 | Rematch of BUG_CATCHER_COLTON | `TRAINER_BUG_CATCHER_COLTON_4` | (VS Seeker) |
| 4 | Rematch of CAMPER_JEFF | `TRAINER_CAMPER_JEFF_2` | (VS Seeker) |
| 4 | Rematch of CAMPER_JEFF | `TRAINER_CAMPER_JEFF_3` | (VS Seeker) |
| 4 | Rematch of CAMPER_JEFF | `TRAINER_CAMPER_JEFF_4` | (VS Seeker) |
| 4 | Rematch of CAMPER_RICKY | `TRAINER_CAMPER_RICKY_2` | (VS Seeker) |
| 4 | Rematch of CAMPER_RICKY | `TRAINER_CAMPER_RICKY_3` | (VS Seeker) |
| 4 | Rematch of CAMPER_RICKY | `TRAINER_CAMPER_RICKY_4` | (VS Seeker) |
| 4 | Rematch of ENGINEER_BERNIE | `TRAINER_ENGINEER_BERNIE_2` | (VS Seeker) |
| 4 | Rematch of GAMER_DARIAN | `TRAINER_GAMER_DARIAN_2` | (VS Seeker) |
| 4 | Rematch of HIKER_FRANKLIN | `TRAINER_HIKER_FRANKLIN_2` | (VS Seeker) |
| 4 | Rematch of LASS_JANICE | `TRAINER_LASS_JANICE_2` | (VS Seeker) |
| 4 | Rematch of LASS_JANICE | `TRAINER_LASS_JANICE_3` | (VS Seeker) |
| 4 | Rematch of LASS_RELI | `TRAINER_LASS_RELI_2` | (VS Seeker) |
| 4 | Rematch of LASS_RELI | `TRAINER_LASS_RELI_3` | (VS Seeker) |
| 4 | Rematch of PICNICKER_ISABELLE | `TRAINER_PICNICKER_ISABELLE_2` | (VS Seeker) |
| 4 | Rematch of PICNICKER_ISABELLE | `TRAINER_PICNICKER_ISABELLE_3` | (VS Seeker) |
| 4 | Rematch of PICNICKER_ISABELLE | `TRAINER_PICNICKER_ISABELLE_4` | (VS Seeker) |
| 4 | Rematch of PICNICKER_KELSEY | `TRAINER_PICNICKER_KELSEY_2` | (VS Seeker) |
| 4 | Rematch of PICNICKER_KELSEY | `TRAINER_PICNICKER_KELSEY_3` | (VS Seeker) |
| 4 | Rematch of PICNICKER_KELSEY | `TRAINER_PICNICKER_KELSEY_4` | (VS Seeker) |
| 4 | Rematch of YOUNGSTER_BEN | `TRAINER_YOUNGSTER_BEN_2` | (VS Seeker) |
| 4 | Rematch of YOUNGSTER_BEN | `TRAINER_YOUNGSTER_BEN_3` | (VS Seeker) |
| 4 | Rematch of YOUNGSTER_BEN | `TRAINER_YOUNGSTER_BEN_4` | (VS Seeker) |
| 4 | Rematch of YOUNGSTER_CHAD | `TRAINER_YOUNGSTER_CHAD_2` | (VS Seeker) |
| 4 | Rematch of YOUNGSTER_CHAD | `TRAINER_YOUNGSTER_CHAD_3` | (VS Seeker) |
| 4 | Rematch of YOUNGSTER_CHAD | `TRAINER_YOUNGSTER_CHAD_4` | (VS Seeker) |
| 4 | Rematch of YOUNGSTER_TIMMY | `TRAINER_YOUNGSTER_TIMMY_2` | (VS Seeker) |
| 4 | Rematch of YOUNGSTER_TIMMY | `TRAINER_YOUNGSTER_TIMMY_3` | (VS Seeker) |
| 4 | Rematch of YOUNGSTER_TIMMY | `TRAINER_YOUNGSTER_TIMMY_4` | (VS Seeker) |
| 4 | Rematch of YOUNGSTER_YASU | `TRAINER_YOUNGSTER_YASU_2` | (VS Seeker) |
| 4 | Rematch of YOUNGSTER_YASU | `TRAINER_YOUNGSTER_YASU_3` | (VS Seeker) |
| 4 | Trainer | `TRAINER_BEAUTY_GRACE` | Route15 |
| 4 | Trainer | `TRAINER_BEAUTY_LOLA` | Route13 |
| 4 | Trainer | `TRAINER_BEAUTY_OLIVIA` | Route15 |
| 4 | Trainer | `TRAINER_BEAUTY_SHEILA` | Route13 |
| 4 | Trainer | `TRAINER_BIKER_ALEX` | Route15 |
| 4 | Trainer | `TRAINER_BIKER_BILLY` | Route17 |
| 4 | Trainer | `TRAINER_BIKER_ERNEST` | Route15 |
| 4 | Trainer | `TRAINER_BIKER_GERALD` | Route14 |
| 4 | Trainer | `TRAINER_BIKER_HIDEO` | Route16 |
| 4 | Trainer | `TRAINER_BIKER_ISAAC` | Route14 |
| 4 | Trainer | `TRAINER_BIKER_JARED` | Route13 |
| 4 | Trainer | `TRAINER_BIKER_JAREN` | Route8 |
| 4 | Trainer | `TRAINER_BIKER_JAXON` | Route17 |
| 4 | Trainer | `TRAINER_BIKER_LAO` | Route16 |
| 4 | Trainer | `TRAINER_BIKER_LUKAS` | Route14 |
| 4 | Trainer | `TRAINER_BIKER_MALIK` | Route14 |
| 4 | Trainer | `TRAINER_BIKER_NIKOLAS` | Route17 |
| 4 | Trainer | `TRAINER_BIKER_RICARDO` | Route8 |
| 4 | Trainer | `TRAINER_BIKER_RUBEN` | Route16 |
| 4 | Trainer | `TRAINER_BIKER_VIRGIL` | Route17 |
| 4 | Trainer | `TRAINER_BIKER_WILLIAM` | Route17 |
| 4 | Trainer | `TRAINER_BIRD_KEEPER_BECK` | Route14 |
| 4 | Trainer | `TRAINER_BIRD_KEEPER_BENNY` | Route14 |
| 4 | Trainer | `TRAINER_BIRD_KEEPER_CARTER` | Route14 |
| 4 | Trainer | `TRAINER_BIRD_KEEPER_CHESTER` | Route15 |
| 4 | Trainer | `TRAINER_BIRD_KEEPER_DONALD` | Route14 |
| 4 | Trainer | `TRAINER_BIRD_KEEPER_EDWIN` | Route15 |
| 4 | Trainer | `TRAINER_BIRD_KEEPER_JACOB` | Route18 |
| 4 | Trainer | `TRAINER_BIRD_KEEPER_MARLON` | Route14 |
| 4 | Trainer | `TRAINER_BIRD_KEEPER_MITCH` | Route14 |
| 4 | Trainer | `TRAINER_BIRD_KEEPER_PERRY` | Route13 |
| 4 | Trainer | `TRAINER_BIRD_KEEPER_RAMIRO` | Route18 |
| 4 | Trainer | `TRAINER_BIRD_KEEPER_ROBERT` | Route13 |
| 4 | Trainer | `TRAINER_BIRD_KEEPER_SEBASTIAN` | Route13 |
| 4 | Trainer | `TRAINER_BIRD_KEEPER_WILTON` | Route18 |
| 4 | Trainer | `TRAINER_BUG_CATCHER_BRENT` | Route9 |
| 4 | Trainer | `TRAINER_BUG_CATCHER_CONNER` | Route9 |
| 4 | Trainer | `TRAINER_CAMPER_CHRIS` | Route9 |
| 4 | Trainer | `TRAINER_CAMPER_DREW` | Route9 |
| 4 | Trainer | `TRAINER_CAMPER_JUSTIN` | Route12 |
| 4 | Trainer | `TRAINER_CHANNELER_ANGELICA` | PokemonTower_6F |
| 4 | Trainer | `TRAINER_CHANNELER_CARLY` | PokemonTower_3F |
| 4 | Trainer | `TRAINER_CHANNELER_EMILIA` | PokemonTower_6F |
| 4 | Trainer | `TRAINER_CHANNELER_HOPE` | PokemonTower_3F |
| 4 | Trainer | `TRAINER_CHANNELER_JANAE` | PokemonTower_5F |
| 4 | Trainer | `TRAINER_CHANNELER_JENNIFER` | PokemonTower_6F |
| 4 | Trainer | `TRAINER_CHANNELER_JODY` | PokemonTower_4F |
| 4 | Trainer | `TRAINER_CHANNELER_KARINA` | PokemonTower_5F |
| 4 | Trainer | `TRAINER_CHANNELER_LAUREL` | PokemonTower_4F |
| 4 | Trainer | `TRAINER_CHANNELER_PATRICIA` | PokemonTower_3F |
| 4 | Trainer | `TRAINER_CHANNELER_PAULA` | PokemonTower_4F |
| 4 | Trainer | `TRAINER_CHANNELER_RUTH` | PokemonTower_5F |
| 4 | Trainer | `TRAINER_CHANNELER_TAMMY` | PokemonTower_5F |
| 4 | Trainer | `TRAINER_CRUSH_KIN_RON_MYA` | Route15 |
| 4 | Trainer | `TRAINER_CUE_BALL_CAMRON` | Route16 |
| 4 | Trainer | `TRAINER_CUE_BALL_COREY` | Route17 |
| 4 | Trainer | `TRAINER_CUE_BALL_ISAIAH` | Route17 |
| 4 | Trainer | `TRAINER_CUE_BALL_JAMAL` | Route17 |
| 4 | Trainer | `TRAINER_CUE_BALL_KOJI` | Route16 |
| 4 | Trainer | `TRAINER_CUE_BALL_LUKE` | Route16 |
| 4 | Trainer | `TRAINER_CUE_BALL_RAUL` | Route17 |
| 4 | Trainer | `TRAINER_CUE_BALL_ZEEK` | Route17 |
| 4 | Trainer | `TRAINER_FISHERMAN_ANDREW` | Route12 |
| 4 | Trainer | `TRAINER_FISHERMAN_CHIP` | Route12 |
| 4 | Trainer | `TRAINER_FISHERMAN_ELLIOT` | Route12 |
| 4 | Trainer | `TRAINER_FISHERMAN_HANK` | Route12 |
| 4 | Trainer | `TRAINER_FISHERMAN_NED` | Route12 |
| 4 | Trainer | `TRAINER_GAMER_RICH` | Route8 |
| 4 | Trainer | `TRAINER_GAMER_STAN` | Route8 |
| 4 | Trainer | `TRAINER_HIKER_ALAN` | Route9 |
| 4 | Trainer | `TRAINER_HIKER_ALLEN` | RockTunnel_B1F |
| 4 | Trainer | `TRAINER_HIKER_BRICE` | Route9 |
| 4 | Trainer | `TRAINER_HIKER_CLARK` | Route10 |
| 4 | Trainer | `TRAINER_HIKER_DUDLEY` | RockTunnel_B1F |
| 4 | Trainer | `TRAINER_HIKER_ERIC` | RockTunnel_B1F |
| 4 | Trainer | `TRAINER_HIKER_JEREMY` | Route9 |
| 4 | Trainer | `TRAINER_HIKER_LENNY` | RockTunnel_1F |
| 4 | Trainer | `TRAINER_HIKER_LUCAS` | RockTunnel_1F |
| 4 | Trainer | `TRAINER_HIKER_OLIVER` | RockTunnel_1F |
| 4 | Trainer | `TRAINER_HIKER_TRENT` | Route10 |
| 4 | Trainer | `TRAINER_JUGGLER_DALTON` | SilphCo_5F |
| 4 | Trainer | `TRAINER_LASS_ANDREA` | Route8 |
| 4 | Trainer | `TRAINER_LASS_JULIA` | Route8 |
| 4 | Trainer | `TRAINER_LASS_MEGAN` | Route8 |
| 4 | Trainer | `TRAINER_LASS_PAIGE` | Route8 |
| 4 | Trainer | `TRAINER_PICNICKER_ALICIA` | Route9 |
| 4 | Trainer | `TRAINER_PICNICKER_ALMA` | Route13 |
| 4 | Trainer | `TRAINER_PICNICKER_ARIANA` | RockTunnel_1F |
| 4 | Trainer | `TRAINER_PICNICKER_BECKY` | Route15 |
| 4 | Trainer | `TRAINER_PICNICKER_CAITLIN` | Route9 |
| 4 | Trainer | `TRAINER_PICNICKER_CAROL` | Route10 |
| 4 | Trainer | `TRAINER_PICNICKER_CELIA` | Route15 |
| 4 | Trainer | `TRAINER_PICNICKER_DANA` | RockTunnel_1F |
| 4 | Trainer | `TRAINER_PICNICKER_GWEN` | Route13 |
| 4 | Trainer | `TRAINER_PICNICKER_HEIDI` | Route10 |
| 4 | Trainer | `TRAINER_PICNICKER_KINDRA` | Route15 |
| 4 | Trainer | `TRAINER_PICNICKER_LEAH` | RockTunnel_1F |
| 4 | Trainer | `TRAINER_PICNICKER_MARTHA` | RockTunnel_B1F |
| 4 | Trainer | `TRAINER_PICNICKER_SOFIA` | RockTunnel_B1F |
| 4 | Trainer | `TRAINER_PICNICKER_SUSIE` | Route13 |
| 4 | Trainer | `TRAINER_PICNICKER_VALERIE` | Route13 |
| 4 | Trainer | `TRAINER_PICNICKER_YAZMIN` | Route15 |
| 4 | Trainer | `TRAINER_POKEMANIAC_ASHTON` | RockTunnel_1F |
| 4 | Trainer | `TRAINER_POKEMANIAC_COOPER` | RockTunnel_B1F |
| 4 | Trainer | `TRAINER_POKEMANIAC_HERMAN` | Route10 |
| 4 | Trainer | `TRAINER_POKEMANIAC_MARK` | Route10 |
| 4 | Trainer | `TRAINER_POKEMANIAC_STEVE` | RockTunnel_B1F |
| 4 | Trainer | `TRAINER_POKEMANIAC_WINSTON` | RockTunnel_B1F |
| 4 | Trainer | `TRAINER_ROCKER_LUCA` | Route12 |
| 4 | Trainer | `TRAINER_SCIENTIST_BEAU` | SilphCo_5F |
| 4 | Trainer | `TRAINER_SCIENTIST_CONNOR` | SilphCo_2F |
| 4 | Trainer | `TRAINER_SCIENTIST_ED` | SilphCo_9F |
| 4 | Trainer | `TRAINER_SCIENTIST_JERRY` | SilphCo_2F |
| 4 | Trainer | `TRAINER_SCIENTIST_JOSE` | SilphCo_3F |
| 4 | Trainer | `TRAINER_SCIENTIST_JOSHUA` | SilphCo_7F |
| 4 | Trainer | `TRAINER_SCIENTIST_PARKER` | SilphCo_8F |
| 4 | Trainer | `TRAINER_SCIENTIST_RODNEY` | SilphCo_4F |
| 4 | Trainer | `TRAINER_SCIENTIST_TAYLOR` | SilphCo_6F |
| 4 | Trainer | `TRAINER_SCIENTIST_TRAVIS` | SilphCo_10F |
| 4 | Trainer | `TRAINER_SUPER_NERD_AIDAN` | Route8 |
| 4 | Trainer | `TRAINER_SUPER_NERD_GLENN` | Route8 |
| 4 | Trainer | `TRAINER_SUPER_NERD_LESLIE` | Route8 |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_10` | RocketHideout_B1F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_11` | RocketHideout_B1F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_12` | RocketHideout_B1F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_13` | RocketHideout_B2F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_14` | RocketHideout_B3F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_15` | RocketHideout_B3F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_16` | RocketHideout_B4F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_17` | RocketHideout_B4F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_18` | RocketHideout_B4F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_19` | PokemonTower_7F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_20` | PokemonTower_7F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_21` | PokemonTower_7F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_23` | SilphCo_2F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_24` | SilphCo_2F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_25` | SilphCo_3F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_26` | SilphCo_4F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_27` | SilphCo_4F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_28` | SilphCo_5F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_29` | SilphCo_5F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_30` | SilphCo_6F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_31` | SilphCo_6F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_32` | SilphCo_8F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_33` | SilphCo_7F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_34` | SilphCo_7F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_35` | SilphCo_7F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_36` | SilphCo_8F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_37` | SilphCo_9F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_38` | SilphCo_9F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_39` | SilphCo_10F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_40` | SilphCo_11F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_41` | SilphCo_11F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_7` | CeladonCity_GameCorner |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_8` | RocketHideout_B1F |
| 4 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_9` | RocketHideout_B1F |
| 4 | Trainer | `TRAINER_TWINS_ELI_ANNE` | Route8 |
| 4 | Trainer | `TRAINER_TWINS_KIRI_JAN` | Route14 |
| 4 | Trainer | `TRAINER_YOUNG_COUPLE_GIA_JES` | Route12 |
| 4 | Trainer | `TRAINER_YOUNG_COUPLE_LEA_JED` | Route16 |
| 3 | Gym trainer | `TRAINER_CAMPER_LIAM` | PewterCity_Gym |
| 3 | Trainer | `TRAINER_BUG_CATCHER_CALE` | Route24 |
| 3 | Trainer | `TRAINER_BUG_CATCHER_COLTON` | Route3 |
| 3 | Trainer | `TRAINER_BUG_CATCHER_ELIJAH` | Route6 |
| 3 | Trainer | `TRAINER_BUG_CATCHER_GREG` | Route3 |
| 3 | Trainer | `TRAINER_BUG_CATCHER_JAMES` | Route3 |
| 3 | Trainer | `TRAINER_BUG_CATCHER_KEIGO` | Route6 |
| 3 | Trainer | `TRAINER_BUG_CATCHER_KENT` | MtMoon_1F |
| 3 | Trainer | `TRAINER_BUG_CATCHER_ROBBY` | MtMoon_1F |
| 3 | Trainer | `TRAINER_CAMPER_ETHAN` | Route24 |
| 3 | Trainer | `TRAINER_CAMPER_FLINT` | Route25 |
| 3 | Trainer | `TRAINER_CAMPER_JEFF` | Route6 |
| 3 | Trainer | `TRAINER_CAMPER_RICKY` | Route6 |
| 3 | Trainer | `TRAINER_CAMPER_SHANE` | Route24 |
| 3 | Trainer | `TRAINER_ENGINEER_BERNIE` | Route11 |
| 3 | Trainer | `TRAINER_ENGINEER_BRAXTON` | Route11 |
| 3 | Trainer | `TRAINER_FISHERMAN_BARNY` | SSAnne_B1F_Room1 |
| 3 | Trainer | `TRAINER_FISHERMAN_DALE` | SSAnne_2F_Room2 |
| 3 | Trainer | `TRAINER_GAMER_DARIAN` | Route11 |
| 3 | Trainer | `TRAINER_GAMER_DIRK` | Route11 |
| 3 | Trainer | `TRAINER_GAMER_HUGO` | Route11 |
| 3 | Trainer | `TRAINER_GAMER_JASPER` | Route11 |
| 3 | Trainer | `TRAINER_GENTLEMAN_ARTHUR` | SSAnne_1F_Room5 |
| 3 | Trainer | `TRAINER_GENTLEMAN_BROOKS` | SSAnne_2F_Room2 |
| 3 | Trainer | `TRAINER_GENTLEMAN_LAMAR` | SSAnne_2F_Room4 |
| 3 | Trainer | `TRAINER_GENTLEMAN_THOMAS` | SSAnne_1F_Room7 |
| 3 | Trainer | `TRAINER_HIKER_FRANKLIN` | Route25 |
| 3 | Trainer | `TRAINER_HIKER_MARCOS` | MtMoon_1F |
| 3 | Trainer | `TRAINER_HIKER_NOB` | Route25 |
| 3 | Trainer | `TRAINER_HIKER_WAYNE` | Route25 |
| 3 | Trainer | `TRAINER_LASS_ALI` | Route24 |
| 3 | Trainer | `TRAINER_LASS_ANN` | SSAnne_1F_Room2 |
| 3 | Trainer | `TRAINER_LASS_CRISSY` | Route4 |
| 3 | Trainer | `TRAINER_LASS_DAWN` | SSAnne_2F_Room4 |
| 3 | Trainer | `TRAINER_LASS_HALEY` | Route25 |
| 3 | Trainer | `TRAINER_LASS_IRIS` | MtMoon_1F |
| 3 | Trainer | `TRAINER_LASS_JANICE` | Route3 |
| 3 | Trainer | `TRAINER_LASS_MIRIAM` | MtMoon_1F |
| 3 | Trainer | `TRAINER_LASS_RELI` | Route24 |
| 3 | Trainer | `TRAINER_LASS_ROBIN` | Route3 |
| 3 | Trainer | `TRAINER_LASS_SALLY` | Route3 |
| 3 | Trainer | `TRAINER_PICNICKER_ISABELLE` | Route6 |
| 3 | Trainer | `TRAINER_PICNICKER_KELSEY` | Route25 |
| 3 | Trainer | `TRAINER_PICNICKER_NANCY` | Route6 |
| 3 | Trainer | `TRAINER_SAILOR_DUNCAN` | SSAnne_B1F_Room4 |
| 3 | Trainer | `TRAINER_SAILOR_DYLAN` | SSAnne_B1F_Room3 |
| 3 | Trainer | `TRAINER_SAILOR_EDMOND` | SSAnne_Deck |
| 3 | Trainer | `TRAINER_SAILOR_HUEY` | SSAnne_B1F_Room2 |
| 3 | Trainer | `TRAINER_SAILOR_LEONARD` | SSAnne_B1F_Room4 |
| 3 | Trainer | `TRAINER_SAILOR_PHILLIP` | SSAnne_B1F_Room1 |
| 3 | Trainer | `TRAINER_SAILOR_TREVOR` | SSAnne_Deck |
| 3 | Trainer | `TRAINER_SUPER_NERD_JOVAN` | MtMoon_1F |
| 3 | Trainer | `TRAINER_SUPER_NERD_MIGUEL` | MtMoon_B2F |
| 3 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT` | MtMoon_B2F |
| 3 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_2` | MtMoon_B2F |
| 3 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_3` | MtMoon_B2F |
| 3 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_4` | MtMoon_B2F |
| 3 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_5` | CeruleanCity |
| 3 | Trainer | `TRAINER_TEAM_ROCKET_GRUNT_6` | Route24 |
| 3 | Trainer | `TRAINER_YOUNGSTER_BEN` | Route3 |
| 3 | Trainer | `TRAINER_YOUNGSTER_CALVIN` | Route3 |
| 3 | Trainer | `TRAINER_YOUNGSTER_CHAD` | Route25 |
| 3 | Trainer | `TRAINER_YOUNGSTER_DAN` | Route25 |
| 3 | Trainer | `TRAINER_YOUNGSTER_DAVE` | Route11 |
| 3 | Trainer | `TRAINER_YOUNGSTER_DILLON` | Route11 |
| 3 | Trainer | `TRAINER_YOUNGSTER_EDDIE` | Route11 |
| 3 | Trainer | `TRAINER_YOUNGSTER_JOEY` | Route25 |
| 3 | Trainer | `TRAINER_YOUNGSTER_JOSH` | MtMoon_1F |
| 3 | Trainer | `TRAINER_YOUNGSTER_TIMMY` | Route24 |
| 3 | Trainer | `TRAINER_YOUNGSTER_TYLER` | SSAnne_1F_Room2 |
| 3 | Trainer | `TRAINER_YOUNGSTER_YASU` | Route11 |
| 2 | Trainer | `TRAINER_BUG_CATCHER_ANTHONY` | ViridianForest |
| 2 | Trainer | `TRAINER_BUG_CATCHER_CHARLIE` | ViridianForest |
| 2 | Trainer | `TRAINER_BUG_CATCHER_DOUG` | ViridianForest |
| 2 | Trainer | `TRAINER_BUG_CATCHER_RICK` | ViridianForest |
| 2 | Trainer | `TRAINER_BUG_CATCHER_SAMMY` | ViridianForest |
