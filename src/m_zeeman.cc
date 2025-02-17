/* Copyright (C) 2012
   Richard Larsson <ric.larsson@gmail.com>

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA. */

#include "auto_md.h"
#include "global_data.h"
#include "propagationmatrix.h"
#include "zeeman.h"

/* Workspace method: Doxygen documentation will be auto-generated */
void zeeman_linerecord_precalcCreateFromLines(
    ArrayOfArrayOfLineRecord& zeeman_linerecord_precalc,
    const ArrayOfArrayOfSpeciesTag& abs_species,
    const ArrayOfArrayOfLineRecord& abs_lines_per_species,
    const Index& wigner_initialized,
    const Verbosity& verbosity) {
  if (not wigner_initialized)
    throw std::runtime_error(
        "Must initialize wigner calculations to compute Zeeman effect");

  if (abs_species.nelem() != abs_lines_per_species.nelem())
    throw std::runtime_error(
        "Dimension of *abs_species* and *abs_lines_per_species* don't match.");

  // creating the ArrayOfArrayOfLineRecord
  zeeman_linerecord_precalc.resize(0);
  create_Zeeman_linerecordarrays(zeeman_linerecord_precalc,
                                 abs_species,
                                 abs_lines_per_species,
                                 false,
                                 verbosity);
}

/* Workspace method: Doxygen documentation will be auto-generated */
void zeeman_linerecord_precalcCreateWithZeroSplitting(
    ArrayOfArrayOfLineRecord& zeeman_linerecord_precalc,
    const ArrayOfArrayOfSpeciesTag& abs_species,
    const ArrayOfArrayOfLineRecord& abs_lines_per_species,
    const Index& wigner_initialized,
    const Verbosity& verbosity) {
  if (not wigner_initialized)
    throw std::runtime_error(
        "Must initialize wigner calculations to compute Zeeman effect");

  if (abs_species.nelem() != abs_lines_per_species.nelem())
    throw std::runtime_error(
        "Dimension of *abs_species* and *abs_lines_per_species* don't match.");

  // creating the ArrayOfArrayOfLineRecord
  zeeman_linerecord_precalc.resize(0);
  create_Zeeman_linerecordarrays(zeeman_linerecord_precalc,
                                 abs_species,
                                 abs_lines_per_species,
                                 true,
                                 verbosity);
}

void zeeman_linerecord_precalcModifyFromData(
    ArrayOfArrayOfLineRecord& zeeman_linerecord_precalc,
    const ArrayOfQuantumIdentifier& keys,
    const Vector& data,
    const Verbosity& verbosity) {
  CREATE_OUT2;

  if (keys.nelem() not_eq data.nelem())
    throw std::runtime_error("Mismatching data and identifier vector");

  for (ArrayOfLineRecord& lines : zeeman_linerecord_precalc) {
    Index i = 0, j = 0;
    for (LineRecord& line : lines) {
      Index upper = -1, lower = -1;
      for (Index k = 0; k < keys.nelem(); k++) {
        const QuantumIdentifier& qid = keys[k];
        if (qid.InLower(line.QuantumIdentity()))
          lower = k;
        else if (qid.InUpper(line.QuantumIdentity()))
          upper = k;
      }

      if (lower not_eq -1) line.ZeemanModel().gl() = data[lower];
      if (upper not_eq -1) line.ZeemanModel().gu() = data[upper];

      if (lower not_eq -1 or upper not_eq -1) ++i;
      if (lower not_eq -1 and upper not_eq -1) ++j;
    }
    out2 << "Modified " << i << "/" << lines.nelem() << " lines of which " << j
         << "/" << lines.nelem() << " were fully modified.\n";
  }
}

void zeeman_linerecord_precalcPrintMissing(
    const ArrayOfArrayOfLineRecord& zeeman_linerecord_precalc,
    const ArrayOfQuantumIdentifier& keys,
    const Verbosity& verbosity) {
  CREATE_OUT0;

  ArrayOfArrayOfIndex c(zeeman_linerecord_precalc.nelem());

  for (Index i = 0; i < c.nelem(); i++) {
    auto& lines = zeeman_linerecord_precalc[i];
    for (Index j = 0; j < lines.nelem(); j++) {
      auto& line = lines[j];
      bool found = false;
      for (auto& key : keys) {
        if (key.InLower(line.QuantumIdentity())) found = true;
        if (key.InUpper(line.QuantumIdentity())) found = true;
        if (found) break;
      }

      if (not found) c[i].push_back(j);
    }
  }

  for (Index i = 0; i < c.nelem(); i++) {
    for (auto& x : c[i])
      out0 << "Line is missing in keys: " << zeeman_linerecord_precalc[i][x]
           << "\n";
  }
}

/* Workspace method: Doxygen documentation will be auto-generated */
void propmat_clearskyAddZeeman(
    ArrayOfPropagationMatrix& propmat_clearsky,
    ArrayOfStokesVector& nlte_source,
    ArrayOfPropagationMatrix& dpropmat_clearsky_dx,
    ArrayOfStokesVector& dnlte_dx_source,
    ArrayOfStokesVector& nlte_dsource_dx,
    const ArrayOfArrayOfLineRecord& zeeman_linerecord_precalc,
    const Vector& f_grid,
    const ArrayOfArrayOfSpeciesTag& abs_species,
    const ArrayOfRetrievalQuantity& jacobian_quantities,
    const SpeciesAuxData& isotopologue_ratios,
    const SpeciesAuxData& partition_functions,
    const Numeric& rtp_pressure,
    const Numeric& rtp_temperature,
    const Vector& rtp_nlte,
    const Vector& rtp_vmr,
    const Vector& rtp_mag,
    const Vector& ppath_los,
    const Index& atmosphere_dim,
    const Index& manual_zeeman_tag,
    const Numeric& manual_zeeman_magnetic_field_strength,
    const Numeric& manual_zeeman_theta,
    const Numeric& manual_zeeman_eta,
    const Verbosity&) try {
  if (zeeman_linerecord_precalc.nelem() == 0) return;

  // Check that correct isotopologue ratios are defined
  checkIsotopologueRatios(abs_species, isotopologue_ratios);
  checkPartitionFunctions(abs_species, partition_functions);

  if ((atmosphere_dim not_eq 3) and (not manual_zeeman_tag))
    throw "Only for 3D *atmosphere_dim* or a manual magnetic field";
  if ((ppath_los.nelem() not_eq 2) and (not manual_zeeman_tag))
    throw "Only for 2D *ppath_los* or a manual magnetic field";

  // Change to LOS by radiation
  Vector rtp_los;
  if (not manual_zeeman_tag) mirror_los(rtp_los, ppath_los, atmosphere_dim);

  // Main computations
  zeeman_on_the_fly(propmat_clearsky,
                    nlte_source,
                    dpropmat_clearsky_dx,
                    dnlte_dx_source,
                    nlte_dsource_dx,
                    abs_species,
                    jacobian_quantities,
                    zeeman_linerecord_precalc,
                    isotopologue_ratios,
                    partition_functions,
                    f_grid,
                    rtp_vmr,
                    rtp_nlte,
                    rtp_mag,
                    rtp_los,
                    rtp_pressure,
                    rtp_temperature,
                    manual_zeeman_tag,
                    manual_zeeman_magnetic_field_strength,
                    manual_zeeman_theta,
                    manual_zeeman_eta);
} catch (const char* e) {
  std::ostringstream os;
  os << "Errors raised by *propmat_clearskyAddZeeman*:\n";
  os << "\tError: " << e << '\n';
  throw std::runtime_error(os.str());
} catch (const std::exception& e) {
  std::ostringstream os;
  os << "Errors in calls by *propmat_clearskyAddZeeman*:\n";
  os << e.what();
  throw std::runtime_error(os.str());
}
