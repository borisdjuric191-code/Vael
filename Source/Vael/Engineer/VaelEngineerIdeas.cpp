// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engineer/VaelEngineerIdeas.h"

#define LOCTEXT_NAMESPACE "VaelEngineerIdeas"

namespace
{
	FVaelEngineerIdea MakeIdea(const TCHAR* Id, const TCHAR* Subject, EVaelResearchStage Stage, TArray<FName> Gear, FText Title, FText Thought, FText Hint)
	{
		FVaelEngineerIdea Idea;
		Idea.Id = Id;
		Idea.Subject = Subject;
		Idea.Stage = Stage;
		Idea.Gear = MoveTemp(Gear);
		Idea.Title = MoveTemp(Title);
		Idea.Thought = MoveTemp(Thought);
		Idea.Hint = MoveTemp(Hint);
		return Idea;
	}
}

const TArray<FVaelEngineerIdea>& VaelEngineerIdeas::GetAll()
{
	static TArray<FVaelEngineerIdea> All;
	if (All.Num() > 0)
	{
		return All;
	}
	using S = EVaelResearchStage;

	// --- Aschenmark: what he meets first. Sighting gives the small thoughts (a branch of a weapon), beating a creature its
	// armour, studying it with Albrun the master pieces.

	All.Add(MakeIdea(TEXT("Krustenglut"), TEXT("Glutkriecher"), S::Sighted, { TEXT("Glutschleuder") },
		LOCTEXT("KrustenglutTitle", "Glut, die nicht bricht"),
		LOCTEXT("KrustenglutThought", "Der Kriecher glüht unter seinen Platten und zerspringt nicht. Eine Glutdrüse ins Leder der Schleuder genäht – dann brennt jeder Stein, den ich werfe."),
		LOCTEXT("KrustenglutHint", "Etwas gräbt unter der Asche und glüht, ohne zu brennen.")));
	All.Add(MakeIdea(TEXT("Schwarzglut"), TEXT("Glutkriecher"), S::Defeated, { TEXT("Schwarzglut_0"), TEXT("Schwarzglut_1") },
		LOCTEXT("SchwarzglutTitle", "Platten wie ein Dach"),
		LOCTEXT("SchwarzglutThought", "Seine Rückenplatten liegen übereinander wie Schindeln: der Schlag gleitet ab. Gehärtet in der Esse, mit Schlackeneisen gefasst, gibt das einen Helm und einen Brustpanzer."),
		LOCTEXT("SchwarzglutHint", "Bezwinge, was unter der Asche gräbt, und sieh dir seinen Rücken an.")));
	All.Add(MakeIdea(TEXT("Glutkiefer"), TEXT("Glutkriecher"), S::Researched, { TEXT("Meisterschleuder") },
		LOCTEXT("GlutkieferTitle", "Der schwere Kiefer"),
		LOCTEXT("GlutkieferThought", "Albrun hat den Kiefer gewogen: schwerer als Eisen, und er hält die Wärme. Als Gewicht an der Gabel schwingt die Schleuder ruhiger und schlägt härter."),
		LOCTEXT("GlutkieferHint", "Was Albrun über den Gräber in der Asche herausfindet, bringt die Schleuder weiter.")));

	All.Add(MakeIdea(TEXT("Rollsehne"), TEXT("Aschenroller"), S::Sighted, { TEXT("Spannschleuder") },
		LOCTEXT("RollsehneTitle", "Eine Sehne als Feder"),
		LOCTEXT("RollsehneThought", "Wie er sich einrollt und wieder aufschnellt! Die Sehne, die ihn zusammenzieht, ist eine Feder. Mit Latex als Band zieht die Schleuder schneller."),
		LOCTEXT("RollsehneHint", "Etwas rollt sich zur Kugel, wenn man ihm zu nahe kommt.")));
	All.Add(MakeIdea(TEXT("Rollwall"), TEXT("Aschenroller"), S::Defeated, { TEXT("Schwarzglut_4") },
		LOCTEXT("RollwallTitle", "Ein Schild, der sich faltet"),
		LOCTEXT("RollwallThought", "Fünf Reihen Rückenpanzer, die ineinander gleiten. Am Unterarm liegen sie flach, im Kampf fächern sie zu einer Wand auf."),
		LOCTEXT("RollwallHint", "Bezwinge die rollende Kugel und sieh, wie ihre Reihen ineinander gleiten.")));
	All.Add(MakeIdea(TEXT("Gelenkplatten"), TEXT("Aschenroller"), S::Researched, { TEXT("Schwarzglut_2"), TEXT("Schwarzglut_3") },
		LOCTEXT("GelenkplattenTitle", "Gelenke aus Panzer"),
		LOCTEXT("GelenkplattenThought", "Albrun zeigt mir, wie seine Platten an den Gelenken überlappen, ohne zu klemmen. So baue ich Armschienen und Beinschienen, die mich nicht steif machen."),
		LOCTEXT("GelenkplattenHint", "Was Albrun über die rollende Kugel herausfindet, schützt Arme und Beine.")));

	All.Add(MakeIdea(TEXT("Russfedern"), TEXT("Aschharpyie"), S::Sighted, { TEXT("Sehnenbogen") },
		LOCTEXT("RussfedernTitle", "Federn, die schneiden"),
		LOCTEXT("RussfedernThought", "Die Harpyie stößt herab, ohne zu flattern: ihre Rußfedern lassen die Luft gleiten. An den Wurfarmen eines Bogens zieht er schneller."),
		LOCTEXT("RussfedernHint", "Etwas kreischt über den Feldern und stößt aus dem Rauch herab.")));
	// The armour of the creature sets waits for models (Boris 17.11.: only the Schwarzglut set stays), so do its ideas
	constexpr bool bCreatureArmourIdeas = false;
	if constexpr (bCreatureArmourIdeas)
	All.Add(MakeIdea(TEXT("Russkleid"), TEXT("Aschharpyie"), S::Defeated, { TEXT("Russfeder_0"), TEXT("Russfeder_1"), TEXT("Russfeder_2"), TEXT("Russfeder_3") },
		LOCTEXT("RusskleidTitle", "Leicht wie Ruß"),
		LOCTEXT("RusskleidThought", "Ihr Gefieder wiegt fast nichts und hält trotzdem eine Klinge auf. Auf Schleierpelz genäht: eine Rüstung, in der ich schneller bin."),
		LOCTEXT("RusskleidHint", "Bezwinge die Kreischende aus dem Rauch.")));
	All.Add(MakeIdea(TEXT("Hohlschnabel"), TEXT("HarpyienAelteste"), S::Defeated, { TEXT("Meisterbogen") },
		LOCTEXT("HohlschnabelTitle", "Schnäbel als Wurfarme"),
		LOCTEXT("HohlschnabelThought", "Die Älteste trug Schnäbel in ihrer Krone, hohl und zäh. In die Wurfarme eingelassen speichern sie mehr Kraft als jedes Holz."),
		LOCTEXT("HohlschnabelHint", "Die mit der Krone aus gestohlenem Tand führt die Kreischenden an.")));

	if constexpr (bCreatureArmourIdeas)
	All.Add(MakeIdea(TEXT("Sichelpanzer"), TEXT("Sichelpuppe"), S::Defeated, { TEXT("Sichelklinge_0"), TEXT("Sichelklinge_1"), TEXT("Sichelklinge_2"), TEXT("Sichelklinge_3") },
		LOCTEXT("SichelpanzerTitle", "Klingen nach außen"),
		LOCTEXT("SichelpanzerThought", "Die Puppe ist eine Garbe voller Sicheln. Wer mich packt, schneidet sich: Sichelspitzen auf Kokonseide, die Klingen nach außen."),
		LOCTEXT("SichelpanzerHint", "Etwas steht auf den Feldern wie eine Garbe und erntet, was ihm zu nahe kommt.")));
	// The Aschenjaeger (26.11., Boris: "Teil für Teil zum Entdecken und Bauen"): each of its three creatures gives a piece of it
	All.Add(MakeIdea(TEXT("Schleierkapuze"), TEXT("Schleieraffe"), S::Researched, { TEXT("Aschenjaeger_0") },
		LOCTEXT("SchleierkapuzeTitle", "Ein Schleier vor den Augen"),
		LOCTEXT("SchleierkapuzeThought", "Der Affe sieht durch seinen Schleier, ich sehe ihn nicht. Eine Kapuze aus seinem Pelz, die Glimmaugen als Gläser: im Rauch bin ich der, der sieht."),
		LOCTEXT("SchleierkapuzeHint", "Erforsche das verhüllte Tier, das aus dem Dunkel stiehlt.")));
	All.Add(MakeIdea(TEXT("Puppenwickel"), TEXT("Sichelpuppe"), S::Observed, { TEXT("Aschenjaeger_1") },
		LOCTEXT("PuppenwickelTitle", "Gewickelt wie eine Garbe"),
		LOCTEXT("PuppenwickelThought", "Die Puppe ist nur Stroh und Garn, und doch hält sie Schläge aus: die Wicklung gibt nach und federt zurück. Kreuzweise um den Leib gebunden, leicht und leise."),
		LOCTEXT("PuppenwickelHint", "Sieh dir an, was auf den Feldern steht und sich bewegt wie eine Garbe.")));
	All.Add(MakeIdea(TEXT("Schwingenarme"), TEXT("Aschharpyie"), S::Defeated, { TEXT("Aschenjaeger_2") },
		LOCTEXT("SchwingenarmeTitle", "Federn am Arm"),
		LOCTEXT("SchwingenarmeThought", "Ihre Rußfedern lassen die Luft gleiten. In Reihen an die Unterarme gebunden, wie kleine Schwingen: der Wurf fliegt ruhiger, die Hand fängt sicherer."),
		LOCTEXT("SchwingenarmeHint", "Bezwinge die Kreischende aus dem Rauch.")));
	All.Add(MakeIdea(TEXT("Krallenschritt"), TEXT("Aschharpyie"), S::Researched, { TEXT("Aschenjaeger_3") },
		LOCTEXT("KrallenschrittTitle", "Krallen an den Stiefeln"),
		LOCTEXT("KrallenschrittThought", "Sie landet lautlos und greift sofort. Ihre Krallen an weichen, gewickelten Stiefeln: kein Laut auf der Asche, und der Fuß greift, wo ich lande."),
		LOCTEXT("KrallenschrittHint", "Erforsche die Kreischende, die aus dem Rauch herabstößt.")));
	// Stage II (26.11.): the Glutkrone from the Glutkoenigin, the Aschenjaeger II from the Harpyien-Aelteste, piece by piece
	All.Add(MakeIdea(TEXT("Brutkrone"), TEXT("Glutkoenigin"), S::Observed, { TEXT("Schwarzglut2_0") },
		LOCTEXT("BrutkroneTitle", "Eine Krone, die Wärme sieht"),
		LOCTEXT("BrutkroneThought", "Ihre vielen Augen sehen nicht Licht, sie sehen Wärme. Ein Kamm aus ihrer Krone, ihr Auge als Scheibe: dann sehe ich, was glüht."),
		LOCTEXT("BrutkroneHint", "Beobachte die Mutter der Glutkriecher in ihrer Kammer.")));
	All.Add(MakeIdea(TEXT("HerzDerGlut"), TEXT("Glutkoenigin"), S::Defeated, { TEXT("Schwarzglut2_1"), TEXT("Schwarzglut2_3") },
		LOCTEXT("HerzDerGlutTitle", "Ein Herz für den Panzer"),
		LOCTEXT("HerzDerGlutThought", "Ihr Herz hört nicht auf zu glühen, auch jetzt nicht. In die Brust gesetzt, treibt es Glut durch jede Platte, und die Kammern brüten weiter."),
		LOCTEXT("HerzDerGlutHint", "Bezwinge die Königin der Glutkriecher.")));
	All.Add(MakeIdea(TEXT("Brutbuechse"), TEXT("Glutkoenigin"), S::Researched, { TEXT("Schwarzglut2_2") },
		LOCTEXT("BrutbuechseTitle", "Ein Sack unter Druck"),
		LOCTEXT("BrutbuechseThought", "Ihr Brutsack hält Hitze und Druck. Kugeln statt Eier, eine Düse am Ende: die Glut treibt sie hinaus, schneller als jede Schleuder."),
		LOCTEXT("BrutbuechseHint", "Erforsche die Königin der Glutkriecher ganz.")));
	All.Add(MakeIdea(TEXT("Kielkrone"), TEXT("HarpyienAelteste"), S::Observed, { TEXT("Aschenjaeger2_0") },
		LOCTEXT("KielkroneTitle", "Die Krone der Ältesten"),
		LOCTEXT("KielkroneThought", "Sie trägt ihre Kiele wie eine Krone, und die Kreischenden folgen ihr. Über meiner Kapuze würden sie den Wind teilen."),
		LOCTEXT("KielkroneHint", "Beobachte die mit der Krone aus gestohlenem Tand.")));
	All.Add(MakeIdea(TEXT("Aeltestenmantel"), TEXT("HarpyienAelteste"), S::Defeated, { TEXT("Aschenjaeger2_1"), TEXT("Aschenjaeger2_2") },
		LOCTEXT("AeltestenmantelTitle", "Schwingen, die tanzen"),
		LOCTEXT("AeltestenmantelThought", "Wenn sie kreist, wird der Wind zum Wirbel. Ihre Schwungfedern als Mantel, die Kiele an den Armen: dreh ich mich, dreht der Wind mit."),
		LOCTEXT("AeltestenmantelHint", "Bezwinge die Älteste der Harpyien.")));
	All.Add(MakeIdea(TEXT("Windlaeufer"), TEXT("HarpyienAelteste"), S::Researched, { TEXT("Aschenjaeger2_3") },
		LOCTEXT("WindlaeuferTitle", "Pfeifen im Lauf"),
		LOCTEXT("WindlaeuferThought", "Pfeifensporen an den Stiefeln: jeder Schritt ein Ton, und der Wind läuft mit."),
		LOCTEXT("WindlaeuferHint", "Erforsche die Älteste der Harpyien ganz.")));
	All.Add(MakeIdea(TEXT("Glimmblick"), TEXT("Schleieraffe"), S::Observed, { TEXT("LinseGlimm") },
		LOCTEXT("GlimmblickTitle", "Augen für den Nebel"),
		LOCTEXT("GlimmblickThought", "Der Affe findet seine Beute im dichtesten Schleier. Seine Augen glimmen von innen. Hinter Rußglas gefasst sehe ich weiter als jetzt."),
		LOCTEXT("GlimmblickHint", "Beobachte in Ruhe, wer im Schleier jagt.")));

	All.Add(MakeIdea(TEXT("Spannhorn"), TEXT("Spannhornkaefer"), S::Sighted, { TEXT("Armbrust") },
		LOCTEXT("SpannhornTitle", "Ein Horn, das spannt"),
		LOCTEXT("SpannhornThought", "Der Käfer zieht sein Horn zurück, rastet ein und lässt los – genau wie ein Abzug. Ein Bogen quer auf einem Schaft, mit einer Raste: eine Armbrust."),
		LOCTEXT("SpannhornHint", "Etwas mit einem Horn spannt sich, bevor es zustößt, und es klackt dabei.")));
	All.Add(MakeIdea(TEXT("Legestachel"), TEXT("Glutkoenigin"), S::Defeated, { TEXT("Meisterarmbrust") },
		LOCTEXT("LegestachelTitle", "Der Stachel der Königin"),
		LOCTEXT("LegestachelThought", "Ihr Legestachel sticht und kehrt zurück, tausendmal. Als Abzug bricht er nie, und ihre Krone hält jede Spannung."),
		LOCTEXT("LegestachelHint", "Die im Krater wartet, trägt den stärksten Stachel der Aschenmark.")));

	All.Add(MakeIdea(TEXT("Distelholz"), TEXT("Glutdistel"), S::Sighted, { TEXT("Dornenbogen") },
		LOCTEXT("DistelholzTitle", "Holz, das sticht"),
		LOCTEXT("DistelholzThought", "Die Distel wehrt sich mit Dornen, die in der Wunde stecken bleiben. Wenn ich das Holz des Bogens damit umwickle, trägt jeder Pfeil ihr Gift."),
		LOCTEXT("DistelholzHint", "Eine Pflanze der Asche glüht an den Spitzen und sticht, wer sie pflückt.")));
	All.Add(MakeIdea(TEXT("Schlackenkopf"), TEXT("Schlacke"), S::Defeated, { TEXT("SchwereArmbrust") },
		LOCTEXT("SchlackenkopfTitle", "Bolzen, die zerspringen"),
		LOCTEXT("SchlackenkopfThought", "Schlacke ist spröde, sie zerspringt in scharfe Splitter. Als Kopf eines Bolzens zerplatzt sie im Ziel."),
		LOCTEXT("SchlackenkopfHint", "Bau ab, was die Gilde an ihren Halden liegen ließ.")));
	All.Add(MakeIdea(TEXT("Nidlinse"), TEXT("Markdruse"), S::Sighted, { TEXT("LinseNid") },
		LOCTEXT("NidlinseTitle", "Ein Splitter, der zeichnet"),
		LOCTEXT("NidlinseThought", "Was das Nid berührt, leuchtet noch lange nach. Ein Splitter davon im Glas, und was ich treffe, trägt das Zeichen."),
		LOCTEXT("NidlinseHint", "Wo das Blut der Schlafenden an die Oberfläche tritt, wachsen violette Drusen.")));

	// --- Fallen-Paket (25.11.): lure, snag, bury, smoke; the devices that make a fight a place he prepared
	All.Add(MakeIdea(TEXT("Grabmine"), TEXT("Glutkriecher"), S::Observed, { TEXT("Grabmine") },
		LOCTEXT("GrabmineTitle", "Was unter der Asche wartet"),
		LOCTEXT("GrabmineThought", "Der Kriecher gräbt sich ein, bis nur noch die Glut atmet, und wartet, bis jemand auf ihn tritt. Eine Glutdrüse unter einer Krustenplatte, mit Distelflaum gepolstert – und ich lege dasselbe in den Boden."),
		LOCTEXT("GrabmineHint", "Beobachte in Ruhe, wie der Gräber in der Asche auf seine Beute wartet.")));
	All.Add(MakeIdea(TEXT("Lockpfeife"), TEXT("Aschharpyie"), S::Observed, { TEXT("Lockpfeife") },
		LOCTEXT("LockpfeifeTitle", "Ein Schrei, und alle kommen"),
		LOCTEXT("LockpfeifeThought", "Eine Harpyie kreischt, und der ganze Schwarm dreht ab zu ihr. Ein Kreischbalg in einem Hohlkiel, mit Pfeifensporen als Zunge: dann rufe ich sie, wohin ich will – in meine Fallen."),
		LOCTEXT("LockpfeifeHint", "Beobachte, was die Kreischenden tun, wenn eine von ihnen ruft.")));
	All.Add(MakeIdea(TEXT("Wurfhaken"), TEXT("Schleieraffe"), S::Defeated, { TEXT("Wurfhaken") },
		LOCTEXT("WurfhakenTitle", "Finger, die nicht loslassen"),
		LOCTEXT("WurfhakenThought", "Der Affe hielt sich noch im Sterben am Fels fest. Seine Klammerfinger an einer Greifklaue, an einem Seil aus Rankenfaser: was ich treffe, hole ich mir her."),
		LOCTEXT("WurfhakenHint", "Bezwinge, was im Schleier klettert und nie loslässt.")));

	// --- Rammschild (25.11.) and the craft of the Kobold-Sprengmeister, who is a small engineer himself
	All.Add(MakeIdea(TEXT("Rammschild"), TEXT("Aschenroller"), S::Observed, { TEXT("Rammschild") },
		LOCTEXT("RammschildTitle", "Was gegen den Fels rollt"),
		LOCTEXT("RammschildThought", "Der Roller rast los und prallt am Fels ab, dann liegt er auf dem Rücken und strampelt. Stelle ich ihm eine Wand aus seinem eigenen Panzer in den Weg, mit Eisen gespickt, macht er das bei mir."),
		LOCTEXT("RammschildHint", "Beobachte in Ruhe, was geschieht, wenn die rollende Kugel auf Fels trifft.")));
	All.Add(MakeIdea(TEXT("Kristallladung"), TEXT("Kobold_Sprengmeister"), S::Observed, { TEXT("Funkenladung"), TEXT("Salzladung"), TEXT("Gruenspanladung") },
		LOCTEXT("KristallladungTitle", "Ein Stopfen aus Kobaltblüte"),
		LOCTEXT("KristallladungThought", "Jede seiner Ladungen hat einen pinken Stopfen, und jedes Erz darunter knallt anders. Funkenerz brennt, Salzspat nässt, Grünspan klebt. Der kleine Kerl ist ein Ingenieur wie ich."),
		LOCTEXT("KristallladungHint", "Beobachte den Kobold mit den Gurten voller Ladungen, ohne dass er dich bemerkt.")));
	All.Add(MakeIdea(TEXT("Stolperdraht"), TEXT("Kobold_Sprengmeister"), S::Defeated, { TEXT("Stolperdraht") },
		LOCTEXT("StolperdrahtTitle", "Der Draht, den ich nicht sah"),
		LOCTEXT("StolperdrahtThought", "Er spannte Drahtsilber zwischen sich und mich, und ich bin hineingelaufen. Zwei Pflöcke, ein Draht, eine Ladung in der Mitte: das kann ich auch."),
		LOCTEXT("StolperdrahtHint", "Bezwinge den Kobold, der Drähte zwischen sich und seine Verfolger spannt.")));
	return All;
}

const FVaelEngineerIdea* VaelEngineerIdeas::Find(FName Id)
{
	return GetAll().FindByPredicate([Id](const FVaelEngineerIdea& Idea) { return Idea.Id == Id; });
}

const FVaelEngineerIdea* VaelEngineerIdeas::FindForGear(FName GearId)
{
	return GetAll().FindByPredicate([GearId](const FVaelEngineerIdea& Idea) { return Idea.Gear.Contains(GearId); });
}

#undef LOCTEXT_NAMESPACE
