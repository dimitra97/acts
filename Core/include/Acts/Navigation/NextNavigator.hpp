// This file is part of the Acts project.
//
// Copyright (C) 2023 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Acts/Definitions/Units.hpp"
#include "Acts/Detector/Detector.hpp"
#include "Acts/Detector/DetectorVolume.hpp"
#include "Acts/Detector/Portal.hpp"
#include "Acts/Geometry/BoundarySurfaceT.hpp"
#include "Acts/Geometry/GeometryIdentifier.hpp"
#include "Acts/Geometry/Layer.hpp"
#include "Acts/Navigation/NavigationState.hpp"
#include "Acts/Propagator/ConstrainedStep.hpp"
#include "Acts/Propagator/Propagator.hpp"
#include "Acts/Surfaces/Surface.hpp"
#include "Acts/Utilities/Logger.hpp"

#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>

#include <boost/algorithm/string.hpp>
#include <boost/container/small_vector.hpp>

namespace Acts {

namespace Experimental {

class NextNavigator {
 public:
  struct Config {
    /// Detector for this Navigation
    const Detector* detector = nullptr;

    /// Configuration for this Navigator
    /// stop at every sensitive surface (whether it has material or not)
    bool resolveSensitive = true;
    /// stop at every material surface (whether it is passive or not)
    bool resolveMaterial = true;
    /// stop at every surface regardless what it is
    bool resolvePassive = false;

    /// The tolerance used to defined "reached"
    double tolerance = s_onSurfaceTolerance;
  };

  /// Nested State struct
  ///
  /// It acts as an internal state which is
  /// created for every propagation/extrapolation step
  /// and keep thread-local navigation information
  struct State : public NavigationState {
    /// Navigation state - external state: the start surface
    const Surface* startSurface = nullptr;
    /// Navigation state - external state: the current surface
    const Surface* currentSurface = nullptr;
    /// Navigation state - external state: the target surface
    const Surface* targetSurface = nullptr;
    /// Indicator if the target is reached
    bool targetReached = false;
    /// Navigation state : a break has been detected
    bool navigationBreak = false;

    void reset(const GeometryContext& /*geoContext*/, const Vector3& /*pos*/,
               const Vector3& /*dir*/, NavigationDirection /*navDir*/,
               const Surface* /*ssurface*/, const Surface* /*tsurface*/) {
      // Reset everything first
      *this = State();

      // TODO do stuff?
    }
  };

  /// Constructor with configuration object
  ///
  /// @param cfg The navigator configuration
  /// @param _logger a logger instance
  explicit NextNavigator(Config cfg, std::shared_ptr<const Logger> _logger =
                                         getDefaultLogger("NextNavigator",
                                                          Logging::Level::INFO))
      : m_cfg{std::move(cfg)}, m_logger{std::move(_logger)} {}

  /// @brief Navigator status call
  ///
  /// @tparam propagator_state_t is the type of Propagatgor state
  /// @tparam stepper_t is the used type of the Stepper by the Propagator
  ///
  /// @param [in,out] state is the mutable propagator state object
  /// @param [in] stepper Stepper in use
  template <typename propagator_state_t, typename stepper_t>
  void status(propagator_state_t& state, const stepper_t& stepper) const {
    ACTS_VERBOSE(volInfo(state)
                 << posInfo(state, stepper) << "Entering navigator::status.");

    auto& nState = state.navigation;
    fillNavigationState(state, stepper, nState);

    if (inactive(state, stepper)) {
      ACTS_VERBOSE("navigator inactive");
      return;
    }

    if (nState.currentDetector == nullptr) {
      initialize(state, stepper);
      return;
    }

    if (nState.surfaceCandidate == nState.surfaceCandidates.end()) {
      ACTS_VERBOSE("no surface candidates - waiting for target call");
      return;
    }

    const Portal* nextPortal = nullptr;
    const Surface* nextSurface = nullptr;
    bool isPortal = false;
    bool boundaryCheck = nState.surfaceCandidate->boundaryCheck;

    if (nState.surfaceCandidate->surface != nullptr) {
      nextSurface = nState.surfaceCandidate->surface;
    } else if (nState.surfaceCandidate->portal != nullptr) {
      nextPortal = nState.surfaceCandidate->portal;
      nextSurface = &nextPortal->surface();
      isPortal = true;
    } else {
      ACTS_ERROR("panic: not a surface not a portal - what is it?");
      return;
    }

    // TODO not sure about the boundary check
    auto surfaceStatus = stepper.updateSurfaceStatus(
        state.stepping, *nextSurface, boundaryCheck, logger());

    // Check if we are at a surface
    if (surfaceStatus == Intersection3D::Status::onSurface) {
      ACTS_VERBOSE("landed on surface");

      if (isPortal) {
        ACTS_VERBOSE("this is a portal, storing it.");

        nState.currentPortal = nextPortal;

        ACTS_VERBOSE("current portal set to "
                     << nState.currentPortal->surface().geometryId());
      } else {
        ACTS_VERBOSE("this is a surface, storing it.");

        // If we are on the surface pointed at by the iterator, we can make
        // it the current one to pass it to the other actors
        nState.currentSurface = nextSurface;
        ACTS_VERBOSE("current surface set to "
                     << nState.currentSurface->geometryId());
        ++nState.surfaceCandidate;
      }
    }
  }

  /// @brief Navigator target call
  ///
  /// @tparam propagator_state_t is the type of Propagatgor state
  /// @tparam stepper_t is the used type of the Stepper by the Propagator
  ///
  /// @param [in,out] state is the mutable propagator state object
  /// @param [in] stepper Stepper in use
  template <typename propagator_state_t, typename stepper_t>
  void target(propagator_state_t& state, const stepper_t& stepper) const {
    ACTS_VERBOSE(volInfo(state)
                 << posInfo(state, stepper) << "Entering navigator::target.");

    auto& nState = state.navigation;

    if (inactive(state, stepper)) {
      ACTS_VERBOSE("navigator inactive");
      return;
    }

    if (nState.currentVolume == nullptr) {
      initializeTarget(state, stepper);
    }

    if (nState.currentSurface != nullptr) {
      ACTS_VERBOSE("stepping through surface");
    } else if (nState.currentPortal != nullptr) {
      ACTS_VERBOSE("stepping through portal");

      nState.surfaceCandidates.clear();
      nState.surfaceCandidate = nState.surfaceCandidates.end();

      nState.currentPortal->updateDetectorVolume(state.geoContext, nState);

      initializeTarget(state, stepper);
    }

    for (; nState.surfaceCandidate != nState.surfaceCandidates.end();
         ++nState.surfaceCandidate) {
      // Screen output how much is left to try
      ACTS_VERBOSE(volInfo(state)
                   << std::distance(nState.surfaceCandidate,
                                    nState.surfaceCandidates.end())
                   << " out of " << nState.surfaceCandidates.size()
                   << " surfaces remain to try.");
      // Take the surface
      const auto& c = *(nState.surfaceCandidate);
      const auto& surface =
          (c.surface != nullptr) ? (*c.surface) : (c.portal->surface());
      // Screen output which surface you are on
      ACTS_VERBOSE("Next surface candidate will be " << surface.geometryId());
      // Estimate the surface status
      bool boundaryCheck = c.boundaryCheck;
      auto surfaceStatus = stepper.updateSurfaceStatus(state.stepping, surface,
                                                       boundaryCheck, logger());
      if (surfaceStatus == Intersection3D::Status::reachable) {
        ACTS_VERBOSE("Surface reachable, step size updated to "
                     << stepper.outputStepSize(state.stepping));
        break;
      }
    }

    nState.currentSurface = nullptr;
    nState.currentPortal = nullptr;
  }

 private:
  Config m_cfg;

  std::shared_ptr<const Logger> m_logger;

  template <typename propagator_state_t>
  std::string volInfo(const propagator_state_t& state) const {
    auto& nState = state.navigation;

    return (nState.currentVolume ? nState.currentVolume->name() : "No Volume") +
           " | ";
  }

  template <typename propagator_state_t, typename stepper_t>
  std::string posInfo(const propagator_state_t& state,
                      const stepper_t& stepper) const {
    std::stringstream ss;
    ss << stepper.position(state.stepping).transpose();
    ss << " | ";
    return ss.str();
  }

  const Logger& logger() const { return *m_logger; }

  /// This checks if a navigation break had been triggered or navigator
  /// is misconfigured
  ///
  /// boolean return triggers exit to stepper
  bool inactive() const {
    if (m_cfg.detector == nullptr) {
      return true;
    }

    if (!m_cfg.resolveSensitive && !m_cfg.resolveMaterial &&
        !m_cfg.resolvePassive) {
      return true;
    }

    return false;
  }

  /// Initialize call - start of propagation
  ///
  /// @tparam propagator_state_t The state type of the propagagor
  /// @tparam stepper_t The type of stepper used for the propagation
  ///
  /// @param [in,out] state is the propagation state object
  /// @param [in] stepper Stepper in use
  ///
  /// @return boolean return triggers exit to stepper
  template <typename propagator_state_t, typename stepper_t>
  void initialize(propagator_state_t& state, const stepper_t& stepper) const {
    ACTS_VERBOSE(volInfo(state) << posInfo(state, stepper) << "initialize");

    auto& nState = state.navigation;

    if (nState.currentDetector == nullptr) {
      ACTS_VERBOSE("assigning detector from the config.");
      nState.currentDetector = m_cfg.detector;
    }

    if (nState.currentDetector == nullptr) {
      ACTS_ERROR("panic: no detector");
      return;
    }
  }

  /// Navigation (re-)initialisation for the target
  ///
  /// This is only called a few times every propagation/extrapolation
  ///
  /// ---------------------------------------------------------------------
  ///
  /// As a straight line estimate can lead you to the wrong destination
  /// Volume, this will be called at:
  /// - initialization
  /// - attempted volume switch
  /// Target finding by association will not be done again
  ///
  /// @tparam propagator_state_t The state type of the propagagor
  /// @tparam stepper_t The type of stepper used for the propagation
  ///
  /// @param [in,out] state is the propagation state object
  /// @param [in] stepper Stepper in use
  ///
  /// boolean return triggers exit to stepper
  template <typename propagator_state_t, typename stepper_t>
  void initializeTarget(propagator_state_t& state,
                        const stepper_t& stepper) const {
    ACTS_VERBOSE(volInfo(state)
                 << posInfo(state, stepper) << "initialize target");

    auto& nState = state.navigation;

    if (nState.currentVolume == nullptr) {
      nState.currentVolume = nState.currentDetector->findDetectorVolume(
          state.geoContext, nState.position);

      if (nState.currentVolume != nullptr) {
        ACTS_VERBOSE(volInfo(state) << "switched detector volume");
      }
    }

    if (nState.currentVolume == nullptr) {
      ACTS_ERROR("panic: no current volume");
      return;
    }

    nState.currentVolume->updateNavigationState(state.geoContext, nState);
  }

  template <typename propagator_state_t, typename stepper_t>
  void fillNavigationState(propagator_state_t& state, const stepper_t& stepper,
                           NavigationState& nState) const {
    nState.position = stepper.position(state.stepping);
    nState.direction = stepper.direction(state.stepping);
    nState.absMomentum = stepper.momentum(state.stepping);
    nState.charge = stepper.charge(state.stepping);
    auto fieldResult = stepper.getField(state.stepping, nState.position);
    if (!fieldResult.ok()) {
      ACTS_ERROR("could not read from the magnetic field");
    }
    nState.magneticField = *fieldResult;
  }
};

}  // namespace Experimental

}  // namespace Acts
