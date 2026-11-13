# prepare <game> <copy>: a copy of the game's database, with a snapshot of the tables the plugin adds to.
#   <game> can also be a database file: Extra Options 0.6 keeps the one it merged from every mod in
#   mods/Cache/fh2-db/merged.sqlite, which has the mods' cars too.
# verify <copy>: checks what the plugin added (expects the default DriftParts.ini).
#   From a database Drift Parts 0.1.0 already went through (merged.sqlite with 0.1.0 installed), only the
#   angle kit is expected to be added.
import sqlite3, shutil, os, sys
TABLES = ["List_TireCompound", "List_TireFrictionCurve", "List_TireFrictionMultiCurve", "List_UpgradeTireCompound",
          "List_UpgradeSpringDamper", "List_SpringDamperPhysics", "Upgrades", "Data_Car", "List_UpgradeAntiSwayFront"]
BEST = "(SELECT x.rowid FROM Before_List_UpgradeSpringDamper x WHERE x.Ordinal=u.Ordinal AND x.Level BETWEEN 0 AND 4 ORDER BY CASE WHEN x.Level<4 THEN x.Level ELSE -1 END DESC, x.rowid LIMIT 1)"

def prepare(game, copy):
    mount = os.path.join(game, r"GameFiles\Anthem_1.0.0.64_x64__8wekyb3d8bbwe\Mount")
    src = os.path.join(mount, r"mediaoverride\Stripped\gamedbRC.slt")
    if not os.path.exists(src): src = os.path.join(mount, r"media\Stripped\gamedbRC.slt")
    if os.path.isfile(game): src = game
    shutil.copyfile(src, copy)
    db = sqlite3.connect(copy, isolation_level=None)
    for t in TABLES: db.execute(f"CREATE TABLE Before_{t} AS SELECT * FROM {t}")
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
    upgrade = one("SELECT COUNT(*) FROM Before_List_TireCompound WHERE TireCompoundID=77") == 1
    if upgrade: print("   (a database 0.1.0 already went through: only the angle kit should be new)")
    expect(one("SELECT COUNT(*) FROM (SELECT * FROM Data_Car EXCEPT SELECT * FROM Before_Data_Car)") == 0, "no car is changed")
    if not upgrade: verify_tires_and_suspension(c, one, cars)
    else:
        for t in TABLES:
            if t in ("Upgrades", "List_UpgradeAntiSwayFront"): continue
            expect(one(f"SELECT COUNT(*) FROM {t}") == one(f"SELECT COUNT(*) FROM Before_{t}"), f"{t}: nothing added")

    # Angle kit
    rows = c.execute("SELECT n.Id,n.Ordinal,n.IsStock,n.Price,n.AntiSwayPhysicsID,n.ManufacturerID,n.MassDiff,r.AntiSwayPhysicsID,r.ManufacturerID,r.MassDiff "
                     "FROM List_UpgradeAntiSwayFront n JOIN Before_List_UpgradeAntiSwayFront r ON r.Ordinal=n.Ordinal AND r.Level=3 WHERE n.Level=16").fetchall()
    expect(len(rows) == cars and len({r[1] for r in rows}) == cars, f"an angle kit for each of the {cars} cars")
    expect(all(r[0] == r[1] * 1000 + 716 and r[2] == 0 and r[3] == 2000 and r[4:7] == r[7:10] for r in rows),
           "its rows: id car * 1000 + 716, not stock, 2,000 CR, the Race bar's physics, maker and weight")
    expect(one("SELECT COUNT(*) FROM List_UpgradeAntiSwayFront") == one("SELECT COUNT(*) FROM Before_List_UpgradeAntiSwayFront") + cars, "nothing else added to the front bars")
    label = c.execute("SELECT TypeId,Name,Description,IconPath FROM Upgrades WHERE id=0x31000003").fetchone()
    expect(label is not None and label[0] == "14" and label[3] and int(label[1][2:]) == (0x7091 << 16 | 65345) and int(label[2][2:]) == (0x7091 << 16 | 65346),
           "its label: front anti-roll bars (14), the game's icon, strings 65345 and 65346")
    expect(one("SELECT COUNT(*) FROM Upgrades") == one("SELECT COUNT(*) FROM Before_Upgrades") + (1 if upgrade else 3), "no other label added")
    expect(c.execute("SELECT Version FROM FH2_DriftPartsReady").fetchall() == [(2,)], "the marker is there, version 2")
    expect(one("SELECT COUNT(*) FROM sqlite_master WHERE name LIKE 'FH2_DriftPartsS%'") == 0, "no scratch tables left behind")
    db.close()
    print("failed:", fails)
    return 1 if fails else 0

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
