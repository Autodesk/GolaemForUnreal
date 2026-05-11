/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#include "Engine/StaticMeshActor.h"

#include "GDAProperty.generated.h"

class AGolaemSimulation;

UENUM()
namespace GDAPropertyDataType
{
    enum Type
    {
        None,
        Distance,
        Angle,
        Mass,
        Speed,
        Acceleration,
        Color,
        Force,
        Direction,
        END
    };
}

UCLASS(BlueprintType)
class UGDAProperty : public UObject
{
    GENERATED_BODY()
public:
    UGDAProperty();

    UPROPERTY(VisibleAnywhere, Category = "GDA")
    FString GDAAttributeShortName;

    UPROPERTY(VisibleAnywhere, Category = "GDA")
    FString GDAAttributeFullName;

    UPROPERTY(VisibleAnywhere, Category = "GDA")
    uint64 Dimension;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDA")
    TArray<int64> Sizes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDA")
    TArray<int64> ArraySizes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDA")
    bool Override = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDA", meta = (DisplayName = "Data Type"))
    TEnumAsByte<GDAPropertyDataType::Type> DataTypeId = GDAPropertyDataType::END;

    UPROPERTY(VisibleAnywhere, Category = "Simulation")
    AGolaemSimulation* GolaemSimulation = NULL;

#if WITH_EDITOR
    virtual bool CanEditChange(const FProperty* InProperty) const override;
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& e) override;
#endif
};

UCLASS(BlueprintType)
class UGDAInt32Property : public UGDAProperty
{
    GENERATED_BODY()
public:
    UGDAInt32Property();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDA", meta = (EditCondition = "Override"))
    TArray<int32> Value;

#if WITH_EDITOR
    virtual bool CanEditChange(const FProperty* InProperty) const override;
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& e) override;
#endif
};

UCLASS(BlueprintType)
class UGDAInt64Property : public UGDAProperty
{
    GENERATED_BODY()
public:
    UGDAInt64Property();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDA", meta = (EditCondition = "Override"))
    TArray<int64> Value;

#if WITH_EDITOR
    virtual bool CanEditChange(const FProperty* InProperty) const override;
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& e) override;
#endif
};

UCLASS(BlueprintType)
class UGDAFloatProperty : public UGDAProperty
{
    GENERATED_BODY()
public:
    UGDAFloatProperty();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDA", meta = (EditCondition = "Override"))
    TArray<float> Value;

#if WITH_EDITOR
    virtual bool CanEditChange(const FProperty* InProperty) const override;
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& e) override;
#endif
};

UCLASS(BlueprintType)
class UGDAVector3Property : public UGDAProperty
{
    GENERATED_BODY()
public:
    UGDAVector3Property();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDA", meta = (EditCondition = "Override"))
    TArray<FVector> Value;

#if WITH_EDITOR
    virtual bool CanEditChange(const FProperty* InProperty) const override;
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& e) override;
#endif
};

UCLASS(BlueprintType)
class UGDAVector4Property : public UGDAProperty
{
    GENERATED_BODY()
public:
    UGDAVector4Property();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDA", meta = (EditCondition = "Override"))
    TArray<FVector4> Value;

#if WITH_EDITOR
    virtual bool CanEditChange(const FProperty* InProperty) const override;
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& e) override;
#endif
};

UCLASS(BlueprintType)
class UGDAStringProperty : public UGDAProperty
{
    GENERATED_BODY()
public:
    UGDAStringProperty();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDA", meta = (EditCondition = "Override"))
    TArray<FString> Value;

#if WITH_EDITOR
    virtual bool CanEditChange(const FProperty* InProperty) const override;
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& e) override;
#endif
};

UCLASS(BlueprintType)
class UGDAMeshProperty : public UGDAProperty
{
    GENERATED_BODY()
public:
    UGDAMeshProperty();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDA", meta = (EditCondition = "Override"))
    TArray<AStaticMeshActor*> Value;

#if WITH_EDITOR
    virtual bool CanEditChange(const FProperty* InProperty) const override;
    virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& e) override;
#endif
};
