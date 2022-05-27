// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BLTBPLibrary.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBlt, Log, All);


UCLASS(Abstract)
class UBltBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static bool ParseJson(const FString& FilePath, TSharedPtr<FJsonObject>& OutObject);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game Testing")
	static bool GetAbsolutePath(const FString& FilePath, FString& AbsoluteFilePath);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game Testing")
	static UClass* FindClass(
		const FString& ClassName,
		const bool& bExactClass = false,
		UObject* const Package = nullptr
	);
	
	UFUNCTION(BlueprintCallable, Category = "Game Testing", meta = (WorldContext = "WorldContextObject"))
	static TArray<AActor*> GetAllActorsOfClass(const UObject* const WorldContextObject, const FString& ActorClassName);
	
	static void ApplyFuzzing(
		const UObject* const WorldContextObject,
		const FString& FilePath,
		const int32 Flags,
		const TArray<AActor*>& AffectedActors = TArray<AActor*>(),
		const bool bUseArray = false
	);
	
	UFUNCTION(BlueprintCallable, Category = "Game Testing", meta = (
		DisplayName = "Apply Fuzzing",
		WorldContext = "WorldContextObject",
		AutoCreateRefTerm = "AffectedActors"
	))
	static void K2ApplyFuzzing(
		const UObject* const WorldContextObject,
		const FString& FilePath,
		UPARAM(meta = (Bitmask, BitmaskEnum = "EFuzzingFlags")) const int32 Flags,
		const TArray<AActor*>& AffectedActors,
		const bool bUseArray = false
	);

	static void RandomiseProperties(
		AActor* const& Actor,
		const UClass* const& JsonActorClassType,
		const TMap<FString, TSharedPtr<FJsonValue>>& ActorClassProperties,
		const int32 FuzzingFlags
	);

	static void RandomisePropertiesDefault(AActor* const& Actor, const FProperty* const& Property);
	
	static void RandomiseNumericProperty(
		AActor* const& Actor,
		const FProperty* const& Property,
		const FJsonValue* const& PropertyValue = nullptr
	);
	
	static void RandomiseStringProperty(
		AActor* const& Actor,
		const FProperty* const& Property,
		const FJsonValue* const& PropertyValue = nullptr
	);
};
