#**************************************************************************
#*                                                                        *
#*  Copyright (C) Golaem S.A. - All Rights Reserved.                      *
#*                                                                        *
#**************************************************************************

from glm.ui import windowUnrealWrapper
import unreal


#**********************************************************************
#
# SimCacheLibWindowUnrealWrapper
# Unreal wrapper for SimCacheLibWindow
#
#**********************************************************************
class SimCacheLibWindowUnrealWrapper(windowUnrealWrapper.WindowUnrealWrapper):
    #******************************************************************
    # Specific
    #******************************************************************

    #------------------------------------------------------------------
    # Returns the app stylesheet
    #------------------------------------------------------------------
    def getStyleSheet(self):
        return 'QWidget#itemLib:hover { background-color: #597b9b ; }'

    #------------------------------------------------------------------
    # Updates the item snapshot and returns it
    #------------------------------------------------------------------
    def updateItemSnapshot(self, item):
        return item

    #------------------------------------------------------------------
    # Create a sim cache proxy node, fills it from item and returns it
    #------------------------------------------------------------------
    def createSimCacheProxyFromLibItem(self, item):
        if (item.isInitialized()):
            location = unreal.Vector()
            # create cache node
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.GolaemCache.static_class(), location)
            if actor is not None:
                # we also need to set editable properties one by one (so that they are not overriden when playing in PIE, which set cacheLibraryFile and cacheIndex again)
                actor.set_editor_property('bEnableLayout', item.enableLayout)
                actor.set_editor_property('layoutFile', item.layoutFile)
                actor.set_editor_property('terrainFileSource', unreal.FilePath(item.sourceTerrain)) # source terrain may need some unlock / lock to be set
                actor.set_editor_property('terrainFileDest', unreal.FilePath(item.destTerrain))
                actor.set_editor_property('currentFrame', item.startFrame + 1)
                actor.set_editor_property('startFrame', item.startFrame)
                actor.set_editor_property('endFrame', item.endFrame)
                actor.set_editor_property('charactersFiles',  item.characterFiles)
                actor.set_editor_property('cacheDirectory', item.cacheDir)
                actor.set_editor_property('cacheName', item.cacheName) # cache name last to have everything for init
                # actor.set_editor_property('crowdFields', ';'.join(item.crowdFields))
                actor.set_editor_property('crowdFields', item.crowdFields)
            return actor
        return None

    #------------------------------------------------------------------
    # Create a sim cache proxy node, fills it from item and returns it
    #------------------------------------------------------------------
    def createSimCacheProxyFromItem(self, lib, itemIdx):
        item = lib.getLibItemAt(itemIdx)
        return self.createSimCacheProxyFromLibItem(item)

    #------------------------------------------------------------------
    # Create sim cache proxy nodes, fill them from item and return them
    #------------------------------------------------------------------
    def createSimCacheProxiesFromItem(self, lib, itemIdx):
        actors = []
        item = lib.getLibItemAt(itemIdx)
        for crowdField in item.crowdFields:
            cfItem = item
            cfItem.crowdFields = [ crowdField ]
            actors.append(self.createSimCacheProxyFromLibItem(cfItem))
        return actors

    #------------------------------------------------------------------
    # Updates a sim cache lib from a set of nodes and returns it
    #------------------------------------------------------------------
    def fillSimCacheLibFromProxies(self, lib, nodes):
        return lib

    #------------------------------------------------------------------
    # Return true if a button is available is this interface
    #------------------------------------------------------------------
    def isButtonAvailable(self, buttonName):
        if buttonName == "Import from selected / scene Simulation Cache Proxy" or buttonName == "Update Thumbnail from Viewport":
            return False
        return True
