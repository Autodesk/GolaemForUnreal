from __future__ import absolute_import
# **************************************************************************
# *                                                                        *
# *  Copyright (C) Golaem S.A. - All Rights Reserved.                      *
# *                                                                        *
# **************************************************************************

from ..simCacheLib import simCacheLibWindow as scl
from ..simCacheLib import simCacheLibWindowUnrealWrapper as sclw
from ..layout import layoutEditorUtils
from ..layout import layoutEditorWrapper
from ..Qtpy.Qt import QtCore, QtWidgets
import unreal
import sys
import os

# **********************************************************************
#
# Launchers
#
# **********************************************************************

glmSimCacheLibWindowUIs = []
# ------------------------------------------------------------------
# SimCacheLibWindowMain
# ------------------------------------------------------------------
def SimCacheLibWindowMain():
    global glmSimCacheLibWindowUIs
    libUI = None
    if not QtWidgets.QApplication.instance():
        unreal.log_error("No QApplication instance found. The Simulation Cache Library window cannot be displayed.")
        return None
    if len(glmSimCacheLibWindowUIs):
        libUI = glmSimCacheLibWindowUIs[0]
    else:
        libUI = scl.SimCacheLibWindow(sclw.SimCacheLibWindowUnrealWrapper())
        glmSimCacheLibWindowUIs.append(libUI)
    libUI.show()
    libUI.setWindowState(libUI.windowState() & ~QtCore.Qt.WindowMinimized | QtCore.Qt.WindowActive)
    libUI.activateWindow()
    return libUI


# ------------------------------------------------------------------
# AboutWindowMain
# ------------------------------------------------------------------
def AboutWindowMain(golaemVersion=""):
    try:
        from . import golaemAboutWindowUnreal as abtUnreal
    except ModuleNotFoundError:
        unreal.log_warning("This is a Golaem for Unreal standalone build, the about window is not available.")
        return None
    except ImportError as e:
        unreal.log_error(f"Error importing about window: {e}")
        return None
    if not QtWidgets.QApplication.instance():
        unreal.log_error("No QApplication instance found. The About window cannot be displayed.")
        return None
    abtUI = abtUnreal.GolaemAboutWindowUnreal(golaemVersion=golaemVersion, baseDir=None)
    abtUI.show()
    abtUI.setWindowState(abtUI.windowState() & ~QtCore.Qt.WindowMinimized | QtCore.Qt.WindowActive)
    abtUI.activateWindow()
    return abtUI


# ------------------------------------------------------------------
# LayoutEditorWindowMain
# ------------------------------------------------------------------
def LayoutEditorWindowMain(golaemUEDir=""):
    if not QtWidgets.QApplication.instance():
        unreal.log_error("No QApplication instance found. The Layout Editor window cannot be displayed.")
        return None

    layoutIconsDir = os.path.join(golaemUEDir, "Resources", "Icons", "layoutToolv7").replace("\\", "/")
    layoutWrapper = layoutEditorWrapper.getTheLayoutEditorWrapperInstance()
    layoutWrapper._iconsDir = layoutIconsDir
    layoutEditor = layoutEditorUtils.getTheLayoutEditorInstance(wrapper=layoutWrapper)
    layoutEditor.show()
    layoutEditor.setWindowState(
        layoutEditor.windowState() & ~QtCore.Qt.WindowMinimized | QtCore.Qt.WindowActive
    )
    layoutEditor.activateWindow()
    return layoutEditor

