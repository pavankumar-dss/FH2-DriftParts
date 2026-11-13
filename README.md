# Drift Parts

A **Drift tire compound**, **Drift springs and dampers**, a **Drift Angle Kit**, a **Drift Transmission** and a **Drift Differential** for every car in **Forza Horizon 2 on PC (Azimuth)**, bought in the Upgrade Shop like any other part. It's an [Extra Options](https://github.com/RokkuDayo/ForzaHorizon2_ExtraOptions) plugin, it works on modded cars too, and it was made to go with [Drift Zones](https://github.com/Philipp55112/fh2-drift-zones).

**This is an early build (0.3.0).** It works and has been driven, but the feel is one person's first pass. Every number is in an ini file, so you can change it.

## What it adds

In the Garage, go to **Upgrades > Custom upgrade**. Each part is last in its list.

- **Drift Tire Compound** (Tires and Rims > Tyre Compound, 3,000 CR): grips like the Street tire up to its peak, then keeps most of that grip at big slip angles and under wheelspin. At full slide it holds about 94% of its peak sideways grip, where the Street tire holds about 85%, so the car holds a drift instead of snapping. It's a Street-level tire, so a car that came on Sport or Race tires drops in PI.
- **Drift Springs and Dampers** (Platform and Handling > Spring and Dampers, 2,500 CR): your car's Race springs and dampers with softer springs available (down to half the Race suspension's softest) and a lower ride height available (15 mm lower). Everything can be tuned afterwards.
- **Drift Angle Kit** (Platform and Handling > Front Anti-roll Bars, 2,000 CR): your car's Race front anti-roll bar (same stiffness range, tuned the same way) with a drift steering angle kit. It's meant for drift builds: the extra lock doesn't help grip driving. The front wheels turn to 60° on a wheel and 50° on a controller or keys, where stock cars stop at 42° and 29°, and they swing back faster when you countersteer.

- **Drift Transmission** (Drivetrain > Transmission, 2,500 CR): your car's Race gearbox made into a close-ratio one. The gears are evenly spaced, so every upshift drops the revs by the same amount and none leaves the engine below its power band, the way a wide 4-speed does. First gear is 15% taller than the Race gearbox's (usable mid-slide, less eager to spin up) and the final drive 10% shorter (more revs everywhere, a little less top speed). It keeps the Race gearbox's number of gears, but has at least 5. Fully tunable.
- **Drift Differential** (Drivetrain > Differential, 2,250 CR): your car's Race differential with the driven wheels locked hard: 100% under power and 50% off the throttle, as the Tune page shows them. That's the rear on rear-wheel drive and the front on front-wheel drive. Four-wheel drive also gets a looser front diff (25% / 0%) and at least 80% of the torque sent to the rear. Fully tunable.

On a BMW M3, for example, the Race gearbox is 2.89 / 1.99 / 1.49 / 1.16 / 0.94 / 0.78 with a 4.30 final drive: shifting from 1st to 2nd drops the revs by 31%. The Drift Transmission is 2.23 / 1.81 / 1.47 / 1.19 / 0.96 / 0.78 with a 4.73 final drive: every shift drops them by about 19%.

With the Drift Angle Kit fitted, **Tuning > Alignment** has a **Steering lock** slider (30° to 80°) under Front Caster. It's kept per car and takes effect straight away; the grey marker is the kit's default.

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
| A different default lock for the Drift Angle Kit | `[AngleKit] MaxAngle` (or the Steering lock slider, per car) |
| Slower or faster countersteer with the kit | `CountersteerRate` (stock 105; `0` keeps the car's own) |
| Closer or wider gears | `[Transmission] FirstGearScale` (lower = a taller first gear, closer gears) |
| Shorter or taller gearing all round | `TopGearScale` (higher = shorter) |
| A set number of gears | `Gears=6` (`0` keeps the car's own, at least 5) |
| More or less lock off the throttle | `[Differential] Decel` (or the Tune page, per car) |

These are suggestions, not tested settings. If you find numbers you like, an issue or pull request with them is welcome.

## Requirements

| Requirement | Version tested |
|---|---|
| **Forza Horizon 2** (Xbox One version) | Build 1.0.0.64 |
| **Azimuth** | 1.0.1 |
| **[Extra Options](https://github.com/RokkuDayo/ForzaHorizon2_ExtraOptions)** by RokkuDayo | 0.6.0 (0.1.0 also ran on 0.4.4) |

## Install

1. Close the game.
2. Download `DriftParts-0.3.0.zip` from **Releases** and drag the `mods` folder inside it onto your Azimuth folder (the one with `azimuth.exe`). You get:

   ```
   mods\Forza Horizon 2\Drift Parts\mod.json
   mods\Forza Horizon 2\Drift Parts\database.json
   mods\Forza Horizon 2\Drift Parts\strings.json
   mods\Forza Horizon 2\Drift Parts\plugins\DriftParts.dll
   mods\Forza Horizon 2\Drift Parts\plugins\DriftParts.ini
   ```

   That's where Extra Options 0.6 keeps Forza Horizon 2's mods. On Extra Options 0.4, move the `Drift Parts` folder up into `mods` itself.

3. Start the game. Extra Options turns on new mod folders by itself.

`FH2_ExtraOptions.log` beside `azimuth.exe` should then have the lines `DRIFT PARTS: Steering lock row ready for the Tuning screen.` and `DRIFT PARTS: added drift tires for 354 cars, drift suspension for 354, the angle kit for 354, and a drift transmission for 351 drivetrains and a drift differential for 357` (more with modded cars).

Updating from 0.1.0 or 0.2: copy the new files over the old ones (your `DriftParts.locks.ini` stays). If you changed `DriftParts.ini`, copy your values into the new one: it has two new sections. 0.1.0's `[SteeringTest]` is gone; the Drift Angle Kit replaces it.

## Good to know

- **Camber and caster:** the drift suspension is built with caster 7 and camber -3 front, -1 rear, but the Tune page keeps showing your car's own alignment after you fit it. Set the alignment you want on the Tune page.
- **The Drift Angle Kit's lock is set on the car you're driving.** The game keeps steering angle per car, not per part, so the plugin raises it on the car Extra Options says you're driving, each time that car is built with the kit. AI cars with the kit (there shouldn't be any) keep their own lock.
- **The Steering lock slider** only shows on a car with the Drift Angle Kit fitted. Per-car values are kept in `plugins\DriftParts.locks.ini`.
- **Where you'll see the extra lock.** The front wheels on screen do turn further with the kit: in a slow full-lock turn or a donut they stick right out past the bodywork at 80°, where at 30° they barely show. On a controller or keys the game trims lock at higher speeds for every car. Measured on a BMW M3 driven on keys with the kit at 80°: the front wheels reached about 60° up to 80 km/h, about 44° at 80–100 km/h and about 23° above 100 km/h. The stock car never passed 29°.
- **More lock isn't always more angle.** On that M3 the driver didn't feel a big difference with or without the kit, even though the wheels turned twice as far. How much it helps seems to depend on the car (and on the tires and suspension holding the slide).
- **The diff's and gearbox's numbers are only the defaults.** The Tune page keeps a car's own tune when you change parts, so on a car you've tuned before, the Differential tab keeps showing (and using) your settings after you fit the Drift Differential. **Setup manager > Reset default tuning settings** brings up the part's own (100% / 50%). The gearing is the same.
- **Gearboxes and diffs belong to a drivetrain, not a car.** A car with a drivetrain swap (an AWD conversion, say) gets the Drift Transmission and Drift Differential for that drivetrain as well. A car's drivetrain swaps share them with any other car on the same drivetrain, as the game's own gearboxes do.
- **With [Gearbox Upgrades](https://github.com/pavankumar-dss/FH2-GearboxUpgrades):** the Drift Transmission is listed after the game's own gearboxes and before Gearbox Upgrades' 1- to 8-speed ones. The two don't share any ids or levels and work in either load order.
- **Other mods:** it adds rows to the game's database and patches no game code. For the Steering lock row it adds a named property to the Tuning screen and chains onto Extra Options' own screen-loading hook, the way Enhanced Tuning does; the two work side by side (tested with Enhanced Tuning installed). Its parts are numbered the game's own way (car × 1000 + n; drivetrain × 1000 + n for the gearbox and diff), which matters with the CommunityGarage mod: that one builds a car carrying a part numbered for "another car" with stock parts instead. A car another mod has already given a part numbered 716 or 816 is left without drift parts.
- **Saves:** a car fitted with one of these parts needs this mod to load it, as with any part a mod adds. Before uninstalling, put other tires, springs, front anti-roll bars, transmissions and differentials back on those cars.

## Uninstall

1. Put other tires, springs, front anti-roll bars, transmissions and differentials back on any car that has these (see above).
2. Delete the `Drift Parts` folder and its line in `loadorder.txt` beside it.

No game file is changed. The parts only exist in the game's database in memory while the game runs. The only file it writes is its own `DriftParts.locks.ini`.

## How it works

Once per launch, when Extra Options hands plugins its merged in-memory database, the plugin:

- copies one stock tire compound and its sideways and under-power friction curves, scaling how far each curve drops past its peak, and gives every car one shop row for the new compound;
- copies each car's best road springs and dampers (Race where it has them, Rally on the few off-roaders with nothing else) with the wider limits, as a new part;
- gives every car a Drift Angle Kit: a copy of its Race front anti-roll bar row, as a new part;
- gives every drivetrain a car has a Drift Transmission (a copy of its Race gearbox with new ratios) and a Drift Differential (a copy of its Race diff with more lock);
- adds the five shop labels, whose text comes from `strings.json`.

The game reads compounds, friction curves, spring physics, anti-roll bars, gearboxes and diffs by id, so it picks the new rows up like its own.

The steering lock is a value of the car, not of a part, so the Drift Angle Kit's comes from code: Extra Options hands plugins the car you're driving once a frame, and when its fitted front anti-roll bar is the kit, the plugin sets the steering lock the car was built with (and the countersteer speed). The Tuning screen's row is a native property that slider binds to, plus a few lines of XAML added to that screen as it loads.

## Building

Visual Studio Build Tools (x64): `build.cmd` builds `release\Drift Parts\plugins\DriftParts.dll` and `build\run_on_copy.exe`.

Tests run the plugin against a copy of the game's database, with no game running:

```
python tests\check.py prepare "<game folder>" copy.slt
build\run_on_copy.exe "release\Drift Parts\plugins\DriftParts.dll" copy.slt
python tests\check.py verify copy.slt
```

`prepare` also takes a database file: Extra Options 0.6 keeps the one it merged from every installed mod in `mods\Cache\fh2-db\merged.sqlite`, which checks the mods' cars as well. `verify` expects the default ini. Run on a database an older Drift Parts already went through (0.1.0 or 0.2), it checks that only the parts that build didn't have are added. The Drift Angle Kit's lock and the Tuning row need the game: they were checked in game (the lock with TestKit reading the car's physics: 50° at full lock on keys with the kit, 29° without). The Drift Transmission and Drift Differential were checked in game on a BMW M3: listed by name, fitted, the car built with the part's numbers, and the Tune page showing the gearbox's ratios and, after a reset, the diff's 100% / 50%.

## Credits

- **[Forza Horizon 2 Extra Options](https://github.com/RokkuDayo/ForzaHorizon2_ExtraOptions)** by Arthur (RokkuDayo), GPL-3.0. Drift Parts is an Extra Options plugin: it uses Extra Options' mod loader, its plugin API (`include\fh2_plugin.h` is Extra Options' plugin API header), and the merged in-memory game database it hands to plugins.
- **[Enhanced Tuning](https://github.com/Pantsedit/FH2-EnhancedTuning)** by Pantsedit, GPL-3.0. The Steering lock row is added to the Tuning screen the way Enhanced Tuning adds its tab: its native slider property and Tuning-screen XAML code were adapted for `src\tune_row.c`.
- **[Drift Zones](https://github.com/Philipp55112/fh2-drift-zones)** by Philipp55112, the reason to want drift parts. Nothing from it is included.
- **[Azimuth](https://almost-there-games.itch.io/azimuth)** by Almost There Games, which runs Forza Horizon 2 on Windows. Nothing from it is included.

Not affiliated with or endorsed by Playground Games, Turn 10 Studios, Microsoft, Almost There Games or the authors of Extra Options and Drift Zones. No game files are included: you need your own copy of Forza Horizon 2.

## AI disclosure

This mod was built with Claude, Anthropic's AI assistant, which did most of the reverse engineering, code and documentation. The author directed the work and tested it in game.

## License

GPL-3.0, see [LICENSE](LICENSE). It's the same licence as Extra Options, whose plugin API header is included.
