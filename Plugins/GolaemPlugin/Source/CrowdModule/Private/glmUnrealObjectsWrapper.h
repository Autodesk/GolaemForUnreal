/***************************************************************************
*                                                                          *
*  Copyright (C) 2014 Golaem S.A. All Rights Reserved.                     *
*                                                                          *
***************************************************************************/

#pragma once

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif
#include "Runtime/Launch/Resources/Version.h"

#include "glmAbstractObjectsWrapper.h"
#include <Math/Matrix.h>

THIRD_PARTY_INCLUDES_START
#include <glmString.h>
#include <glmMutex.h>
#include <glmGtgPinMesh.h>
THIRD_PARTY_INCLUDES_END


class AGolaemSimulation;
class UGDAProperty;
class AStaticMeshActor;

namespace glm
{
    namespace engine
    {
        class Engine;
    }

    //-------------------------------------------------------------------------
    class UnrealObjectsWrapper : public engine::AbstractObjectsWrapper
    {
    public:
        static FMatrix44f MatUEToGda;

    protected: //data
        engine::Engine* _engine = NULL;
        AGolaemSimulation* _unrealActor = NULL;
        Time _currentTime = 0;

        TMap<FName, UGDAProperty*> _foundProperties;

    private:
        glm::SpinLock _propertiesLock;

    public: //methods
        UnrealObjectsWrapper(engine::Engine* engine, AGolaemSimulation* unrealActor);
        virtual ~UnrealObjectsWrapper();

        void update(Time dt);
        void fetchData(engine::AbstractWrapper* wrapper) override;

        static void sanitizeName(GlmString& name);

    private:
        void updatePinMeshFromMeshActor(engine::GtgPinMesh::SP pinMesh, AStaticMeshActor* meshActor);
    };

} // namespace glm
