# Drift Parts

A **Drift tire compound** and **Drift springs and dampers** for every car in **Forza Horizon 2 on PC (Azimuth)**, bought in the Upgrade Shop like any other part. It's an [Extra Options](https://github.com/RokkuDayo/ForzaHorizon2_ExtraOptions) plugin, it works on modded cars too, and it was made to go with [Drift Zones](https://github.com/Philipp55112/fh2-drift-zones).

**This is a first build (0.1.0).** It works and has been driven, but the feel is one person's first pass. Every number is in an ini file, so you can change it.

## What it adds

In the Garage, go to **Upgrades > Custom upgrade**. Both parts are last in their lists.

- **Drift Tire Compound** (Tires and Rims > Tyre Compound, 3,000 CR): grips like the Street tire up to its peak, then keeps most of that grip at big slip angles and under wheelspin. At full slide it holds about 94% of its peak sideways grip, where the Street tire holds about 85%, so the car holds a drift instead of snapping. It's a Street-level tire, so a car that came on Sport or Race tires drops in PI.
- **Drift Springs and Dampers** (Platform and Handling > Spring and Dampers, 2,500 CR): your car's Race springs and dampers with softer springs available (down to half the Race suspension's softest) and a lower ride height available (15 mm lower). Everything can be tuned afterwards.

## How it drives, so far

From the first in-game drive:

- It slides and holds a drift as intended.
- It needs a fair amount of power to break the rear loose.
- Pushed past a big angle, the car spins.
- Switching from side to side is a little slow.

If you want it different, open `plugins\DriftParts.ini` in the mod's folder and restart the game after changing it:

| You want | Try |
|---|---|
| The rear to break loose with less power | `PowerGrip=0.9` (or lower) |
| More angle before it spins, easier to catch | `SideFalloff=0.2` |
| Less grip overall, a looser car | `SideGrip=0.9` |
| A grippier drift tire for fast cars | `BaseCompound=6` (Sport) |

These are suggestions, not tested settings. If you find numbers you like, an issue or pull request with them is welcome.

## Requirements

| Requirement | Version tested |
|---|---|
| **Forza Horizon 2** (Xbox One version) | Build 1.0.0.64 |
| **Azimuth** | 1.0.1 |
| **[Extra Options](https://github.com/RokkuDayo/ForzaHorizon2_ExtraOptions)** by RokkuDayo | 0.4.4 and 0.6.0 |

## Install

1. Close the game.
2. Download `DriftParts-0.1.0.zip` from **Releases** and drag the `mods` folder inside it onto your Azimuth folder (the one with `azimuth.exe`). You get:

   ```
   mods\Forza Horizon 2\Drift Parts\mod.json
   mods\Forza Horizon 2\Drift Parts\database.json
   mods\Forza Horizon 2\Drift Parts\strings.json
   mods\Forza Horizon 2\Drift Parts\plugins\DriftParts.dll
   mods\Forza Horizon 2\Drift Parts\plugins\DriftParts.ini
   ```

   That's where Extra Options 0.6 keeps Forza Horizon 2's mods. On Extra Options 0.4, move the `Drift Parts` folder up into `mods` itself.

3. Start the game. Extra Options turns on new mod folders by itself.

`FH2_ExtraOptions.log` beside `azimuth.exe` should then have a line like `DRIFT PARTS: added drift tires for 354 cars and drift suspension for 354`.

## Good to know

- **Camber and caster:** the drift suspension is built with caster 7 and camber -3 front, -1 rear, but the Tune page keeps showing your car's own alignment after you fit it. Set the alignment you want on the Tune page.
- **Steering angle isn't part of it yet.** The game keeps steering angle per car, not per part, so the suspension can't carry more lock. `[SteeringTest]` in the ini is an experiment that raises it on one car you name; it's off by default.
- **Other mods:** it adds rows to the game's database and hooks nothing, so it doesn't interfere with other mods' code. Its parts are numbered the game's own way (car × 1000 + n), which matters with the CommunityGarage mod: that one builds a car carrying a part numbered for "another car" with stock parts instead. A car another mod has already given a part numbered 716 or 816 is left without drift parts.
- **Saves:** a car fitted with one of these parts needs this mod to load it, as with any part a mod adds. Before uninstalling, put other tires and springs back on those cars.

## Uninstall

1. Put other tires and springs back on any car that has these (see above).
2. Delete the `Drift Parts` folder and its line in `loadorder.txt` beside it.

No game file is changed. The parts only exist in the game's database in memory while the game runs.

## How it works

Once per launch, when Extra Options hands plugins its merged in-memory database, the plugin:

- copies one stock tire compound and its sideways and under-power friction curves, scaling how far each curve drops past its peak, and gives every car one shop row for the new compound;
- copies each car's best road springs and dampers (Race where it has them, Rally on the few off-roaders with nothing else) with the wider limits, as a new part;
- adds the two shop labels, whose text comes from `strings.json`.

The game reads compounds, friction curves and spring physics by id, so it picks the new rows up like its own.

## Building

Visual Studio Build Tools (x64): `build.cmd` builds `release\Drift Parts\plugins\DriftParts.dll` and `build\run_on_copy.exe`.

Tests run the plugin against a copy of the game's database, with no game running:

```
python tests\check.py prepare "<game folder>" copy.slt
build\run_on_copy.exe "release\Drift Parts\plugins\DriftParts.dll" copy.slt
python tests\check.py verify copy.slt
```

`prepare` also takes a database file: Extra Options 0.6 keeps the one it merged from every installed mod in `mods\Cache\fh2-db\merged.sqlite`, which checks the mods' cars as well. `verify` expects the default ini, with `[SteeringTest] Car=1022` to check the steering test too.

## Credits

- **[Forza Horizon 2 Extra Options](https://github.com/RokkuDayo/ForzaHorizon2_ExtraOptions)** by Arthur (RokkuDayo), GPL-3.0. Drift Parts is an Extra Options plugin: it uses Extra Options' mod loader, its plugin API (`include\fh2_plugin.h` is Extra Options' plugin API header), and the merged in-memory game database it hands to plugins.
- **[Drift Zones](https://github.com/Philipp55112/fh2-drift-zones)** by Philipp55112, the reason to want drift parts. Nothing from it is included.
- **[Azimuth](https://almost-there-games.itch.io/azimuth)** by Almost There Games, which runs Forza Horizon 2 on Windows. Nothing from it is included.

Not affiliated with or endorsed by Playground Games, Turn 10 Studios, Microsoft, Almost There Games or the authors of Extra Options and Drift Zones. No game files are included: you need your own copy of Forza Horizon 2.

## AI disclosure

This mod was built with Claude, Anthropic's AI assistant, which did most of the reverse engineering, code and documentation. The author directed the work and tested it in game.

## License

GPL-3.0, see [LICENSE](LICENSE). It's the same licence as Extra Options, whose plugin API header is included.
