// Copyright Epic Games, Inc. All Rights Reserved.

#include "BltBPLibrary.h"

#include "FuzzingFlags.h"
#include "Kismet/GameplayStatics.h"
#include "PythonBridge.h"

DEFINE_LOG_CATEGORY(LogBlt);


bool UBltBPLibrary::ParseJson(const FString& FilePath, TSharedPtr<FJsonObject>& OutObject)
{
	FString AbsoluteFilePath;
	if (!GetAbsolutePath(FilePath, AbsoluteFilePath))
		return false;
	
	FString JsonRaw;
	FFileHelper::LoadFileToString(JsonRaw, *AbsoluteFilePath);
	if (!FJsonSerializer::Deserialize<TCHAR>(TJsonReaderFactory<TCHAR>::Create(JsonRaw), OutObject))
	{
		UE_LOG(LogBlt, Error, TEXT("Could not deserialize %s [check if file is JSON]"), *AbsoluteFilePath);
		return false;
	}

	return true;
}

bool UBltBPLibrary::GetAbsolutePath(const FString& FilePath, FString& AbsoluteFilePath)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	AbsoluteFilePath = FilePath;
	if (PlatformFile.FileExists(*AbsoluteFilePath))
		return true;

	AbsoluteFilePath = FPaths::ProjectContentDir() + FilePath;
	if (PlatformFile.FileExists(*AbsoluteFilePath))
		return true;

	UE_LOG(LogBlt, Error, TEXT("File %s not found [relative path starts from /Content/]"), *AbsoluteFilePath);
	return false;
}

UClass* UBltBPLibrary::FindClass(const FString& ClassName, const bool& bExactClass, UObject* const Package)
{
	check(*ClassName);
	UObject* const Outer = Package ? Package : ANY_PACKAGE;
	
	if (UClass* const ClassType = FindObject<UClass>(Outer, *ClassName, bExactClass))
		return ClassType;

	if (const UObjectRedirector* const RenamedClassRedirector
		= FindObject<UObjectRedirector>(Outer, *ClassName, bExactClass))
		return CastChecked<UClass>(RenamedClassRedirector->DestinationObject);

	UE_LOG(LogBlt, Warning, TEXT("Class %s could not be found!"), *ClassName);
	return nullptr;
}

TArray<AActor*> UBltBPLibrary::GetAllActorsOfClass(
	const UObject* const WorldContextObject,
	const FString& ActorClassName
)
{
	const TSubclassOf<AActor> ActorClass = FindClass(ActorClassName);
	if (!ActorClass)
		return TArray<AActor*>();

	TArray<AActor*> OutActors;
	UGameplayStatics::GetAllActorsOfClass(WorldContextObject->GetWorld(), ActorClass, OutActors);
	return OutActors;
}

void UBltBPLibrary::ApplyFuzzing(
	const UObject* const WorldContextObject,
	const FString& FilePath,
	const int32 Flags,
	const TArray<AActor*>& AffectedActors,
	const bool bUseArray
)
{
	TSharedPtr<FJsonObject> JsonParsed;
	if (!ParseJson(FilePath, JsonParsed))
		return;

	const TMap<FString, TSharedPtr<FJsonValue>> JsonClasses = JsonParsed.Get()->Values;
	for (const TTuple<FString, TSharedPtr<FJsonValue>>& JsonClass : JsonClasses)
	{
		const FString& ActorClassName = JsonClass.Key;
		const UClass* const& JsonActorClassType = FindClass(ActorClassName);
		if (!JsonActorClassType)
			continue;
		
		const TSharedPtr<FJsonObject>* ActorClassObject;
		if (!JsonClass.Value->TryGetObject(ActorClassObject))
		{
			UE_LOG(LogBlt, Error, TEXT("Entry %s must have an Object type value!"), *ActorClassName);
			continue;
		}

		const TMap<FString, TSharedPtr<FJsonValue>>& ActorClassProperties = ActorClassObject->Get()->Values;
		for (AActor* const& Actor :
			bUseArray ? AffectedActors : GetAllActorsOfClass(WorldContextObject, ActorClassName))
		{
			RandomiseProperties(Actor, JsonActorClassType, ActorClassProperties, Flags);
		}
	}
}

void UBltBPLibrary::K2ApplyFuzzing(
	const UObject* const WorldContextObject,
	const FString& FilePath,
	const int32 Flags,
	const TArray<AActor*>& AffectedActors,
	const bool bUseArray
)
{
	ApplyFuzzing(WorldContextObject, FilePath, Flags, AffectedActors, bUseArray);
}

void UBltBPLibrary::RandomiseProperties(
	AActor* const& Actor,
	const UClass* const& JsonActorClassType,
	const TMap<FString, TSharedPtr<FJsonValue>>& ActorClassProperties,
	const int32 FuzzingFlags
)
{
	if (!Actor)
		return;
	
	const UClass* const& ActorClass = Actor->GetClass();
	if (!ActorClass->IsChildOf(JsonActorClassType))
		return;

	const bool& bIncludeBase = FuzzingFlags & static_cast<uint8>(EFuzzingFlags::IncludeBase);
	const bool& bIncludeSuper = FuzzingFlags & static_cast<uint8>(EFuzzingFlags::IncludeSuper);
	const bool& bIncludeNull = FuzzingFlags & static_cast<uint8>(EFuzzingFlags::IncludeNull);
	
	for (TFieldIterator<FProperty> Iterator(ActorClass); Iterator; ++Iterator)
	{
		const FProperty* const Property = *Iterator;
		const FString& PropertyName = Property->GetNameCPP();
		
		if (!ActorClassProperties.Contains(PropertyName))
		{
			const UClass* const& OwnerClass = Property->GetOwnerClass();
			if (
				bIncludeBase && OwnerClass == JsonActorClassType ||
				bIncludeSuper && OwnerClass == JsonActorClassType->GetSuperClass()
			) {
				RandomisePropertiesDefault(Actor, Property);
			}
			continue;
		}

		const FJsonValue* const PropertyValue = ActorClassProperties.Find(PropertyName)->Get();
		switch (PropertyValue->Type)
		{
		case EJson::Array:
			RandomiseNumericProperty(Actor, Property, PropertyValue);
			break;

		case EJson::String:
			RandomiseStringProperty(Actor, Property, PropertyValue);
			break;

		case EJson::Null:
			if (bIncludeNull)
			{
				RandomisePropertiesDefault(Actor, Property);
			}
			break;
			
		default:
			break;
		}
	}
}

void UBltBPLibrary::RandomisePropertiesDefault(AActor* const& Actor, const FProperty* const& Property)
{
	RandomiseNumericProperty(Actor, Property);
	RandomiseStringProperty(Actor, Property);
}

void UBltBPLibrary::RandomiseNumericProperty(
	AActor* const& Actor,
	const FProperty* const& Property,
	const FJsonValue* const& PropertyValue
)
{
	const TArray<TSharedPtr<FJsonValue>>& Interval = PropertyValue ?
		PropertyValue->AsArray() : TArray<TSharedPtr<FJsonValue>>();
	
	const float& RandomNumber = FMath::FRandRange(
	Interval.Num() > 0u ?
		Interval[0u].Get()->AsNumber() : 0.0f,
		
	Interval.Num() > 1u ?
		Interval[1u].Get()->AsNumber() : FMath::FRandRange(0.0f, static_cast<float>(RAND_MAX))
	);
	
	if (const FNumericProperty* const NumericProperty = CastField<const FNumericProperty>(Property))
	{
		NumericProperty->SetNumericPropertyValueFromString(
			NumericProperty->ContainerPtrToValuePtr<float>(Actor),
			*FString::Printf(TEXT("%f"), RandomNumber)
		);
	}
	else if (const FBoolProperty* const BoolProperty = CastField<const FBoolProperty>(Property))
	{
		BoolProperty->SetPropertyValue_InContainer(Actor, static_cast<uint8>(RandomNumber) % 2u);
	}
	else if (PropertyValue)
	{
		UE_LOG(LogBlt, Error, TEXT("%s is not Numeric!"), *Property->GetFullName());
	}
}

void UBltBPLibrary::RandomiseStringProperty(
	AActor* const& Actor,
	const FProperty* const& Property,
	const FJsonValue* const& PropertyValue
)
{
	const UPythonBridge* const PythonBridge = UPythonBridge::Get();
	if (!PythonBridge)
	{
		UE_LOG(LogBlt, Error, TEXT("Python bridge could not be instantiated!"));
		return;
	}
	
	const FString& RandomString = PythonBridge->GenerateStringFromRegex(
		PropertyValue ? PropertyValue->AsString() : "[\\w\\W\\s\\S\\d\\D]{:255}"
	);
	
	if (const FStrProperty* const StringProperty = CastField<const FStrProperty>(Property))
	{
		StringProperty->SetPropertyValue_InContainer(Actor, RandomString);
	}
	else if (const FNameProperty* const NameProperty = CastField<const FNameProperty>(Property))
	{
		NameProperty->SetPropertyValue_InContainer(Actor, FName(RandomString));
	}
	else if (const FTextProperty* const TextProperty = CastField<const FTextProperty>(Property))
	{
		TextProperty->SetPropertyValue_InContainer(Actor, FText::FromString(RandomString));
	}
	else if (PropertyValue)
	{
		UE_LOG(LogBlt, Error, TEXT("%s is not FString, FName or FText!"), *Property->GetFullName());
	}
}
