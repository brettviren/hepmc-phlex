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

// modules/gen_event_gun.cpp
//
// Phlex source (provider) module: registers hepmc_phlex::GenEventGun.  For each
// data cell it emits one hepmc_phlex::GenEvent carrying `number` identical
// primary particles from a single vertex.  Phlex prepends 'lib' and appends
// '.so', so add_library(hmp_gen_event_gun MODULE ...) -> libhmp_gen_event_gun.so
// (cpp: 'hmp_gen_event_gun' in the workflow).
//
// Config keys (see README.md for the full schema and validation rules):
//   output_layer   (string, required):        Phlex layer for the output product.
//   output_suffix  (string, optional="genevent"): product suffix.
//   plus the GenEventGun parameters (pdg, energy/direction or momentum, ...).

#include "hepmc_phlex/GenEventGun.hpp"

#include "phlex/configuration.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/source.hpp"

#include <HepMC3/GenEvent.h>

#include <cstddef>
#include <memory>
#include <string>

using namespace phlex;

PHLEX_REGISTER_PROVIDERS(m, config)
{
    auto const layer = config.get<std::string>("output_layer");
    auto const suffix = config.get<std::string>("output_suffix", std::string{"genevent"});

    // Parses and validates the gun configuration now (fails fast if invalid).
    auto gun = std::make_shared<hepmc_phlex::GenEventGun>(config);

    m.provide("hepmc_gen_event_gun",
              [gun](data_cell_index const& id) -> HepMC3::GenEvent {
                  return (*gun)(static_cast<std::size_t>(id.number()));
              })
      .output_product("input", experimental::identifier{suffix}, experimental::identifier{layer});
}
