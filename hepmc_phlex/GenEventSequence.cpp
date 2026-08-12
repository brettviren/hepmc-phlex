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

// hepmc_phlex/GenEventSequence.cpp

#include "hepmc_phlex/GenEventSequence.hpp"

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
        return std::runtime_error("hepmc_phlex::GenEventSequence: " + msg);
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

    // Nanoseconds per named time unit.
    double time_unit_ns(std::string const& s)
    {
        if (s == "ns") return 1.0;
        if (s == "us") return 1.0e3;
        if (s == "ms") return 1.0e6;
        if (s == "s") return 1.0e9;
        throw bad("'time_unit' must be one of \"ns\",\"us\",\"ms\",\"s\" (got \"" + s + "\")");
    }

} // namespace

namespace hepmc_phlex {

    GenEventSequence::GenEventSequence(phlex::configuration const& config)
    {
        // --- units (shared by the whole sequence) ---
        momentum_unit_ =
          parse_momentum_unit(config.get<std::string>("momentum_unit", std::string{"MeV"}));
        length_unit_ =
          parse_length_unit(config.get<std::string>("length_unit", std::string{"mm"}));

        // Speed of light expressed in the sequence's length unit per ns, used to
        // turn a physical vertex time into the HepMC position-4-vector t (c*t).
        //   c = 299.792458 mm/ns = 29.9792458 cm/ns
        const double c_len_per_ns = (length_unit_ == HepMC3::Units::MM) ? 299.792458 : 29.9792458;

        // --- the event list ---
        auto events = config.get_if_present<std::vector<phlex::configuration>>("events");
        if (!events || events->empty()) {
            throw bad("'events' is required and must be a non-empty list of events");
        }

        for (std::size_t ie = 0; ie < events->size(); ++ie) {
            phlex::configuration const& ev = (*events)[ie];
            const std::string where = "event[" + std::to_string(ie) + "]: ";

            auto parts = ev.get_if_present<std::vector<phlex::configuration>>("particles");
            if (!parts || parts->empty()) {
                throw bad(where + "'particles' is required and must be a non-empty list");
            }

            std::vector<Emitter> emitters;
            emitters.reserve(parts->size());

            for (std::size_t ip = 0; ip < parts->size(); ++ip) {
                phlex::configuration const& p = (*parts)[ip];
                const std::string pw =
                  where + "particle[" + std::to_string(ip) + "]: ";

                Emitter em;

                // --- particle identity ---
                auto pdg = p.get_if_present<int>("pdg");
                if (!pdg) throw bad(pw + "'pdg' is required (integer PDG code)");
                em.pdg = *pdg;

                em.mass = p.get<double>("mass", 0.0);
                if (em.mass < 0.0) throw bad(pw + "'mass' must be >= 0");

                em.number = p.get<int>("number", 1);
                if (em.number < 1) throw bad(pw + "'number' must be >= 1");

                // --- vertex ---
                em.position = get_vec3(p, "position", {0.0, 0.0, 0.0});
                const double time = p.get<double>("time", 0.0);
                const std::string tu = p.get<std::string>("time_unit", std::string{"ns"});
                em.ctime = c_len_per_ns * time * time_unit_ns(tu); // c*t in length_unit

                // --- kinematics: EITHER a direct 3-momentum OR kinetic energy +
                // direction (mirroring GenEventGun / G4ParticleGun). ---
                auto momentum = p.get_if_present<std::vector<double>>("momentum");
                auto energy = p.get_if_present<double>("energy");
                auto direction = p.get_if_present<std::vector<double>>("direction");

                if (momentum && (energy || direction)) {
                    throw bad(pw + "specify EITHER 'momentum' OR 'energy'+'direction', not both");
                }

                if (momentum) {
                    if (momentum->size() != 3)
                        throw bad(pw + "'momentum' must have 3 elements [px,py,pz]");
                    em.px = (*momentum)[0];
                    em.py = (*momentum)[1];
                    em.pz = (*momentum)[2];
                    em.e = std::sqrt(em.px * em.px + em.py * em.py + em.pz * em.pz +
                                     em.mass * em.mass);
                }
                else if (energy && direction) {
                    if (*energy < 0.0) throw bad(pw + "'energy' (kinetic) must be >= 0");
                    if (direction->size() != 3)
                        throw bad(pw + "'direction' must have 3 elements [dx,dy,dz]");
                    const double dx = (*direction)[0];
                    const double dy = (*direction)[1];
                    const double dz = (*direction)[2];
                    const double dmag = std::sqrt(dx * dx + dy * dy + dz * dz);
                    if (dmag <= 0.0) throw bad(pw + "'direction' must be non-zero");

                    const double etot = *energy + em.mass; // total = kinetic + rest mass
                    const double pmag =
                      std::sqrt(std::max(0.0, etot * etot - em.mass * em.mass));
                    em.px = pmag * dx / dmag;
                    em.py = pmag * dy / dmag;
                    em.pz = pmag * dz / dmag;
                    em.e = etot;
                }
                else {
                    throw bad(pw + "must specify 'momentum' [px,py,pz], or both 'energy' "
                                   "(kinetic) and 'direction' [dx,dy,dz]");
                }

                emitters.push_back(em);
            }

            events_.push_back(std::move(emitters));
        }
    }

    HepMC3::GenEvent GenEventSequence::operator()(std::size_t event_number) const
    {
        if (event_number >= events_.size()) {
            throw bad("event number " + std::to_string(event_number) +
                      " is out of range (configured " + std::to_string(events_.size()) +
                      " events); set the driver layer 'total' to match the 'events' list");
        }

        HepMC3::GenEvent evt(momentum_unit_, length_unit_);
        evt.set_event_number(static_cast<int>(event_number));

        for (const auto& em : events_[event_number]) {
            auto vtx = std::make_shared<HepMC3::GenVertex>(HepMC3::FourVector(
              em.position[0], em.position[1], em.position[2], em.ctime));

            for (int i = 0; i < em.number; ++i) {
                auto part = std::make_shared<HepMC3::GenParticle>(
                  HepMC3::FourVector(em.px, em.py, em.pz, em.e), em.pdg, /*status=*/1);
                part->set_generated_mass(em.mass);
                vtx->add_particle_out(part);
            }
            evt.add_vertex(vtx);
        }

        return evt; // by value; Phlex takes ownership
    }

} // namespace hepmc_phlex
