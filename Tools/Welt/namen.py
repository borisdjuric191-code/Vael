"""
Names of the people of Vael (Strandgut der Welt): real everyday names of many cultures, each altered slightly so one
still hears where a person comes from but no name is used 1:1.

Data: Scripts/Data/Namen/Kulturen.json (pools + alteration rules per culture). Every name handed out is kept in
Scripts/Data/Namen/Vergeben.json under a stable key (like "Kesselgrund/Lager/3"), so rebuilding a settlement gives the
same people the same names, and no full name exists twice in the world.

Alteration, per culture: swap a typical ending for another one, swap a vowel (never the first letter), swap a
consonant pair, or (Korea, China) mix syllables of real names anew. A result is only taken if it is not a real name of
any pool, stays close to the original (edit distance), fits the culture's sound pattern if one is given, and contains
nothing of the block list.

  py Tools/Welt/namen.py probe              writes Docs/Namen_Probe.md: originals and alterations per culture
  py Tools/Welt/namen.py test               checks every pool name can be altered, determinism, uniqueness
  py Tools/Welt/namen.py vergib KEY [--kultur K] [--geschlecht f|m] [--haushalt H] [--kind]

Used as a module by the settlement scripts: Namen().vergib(key, ...) then .speichern().
"""
import argparse
import hashlib
import json
import os
import random
import re
import sys
import unicodedata

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DATA = os.path.join(ROOT, "Scripts", "Data", "Namen")
KULTUREN = os.path.join(DATA, "Kulturen.json")
VERGEBEN = os.path.join(DATA, "Vergeben.json")
# Share of people whose given name comes from another culture than their family (marriage, adoption, generations on Vael)
MISCHUNG = 0.2


def rng(*parts):
    """A random source that gives the same numbers for the same parts on every machine and run"""
    digest = hashlib.sha256("/".join(str(p) for p in parts).encode("utf-8")).hexdigest()
    return random.Random(int(digest[:16], 16))


def fold(text):
    """Lowercase without accents, hyphens and spaces, for comparing names"""
    plain = unicodedata.normalize("NFKD", text.lower())
    plain = "".join(ch for ch in plain if not unicodedata.combining(ch))
    return plain.replace("-", "").replace(" ", "").replace("'", "").replace("ı", "i").replace("ø", "o")


def distance(a, b):
    """Edit distance (Levenshtein)"""
    row = list(range(len(b) + 1))
    for i, ca in enumerate(a, 1):
        prev, row[0] = row[0], i
        for j, cb in enumerate(b, 1):
            prev, row[j] = row[j], min(row[j] + 1, row[j - 1] + 1, prev + (ca != cb))
    return row[-1]


def split_core(name):
    """Prefix that stays as it is and the core that gets altered: the last word, or what follows an inner capital (MacLeod)"""
    head, _, last = name.rpartition(" ")
    head = head + " " if head else ""
    for i in range(len(last) - 1, 0, -1):
        if last[i].isupper():
            return head + last[:i], last[i:]
    return head, last


class Namen:

    def __init__(self):
        data = json.load(open(KULTUREN, encoding="utf-8"))
        self.kulturen = data["kulturen"]
        self.sperre = [fold(s) for s in data["sperre"]]
        self.fest = {fold(n) for n in data["fest"]["namen"]}
        # Every real name of every pool: an alteration may never land on one of them
        self.echt = set(self.fest)
        for k in self.kulturen.values():
            for pool in ("f", "m", "familie"):
                self.echt.update(fold(n) for n in k[pool])
        # Letter pairs (with ^ start and $ end) that real names of each culture use: an alteration may use no other pair
        self.paare = {}
        for k, c in self.kulturen.items():
            pairs = set()
            for pool in ("f", "m", "familie"):
                for n in c[pool]:
                    for word in (fold(w) for w in re.split(r"[ -]", n)):
                        w = "^" + word + "$"
                        pairs.update(w[i:i + 2] for i in range(len(w) - 1))
            self.paare[k] = pairs
        self.vergeben = json.load(open(VERGEBEN, encoding="utf-8")) if os.path.isfile(VERGEBEN) else {"personen": {}, "haushalte": {}}
        self.belegt = {fold(p["name"]) for p in self.vergeben["personen"].values()} | self.fest

    # --- alteration -------------------------------------------------------------------------------------------------

    def _ok(self, k, original, cand):
        c = self.kulturen[k]
        f = fold(cand)
        if f == fold(original) or f in self.echt or len(f) < 2 or len(f) > len(fold(original)) + 3:
            return False
        if re.search(r"(.)\1\1", f) or any(s in f for s in self.sperre):
            return False
        if distance(f, fold(original)) > max(2, len(f) // 3):
            return False
        # no new doubled letters (Hellmi, Brantt) and no new stutter (Jovanovov, Nanana)
        if len(re.findall(r"(.)\1", f)) > len(re.findall(r"(.)\1", fold(original))):
            return False
        if any(m.group(0) not in fold(original) for m in re.finditer(r"(..+?)\1", f)):
            return False
        if re.search(r"(..+?)\1\1", f):
            return False
        if "muster" in c and not re.match(c["muster"], f):
            return False
        for word in (fold(w) for w in re.split(r"[ -]", cand)):
            w = "^" + word + "$"
            if any(w[i:i + 2] not in self.paare[k] for i in range(len(w) - 1)):
                return False
        return True

    @staticmethod
    def _ending(c, core, r):
        low = core.lower()
        endings = sorted((e for e in c["endungen"] if low.endswith(e) and len(low) > len(e) + 1), key=len, reverse=True)
        if not endings:
            return None
        old = endings[0]
        choices = [e for e in c.get("endungen_neu", c["endungen"]) if e != old]
        if not choices:
            return None
        new = r.choice(choices)
        return low[:-len(old)] + new

    @staticmethod
    def _vowel(c, core, r):
        low = core.lower()
        spots = [i for i, ch in enumerate(low) if i > 0 and ch in c["vokale"]]
        if not spots:
            return None
        i = r.choice(spots)
        return low[:i] + r.choice([v for v in c["vokale"] if v != low[i]]) + low[i + 1:]

    @staticmethod
    def _consonant(c, core, r):
        low = core.lower()
        swaps = [(a, b) for a, b in c["konsonanten"] if a in low[1:]] + [(b, a) for a, b in c["konsonanten"] if b in low[1:]]
        if not swaps:
            return None
        a, b = r.choice(swaps)
        spots = [m.start() for m in re.finditer(re.escape(a), low) if m.start() > 0]
        i = r.choice(spots)
        return low[:i] + b + low[i + len(a):]

    def _syllables(self, k, art, name, r):
        """Korea, China: two syllables of real names of this gender, mixed anew"""
        pool = sorted({s for n in self.kulturen[k][art] for s in n.lower().split("-")})
        first, second = r.sample(pool, 2)
        if k == "korea":
            return first.capitalize() + "-" + second
        return (first + second).capitalize()

    def abwandeln(self, k, name, r, art):
        """One real name of culture k altered slightly; None if no alteration passes the checks"""
        c = self.kulturen[k]
        if c.get("silben") and art != "familie":
            for _ in range(60):
                cand = self._syllables(k, art, name, r)
                f = fold(cand)
                if f not in self.echt and not any(s in f for s in self.sperre):
                    return cand
            return None
        head, core = split_core(name)
        # a new ending only if real names of this pool end like that (no male ending on a woman's name)
        c = dict(c, endungen_neu=[e for e in c["endungen"] if any(fold(n).endswith(fold(e)) for n in c[art])])
        for _ in range(60):
            ops = [self._ending, self._vowel, self._consonant]
            r.shuffle(ops)
            steps = 2 if len(core) >= 7 and r.random() < 0.3 else 1
            cand = core
            for op in ops:
                out = op(c, cand, r)
                if out is not None:
                    cand = out
                    steps -= 1
                    if steps == 0:
                        break
            if cand == core:
                continue
            cand = head + (cand[0].upper() + cand[1:] if core[0].isupper() else cand)
            if self._ok(k, name, cand):
                return cand
        return None

    def vorname(self, k, art, r):
        pool = self.kulturen[k][art]
        for _ in range(40):
            out = self.abwandeln(k, r.choice(pool), r, art)
            if out:
                return out
        raise RuntimeError("no given name for {} {}".format(k, art))

    def familienname(self, k, r):
        pool = self.kulturen[k]["familie"]
        for _ in range(40):
            out = self.abwandeln(k, r.choice(pool), r, "familie")
            if out:
                return out
        raise RuntimeError("no family name for " + k)

    def _weiblich(self, k, familie):
        for male, female in self.kulturen[k].get("weiblich", []):
            if familie.endswith(male):
                return familie[:-len(male)] + female
        return familie

    def voller_name(self, k, art, r, familie=None, vater=None, familie_kultur=None):
        """A full name after the rule of culture k; familie/vater are shared within a household"""
        c = self.kulturen[k]
        regel = c["regel"]
        rufname = self.vorname(k, art, r)
        fk = familie_kultur or k
        if regel in ("vorname_familie", "familie_vorne", "zwei_familien") or (regel == "einzel" and self.kulturen[fk]["familie"] and (familie or r.random() < 0.5)):
            familie = familie or self.familienname(fk, r)
            if art == "f":
                familie = self._weiblich(fk, familie)
            if regel == "zwei_familien" and " " not in familie:
                familie = familie + " " + self.familienname(fk, r)
            return familie + " " + rufname if regel == "familie_vorne" else rufname + " " + familie
        if regel in ("vater", "patronym"):
            vater = vater or self.vorname(k, "m", r)
            if regel == "vater":
                return rufname + " " + vater
            stem = vater if vater.endswith("s") else vater + "s"
            return rufname + " " + stem[:-1] + (c["sohn"] if art == "m" else c["tochter"])
        return rufname

    # --- handing out ------------------------------------------------------------------------------------------------

    def vergib(self, schluessel, kultur=None, geschlecht=None, haushalt=None, kind=False):
        """The person under this key: taken from the record if known, else made, recorded and returned.
        Members of one household share culture and family name (children also the father's name); about one in five
        adults of a household has a given name of another culture."""
        if schluessel in self.vergeben["personen"]:
            return self.vergeben["personen"][schluessel]
        for versuch in range(200):
            r = rng(schluessel, versuch)
            art = geschlecht or r.choice("fm")
            haus = self.vergeben["haushalte"].get(haushalt) if haushalt else None
            if haus is None:
                k = kultur or r.choice(sorted(self.kulturen))
                haus = {"kultur": k}
                c = self.kulturen[k]
                if c["regel"] in ("vorname_familie", "familie_vorne", "zwei_familien") or (c["regel"] == "einzel" and c["familie"]):
                    haus["familie"] = self.familienname(k, r)
                    if c["regel"] == "zwei_familien":
                        haus["familie"] += " " + self.familienname(k, r)
                if c["regel"] in ("vater", "patronym"):
                    haus["vater"] = self.vorname(k, "m", r)
            fk = haus["kultur"]
            k = fk if (kind or kultur or r.random() >= MISCHUNG) else r.choice(sorted(self.kulturen))
            if self.kulturen[k]["regel"] != self.kulturen[fk]["regel"]:
                k = fk  # a mixed given name only where both cultures build names alike
            name = self.voller_name(k, art, r, familie=haus.get("familie"), vater=haus.get("vater") if kind else None, familie_kultur=fk)
            if fold(name) in self.belegt:
                continue
            person = {"name": name, "kultur": k, "familie_kultur": fk, "geschlecht": art}
            if haushalt:
                person["haushalt"] = haushalt
                haus = self.vergeben["haushalte"].setdefault(haushalt, haus)
                # the first man of a household that names children after their father is that father
                if not kind and art == "m" and k == fk and self.kulturen[fk]["regel"] in ("vater", "patronym") and not haus.get("vater_ist_mitglied"):
                    haus["vater"] = name.split(" ")[0]
                    haus["vater_ist_mitglied"] = True
            self.vergeben["personen"][schluessel] = person
            self.belegt.add(fold(name))
            return person
        raise RuntimeError("no free name for " + schluessel)

    def speichern(self):
        os.makedirs(DATA, exist_ok=True)
        with open(VERGEBEN, "w", encoding="utf-8") as out:
            json.dump(self.vergeben, out, ensure_ascii=False, indent=1, sort_keys=True)


# --- commands -------------------------------------------------------------------------------------------------------

def probe(je):
    namen = Namen()
    lines = ["# Namensprobe", "",
             "Erzeugt von `Tools/Welt/namen.py probe`. Links der echte Name, rechts die Abwandlung, die im Spiel vorkommen kann.",
             "Was komisch klingt oder eine schlechte Bedeutung hat: Zeile notieren, dann kommt es in die Sperrliste.", ""]
    for k in sorted(namen.kulturen):
        c = namen.kulturen[k]
        lines += ["## " + c["label"], "", "Regel: `{}`".format(c["regel"]), ""]
        for art, titel in (("f", "Frauen"), ("m", "Männer"), ("familie", "Familien")):
            pool = c[art]
            if not pool:
                continue
            pairs = []
            for i, name in enumerate(pool[:je]):
                alt = namen.abwandeln(k, name, rng("probe", k, art, i), art)
                pairs.append("{} → {}".format(name, alt or "–"))
            lines.append("- {}: {}".format(titel, ", ".join(pairs)))
        voll = [namen.voller_name(k, rng("voll", k, i).choice("fm"), rng("voll", k, i, 1)) for i in range(4)]
        lines += ["- Volle Namen: " + ", ".join(voll), ""]
    path = os.path.join(ROOT, "Docs", "Namen_Probe.md")
    with open(path, "w", encoding="utf-8") as out:
        out.write("\n".join(lines))
    print("written " + path)


def test():
    namen = Namen()
    failed = 0
    for k, c in sorted(namen.kulturen.items()):
        bad = [n for art in ("f", "m", "familie") for i, n in enumerate(c[art])
               if namen.abwandeln(k, n, rng("test", k, art, i), art) is None]
        total = len(c["f"]) + len(c["m"]) + len(c["familie"])
        if bad:
            # a name that cannot be altered is simply never used; only a pool that loses a quarter is a problem
            print("{}: not used ({}/{}): {}".format(k, len(bad), total, ", ".join(bad)))
            if len(bad) * 4 > total:
                failed += 1
    # determinism and uniqueness on a scratch record
    a, b = Namen(), Namen()
    a.vergeben = {"personen": {}, "haushalte": {}}
    b.vergeben = {"personen": {}, "haushalte": {}}
    a.belegt, b.belegt = set(a.fest), set(b.fest)
    pa = [a.vergib("T/{}".format(i), haushalt="H{}".format(i // 4), kind=i % 4 > 1)["name"] for i in range(400)]
    pb = [b.vergib("T/{}".format(i), haushalt="H{}".format(i // 4), kind=i % 4 > 1)["name"] for i in range(400)]
    if pa != pb:
        print("not deterministic")
        failed += 1
    if len({fold(n) for n in pa}) != len(pa):
        print("names twice")
        failed += 1
    echt = [n for n in pa if any(fold(part) in a.echt for part in re.split(r"[ ]", n) if len(fold(part)) > 3)]
    print("400 people, {} with a real name part (family names may stay real only in Korea/China)".format(len(echt)))
    for n in pa[:24]:
        print("  " + n)
    print("FAILED {}".format(failed) if failed else "OK")
    return failed == 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("befehl", choices=["probe", "test", "vergib"])
    parser.add_argument("schluessel", nargs="?")
    parser.add_argument("--je", type=int, default=6)
    parser.add_argument("--kultur")
    parser.add_argument("--geschlecht", choices=["f", "m"])
    parser.add_argument("--haushalt")
    parser.add_argument("--kind", action="store_true")
    args = parser.parse_args()
    if args.befehl == "probe":
        probe(args.je)
    elif args.befehl == "test":
        sys.exit(0 if test() else 1)
    else:
        namen = Namen()
        print(json.dumps(namen.vergib(args.schluessel, args.kultur, args.geschlecht, args.haushalt, args.kind), ensure_ascii=False))
        namen.speichern()


if __name__ == "__main__":
    main()
