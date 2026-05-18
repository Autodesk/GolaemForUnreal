#**************************************************************************
#*                                                                        *
#*  Copyright (C) Golaem S.A. - All Rights Reserved.                      *
#*                                                                        *
#**************************************************************************

from glm.Qtpy.Qt import QtCore
from glm.ui import windowUnrealLauncher
from glm.layout import layoutEditorUtils
from glm.layout import layoutEditorWrapper


#**********************************************************************
#
# WindowUnrealWrapper
# Unreal wrapper for golaem window tools
#
#**********************************************************************
#class LayoutEditorUnrealWrapper(LayoutEditorWrapper.LayoutEditorWrapper):   

    # def path_leaf(self, path):
    #     head, tail = ntpath.split(path)
    #     return tail or ntpath.basename(head)

def checkLayoutEditor(golaemUEDir=""):
    layoutWrapper = layoutEditorWrapper.getTheLayoutEditorWrapperInstance(False)
    if (layoutWrapper is None):
        windowUnrealLauncher.LayoutEditorWindowMain(golaemUEDir)

    # force creation of layout editor instance        
    layoutEditorUtils.getTheLayoutEditorInstance(wrapper=layoutWrapper)

# meant for alternative dcc who loads layout files from their gscl file configuration setting
def openLayoutFileFromPath(filePath, newTabName, focus=True):
    layoutWrapper = layoutEditorWrapper.getTheLayoutEditorWrapperInstance(False)
    if layoutWrapper is None:
        return

    layoutEditor = layoutEditorUtils.getTheLayoutEditorInstance(wrapper=layoutWrapper)
    if layoutEditor is None:
        return

    # Insure that layout is visible
    if (focus):        
        layoutEditor.show()
        layoutEditor.setWindowState(
            layoutEditor.windowState() & ~QtCore.Qt.WindowMinimized | QtCore.Qt.WindowActive
        )
        layoutEditor.activateWindow()
        layoutEditor.raise_()
        if layoutEditor.isMinimized():
            layoutEditor.showNormal()

    if (focus and not layoutEditor.isVisible()):
        layoutEditor.show() # don't change its status if minimized, just insure that is it accessible to user

    # open or focus the layout file
    layoutWrapper.openLayoutFileFromPath(filePath, newTabName, True) #True : always focus the relative tab
