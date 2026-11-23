# Pokémon Locations

Source: `../src/data/wild_encounters.json`

## Route 1 (`MAP_ROUTE1`)

### Grass (land encounters)

Encounter rate: 50. The data is identical for FireRed and LeafGreen.

| Pokémon | Level | Total chance |
|---------|-------|--------------|
| Rattata | 5     | 25%          |
| Meowth  | 5     | 25%          |
| Sentret | 5     | 25%          |
| Skitty  | 5     | 25%          |

### Water, rock smash, and fishing

None.

## Route 2 (`MAP_ROUTE2`)

### Grass (land encounters)

Encounter rate: 50. The data is identical for FireRed and LeafGreen.

| Pokémon   | Level | Total chance |
|-----------|-------|--------------|
| Slakoth   | 7     | 25%          |
| Spoink    | 7     | 25%          |
| Poochyena | 7     | 25%          |
| Zigzagoon | 7     | 25%          |

## Viridian Forest (`MAP_VIRIDIAN_FOREST`)

### Grass (land encounters)

Encounter rate: 50. The data is identical for FireRed and LeafGreen.

| Pokémon  | Level | Total chance |
|----------|-------|--------------|
| Caterpie | 9     | 25%          |
| Weedle   | 9     | 25%          |
| Ledyba   | 9     | 25%          |
| Spinarak | 9     | 25%          |

## Route 3 (`MAP_ROUTE3`)

### Grass (land encounters)

Encounter rate: 21. The data is identical for FireRed and LeafGreen.

| Pokémon    | Level | Total chance |
|------------|-------|--------------|
| Oddish     | 17    | 25%          |
| Bellsprout | 17    | 25%          |
| Mareep     | 17    | 25%          |
| Pikachu    | 17    | 25%          |

## Mt. Moon 1F (`MAP_MT_MOON_1F`)

### Grass (land encounters)

Encounter rate: 7. The data is identical for FireRed and LeafGreen.

| Pokémon | Level | Total chance |
|---------|-------|--------------|
| Numel | 20 | 25% |
| Spinda | 20 | 25% |
| Baltoy | 20 | 25% |
| Swinub | 20 | 25% |

## Mt. Moon B1F (`MAP_MT_MOON_B1F`)

### Grass (land encounters)

Encounter rate: 5. The data is identical for FireRed and LeafGreen.

| Pokémon | Level | Total chance |
|---------|-------|--------------|
| Kecleon | 21 | 25% |
| Magnemite | 21 | 25% |
| Cubone | 21 | 25% |
| Duskull | 21 | 25% |

## Mt. Moon B2F (`MAP_MT_MOON_B2F`)

### Grass (land encounters)

Encounter rate: 7. The data is identical for FireRed and LeafGreen.

| Pokémon | Level | Total chance |
|---------|-------|--------------|
| Aron | 22 | 25% |
| Clefairy | 22 | 25% |
| Snubbull | 22 | 25% |
| Dunsparce | 22 | 25% |

## Pallet Town (`MAP_PALLET_TOWN`)

### Fishing

Encounter rate: 100. The data is identical for FireRed and LeafGreen. Surfing (water) encounters are unchanged.

| Rod       | Pokémon  | Level | Chance |
|-----------|----------|-------|--------|
| Old Rod   | Magikarp | 8     | 100%   |
| Good Rod  | Feebas   | 9     | 100%   |
| Super Rod | Poliwag  | 10    | 100%   |

## Viridian City (`MAP_VIRIDIAN_CITY`)

### Fishing

Encounter rate: 100. The data is identical for FireRed and LeafGreen. Surfing (water) encounters are unchanged.

| Rod       | Pokémon | Level | Chance |
|-----------|---------|-------|--------|
| Old Rod   | Wooper  | 8     | 100%   |
| Good Rod  | Lotad   | 9     | 100%   |
| Super Rod | Spheal  | 10    | 100%   |

## Route 22 (`MAP_ROUTE22`)

### Fishing

Encounter rate: 100. The data is identical for FireRed and LeafGreen. Other encounters are unchanged.

| Rod       | Pokémon  | Level | Chance |
|-----------|----------|-------|--------|
| Old Rod   | Clamperl | 11    | 100%   |
| Good Rod  | Krabby   | 12    | 100%   |
| Super Rod | Horsea   | 13    | 100%   |

### Grass (land encounters)

Encounter rate: 50. The data is identical for FireRed and LeafGreen.

| Pokémon   | Level | Total chance |
|-----------|-------|--------------|
| Sandshrew | 11    | 25%          |
| Diglett   | 11    | 25%          |
| Geodude   | 11    | 25%          |
| Nosepass  | 11    | 25%          |

## Safari Zone North (`MAP_SAFARI_ZONE_NORTH`)

### Grass (land encounters)

Encounter rate: 21. The data is identical for FireRed and LeafGreen.

| Pokémon  | Level | Total chance |
|----------|-------|--------------|
| Abra     | 8     | 25%          |
| Ralts    | 8     | 25%          |
| Houndour | 8     | 25%          |
| Eevee    | 8     | 25%          |

### Fishing

Encounter rate: 100. The data is identical for FireRed and LeafGreen. Surfing (water) encounters are unchanged.

| Rod       | Pokémon   | Level | Chance |
|-----------|-----------|-------|--------|
| Old Rod   | Dratini   | 8     | 100%   |
| Good Rod  | Totodile  | 8     | 100%   |
| Super Rod | Mudkip    | 8     | 100%   |

### Capture rate changes

These are global changes to `../src/data/pokemon/species_info.h`, so they apply everywhere the species appears, not just in the Safari Zone. Species whose original capture rate was already above 200 were left unchanged.

| Pokémon  | Old capture rate | New capture rate |
|----------|------------------|------------------|
| Houndour | 120              | 200              |
| Eevee    | 45               | 200              |
| Dratini  | 45               | 200              |
| Totodile | 45               | 200              |
| Mudkip   | 45               | 200              |

## Safari Zone Center (`MAP_SAFARI_ZONE_CENTER`)

### Grass (land encounters)

Encounter rate: 21. The data is identical for FireRed and LeafGreen.

| Pokémon  | Level | Total chance |
|----------|-------|--------------|
| Larvitar | 8     | 25%          |
| Bagon    | 8     | 25%          |
| Beldum   | 8     | 25%          |
| Gastly   | 8     | 25%          |

### Fishing

Encounter rate: 100. The data is identical for FireRed and LeafGreen. Surfing (water) encounters are unchanged.

| Rod       | Pokémon  | Level | Chance |
|-----------|----------|-------|--------|
| Old Rod   | Staryu   | 8     | 100%   |
| Good Rod  | Slowpoke | 8     | 100%   |
| Super Rod | Omanyte  | 8     | 100%   |

### Capture rate changes

These are global changes to `../src/data/pokemon/species_info.h`, so they apply everywhere the species appears, not just in the Safari Zone. Species whose original capture rate was already above 200 were left unchanged.

| Pokémon  | Old capture rate | New capture rate |
|----------|------------------|------------------|
| Larvitar | 45               | 200              |
| Bagon    | 45               | 200              |
| Beldum   | 3                | 200              |
| Gastly   | 190              | 200              |
| Slowpoke | 190              | 200              |
| Omanyte  | 45               | 200              |

Staryu remains at capture rate 225.

## Safari Zone East (`MAP_SAFARI_ZONE_EAST`)

### Grass (land encounters)

Encounter rate: 21. The data is identical for FireRed and LeafGreen.

| Pokémon   | Level | Total chance |
|-----------|-------|--------------|
| Treecko   | 8     | 25%          |
| Chikorita | 8     | 25%          |
| Cyndaquil | 8     | 25%          |
| Torchic   | 8     | 25%          |

### Fishing

Encounter rate: 100. The data is identical for FireRed and LeafGreen. Surfing (water) encounters are unchanged.

| Rod       | Pokémon  | Level | Chance |
|-----------|----------|-------|--------|
| Old Rod   | Kabuto   | 8     | 100%   |
| Good Rod  | Marill   | 8     | 100%   |
| Super Rod | Remoraid | 8     | 100%   |

### Capture rate changes

These are global changes to `../src/data/pokemon/species_info.h`, so they apply everywhere the species appears, not just in the Safari Zone. Species whose original capture rate was already above 200 were left unchanged.

| Pokémon   | Old capture rate | New capture rate |
|-----------|------------------|------------------|
| Treecko   | 45               | 200              |
| Chikorita | 45               | 200              |
| Cyndaquil | 45               | 200              |
| Torchic   | 45               | 200              |
| Kabuto    | 45               | 200              |
| Marill    | 190              | 200              |
| Remoraid  | 190              | 200              |

## Safari Zone West (`MAP_SAFARI_ZONE_WEST`)

### Grass (land encounters)

Encounter rate: 21. The data is identical for FireRed and LeafGreen.

| Pokémon    | Level | Total chance |
|------------|-------|--------------|
| Charmander | 8     | 25%          |
| Bulbasaur  | 8     | 25%          |
| Machop     | 8     | 25%          |
| Trapinch   | 8     | 25%          |

### Fishing

Encounter rate: 100. The data is identical for FireRed and LeafGreen. Surfing (water) encounters are unchanged.

| Rod       | Pokémon  | Level | Chance |
|-----------|----------|-------|--------|
| Old Rod   | Squirtle | 8     | 100%   |
| Good Rod  | Carvanha | 8     | 100%   |
| Super Rod | Corphish | 8     | 100%   |

### Capture rate changes

These are global changes to `../src/data/pokemon/species_info.h`, so they apply everywhere the species appears, not just in the Safari Zone. Species whose original capture rate was already above 200 were left unchanged.

| Pokémon    | Old capture rate | New capture rate |
|------------|------------------|------------------|
| Charmander | 45               | 200              |
| Bulbasaur  | 45               | 200              |
| Machop     | 180              | 200              |
| Squirtle   | 45               | 200              |

Trapinch remains at 255, Carvanha at 225, and Corphish at 205.
