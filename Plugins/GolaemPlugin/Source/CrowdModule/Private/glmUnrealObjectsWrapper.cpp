/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#include "glmUnrealObjectsWrapper.h"

#include "UObject/UnrealType.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "StaticMeshResources.h"

#include "GolaemSimulation.h"

#include <glmSpecificWrapper.h>
#include <glmCrowdTerrainMesh.h>

namespace glm
{
    // swap y and z coordinates
    FMatrix44f UnrealObjectsWrapper::MatUEToGda = FMatrix44f(FVector3f(1, 0, 0), FVector3f(0, 0, 1), FVector3f(0, 1, 0), FVector3f(0, 0, 0));

    //-------------------------------------------------------------------------
    UnrealObjectsWrapper::UnrealObjectsWrapper(engine::Engine* engine, AGolaemSimulation* unrealActor)
        : AbstractObjectsWrapper()
        , _engine(engine)
        , _unrealActor(unrealActor)
    {
    }

    //-------------------------------------------------------------------------
    UnrealObjectsWrapper::~UnrealObjectsWrapper()
    {
    }

    //-------------------------------------------------------------------------
    void UnrealObjectsWrapper::update(Time dt)
    {
        _currentTime += dt;

        //wrapper push exemple
        if (_engine != NULL)
        {
            Array<engine::AbstractWrapper*>& objectsToWrap = _engine->touchObjectsToWrap();
            for (size_t iObj = 0, objCount = objectsToWrap.size(); iObj < objCount; ++iObj)
            {
                engine::AbstractWrapper* wrapper = objectsToWrap[iObj];
                if (wrapper->_identifier.compare("currentTime") == 0)
                {
                    if (wrapper->getType() == engine::WrapperType::DOUBLE)
                    {
                        engine::SpecificWrapper<Time>* objectAsTimeWrapper = static_cast<engine::SpecificWrapper<Time>*>(wrapper);

                        ScopedLock<glm::SpinLock> dataLock(wrapper->_dataLock);

                        Time& currentTimeValue = objectAsTimeWrapper->touchData();
                        currentTimeValue = _currentTime;
                    }
                    break;
                }
            }
        }
    }

    //-------------------------------------------------------------------------
    void exportToGtg(const crowdio::crowdTerrain::TerrainMesh& terrainMesh, const char* file, bool useTransform = true)
    {
        // for debugging created terrainMesh (especially transforms)

        crowdio::GlmFileMesh* fileMesh = new crowdio::GlmFileMesh();
        memset(fileMesh, 0, sizeof(crowdio::GlmFileMesh));
        crowdio::GlmFileMeshTransform* fileMeshTransform = new crowdio::GlmFileMeshTransform();
        memset(fileMeshTransform, 0, sizeof(crowdio::GlmFileMeshTransform));
        crowdio::glmFileSetString(fileMesh->_meshName, "UETest");

        fileMesh->_vertexCount = terrainMesh._subMeshes[0]->_vertexCount;
        fileMesh->_triangleCount = terrainMesh._subMeshes[0]->_indiceCount / 3;

        fileMesh->_trianglesVertexIndices = (uint32_t(*)[3])GLM_ALLOC_NOPROFILING(fileMesh->_triangleCount * 3 * sizeof(uint32_t), glm::crowdio::MemoryStatIds::GolaemCrowdIO);
        for (size_t iIdx = 0; iIdx < fileMesh->_triangleCount; ++iIdx)
        {
            fileMesh->_trianglesVertexIndices[iIdx][0] = terrainMesh._subMeshes[0]->_indices[3 * iIdx];
            fileMesh->_trianglesVertexIndices[iIdx][1] = terrainMesh._subMeshes[0]->_indices[3 * iIdx + 1];
            fileMesh->_trianglesVertexIndices[iIdx][2] = terrainMesh._subMeshes[0]->_indices[3 * iIdx + 2];
        }

        fileMesh->_vertices = (crowdio::GlmFileMeshVertex*)GLM_ALLOC_NOPROFILING(sizeof(crowdio::GlmFileMeshVertex) * fileMesh->_vertexCount, glm::crowdio::MemoryStatIds::GolaemCrowdIO);
        memset(fileMesh->_vertices, 0, sizeof(crowdio::GlmFileMeshVertex) * fileMesh->_vertexCount);
        if (useTransform)
        {
            // get local positions
            for (size_t iVtx = 0; iVtx < fileMesh->_vertexCount; ++iVtx)
            {
                fileMesh->_vertices[iVtx]._position[0] = terrainMesh._subMeshes[0]->_vertices[iVtx][0];
                fileMesh->_vertices[iVtx]._position[1] = terrainMesh._subMeshes[0]->_vertices[iVtx][1];
                fileMesh->_vertices[iVtx]._position[2] = terrainMesh._subMeshes[0]->_vertices[iVtx][2];
            }

            memcpy(fileMeshTransform->_defaultLocalToWorldMatrix, terrainMesh._subMeshes[0]->_localToWorld._matrix, sizeof(float[16]));
        }
        else
        {
            // get world positions
            for (size_t iVtx = 0; iVtx < fileMesh->_vertexCount; ++iVtx)
            {
                Vector3 worldPos = transformPoint(terrainMesh._subMeshes[0]->_vertices[iVtx], terrainMesh._subMeshes[0]->_localToWorld);
                fileMesh->_vertices[iVtx]._position[0] = worldPos[0];
                fileMesh->_vertices[iVtx]._position[1] = worldPos[1];
                fileMesh->_vertices[iVtx]._position[2] = worldPos[2];
            }

            // positions are in world coords, set matrix to identity
            memcpy(fileMeshTransform->_defaultLocalToWorldMatrix, Matrix4::identity._matrix, sizeof(float[16]));
        }

        crowdio::crowdTerrain::glmWriteTerrainFile(file, 1, fileMesh, 1, fileMeshTransform);

        delete fileMesh;
        delete fileMeshTransform;
    }

    //-------------------------------------------------------------------------
    void UnrealObjectsWrapper::updatePinMeshFromMeshActor(engine::GtgPinMesh::SP pinMesh, AStaticMeshActor* meshActor)
    {
        if (meshActor != NULL && meshActor->GetStaticMeshComponent() != NULL && meshActor->GetStaticMeshComponent()->GetStaticMesh() != NULL)
        {
            UStaticMesh* staticMesh = meshActor->GetStaticMeshComponent()->GetStaticMesh();
            if (staticMesh->HasValidRenderData())
            {
                FStaticMeshLODResources& lod0 = staticMesh->GetRenderData()->LODResources[0];

                if (pinMesh->getVertexCount() != staticMesh->GetNumVertices(0) || pinMesh->getTotalTriangleCount() != lod0.GetNumTriangles())
                {
                    // reconstruct the mesh
                    pinMesh->initialize(lod0.GetNumTriangles());

                    FStaticMeshVertexBuffers& vertexBuffers = lod0.VertexBuffers;
                    FPositionVertexBuffer& positionBuffer = vertexBuffers.PositionVertexBuffer;
                    FRawStaticIndexBuffer& indexBuffer = lod0.IndexBuffer;

                    FVector3f unrealValue;
                    int currentTriangle = 0;
                    for (uint32 iTriangle = 0, triangleCount = pinMesh->getTotalTriangleCount(); iTriangle < triangleCount; ++iTriangle)
                    {
                        for (uint32 iVertex = 0; iVertex < 3; ++iVertex)
                        {
                            int* rayCastToMeshIndices = pinMesh->getRayCastToMeshIndices();
                            rayCastToMeshIndices[3 * currentTriangle + iVertex] = 3 * currentTriangle + iVertex; // 0 -> 0, 1 -> 1, etc. suboptimal as no sharing of vertices occurs

                            uint32 vertexIndex = indexBuffer.GetIndex(3 * iTriangle + iVertex);
                            PinMesh::RayCastVertex& pinMeshVertex = pinMesh->touchMeshVertices()[3 * currentTriangle + iVertex];
                            {
                                unrealValue = MatUEToGda.TransformPosition(positionBuffer.VertexPosition(vertexIndex));
                                Vector3& glmValue = pinMeshVertex._position;
                                glmValue.setValues(&unrealValue.X);
                                glmValue /= _unrealActor->_crowdUnitFactor;
                            }
                            {
                                unrealValue = MatUEToGda.TransformVector(vertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(vertexIndex));
                                Vector3& glmValue = pinMeshVertex._normal;
                                glmValue.setValues(&unrealValue.X);
                            }
                            pinMeshVertex._faceIndex = iTriangle;
                        }
                        pinMesh->computeFaceNormals(currentTriangle);

                        // check for degenerated triangle and take it out
                        if (pinMesh->getFaceNormal(currentTriangle).norm2() < GLM_NUMERICAL_PRECISION)
                        {
                            continue;
                        }

                        ++currentTriangle;
                    }

                    pinMesh->setTotalTriangleCount(currentTriangle);

                    // must do this for raycasting to work
                    pinMesh->finalize();
                }

                // set transform matrix
                FTransform meshTransform = meshActor->GetStaticMeshComponent()->GetComponentTransform();
                FMatrix44f meshTransformMatrix = MatUEToGda * FTransform3f(meshTransform).ToMatrixWithScale() * MatUEToGda;
                meshTransformMatrix.ScaleTranslation(FVector3f(1 / _unrealActor->_crowdUnitFactor));
                pinMesh->setWorldMatrix(glm::Matrix4(&meshTransformMatrix.M[0][0]));
            }
        }
    }

    //-------------------------------------------------------------------------
    void UnrealObjectsWrapper::fetchData(engine::AbstractWrapper* wrapper)
    {
        const UGDAProperty* gdaProp = NULL;
        {
            ScopedLock<glm::SpinLock> propertiesLock(_propertiesLock);
            UGDAProperty** foundProperty = _foundProperties.Find(wrapper->_identifier.c_str());
            if (foundProperty == NULL)
            {
                UGDAProperty* actorProp = _unrealActor->FindGdaPropertyByShortName(wrapper->_identifier.c_str());
                _foundProperties.Add(wrapper->_identifier.c_str(), actorProp);
                gdaProp = actorProp;
            }
            else
            {
                gdaProp = *foundProperty;
            }
        }
        if (gdaProp == NULL || !gdaProp->Override)
        {
            return;
        }

        ScopedLock<glm::SpinLock> dataLock(wrapper->_dataLock);
        engine::WrapperType::Value wrapperType = wrapper->getType();
        switch (wrapperType)
        {
        case engine::WrapperType::INTEGER:
        {
            if (gdaProp->IsA(UGDAInt32Property::StaticClass()))
            {
                const UGDAInt32Property* gdaSpecificProp = static_cast<const UGDAInt32Property*>(gdaProp);
                if (gdaSpecificProp->Value.Num() > 0)
                {
                    engine::SpecificWrapper<int>* objectAsIntWrapper = static_cast<engine::SpecificWrapper<int>*>(wrapper);
                    objectAsIntWrapper->touchData() = gdaSpecificProp->Value[0];
                }
            }
            else if (gdaProp->IsA(UGDAInt64Property::StaticClass()))
            {
                const UGDAInt64Property* gdaSpecificProp = static_cast<const UGDAInt64Property*>(gdaProp);
                if (gdaSpecificProp->Value.Num() > 0)
                {
                    engine::SpecificWrapper<int>* objectAsIntWrapper = static_cast<engine::SpecificWrapper<int>*>(wrapper);
                    objectAsIntWrapper->touchData() = gdaSpecificProp->Value[0];
                }
            }
        }
        break;
        case engine::WrapperType::FLOAT:
        {
            if (gdaProp->IsA(UGDAFloatProperty::StaticClass()))
            {
                const UGDAFloatProperty* gdaSpecificProp = static_cast<const UGDAFloatProperty*>(gdaProp);
                if (gdaSpecificProp->Value.Num() > 0)
                {
                    engine::SpecificWrapper<float>* objectAsFloatWrapper = static_cast<engine::SpecificWrapper<float>*>(wrapper);
                    const float& unrealValue = gdaSpecificProp->Value[0];
                    AGolaemSimulation::unrealToGda(
                        unrealValue,
                        objectAsFloatWrapper->touchData(),
                        _unrealActor->_crowdUnitFactor,
                        gdaSpecificProp->DataTypeId.GetValue());
                }
            }
        }
        break;
        case engine::WrapperType::DOUBLE:
        {
            if (gdaProp->IsA(UGDAFloatProperty::StaticClass()))
            {
                const UGDAFloatProperty* gdaSpecificProp = static_cast<const UGDAFloatProperty*>(gdaProp);
                if (gdaSpecificProp->Value.Num() > 0)
                {
                    engine::SpecificWrapper<double>* objectAsDoubleWrapper = static_cast<engine::SpecificWrapper<double>*>(wrapper);
                    const float& unrealValue = gdaSpecificProp->Value[0];
                    AGolaemSimulation::unrealToGda(
                        unrealValue,
                        objectAsDoubleWrapper->touchData(),
                        _unrealActor->_crowdUnitFactor,
                        gdaSpecificProp->DataTypeId.GetValue());
                }
            }
        }
        break;
        case engine::WrapperType::VECTOR3:
        {
            if (gdaProp->IsA(UGDAVector3Property::StaticClass()))
            {
                const UGDAVector3Property* gdaSpecificProp = static_cast<const UGDAVector3Property*>(gdaProp);
                if (gdaSpecificProp->Value.Num() > 0)
                {
                    engine::SpecificWrapper<Vector3>* objectAsVector3Wrapper = static_cast<engine::SpecificWrapper<Vector3>*>(wrapper);
                    const FVector& unrealValue = gdaSpecificProp->Value[0];
                    AGolaemSimulation::unrealToGda(
                        unrealValue,
                        objectAsVector3Wrapper->touchData(),
                        _unrealActor->_crowdUnitFactor,
                        gdaSpecificProp->DataTypeId.GetValue());
                }
            }
        }
        break;
        case engine::WrapperType::ARRAY_INTEGER:
        {
            if (gdaProp->IsA(UGDAInt32Property::StaticClass()))
            {
                const UGDAInt32Property* gdaSpecificProp = static_cast<const UGDAInt32Property*>(gdaProp);

                engine::SpecificWrapper<PODArray<int>>* objectAsArrayIntWrapper = static_cast<engine::SpecificWrapper<PODArray<int>>*>(wrapper);
                PODArray<int>& arrayValue = objectAsArrayIntWrapper->touchData();
                arrayValue.resize(gdaSpecificProp->Value.Num());
                for (int iElem = 0, arraySize = gdaSpecificProp->Value.Num(); iElem < arraySize; ++iElem)
                {
                    arrayValue[iElem] = gdaSpecificProp->Value[iElem];
                }
            }
            else if (gdaProp->IsA(UGDAInt64Property::StaticClass()))
            {
                const UGDAInt64Property* gdaSpecificProp = static_cast<const UGDAInt64Property*>(gdaProp);

                engine::SpecificWrapper<PODArray<int>>* objectAsArrayIntWrapper = static_cast<engine::SpecificWrapper<PODArray<int>>*>(wrapper);
                PODArray<int>& arrayValue = objectAsArrayIntWrapper->touchData();
                arrayValue.resize(gdaSpecificProp->Value.Num());
                for (int iElem = 0, arraySize = gdaSpecificProp->Value.Num(); iElem < arraySize; ++iElem)
                {
                    arrayValue[iElem] = gdaSpecificProp->Value[iElem];
                }
            }
        }
        break;
        case engine::WrapperType::ARRAY_FLOAT:
        {
            if (gdaProp->IsA(UGDAFloatProperty::StaticClass()))
            {
                const UGDAFloatProperty* gdaSpecificProp = static_cast<const UGDAFloatProperty*>(gdaProp);

                engine::SpecificWrapper<PODArray<float>>* objectAsArrayFloatWrapper = static_cast<engine::SpecificWrapper<PODArray<float>>*>(wrapper);
                PODArray<float>& arrayValue = objectAsArrayFloatWrapper->touchData();
                arrayValue.resize(gdaSpecificProp->Value.Num());
                for (int iElem = 0, arraySize = gdaSpecificProp->Value.Num(); iElem < arraySize; ++iElem)
                {
                    const float& unrealValue = gdaSpecificProp->Value[iElem];
                    AGolaemSimulation::unrealToGda(
                        unrealValue,
                        arrayValue[iElem],
                        _unrealActor->_crowdUnitFactor,
                        gdaSpecificProp->DataTypeId.GetValue());
                }
            }
        }
        break;
        case engine::WrapperType::ARRAY_DOUBLE:
        {
            if (gdaProp->IsA(UGDAFloatProperty::StaticClass()))
            {
                const UGDAFloatProperty* gdaSpecificProp = static_cast<const UGDAFloatProperty*>(gdaProp);

                engine::SpecificWrapper<PODArray<double>>* objectAsArrayDoubleWrapper = static_cast<engine::SpecificWrapper<PODArray<double>>*>(wrapper);
                PODArray<double>& arrayValue = objectAsArrayDoubleWrapper->touchData();
                arrayValue.resize(gdaSpecificProp->Value.Num());
                for (int iElem = 0, arraySize = gdaSpecificProp->Value.Num(); iElem < arraySize; ++iElem)
                {
                    const float& unrealValue = gdaSpecificProp->Value[iElem];
                    AGolaemSimulation::unrealToGda(
                        unrealValue,
                        arrayValue[iElem],
                        _unrealActor->_crowdUnitFactor,
                        gdaSpecificProp->DataTypeId.GetValue());
                }
            }
        }
        break;
        case engine::WrapperType::ARRAY_VECTOR3:
        {
            if (gdaProp->IsA(UGDAVector3Property::StaticClass()))
            {
                const UGDAVector3Property* gdaSpecificProp = static_cast<const UGDAVector3Property*>(gdaProp);

                engine::SpecificWrapper<Array<Vector3>>* objectAsArrayVector3Wrapper = static_cast<engine::SpecificWrapper<Array<Vector3>>*>(wrapper);
                Array<Vector3>& arrayValue = objectAsArrayVector3Wrapper->touchData();
                arrayValue.resize(gdaSpecificProp->Value.Num());
                for (int iElem = 0, arraySize = gdaSpecificProp->Value.Num(); iElem < arraySize; ++iElem)
                {
                    const FVector& unrealValue = gdaSpecificProp->Value[iElem];
                    AGolaemSimulation::unrealToGda(
                        unrealValue,
                        arrayValue[iElem],
                        _unrealActor->_crowdUnitFactor,
                        gdaSpecificProp->DataTypeId.GetValue());
                }
            }
        }
        break;
        case engine::WrapperType::MESH:
        {
            if (gdaProp->IsA(UGDAMeshProperty::StaticClass()))
            {
                const UGDAMeshProperty* gdaSpecificProp = static_cast<const UGDAMeshProperty*>(gdaProp);
                if (gdaSpecificProp->Value.Num() > 0)
                {
                    AStaticMeshActor* meshActor = gdaSpecificProp->Value[0];
                    engine::SpecificWrapper<engine::GtgPinMesh::SP>* objectAsMeshWrapper = static_cast<engine::SpecificWrapper<engine::GtgPinMesh::SP>*>(wrapper);
                    engine::GtgPinMesh::SP pinMesh = objectAsMeshWrapper->touchData();
                    if (pinMesh == NULL)
                    {
                        pinMesh = new engine::GtgPinMesh();
                        objectAsMeshWrapper->touchData() = pinMesh;
                    }
                    updatePinMeshFromMeshActor(pinMesh, meshActor);
                }
            }
        }
        break;
        case engine::WrapperType::ARRAY_MESH:
        {
            if (gdaProp->IsA(UGDAMeshProperty::StaticClass()))
            {
                const UGDAMeshProperty* gdaSpecificProp = static_cast<const UGDAMeshProperty*>(gdaProp);
                engine::SpecificWrapper<Array<engine::GtgPinMesh::SP>>* objectAsMeshArrayWrapper = static_cast<engine::SpecificWrapper<Array<engine::GtgPinMesh::SP>>*>(wrapper);
                Array<engine::GtgPinMesh::SP>& terrainMeshArray = objectAsMeshArrayWrapper->touchData();
                terrainMeshArray.resize(gdaSpecificProp->Value.Num());

                for (int32 iMesh = 0, meshCount = gdaSpecificProp->Value.Num(); iMesh < meshCount; ++iMesh)
                {
                    AStaticMeshActor* meshActor = gdaSpecificProp->Value[iMesh];
                    engine::GtgPinMesh::SP pinMesh = terrainMeshArray[iMesh];
                    if (pinMesh == NULL)
                    {
                        pinMesh = new engine::GtgPinMesh();
                        terrainMeshArray[iMesh] = pinMesh;
                    }
                    updatePinMeshFromMeshActor(pinMesh, meshActor);
                }
            }
        }
        break;
        case engine::WrapperType::STRING:
        {
            if (gdaProp->IsA(UGDAStringProperty::StaticClass()))
            {
                const UGDAStringProperty* gdaSpecificProp = static_cast<const UGDAStringProperty*>(gdaProp);
                if (gdaSpecificProp->Value.Num() > 0)
                {
                    engine::SpecificWrapper<GlmString>* objectAsGlmStringWrapper = static_cast<engine::SpecificWrapper<GlmString>*>(wrapper);
                    const FString& unrealValue = gdaSpecificProp->Value[0];
                    objectAsGlmStringWrapper->touchData() = TCHAR_TO_ANSI(*unrealValue);
                }
            }
        }
        break;
        default:
            break;
        }
    }

    //-------------------------------------------------------------------------
    void UnrealObjectsWrapper::sanitizeName(GlmString& name)
    {
        name.replaceAll('.', '_');
        name.replaceAll(':', '_');
    }

} // namespace glm
