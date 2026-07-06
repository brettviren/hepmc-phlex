/*
 * This file is part of the DUNE Xerosere project.
 *
 * Copyright (c) 2026, Brookhaven Science Associates, LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 */

#pragma once

// hepmc_phlex/GenEventGun.hpp
//
// A HepMC3 analogue of Geant4's G4ParticleGun: it fires a fixed number of
// identical particles from a single vertex and returns a HepMC3::GenEvent by
// value.  Phlex takes ownership of the returned value (it moves it into its own
// product store and hands consumers a `HepMC3::GenEvent const&`), so no wrapper
// struct or shared_ptr is needed -- the product type is HepMC3::GenEvent itself.
//
// Depends only on phlex + HepMC3 (+ Boost via phlex); no Geant4 dependency.
//
// The configuration is parsed AND validated in the constructor, so an invalid
// gun configuration fails fast (before any event is produced).  See README.md
// for the full configuration schema.

#include "phlex/configuration.hpp"

#include <HepMC3/GenEvent.h>
#include <HepMC3/Units.h>

#include <array>
#include <cstddef>

namespace hepmc_phlex {

    class GenEventGun {
    public:
        // Parse and validate the gun configuration.  Throws std::runtime_error
        // with a descriptive message on any invalid or inconsistent parameter.
        explicit GenEventGun(phlex::configuration const& config);

        // Build one GenEvent (with `number` identical primaries) for the given
        // event number.  Returned by value; Phlex takes ownership.
        HepMC3::GenEvent operator()(std::size_t event_number) const;

    private:
        int pdg_ = 0;
        double mass_ = 0.0;
        int number_ = 1;
        std::array<double, 3> position_{0.0, 0.0, 0.0};
        double time_ = 0.0;

        // Final per-particle 4-momentum, precomputed in the momentum unit.
        double px_ = 0.0;
        double py_ = 0.0;
        double pz_ = 0.0;
        double e_ = 0.0;

        HepMC3::Units::MomentumUnit momentum_unit_ = HepMC3::Units::MEV;
        HepMC3::Units::LengthUnit length_unit_ = HepMC3::Units::MM;
    };

} // namespace hepmc_phlex
