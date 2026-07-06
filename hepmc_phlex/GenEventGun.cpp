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

// hepmc_phlex/GenEventGun.cpp

#include "hepmc_phlex/GenEventGun.hpp"

#include <HepMC3/FourVector.h>
#include <HepMC3/GenEvent.h>
#include <HepMC3/GenParticle.h>
#include <HepMC3/GenVertex.h>

#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

    std::runtime_error bad(std::string const& msg)
    {
        return std::runtime_error("hepmc_phlex::GenEventGun: " + msg);
    }

    // Read an optional [x,y,z] parameter, defaulting when absent; validates size.
    std::array<double, 3> get_vec3(phlex::configuration const& c,
                                   std::string const& key,
                                   std::array<double, 3> dflt)
    {
        auto v = c.get_if_present<std::vector<double>>(key);
        if (!v) return dflt;
        if (v->size() != 3) {
            throw bad("'" + key + "' must have exactly 3 elements");
        }
        return {(*v)[0], (*v)[1], (*v)[2]};
    }

    HepMC3::Units::MomentumUnit parse_momentum_unit(std::string const& s)
    {
        if (s == "MeV") return HepMC3::Units::MEV;
        if (s == "GeV") return HepMC3::Units::GEV;
        throw bad("'momentum_unit' must be \"MeV\" or \"GeV\" (got \"" + s + "\")");
    }

    HepMC3::Units::LengthUnit parse_length_unit(std::string const& s)
    {
        if (s == "mm") return HepMC3::Units::MM;
        if (s == "cm") return HepMC3::Units::CM;
        throw bad("'length_unit' must be \"mm\" or \"cm\" (got \"" + s + "\")");
    }

} // namespace

namespace hepmc_phlex {

    GenEventGun::GenEventGun(phlex::configuration const& config)
    {
        // --- particle identity ---
        auto pdg = config.get_if_present<int>("pdg");
        if (!pdg) throw bad("'pdg' is required (integer PDG code)");
        pdg_ = *pdg;

        mass_ = config.get<double>("mass", 0.0);
        if (mass_ < 0.0) throw bad("'mass' must be >= 0");

        // --- multiplicity (identical particles per event, like G4's
        // SetNumberOfParticles) ---
        number_ = config.get<int>("number", 1);
        if (number_ < 1) throw bad("'number' must be >= 1");

        // --- vertex ---
        position_ = get_vec3(config, "position", {0.0, 0.0, 0.0});
        time_ = config.get<double>("time", 0.0);

        // --- units ---
        momentum_unit_ =
          parse_momentum_unit(config.get<std::string>("momentum_unit", std::string{"MeV"}));
        length_unit_ =
          parse_length_unit(config.get<std::string>("length_unit", std::string{"mm"}));

        // --- kinematics: EITHER a direct 3-momentum, OR kinetic energy +
        // direction (mirroring G4ParticleGun::SetParticleMomentum vs
        // SetParticleEnergy + SetParticleMomentumDirection).  Exactly one form. ---
        auto momentum = config.get_if_present<std::vector<double>>("momentum");
        auto energy = config.get_if_present<double>("energy");
        auto direction = config.get_if_present<std::vector<double>>("direction");

        if (momentum && (energy || direction)) {
            throw bad("specify EITHER 'momentum' OR 'energy'+'direction', not both");
        }

        if (momentum) {
            if (momentum->size() != 3) throw bad("'momentum' must have 3 elements [px,py,pz]");
            px_ = (*momentum)[0];
            py_ = (*momentum)[1];
            pz_ = (*momentum)[2];
            e_ = std::sqrt(px_ * px_ + py_ * py_ + pz_ * pz_ + mass_ * mass_);
        }
        else if (energy && direction) {
            if (*energy < 0.0) throw bad("'energy' (kinetic) must be >= 0");
            if (direction->size() != 3) throw bad("'direction' must have 3 elements [dx,dy,dz]");
            const double dx = (*direction)[0];
            const double dy = (*direction)[1];
            const double dz = (*direction)[2];
            const double dmag = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dmag <= 0.0) throw bad("'direction' must be non-zero");

            const double etot = *energy + mass_; // total = kinetic + rest mass
            const double pmag = std::sqrt(std::max(0.0, etot * etot - mass_ * mass_));
            px_ = pmag * dx / dmag;
            py_ = pmag * dy / dmag;
            pz_ = pmag * dz / dmag;
            e_ = etot;
        }
        else {
            throw bad("must specify 'momentum' [px,py,pz], or both 'energy' (kinetic)"
                      " and 'direction' [dx,dy,dz]");
        }
    }

    HepMC3::GenEvent GenEventGun::operator()(std::size_t event_number) const
    {
        HepMC3::GenEvent evt(momentum_unit_, length_unit_);
        evt.set_event_number(static_cast<int>(event_number));

        // The position 4-vector time component is c*t in the length unit (HepMC
        // convention).  For a gun `time` is normally 0.  (GenVertex/GenParticle
        // are shared_ptr-based in HepMC3's own model; that is internal to the
        // GenEvent value we return.)
        auto vtx = std::make_shared<HepMC3::GenVertex>(
          HepMC3::FourVector(position_[0], position_[1], position_[2], time_));

        for (int i = 0; i < number_; ++i) {
            auto part = std::make_shared<HepMC3::GenParticle>(
              HepMC3::FourVector(px_, py_, pz_, e_), pdg_, /*status=*/1); // 1 = final state
            part->set_generated_mass(mass_);
            vtx->add_particle_out(part);
        }
        evt.add_vertex(vtx);

        return evt; // by value; Phlex takes ownership
    }

} // namespace hepmc_phlex
