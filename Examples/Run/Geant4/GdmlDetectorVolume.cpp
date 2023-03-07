

	

	#include "Acts/Detector/DetectorVolume.hpp"
	#include "Acts/Detector/PortalGenerators.hpp"
	#include "Acts/Detector/PortalHelper.hpp"
	#include "ActsExamples/Geant4/GdmlDetectorConstruction.hpp"
	#include "ActsExamples/Geant4Detector/Geant4Detector.hpp"
	#include "Acts/Plugins/Geant4/Geant4PhysicalVolumeSelectors.hpp"
	#include "Acts/Plugins/Geant4/Geant4DetectorSurfaceFactory.hpp"
	#include "Acts/Plugins/Geant4/Geant4Converters.hpp"
	#include "Acts/Plugins/Geant4/Geant4DetectorElement.hpp"
	#include "Acts/Surfaces/Surface.hpp"	
	#include "Acts/Geometry/CuboidVolumeBounds.hpp"
	#include "Acts/Navigation/DetectorVolumeUpdators.hpp"
	#include "Acts/Navigation/NavigationState.hpp"
	#include "Acts/Navigation/SurfaceCandidatesUpdators.hpp"


	#include "G4LogicalVolume.hh"
	#include "G4VPhysicalVolume.hh"


	#include<iostream>
	#include <exception>
	#include <memory>
	#include<string>

	using namespace Acts;
	using namespace ActsExamples;
	using namespace ActsExamples::Geant4;
	using namespace Acts::Experimental;

	class G4VSolid;

	using AllSurfacesProvider = Acts::Experimental::StaticUpdatorImpl<
    Acts::Experimental::AllSurfacesExtractor,
    Acts::Experimental::SurfacesFiller>;

using AllPortalsProvider = Acts::Experimental::StaticUpdatorImpl<
    Acts::Experimental::AllPortalsExtractor, Acts::Experimental::PortalsFiller>;


int main(){

	//std::tuple

	std::vector<std::string>SensitiveNames = {"Inner_Wire", "Middle_Wire", "Outer_Wire"};
	std::vector<std::string>PassiveNames = {"xx"};

	GdmlDetectorConstruction geo_gdml("/home/dimitra/ACTS_Dimitra/acts/Examples/Run/Geant4/MuonChamber.gdml");
	auto g4World = geo_gdml.Construct();

	auto g4World_detector = Geant4Detector();
	auto g4World_config = Geant4Detector::Config();
	g4World_config.name="MsSlice";  
	g4World_config.g4World = g4World;

	auto g4Sensitive = std::make_shared<Geant4PhysicalVolumeSelectors::NameSelector>(SensitiveNames);	
	auto g4Passive = std::make_shared<Geant4PhysicalVolumeSelectors::NameSelector>(PassiveNames);

	//std::cout<<typeid(g4World->GetLogicalVolume()->GetSolid()).name()  <<std::endl;

	auto g4SurfaceOptions = Geant4DetectorSurfaceFactory::Options();
	g4SurfaceOptions.sensitiveSurfaceSelector =  g4Sensitive;
	g4SurfaceOptions.passiveSurfaceSelector = g4Passive;

	g4World_config.g4SurfaceOptions = g4SurfaceOptions;

	auto [trackingGeometry, decorators, detectorElements] = g4World_detector.constructMsSlice(g4World_config);


	std::cout<<detectorElements.size()<<std::endl;
	std::cout<<typeid(detectorElements).name()<<std::endl;

	std::vector<std::shared_ptr<Acts::Surface>> straw_surfaces = {};
	std::vector<std::shared_ptr<Acts::Surface>> straw_surfaces_inner = {};
	std::vector<std::shared_ptr<Acts::Surface>> straw_surfaces_middle = {};
	std::vector<std::shared_ptr<Acts::Surface>> straw_surfaces_outer = {};
	std::vector<std::shared_ptr<Acts::Experimental::DetectorVolume>> detector_volumes= {};


	//create a detector volume to get all the tubes inside

	

	for(auto& detectorElement : detectorElements){

		//std::cout<<"i="<<&i<<std::endl;
		auto context = GeometryContext();
		auto g4conv = Geant4PhysicalVolumeConverter();
		g4conv.forcedType = Surface::SurfaceType::Straw;
		auto g4conv_surf = g4conv.Geant4PhysicalVolumeConverter::surface_conv(detectorElement->g4PhysicalVolume(), detectorElement, context, false, 0.);
		straw_surfaces.push_back(g4conv_surf);
		std::string volume_name = detectorElement->g4PhysicalVolume().GetName();
		std::cout<<volume_name[0]<<std::endl;
		if(volume_name[0] == 'I') straw_surfaces_inner.push_back(g4conv_surf);
		if(volume_name[0] == 'M') straw_surfaces_middle.push_back(g4conv_surf);
		if(volume_name[0] == 'O') straw_surfaces_outer.push_back(g4conv_surf);
		
		//std::cout<<detectorElement->g4PhysicalVolume().GetName()<<std::endl;
		//std::cout<<g4conv_surf->type()<<std::endl;
		
		//std::cout<<"straw surface's transform:"<<g4conv_surf->center(context)<<std::endl;

	}


	Transform3 nominal = Transform3::Identity();
	GeometryContext cxt;

	ActsScalar hx = 5000.;
	ActsScalar hy = 5000.;
	ActsScalar hz = 5000.;
	

	//auto portal = defaultPortalGenerator();
	auto generatePortalsUpdateInternals = defaultPortalAndSubPortalGenerator();
	auto detectorVolumeBounds = std::make_unique<CuboidVolumeBounds>(hx,hy,hz);

	auto detectorVolumeBounds_ch1 = std::make_unique<CuboidVolumeBounds>(hx/3,hy/3,hz/3);
	auto detectorVolumeBounds_ch2 = std::make_unique<CuboidVolumeBounds>(hx/3,hy/3,hz/3);
	auto detectorVolumeBounds_ch3 = std::make_unique<CuboidVolumeBounds>(hx/3,hy/3,hz/3);

	//auto detectorVolume = DetectorVolumeFactory::construct(portal, cxt, "MsSlice_Volume", nominal, std::move(detectorVolumeBounds), allPortals());


	auto detectorVolume = DetectorVolumeFactory::construct(generatePortalsUpdateInternals, cxt, "DetectorVolume_MsSlice", nominal,  std::move(detectorVolumeBounds), 
		straw_surfaces, detector_volumes, allPortalsAndSurfaces());

	auto detectorVolume_inner = DetectorVolumeFactory::construct(generatePortalsUpdateInternals, cxt, "DetectorVolume_InnerChamber", nominal,  std::move(detectorVolumeBounds_ch1), 
		straw_surfaces_inner, detector_volumes, allPortalsAndSurfaces());

	auto detectorVolume_middle = DetectorVolumeFactory::construct(generatePortalsUpdateInternals, cxt, "DetectorVolume_MiddleChamber", nominal,  std::move(detectorVolumeBounds_ch2), 
		straw_surfaces_middle, detector_volumes, allPortalsAndSurfaces());

	auto detectorVolume_outer = DetectorVolumeFactory::construct(generatePortalsUpdateInternals, cxt, "DetectorVolume_OuterChamber", nominal,  std::move(detectorVolumeBounds_ch3), 
		straw_surfaces_outer, detector_volumes, allPortalsAndSurfaces());

	

	std::cout<<"the surfaces of the detector volume="<<detectorVolume->surfaces().size()<<std::endl;
	std::cout<<"the surfaces of the inner detector volume="<<detectorVolume_inner->surfaces().size()<<std::endl;
	std::cout<<"the surfaces of the middle detector volume="<<detectorVolume_middle->surfaces().size()<<std::endl;
	std::cout<<"the surfaces of the outer detector volume="<<detectorVolume_outer->surfaces().size()<<std::endl;
	
	
	

	NavigationState nState;
	nState.currentVolume= detectorVolume.get(); 
	std::cout<<"nstate surfacecandidates before update:"<<nState.surfaceCandidates.size()<<std::endl;
	AllPortalsProvider allPortals;
	AllSurfacesProvider allSurfaces;
	auto allPortalsAllSurfaces =
	 ChainedUpdatorImpl<AllPortalsProvider,AllSurfacesProvider>(std::tie(allPortals, allSurfaces));

      allPortalsAllSurfaces.update(cxt, nState);	
	

	std::cout<<"nstate surface candidated="<<nState.currentVolume<<std::endl; 

	std::cout<<"nstate surface candidates after the update="<<nState.surfaceCandidates.size()<<std::endl; 


	return 0;

	}