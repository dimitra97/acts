import os

import acts
import acts.examples.geant4 as actsG4

def buildMockupDetector(nSectors=1):

    mockupConfig = actsG4.MockupSectorBuilder.Config()

    mockupChamberConfigInner = actsG4.MockupSectorBuilder.ChamberConfig()
    mockupChamberConfigInner.name = "Inner_Detector_Chamber"
    mockupChamberConfigInner.SensitiveNames = ["Inner_Skin"]
    mockupChamberConfigInner.PassiveNames = ["xx"]

    mockupChamberConfigMiddle = actsG4.MockupSectorBuilder.ChamberConfig()
    mockupChamberConfigMiddle.name = "Middle_Detector_Chamber"
    mockupChamberConfigMiddle.SensitiveNames = ["Middle_Skin"]
    mockupChamberConfigMiddle.PassiveNames = ["xx"]

    mockupChamberConfigOuter = actsG4.MockupSectorBuilder.ChamberConfig()
    mockupChamberConfigOuter.name = "Outer_Detector_Chamber"
    mockupChamberConfigOuter.SensitiveNames = ["Outer_Skin"]
    mockupChamberConfigOuter.PassiveNames = ["xx"]

    dirOfThisScript = os.path.dirname(__file__)
    mockupConfig.gdmlPath = os.path.join(
        dirOfThisScript,
        "../../../../Detectors/MuonSpectrometerMockupDetector/MuonChamber.gdml",
    )
    mockupConfig.NumberOfSectors = nSectors

    mockupBuilder = actsG4.MockupSectorBuilder(mockupConfig)

    detectorVolumeInner = mockupBuilder.buildChamber(mockupChamberConfigInner)

    detectorVolumeOuter = mockupBuilder.buildChamber(mockupChamberConfigOuter)

    detectorVolumeMiddle = mockupBuilder.buildChamber(mockupChamberConfigMiddle)

    detectorVolumes = [detectorVolumeInner, detectorVolumeMiddle, detectorVolumeOuter]

    detectorVolumeSector = mockupBuilder.buildSector(detectorVolumes)

    actsG4.MockupSectorBuilder.drawSector(detectorVolumeSector,"sector_drawn_from_python")

    detector = mockupBuilder.buildMuonDetector([detectorVolumeSector])

    return detector

if "__main__" == __name__:

    #detVolume = buildMockupDetector(8)

    detector = buildMockupDetector(8)
    print("done")

    #actsG4.MockupSectorBuilder.drawSector(detVolume,"sector_drawn_from_python")
