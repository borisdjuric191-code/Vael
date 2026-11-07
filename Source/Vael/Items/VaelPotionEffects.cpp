// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/VaelPotionEffects.h"
#include "World/VaelBalance.h"

#include "AbilitySystemComponent.h"
#include "Combat/VaelAttributeSet.h"
#include "Combat/VaelCombatStatics.h"
#include "Components/PointLightComponent.h"
#include "EngineUtils.h"
#include "Items/VaelBrewing.h"
#include "Items/VaelMaterial.h"
#include "Items/VaelMaterialBag.h"
#include "Combat/VaelArcShot.h"
#include "TimerManager.h"
#include "Magic/VaelGameplayTags.h"
#include "Player/VaelCharacter.h"
#include "Save/VaelSaveSubsystem.h"
#include "UI/VaelCombatTextSubsystem.h"
#include "Vael.h"

#define LOCTEXT_NAMESPACE "VaelPotionEffects"

namespace
{
	// Names of the potions as in FVaelBrewing (the results are DA_Material_Trank_<Id>)
	const FName Manatrank(TEXT("Manatrank"));
	const FName Nachtsicht(TEXT("Nachtsicht"));
	const FName Loeschtrank(TEXT("Loeschtrank"));
	const FName Ascheschleier(TEXT("Ascheschleier"));
	const FName Leichtfuss(TEXT("Leichtfuss"));
	const FName Rindenhaut(TEXT("Rindenhaut"));
	const FName Feuerschutz(TEXT("Feuerschutz"));
	const FName Giftphiole(TEXT("Giftphiole"));
	const FName Ermattungsfluch(TEXT("Ermattungsfluch"));
	const FName Blutoel(TEXT("Blutoel"));
	const FName Schwarzbrand(TEXT("Schwarzbrand"));
	const FName Gruppenheilung(TEXT("Gruppenheilung"));
	const FName Saeurewolke(TEXT("Saeurewolke"));
	const FName Reinigungstrank(TEXT("Reinigungstrank"));
	const FName Daemmertrank(TEXT("Daemmertrank"));
	const FName Hainruf(TEXT("Hainruf"));
	const FName Wuergherrschaft(TEXT("Wuergherrschaft"));
	const FName Feenleib(TEXT("Feenleib"));
	const FName Glutblut(TEXT("Glutblut"));
	const FName Aschegestalt(TEXT("Aschegestalt"));
	const FName Sternenblick(TEXT("Sternenblick"));
	const FName Blutrausch(TEXT("Blutrausch"));
	const FName Daemmergarten(TEXT("Daemmergarten"));
	const FName Heiltrank(TEXT("Heiltrank"));

	constexpr float Minute = 60.0f;

	/** How far a vial flies to its target, and how far the power potions reach for enemies */
	constexpr float ThrowRange = 1200.0f;
	/** The engineer: the sling up to this far, the bow beyond (to BowRange) */
	constexpr float SlingRange = 900.0f;
	constexpr float BowRange = 2000.0f;
	constexpr float PulseRange = 1000.0f;

	const FVaelRecipe* FindRecipe(const UVaelMaterial* Potion)
	{
		if (Potion == nullptr)
		{
			return nullptr;
		}
		const FSoftObjectPath Path(Potion);
		for (const FVaelRecipe& Recipe : FVaelBrewing::GetRecipes())
		{
			if (Recipe.Result.ToSoftObjectPath() == Path)
			{
				return &Recipe;
			}
		}
		return nullptr;
	}

	/** Thrown vials of the craft "Wurf und Öl" */
	bool IsThrown(FName Id)
	{
		return Id == Giftphiole || Id == Ermattungsfluch || Id == Schwarzbrand || Id == Saeurewolke || Id == Gruppenheilung;
	}
}

UVaelPotionEffects::UVaelPotionEffects()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
}

UVaelPotionEffects* UVaelPotionEffects::Find(const AActor* Player)
{
	return Player != nullptr ? Player->FindComponentByClass<UVaelPotionEffects>() : nullptr;
}

AVaelCharacter* UVaelPotionEffects::GetPlayer() const
{
	return Cast<AVaelCharacter>(GetOwner());
}

float UVaelPotionEffects::Now() const
{
	return GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f;
}

bool UVaelPotionEffects::Use(AVaelCharacter* Player, const UVaelMaterial* Potion)
{
	const FVaelRecipe* Recipe = FindRecipe(Potion);
	UVaelMaterialBag* Bag = Player != nullptr ? Player->GetMaterialBag() : nullptr;
	if (Recipe == nullptr || Bag == nullptr || Bag->GetCount(Potion) <= 0)
	{
		return false;
	}

	UVaelPotionEffects* Effects = Find(Player);
	if (Effects == nullptr)
	{
		Effects = NewObject<UVaelPotionEffects>(Player, TEXT("PotionEffects"));
		Effects->RegisterComponent();
	}

	// The own craft: 25 % stronger, 50 % longer
	const bool bOwn = FVaelBrewing::IsOwnCraft(Player, Recipe->Craft);
	const float Strength = bOwn ? 1.25f : 1.0f;
	const float DurationScale = bOwn ? 1.5f : 1.0f;

	// Vials of "Wurf und Öl": the engineer fires them (sling, later bow); every other class coats its weapon with them
	bool bUsed = false;
	if (IsThrown(Recipe->Id) && Recipe->Id != Gruppenheilung)
	{
		if (ThrowsVials(Player))
		{
			bUsed = Effects->Throw(Recipe->Id, Strength);
		}
		else
		{
			Effects->WeaponOil = Recipe->Id;
			Effects->WeaponOilEnd = Effects->Now() + 60.0f * DurationScale;
			Effects->OilTouched.Reset();
			UVaelCombatTextSubsystem::PostReaction(Player, FText::Format(LOCTEXT("OilOn", "Öl auf der Waffe: {0}"), Recipe->Name));
			bUsed = true;
		}
	}
	else
	{
		bUsed = Effects->Drink(Recipe->Id, Strength, DurationScale);
	}
	if (!bUsed)
	{
		return false;
	}

	Bag->RemoveMaterial(Potion, 1);
	UVaelCombatTextSubsystem::PostPickup(Player, Recipe->Name, Recipe->Color);

	// Die Waage: shadow brews cost a little of oneself, twilight brews half of that
	if (Recipe->Way == EVaelWay::Shadow || Recipe->Way == EVaelWay::Twilight)
	{
		const float Price = (Recipe->Tier == EVaelBrewTier::Power ? 3.0f : Recipe->Tier == EVaelBrewTier::Strong ? 2.0f : 1.0f) * (Recipe->Way == EVaelWay::Twilight ? 0.5f : 1.0f);
		UVaelBalanceSubsystem::ChangePlayer(Player, Price, LOCTEXT("ShadowBrewReason", "Schattentrank"));
	}
	UVaelSaveSubsystem::RequestSave(Player);
	UE_LOG(LogVael, Log, TEXT("Potion: %s uses %s%s"), *Player->GetName(), *Recipe->Id.ToString(), bOwn ? TEXT(" (own craft)") : TEXT(""));
	return true;
}

void UVaelPotionEffects::Start(FName PotionId, float Seconds, float InStrength, float FirstPulse)
{
	FLasting& Effect = Lasting.FindOrAdd(PotionId);
	Effect.EndTime = Now() + Seconds;
	Effect.Strength = InStrength;
	Effect.NextPulse = Now() + FirstPulse;
	Effect.Pulses = 0;
}

float UVaelPotionEffects::Strength(FName PotionId) const
{
	const FLasting* Effect = Lasting.Find(PotionId);
	return Effect != nullptr && Effect->EndTime > Now() ? Effect->Strength : 0.0f;
}

bool UVaelPotionEffects::IsActive(FName PotionId) const
{
	return Strength(PotionId) > 0.0f;
}

TArray<TPair<FName, float>> UVaelPotionEffects::GetActive() const
{
	TArray<TPair<FName, float>> Active;
	for (const TPair<FName, FLasting>& Pair : Lasting)
	{
		if (Pair.Value.EndTime > Now())
		{
			Active.Emplace(Pair.Key, Pair.Value.EndTime - Now());
		}
	}
	return Active;
}

bool UVaelPotionEffects::Drink(FName Id, float S, float D)
{
	AVaelCharacter* Player = GetPlayer();
	UAbilitySystemComponent* AbilitySystem = Player != nullptr ? Player->GetAbilitySystemComponent() : nullptr;
	if (AbilitySystem == nullptr)
	{
		return false;
	}

	if (Id == Manatrank)
	{
		// 40 % of the mana at once, then faster recovery for 5 minutes
		const float MaxMana = AbilitySystem->GetNumericAttribute(UVaelAttributeSet::GetMaxManaAttribute());
		const float Mana = AbilitySystem->GetNumericAttribute(UVaelAttributeSet::GetManaAttribute());
		AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetManaAttribute(), FMath::Min(MaxMana, Mana + MaxMana * 0.4f * S));
		Start(Id, 5.0f * Minute * D, S);
	}
	else if (Id == Loeschtrank)
	{
		UVaelCombatStatics::RemoveStatus(Player, EVaelStatus::Burning);
		Start(Id, 5.0f * Minute * D, S);
	}
	else if (Id == Reinigungstrank)
	{
		UVaelBalanceSubsystem::ChangePlayer(Player, -10.0f * S, LOCTEXT("CleanseReason", "Reinigungstrank"));
	}
	else if (Id == Gruppenheilung)
	{
		// a cloud of healing at the feet: every player in it is healed
		for (TActorIterator<AVaelCharacter> It(GetWorld()); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), Player->GetActorLocation()) < 600.0f && !It->IsDowned())
			{
				UVaelCombatStatics::Heal(*It, It->GetMaxHealth() * 0.35f * S);
			}
		}
	}
	else if (Id == Heiltrank)
	{
		return Player->DrinkPotion();
	}
	else if (Id == Nachtsicht || Id == Ascheschleier || Id == Leichtfuss || Id == Rindenhaut || Id == Feuerschutz || Id == Blutoel || Id == Daemmertrank || Id == Blutrausch)
	{
		Start(Id, 15.0f * Minute * D, S);
	}
	else if (Id == Hainruf || Id == Wuergherrschaft || Id == Aschegestalt || Id == Daemmergarten)
	{
		Start(Id, 20.0f * Minute * D, S, 2.0f);
	}
	else if (Id == Feenleib || Id == Glutblut || Id == Sternenblick)
	{
		Start(Id, 30.0f * Minute * D, S, 2.0f);
	}
	else
	{
		return false;
	}

	// Nachtsicht and Sternenblick: a soft light that goes with the player
	if ((Id == Nachtsicht || Id == Sternenblick) && SightLight == nullptr)
	{
		SightLight = NewObject<UPointLightComponent>(Player, TEXT("PotionSight"));
		SightLight->SetupAttachment(Player->GetRootComponent());
		SightLight->SetRelativeLocation(FVector(0.0f, 0.0f, 260.0f));
		SightLight->SetIntensityUnits(ELightUnits::Candelas);
		SightLight->SetCastShadows(false);
		SightLight->RegisterComponent();
	}
	if (SightLight != nullptr && (Id == Nachtsicht || Id == Sternenblick))
	{
		const bool bStars = Id == Sternenblick;
		SightLight->SetLightColor(bStars ? FLinearColor(0.75f, 0.72f, 1.0f) : FLinearColor(0.7f, 1.0f, 0.75f));
		SightLight->SetIntensity(bStars ? 60.0f : 40.0f);
		SightLight->SetAttenuationRadius(bStars ? 1700.0f : 1100.0f);
		SightLight->SetVisibility(true);
	}
	return true;
}

bool UVaelPotionEffects::Throw(FName Id, float S)
{
	AVaelCharacterBase* Target = FindEnemy(BowRange);
	if (Target == nullptr)
	{
		UVaelCombatTextSubsystem::PostReaction(GetOwner(), LOCTEXT("NoTarget", "Kein Ziel in Wurfweite"));
		return false;
	}
	const FVaelRecipe* Recipe = FVaelBrewing::GetRecipes().FindByPredicate([Id](const FVaelRecipe& Each) { return Each.Id == Id; });
	// Near the sling: the vial flies in a flat arc from the hand. Far the bow: the vial is bound to an arrow and flies
	// almost straight and fast. Either way it bursts where the enemy stands when it lands.
	const FVector From = GetOwner()->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
	const FVector To = Target->GetActorLocation();
	const float Distance = FVector::Dist2D(From, To);
	const bool bBow = Distance > SlingRange;
	const float Flight = bBow ? FMath::Clamp(Distance / 3200.0f, 0.3f, 0.7f) : FMath::Clamp(Distance / 1600.0f, 0.25f, 0.8f);
	AVaelArcShot::Lob(GetWorld(), From, To, Flight, bBow ? 30.0f : 120.0f, Recipe != nullptr ? Recipe->Color : FLinearColor::White, bBow ? 10.0f : 16.0f);
	TWeakObjectPtr<AVaelCharacterBase> WeakTarget(Target);
	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Id, S, WeakTarget, To]()
	{
		Burst(Id, S, WeakTarget.IsValid() ? WeakTarget->GetActorLocation() : To);
	}), Flight, false);
	UVaelCombatTextSubsystem::PostReaction(GetOwner(), FText::Format(bBow ? LOCTEXT("BowShot", "Bogen: {0}") : LOCTEXT("SlingShot", "Schleuder: {0}"), Recipe != nullptr ? Recipe->Name : FText::FromName(Id)));
	UE_LOG(LogVael, Log, TEXT("Potion: %s fires %s with the %s over %.0f cm"), *GetOwner()->GetName(), *Id.ToString(), bBow ? TEXT("bow") : TEXT("sling"), Distance);
	return true;
}

void UVaelPotionEffects::Burst(FName Id, float S, const FVector& Center)
{
	AVaelCharacter* Player = GetPlayer();
	UE_LOG(LogVael, Log, TEXT("Potion: vial %s bursts at %s"), *Id.ToString(), *Center.ToCompactString());

	if (Id == Giftphiole)
	{
		// a poison cloud where it bursts: enemies in it bleed
		Clouds.Add({ Center, 320.0f, 7.0f * S, Now() + 8.0f, Now() });
	}
	else if (Id == Saeurewolke)
	{
		// acid that eats armour: enemies in it take more from everything, and it burns them a little
		Clouds.Add({ Center, 400.0f, 4.0f * S, Now() + 10.0f, Now() });
		for (AVaelCharacterBase* Enemy : FindEnemies(Center, 400.0f))
		{
			UVaelCombatStatics::ApplyStatus(Player, Enemy, EVaelStatus::Marked, 10.0f * S);
		}
	}
	else if (Id == Ermattungsfluch)
	{
		for (AVaelCharacterBase* Enemy : FindEnemies(Center, 400.0f))
		{
			UVaelCombatStatics::ApplyStatus(Player, Enemy, EVaelStatus::Slowed, 60.0f);
			UVaelCombatStatics::ApplyStatus(Player, Enemy, EVaelStatus::Confused, 6.0f * S);
		}
	}
	else if (Id == Schwarzbrand)
	{
		// black fire that is hard to put out: it burns long
		for (AVaelCharacterBase* Enemy : FindEnemies(Center, 320.0f))
		{
			UVaelCombatStatics::ApplyStatus(Player, Enemy, EVaelStatus::Burning, 12.0f, 8.0f * S);
		}
	}
}

AVaelCharacterBase* UVaelPotionEffects::FindEnemy(float Range) const
{
	const AActor* Owner = GetOwner();
	AVaelCharacterBase* Nearest = nullptr;
	float Best = Range;
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (It->IsA<AVaelCharacter>() || !UVaelCombatStatics::CanDamage(Owner, *It))
		{
			continue;
		}
		const float Distance = FVector::Dist2D(It->GetActorLocation(), Owner->GetActorLocation());
		if (Distance < Best)
		{
			Nearest = *It;
			Best = Distance;
		}
	}
	return Nearest;
}

TArray<AVaelCharacterBase*> UVaelPotionEffects::FindEnemies(const FVector& Center, float Radius) const
{
	TArray<AVaelCharacterBase*> Found;
	for (TActorIterator<AVaelCharacterBase> It(GetWorld()); It; ++It)
	{
		if (!It->IsA<AVaelCharacter>() && UVaelCombatStatics::CanDamage(GetOwner(), *It) && FVector::Dist2D(It->GetActorLocation(), Center) < Radius)
		{
			Found.Add(*It);
		}
	}
	return Found;
}

float UVaelPotionEffects::GetIncomingMultiplier(const FGameplayTagContainer& DamageTags) const
{
	float Factor = 1.0f;
	if (DamageTags.HasTag(VaelTags::Element_Fire))
	{
		// fire protection, the quenching potion's after-effect and Glutblut: each takes its share
		Factor *= 1.0f - 0.3f * Strength(Feuerschutz);
		Factor *= 1.0f - 0.2f * Strength(Loeschtrank);
		Factor *= 1.0f - FMath::Min(0.5f * Strength(Glutblut), 0.75f);
	}
	// bark skin: +20 % armour
	if (const float Bark = Strength(Rindenhaut); Bark > 0.0f)
	{
		Factor /= 1.0f + 0.2f * Bark;
	}
	// as ash: 15 % of the blows go through smoke
	if (const float Ash = Strength(Aschegestalt); Ash > 0.0f && FMath::FRand() < 0.15f * Ash)
	{
		UVaelCombatTextSubsystem::PostReaction(GetOwner(), LOCTEXT("Dodged", "Rauch"));
		return 0.0f;
	}
	return FMath::Max(Factor, 0.0f);
}

float UVaelPotionEffects::GetSpeedMultiplier() const
{
	float Factor = 1.0f + 0.2f * Strength(Leichtfuss);
	if (IsActive(Blutrausch) && Now() - LastFrenzyHit < 4.0f)
	{
		Factor *= 1.0f + 0.04f * FrenzyStacks;
	}
	return Factor;
}

float UVaelPotionEffects::GetNoticeMultiplier() const
{
	return IsActive(Ascheschleier) ? 0.5f / Strength(Ascheschleier) : 1.0f;
}

bool UVaelPotionEffects::ThrowsVials(const AVaelCharacter* Player)
{
	// Boris 2026-11-07: throwing belongs to the engineer, with vials of his own for sling and bow; every other class coats the weapon
	return Player != nullptr && Player->IsEngineer();
}

FName UVaelPotionEffects::GetWeaponOil() const
{
	return Now() < WeaponOilEnd ? WeaponOil : NAME_None;
}

void UVaelPotionEffects::OnDamageDealt(AActor* Target, float Damage)
{
	AVaelCharacter* Player = GetPlayer();
	if (Player == nullptr || Damage <= 0.0f)
	{
		return;
	}
	// the oil on the weapon: each enemy takes its effect once every few seconds
	const FName Oil = GetWeaponOil();
	AVaelCharacterBase* Enemy = Cast<AVaelCharacterBase>(Target);
	if (!Oil.IsNone() && Enemy != nullptr && Enemy != Player)
	{
		float& Last = OilTouched.FindOrAdd(Enemy, -100.0f);
		if (Now() - Last > 3.0f)
		{
			Last = Now();
			if (Oil == Giftphiole)
			{
				Clouds.Add({ Enemy->GetActorLocation(), 120.0f, 5.0f, Now() + 4.0f, Now() });
			}
			else if (Oil == Schwarzbrand)
			{
				UVaelCombatStatics::ApplyStatus(Player, Enemy, EVaelStatus::Burning, 6.0f, 5.0f);
			}
			else if (Oil == Ermattungsfluch)
			{
				UVaelCombatStatics::ApplyStatus(Player, Enemy, EVaelStatus::Slowed, 6.0f);
				UVaelCombatStatics::ApplyStatus(Player, Enemy, EVaelStatus::Confused, 2.0f);
			}
			else if (Oil == Saeurewolke)
			{
				UVaelCombatStatics::ApplyStatus(Player, Enemy, EVaelStatus::Marked, 5.0f);
			}
		}
	}
	// blood oil: the weapon steals 10 % of what it takes
	if (const float BloodOil = Strength(Blutoel); BloodOil > 0.0f)
	{
		UVaelCombatStatics::Heal(Player, Damage * 0.1f * BloodOil);
	}
	// blood frenzy: every hit heals a little and makes faster, up to 5 times
	if (const float Frenzy = Strength(Blutrausch); Frenzy > 0.0f)
	{
		FrenzyStacks = Now() - LastFrenzyHit < 4.0f ? FMath::Min(FrenzyStacks + 1, 5) : 1;
		LastFrenzyHit = Now();
		UVaelCombatStatics::Heal(Player, Player->GetMaxHealth() * 0.01f * Frenzy);
	}
}

void UVaelPotionEffects::OnDamageTaken(AActor* Attacker, float Damage)
{
	AVaelCharacter* Player = GetPlayer();
	const float Twilight = Strength(Daemmertrank);
	if (Player == nullptr || Twilight <= 0.0f || Damage <= 0.0f)
	{
		return;
	}
	// heals a little at every hit and throws a share of the damage back at the attacker
	UVaelCombatStatics::Heal(Player, Player->GetMaxHealth() * 0.015f * Twilight);
	if (Attacker != nullptr && Attacker != Player && UVaelCombatStatics::CanDamage(Player, Attacker))
	{
		UVaelCombatStatics::DealDamage(Player, Attacker, Damage * 0.2f * Twilight, EVaelElement::Earth);
	}
}

void UVaelPotionEffects::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AVaelCharacter* Player = GetPlayer();
	UAbilitySystemComponent* AbilitySystem = Player != nullptr ? Player->GetAbilitySystemComponent() : nullptr;
	if (AbilitySystem == nullptr || Player->IsDowned())
	{
		return;
	}
	const float T = Now();

	// faster mana: the mana potion and Glutblut
	const float ManaBoost = 0.004f * Strength(Manatrank) + 0.003f * Strength(Glutblut);
	if (ManaBoost > 0.0f)
	{
		const float MaxMana = AbilitySystem->GetNumericAttribute(UVaelAttributeSet::GetMaxManaAttribute());
		const float Mana = AbilitySystem->GetNumericAttribute(UVaelAttributeSet::GetManaAttribute());
		AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetManaAttribute(), FMath::Min(MaxMana, Mana + MaxMana * ManaBoost * DeltaTime));
	}

	// the frenzy costs life while enemies are near
	if (IsActive(Blutrausch) && FindEnemy(1500.0f) != nullptr)
	{
		const float Health = Player->GetHealth();
		if (Health > Player->GetMaxHealth() * 0.2f)
		{
			AbilitySystem->SetNumericAttributeBase(UVaelAttributeSet::GetHealthAttribute(), Health - Player->GetMaxHealth() * 0.002f * DeltaTime);
		}
	}

	// pulses of the power potions
	for (TPair<FName, FLasting>& Pair : Lasting)
	{
		FLasting& Effect = Pair.Value;
		if (Effect.EndTime <= T || T < Effect.NextPulse)
		{
			continue;
		}
		const FName Id = Pair.Key;
		float Interval = 0.0f;
		if (Id == Hainruf)
		{
			// a tendril takes an enemy, sleeping pollen numbs it
			Interval = 20.0f;
			if (AVaelCharacterBase* Enemy = FindEnemy(PulseRange))
			{
				UVaelCombatStatics::ApplyStatus(Player, Enemy, EVaelStatus::Frozen, 2.5f * Effect.Strength);
				UVaelCombatStatics::ApplyStatus(Player, Enemy, EVaelStatus::Confused, 3.0f);
				UVaelCombatTextSubsystem::PostReaction(Enemy, LOCTEXT("Tendril", "Ranke"));
			}
		}
		else if (Id == Wuergherrschaft)
		{
			// plants creep at an enemy, choke and poison it
			Interval = 12.0f;
			if (AVaelCharacterBase* Enemy = FindEnemy(PulseRange))
			{
				UVaelCombatStatics::ApplyStatus(Player, Enemy, EVaelStatus::Frozen, 2.0f);
				Clouds.Add({ Enemy->GetActorLocation(), 160.0f, 6.0f * Effect.Strength, T + 6.0f, T });
				UVaelCombatTextSubsystem::PostReaction(Enemy, LOCTEXT("Choke", "Würgen"));
			}
		}
		else if (Id == Feenleib)
		{
			// a spark of the grove spirits strikes a near enemy
			Interval = 5.0f;
			if (AVaelCharacterBase* Enemy = FindEnemy(PulseRange))
			{
				UVaelCombatStatics::DealDamage(Player, Enemy, 12.0f * Effect.Strength, EVaelElement::Air);
			}
		}
		else if (Id == Daemmergarten)
		{
			// the plants change every 10 seconds between healing and choking
			Interval = 10.0f;
			if (Effect.Pulses % 2 == 0)
			{
				for (TActorIterator<AVaelCharacter> It(GetWorld()); It; ++It)
				{
					if (FVector::Dist2D(It->GetActorLocation(), Player->GetActorLocation()) < 600.0f && !It->IsDowned())
					{
						UVaelCombatStatics::Heal(*It, It->GetMaxHealth() * 0.05f * Effect.Strength);
					}
				}
			}
			else if (AVaelCharacterBase* Enemy = FindEnemy(PulseRange))
			{
				UVaelCombatStatics::ApplyStatus(Player, Enemy, EVaelStatus::Frozen, 2.0f);
				UVaelCombatStatics::DealDamage(Player, Enemy, 10.0f * Effect.Strength, EVaelElement::Earth);
			}
		}
		if (Interval > 0.0f)
		{
			Effect.NextPulse = T + Interval;
			++Effect.Pulses;
		}
		else
		{
			Effect.NextPulse = Effect.EndTime;
		}
	}

	// poison and acid clouds of thrown vials
	for (int32 Index = Clouds.Num() - 1; Index >= 0; --Index)
	{
		FCloud& Cloud = Clouds[Index];
		if (Cloud.EndTime <= T)
		{
			Clouds.RemoveAt(Index);
			continue;
		}
		// one bite a second, so the numbers stay readable
		if (T < Cloud.NextBite)
		{
			continue;
		}
		Cloud.NextBite = T + 1.0f;
		for (AVaelCharacterBase* Enemy : FindEnemies(Cloud.Center, Cloud.Radius))
		{
			UVaelCombatStatics::DealDamage(Player, Enemy, Cloud.DamagePerSecond, EVaelElement::Water);
		}
	}

	// the light of sight goes out with its potion
	if (SightLight != nullptr && !IsActive(Nachtsicht) && !IsActive(Sternenblick))
	{
		SightLight->SetVisibility(false);
	}
}

#undef LOCTEXT_NAMESPACE
