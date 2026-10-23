# Secondary motion: source audit and replacement

## Why the first video was rejected

The initial `SpringRig` prototype is not accepted as a realistic simulation.
Its tests establish repeatability and bounded simple cases, not believable
hair or garment dynamics. The recorded video's 60 fps is its presentation
rate, not a measured real-time rendering rate.

Code inspection identified a concrete coordinate bug: three.js r160
`MMDLoader` changes PMX rigid-body positions from model space to bone-relative
offsets **in place**. The first exporter subtracted the bone position again,
misplacing the generated sphere colliders. It also discarded the source
collision groups, mass, shapes, joint frames, angular springs and the links
between skirt panels. An arbitrary cone limit and world-direction spring
cannot substitute for those authored constraints.

## Primary sources and what they establish

| Source | Relevant implementation | Application here |
| --- | --- | --- |
| [XPBD, Macklin et al. 2016](https://matthias-research.github.io/pages/publications/XPBD.pdf) | Compliance and accumulated constraint multipliers decouple stiffness from classic PBD iteration dependence | A basis for a new deformable solver, not a label for the old Verlet/cone code |
| [Small Steps, Macklin et al. 2019](https://matthias-research.github.io/pages/publications/smallsteps.pdf) | Splitting the visual step into smaller XPBD steps improves convergence in their experiments | Substeps need consistent moving-boundary sampling; increasing a constant does not repair bad collision coordinates |
| [UE hair physics overview](https://dev.epicgames.com/documentation/unreal-engine/hair-physics-in-unreal-engine---overview) | GPU XPBD, Cosserat rods or angular springs, guide strands, body contacts and voxel-based strand interactions | These are strand/rod models with constitutive behaviour, unlike independent tail springs |
| [Chaos panel cloth reference](https://dev.epicgames.com/community/learning/tutorials/eKwY/unreal-engine-panel-cloth-constraint-node-reference) | Separate cloth constraint controls, including XPBD stretch models | True garment cloth needs connected surface stretch/shear/bend constraints |
| [Unity Demo Team hair source](https://github.com/Unity-Technologies/com.unity.demoteam.hair) and [Time Ghost technical article](https://discussions.unity.com/t/making-of-time-ghost-evolving-the-hair-system/1598213) | Strand simulation, spatial clustering and simulation/rendering LOD | Asset representation and workload control matter as much as the integrator |
| [AMD TressFX shader source](https://github.com/GPUOpen-Effects/TressFX/blob/master/src/Shaders/TressFXSimulation.hlsl) | Local/global shape, length, velocity shock propagation and collision passes | Root-motion response and interaction cannot be replaced by arbitrary damping |
| [TressFX 5 guide](https://gpuopen.com/download/TressFX5.0_DeveloperGuide.pdf) | Documents guide/follow strands, simulation parameters and shock handling | Useful production reference, not evidence that these parameters fit the Miku mesh |
| [three.js r160 MMDPhysics](https://github.com/mrdoob/three.js/blob/r160/examples/jsm/animation/MMDPhysics.js) and [MMDLoader](https://github.com/mrdoob/three.js/blob/r160/examples/jsm/loaders/MMDLoader.js) | Full authored body shapes, mass/damping/filtering, six-DOF springs and body-to-bone modes | Directly matches the PMX asset already in this workspace |
| [Babylon MMD native Bullet documentation](https://noname0310.github.io/babylon-mmd/docs/reference/runtime/bullet-physics/) | Uses Bullet for runtime MMD simulation | Independent production-oriented reference for the asset-compatible baseline |
| [Bullet six-DOF spring source](https://github.com/bulletphysics/bullet3/blob/3.25/src/BulletDynamics/ConstraintSolver/btGeneric6DofSpringConstraint.cpp) | Native constraint implementation used by the replacement | Use the implementation rather than inventing an approximate spring solver |

## Implementation decision

For **this PMX mesh**, first restore the authored rigid-body model in native
Bullet 3.25. The asset was constructed for that representation. Switching its
parameters into an unrelated strand/cloth solver would require new simulation
geometry and material calibration, not just an algorithm name.

The replacement imports all 173 bodies and 229 constraints, using effective
three.js MMDLoader descriptors (including its documented mode-2 correction).
The exporter verifies that reconstructed body bind positions match original
PMX positions. Source body collision shapes, collision masks, mass, damping,
friction, restitution, joint frames, six limits and six spring coefficients
are preserved. The old sphere approximation and hardcoded cone are not used.

The world operates in the source MMD units, with gravity 98 units/s² and the
scene's existing 0.1 scale. This preserves authored dynamics; it does not claim
that PMX material parameters are calibrated SI properties. Kinematic targets
are interpolated across 120 Hz runtime substeps. A one-second settling phase
runs in memory on reset; it is not a precomputed animation or Blender bake.
Bullet has 20 solver iterations. Stop ERP 0.475 follows the referenced MMD
bridge. These choices must be reported alongside measurements.

This baseline can produce asset-appropriate motion but remains rigid proxies,
not a photorealistic continuum cloth or individual-fibre hair simulation.
A subsequent true cloth/strand implementation requires explicit simulation
topology, skin-to-simulation binding, stretch/shear/bend or rod material models,
and body/self-contact handling. Its quality must be evaluated separately.

## Acceptance evidence

- Check every body bind position against the original PMX, before simulation.
- Exercise rest settling, walking/dancing, sudden direction changes, pause,
  seek and reset. Inspect front/side views; do not accept a single attractive still.
- Keep animation/camera identical in comparisons. Label physical models.
- Measure physics+binding, complete scene update, and GPU-completed Viewer
  frames **without PNG encoding/readback**, reporting p50/p95/max and hardware.
- Record penetration diagnostics and constraint behaviour; do not equate
  deterministic replay or passing unit tests with realistic motion.
- Report initialization/settling cost separately from steady-state timing.
- Real-time target: 60 Hz (16.67 ms total frame budget) on the current station;
  report a miss instead of silently calling a fixed-rate export real time.
