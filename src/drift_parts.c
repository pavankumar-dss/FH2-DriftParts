// Drift Parts: a Drift tire compound and Drift springs and dampers for every car.
//
// An Extra Options plugin. Once per merged database it adds:
//
//   Tires       One new compound (List_TireCompound), a copy of a stock one whose grip falls away
//               less once the tire is past its peak, sideways and under power. A tire that keeps
//               most of its grip at big slip angles holds a slide instead of snapping. Each car gets
//               one shop row for it (List_UpgradeTireCompound).
//   Suspension  Per car, a copy of its best springs and dampers (Race where it has them) with more
//               caster and front camber, softer springs available and a lower ride height available.
//   Drift Angle Kit  Per car, a front anti-roll bar: a copy of its Race one (so its stiffness tunes the
//               same) that also gives the car more steering lock. The game keeps the steering angle
//               per car (Data_Car), not per part, so that part can't come from the database: once a
//               frame, when the car being driven has the kit fitted, the plugin raises the lock it
//               was built with (FH2PluginVehicleUpdate, below).
//
// The game reads compounds, friction curves and spring physics by id with plain SELECTs
// (game+0x1058AE6, +0x1062xxx), so new rows are picked up like its own.
//
// In the car (the object Extra Options hands plugins each frame), the fitted part ids are at car+0x128,
// 4 bytes each, in the order of the slot table at game+0x2E797F0: engine, drivetrain, body, motor,
// brakes, springs, front anti-roll bar, ... The car builder (game+0x1056210) builds a definition (on
// its caller's stack) that ends up at car+0x1200; its loader (game+0x1061070, through game+0x10622C0)
// reads Data_Car's steering columns into definition+0x5C: SteerMaxAngle, SteerMaxAngleFiltered,
// SteerMaxAngVelTurning, SteerMaxAngVelStraighten, SteerAngVelCountersteer, ...
//
// Every number is in DriftParts.ini next to the DLL, read when the game starts.
//
// Nothing here touches a file: it's the merged database in memory. A car saved with one of these
// parts needs this mod installed, as it does with any added part.
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "fh2_plugin.h"
#include "tune_row.h"

#define MARKER "FH2_DriftPartsReady"

enum {
    SQLITE_ROW = 100, SQLITE_DONE = 101,
    CURVE_POINTS = 100,
    LEVEL = 16,                    // past the game's 0..5, below Extra Options' 24+
    COMPOUND_ID = 77,              // the game's are 1..9 and 20
    CURVE_BASE = 7700,             // friction curves 7700..7703, multi-curves 7700 (sideways), 7701 (power)
    // Part ids are the game's own way round: car * 1000 + n. It matters: the CommunityGarage mod takes a
    // part's car to be id / 1000, and builds a car "given another car's parts" with stock ones instead (as
    // it did, silently, when these were 0x31000000 + car). The game's own n are 0..5 and 100..104.
    PART_N = 716,                  // the tire row and the spring row (separate tables), and the front physics
    REAR_PHYSICS_N = 816,
    MAX_CAR_ID = 2000000,          // car * 1000 + n stays an int
    TIRE_LABEL_ID = 0x31000001, SPRING_LABEL_ID = 0x31000002, KIT_LABEL_ID = 0x31000003,
    // Names are string table references: "_&" + (table << 16 | entry). 0x7091 is the game's Upgrades
    // table; strings.json fills these entries.
    STRING_TABLE = 0x7091,
    TIRE_NAME = 65341, TIRE_DESCRIPTION = 65342, SPRING_NAME = 65343, SPRING_DESCRIPTION = 65344,
    KIT_NAME = 65345, KIT_DESCRIPTION = 65346,
    // In the car Extra Options hands plugins (see the top of the file).
    CAR_ID_IN_CAR = 0x118, FRONT_ANTI_ROLL_IN_CAR = 0x128 + 6 * 4, STEERING_IN_CAR = 0x1200 + 0x5C,
    TUNE_MIN = 30, TUNE_MAX = 80,  // the Steering lock row's range, in degrees
};
#define TIRE_TYPE "21"    // UpgradeTypes id of the tire compound
#define SPRING_TYPE "13"  // ... and of springs and dampers
#define KIT_TYPE "14"     // ... and of front anti-roll bars

typedef struct {
    int base_compound;
    double side_falloff, power_falloff;  // how much of the drop past the peak is kept (1 = the stock tire)
    double side_grip, power_grip;        // scales on the whole curve
    int tire_price;
    double caster, front_camber, rear_camber, spring_min_scale, ride_height_drop;
    int spring_price;
    double kit_angle, kit_angle_pad, kit_turn_rate, kit_countersteer_rate;  // 0 rates = the car's own
    int kit_price;
} Settings;

static void (*log_utf8)(const char *);
static Settings cfg;
static wchar_t locks_path[MAX_PATH + 16];  // DriftParts.locks.ini: the Steering lock row's per-car values

static void log_line(const char *line) {
    if (log_utf8) log_utf8(line);
}

static void logf_line(const char *format, ...) {
    char line[400];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof line, format, args);
    va_end(args);
    if (log_utf8) log_utf8(line);
}

static double ini_number(const wchar_t *path, const wchar_t *section, const wchar_t *key, double fallback, double low, double high) {
    wchar_t text[64], *end = NULL;
    if (!GetPrivateProfileStringW(section, key, L"", text, 64, path)) return fallback;
    double value = wcstod(text, &end);
    if (end == text || value < low || value > high) return fallback;
    return value;
}

static void read_settings(void) {
    wchar_t path[MAX_PATH + 8] = L"";
    HMODULE self = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (const wchar_t *)&read_settings, &self);
    DWORD length = self ? GetModuleFileNameW(self, path, MAX_PATH) : 0;
    wchar_t *dot = length ? wcsrchr(path, L'.') : NULL;
    if (dot) {
        wcscpy(dot, L".locks.ini");
        wcscpy(locks_path, path);
        wcscpy(dot, L".ini");
    } else {
        path[0] = 0;
    }
    cfg.base_compound = (int)ini_number(path, L"Tires", L"BaseCompound", 5, 1, 9);
    cfg.side_falloff = ini_number(path, L"Tires", L"SideFalloff", 0.4, 0, 2);
    cfg.power_falloff = ini_number(path, L"Tires", L"PowerFalloff", 0.5, 0, 2);
    cfg.side_grip = ini_number(path, L"Tires", L"SideGrip", 1.0, 0.3, 2);
    cfg.power_grip = ini_number(path, L"Tires", L"PowerGrip", 1.0, 0.3, 2);
    cfg.tire_price = (int)ini_number(path, L"Tires", L"Price", 3000, 0, 1000000);
    cfg.caster = ini_number(path, L"Suspension", L"Caster", 7.0, 0, 15);
    cfg.front_camber = ini_number(path, L"Suspension", L"FrontCamber", -3.0, -10, 5);
    cfg.rear_camber = ini_number(path, L"Suspension", L"RearCamber", -1.0, -10, 5);
    cfg.spring_min_scale = ini_number(path, L"Suspension", L"SoftestSpringScale", 0.5, 0.1, 1);
    cfg.ride_height_drop = ini_number(path, L"Suspension", L"RideHeightDrop", 0.015, 0, 0.1);
    cfg.spring_price = (int)ini_number(path, L"Suspension", L"Price", 2500, 0, 1000000);
    cfg.kit_angle = ini_number(path, L"AngleKit", L"MaxAngle", 60, 10, 85);
    cfg.kit_angle_pad = ini_number(path, L"AngleKit", L"MaxAnglePad", 50, 10, 85);
    cfg.kit_turn_rate = ini_number(path, L"AngleKit", L"TurnRate", 0, 0, 1000);
    cfg.kit_countersteer_rate = ini_number(path, L"AngleKit", L"CountersteerRate", 150, 0, 1000);
    cfg.kit_price = (int)ini_number(path, L"AngleKit", L"Price", 2000, 0, 1000000);
}

// Runs a statement; fills up to `wanted` integer columns of its first row. 1 on success.
static int run_row(const FH2PluginDatabaseFrame *db, const char *sql, int64_t *columns, int wanted, int *got_row) {
    void *statement = NULL;
    const char *tail = NULL;
    if (got_row) *got_row = 0;
    if (db->prepare(db->database, sql, -1, &statement, &tail) != 0 || !statement) return 0;
    int result = db->step(statement);
    if (result == SQLITE_ROW) {
        for (int i = 0; i < wanted; ++i) columns[i] = db->column_int64(statement, i);
        if (got_row) *got_row = 1;
    }
    while (result == SQLITE_ROW) result = db->step(statement);
    return db->finalize(statement) == 0 && result == SQLITE_DONE;
}

static int run(const FH2PluginDatabaseFrame *db, const char *sql) { return run_row(db, sql, NULL, 0, NULL); }

static int runf(const FH2PluginDatabaseFrame *db, const char *format, ...) {
    static char sql[8192];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(sql, sizeof sql, format, args);
    va_end(args);
    return n > 0 && n < (int)sizeof sql && run(db, sql);
}

static int count(const FH2PluginDatabaseFrame *db, const char *sql, int64_t *value) {
    int got = 0;
    return run_row(db, sql, value, 1, &got) && got;
}

// A copy of friction curve `source` as `id`, with the drop past its peak scaled by `falloff` and the
// whole curve by `grip`. The curve is grip against slip, 1.0 at its peak.
static int add_curve(const FH2PluginDatabaseFrame *db, int64_t source, int id, double falloff, double grip) {
    static char sql[8192];
    static int64_t values[CURVE_POINTS + 1];
    int at = snprintf(sql, sizeof sql, "SELECT NumCurveValues");
    for (int i = 0; i < CURVE_POINTS; ++i) at += snprintf(sql + at, sizeof sql - at, ",CAST(ROUND(v%d*1e9) AS INTEGER)", i);
    snprintf(sql + at, sizeof sql - at, " FROM List_TireFrictionCurve WHERE FrictionCurveID=%lld", (long long)source);
    int got = 0;
    if (!run_row(db, sql, values, CURVE_POINTS + 1, &got) || !got) return 0;
    int n = (int)values[0], peak = 0;
    if (n < 2 || n > CURVE_POINTS) return 0;
    for (int i = 1; i < n; ++i)
        if (values[1 + i] >= values[1 + peak]) peak = i;  // the last of the highest points
    double top = (double)values[1 + peak] / 1e9;
    at = snprintf(sql, sizeof sql, "INSERT INTO List_TireFrictionCurve SELECT %d,NumCurveValues,FrictionScale*%.9g", id, grip);
    for (int i = 0; i < CURVE_POINTS; ++i) {
        double v = (double)values[1 + i] / 1e9;
        if (i >= n) at += snprintf(sql + at, sizeof sql - at, ",v%d", i);
        else at += snprintf(sql + at, sizeof sql - at, ",%.9g", i > peak ? top - (top - v) * falloff : v);
    }
    snprintf(sql + at, sizeof sql - at, " FROM List_TireFrictionCurve WHERE FrictionCurveID=%lld", (long long)source);
    return run(db, sql);
}

// A copy of multi-curve `source` (a pair of friction curves, for light and heavy load) as `id`.
static int add_multi_curve(const FH2PluginDatabaseFrame *db, int64_t source, int id, int first_curve, double falloff, double grip) {
    int64_t curves[2];
    int got = 0;
    char sql[200];
    snprintf(sql, sizeof sql, "SELECT TireFrictionCurveID0,TireFrictionCurveID1 FROM List_TireFrictionMultiCurve WHERE FrictionMultiCurveID=%lld",
             (long long)source);
    if (!run_row(db, sql, curves, 2, &got) || !got) return 0;
    return add_curve(db, curves[0], first_curve, falloff, grip) && add_curve(db, curves[1], first_curve + 1, falloff, grip) &&
           runf(db,
                "INSERT INTO List_TireFrictionMultiCurve(FrictionMultiCurveID,MaxSlip,MinLoadCurve,MaxLoadCurve,LoadClamp,TireFrictionCurveID0,"
                "TireFrictionCurveID1) SELECT %d,MaxSlip,MinLoadCurve,MaxLoadCurve,LoadClamp,%d,%d FROM List_TireFrictionMultiCurve "
                "WHERE FrictionMultiCurveID=%lld",
                id, first_curve, first_curve + 1, (long long)source);
}

static int add_label(const FH2PluginDatabaseFrame *db, int id, const char *type, int icon_level, int name, int description) {
    int64_t changed = 0;
    return runf(db,
                "INSERT INTO Upgrades(id,TypeId,IsException,Name,Level,Description,IconPath,ImagePath) "
                "SELECT %d,'%s',0,'_&%u',%d,'_&%u',IconPath,ImagePath FROM Upgrades WHERE TypeId='%s' AND Level=%d AND IsException=0 LIMIT 1",
                id, type, ((unsigned)STRING_TABLE << 16) | (unsigned)name, LEVEL, ((unsigned)STRING_TABLE << 16) | (unsigned)description, type,
                icon_level) &&
           count(db, "SELECT changes()", &changed) && changed == 1;
}

static int add_tires(const FH2PluginDatabaseFrame *db, int64_t *added) {
    int64_t curves[2];
    int got = 0;
    char sql[200];
    snprintf(sql, sizeof sql, "SELECT FrictionMultiCurveLateralID,FrictionMultiCurveLongitudinalAccelID FROM List_TireCompound WHERE TireCompoundID=%d",
             cfg.base_compound);
    if (!run_row(db, sql, curves, 2, &got) || !got) return 0;
    return add_multi_curve(db, curves[0], CURVE_BASE, CURVE_BASE, cfg.side_falloff, cfg.side_grip) &&
           add_multi_curve(db, curves[1], CURVE_BASE + 1, CURVE_BASE + 2, cfg.power_falloff, cfg.power_grip) &&
           // The compound has 62 columns; copy the row whole and change four.
           runf(db, "CREATE TABLE FH2_DriftPartsScratch AS SELECT * FROM List_TireCompound WHERE TireCompoundID=%d", cfg.base_compound) &&
           runf(db,
                "UPDATE FH2_DriftPartsScratch SET TireCompoundID=%d,DisplayName='%d_Drift',FrictionMultiCurveLateralID=%d,"
                "FrictionMultiCurveLongitudinalAccelID=%d",
                COMPOUND_ID, COMPOUND_ID, CURVE_BASE, CURVE_BASE + 1) &&
           run(db, "INSERT INTO List_TireCompound SELECT * FROM FH2_DriftPartsScratch") && run(db, "DROP TABLE FH2_DriftPartsScratch") &&
           add_label(db, TIRE_LABEL_ID, TIRE_TYPE, 2, TIRE_NAME, TIRE_DESCRIPTION) &&
           runf(db,
                "INSERT INTO List_UpgradeTireCompound(Id,Ordinal,Level,IsStock,TireCompoundID,ManufacturerID,Price,MassDiff,DragScale,"
                "WindInstabilityScale,RequiresGraphics,TireModelName) "
                "SELECT Ordinal*1000+%d,Ordinal,%d,0,%d,ManufacturerID,%d,MassDiff,DragScale,WindInstabilityScale,RequiresGraphics,TireModelName "
                "FROM List_UpgradeTireCompound WHERE rowid IN (SELECT MIN(rowid) FROM List_UpgradeTireCompound WHERE IsStock=1 AND Ordinal "
                "BETWEEN 1 AND %d AND Ordinal IN (SELECT Id FROM Data_Car) GROUP BY Ordinal) "
                "AND Ordinal*1000+%d NOT IN (SELECT Id FROM List_UpgradeTireCompound)",
                PART_N, LEVEL, COMPOUND_ID, cfg.tire_price, MAX_CAR_ID, PART_N) &&
           count(db, "SELECT changes()", added);
}

// One end's spring physics for every car in FH2_DriftPartsSource, copied with the drift changes.
static int add_physics(const FH2PluginDatabaseFrame *db, int n, const char *source_column, double camber) {
    return runf(db,
                "INSERT INTO List_SpringDamperPhysics(SpringDamperPhysicsID,Ordinal,SuspensionPhysicsTypeID,UseProgressiveSpring,DefRideHeight,"
                "MinRideHeight,MaxRideHeight,MaxCompressHeight,DefSpringRate,MinSpringRate,MaxSpringRate,DefDampenBumpRate,MinDampenBumpRate,"
                "MaxDampenBumpRate,DampenBumpClamp,DefDampenReboundRate,MinDampenReboundRate,MaxDampenReboundRate,DampenReboundClamp,"
                "BumpstopStiffness,BumpstopDamping,ChassisPivotX,ChassisPivotY,TirePivotX,CamberChangeRatioCompression,"
                "CamberChangeRatioExpansion,StaticToe,StaticCamber,Caster,SteerRotAxisTireOffsetX,YMaxSpringVel,MaxStretchDeltaFromRideHeight) "
                "SELECT s.Ordinal*1000+%d,p.Ordinal,p.SuspensionPhysicsTypeID,p.UseProgressiveSpring,p.DefRideHeight,"
                // lower, but never down to where the suspension is fully compressed
                "MIN(p.MinRideHeight,MAX(p.MinRideHeight-%.9g,p.MaxCompressHeight+0.01)),"
                "p.MaxRideHeight,p.MaxCompressHeight,p.DefSpringRate,p.MinSpringRate*%.9g,p.MaxSpringRate,p.DefDampenBumpRate,"
                "p.MinDampenBumpRate,p.MaxDampenBumpRate,p.DampenBumpClamp,p.DefDampenReboundRate,p.MinDampenReboundRate,p.MaxDampenReboundRate,"
                "p.DampenReboundClamp,p.BumpstopStiffness,p.BumpstopDamping,p.ChassisPivotX,p.ChassisPivotY,p.TirePivotX,"
                "p.CamberChangeRatioCompression,p.CamberChangeRatioExpansion,p.StaticToe,%.9g,%.9g,p.SteerRotAxisTireOffsetX,p.YMaxSpringVel,"
                "p.MaxStretchDeltaFromRideHeight "
                "FROM FH2_DriftPartsSource s JOIN List_SpringDamperPhysics p ON p.SpringDamperPhysicsID=s.%s",
                n, cfg.ride_height_drop, cfg.spring_min_scale, camber, cfg.caster, source_column);
}

static int add_suspension(const FH2PluginDatabaseFrame *db, int64_t *added) {
    return
        // Each car's best road springs and dampers (3 is Race), with both ends' physics there. A few off-roaders
        // only have Rally ones (4), so theirs are copied.
        runf(db,
             "CREATE TABLE FH2_DriftPartsSource AS SELECT u.* FROM List_UpgradeSpringDamper u WHERE u.Ordinal BETWEEN 1 AND %d "
             "AND u.Ordinal IN (SELECT Id FROM Data_Car) AND u.rowid=(SELECT x.rowid FROM List_UpgradeSpringDamper x WHERE x.Ordinal=u.Ordinal "
             "AND x.Level BETWEEN 0 AND 4 "
             "AND x.FrontSpringDamperPhysicsID IN (SELECT SpringDamperPhysicsID FROM List_SpringDamperPhysics) "
             "AND x.RearSpringDamperPhysicsID IN (SELECT SpringDamperPhysicsID FROM List_SpringDamperPhysics) "
             "ORDER BY CASE WHEN x.Level<4 THEN x.Level ELSE -1 END DESC,x.rowid LIMIT 1) "
             // ... and none of the three ids taken (another mod's part, say): that car is left alone
             "AND u.Ordinal*1000+%d NOT IN (SELECT Id FROM List_UpgradeSpringDamper) "
             "AND u.Ordinal*1000+%d NOT IN (SELECT SpringDamperPhysicsID FROM List_SpringDamperPhysics) "
             "AND u.Ordinal*1000+%d NOT IN (SELECT SpringDamperPhysicsID FROM List_SpringDamperPhysics)",
             MAX_CAR_ID, PART_N, PART_N, REAR_PHYSICS_N) &&
        add_physics(db, PART_N, "FrontSpringDamperPhysicsID", cfg.front_camber) &&
        add_physics(db, REAR_PHYSICS_N, "RearSpringDamperPhysicsID", cfg.rear_camber) &&
        add_label(db, SPRING_LABEL_ID, SPRING_TYPE, 3, SPRING_NAME, SPRING_DESCRIPTION) &&
        runf(db,
             "INSERT INTO List_UpgradeSpringDamper(Id,Ordinal,Level,ManufacturerID,MassDiff,DragScale,WindInstabilityScale,Price,IsStock,"
             "SuspensionDamageModDefID,TiresDamageModDefID,FrontSpringDamperPhysicsID,RearSpringDamperPhysicsID) "
             "SELECT Ordinal*1000+%d,Ordinal,%d,ManufacturerID,MassDiff,DragScale,WindInstabilityScale,%d,0,SuspensionDamageModDefID,"
             "TiresDamageModDefID,Ordinal*1000+%d,Ordinal*1000+%d FROM FH2_DriftPartsSource",
             PART_N, LEVEL, cfg.spring_price, PART_N, REAR_PHYSICS_N) &&
        count(db, "SELECT changes()", added) && run(db, "DROP TABLE FH2_DriftPartsSource");
}

// A front anti-roll bar per car: its Race bar (Level 3, which every car has) with the same physics, so the
// Tune page's stiffness slider works as with the Race bar. The extra lock is set on the car below.
static int add_angle_kit(const FH2PluginDatabaseFrame *db, int64_t *added) {
    return add_label(db, KIT_LABEL_ID, KIT_TYPE, 3, KIT_NAME, KIT_DESCRIPTION) &&
           runf(db,
                "INSERT INTO List_UpgradeAntiSwayFront(Id,Ordinal,Level,ManufacturerID,MassDiff,Price,IsStock,AntiSwayPhysicsID) "
                "SELECT Ordinal*1000+%d,Ordinal,%d,ManufacturerID,MassDiff,%d,0,AntiSwayPhysicsID FROM List_UpgradeAntiSwayFront u "
                "WHERE u.rowid IN (SELECT MAX(rowid) FROM List_UpgradeAntiSwayFront WHERE Level=3 AND Ordinal BETWEEN 1 AND %d "
                "AND Ordinal IN (SELECT Id FROM Data_Car) GROUP BY Ordinal) "
                "AND u.AntiSwayPhysicsID IN (SELECT AntiSwayPhysicsID FROM List_AntiSwayPhysics) "
                "AND u.Ordinal*1000+%d NOT IN (SELECT Id FROM List_UpgradeAntiSwayFront)",
                PART_N, LEVEL, cfg.kit_price, MAX_CAR_ID, PART_N) &&
           count(db, "SELECT changes()", added);
}

// ---- the steering lock, on the driven car -------------------------------------------------------------
// Extra Options hands plugins the local car once a frame. Its Data_Car id is at car+0x118, its fitted
// front anti-roll bar at car+0x140 and the steering block it was built with at car+0x125C. A rebuild
// (buying a part, changing car) writes the car's own lock back; the next frame sets the kit's again.
//
// The lock is per car (the Tuning screen's Alignment > Steering lock row, tune_row.c), kept in DriftParts.locks.ini
// next to the DLL; a car without one gets [AngleKit] MaxAngle. The controller and keys lock keeps the
// ini's ratio to it (50 to 60 by default).
enum { MAX_LOCKS = 2048 };
static struct { int car; float lock; } locks[MAX_LOCKS];
static volatile LONG lock_count, driven_car_id, driven_kit_fitted, kit_cars_logged;

static float lock_for(int car) {
    for (LONG i = 0, n = lock_count; i < n; ++i)
        if (locks[i].car == car) return locks[i].lock;
    return (float)cfg.kit_angle;
}

static void read_locks(void) {
    static wchar_t section[MAX_LOCKS * 16];
    if (!locks_path[0] || !GetPrivateProfileSectionW(L"Lock", section, ARRAYSIZE(section), locks_path)) return;
    for (const wchar_t *line = section; *line && lock_count < MAX_LOCKS; line += wcslen(line) + 1) {
        wchar_t *end = NULL;
        long car = wcstol(line, &end, 10);
        double lock = end && *end == L'=' ? wcstod(end + 1, NULL) : 0;
        if (car > 0 && car <= MAX_CAR_ID && lock >= TUNE_MIN && lock <= TUNE_MAX) {
            locks[lock_count].car = (int)car;
            locks[lock_count].lock = (float)lock;
            ++lock_count;
        }
    }
}

static float tune_get(void) { return lock_for((int)driven_car_id); }
static int tune_fitted(void) { return driven_kit_fitted != 0; }

// From the Tuning screen (its UI thread). The next frame puts it on the car.
static int tune_set(float lock) {
    const int car = (int)driven_car_id;
    if (car <= 0 || lock < TUNE_MIN || lock > TUNE_MAX) return 0;
    LONG i = 0, n = lock_count;
    while (i < n && locks[i].car != car) ++i;
    if (i < n && locks[i].lock == lock) return 1;
    if (i == n) {
        if (n == MAX_LOCKS) return 0;
        locks[n].car = car;
        locks[n].lock = lock;
        InterlockedIncrement(&lock_count);
    } else {
        locks[i].lock = lock;
    }
    wchar_t key[16], value[16];
    swprintf(key, ARRAYSIZE(key), L"%d", car);
    swprintf(value, ARRAYSIZE(value), L"%.0f", lock);
    if (locks_path[0]) WritePrivateProfileStringW(L"Lock", key, value, locks_path);
    return 1;
}

__declspec(dllexport) void WINAPI FH2PluginVehicleUpdate(const FH2PluginVehicleFrame *frame) {
    if (!frame || frame->size < sizeof *frame || !frame->car) return;
    __try {
        const char *car = (const char *)frame->car;
        const int id = *(const int *)(car + CAR_ID_IN_CAR);
        if (id > 0 && id <= MAX_CAR_ID) InterlockedExchange(&driven_car_id, id);
        const int part = *(const int *)(car + FRONT_ANTI_ROLL_IN_CAR);
        const LONG fitted = part > 0 && part % 1000 == PART_N;
        InterlockedExchange(&driven_kit_fitted, fitted);
        if (!fitted) return;
        float *steering = (float *)(car + STEERING_IN_CAR);
        const float lock = lock_for(id), pad = (float)(lock * cfg.kit_angle_pad / cfg.kit_angle);
        if (steering[0] == lock && steering[1] == pad) return;  // done
        const float before = steering[0], before_pad = steering[1];
        if (!(before > 1 && before < 90 && before_pad > 1 && before_pad < 90)) return;  // not a steering block
        steering[0] = lock;
        steering[1] = pad;
        if (cfg.kit_turn_rate > 0) steering[2] = (float)cfg.kit_turn_rate;                  // SteerMaxAngVelTurning
        if (cfg.kit_countersteer_rate > 0) steering[4] = (float)cfg.kit_countersteer_rate;  // SteerAngVelCountersteer
        if (InterlockedIncrement(&kit_cars_logged) <= 20)
            logf_line("DRIFT PARTS: angle kit on car %d: lock %.1f -> %.1f degrees (%.1f -> %.1f on a controller or keys).\r\n", id, before,
                      lock, before_pad, pad);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

__declspec(dllexport) BOOL WINAPI FH2PluginDatabaseUpdate(const FH2PluginDatabaseFrame *db) {
    int64_t marked = 0, tires = 0, springs = 0, kits = 0;
    if (!db || db->size < sizeof *db || !db->database || !db->prepare || !db->step || !db->finalize || !db->column_int64) return FALSE;
    if (!count(db, "SELECT COUNT(*) FROM sqlite_master WHERE name='" MARKER "'", &marked)) return FALSE;
    // A database 0.1.0 already went through (Version 1) has the tires and suspension: it only needs the kit.
    int64_t version = 0;
    if (marked && !count(db, "SELECT MAX(Version) FROM " MARKER, &version)) return FALSE;
    if (marked && version >= 2) return TRUE;
    ULONGLONG started = GetTickCount64();
    if (!run(db, "SAVEPOINT drift_parts")) return FALSE;
    int ok = 1;
    if (!marked && !(ok = add_tires(db, &tires))) logf_line("DRIFT PARTS: couldn't add the tires.\r\n");
    if (!marked && ok && !(ok = add_suspension(db, &springs))) logf_line("DRIFT PARTS: couldn't add the suspension.\r\n");
    if (ok && !(ok = add_angle_kit(db, &kits))) logf_line("DRIFT PARTS: couldn't add the angle kit.\r\n");
    if (ok && marked) ok = run(db, "DROP TABLE " MARKER);
    ok = ok && run(db, "CREATE TABLE " MARKER " AS SELECT 2 AS Version");
    if (!ok || !run(db, "RELEASE drift_parts")) {
        run(db, "ROLLBACK TO drift_parts");
        run(db, "RELEASE drift_parts");
        logf_line("DRIFT PARTS: rolled back; nothing added.\r\n");
        return FALSE;
    }
    logf_line("DRIFT PARTS: added drift tires for %lld cars, drift suspension for %lld and the angle kit for %lld in %llu ms.\r\n",
              (long long)tires, (long long)springs, (long long)kits, (unsigned long long)(GetTickCount64() - started));
    return TRUE;
}

__declspec(dllexport) BOOL WINAPI FH2PluginInitialize(const FH2PluginContext *context) {
    if (!context || context->size < sizeof *context) return TRUE;
    log_utf8 = context->log_utf8;
    read_settings();
    read_locks();
    static const TuneRowSpec row = {
        "DRIFT ANGLE KIT", "STEERING LOCK",
        "How far the front wheels turn with the Drift Angle Kit fitted, in degrees on a wheel (a little less on a controller or keys). "
        "More lock holds bigger drift angles and catches slides that would otherwise spin; less feels calmer. Stock cars turn 42. "
        "Needs the Drift Angle Kit: Upgrades, Platform and Handling, Front anti-roll bars. Kept for this car.",
        TUNE_MIN, TUNE_MAX, 0, tune_get, tune_set, tune_fitted, log_line,
    };
    TuneRowSpec spec = row;
    spec.marker = (float)cfg.kit_angle;
    tune_row_install((unsigned char *)context->game_image, context->game_image_size, &spec);
    return TRUE;
}
