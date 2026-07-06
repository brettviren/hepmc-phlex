# hepmc-phlex

HepMC3 kinematics producers as [Phlex](https://github.com/Framework-R-D/phlex)
nodes.  Depends only on **phlex** and **HepMC3** (plus Boost, via phlex) — no
other domain packages, and in particular **no Geant4 / edep-sim**.

All code is in the `hepmc_phlex` C++ namespace.

## Provided

The inter-node kinematics product is **`HepMC3::GenEvent` itself**, emitted by
value.  Phlex moves the returned value into its own product store and hands
consumers a `HepMC3::GenEvent const&`, so no wrapper struct or `shared_ptr` is
needed (and downstream nodes need not depend on this package for the type).

- **`GenEventGun`** (`hepmc_phlex/GenEventGun.hpp`) — a HepMC3 analogue of
  Geant4's `G4ParticleGun`.  It fires a fixed number of identical particles from
  a single vertex and returns a `HepMC3::GenEvent` by value.  Registered as a Phlex
  **source (provider)** node in `modules/gen_event_gun.cpp` (module library
  `hmp_gen_event_gun`, i.e. `cpp: 'hmp_gen_event_gun'` in a workflow), producing
  one `GenEvent` per data cell.

## `GenEventGun` configuration schema

The gun mirrors `G4ParticleGun`'s setters.  Configuration is **parsed and
validated in the constructor** — an invalid configuration throws before any event
is produced.

### Module keys (consumed by the module wrapper)

| key             | type   | required | default      | meaning                                  |
|-----------------|--------|----------|--------------|------------------------------------------|
| `output_layer`  | string | yes      | —            | Phlex layer for the emitted product      |
| `output_suffix` | string | no       | `"genevent"` | product suffix                           |

### Gun keys (consumed by `GenEventGun`)

| key             | type          | required | default   | meaning                                                             |
|-----------------|---------------|----------|-----------|---------------------------------------------------------------------|
| `pdg`           | int           | **yes**  | —         | PDG code of the particle to fire                                    |
| `mass`          | double        | no       | `0.0`     | rest mass (in `momentum_unit`); needed for correct energy↔momentum  |
| `number`        | int           | no       | `1`       | identical particles fired from the vertex (`SetNumberOfParticles`)  |
| `position`      | [x,y,z]       | no       | `[0,0,0]` | vertex position (in `length_unit`)                                  |
| `time`          | double        | no       | `0.0`     | vertex time as the HepMC position 4-vector `t` (c·t, `length_unit`) |
| `momentum_unit` | string        | no       | `"MeV"`   | `"MeV"` or `"GeV"`                                                   |
| `length_unit`   | string        | no       | `"mm"`    | `"mm"` or `"cm"`                                                     |

**Kinematics — supply *exactly one* of these two forms:**

1. **Direct 3-momentum** (`G4ParticleGun::SetParticleMomentum`):

   | key        | type       | meaning                                    |
   |------------|------------|--------------------------------------------|
   | `momentum` | [px,py,pz] | 3-momentum in `momentum_unit`              |

   Total energy is computed as `E = sqrt(|p|² + mass²)`.

2. **Kinetic energy + direction** (`SetParticleEnergy` + `SetParticleMomentumDirection`):

   | key         | type       | meaning                                             |
   |-------------|------------|-----------------------------------------------------|
   | `energy`    | double     | **kinetic** energy in `momentum_unit` (≥ 0)         |
   | `direction` | [dx,dy,dz] | momentum direction (need not be normalized; ≠ 0)    |

   Then `E_total = energy + mass`, `|p| = sqrt(E_total² − mass²)`, and the
   momentum is `|p| · direction̂`.

### Validation rules (all enforced in the constructor)

- `pdg` present and integer.
- `mass ≥ 0`; `number ≥ 1`.
- `position` / `momentum` / `direction`, when present, have exactly 3 elements.
- Exactly one kinematics form: `momentum`, **xor** (`energy` **and** `direction`).
  Providing `momentum` together with `energy`/`direction` is an error; providing
  neither form is an error.
- `energy ≥ 0`; `direction` non-zero.
- `momentum_unit ∈ {MeV, GeV}`; `length_unit ∈ {mm, cm}`.

### Example (Jsonnet)

```jsonnet
{
  cpp: 'hmp_gen_event_gun',
  output_layer: 'event',
  // 3 GeV muons along +z from the origin:
  pdg: 13,
  mass: 105.658,        // MeV
  energy: 3000.0,       // MeV kinetic
  direction: [0, 0, 1],
  number: 1,
  momentum_unit: 'MeV',
  length_unit: 'mm',
}
```

## Status

- `GenEventGun` is the standalone particle-gun node of **ddm-4nd.12**.
- Consumers (e.g. the edep-sim `Tracking` node, ddm-4nd.11) consume
  `HepMC3::GenEvent const&` directly — the same product type, with no shared
  wrapper and no dependency on this package.
