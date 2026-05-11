/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#include "GDAProperty.h"
#include "GolaemSimulation.h"

//-----------------------------------------------------------------------------
UGDAProperty::UGDAProperty()
{
}

#if WITH_EDITOR
//-----------------------------------------------------------------------------
bool UGDAProperty::CanEditChange(const FProperty* InProperty) const
{
    const bool ParentVal = Super::CanEditChange(InProperty);
    if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UGDAProperty, Sizes))
    {
        return Override && Dimension > 0;
    }
    if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UGDAProperty, ArraySizes))
    {
        return Override && Dimension > 0;
    }
    if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UGDAProperty, DataTypeId))
    {
        return Override;
    }
    return ParentVal;
}

//-----------------------------------------------------------------------------
void UGDAProperty::PostEditChangeChainProperty(FPropertyChangedChainEvent& e)
{
    if (e.PropertyChain.GetHead()->GetValue()->GetFName() == GET_MEMBER_NAME_CHECKED(UGDAProperty, Override))
    {
        if (GolaemSimulation != NULL)
        {
            GolaemSimulation->ReloadSimuFromFile();
        }
    }

    Super::PostEditChangeChainProperty(e);
}

#endif

//-----------------------------------------------------------------------------
UGDAInt32Property::UGDAInt32Property()
{
}

#if WITH_EDITOR
//-----------------------------------------------------------------------------
bool UGDAInt32Property::CanEditChange(const FProperty* InProperty) const
{
    const bool ParentVal = Super::CanEditChange(InProperty);
    if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UGDAInt32Property, Value))
    {
        return Override;
    }
    return ParentVal;
}

//-----------------------------------------------------------------------------
void UGDAInt32Property::PostEditChangeChainProperty(FPropertyChangedChainEvent& e)
{
    if (e.PropertyChain.GetHead()->GetValue()->GetFName() == GET_MEMBER_NAME_CHECKED(UGDAInt32Property, Value))
    {
        if (GolaemSimulation != NULL)
        {
            GolaemSimulation->ReloadSimuFromFile();
        }
    }

    Super::PostEditChangeChainProperty(e);
}

#endif

//-----------------------------------------------------------------------------
UGDAInt64Property::UGDAInt64Property()
{
}

#if WITH_EDITOR
//-----------------------------------------------------------------------------
bool UGDAInt64Property::CanEditChange(const FProperty* InProperty) const
{
    const bool ParentVal = Super::CanEditChange(InProperty);
    if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UGDAInt64Property, Value))
    {
        return Override;
    }
    return ParentVal;
}

//-----------------------------------------------------------------------------
void UGDAInt64Property::PostEditChangeChainProperty(FPropertyChangedChainEvent& e)
{
    if (e.PropertyChain.GetHead()->GetValue()->GetFName() == GET_MEMBER_NAME_CHECKED(UGDAInt64Property, Value))
    {
        if (GolaemSimulation != NULL)
        {
            GolaemSimulation->ReloadSimuFromFile();
        }
    }

    Super::PostEditChangeChainProperty(e);
}

#endif

//-----------------------------------------------------------------------------
UGDAFloatProperty::UGDAFloatProperty()
{
}

#if WITH_EDITOR
//-----------------------------------------------------------------------------
bool UGDAFloatProperty::CanEditChange(const FProperty* InProperty) const
{
    const bool ParentVal = Super::CanEditChange(InProperty);
    if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UGDAFloatProperty, Value))
    {
        return Override;
    }
    return ParentVal;
}

//-----------------------------------------------------------------------------
void UGDAFloatProperty::PostEditChangeChainProperty(FPropertyChangedChainEvent& e)
{
    if (e.PropertyChain.GetHead()->GetValue()->GetFName() == GET_MEMBER_NAME_CHECKED(UGDAFloatProperty, Value))
    {
        if (GolaemSimulation != NULL)
        {
            GolaemSimulation->ReloadSimuFromFile();
        }
    }

    Super::PostEditChangeChainProperty(e);
}

#endif

//-----------------------------------------------------------------------------
UGDAVector3Property::UGDAVector3Property()
{
}

#if WITH_EDITOR
//-----------------------------------------------------------------------------
bool UGDAVector3Property::CanEditChange(const FProperty* InProperty) const
{
    const bool ParentVal = Super::CanEditChange(InProperty);
    if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UGDAVector3Property, Value))
    {
        return Override;
    }
    return ParentVal;
}

//-----------------------------------------------------------------------------
void UGDAVector3Property::PostEditChangeChainProperty(FPropertyChangedChainEvent& e)
{
    if (e.PropertyChain.GetHead()->GetValue()->GetFName() == GET_MEMBER_NAME_CHECKED(UGDAVector3Property, Value))
    {
        if (GolaemSimulation != NULL)
        {
            GolaemSimulation->ReloadSimuFromFile();
        }
    }

    Super::PostEditChangeChainProperty(e);
}

#endif

//-----------------------------------------------------------------------------
UGDAVector4Property::UGDAVector4Property()
{
}

#if WITH_EDITOR
//-----------------------------------------------------------------------------
bool UGDAVector4Property::CanEditChange(const FProperty* InProperty) const
{
    const bool ParentVal = Super::CanEditChange(InProperty);
    if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UGDAVector4Property, Value))
    {
        return Override;
    }
    return ParentVal;
}

//-----------------------------------------------------------------------------
void UGDAVector4Property::PostEditChangeChainProperty(FPropertyChangedChainEvent& e)
{
    if (e.PropertyChain.GetHead()->GetValue()->GetFName() == GET_MEMBER_NAME_CHECKED(UGDAVector4Property, Value))
    {
        if (GolaemSimulation != NULL)
        {
            GolaemSimulation->ReloadSimuFromFile();
        }
    }

    Super::PostEditChangeChainProperty(e);
}

#endif

//-----------------------------------------------------------------------------
UGDAStringProperty::UGDAStringProperty()
{
}

#if WITH_EDITOR
//-----------------------------------------------------------------------------
bool UGDAStringProperty::CanEditChange(const FProperty* InProperty) const
{
    const bool ParentVal = Super::CanEditChange(InProperty);
    if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UGDAStringProperty, Value))
    {
        return Override;
    }
    return ParentVal;
}

//-----------------------------------------------------------------------------
void UGDAStringProperty::PostEditChangeChainProperty(FPropertyChangedChainEvent& e)
{
    if (e.PropertyChain.GetHead()->GetValue()->GetFName() == GET_MEMBER_NAME_CHECKED(UGDAStringProperty, Value))
    {
        if (GolaemSimulation != NULL)
        {
            GolaemSimulation->ReloadSimuFromFile();
        }
    }

    Super::PostEditChangeChainProperty(e);
}

#endif

//-----------------------------------------------------------------------------
UGDAMeshProperty::UGDAMeshProperty()
{
}

#if WITH_EDITOR
//-----------------------------------------------------------------------------
bool UGDAMeshProperty::CanEditChange(const FProperty* InProperty) const
{
    const bool ParentVal = Super::CanEditChange(InProperty);
    if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UGDAMeshProperty, Value))
    {
        return Override;
    }
    return ParentVal;
}

//-----------------------------------------------------------------------------
void UGDAMeshProperty::PostEditChangeChainProperty(FPropertyChangedChainEvent& e)
{
    if (e.PropertyChain.GetHead()->GetValue()->GetFName() == GET_MEMBER_NAME_CHECKED(UGDAMeshProperty, Value))
    {
        if (GolaemSimulation != NULL)
        {
            GolaemSimulation->ReloadSimuFromFile();
        }
    }

    Super::PostEditChangeChainProperty(e);
}

#endif
