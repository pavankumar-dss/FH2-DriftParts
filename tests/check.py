# prepare <game> <copy>: a copy of the game's database, with a snapshot of the tables the plugin adds to.
#   <game> can also be a database file: Extra Options 0.6 keeps the one it merged from every mod in
#   mods/Cache/fh2-db/merged.sqlite, which has the mods' cars too. <game> and <copy> can be the same file,
#   to re-snapshot a copy an older build already went through.
# verify <copy>: checks what the plugin added (expects the default DriftParts.ini).
#   From a database an older Drift Parts already went through (its marker's Version: 1 for 0.1.0, 2 for 0.2),
#   only what that build didn't add is expected: after 0.1.0 the angle kit, transmission and diff; after 0.2
#   the transmission and diff.
import sqlite3, shutil, os, sys, math
TABLES = ["List_TireCompound", "List_TireFrictionCurve", "List_TireFrictionMultiCurve", "List_UpgradeTireCompound",
          "List_UpgradeSpringDamper", "List_SpringDamperPhysics", "Upgrades", "Data_Car", "List_UpgradeAntiSwayFront",
          "List_UpgradeDrivetrainTransmission", "List_UpgradeDrivetrainDifferential", "List_UpgradeDrivetrain", "Data_Drivetrain"]
TRANS = "List_UpgradeDrivetrainTransmission"
DIFF = "List_UpgradeDrivetrainDifferential"
BEST = "(SELECT x.rowid FROM Before_List_UpgradeSpringDamper x WHERE x.Ordinal=u.Ordinal AND x.Level BETWEEN 0 AND 4 ORDER BY CASE WHEN x.Level<4 THEN x.Level ELSE -1 END DESC, x.rowid LIMIT 1)"
# The drivetrains the cars have (stock or swap), as the plugin picks them
CAR_DRIVETRAINS = ("SELECT DISTINCT DrivetrainID FROM Before_List_UpgradeDrivetrain WHERE Ordinal BETWEEN 1 AND 2000000 "
                   "AND Ordinal IN (SELECT Id FROM Before_Data_Car) AND DrivetrainID BETWEEN 1 AND 500000 "
                   "AND DrivetrainID IN (SELECT DrivetrainID FROM Before_Data_Drivetrain)")

def prepare(game, copy):
    mount = os.path.join(game, r"GameFiles\Anthem_1.0.0.64_x64__8wekyb3d8bbwe\Mount")
    src = os.path.join(mount, r"mediaoverride\Stripped\gamedbRC.slt")
    if not os.path.exists(src): src = os.path.join(mount, r"media\Stripped\gamedbRC.slt")
    if os.path.isfile(game): src = game
    if os.path.abspath(src) != os.path.abspath(copy): shutil.copyfile(src, copy)
    db = sqlite3.connect(copy, isolation_level=None)
    for t in TABLES:
        db.execute(f"DROP TABLE IF EXISTS Before_{t}")
        db.execute(f"CREATE TABLE Before_{t} AS SELECT * FROM {t}")
    marked = db.execute("SELECT COUNT(*) FROM sqlite_master WHERE name='FH2_DriftPartsReady'").fetchone()[0]
    db.execute("DROP TABLE IF EXISTS Before_Version")
    db.execute("CREATE TABLE Before_Version AS SELECT " + ("(SELECT MAX(Version) FROM FH2_DriftPartsReady)" if marked else "0") + " AS Version")
    print("prepared from", src)
    db.close()

fails = 0
def expect(ok, what):
    global fails
    print(("PASS: " if ok else "FAIL: ") + what)
    fails += 0 if ok else 1

def verify(copy):
    db = sqlite3.connect(copy); c = db.cursor()
    one = lambda s, *a: c.execute(s, a).fetchone()[0]
    for t in TABLES:
        if t == "Data_Car": continue
        expect(one(f"SELECT COUNT(*) FROM (SELECT * FROM Before_{t} EXCEPT SELECT * FROM {t})") == 0, f"{t}: the existing rows are untouched")
    cars = one("SELECT COUNT(*) FROM Data_Car")
    version = one("SELECT Version FROM Before_Version")
    if version: print(f"   (a database an older build already went through: marker version {version})")
    expect(one("SELECT COUNT(*) FROM (SELECT * FROM Data_Car EXCEPT SELECT * FROM Before_Data_Car)") == 0, "no car is changed")
    for t in ("List_UpgradeDrivetrain", "Data_Drivetrain"):
        expect(one(f"SELECT COUNT(*) FROM {t}") == one(f"SELECT COUNT(*) FROM Before_{t}"), f"{t}: nothing added")
    if version < 1: verify_tires_and_suspension(c, one, cars)
    else:
        for t in TABLES[:6]:
            expect(one(f"SELECT COUNT(*) FROM {t}") == one(f"SELECT COUNT(*) FROM Before_{t}"), f"{t}: nothing added")
    if version < 2: verify_angle_kit(c, one, cars)
    else:
        expect(one("SELECT COUNT(*) FROM List_UpgradeAntiSwayFront") == one("SELECT COUNT(*) FROM Before_List_UpgradeAntiSwayFront"),
               "List_UpgradeAntiSwayFront: nothing added")
    verify_transmissions(c, one)
    verify_diffs(c, one)
    expect(one("SELECT COUNT(*) FROM Upgrades") == one("SELECT COUNT(*) FROM Before_Upgrades") + {0: 5, 1: 3, 2: 2}[version], "no other label added")
    for t, level in ((TRANS, 15), (DIFF, 16)):
        expect(one(f"SELECT COUNT(*) FROM (SELECT 1 FROM {t} GROUP BY DrivetrainID, Level HAVING COUNT(*) > 1)") ==
               one(f"SELECT COUNT(*) FROM (SELECT 1 FROM Before_{t} GROUP BY DrivetrainID, Level HAVING COUNT(*) > 1)"),
               f"{t}: no drivetrain gets two parts of one level (the shop labels a part by its type and level)")
        expect(one(f"SELECT COUNT(*) FROM {t} WHERE Level=? AND Id%1000<>716", level) == 0, f"{t}: Level {level} is only ours")
    expect(one("SELECT COUNT(*) FROM (SELECT 1 FROM Upgrades GROUP BY TypeId, Level, IsException HAVING COUNT(*) > 1)") ==
           one("SELECT COUNT(*) FROM (SELECT 1 FROM Before_Upgrades GROUP BY TypeId, Level, IsException HAVING COUNT(*) > 1)"),
           "no new label shares its type and level with another")
    expect(c.execute("SELECT Version FROM FH2_DriftPartsReady").fetchall() == [(3,)], "the marker is there, version 3")
    expect(one("SELECT COUNT(*) FROM sqlite_master WHERE name LIKE 'FH2_DriftParts%' AND name<>'FH2_DriftPartsReady'") == 0, "no scratch tables or indexes left behind")
    db.close()
    print("failed:", fails)
    return 1 if fails else 0

# A Python copy of drift_ratios() in src/drift_parts.c, with the default ini.
FIRST_SCALE, TOP_SCALE, MIN_GEARS = 0.85, 1.1, 5
def hundredths(v): return math.floor(v * 100 + 0.5) / 100
def drift_ratios(fd, ratios):
    gears = max(len(ratios), MIN_GEARS)
    first, top = ratios[0] * fd * FIRST_SCALE, ratios[-1] * fd * TOP_SCALE
    if not first > top * 1.05: return None
    low, high = max(2.2, first / 6.0), min(6.1, top / 0.48)
    final = fd * TOP_SCALE
    final = min(max(final, low), high) if low <= high else min(max(final, 2.2), 6.1)
    final = hundredths(final)
    out = []
    for g in range(1, gears + 1):
        out.append(hundredths(min(max(top * (first / top) ** ((gears - g) / (gears - 1)) / final, 0.48), 6.0)))
        if g > 1 and not out[-1] < out[-2]: return None
    return gears, final, out

def verify_transmissions(c, one):
    cols = [r[1] for r in c.execute(f"PRAGMA table_info({TRANS})")]
    src_q = (f"SELECT d.DrivetrainID, (SELECT x.rowid FROM Before_{TRANS} x WHERE x.DrivetrainID=d.DrivetrainID AND x.Level BETWEEN 0 AND 3 "
             f"AND x.NumGears BETWEEN 3 AND 9 AND x.FinalDriveRatio>0 AND x.GearRatio1>0 ORDER BY x.Level DESC, x.IsStock, x.rowid LIMIT 1) "
             f"FROM ({CAR_DRIVETRAINS}) d")
    sources = {dt: dict(zip(cols, c.execute(f"SELECT * FROM Before_{TRANS} WHERE rowid=?", (row,)).fetchone()))
               for dt, row in c.execute(src_q).fetchall() if row is not None}
    new = {r[1]: dict(zip(cols, r)) for r in c.execute(f"SELECT * FROM {TRANS} WHERE Level=15")}
    expect(one(f"SELECT COUNT(*) FROM {TRANS}") == one(f"SELECT COUNT(*) FROM Before_{TRANS}") + len(new), "nothing else added to the transmissions")
    made = bad = 0
    copied = ["DrivetrainID", "ManufacturerID", "MassDiff", "MomentInertia", "GearShiftTime", "GearRatio0", "CVTShiftTime"]
    for dt, s in sources.items():
        ratios = [s[f"GearRatio{g}"] for g in range(1, s["NumGears"])]
        want = drift_ratios(s["FinalDriveRatio"], ratios) if ratios[-1] > 0 and ratios[0] > ratios[-1] else None
        n = new.get(dt)
        if want is None:
            bad += n is not None
            continue
        made += 1
        gears, final, out = want
        ok = n is not None and n["Id"] == dt * 1000 + 716 and n["IsStock"] == 0 and n["Price"] == 2500 and all(n[k] == s[k] for k in copied)
        ok = ok and n["NumGears"] == gears + 1 and abs(n["FinalDriveRatio"] - final) < 1e-9
        ok = ok and all(abs(n[f"GearRatio{g}"] - out[g - 1]) < 1e-9 for g in range(1, gears + 1)) and all(n[f"GearRatio{g}"] == -1 for g in range(gears + 1, 9))
        bad += 0 if ok else 1
    expect(made == len(new) and bad == 0 and made > 0,
           f"a drift gearbox for each of the {made} drivetrains with a gearbox to make it from ({len(sources) - made} without): id drivetrain * 1000 + 716, "
           f"Level 15, 2,500 CR, the ratios of the formula, the Race box's reverse, shift time, weight and inertia ({bad} wrong)")
    # What they're like, from the rows themselves
    worst_step = worst_ends = 0; inside = True
    for n in new.values():
        g, s = n["NumGears"] - 1, sources[n["DrivetrainID"]]
        r = [n[f"GearRatio{i}"] for i in range(1, g + 1)]
        steps = [r[i] / r[i + 1] for i in range(g - 1)]
        worst_step = max(worst_step, max(steps) / min(steps))
        inside = inside and 2.2 <= n["FinalDriveRatio"] <= 6.1 and all(0.48 <= x <= 6 for x in r) and 5 <= g <= 8
        sf, st = s["GearRatio1"] * s["FinalDriveRatio"], s[f"GearRatio{s['NumGears'] - 1}"] * s["FinalDriveRatio"]
        worst_ends = max(worst_ends, abs(r[0] * n["FinalDriveRatio"] / sf / FIRST_SCALE - 1), abs(r[-1] * n["FinalDriveRatio"] / st / TOP_SCALE - 1))
    expect(inside, "each has 5 to 8 gears and ratios inside the Tune page's limits (gears 0.48 to 6, final drive 2.2 to 6.1)")
    expect(worst_step < 1.06, f"evenly spaced: in each one the biggest step is within 6% of the smallest (worst {worst_step:.3f}, from rounding)")
    expect(worst_ends < 0.03, f"overall, first gear 15% taller and top gear 10% shorter than the Race box's (worst {worst_ends:.1%} off)")
    label = c.execute("SELECT TypeId,Level,Name,Description,IconPath FROM Upgrades WHERE id=0x31000004").fetchone()
    expect(label is not None and label[:2] == ("15", 15) and label[4] and int(label[2][2:]) == (0x7091 << 16 | 65347) and int(label[3][2:]) == (0x7091 << 16 | 65348),
           "its label: transmission (15), Level 15, the game's icon, strings 65347 and 65348")
    for car in (1022, 1011):
        f = c.execute(f"SELECT * FROM {TRANS} WHERE Level=15 AND DrivetrainID=(SELECT DrivetrainID FROM Before_List_UpgradeDrivetrain WHERE Ordinal=? AND IsStock=1)",
                      (car,)).fetchone()
        if f: print(f"   car {car}'s drift gearbox (final drive, gears):", f[cols.index("FinalDriveRatio")], f[cols.index("GearRatio1"):cols.index("GearRatio1") + f[cols.index("NumGears")] - 1])

def verify_diffs(c, one):
    cols = [r[1] for r in c.execute(f"PRAGMA table_info({DIFF})")]
    src_q = (f"SELECT d.DrivetrainID, t.DrivetypeID, (SELECT y.rowid FROM Before_{DIFF} y WHERE y.DrivetrainID=d.DrivetrainID AND y.Level BETWEEN 0 AND 3 "
             f"ORDER BY y.Level DESC, y.IsStock, y.rowid LIMIT 1) FROM ({CAR_DRIVETRAINS}) d JOIN Before_Data_Drivetrain t USING (DrivetrainID)")
    sources = [(dt, kind, row) for dt, kind, row in c.execute(src_q).fetchall() if row is not None and kind in (1, 2, 3)]
    new = {r[1]: dict(zip(cols, r)) for r in c.execute(f"SELECT * FROM {DIFF} WHERE Level=16")}
    expect(one(f"SELECT COUNT(*) FROM {DIFF}") == one(f"SELECT COUNT(*) FROM Before_{DIFF}") + len(new), "nothing else added to the differentials")
    bad = 0; kinds = {}
    for dt, kind, row in sources:
        s = dict(zip(cols, c.execute(f"SELECT * FROM Before_{DIFF} WHERE rowid=?", (row,)).fetchone()))
        want = dict(s, Id=dt * 1000 + 716, Level=16, IsStock=0, Price=2250)
        if kind == 1: want.update(FrontLimitedSlipTorqueAccel=2000.0, FrontLimitedSlipTorqueDecel=500.0)
        else: want.update(RearLimitedSlipTorqueAccel=2000.0, RearLimitedSlipTorqueDecel=500.0)
        if kind == 3: want.update(FrontLimitedSlipTorqueAccel=500.0, FrontLimitedSlipTorqueDecel=0.0, RearToqueSplit=max(s["RearToqueSplit"], 0.8))
        n = new.get(dt)
        bad += 0 if n is not None and all(abs(n[k] - want[k]) < 1e-6 for k in cols) else 1
        kinds[kind] = kinds.get(kind, 0) + 1
    expect(len(new) == len(sources) and bad == 0 and len(sources) > 0,
           f"a drift diff for each of the {len(sources)} drivetrains (front-, rear- and four-wheel drive: {kinds.get(1, 0)}, {kinds.get(2, 0)}, {kinds.get(3, 0)}): "
           f"id drivetrain * 1000 + 716, Level 16, 2,250 CR, the driven axle locked 100% under power and 50% off it, four-wheel drive's front 25% / 0% "
           f"and at least 80% of the torque to the rear, the rest the Race diff's ({bad} wrong)")
    label = c.execute("SELECT TypeId,Level,Name,Description,IconPath FROM Upgrades WHERE id=0x31000005").fetchone()
    expect(label is not None and label[:2] == ("19", 16) and label[4] and int(label[2][2:]) == (0x7091 << 16 | 65349) and int(label[3][2:]) == (0x7091 << 16 | 65350),
           "its label: differential (19), Level 16, the game's icon, strings 65349 and 65350")

def verify_angle_kit(c, one, cars):
    rows = c.execute("SELECT n.Id,n.Ordinal,n.IsStock,n.Price,n.AntiSwayPhysicsID,n.ManufacturerID,n.MassDiff,r.AntiSwayPhysicsID,r.ManufacturerID,r.MassDiff "
                     "FROM List_UpgradeAntiSwayFront n JOIN Before_List_UpgradeAntiSwayFront r ON r.Ordinal=n.Ordinal AND r.Level=3 WHERE n.Level=16").fetchall()
    expect(len(rows) == cars and len({r[1] for r in rows}) == cars, f"an angle kit for each of the {cars} cars")
    expect(all(r[0] == r[1] * 1000 + 716 and r[2] == 0 and r[3] == 2000 and r[4:7] == r[7:10] for r in rows),
           "its rows: id car * 1000 + 716, not stock, 2,000 CR, the Race bar's physics, maker and weight")
    expect(one("SELECT COUNT(*) FROM List_UpgradeAntiSwayFront") == one("SELECT COUNT(*) FROM Before_List_UpgradeAntiSwayFront") + cars, "nothing else added to the front bars")
    label = c.execute("SELECT TypeId,Name,Description,IconPath FROM Upgrades WHERE id=0x31000003").fetchone()
    expect(label is not None and label[0] == "14" and label[3] and int(label[1][2:]) == (0x7091 << 16 | 65345) and int(label[2][2:]) == (0x7091 << 16 | 65346),
           "its label: front anti-roll bars (14), the game's icon, strings 65345 and 65346")

def verify_tires_and_suspension(c, one, cars):
    # Tires
    expect(one("SELECT COUNT(*) FROM List_TireCompound") == one("SELECT COUNT(*) FROM Before_List_TireCompound") + 1, "one new compound")
    cols = [r[1] for r in c.execute("PRAGMA table_info(List_TireCompound)")]
    new = c.execute("SELECT * FROM List_TireCompound WHERE TireCompoundID=77").fetchone()
    base = c.execute("SELECT * FROM List_TireCompound WHERE TireCompoundID=5").fetchone()
    diff = sorted(cols[i] for i in range(len(cols)) if new[i] != base[i])
    expect(diff == ["DisplayName", "FrictionMultiCurveLateralID", "FrictionMultiCurveLongitudinalAccelID", "TireCompoundID"],
           f"it's the Street compound with new sideways and power curves ({diff})")
    def curve(i):
        r = c.execute("SELECT * FROM List_TireFrictionCurve WHERE FrictionCurveID=?", (i,)).fetchone()
        return r[2], list(r[3:3 + r[1]])
    def multi(i):
        return c.execute("SELECT TireFrictionCurveID0,TireFrictionCurveID1,MaxSlip FROM List_TireFrictionMultiCurve WHERE FrictionMultiCurveID=?", (i,)).fetchone()
    for name, col, falloff in (("sideways", "FrictionMultiCurveLateralID", 0.4), ("power", "FrictionMultiCurveLongitudinalAccelID", 0.5)):
        nm, bm = multi(new[cols.index(col)]), multi(base[cols.index(col)])
        expect(nm is not None and nm[2] == bm[2], f"{name}: same slip range as the Street tire")
        for k in (0, 1):
            (ns, nv), (bs, bv) = curve(nm[k]), curve(bm[k])
            top = max(bv); peak = max(i for i, v in enumerate(bv) if v == top)
            ok = ns == bs and len(nv) == len(bv) and all(abs(nv[i] - bv[i]) < 1e-9 for i in range(peak + 1)) and \
                all(abs((top - nv[i]) - falloff * (top - bv[i])) < 1e-6 for i in range(peak + 1, len(bv)))
            expect(ok, f"{name} curve {k}: the same up to the peak, then {falloff:.0%} of the drop (ends at {nv[-1]:.3f}; the Street tire at {bv[-1]:.3f})")
            expect(all(nv[i] >= nv[i + 1] - 1e-9 for i in range(peak, len(nv) - 1)), f"{name} curve {k}: never rises again past the peak")
    rows = c.execute("SELECT Id,Ordinal,IsStock,Price,TireCompoundID FROM List_UpgradeTireCompound WHERE Level=16").fetchall()
    expect(len(rows) == cars and len({r[1] for r in rows}) == cars, f"a drift tire for each of the {cars} cars")
    expect(all(r[0] == r[1] * 1000 + 716 and r[2] == 0 and r[3] == 3000 and r[4] == 77 for r in rows), "its rows: id car * 1000 + 716 (the game's numbering, which CommunityGarage checks), not stock, 3,000 CR, the new compound")
    expect(one("SELECT COUNT(*) FROM List_UpgradeTireCompound n JOIN Before_List_UpgradeTireCompound s ON s.Ordinal=n.Ordinal AND s.IsStock=1 "
               "WHERE n.Level=16 AND n.TireModelName IS NOT s.TireModelName") == 0, "each keeps its car's own tire model")

    # Suspension
    rows = c.execute("SELECT Id,Ordinal,IsStock,Price,FrontSpringDamperPhysicsID,RearSpringDamperPhysicsID FROM List_UpgradeSpringDamper WHERE Level=16").fetchall()
    expect(len(rows) == cars and len({r[1] for r in rows}) == cars, f"drift suspension for each of the {cars} cars")
    expect(all(r[0] == r[1] * 1000 + 716 and r[2] == 0 and r[3] == 2500 and r[4] == r[1] * 1000 + 716 and r[5] == r[1] * 1000 + 816 for r in rows),
           "its rows: id car * 1000 + 716, not stock, 2,500 CR, physics of its own (car * 1000 + 716 and + 816)")
    pc = [r[1] for r in c.execute("PRAGMA table_info(List_SpringDamperPhysics)")]
    changed = {"SpringDamperPhysicsID", "MinRideHeight", "MinSpringRate", "StaticCamber", "Caster"}
    bad = n = 0
    for col, camber in (("FrontSpringDamperPhysicsID", -3.0), ("RearSpringDamperPhysicsID", -1.0)):
        q = f"""SELECT n.*, s.* FROM List_UpgradeSpringDamper u JOIN List_SpringDamperPhysics n ON n.SpringDamperPhysicsID=u.{col}
                JOIN Before_List_UpgradeSpringDamper b ON b.rowid={BEST}
                JOIN Before_List_SpringDamperPhysics s ON s.SpringDamperPhysicsID=b.{col} WHERE u.Level=16"""
        for r in c.execute(q).fetchall():
            new, src = dict(zip(pc, r[:len(pc)])), dict(zip(pc, r[len(pc):])); n += 1
            ok = all(new[k] == src[k] for k in pc if k not in changed)
            ok = ok and new["Caster"] == 7.0 and new["StaticCamber"] == camber and abs(new["MinSpringRate"] - 0.5 * src["MinSpringRate"]) < 1e-9
            ok = ok and src["MinRideHeight"] - 0.015 - 1e-9 <= new["MinRideHeight"] <= src["MinRideHeight"]
            ok = ok and new["MinRideHeight"] >= min(src["MinRideHeight"], src["MaxCompressHeight"] + 0.01) - 1e-9
            # the defaults stay inside the limits, wherever the car's own did (two car-pack cars have a default spring past their limit)
            if src["MinSpringRate"] <= src["DefSpringRate"] <= src["MaxSpringRate"]: ok = ok and new["MinSpringRate"] <= new["DefSpringRate"] <= new["MaxSpringRate"]
            if src["MinRideHeight"] <= src["DefRideHeight"] <= src["MaxRideHeight"]: ok = ok and new["MinRideHeight"] <= new["DefRideHeight"] <= new["MaxRideHeight"]
            bad += 0 if ok else 1
    expect(n == 2 * cars and bad == 0, f"each end is the car's best road suspension with caster 7, the camber, and softer and lower limits ({n} checked, {bad} wrong)")
    print("   copied from level:", c.execute(f"SELECT b.Level, COUNT(*) FROM List_UpgradeSpringDamper u JOIN Before_List_UpgradeSpringDamper b ON b.rowid={BEST} WHERE u.Level=16 GROUP BY 1").fetchall())

    # Labels
    labels = c.execute("SELECT TypeId,Name,Description,IconPath FROM Upgrades WHERE Level=16 AND id IN (0x31000001,0x31000002) ORDER BY id").fetchall()
    expect([l[0] for l in labels] == ["21", "13"] and all(l[3] for l in labels), "two labels (tires, springs), with the game's icons")
    expect(all(int(l[1][2:]) >> 16 == 0x7091 and int(l[2][2:]) >> 16 == 0x7091 for l in labels), "named through the game's Upgrades string table")

    f = c.execute("SELECT u.Id,p.MinRideHeight,p.MinSpringRate,p.DefSpringRate,p.StaticCamber,p.Caster FROM List_UpgradeSpringDamper u "
                  "JOIN List_SpringDamperPhysics p ON p.SpringDamperPhysicsID=u.FrontSpringDamperPhysicsID WHERE u.Level=16 AND u.Ordinal=1022").fetchone()
    print("   the F430's drift front end (id, lowest ride height, softest spring, default spring, camber, caster):", f)

if sys.argv[1] == "prepare": prepare(sys.argv[2], sys.argv[3])
else: sys.exit(verify(sys.argv[2]))
