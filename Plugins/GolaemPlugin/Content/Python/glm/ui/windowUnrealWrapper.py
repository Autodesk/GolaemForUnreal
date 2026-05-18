#**************************************************************************
#*                                                                        *
#*  Copyright (C) Golaem S.A. - All Rights Reserved.                      *
#*                                                                        *
#**************************************************************************

import os
import unreal
from glm.ui import windowWrapper


#**********************************************************************
#
# WindowUnrealWrapper
# Unreal wrapper for golaem window tools
#
#**********************************************************************
class WindowUnrealWrapper(windowWrapper.WindowWrapper):   

    #******************************************************************
    # Select
    #******************************************************************

    #------------------------------------------------------------------
    # selects a node
    #------------------------------------------------------------------
    def selectNode(self, nodeToSelect):
        pass

    #------------------------------------------------------------------
    # returns all selected objects of type
    #------------------------------------------------------------------
    def getSelectedObjectsOfType(self, objectType):
        objectsOfType = []
        allActors = unreal.EditorLevelLibrary.get_all_level_actors()
        allActors = unreal.EditorFilterLibrary.by_selection(allActors)
        for actor in allActors:
            if (actor.get_class().get_name() == objectType):
                objectsOfType.append(actor.get_name())
        return objectsOfType

    #------------------------------------------------------------------
    # returns all objects of type
    #------------------------------------------------------------------
    def getObjectsOfType(self, objectType):
        objectsOfType = []
        allActors = unreal.EditorLevelLibrary.get_all_level_actors()
        for actor in allActors:
            if (actor.get_class().get_name() == objectType):
                objectsOfType.append(actor.get_name())
        return objectsOfType

    #******************************************************************
    # UI
    #******************************************************************

    #------------------------------------------------------------------
    # log a message (level : info / warning / error)
    #------------------------------------------------------------------
    def log(self, logLevel, message):
        if logLevel == 'info':
            unreal.log(message)
        elif logLevel == 'warning':
            unreal.log_warning(message)
        elif logLevel == 'error':
            unreal.log_error(message)

    #------------------------------------------------------------------
    # returns the directory with the icons
    #------------------------------------------------------------------
    def getIconsDir(self):
        thisDirectory = os.path.dirname(os.path.realpath(__file__))
        return (thisDirectory + '/../../../../Resources/Icons/')

