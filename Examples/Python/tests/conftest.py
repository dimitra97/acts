from pathlib import Path
import sys
import os
import tempfile


sys.path += [
    str(Path(__file__).parent.parent.parent.parent / "Examples/Scripts/Python/"),
    str(Path(__file__).parent),
]

import helpers

import pytest

import acts
import acts.examples

u = acts.UnitConstants


def kwargsConstructor(cls, *args, **kwargs):
    return cls(*args, **kwargs)


def configKwConstructor(cls, *args, **kwargs):
    assert hasattr(cls, "Config")
    _kwargs = {}
    if "level" in kwargs:
        _kwargs["level"] = kwargs.pop("level")
    config = cls.Config()
    for k, v in kwargs.items():
        setattr(config, k, v)
    return cls(*args, config=config, **_kwargs)


def configPosConstructor(cls, *args, **kwargs):
    assert hasattr(cls, "Config")
    _kwargs = {}
    if "level" in kwargs:
        _kwargs["level"] = kwargs.pop("level")
    config = cls.Config()
    for k, v in kwargs.items():
        setattr(config, k, v)

    return cls(config, *args, **_kwargs)


@pytest.fixture(params=[configPosConstructor, configKwConstructor, kwargsConstructor])
def conf_const(request):
    return request.param


@pytest.fixture
def rng():
    return acts.examples.RandomNumbers(seed=42)


@pytest.fixture
def basic_prop_seq(rng):
    def _basic_prop_seq_factory(geo, s=None):
        if s is None:
            s = acts.examples.Sequencer(events=10, numThreads=1)

        nav = acts.Navigator(trackingGeometry=geo)
        stepper = acts.StraightLineStepper()

        prop = acts.examples.ConcretePropagator(acts.Propagator(stepper, nav))
        alg = acts.examples.PropagationAlgorithm(
            propagatorImpl=prop,
            level=acts.logging.INFO,
            randomNumberSvc=rng,
            ntests=10,
            sterileLogger=False,
            propagationStepCollection="propagation-steps",
        )
        s.addAlgorithm(alg)
        return s, alg

    return _basic_prop_seq_factory


@pytest.fixture
def trk_geo(request):
    detector, geo, contextDecorators = acts.examples.GenericDetector.create()
    yield geo


@pytest.fixture
def ptcl_gun(rng):
    def _factory(s):
        evGen = acts.examples.EventGenerator(
            level=acts.logging.INFO,
            generators=[
                acts.examples.EventGenerator.Generator(
                    multiplicity=acts.examples.FixedMultiplicityGenerator(n=2),
                    vertex=acts.examples.GaussianVertexGenerator(
                        stddev=acts.Vector4(0, 0, 0, 0), mean=acts.Vector4(0, 0, 0, 0)
                    ),
                    particles=acts.examples.ParametricParticleGenerator(
                        p=(1 * u.GeV, 10 * u.GeV),
                        eta=(-2, 2),
                        phi=(0, 360 * u.degree),
                        randomizeCharge=True,
                        numParticles=2,
                    ),
                )
            ],
            outputParticles="particles_input",
            randomNumbers=rng,
        )

        s.addReader(evGen)

        return evGen

    return _factory


@pytest.fixture
def fatras(ptcl_gun, trk_geo, rng):
    def _factory(s):
        evGen = ptcl_gun(s)

        field = acts.ConstantBField(acts.Vector3(0, 0, 2 * acts.UnitConstants.T))
        simAlg = acts.examples.FatrasSimulation(
            level=acts.logging.INFO,
            inputParticles=evGen.config.outputParticles,
            outputParticlesInitial="particles_initial",
            outputParticlesFinal="particles_final",
            outputSimHits="simhits",
            randomNumbers=rng,
            trackingGeometry=trk_geo,
            magneticField=field,
            generateHitsOnSensitive=True,
        )

        s.addAlgorithm(simAlg)

        # Digitization
        digiCfg = acts.examples.DigitizationConfig(
            acts.examples.readDigiConfigFromJson(
                "Examples/Algorithms/Digitization/share/default-smearing-config-generic.json"
            ),
            trackingGeometry=trk_geo,
            randomNumbers=rng,
            inputSimHits=simAlg.config.outputSimHits,
        )
        digiAlg = acts.examples.DigitizationAlgorithm(digiCfg, acts.logging.INFO)

        s.addAlgorithm(digiAlg)

        return evGen, simAlg, digiAlg

    return _factory