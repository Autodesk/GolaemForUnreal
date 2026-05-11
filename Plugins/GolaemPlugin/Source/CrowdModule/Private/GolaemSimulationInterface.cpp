/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#include "GolaemSimulationInterface.h" //needs to be included first

#include "Runtime/Launch/Resources/Version.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Containers/Queue.h"

#include "GolaemCharacter.h"

//-------------------------------------------------------------------------
bool GolaemBoneNameCompare(const FName& A, const FName& B)
{
    FString AString = A.GetPlainNameString();
    FString BString = B.GetPlainNameString();
    int32 aSize = AString.Len();
    int32 bSize = BString.Len();
    int32 smallerSize = aSize < bSize ? aSize : bSize;
    size_t i = 0;

    for (; i < smallerSize && AString[i] == BString[i]; i++)
    {
    }

    // different before end
    if (i < smallerSize)
    {
        return (static_cast<int32>(AString[i]) < static_cast<int32>(BString[i]));
    }
    else // identical up to smallest string, compare size
    {
        return (static_cast<int32>(aSize) < static_cast<int32>(bSize));
    }
}

//-------------------------------------------------------------------------
void IGolaemSimulationInterface::ComputeSkeleton(AGolaemCharacter* entityActor, TArray<int32>& boneHierarchy)
{
    TArray<TMap<FName, int32>> SkeletonHierarchy;
    TArray<FName> SkeletonBoneNames;

    // create skeleton hierarchy
    USkeletalMeshComponent* ThisSkeletalMeshComp = /*MergeMode ? entityActor->GolaemMesh : */ entityActor->GetSkeletalMeshComponent();
    ThisSkeletalMeshComp->GetBoneNames(SkeletonBoneNames);

    SkeletonHierarchy.SetNum(SkeletonBoneNames.Num());
    for (int32 iBone = 0; iBone < SkeletonBoneNames.Num(); ++iBone)
    {
        int32 parentIndex = ThisSkeletalMeshComp->GetSkeletalMeshAsset()->GetRefSkeleton().GetParentIndex(iBone);
        if (parentIndex != -1)
        {
            SkeletonHierarchy[parentIndex].Add(SkeletonBoneNames[iBone], iBone);
        }
    }

    // compute sorted bones
    TArray<int32> SortedBoneIds;
    if (SkeletonHierarchy.Num())
    {
        TQueue<int32> BonesToProcess;
        BonesToProcess.Enqueue(0);
        while (!BonesToProcess.IsEmpty())
        {
            int32 BoneIdToProcess;
            BonesToProcess.Dequeue(BoneIdToProcess);

            //sorting the map, using a predicate class
            SkeletonHierarchy[BoneIdToProcess].KeySort([](FName A, FName B) { return GolaemBoneNameCompare(A, B); });

            TMap<FName, int32>::TIterator iterator = SkeletonHierarchy[BoneIdToProcess].CreateIterator();

            while (iterator)
            {
                BonesToProcess.Enqueue(iterator.Value());
                iterator.operator++();
            }
            SortedBoneIds.Add(BoneIdToProcess); // TODO: check if it's a bone before inserting
        }
    }

    // todo error message id SortedBoneIds.Num() != SkeletonBoneNames.Num()
    // todo filtrer les noms des bones en fonctions de ce qui est contenu dans le gcha

    // get relative bone names
    for (int32 iSorted = 0; iSorted < SortedBoneIds.Num(); iSorted++)
    {
        if (SkeletonBoneNames[SortedBoneIds[iSorted]] != "DeformationSystem")
        {
            boneHierarchy.Add(ThisSkeletalMeshComp->GetBoneIndex(SkeletonBoneNames[SortedBoneIds[iSorted]]));
        }
    }
}
