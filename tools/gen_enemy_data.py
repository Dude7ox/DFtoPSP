#!/usr/bin/env python3
"""Generate src/core/EnemyData.{h,cpp} from Daggerfall Unity reference sources.

Daggerfall Unity (https://github.com/Interkarma/daggerfall-unity) is MIT
licensed.  Two tables that only exist inside the original FALL.EXE are taken
from it:

  Assets/Scripts/Utility/EnemyBasics.cs      -> per-enemy combat/animation data
  Assets/Scripts/Utility/RandomEncounters.cs -> dungeon encounter tables
  Assets/Scripts/DaggerfallUnityEnums.cs     -> MobileTypes id order

Everything else (names, hit points per level, attributes, resistances) is read
from the original MONSTER.BSA at run time; the generated names are only a
fallback for data sets without MONSTER.BSA.

Usage:
    python3 tools/gen_enemy_data.py --dfu /path/to/daggerfall-unity
    python3 tools/gen_enemy_data.py --files /path/to/dir/with/the/cs/files
"""
import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

BEHAVIOURS = ["General", "Flying", "Aquatic", "Spectral", "Guard"]
AFFINITIES = ["None", "Daylight", "Darkness", "Undead", "Animal", "Daedra", "Golem", "Water", "Human"]
TEAMS = ["PlayerEnemy", "PlayerAlly", "Vermin", "Spriggans", "Bears", "Tigers", "Spiders", "Orcs",
         "Centaurs", "Werecreatures", "Nymphs", "Aquatic", "Harpies", "Undead", "Giants", "Scorpions",
         "Magic", "Daedra", "Dragonlings", "KnightsAndMages", "Criminals", "CityWatch"]
METALS = ["None", "Iron", "Steel", "Silver", "Elven", "Dwarven", "Mithril", "Adamantium", "Ebony", "Daedric"]
FLAG_KEYS = ["HasIdle", "HasRangedAttack1", "HasRangedAttack2", "CanOpenDoors", "CastsMagic",
             "NoShadow", "ParrySounds"]


def humanize(name):
    return re.sub(r"([a-z0-9])([A-Z])", r"\1 \2", name).replace("_", " ").strip()


def locate(args):
    wanted = {"EnemyBasics.cs": "enemies", "RandomEncounters.cs": "encounters",
              "DaggerfallUnityEnums.cs": "enums"}
    found = {}
    if args.files:
        for entry in sorted(os.listdir(args.files)):
            if entry in wanted:
                found[wanted[entry]] = os.path.join(args.files, entry)
    elif args.dfu:
        paths = {"enemies": "Assets/Scripts/Utility/EnemyBasics.cs",
                 "encounters": "Assets/Scripts/Utility/RandomEncounters.cs",
                 "enums": "Assets/Scripts/DaggerfallUnityEnums.cs"}
        for key, rel in paths.items():
            full = os.path.join(args.dfu, rel)
            if os.path.isfile(full):
                found[key] = full
    missing = sorted(set(wanted.values()) - set(found))
    if missing:
        sys.exit("missing reference sources: %s" % ", ".join(missing))
    return found


def read(path):
    with open(path, encoding="utf-8-sig") as fh:
        return fh.read()


def parse_enum(text, name):
    m = re.search(r"enum\s+%s\s*\{(.*?)\n\s{4}\}" % name, text, re.S)
    if not m:
        return {}
    out = {}
    value = 0
    for raw in m.group(1).split("\n"):
        line = raw.split("//")[0].strip().rstrip(",").strip()
        if not line:
            continue
        if "=" in line:
            key, val = line.split("=", 1)
            key, val = key.strip(), val.strip().replace("(int)", "").strip()
            try:
                value = int(val, 0)
            except ValueError:
                value = out.get(val, 0)
        else:
            key = line
        if re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", key):
            out[key] = value
            value += 1
    return out


def split_entries(text, keyword):
    """Return [(comment, body)] for every `new <keyword>()` initialiser in text."""
    entries = []
    pending = []
    i = 0
    n = len(text)
    while i < n:
        m = re.compile(r"[ \t]*new\s+%s\s*\(\s*\)\s*\{" % keyword).match(text, i)
        if m:
            brace = m.end() - 1
            depth = 1
            j = brace + 1
            while j < n and depth:
                if text[j] == "{":
                    depth += 1
                elif text[j] == "}":
                    depth -= 1
                j += 1
            entries.append(("\n".join(pending).strip(), text[brace + 1:j - 1]))
            pending = []
            i = j
            continue
        line_end = text.find("\n", i)
        if line_end < 0:
            break
        stripped = text[i:line_end].strip()
        if stripped.startswith("//"):
            pending.append(stripped.lstrip("/").strip())
        elif stripped:
            pending = []
        i = line_end + 1
    return entries


def parse_fields(body):
    fields = {}
    for m in re.finditer(r"([A-Za-z_][A-Za-z0-9_]*)\s*=\s*", body):
        key = m.group(1)
        rest = body[m.end():]
        if rest.startswith("new int[]"):
            close = rest.find("}")
            fields[key] = [int(v) for v in re.findall(r"-?\d+", rest[:close])]
        elif rest.startswith("CorpseTexture("):
            nums = re.findall(r"\d+", rest[:rest.find(")")])
            fields[key] = [int(nums[0]), int(nums[1])] if len(nums) >= 2 else [0, 0]
        elif rest.startswith('"'):
            fields[key] = rest[1:rest.find('"', 1)]
        else:
            m2 = re.match(r"([A-Za-z_.()0-9 ]+?)\s*,", rest)
            if m2:
                fields[key] = m2.group(1).strip()
    return fields


def as_int(fields, key, default=0):
    raw = str(fields.get(key, default)).strip()
    try:
        return int(raw)
    except ValueError:
        return default


def as_index(names, fields, key, default=0):
    raw = str(fields.get(key, "")).strip()
    if "." in raw:
        raw = raw.split(".")[-1]
    if raw in names:
        return names.index(raw)
    try:
        return int(raw)
    except ValueError:
        return default


def frames(fields, key):
    seq = [max(-2, min(127, v)) for v in fields.get(key, [])][:10]
    return seq + [-2] * (10 - len(seq))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dfu", help="path to a daggerfall-unity checkout")
    ap.add_argument("--files", help="directory holding the reference .cs files")
    ap.add_argument("--out", default=os.path.join(ROOT, "src", "core"))
    args = ap.parse_args()
    if not args.dfu and not args.files:
        ap.error("provide --dfu or --files")

    files = locate(args)
    enemies_src = read(files["enemies"])
    encounters_src = read(files["encounters"])
    enums_src = read(files["enums"])

    mobile_types = parse_enum(enums_src, "MobileTypes")
    behaviours = parse_enum(enums_src, "MobileBehaviour")
    affinities = parse_enum(enums_src, "MobileAffinity")
    teams = parse_enum(enums_src, "MobileTeams")
    if not mobile_types:
        sys.exit("MobileTypes enum not found")
    behaviour_names = [k for k, _ in sorted(behaviours.items(), key=lambda kv: kv[1])] or BEHAVIOURS
    affinity_names = [k for k, _ in sorted(affinities.items(), key=lambda kv: kv[1])] or AFFINITIES
    team_names = [k for k, _ in sorted(teams.items(), key=lambda kv: kv[1])] or TEAMS

    m = re.search(r"MobileEnemy\[\]\s+Enemies\s*=\s*new\s+MobileEnemy\[\]\s*\{", enemies_src)
    if not m:
        sys.exit("Enemies array not found")
    rows = []
    for comment, body in split_entries(enemies_src[m.end():], "MobileEnemy"):
        f = parse_fields(body)
        if "ID" not in f or "MaleTexture" not in f:
            continue
        mid = as_int(f, "ID", -1)
        if mid < 0:
            continue
        fallback = next((humanize(k) for k, v in mobile_types.items() if v == mid), None)
        corpse = f.get("CorpseTexture", [0, 0])
        flags = 0
        for bit, key in enumerate(FLAG_KEYS):
            if str(f.get(key, "")).strip() == "true":
                flags |= 1 << bit
        rows.append(dict(
            id=mid, name=(fallback or comment or "creature"),
            archive=as_int(f, "MaleTexture"), female=as_int(f, "FemaleTexture"),
            corpseA=corpse[0], corpseR=corpse[1],
            level=as_int(f, "Level"), minHp=as_int(f, "MinHealth"), maxHp=as_int(f, "MaxHealth"),
            dmg=[as_int(f, "MinDamage" + (str(i) if i else "")) for i in range(3)],
            dmgMax=[as_int(f, "MaxDamage" + (str(i) if i else "")) for i in range(3)],
            armor=as_int(f, "ArmorValue"),
            metal=as_index(METALS, f, "MinMetalToHit"),
            behaviour=as_index(behaviour_names, f, "Behaviour"),
            affinity=as_index(affinity_names, f, "Affinity"),
            team=as_index(team_names, f, "Team"),
            flags=flags,
            atk1=frames(f, "PrimaryAttackAnimFrames"),
            atk2=frames(f, "PrimaryAttackAnimFrames2"),
            atk3=frames(f, "PrimaryAttackAnimFrames3"),
            chance2=as_int(f, "ChanceForAttack2"), chance3=as_int(f, "ChanceForAttack3"),
            soul=as_int(f, "SoulPts"),
        ))
    rows.sort(key=lambda r: r["id"])
    if len(rows) < 40:
        sys.exit("parsed only %d enemies" % len(rows))

    m = re.search(r"RandomEncounterTable\[\]\s+EncounterTables\s*=\s*new\s+RandomEncounterTable\[\]\s*\{", encounters_src)
    if not m:
        sys.exit("EncounterTables array not found")
    tables = []
    for _comment, body in split_entries(encounters_src[m.end():], "RandomEncounterTable"):
        tail = body.split("Enemies", 1)[1] if "Enemies" in body else ""
        ids = [mobile_types[x] for x in re.findall(r"MobileTypes\.([A-Za-z0-9_]+)", tail) if x in mobile_types][:20]
        if not ids:
            continue
        while len(ids) < 20:
            ids.append(ids[-1])
        tables.append(ids)
    if len(tables) < 20:
        sys.exit("expected 20 encounter tables, got %d" % len(tables))
    tables = tables[:20]

    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "EnemyData.h"), "w") as fh:
        fh.write(HEADER)
    with open(os.path.join(args.out, "EnemyData.cpp"), "w") as fs:
        fs.write('#include "EnemyData.h"\nnamespace dfpsp {\n')
        fs.write("const int kEnemyCount = %d;\nconst EnemyStatic kEnemies[kEnemyCount] = {\n" % len(rows))
        for r in rows:
            fs.write('    {%d,"%s",%d,%d,%d,%d,%d,%d,%d,{%d,%d,%d},{%d,%d,%d},%d,%d,%u,%u,%u,%u,'
                     '{%s},{%s},{%s},%u,%u,%d},\n' % (
                         r["id"], r["name"].replace('"', "'"), r["archive"], r["female"],
                         r["corpseA"], r["corpseR"], r["level"], r["minHp"], r["maxHp"],
                         r["dmg"][0], r["dmg"][1], r["dmg"][2],
                         r["dmgMax"][0], r["dmgMax"][1], r["dmgMax"][2],
                         r["armor"], r["metal"], r["behaviour"], r["affinity"], r["team"], r["flags"],
                         ",".join(str(v) for v in r["atk1"]),
                         ",".join(str(v) for v in r["atk2"]),
                         ",".join(str(v) for v in r["atk3"]),
                         r["chance2"], r["chance3"], r["soul"]))
        fs.write("};\nconst short kEncounterTables[20][20] = {\n")
        for t in tables:
            fs.write("    {%s},\n" % ",".join(str(v) for v in t))
        fs.write("};\n")
        fs.write(FOOTER)
    print("wrote EnemyData.h/EnemyData.cpp: %d enemies, %d encounter tables" % (len(rows), len(tables)))


HEADER = '''#pragma once
// GENERATED FILE - do not edit by hand.
// Regenerate with: python3 tools/gen_enemy_data.py --dfu /path/to/daggerfall-unity
//
// The tables in EnemyData.cpp are derived from Daggerfall Unity
// (MIT License, Copyright (C) 2009-2023 Daggerfall Workshop,
//  http://www.dfworkshop.net, https://github.com/Interkarma/daggerfall-unity):
//   Assets/Scripts/Utility/EnemyBasics.cs
//   Assets/Scripts/Utility/RandomEncounters.cs
//   Assets/Scripts/DaggerfallUnityEnums.cs
// They reproduce enemy combat numbers and encounter tables that live inside the
// original FALL.EXE rather than in the game data files. Names, hit points per
// level, attributes and resistances are read from the original MONSTER.BSA at
// run time instead, so mods that replace MONSTER.BSA still take effect.
namespace dfpsp {
enum EnemyBehaviour { BehaviourGeneral=0, BehaviourFlying=1, BehaviourAquatic=2, BehaviourSpectral=3, BehaviourGuard=4 };
// Animation records inside a monster texture archive: 5 records per state, each
// record holding the frames for one of eight compass directions.
enum EnemyAnimState { AnimMove=0, AnimPrimaryAttack=1, AnimHurt=2, AnimIdle=3, AnimRangedAttack1=4, AnimSpell=5, AnimRangedAttack2=6 };
enum EnemyFlag { EnemyHasIdle=1, EnemyHasRanged1=2, EnemyHasRanged2=4, EnemyCanOpenDoors=8, EnemyCastsMagic=16, EnemyNoShadow=32, EnemyParrySounds=64 };
struct EnemyStatic {
    int id;             // MobileTypes value: 0-42 monsters, 128-146 humanoids
    const char* name;   // fallback name, MONSTER.BSA wins when available
    int archive;        // texture archive holding the animation records
    int femaleArchive;  // archive used for female humanoids
    int corpseArchive, corpseRecord;
    int level;
    int minHealth, maxHealth;
    int minDamage[3], maxDamage[3];
    int armorValue;
    int minMetalToHit;
    unsigned char behaviour, affinity, team, flags;
    signed char attack1[10], attack2[10], attack3[10]; // -2 terminates a sequence
    unsigned char chance2, chance3;
    int soulPts;
};
extern const int kEnemyCount;
extern const EnemyStatic kEnemies[];
// 20 classic encounter tables: indices 0-18 are the dungeon types, 19 is underwater.
extern const short kEncounterTables[20][20];
const EnemyStatic* findEnemyStatic(int mobileId);
const char* enemyFallbackName(int mobileId);
} // namespace dfpsp
'''

FOOTER = '''const EnemyStatic* findEnemyStatic(int mobileId){
    for(int i=0;i<kEnemyCount;i++) if(kEnemies[i].id==mobileId) return &kEnemies[i];
    return nullptr;
}
const char* enemyFallbackName(int mobileId){
    const EnemyStatic* e=findEnemyStatic(mobileId);
    return e?e->name:"creature";
}
} // namespace dfpsp
'''


if __name__ == "__main__":
    main()
