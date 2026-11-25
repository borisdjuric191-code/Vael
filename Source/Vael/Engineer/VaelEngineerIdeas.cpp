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
