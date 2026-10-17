// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"
#include "Misc/ScopeLock.h"

/** Catches warnings and errors the log writes while the check runs */
class FLogCatcher : public FOutputDevice
{
public:

	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		const ELogVerbosity::Type Level = static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
		if (Level != ELogVerbosity::Error && Level != ELogVerbosity::Warning && Level != ELogVerbosity::Fatal)
		{
			return;
		}

		FScopeLock Lock(&Guard);
		(Level == ELogVerbosity::Warning ? Warnings : Errors).Add(FString::Printf(TEXT("%s: %s"), *Category.ToString(), Message));
	}

	virtual bool CanBeUsedOnAnyThread() const override { return true; }

	/** Hands out what was caught since the last call and forgets it */
	void Take(TArray<FString>& OutWarnings, TArray<FString>& OutErrors)
	{
		FScopeLock Lock(&Guard);
		OutWarnings = MoveTemp(Warnings);
		OutErrors = MoveTemp(Errors);
		Warnings.Reset();
		Errors.Reset();
	}

private:

	FCriticalSection Guard;
	TArray<FString> Warnings;
	TArray<FString> Errors;
};
