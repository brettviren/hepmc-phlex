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

// hepmc_phlex/GenEventSequence.hpp
//
// A multi-event, multi-vertex generalization of GenEventGun.  Where GenEventGun
// fires one fixed kinematics for every event, GenEventSequence holds an ordered
// LIST of events; for phlex event number N it emits the N-th configured event as
// a single HepMC3::GenEvent.  Each event is itself a list of "particle" entries,
// and each entry becomes ONE GenVertex carrying `number` identical primaries.
// An event with two entries therefore yields two vertices -- the mechanism that
// lets a single event contain, e.g., two tracks at different positions, times
// and directions (see the overlap event in the fullchain multi-event test).
//
// This is the natural node for exercising phlex's multi-"event" execution and
// the multi-event output file: drive it with a driver layer of `total: N` and
// configure exactly N events.
//
// Each particle entry reuses GenEventGun's kinematics contract (EITHER a direct
// 3-momentum OR kinetic energy + direction) and its position/mass/number keys.
// Units (momentum, length) are shared by the whole sequence, as a HepMC3
// GenEvent carries a single unit system.  Unlike GenEventGun, the vertex `time`
// is given as a PHYSICAL time with an explicit `time_unit` (ns/us/ms/s) and is
// converted internally to the HepMC position-4-vector convention (c*t in the
// length unit), so time offsets can be written in natural units.
//
// The full configuration is parsed AND validated in the constructor, so an
// invalid sequence fails fast (before any event is produced).  See README.md for
// the configuration schema.

#include "phlex/configuration.hpp"

#include <HepMC3/GenEvent.h>
#include <HepMC3/Units.h>

#include <array>
#include <cstddef>
#include <vector>

namespace hepmc_phlex {

    class GenEventSequence {
    public:
        // Parse and validate the whole sequence.  Throws std::runtime_error with
        // a descriptive message on any invalid or inconsistent parameter.
        explicit GenEventSequence(phlex::configuration const& config);

        // Build the GenEvent for the given phlex event number.  `event_number`
        // must be < size().  Returned by value; Phlex takes ownership.
        HepMC3::GenEvent operator()(std::size_t event_number) const;

        // Number of configured events.
        std::size_t size() const { return events_.size(); }

    private:
        // One particle entry == one vertex carrying `number` identical primaries.
        struct Emitter {
            int pdg = 0;
            double mass = 0.0;                       // in momentum_unit
            int number = 1;
            std::array<double, 3> position{0.0, 0.0, 0.0}; // in length_unit
            double ctime = 0.0;                      // c*t, in length_unit (HepMC convention)
            double px = 0.0, py = 0.0, pz = 0.0, e = 0.0;  // in momentum_unit
        };

        std::vector<std::vector<Emitter>> events_; // events_[n] == the n-th event's emitters

        HepMC3::Units::MomentumUnit momentum_unit_ = HepMC3::Units::MEV;
        HepMC3::Units::LengthUnit length_unit_ = HepMC3::Units::MM;
    };

} // namespace hepmc_phlex
