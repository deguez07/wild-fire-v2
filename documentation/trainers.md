# Trainers

## Viridian Forest

All five are Bug Catchers. Party data lives in `src/data/trainer_parties.h`.
All are rematchable with the VS Seeker (see `sRematches` in `src/vs_seeker.c`);
a rematch uses the same team.

Approximate order when travelling from Viridian City to Pewter City
(based on map coordinates, not verified in game): Rick, Doug, Anthony, Charlie, Sammy.

### Rick (`TRAINER_BUG_CATCHER_RICK`) - all level 8

| # | Pokemon |
|---|---------|
| 1 | Vulpix |
| 2 | Snubbull |
| 3 | Charmander |
| 4 | Cyndaquil |

### Doug (`TRAINER_BUG_CATCHER_DOUG`) - all level 8

| # | Pokemon |
|---|---------|
| 1 | Ditto |
| 2 | Squirtle |
| 3 | Mudkip |
| 4 | Totodile |

### Anthony (`TRAINER_BUG_CATCHER_ANTHONY`) - all level 10

| # | Pokemon |
|---|---------|
| 1 | Unown |
| 2 | Bulbasaur |
| 3 | Chikorita |
| 4 | Treecko |

### Charlie (`TRAINER_BUG_CATCHER_CHARLIE`) - all level 10

| # | Pokemon |
|---|---------|
| 1 | Dratini |
| 2 | Larvitar |
| 3 | Machop |
| 4 | Jigglypuff |

### Sammy (`TRAINER_BUG_CATCHER_SAMMY`) - all level 12

| # | Pokemon |
|---|---------|
| 1 | Snorunt |
| 2 | Venonat |
| 3 | Spheal |
| 4 | Chansey |

## Route 22

### Rival Terry, first battle (`TRAINER_RIVAL_ROUTE22_EARLY_SQUIRTLE` / `_BULBASAUR` / `_CHARMANDER`) - all level 12

Single battle, IVs 50, default movesets, no items. The three trainer IDs (one per starter) share the same team.
Not rematchable.

| # | Pokemon |
|---|---------|
| 1 | Bulbasaur |
| 2 | Charmander |
| 3 | Squirtle |
| 4 | Dratini |
| 5 | Abra |
| 6 | Pikachu |

## Pewter City Gym

Both are double battles. Party data lives in `src/data/trainer_parties.h`. Neither is in `sRematches`.

### Camper Liam (`TRAINER_CAMPER_LIAM`)

| # | Pokemon | Level | Moves |
|---|---------|-------|-------|
| 1 | Aron | 13 | Tackle, Harden, Metal Claw |
| 2 | Omanyte | 13 | Water Gun, Withdraw, Ancient Power |
| 3 | Kabuto | 14 | Scratch, Harden, Ancient Power |
| 4 | Corsola | 14 | Tackle, Harden, Ancient Power, Bubble Beam |
| 5 | Larvitar | 14 | Bite, Scary Face, Rock Tomb |
| 6 | Shuckle | 15 | Wrap, Withdraw, Rock Tomb, Bide |

### Leader Brock (`TRAINER_LEADER_BROCK`) - all level 16

| # | Pokemon | Level | Moves |
|---|---------|-------|-------|
| 1 | Onix | 16 | Rock Tomb, Bind, Screech, Rock Throw |
| 2 | Solrock | 16 | Rock Throw, Confusion, Harden, Fire Spin |
| 3 | Nosepass | 16 | Rock Tomb, Thunder Wave, Harden, Rock Throw |
| 4 | Rhyhorn | 16 | Stomp, Rock Tomb, Horn Attack, Scary Face |
| 5 | Lunatone | 16 | Rock Throw, Confusion, Hypnosis, Harden |
| 6 | Sudowoodo | 16 | Rock Tomb, Mimic, Flail, Slam |

## Route 3

All eight are double battles with default movesets and no items. All are rematchable with the VS Seeker
(see `sRematches` in `src/vs_seeker.c`); rematch variants (`_2`, `_3`, `_4`) use the same team as the first battle.

Order when travelling west to east from Pewter City (based on map coordinates, not verified in game): Colton, Ben, Janice, Greg, Calvin, Sally, James, Robin.

### Bug Catcher Colton (`TRAINER_BUG_CATCHER_COLTON`) - all level 14

| # | Pokemon |
|---|---------|
| 1 | Unown |
| 2 | Surskit |
| 3 | Treecko |
| 4 | Nincada |

### Youngster Ben (`TRAINER_YOUNGSTER_BEN`) - all level 15

| # | Pokemon |
|---|---------|
| 1 | Charmander |
| 2 | Squirtle |
| 3 | Treecko |
| 4 | Pikachu |

### Lass Janice (`TRAINER_LASS_JANICE`) - all level 15

| # | Pokemon |
|---|---------|
| 1 | Whismur |
| 2 | Meowth |
| 3 | Ralts |
| 4 | Togepi |

### Bug Catcher Greg (`TRAINER_BUG_CATCHER_GREG`) - all level 15

| # | Pokemon |
|---|---------|
| 1 | Paras |
| 2 | Hoppip |
| 3 | Natu |
| 4 | Kakuna |

### Youngster Calvin (`TRAINER_YOUNGSTER_CALVIN`) - all level 16

| # | Pokemon |
|---|---------|
| 1 | Lotad |
| 2 | Abra |
| 3 | Machop |
| 4 | Jigglypuff |

### Lass Sally (`TRAINER_LASS_SALLY`) - all level 16

| # | Pokemon |
|---|---------|
| 1 | Swablu |
| 2 | Clefairy |
| 3 | Jigglypuff |
| 4 | Slugma |

### Bug Catcher James (`TRAINER_BUG_CATCHER_JAMES`) - all level 16

| # | Pokemon |
|---|---------|
| 1 | Spoink |
| 2 | Grimer |
| 3 | Bellsprout |
| 4 | Shuckle |

### Lass Robin (`TRAINER_LASS_ROBIN`) - all level 17

| # | Pokemon |
|---|---------|
| 1 | Horsea |
| 2 | Pikachu |
| 3 | Magnemite |
| 4 | Growlithe |

## Mt. Moon

All Mt. Moon trainers are double battles (`.doubleBattle = TRUE`, scripted with
`trainerbattle_double`), like Brock's gym. B1F has no trainers.
All except Miguel are rematchable with the VS Seeker (see `sRematches` in
`src/vs_seeker.c`); a rematch uses the same team. Miguel is excluded because
he is a scripted fossil encounter, not a sight-based trainer.

Approximate order (based on map coordinates, not verified in game): 1F Jovan,
Robby, Iris, Kent, Josh, Marcos, Miriam; B2F Grunt 2, Grunt 1, Miguel, Grunt 3, Grunt 4.

### Mt. Moon 1F

#### Super Nerd Jovan (`TRAINER_SUPER_NERD_JOVAN`) - all level 18

| # | Pokemon |
|---|---------|
| 1 | Chinchou |
| 2 | Magnemite |
| 3 | Growlithe |
| 4 | Farfetch'd |

#### Lass Iris (`TRAINER_LASS_IRIS`) - all level 17

| # | Pokemon |
|---|---------|
| 1 | Aipom |
| 2 | Eevee |
| 3 | Doduo |
| 4 | Cacnea |

#### Bug Catcher Robby (`TRAINER_BUG_CATCHER_ROBBY`) - all level 18

| # | Pokemon |
|---|---------|
| 1 | Cacnea |
| 2 | Exeggcute |
| 3 | Mawile |
| 4 | Anorith |

#### Bug Catcher Kent (`TRAINER_BUG_CATCHER_KENT`) - all level 18

| # | Pokemon |
|---|---------|
| 1 | Lileep |
| 2 | Drowzee |
| 3 | Beldum |
| 4 | Smeargle |

#### Youngster Josh (`TRAINER_YOUNGSTER_JOSH`) - all level 19

| # | Pokemon |
|---|---------|
| 1 | Sableye |
| 2 | Qwilfish |
| 3 | Tentacool |
| 4 | Murkrow |

#### Lass Miriam (`TRAINER_LASS_MIRIAM`) - all level 19

| # | Pokemon |
|---|---------|
| 1 | Voltorb |
| 2 | Elekid |
| 3 | Gligar |
| 4 | Wigglytuff |

#### Hiker Marcos (`TRAINER_HIKER_MARCOS`) - all level 19

| # | Pokemon |
|---|---------|
| 1 | Nosepass |
| 2 | Corsola |
| 3 | Koffing |
| 4 | Raticate |

### Mt. Moon B2F

#### Rocket Grunt 2 (`TRAINER_TEAM_ROCKET_GRUNT_2`) - all level 20

| # | Pokemon |
|---|---------|
| 1 | Murkrow |
| 2 | Gligar |
| 3 | Houndour |
| 4 | Koffing |

#### Rocket Grunt 1 (`TRAINER_TEAM_ROCKET_GRUNT`) - all level 20

| # | Pokemon |
|---|---------|
| 1 | Raticate |
| 2 | Forretress |
| 3 | Nidorino |
| 4 | Marowak |

#### Rocket Grunt 3 (`TRAINER_TEAM_ROCKET_GRUNT_3`) - all level 20

| # | Pokemon |
|---|---------|
| 1 | Furret |
| 2 | Nidorina |
| 3 | Teddiursa |
| 4 | Phanpy |

#### Rocket Grunt 4 (`TRAINER_TEAM_ROCKET_GRUNT_4`) - all level 21

| # | Pokemon |
|---|---------|
| 1 | Raichu |
| 2 | Haunter |
| 3 | Loudred |
| 4 | Vibrava |

#### Super Nerd Miguel (`TRAINER_SUPER_NERD_MIGUEL`) - all level 21

| # | Pokemon |
|---|---------|
| 1 | Plusle |
| 2 | Minun |
| 3 | Lickitung |
| 4 | Mightyena |
