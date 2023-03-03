from pathlib import Path
import math
import inspect

import acts
import acts.examples.geant4 as actsG4
from acts import GeometryContext
from acts import NavigationState
u = acts.UnitConstants


def buildGDMLGeometry(
        gdmlFile,
        sensitiveTags,
        passiveTags
             
):
    geo_gdml = actsG4.GdmlDetectorConstruction(gdmlFile)
    g4World=geo_gdml.Construct()
    
    g4World_config = actsG4.Geant4Detector.Config()
    g4World_config.name = "MsSlice"
    g4World_config.g4World = g4World
    
    g4SensitiveSelector = actsG4.VolumeNameSelector(sensitiveTags, False)
    g4PassiveSelector = actsG4.VolumeNameSelector(passiveTags, False)
    
    g4SurfaceOptions = actsG4.SurfaceFactoryOptions()
    
    g4SurfaceOptions.sensitiveSurfaceSelector = g4SensitiveSelector
    g4SurfaceOptions.passiveSurfaceSelector = g4PassiveSelector
    
    g4World_config.g4SurfaceOptions = g4SurfaceOptions
    g4World_config.logLevel=acts.logging.VERBOSE
    
    print(g4World_config.g4World)
    #return actsG4.Geant4Detector().constructTrackingGeometry(g4World_config)
    return actsG4.Geant4Detector().constructMsSlice(g4World_config)
    
   
    #print(g4World)


trackingGeometry, decorators, detectorElements=buildGDMLGeometry("MuonChamber.gdml",['Inner_Wire', 'Middle_Wire', 'Outer_Wire'], ['xx'])
type=acts.SurfaceType.Straw
print(type)
print(len(detectorElements))
print(detectorElements[1].surface().type())

nav_state=NavigationState()


for volume in range(len(detectorElements)):
	context=GeometryContext()	
	g4conv=actsG4.Geant4PhysicalVolumeConverter(acts.SurfaceType.Straw)
	g4conv_surface=g4conv.surface_conv(detectorElements[volume].g4PhysicalVolume(),detectorElements[volume], context, False, 0.)
	print(g4conv_surface.type())



    
    
    
                                   
