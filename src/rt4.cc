/* Copyright (C) 2016 Jana Mendrok <jana.mendrok@gmail.com>

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
   USA. 
*/

/*!
  \file   rt4.cc
  \author Jana Mendrok <jana.mendrok@gmail.com>
  \date   2016-05-24
  
  \brief  Contains functions related to application of scattering solver RT4.
  
*/

/*===========================================================================
  === External declarations
  ===========================================================================*/

#include "config.h"

#ifdef ENABLE_RT4

#include <complex.h>
#include <cfloat>
#include <stdexcept>
#include "interpolation.h"
#include "m_xml.h"
#include "optproperties.h"
#include "physics_funcs.h"
#include "rt4.h"
#include "rte.h"

using std::ostringstream;
using std::runtime_error;

extern const Numeric PI;
extern const Numeric DEG2RAD;
extern const Numeric RAD2DEG;
extern const Numeric SPEED_OF_LIGHT;
extern const Numeric COSMIC_BG_TEMP;

//! check_rt4_input
/*!
  Checks that input of RT4Calc* is sane.

  \param nhstreams             Number of single hemisphere streams (quadrature angles).
  \param nhza                  Number of single hemisphere additional angles with RT output.
  \param nummu                 Total number of single hemisphere angles with RT output.
  \param cloudbox_on           as the WSV 
  \param rt4_is_initialized    as the WSV 
  \param atmfields_checked     as the WSV 
  \param atmgeom_checked       as the WSV 
  \param cloudbox_checked      as the WSV 
  \param nstreams              Total number of quadrature angles (both hemispheres).
  \param quad_type             Quadrature method.
  \param pnd_ncols             Number of columns (latitude points) in *pnd_field*.
  \param ifield_npages         Number of pages (polar angle points) in *doit_i_field*.
  
  \author Jana Mendrok
  \date   2017-02-22
*/
void check_rt4_input(  // Output
    Index& nhstreams,
    Index& nhza,
    Index& nummu,
    // Input
    const Index& cloudbox_on,
    const Index& atmfields_checked,
    const Index& atmgeom_checked,
    const Index& cloudbox_checked,
    const Index& scat_data_checked,
    const ArrayOfIndex& cloudbox_limits,
    const ArrayOfArrayOfSingleScatteringData& scat_data,
    const Index& atmosphere_dim,
    const Index& stokes_dim,
    const Index& nstreams,
    const String& quad_type,
    const Index& add_straight_angles,
    const Index& pnd_ncols) {
  // Don't do anything if there's no cloudbox defined.
  //if (!cloudbox_on) return;
  // Seems to loopholy to just skip the scattering, so rather throw an error
  // (assuming if RT4 is called than it's expected that a scattering calc is
  // performed. semi-quietly skipping can easily be missed and lead to wrong
  // conclusions.).
  if (!cloudbox_on) {
    throw runtime_error(
        "Cloudbox is off, no scattering calculations to be"
        " performed.");
  }

  if (atmfields_checked != 1)
    throw runtime_error(
        "The atmospheric fields must be flagged to have"
        " passed a consistency check (atmfields_checked=1).");
  if (atmgeom_checked != 1)
    throw runtime_error(
        "The atmospheric geometry must be flagged to have"
        " passed a consistency check (atmgeom_checked=1).");
  if (cloudbox_checked != 1)
    throw runtime_error(
        "The cloudbox must be flagged to have"
        " passed a consistency check (cloudbox_checked=1).");
  if (scat_data_checked != 1)
    throw runtime_error(
        "The scat_data must be flagged to have "
        "passed a consistency check (scat_data_checked=1).");

  if (atmosphere_dim != 1)
    throw runtime_error(
        "For running RT4, atmospheric dimensionality"
        " must be 1.\n");

  if (stokes_dim < 0 || stokes_dim > 2)
    throw runtime_error(
        "For running RT4, the dimension of stokes vector"
        " must be 1 or 2.\n");

  if (cloudbox_limits[0] != 0) {
    ostringstream os;
    os << "RT4 calculations currently only possible with"
       << " lower cloudbox limit\n"
       << "at 0th atmospheric level"
       << " (assumes surface there, ignoring z_surface).\n";
    throw runtime_error(os.str());
  }

  if (cloudbox_limits.nelem() != 2 * atmosphere_dim)
    throw runtime_error(
        "*cloudbox_limits* is a vector which contains the"
        " upper and lower limit of the cloud for all"
        " atmospheric dimensions. So its dimension must"
        " be 2 x *atmosphere_dim*");

  if (scat_data.empty())
    throw runtime_error(
        "No single scattering data present.\n"
        "See documentation of WSV *scat_data* for options to"
        " make single scattering data available.\n");

  if (pnd_ncols != 1)
    throw runtime_error(
        "*pnd_field* is not 1D! \n"
        "RT4 can only be used for 1D!\n");

  if (quad_type.length() > 1) {
    ostringstream os;
    os << "Input parameter *quad_type* not allowed to contain more than a"
       << " single character.\n"
       << "Yours has " << quad_type.length() << ".\n";
    throw runtime_error(os.str());
  }

  if (quad_type == "D" || quad_type == "G") {
    if (add_straight_angles)
      nhza = 1;
    else
      nhza = 0;
  } else if (quad_type == "L") {
    nhza = 0;
  } else {
    ostringstream os;
    os << "Unknown quadrature type: " << quad_type
       << ".\nOnly D, G, and L allowed.\n";
    throw runtime_error(os.str());
  }

  // RT4 actually uses number of angles in single hemisphere. However, we don't
  // want a bunch of different approaches used in the interface, so we apply the
  // DISORT (and ARTS) way of total number of angles here. Hence, we have to
  // ensure here that total number is even.
  if (nstreams / 2 * 2 != nstreams) {
    ostringstream os;
    os << "RT4 requires an even number of streams, but yours is " << nstreams
       << ".\n";
    throw runtime_error(os.str());
  }
  nhstreams = nstreams / 2;
  // nummu is the total number of angles in one hemisphere, including both
  // the quadrature angles and the "extra" angles.
  nummu = nhstreams + nhza;

  // RT4 can only completely or azimuthally randomly oriented particles.
  bool no_arb_ori = true;
  for (Index i_ss = 0; i_ss < scat_data.nelem(); i_ss++)
    for (Index i_se = 0; i_se < scat_data[i_ss].nelem(); i_se++)
      if (scat_data[i_ss][i_se].ptype != PTYPE_TOTAL_RND &&
          scat_data[i_ss][i_se].ptype != PTYPE_AZIMUTH_RND)
        no_arb_ori = false;
  if (!no_arb_ori) {
    ostringstream os;
    os << "RT4 can only handle scattering elements of type " << PTYPE_TOTAL_RND
       << " (" << PTypeToString(PTYPE_TOTAL_RND) << ") and\n"
       << PTYPE_AZIMUTH_RND << " (" << PTypeToString(PTYPE_AZIMUTH_RND)
       << "),\n"
       << "but at least one element of other type (" << PTYPE_GENERAL << "="
       << PTypeToString(PTYPE_GENERAL) << ") is present.\n";
    throw runtime_error(os.str());
  }

  //------------- end of checks ---------------------------------------
}

//! get_quad_angles
/*!
  Derive the quadrature angles related to selected RT4 quadrature type and set
  scat_za_grid accordingly.

  \param mu_values             Quadrature angle cosines.
  \param quad_weights          Quadrature weights associated with mu_values.
  \param scat_za_grid          as the WSV
  \param scat_aa_grid          as the WSV
  \param quad_type             Quadrature method.
  \param nhstreams             Number of single hemisphere streams (quadrature angles). 
  \param nhza                  Number of single hemisphere additional angles with RT output.

  \author Jana Mendrok
  \date   2017-02-22
*/
void get_quad_angles(  // Output
    VectorView mu_values,
    VectorView quad_weights,
    Vector& scat_za_grid,
    Vector& scat_aa_grid,
    // Input
    const String& quad_type,
    const Index& nhstreams,
    const Index& nhza,
    const Index& nummu) {
  if (quad_type == "D") {
    double_gauss_quadrature_(
        nhstreams, mu_values.get_c_array(), quad_weights.get_c_array());
  } else if (quad_type == "G") {
    gauss_legendre_quadrature_(
        nhstreams, mu_values.get_c_array(), quad_weights.get_c_array());
  } else  //if( quad_type=="L" )
  {
    lobatto_quadrature_(
        nhstreams, mu_values.get_c_array(), quad_weights.get_c_array());
  }

  // Set "extra" angle (at 0deg) if quad_type!="L" && !add_straight_angles
  if (nhza > 0) mu_values[nhstreams] = 1.;

  // FIXME: we should be able to avoid setting scat_za_grid here in one way,
  // and resetting in another before leaving the WSM. This, however, requires
  // rearranging the angle order and angle assignment in the RT4-SSP prep
  // routines.
  scat_za_grid.resize(2 * nummu);
  for (Index imu = 0; imu < nummu; imu++) {
    scat_za_grid[imu] = acos(mu_values[imu]) * RAD2DEG;
    scat_za_grid[nummu + imu] = 180. - scat_za_grid[imu];
    //cout << "Setting za[" << imu << "]=" << scat_za_grid[imu]
    //     << " and  za[" << nummu+imu << "]=" << scat_za_grid[nummu+imu]
    //     << " from mu[" << imu << "]=" << mu_values[imu]
    //     << " with quadweight w=" << quad_weights[imu] << ".\n";
  }
  scat_aa_grid.resize(1);
  scat_aa_grid[0] = 0.;
}

//! get_rt4surf_props
/*!
  Derive surface property input for RT4's proprietary surface handling depending
  on surface reflection type.

  \param ground_albedo         Scalar surface albedo (for ground_type=L).
  \param ground_reflec         Vector surface relfectivity (for ground_type=S).
  \param ground_index          Surface complex refractive index (for ground_type=F).
  \param f_grid                as the WSV
  \param ground_type           Surface reflection type flag.
  \param surface_skin_t        as the WSV
  \param surface_scalar_reflectivity  as the WSV (used with ground_type=L).
  \param surface_reflectivity  as the WSV (used with ground_type=S).
  \param surface_complex_refr_index   as the WSV (used with ground_type=F).
  \param stokes_dim            as the WSV

  \author Jana Mendrok
  \date   2017-02-22
*/
void get_rt4surf_props(  // Output
    Vector& ground_albedo,
    Tensor3& ground_reflec,
    ComplexVector& ground_index,
    // Input
    ConstVectorView f_grid,
    const String& ground_type,
    const Numeric& surface_skin_t,
    ConstVectorView surface_scalar_reflectivity,
    ConstTensor3View surface_reflectivity,
    const GriddedField3& surface_complex_refr_index,
    const Index& stokes_dim) {
  if (surface_skin_t < 0. || surface_skin_t > 1000.) {
    ostringstream os;
    os << "Surface temperature is set to " << surface_skin_t << " K,\n"
       << "which is not considered a meaningful value.\n";
    throw runtime_error(os.str());
  }

  const Index nf = f_grid.nelem();

  if (ground_type == "L")  // RT4's proprietary Lambertian
  {
    if (min(surface_scalar_reflectivity) < 0 ||
        max(surface_scalar_reflectivity) > 1) {
      throw runtime_error(
          "All values in *surface_scalar_reflectivity* must be inside [0,1].");
    }

    // surface albedo
    if (surface_scalar_reflectivity.nelem() == f_grid.nelem())
      ground_albedo = surface_scalar_reflectivity;
    else if (surface_scalar_reflectivity.nelem() == 1)
      ground_albedo += surface_scalar_reflectivity[0];
    else {
      ostringstream os;
      os << "For Lambertian surface reflection, the number of elements in\n"
         << "*surface_scalar_reflectivity* needs to match the length of\n"
         << "*f_grid* or be 1."
         << "\n length of *f_grid* : " << f_grid.nelem()
         << "\n length of *surface_scalar_reflectivity* : "
         << surface_scalar_reflectivity.nelem() << "\n";
      throw runtime_error(os.str());
    }

  } else if (ground_type == "S")  // RT4's 'proprietary' Specular
  {
    const Index ref_sto = surface_reflectivity.nrows();

    chk_if_in_range("surface_reflectivity's stokes_dim", ref_sto, 1, 4);
    if (ref_sto != surface_reflectivity.ncols()) {
      ostringstream os;
      os << "The number of rows and columnss in *surface_reflectivity*\n"
         << "must match each other.";
      throw runtime_error(os.str());
    }

    if (min(surface_reflectivity(joker, 0, 0)) < 0 ||
        max(surface_reflectivity(joker, 0, 0)) > 1) {
      throw runtime_error(
          "All r11 values in *surface_reflectivity* must be inside [0,1].");
    }

    // surface reflectivity
    if (surface_reflectivity.npages() == f_grid.nelem())
      if (ref_sto < stokes_dim)
        ground_reflec(joker, Range(0, ref_sto), Range(0, ref_sto)) =
            surface_reflectivity;
      else
        ground_reflec = surface_reflectivity(
            joker, Range(0, stokes_dim), Range(0, stokes_dim));
    else if (surface_reflectivity.npages() == 1)
      if (ref_sto < stokes_dim)
        for (Index f_index = 0; f_index < nf; f_index++)
          ground_reflec(f_index, Range(0, ref_sto), Range(0, ref_sto)) +=
              surface_reflectivity(0, joker, joker);
      else
        for (Index f_index = 0; f_index < nf; f_index++)
          ground_reflec(f_index, joker, joker) += surface_reflectivity(
              0, Range(0, stokes_dim), Range(0, stokes_dim));
    else {
      ostringstream os;
      os << "For specular surface reflection, the number of elements in\n"
         << "*surface_reflectivity* needs to match the length of\n"
         << "*f_grid* or be 1."
         << "\n length of *f_grid* : " << f_grid.nelem()
         << "\n length of *surface_reflectivity* : "
         << surface_reflectivity.npages() << "\n";
      throw runtime_error(os.str());
    }

  } else if (ground_type == "F")  // RT4's proprietary Fresnel
  {
    //though complex ref index is typically not smaller than (1.,0.), there
    //are physically possible exceptions. hence we don't test the values here.

    //extract/interpolate from GriddedField
    Matrix n_real(nf, 1), n_imag(nf, 1);
    complex_n_interp(n_real,
                     n_imag,
                     surface_complex_refr_index,
                     "surface_complex_refr_index",
                     f_grid,
                     Vector(1, surface_skin_t));
    //ground_index = Complex(n_real(joker,0),n_imag(joker,0));
    for (Index f_index = 0; f_index < nf; f_index++) {
      ground_index[f_index] = Complex(n_real(f_index, 0), n_imag(f_index, 0));
    }
  } else {
    ostringstream os;
    os << "Unknown surface type.\n";
    throw runtime_error(os.str());
  }
}

//! run_rt4
/*!
  Prepares actual input variables for RT4, runs it, and sorts the output into
  doit_i_field.

  \param ws                    Current workspace
  \param doit_i_field          as the WSV
  \param f_grid                as the WSV
  \param p_grid                as the WSV
  \param z_field               as the WSV
  \param t_field               as the WSV
  \param vmr_field             as the WSV
  \param pnd_field             as the WSV
  \param scat_data             as the WSV
  \param propmat_clearsky_agenda  as the WSA
  \param cloudbox_limits       as the WSV 
  \param stokes_dim            as the WSV
  \param nummu                 Total number of single hemisphere angles with RT output. 
  \param nhza                  Number of single hemisphere additional angles with RT output.
  \param ground_type           Surface reflection type flag.
  \param surface_skin_t        as the WSV
  \param ground_albedo         Scalar surface albedo (for ground_type=L).
  \param ground_reflec         Vector surface relfectivity (for ground_type=S).
  \param ground_index          Surface complex refractive index (for ground_type=F).
  \param surf_refl_mat         Surface reflection matrix (for ground_type=A).
  \param surf_emis_vec         Surface emission vector (for ground_type=A).
  \param quad_type             Quadrature method.
  \param scat_za_grid          as the WSV
  \param mu_values             Quadrature angle cosines.
  \param quad_weights          Quadrature weights associated with mu_values.
  \param auto_inc_nstreams     as the WSV
  \param pfct_method           see RT4Calc doc.
  \param pfct_aa_grid_size     see RT4Calc doc. 
  \param pfct_threshold        Requested scatter_matrix norm accuracy (in terms of single scat albedo).
  \param max_delta_tau         see RT4Calc doc.

  \author Jana Mendrok
  \date   2017-02-22
*/
void run_rt4(Workspace& ws,
             // Output
             Tensor7& doit_i_field,
             Vector& scat_za_grid,
             // Input
             ConstVectorView f_grid,
             ConstVectorView p_grid,
             ConstTensor3View z_field,
             ConstTensor3View t_field,
             ConstTensor4View vmr_field,
             ConstTensor4View pnd_field,
             const ArrayOfArrayOfSingleScatteringData& scat_data,
             const Agenda& propmat_clearsky_agenda,
             const ArrayOfIndex& cloudbox_limits,
             const Index& stokes_dim,
             const Index& nummu,
             const Index& nhza,
             const String& ground_type,
             const Numeric& surface_skin_t,
             ConstVectorView ground_albedo,
             ConstTensor3View ground_reflec,
             ConstComplexVectorView ground_index,
             ConstTensor5View surf_refl_mat,
             ConstTensor3View surf_emis_vec,
             const Agenda& surface_rtprop_agenda,
             const Numeric& surf_altitude,
             const String& quad_type,
             Vector& mu_values,
             ConstVectorView quad_weights,
             const Index& auto_inc_nstreams,
             const Index& robust,
             const Index& za_interp_order,
             const Index& cos_za_interp,
             const String& pfct_method,
             const Index& pfct_aa_grid_size,
             const Numeric& pfct_threshold,
             const Numeric& max_delta_tau,
             const Index& new_optprop,
             const Verbosity& verbosity) {
  // Input variables for RT4
  Index num_layers = p_grid.nelem() - 1;
  /*
  bool do_non_iso;
  if( p_grid.nelem() == pnd_field.npages() )
    // cloudbox covers whole atmo anyways. no need for a calculation of
    // non-iso incoming field at top-of-cloudbox. disort will be run over
    // whole atmo.
    {
      do_non_iso = false;
      num_layers = p_grid.nelem()-1;
    }
  else
    {
      if( non_iso_inc )
        // cloudbox only covers part of atmo. disort will be initialized with
        // non-isotropic incoming field and run over cloudbox only.
        {
          do_non_iso = true;
          num_layers = pnd_field.npages()-1;
        }
      else
        // cloudbox only covers part of atmo. disort will be run over whole
        // atmo, though (but only in-cloudbox rad field passed to doit_i_field).
        {
          do_non_iso = false;
          num_layers = p_grid.nelem()-1;
        }
    }
  */

  // Top of the atmosphere temperature
  //  FIXME: so far hard-coded to cosmic background. However, change that to set
  //  according to/from space_agenda.
  // to do so, we need to hand over sky_radiance instead of sky_temp as
  // Tensor3(2,stokes_dim,nummu) per frequency. That is, for properly using
  // iy_space_agenda, we need to recall the agenda over the stream angles (not
  // sure what to do with the upwelling ones. according to the RT¤-internal
  // sizing, sky_radiance contains even those. but they might not be used
  // (check!) and hence be set arbitrary.
  const Numeric sky_temp = COSMIC_BG_TEMP;

  // Data fields
  Vector height(num_layers + 1);
  Vector temperatures(num_layers + 1);
  for (Index i = 0; i < height.nelem(); i++) {
    height[i] = z_field(num_layers - i, 0, 0);
    temperatures[i] = t_field(num_layers - i, 0, 0);
  }

  // this indexes all cloudbox layers as cloudy layers.
  // optional FIXME: to use the power of RT4 (faster solving scheme for
  // individual non-cloudy layers), one should consider non-cloudy layers within
  // cloudbox. That requires some kind of recognition and index setting based on
  // pnd_field or (derived) cloud layer extinction or scattering.
  // we use something similar with iyHybrid. have a look there...
  const Index num_scatlayers = pnd_field.npages() - 1;
  Vector scatlayers(num_layers, 0.);
  Vector gas_extinct(num_layers, 0.);
  Tensor6 scatter_matrix(
      num_scatlayers, 4, nummu, stokes_dim, nummu, stokes_dim, 0.);
  Tensor6 extinct_matrix(
      1, num_scatlayers, 2, nummu, stokes_dim, stokes_dim, 0.);
  Tensor5 emis_vector(1, num_scatlayers, 2, nummu, stokes_dim, 0.);

  // if there is no scatt particle at all, we don't need to calculate the
  // scat properties (FIXME: that should rather be done by a proper setting
  // of scat_layers).
  Vector pnd_per_level(pnd_field.npages());
  for (Index clev = 0; clev < pnd_field.npages(); clev++)
    pnd_per_level[clev] = pnd_field(joker, clev, 0, 0).sum();
  Numeric pndtot = pnd_per_level.sum();

  for (Index i = 0; i < cloudbox_limits[1] - cloudbox_limits[0]; i++) {
    scatlayers[num_layers - 1 - cloudbox_limits[0] - i] = float(i + 1);
  }

  // Output variables
  Tensor3 up_rad(num_layers + 1, nummu, stokes_dim, 0.);
  Tensor3 down_rad(num_layers + 1, nummu, stokes_dim, 0.);

  Tensor6 extinct_matrix_allf;
  Tensor5 emis_vector_allf;
  if (new_optprop && !auto_inc_nstreams) {
    extinct_matrix_allf.resize(
        f_grid.nelem(), num_scatlayers, 2, nummu, stokes_dim, stokes_dim);
    emis_vector_allf.resize(
        f_grid.nelem(), num_scatlayers, 2, nummu, stokes_dim);
    par_optpropCalc2(emis_vector_allf,
                     extinct_matrix_allf,
                     //scatlayers,
                     scat_data,
                     scat_za_grid,
                     -1,
                     pnd_field,
                     t_field(Range(0, num_layers + 1), joker, joker),
                     cloudbox_limits,
                     stokes_dim);
    if (emis_vector_allf.nshelves() == 1)  // scat_data had just a single freq
                                           // point. copy into emis/ext here and
                                           // don't touch anymore later on.
    {
      emis_vector = emis_vector_allf(Range(0, 1), joker, joker, joker, joker);
      extinct_matrix =
          extinct_matrix_allf(Range(0, 1), joker, joker, joker, joker, joker);
    }
  }

  // FIXME: once, all old optprop scheme incl. the applied agendas is removed,
  // we can remove this as well.
  Vector scat_za_grid_orig;
  if (auto_inc_nstreams)
    // For the WSV scat_za_grid, we need to reset these grids instead of
    // creating a new container. this because further down some agendas are
    // used that access scat_za/aa_grid through the workspace.
    // Later on, we need to reconstruct the original setting, hence backup
    // that here.
    scat_za_grid_orig = scat_za_grid;

  Index nummu_new = 0;
  // Loop over frequencies
  for (Index f_index = 0; f_index < f_grid.nelem(); f_index++) {
    // Wavelength [um]
    Numeric wavelength;
    wavelength = 1e6 * SPEED_OF_LIGHT / f_grid[f_index];

    Matrix groundreflec = ground_reflec(f_index, joker, joker);
    Tensor4 surfreflmat = surf_refl_mat(f_index, joker, joker, joker, joker);
    Matrix surfemisvec = surf_emis_vec(f_index, joker, joker);
    //Vector muvalues=mu_values;

    // only update gas_extinct if there is any gas absorption at all (since
    // vmr_field is not freq-dependent, gas_extinct will remain as above
    // initialized (with 0) for all freqs, ie we can rely on that it wasn't
    // changed.
    if (vmr_field.nbooks() > 0) {
      gas_optpropCalc(ws,
                      gas_extinct,
                      propmat_clearsky_agenda,
                      t_field(Range(0, num_layers + 1), joker, joker),
                      vmr_field(joker, Range(0, num_layers + 1), joker, joker),
                      p_grid[Range(0, num_layers + 1)],
                      f_grid[Range(f_index, 1)]);
    }

    Index pfct_failed = 0;
    if (pndtot != 0) {
      if (nummu_new < nummu) {
        if (new_optprop) {
          if (!auto_inc_nstreams)  // all freq calculated before. just copy
                                   // here. but only if needed.
          {
            if (emis_vector_allf.nshelves() != 1) {
              emis_vector = emis_vector_allf(
                  Range(f_index, 1), joker, joker, joker, joker);
              extinct_matrix = extinct_matrix_allf(
                  Range(f_index, 1), joker, joker, joker, joker, joker);
            }
          } else {
            par_optpropCalc2(emis_vector,
                             extinct_matrix,
                             //scatlayers,
                             scat_data,
                             scat_za_grid,
                             f_index,
                             pnd_field,
                             t_field(Range(0, num_layers + 1), joker, joker),
                             cloudbox_limits,
                             stokes_dim);
          }
        } else {
          par_optpropCalc(emis_vector(0, joker, joker, joker, joker),
                          extinct_matrix(0, joker, joker, joker, joker, joker),
                          //scatlayers,
                          scat_data,
                          scat_za_grid,
                          f_index,
                          pnd_field,
                          t_field(Range(0, num_layers + 1), joker, joker),
                          cloudbox_limits,
                          stokes_dim,
                          nummu,
                          verbosity);
        }
        sca_optpropCalc(scatter_matrix,
                        pfct_failed,
                        emis_vector(0, joker, joker, joker, joker),
                        extinct_matrix(0, joker, joker, joker, joker, joker),
                        f_index,
                        scat_data,
                        pnd_field,
                        stokes_dim,
                        scat_za_grid,
                        quad_weights,
                        pfct_method,
                        pfct_aa_grid_size,
                        pfct_threshold,
                        auto_inc_nstreams,
                        verbosity);
      } else {
        pfct_failed = 1;
      }
    }

    if (!pfct_failed) {
#pragma omp critical(fortran_rt4)
      {
        // Call RT4
        radtrano_(stokes_dim,
                  nummu,
                  nhza,
                  max_delta_tau,
                  quad_type.c_str(),
                  surface_skin_t,
                  ground_type.c_str(),
                  ground_albedo[f_index],
                  ground_index[f_index],
                  groundreflec.get_c_array(),
                  surfreflmat.get_c_array(),
                  surfemisvec.get_c_array(),
                  sky_temp,
                  wavelength,
                  num_layers,
                  height.get_c_array(),
                  temperatures.get_c_array(),
                  gas_extinct.get_c_array(),
                  num_scatlayers,
                  scatlayers.get_c_array(),
                  extinct_matrix.get_c_array(),
                  emis_vector.get_c_array(),
                  scatter_matrix.get_c_array(),
                  //noutlevels,
                  //outlevels.get_c_array(),
                  mu_values.get_c_array(),
                  up_rad.get_c_array(),
                  down_rad.get_c_array());
      }
    } else  // if (auto_inc_nstreams)
    {
      if (nummu_new < nummu) nummu_new = nummu + 1;

      Index nhstreams_new;
      Vector mu_values_new, quad_weights_new, scat_aa_grid_new;
      Tensor6 scatter_matrix_new;
      Tensor6 extinct_matrix_new;
      Tensor5 emis_vector_new;
      Tensor4 surfreflmat_new;
      Matrix surfemisvec_new;

      while (pfct_failed && (2 * nummu_new) <= auto_inc_nstreams) {
        // resize and recalc nstream-affected/determined variables:
        //   - mu_values, quad_weights (resize & recalc)
        nhstreams_new = nummu_new - nhza;
        mu_values_new.resize(nummu_new);
        mu_values_new = 0.;
        quad_weights_new.resize(nummu_new);
        quad_weights_new = 0.;
        get_quad_angles(mu_values_new,
                        quad_weights_new,
                        scat_za_grid,
                        scat_aa_grid_new,
                        quad_type,
                        nhstreams_new,
                        nhza,
                        nummu_new);
        //   - resize & recalculate emis_vector, extinct_matrix (as input to scatter_matrix calc)
        extinct_matrix_new.resize(
            1, num_scatlayers, 2, nummu_new, stokes_dim, stokes_dim);
        extinct_matrix_new = 0.;
        emis_vector_new.resize(1, num_scatlayers, 2, nummu_new, stokes_dim);
        emis_vector_new = 0.;
        // FIXME: So far, outside-of-freq-loop calculated optprops will fall
        // back to in-loop-calculated ones in case of auto-increasing stream
        // numbers. There might be better options, but I (JM) couldn't come up
        // with or decide for one so far (we could recalc over all freqs. but
        // that would unnecessarily recalc lower-freq optprops, too, which are
        // not needed anymore. which could likely take more time than we
        // potentially safe through all-at-once temperature and direction
        // interpolations.
        if (new_optprop)
          par_optpropCalc2(emis_vector_new,
                           extinct_matrix_new,
                           //scatlayers,
                           scat_data,
                           scat_za_grid,
                           f_index,
                           pnd_field,
                           t_field(Range(0, num_layers + 1), joker, joker),
                           cloudbox_limits,
                           stokes_dim);
        else
          par_optpropCalc(
              emis_vector_new(0, joker, joker, joker, joker),
              extinct_matrix_new(0, joker, joker, joker, joker, joker),
              //scatlayers,
              scat_data,
              scat_za_grid,
              f_index,
              pnd_field,
              t_field(Range(0, num_layers + 1), joker, joker),
              cloudbox_limits,
              stokes_dim,
              nummu_new,
              verbosity);
        //   - resize & recalc scatter_matrix
        scatter_matrix_new.resize(
            num_scatlayers, 4, nummu_new, stokes_dim, nummu_new, stokes_dim);
        scatter_matrix_new = 0.;
        pfct_failed = 0;
        sca_optpropCalc(
            scatter_matrix_new,
            pfct_failed,
            emis_vector_new(0, joker, joker, joker, joker),
            extinct_matrix_new(0, joker, joker, joker, joker, joker),
            f_index,
            scat_data,
            pnd_field,
            stokes_dim,
            scat_za_grid,
            quad_weights_new,
            pfct_method,
            pfct_aa_grid_size,
            pfct_threshold,
            auto_inc_nstreams,
            verbosity);

        if (pfct_failed) nummu_new = nummu_new + 1;
      }

      if (pfct_failed) {
        nummu_new = nummu_new - 1;
        ostringstream os;
        os << "Could not increase nstreams sufficiently (current: "
           << 2 * nummu_new << ")\n"
           << "to satisfy scattering matrix norm at f[" << f_index
           << "]=" << f_grid[f_index] * 1e-9 << " GHz.\n";
        if (!robust) {
          // couldn't find a nstreams within the limits of auto_inc_nstremas
          // (aka max. nstreams) that satisfies the scattering matrix norm.
          // Hence fail completely.
          os << "Try higher maximum number of allowed streams (ie. higher"
             << " auto_inc_nstreams than " << auto_inc_nstreams << ").";
          throw runtime_error(os.str());
        } else {
          CREATE_OUT1;
          os << "Continuing with nstreams=" << 2 * nummu_new
             << ". Output for this frequency might be erroneous.";
          out1 << os.str();

          nhstreams_new = nummu_new - nhza;
          mu_values_new.resize(nummu_new);
          mu_values_new = 0.;
          quad_weights_new.resize(nummu_new);
          quad_weights_new = 0.;
          get_quad_angles(mu_values_new,
                          quad_weights_new,
                          scat_za_grid,
                          scat_aa_grid_new,
                          quad_type,
                          nhstreams_new,
                          nhza,
                          nummu_new);
          extinct_matrix_new.resize(
              1, num_scatlayers, 2, nummu_new, stokes_dim, stokes_dim);
          extinct_matrix_new = 0.;
          emis_vector_new.resize(1, num_scatlayers, 2, nummu_new, stokes_dim);
          emis_vector_new = 0.;
          if (new_optprop)
            par_optpropCalc2(emis_vector_new,
                             extinct_matrix_new,
                             //scatlayers,
                             scat_data,
                             scat_za_grid,
                             f_index,
                             pnd_field,
                             t_field(Range(0, num_layers + 1), joker, joker),
                             cloudbox_limits,
                             stokes_dim);
          else
            par_optpropCalc(
                emis_vector_new(0, joker, joker, joker, joker),
                extinct_matrix_new(0, joker, joker, joker, joker, joker),
                //scatlayers,
                scat_data,
                scat_za_grid,
                f_index,
                pnd_field,
                t_field(Range(0, num_layers + 1), joker, joker),
                cloudbox_limits,
                stokes_dim,
                nummu_new,
                verbosity);
          //   - resize & recalc scatter_matrix
          scatter_matrix_new.resize(
              num_scatlayers, 4, nummu_new, stokes_dim, nummu_new, stokes_dim);
          scatter_matrix_new = 0.;
          pfct_failed = -1;
          sca_optpropCalc(
              scatter_matrix_new,
              pfct_failed,
              emis_vector_new(0, joker, joker, joker, joker),
              extinct_matrix_new(0, joker, joker, joker, joker, joker),
              f_index,
              scat_data,
              pnd_field,
              stokes_dim,
              scat_za_grid,
              quad_weights_new,
              pfct_method,
              pfct_aa_grid_size,
              pfct_threshold,
              0,
              verbosity);
        }
      }

      // resize and calc remaining nstream-affected variables:
      //   - in case of surface_rtprop_agenda driven surface: surfreflmat, surfemisvec
      if (ground_type == "A")  // surface_rtprop_agenda driven surface
      {
        Tensor5 srm_new(1, nummu_new, stokes_dim, nummu_new, stokes_dim, 0.);
        Tensor3 sev_new(1, nummu_new, stokes_dim, 0.);
        surf_optpropCalc(ws,
                         srm_new,
                         sev_new,
                         surface_rtprop_agenda,
                         f_grid[Range(f_index, 1)],
                         scat_za_grid,
                         mu_values_new,
                         quad_weights_new,
                         stokes_dim,
                         surf_altitude);
        surfreflmat_new = srm_new(0, joker, joker, joker, joker);
        surfemisvec_new = sev_new(0, joker, joker);
      }
      //   - up/down_rad (resize only)
      Tensor3 up_rad_new(num_layers + 1, nummu_new, stokes_dim, 0.);
      Tensor3 down_rad_new(num_layers + 1, nummu_new, stokes_dim, 0.);
      //
      // run radtrano_
#pragma omp critical(fortran_rt4)
      {
        // Call RT4
        radtrano_(stokes_dim,
                  nummu_new,
                  nhza,
                  max_delta_tau,
                  quad_type.c_str(),
                  surface_skin_t,
                  ground_type.c_str(),
                  ground_albedo[f_index],
                  ground_index[f_index],
                  groundreflec.get_c_array(),
                  surfreflmat_new.get_c_array(),
                  surfemisvec_new.get_c_array(),
                  sky_temp,
                  wavelength,
                  num_layers,
                  height.get_c_array(),
                  temperatures.get_c_array(),
                  gas_extinct.get_c_array(),
                  num_scatlayers,
                  scatlayers.get_c_array(),
                  extinct_matrix_new(0, joker, joker, joker, joker, joker)
                      .get_c_array(),
                  emis_vector_new(0, joker, joker, joker, joker).get_c_array(),
                  scatter_matrix_new.get_c_array(),
                  //noutlevels,
                  //outlevels.get_c_array(),
                  mu_values_new.get_c_array(),
                  up_rad_new.get_c_array(),
                  down_rad_new.get_c_array());
      }
      // back-interpolate nstream_new fields to nstreams
      //   (possible to use iyCloudboxInterp agenda? nja, not really a good
      //   idea. too much overhead there (checking, 3D+2ang interpol). rather
      //   use interp_order as additional user parameter.
      //   extrapol issues shouldn't occur as we go from finer to coarser
      //   angular grid)
      //   - loop over nummu:
      //     - determine weights per ummu ang (should be valid for both up and
      //       down)
      //     - loop over num_layers and stokes_dim:
      //       - apply weights
      for (Index j = 0; j < nummu; j++) {
        GridPosPoly gp_za;
        if (cos_za_interp) {
          gridpos_poly(
              gp_za, mu_values_new, mu_values[j], za_interp_order, 0.5);
        } else {
          gridpos_poly(gp_za,
                       scat_za_grid[Range(0, nummu_new)],
                       scat_za_grid_orig[j],
                       za_interp_order,
                       0.5);
        }
        Vector itw(gp_za.idx.nelem());
        interpweights(itw, gp_za);

        for (Index k = 0; k < num_layers + 1; k++)
          for (Index ist = 0; ist < stokes_dim; ist++) {
            up_rad(k, j, ist) = interp(itw, up_rad_new(k, joker, ist), gp_za);
            down_rad(k, j, ist) =
                interp(itw, down_rad_new(k, joker, ist), gp_za);
          }
      }

      // reconstruct scat_za_grid
      scat_za_grid = scat_za_grid_orig;
    }

    // RT4 rad output is in wavelength units, nominally in W/(m2 sr um), where
    // wavelength input is required in um.
    // FIXME: When using wavelength input in m, output should be in W/(m2 sr
    // m). However, check this. So, at first we use wavelength in um. Then
    // change and compare.
    //
    // FIXME: if ever we allow the cloudbox to be not directly at the surface
    // (at atm level #0, respectively), the assigning from up/down_rad to
    // doit_i_field needs to checked. there seems some offsetting going on
    // (test example: TestDOIT.arts. if kept like below, doit_i_field at
    // top-of-cloudbox seems to actually be from somewhere within the
    // cloud(box) indicated by downwelling being to high and downwelling
    // exhibiting a non-zero polarisation signature (which it wouldn't with
    // only scalar gas abs above).
    //
    Numeric rad_l2f = wavelength / f_grid[f_index];
    // down/up_rad contain the radiances in order from slant (90deg) to steep
    // (0 and 180deg, respectively) streams,then the possible extra angle(s).
    // We need to resort them properly into doit_i_field, such that order is
    // from 0 to 180deg.
    for (Index j = 0; j < nummu; j++)
      for (Index k = 0; k < (cloudbox_limits[1] - cloudbox_limits[0] + 1); k++)
        for (Index ist = 0; ist < stokes_dim; ist++) {
          //doit_i_field(f_index, k, 0, 0, nummu+j, 0, ist) =
          //  up_rad(num_layers-k,j,ist)*rad_l2f;
          //doit_i_field(f_index, k, 0, 0, nummu-1-j, 0, ist) =
          //  down_rad(num_layers-k,j,ist)*rad_l2f;
          doit_i_field(f_index, k, 0, 0, nummu + j, 0, ist) =
              up_rad(num_layers - k, j, ist) * rad_l2f;
          doit_i_field(f_index, k, 0, 0, nummu - 1 - j, 0, ist) =
              down_rad(num_layers - k, j, ist) * rad_l2f;
        }
  }
}

//! run_rt4_new
/*!
  Prepares actual input variables for RT4, runs it, and sorts the output into
  doit_i_field.

  \param ws                    Current workspace
  \param doit_i_field          as the WSV
  \param f_grid                as the WSV
  \param p_grid                as the WSV
  \param z_field               as the WSV
  \param t_field               as the WSV
  \param vmr_field             as the WSV
  \param pnd_field             as the WSV
  \param scat_data             as the WSV
  \param propmat_clearsky_agenda  as the WSA
  \param cloudbox_limits       as the WSV 
  \param stokes_dim            as the WSV
  \param nummu                 Total number of single hemisphere angles with RT output. 
  \param nhza                  Number of single hemisphere additional angles with RT output.
  \param ground_type           Surface reflection type flag.
  \param surface_skin_t        as the WSV
  \param ground_albedo         Scalar surface albedo (for ground_type=L).
  \param ground_reflec         Vector surface relfectivity (for ground_type=S).
  \param ground_index          Surface complex refractive index (for ground_type=F).
  \param surf_refl_mat         Surface reflection matrix (for ground_type=A).
  \param surf_emis_vec         Surface emission vector (for ground_type=A).
  \param quad_type             Quadrature method.
  \param scat_za_grid          as the WSV
  \param mu_values             Quadrature angle cosines.
  \param quad_weights          Quadrature weights associated with mu_values.
  \param auto_inc_nstreams     as the WSV
  \param pfct_method           see RT4Calc doc.
  \param pfct_aa_grid_size     see RT4Calc doc. 
  \param pfct_threshold        Requested scatter_matrix norm accuracy (in terms of single scat albedo).
  \param max_delta_tau         see RT4Calc doc.

  \author Jana Mendrok
  \date   2017-02-22
*/
void run_rt4_new(Workspace& ws,
                 // Output
                 Tensor7& doit_i_field,
                 Vector& scat_za_grid,
                 // Input
                 ConstVectorView f_grid,
                 ConstVectorView p_grid,
                 ConstTensor3View z_field,
                 ConstTensor3View t_field,
                 ConstTensor4View vmr_field,
                 ConstTensor4View pnd_field,
                 const ArrayOfArrayOfSingleScatteringData& scat_data,
                 const Agenda& propmat_clearsky_agenda,
                 const ArrayOfIndex& cloudbox_limits,
                 const Index& stokes_dim,
                 const Index& nummu,
                 const Index& nhza,
                 const String& ground_type,
                 const Numeric& surface_skin_t,
                 ConstVectorView ground_albedo,
                 ConstTensor3View ground_reflec,
                 ConstComplexVectorView ground_index,
                 ConstTensor5View surf_refl_mat,
                 ConstTensor3View surf_emis_vec,
                 const Agenda& surface_rtprop_agenda,
                 const Numeric& surf_altitude,
                 const String& quad_type,
                 Vector& mu_values,
                 ConstVectorView quad_weights,
                 const Index& auto_inc_nstreams,
                 const Index& robust,
                 const Index& za_interp_order,
                 const Index& cos_za_interp,
                 const String& pfct_method,
                 const Index& pfct_aa_grid_size,
                 const Numeric& pfct_threshold,
                 const Numeric& max_delta_tau,
                 const Verbosity& verbosity) {
  // Input variables for RT4
  Index num_layers = p_grid.nelem() - 1;
  /*
  bool do_non_iso;
  if( p_grid.nelem() == pnd_field.npages() )
    // cloudbox covers whole atmo anyways. no need for a calculation of
    // non-iso incoming field at top-of-cloudbox. rt4 will be run over
    // whole atmo.
    {
      do_non_iso = false;
      num_layers = p_grid.nelem()-1;
    }
  else
    {
      if( non_iso_inc )
        // cloudbox only covers part of atmo. rt4 will be initialized with
        // non-isotropic incoming field and run over cloudbox only.
        {
          do_non_iso = true;
          num_layers = pnd_field.npages()-1;
        }
      else
        // cloudbox only covers part of atmo. rt4 will be run over whole
        // atmo, though (but only in-cloudbox rad field passed to doit_i_field).
        {
          do_non_iso = false;
          num_layers = p_grid.nelem()-1;
        }
    }
  */

  // Top of the atmosphere temperature
  //  FIXME: so far hard-coded to cosmic background. However, change that to set
  //  according to/from space_agenda.
  // to do so, we need to hand over sky_radiance instead of sky_temp as
  // Tensor3(2,stokes_dim,nummu) per frequency. That is, for properly using
  // iy_space_agenda, we need to recall the agenda over the stream angles (not
  // sure what to do with the upwelling ones. according to the RT4-internal
  // sizing, sky_radiance contains even those. but they might not be used
  // (check!) and hence be set arbitrary.
  const Numeric sky_temp = COSMIC_BG_TEMP;

  // Data fields
  Vector height(num_layers + 1);
  Vector temperatures(num_layers + 1);
  for (Index i = 0; i < height.nelem(); i++) {
    height[i] = z_field(num_layers - i, 0, 0);
    temperatures[i] = t_field(num_layers - i, 0, 0);
  }

  // this indexes all cloudbox layers as cloudy layers.
  // optional FIXME: to use the power of RT4 (faster solving scheme for
  // individual non-cloudy layers), one should consider non-cloudy layers within
  // cloudbox. That requires some kind of recognition and index setting based on
  // pnd_field or (derived) cloud layer extinction or scattering.
  // we use something similar with iyHybrid. have a look there...
  const Index num_scatlayers = pnd_field.npages() - 1;
  Vector scatlayers(num_layers, 0.);
  Vector gas_extinct(num_layers, 0.);
  Tensor6 scatter_matrix(
      num_scatlayers, 4, nummu, stokes_dim, nummu, stokes_dim, 0.);
  Tensor6 extinct_matrix(
      1, num_scatlayers, 2, nummu, stokes_dim, stokes_dim, 0.);
  Tensor5 emis_vector(1, num_scatlayers, 2, nummu, stokes_dim, 0.);

  // if there is no scatt particle at all, we don't need to calculate the
  // scat properties (FIXME: that should rather be done by a proper setting
  // of scat_layers).
  Vector pnd_per_level(pnd_field.npages());
  for (Index clev = 0; clev < pnd_field.npages(); clev++)
    pnd_per_level[clev] = pnd_field(joker, clev, 0, 0).sum();
  Numeric pndtot = pnd_per_level.sum();

  for (Index i = 0; i < cloudbox_limits[1] - cloudbox_limits[0]; i++) {
    scatlayers[num_layers - 1 - cloudbox_limits[0] - i] = float(i + 1);
  }

  // Output variables
  Tensor3 up_rad(num_layers + 1, nummu, stokes_dim, 0.);
  Tensor3 down_rad(num_layers + 1, nummu, stokes_dim, 0.);

  Tensor6 extinct_matrix_allf;
  Tensor5 emis_vector_allf;
  if (!auto_inc_nstreams) {
    extinct_matrix_allf.resize(
        f_grid.nelem(), num_scatlayers, 2, nummu, stokes_dim, stokes_dim);
    emis_vector_allf.resize(
        f_grid.nelem(), num_scatlayers, 2, nummu, stokes_dim);
    par_optpropCalc2(emis_vector_allf,
                     extinct_matrix_allf,
                     //scatlayers,
                     scat_data,
                     scat_za_grid,
                     -1,
                     pnd_field,
                     t_field(Range(0, num_layers + 1), joker, joker),
                     cloudbox_limits,
                     stokes_dim);
    if (emis_vector_allf.nshelves() == 1)  // scat_data had just a single freq
                                           // point. copy into emis/ext here and
                                           // don't touch anymore later on.
    {
      emis_vector = emis_vector_allf(Range(0, 1), joker, joker, joker, joker);
      extinct_matrix =
          extinct_matrix_allf(Range(0, 1), joker, joker, joker, joker, joker);
    }
  }

  // FIXME: once, all old optprop scheme incl. the applied agendas is removed,
  // we can remove this as well.
  Vector scat_za_grid_orig;
  if (auto_inc_nstreams)
    // For the WSV scat_za_grid, we need to reset these grids instead of
    // creating a new container. this because further down some agendas are
    // used that access scat_za/aa_grid through the workspace.
    // Later on, we need to reconstruct the original setting, hence backup
    // that here.
    scat_za_grid_orig = scat_za_grid;

  Index nummu_new = 0;
  // Loop over frequencies
  for (Index f_index = 0; f_index < f_grid.nelem(); f_index++) {
    // Wavelength [um]
    Numeric wavelength;
    wavelength = 1e6 * SPEED_OF_LIGHT / f_grid[f_index];

    Matrix groundreflec = ground_reflec(f_index, joker, joker);
    Tensor4 surfreflmat = surf_refl_mat(f_index, joker, joker, joker, joker);
    Matrix surfemisvec = surf_emis_vec(f_index, joker, joker);
    //Vector muvalues=mu_values;

    // only update gas_extinct if there is any gas absorption at all (since
    // vmr_field is not freq-dependent, gas_extinct will remain as above
    // initialized (with 0) for all freqs, ie we can rely on that it wasn't
    // changed).
    if (vmr_field.nbooks() > 0) {
      gas_optpropCalc(ws,
                      gas_extinct,
                      propmat_clearsky_agenda,
                      t_field(Range(0, num_layers + 1), joker, joker),
                      vmr_field(joker, Range(0, num_layers + 1), joker, joker),
                      p_grid[Range(0, num_layers + 1)],
                      f_grid[Range(f_index, 1)]);
    }

    Index pfct_failed = 0;
    if (pndtot != 0) {
      if (nummu_new < nummu) {
        if (!auto_inc_nstreams)  // all freq calculated before. just copy
                                 // here. but only if needed.
        {
          if (emis_vector_allf.nshelves() != 1) {
            emis_vector =
                emis_vector_allf(Range(f_index, 1), joker, joker, joker, joker);
            extinct_matrix = extinct_matrix_allf(
                Range(f_index, 1), joker, joker, joker, joker, joker);
          }
        } else {
          par_optpropCalc2(emis_vector,
                           extinct_matrix,
                           //scatlayers,
                           scat_data,
                           scat_za_grid,
                           f_index,
                           pnd_field,
                           t_field(Range(0, num_layers + 1), joker, joker),
                           cloudbox_limits,
                           stokes_dim);
        }
        sca_optpropCalc(scatter_matrix,
                        pfct_failed,
                        emis_vector(0, joker, joker, joker, joker),
                        extinct_matrix(0, joker, joker, joker, joker, joker),
                        f_index,
                        scat_data,
                        pnd_field,
                        stokes_dim,
                        scat_za_grid,
                        quad_weights,
                        pfct_method,
                        pfct_aa_grid_size,
                        pfct_threshold,
                        auto_inc_nstreams,
                        verbosity);
      } else {
        pfct_failed = 1;
      }
    }

    if (!pfct_failed) {
#pragma omp critical(fortran_rt4)
      {
        // Call RT4
        radtrano_(stokes_dim,
                  nummu,
                  nhza,
                  max_delta_tau,
                  quad_type.c_str(),
                  surface_skin_t,
                  ground_type.c_str(),
                  ground_albedo[f_index],
                  ground_index[f_index],
                  groundreflec.get_c_array(),
                  surfreflmat.get_c_array(),
                  surfemisvec.get_c_array(),
                  sky_temp,
                  wavelength,
                  num_layers,
                  height.get_c_array(),
                  temperatures.get_c_array(),
                  gas_extinct.get_c_array(),
                  num_scatlayers,
                  scatlayers.get_c_array(),
                  extinct_matrix.get_c_array(),
                  emis_vector.get_c_array(),
                  scatter_matrix.get_c_array(),
                  //noutlevels,
                  //outlevels.get_c_array(),
                  mu_values.get_c_array(),
                  up_rad.get_c_array(),
                  down_rad.get_c_array());
      }
    } else  // if (auto_inc_nstreams)
    {
      if (nummu_new < nummu) nummu_new = nummu + 1;

      Index nhstreams_new;
      Vector mu_values_new, quad_weights_new, scat_aa_grid_new;
      Tensor6 scatter_matrix_new;
      Tensor6 extinct_matrix_new;
      Tensor5 emis_vector_new;
      Tensor4 surfreflmat_new;
      Matrix surfemisvec_new;

      while (pfct_failed && (2 * nummu_new) <= auto_inc_nstreams) {
        // resize and recalc nstream-affected/determined variables:
        //   - mu_values, quad_weights (resize & recalc)
        nhstreams_new = nummu_new - nhza;
        mu_values_new.resize(nummu_new);
        mu_values_new = 0.;
        quad_weights_new.resize(nummu_new);
        quad_weights_new = 0.;
        get_quad_angles(mu_values_new,
                        quad_weights_new,
                        scat_za_grid,
                        scat_aa_grid_new,
                        quad_type,
                        nhstreams_new,
                        nhza,
                        nummu_new);

        //   - resize & recalculate emis_vector, extinct_matrix (as input to scatter_matrix calc)
        extinct_matrix_new.resize(
            1, num_scatlayers, 2, nummu_new, stokes_dim, stokes_dim);
        extinct_matrix_new = 0.;
        emis_vector_new.resize(1, num_scatlayers, 2, nummu_new, stokes_dim);
        emis_vector_new = 0.;
        // FIXME: So far, outside-of-freq-loop calculated optprops will fall
        // back to in-loop-calculated ones in case of auto-increasing stream
        // numbers. There might be better options, but I (JM) couldn't come up
        // with or decide for one so far (we could recalc over all freqs. but
        // that would unnecessarily recalc lower-freq optprops, too, which are
        // not needed anymore. which could likely take more time than we
        // potentially safe through all-at-once temperature and direction
        // interpolations.
        par_optpropCalc2(emis_vector_new,
                         extinct_matrix_new,
                         //scatlayers,
                         scat_data,
                         scat_za_grid,
                         f_index,
                         pnd_field,
                         t_field(Range(0, num_layers + 1), joker, joker),
                         cloudbox_limits,
                         stokes_dim);

        //   - resize & recalc scatter_matrix
        scatter_matrix_new.resize(
            num_scatlayers, 4, nummu_new, stokes_dim, nummu_new, stokes_dim);
        scatter_matrix_new = 0.;
        pfct_failed = 0;
        sca_optpropCalc(
            scatter_matrix_new,
            pfct_failed,
            emis_vector_new(0, joker, joker, joker, joker),
            extinct_matrix_new(0, joker, joker, joker, joker, joker),
            f_index,
            scat_data,
            pnd_field,
            stokes_dim,
            scat_za_grid,
            quad_weights_new,
            pfct_method,
            pfct_aa_grid_size,
            pfct_threshold,
            auto_inc_nstreams,
            verbosity);

        if (pfct_failed) nummu_new = nummu_new + 1;
      }

      if (pfct_failed) {
        nummu_new = nummu_new - 1;
        ostringstream os;
        os << "Could not increase nstreams sufficiently (current: "
           << 2 * nummu_new << ")\n"
           << "to satisfy scattering matrix norm at f[" << f_index
           << "]=" << f_grid[f_index] * 1e-9 << " GHz.\n";
        if (!robust) {
          // couldn't find a nstreams within the limits of auto_inc_nstremas
          // (aka max. nstreams) that satisfies the scattering matrix norm.
          // Hence fail completely.
          os << "Try higher maximum number of allowed streams (ie. higher"
             << " auto_inc_nstreams than " << auto_inc_nstreams << ").";
          throw runtime_error(os.str());
        } else {
          CREATE_OUT1;
          os << "Continuing with nstreams=" << 2 * nummu_new
             << ". Output for this frequency might be erroneous.";
          out1 << os.str();
          pfct_failed = -1;
          sca_optpropCalc(
              scatter_matrix_new,
              pfct_failed,
              emis_vector_new(0, joker, joker, joker, joker),
              extinct_matrix_new(0, joker, joker, joker, joker, joker),
              f_index,
              scat_data,
              pnd_field,
              stokes_dim,
              scat_za_grid,
              quad_weights_new,
              pfct_method,
              pfct_aa_grid_size,
              pfct_threshold,
              0,
              verbosity);
        }
      }

      // resize and calc remaining nstream-affected variables:
      //   - in case of surface_rtprop_agenda driven surface: surfreflmat, surfemisvec
      if (ground_type == "A")  // surface_rtprop_agenda driven surface
      {
        Tensor5 srm_new(1, nummu_new, stokes_dim, nummu_new, stokes_dim, 0.);
        Tensor3 sev_new(1, nummu_new, stokes_dim, 0.);
        surf_optpropCalc(ws,
                         srm_new,
                         sev_new,
                         surface_rtprop_agenda,
                         f_grid[Range(f_index, 1)],
                         scat_za_grid,
                         mu_values_new,
                         quad_weights_new,
                         stokes_dim,
                         surf_altitude);
        surfreflmat_new = srm_new(0, joker, joker, joker, joker);
        surfemisvec_new = sev_new(0, joker, joker);
      }
      //   - up/down_rad (resize only)
      Tensor3 up_rad_new(num_layers + 1, nummu_new, stokes_dim, 0.);
      Tensor3 down_rad_new(num_layers + 1, nummu_new, stokes_dim, 0.);
      //
      // run radtrano_
#pragma omp critical(fortran_rt4)
      {
        // Call RT4
        radtrano_(stokes_dim,
                  nummu_new,
                  nhza,
                  max_delta_tau,
                  quad_type.c_str(),
                  surface_skin_t,
                  ground_type.c_str(),
                  ground_albedo[f_index],
                  ground_index[f_index],
                  groundreflec.get_c_array(),
                  surfreflmat_new.get_c_array(),
                  surfemisvec_new.get_c_array(),
                  sky_temp,
                  wavelength,
                  num_layers,
                  height.get_c_array(),
                  temperatures.get_c_array(),
                  gas_extinct.get_c_array(),
                  num_scatlayers,
                  scatlayers.get_c_array(),
                  extinct_matrix_new(0, joker, joker, joker, joker, joker)
                      .get_c_array(),
                  emis_vector_new(0, joker, joker, joker, joker).get_c_array(),
                  scatter_matrix_new.get_c_array(),
                  //noutlevels,
                  //outlevels.get_c_array(),
                  mu_values_new.get_c_array(),
                  up_rad_new.get_c_array(),
                  down_rad_new.get_c_array());
      }
      // back-interpolate nstream_new fields to nstreams
      //   (possible to use iyCloudboxInterp agenda? nja, not really a good
      //   idea. too much overhead there (checking, 3D+2ang interpol). rather
      //   use interp_order as additional user parameter.
      //   extrapol issues shouldn't occur as we go from finer to coarser
      //   angular grid)
      //   - loop over nummu:
      //     - determine weights per ummu ang (should be valid for both up and
      //       down)
      //     - loop over num_layers and stokes_dim:
      //       - apply weights
      for (Index j = 0; j < nummu; j++) {
        GridPosPoly gp_za;
        if (cos_za_interp) {
          gridpos_poly(
              gp_za, mu_values_new, mu_values[j], za_interp_order, 0.5);
        } else {
          gridpos_poly(gp_za,
                       scat_za_grid[Range(0, nummu_new)],
                       scat_za_grid_orig[j],
                       za_interp_order,
                       0.5);
        }
        Vector itw(gp_za.idx.nelem());
        interpweights(itw, gp_za);

        for (Index k = 0; k < num_layers + 1; k++)
          for (Index ist = 0; ist < stokes_dim; ist++) {
            up_rad(k, j, ist) = interp(itw, up_rad_new(k, joker, ist), gp_za);
            down_rad(k, j, ist) =
                interp(itw, down_rad_new(k, joker, ist), gp_za);
          }
      }

      // reconstruct scat_za_grid
      scat_za_grid = scat_za_grid_orig;
    }

    // RT4 rad output is in wavelength units, nominally in W/(m2 sr um), where
    // wavelength input is required in um.
    // FIXME: When using wavelength input in m, output should be in W/(m2 sr
    // m). However, check this. So, at first we use wavelength in um. Then
    // change and compare.
    //
    // FIXME: if ever we allow the cloudbox to be not directly at the surface
    // (at atm level #0, respectively), the assigning from up/down_rad to
    // doit_i_field needs to checked. there seems some offsetting going on
    // (test example: TestDOIT.arts. if kept like below, doit_i_field at
    // top-of-cloudbox seems to actually be from somewhere within the
    // cloud(box) indicated by downwelling being to high and downwelling
    // exhibiting a non-zero polarisation signature (which it wouldn't with
    // only scalar gas abs above).
    //
    Numeric rad_l2f = wavelength / f_grid[f_index];
    // down/up_rad contain the radiances in order from slant (90deg) to steep
    // (0 and 180deg, respectively) streams,then the possible extra angle(s).
    // We need to resort them properly into doit_i_field, such that order is
    // from 0 to 180deg.
    for (Index j = 0; j < nummu; j++)
      for (Index k = 0; k < (cloudbox_limits[1] - cloudbox_limits[0] + 1); k++)
        for (Index ist = 0; ist < stokes_dim; ist++) {
          //doit_i_field(f_index, k, 0, 0, nummu+j, 0, ist) =
          //  up_rad(num_layers-k,j,ist)*rad_l2f;
          //doit_i_field(f_index, k, 0, 0, nummu-1-j, 0, ist) =
          //  down_rad(num_layers-k,j,ist)*rad_l2f;
          doit_i_field(f_index, k, 0, 0, nummu + j, 0, ist) =
              up_rad(num_layers - k, j, ist) * rad_l2f;
          doit_i_field(f_index, k, 0, 0, nummu - 1 - j, 0, ist) =
              down_rad(num_layers - k, j, ist) * rad_l2f;
        }
  }
}

//! scat_za_grid_adjust
/*!
  Reset scat_za_grid such that it is consistent with ARTS' scat_za_grid
  requirements (instead of with RT4 as in input state).

  \param scat_za_grid          as the WSV
  \param mu_values             Quadrature angle cosines.
  \param nummu                 Number of single hemisphere polar angles.

  \author Jana Mendrok
  \date   2017-02-22
*/
void scat_za_grid_adjust(  // Output
    Vector& scat_za_grid,
    // Input
    ConstVectorView mu_values,
    const Index& nummu) {
  for (Index j = 0; j < nummu; j++) {
    scat_za_grid[nummu - 1 - j] = acos(mu_values[j]) * RAD2DEG;
    scat_za_grid[nummu + j] = 180. - acos(mu_values[j]) * RAD2DEG;
    //cout << "setting scat_za[" << nummu-1-j << "]=" << scat_za_grid[nummu-1-j]
    //     << " and [" << nummu+j << "]=" << scat_za_grid[nummu+j]
    //     << " from mu[" << j << "]=" << mu_values[j] << "\n";
  }
}

//! gas_optpropCalc
/*!
  Calculates layer averaged gaseous extinction (gas_extinct). This variable is
  required as input for the RT4 subroutine.

  \param ws                    Current workspace
  \param gas_extinct           Layer averaged gas extinction for all layers
  \param propmat_clearsky_agenda as the WSA
  \param t_field               as the WSV
  \param vmr_field             as the WSV
  \param p_grid                as the WSV
  \param f_mono                frequency (single entry vector)
  
  \author Jana Mendrok
  \date   2016-08-08
*/
void gas_optpropCalc(Workspace& ws,
                     VectorView gas_extinct,
                     const Agenda& propmat_clearsky_agenda,
                     ConstTensor3View t_field,
                     ConstTensor4View vmr_field,
                     ConstVectorView p_grid,
                     ConstVectorView f_mono) {
  // Initialization
  gas_extinct = 0.;

  const Index Np = p_grid.nelem();

  assert(gas_extinct.nelem() == Np - 1);

  // Local variables to be used in agendas
  Numeric rtp_temperature_local;
  Numeric rtp_pressure_local;
  ArrayOfPropagationMatrix propmat_clearsky_local;
  Vector rtp_vmr_local(vmr_field.nbooks());

  const Vector rtp_temperature_nlte_local_dummy(0);

  // Calculate layer averaged gaseous extinction
  for (Index i = 0; i < Np - 1; i++) {
    rtp_pressure_local = 0.5 * (p_grid[i] + p_grid[i + 1]);
    rtp_temperature_local = 0.5 * (t_field(i, 0, 0) + t_field(i + 1, 0, 0));

    // Average vmrs
    for (Index j = 0; j < vmr_field.nbooks(); j++)
      rtp_vmr_local[j] =
          0.5 * (vmr_field(j, i, 0, 0) + vmr_field(j, i + 1, 0, 0));

    const Vector rtp_mag_dummy(3, 0);
    const Vector ppath_los_dummy;

    //FIXME: do this right?
    ArrayOfStokesVector nlte_dummy;
    // This is right since there should be only clearsky partials
    ArrayOfPropagationMatrix partial_dummy;
    ArrayOfStokesVector partial_source_dummy, partial_nlte_dummy;
    propmat_clearsky_agendaExecute(ws,
                                   propmat_clearsky_local,
                                   nlte_dummy,
                                   partial_dummy,
                                   partial_source_dummy,
                                   partial_nlte_dummy,
                                   ArrayOfRetrievalQuantity(0),
                                   f_mono,  // monochromatic calculation
                                   rtp_mag_dummy,
                                   ppath_los_dummy,
                                   rtp_pressure_local,
                                   rtp_temperature_local,
                                   rtp_temperature_nlte_local_dummy,
                                   rtp_vmr_local,
                                   propmat_clearsky_agenda);

    //Assuming non-polarized light and only one frequency
    if (propmat_clearsky_local.nelem()) {
      gas_extinct[Np - 2 - i] = propmat_clearsky_local[0].Kjj()[0];
      for (Index j = 1; j < propmat_clearsky_local.nelem(); j++) {
        gas_extinct[Np - 2 - i] += propmat_clearsky_local[j].Kjj()[0];
      }
    }
  }
}

//! par_optpropCalc
/*!
  Calculates layer averaged particle extinction and absorption (extinct_matrix
  and emis_vector)). These variables are required as input for the RT4 subroutine.

  \param emis_vector           Layer averaged particle absorption for all particle layers
  \param extinct_matrix        Layer averaged particle extinction for all particle layers
  \param scat_data             as the WSV
  \param scat_za_grid          as the WSV
  \param f_index               index of frequency grid point handeled
  \param pnd_field             as the WSV
  \param t_field               as the WSV
  \param cloudbox_limits       as the WSV 
  \param stokes_dim            as the WSV
  \param nummu                 Number of single hemisphere polar angles.
  
  \author Jana Mendrok
  \date   2016-08-08
*/
void par_optpropCalc(Tensor4View emis_vector,
                     Tensor5View extinct_matrix,
                     //VectorView scatlayers,
                     const ArrayOfArrayOfSingleScatteringData& scat_data,
                     const Vector& scat_za_grid,
                     const Index& f_index,
                     ConstTensor4View pnd_field,
                     ConstTensor3View t_field,
                     const ArrayOfIndex& cloudbox_limits,
                     const Index& stokes_dim,
                     const Index& nummu,
                     const Verbosity& verbosity) {
  // Initialization
  extinct_matrix = 0.;
  emis_vector = 0.;

  const Index N_se = pnd_field.nbooks();
  const Index Np_cloud = pnd_field.npages();

  assert(emis_vector.nbooks() == Np_cloud - 1);
  assert(extinct_matrix.nshelves() == Np_cloud - 1);

  // Local variables to be used in agendas
  ArrayOfStokesVector abs_vec_spt_local(N_se);
  for (auto& sv : abs_vec_spt_local) {
    sv = StokesVector(1, stokes_dim);
    sv.SetZero();
  }

  ArrayOfPropagationMatrix ext_mat_spt_local(N_se);
  for (auto& pm : ext_mat_spt_local) {
    pm = PropagationMatrix(1, stokes_dim);
    pm.SetZero();
  }

  StokesVector abs_vec_local(1, stokes_dim);
  PropagationMatrix ext_mat_local(1, stokes_dim);

  Numeric rtp_temperature_local;
  Tensor4 ext_vector(Np_cloud, 2 * nummu, stokes_dim, stokes_dim, 0.);
  Tensor3 abs_vector(Np_cloud, 2 * nummu, stokes_dim, 0.);
  Vector aa_dummy(1, 0.);

  // Calculate ext_mat and abs_vec for all pressure points in cloudbox
  for (Index scat_p_index_local = 0; scat_p_index_local < Np_cloud;
       scat_p_index_local++) {
    rtp_temperature_local =
        t_field(scat_p_index_local + cloudbox_limits[0], 0, 0);

    for (Index iza = 0; iza < 2 * nummu; iza++) {
      //Calculate optical properties for all individual scattering elements:
      //
      // FIXME: this might be better done PER scatt element
      // why?
      //   - for totally random, we only need to extract for 1 angle
      //   - if T-dimension is 1, we only need to do once for all p-levels
      opt_prop_sptFromScat_data(ext_mat_spt_local,
                                abs_vec_spt_local,
                                scat_data,
                                1,
                                scat_za_grid,
                                aa_dummy,
                                iza,
                                0,
                                f_index,
                                rtp_temperature_local,
                                pnd_field,
                                scat_p_index_local,
                                0,
                                0,
                                verbosity);

      //Calculate bulk optical properties:
      opt_prop_bulkCalc(ext_mat_local,
                        abs_vec_local,
                        ext_mat_spt_local,
                        abs_vec_spt_local,
                        pnd_field,
                        scat_p_index_local,
                        0,
                        0,
                        verbosity);

      ext_mat_local.MatrixAtPosition(
          ext_vector(scat_p_index_local, iza, joker, joker));
      abs_vec_local.VectorAtPosition(
          abs_vector(scat_p_index_local, iza, joker));
    }
  }

  // Calculate layer averaged extinction and absorption
  for (Index scat_p_index_local = 0; scat_p_index_local < Np_cloud - 1;
       scat_p_index_local++) {
    /*
      if ( (ext_vector(scat_p_index_local,0,0,0)+
            ext_vector(scat_p_index_local+1,0,0,0)) > 0. )
        {
          scatlayers[Np_cloud-2-cloudbox_limits[0]-scat_p_index_local] =
            float(scat_p_index_local);
*/
    for (Index imu = 0; imu < nummu; imu++)
      for (Index ist1 = 0; ist1 < stokes_dim; ist1++) {
        for (Index ist2 = 0; ist2 < stokes_dim; ist2++) {
          extinct_matrix(scat_p_index_local, 0, imu, ist2, ist1) =
              .5 * (ext_vector(scat_p_index_local, imu, ist1, ist2) +
                    ext_vector(scat_p_index_local + 1, imu, ist1, ist2));
          extinct_matrix(scat_p_index_local, 1, imu, ist2, ist1) =
              .5 *
              (ext_vector(scat_p_index_local, nummu + imu, ist1, ist2) +
               ext_vector(scat_p_index_local + 1, nummu + imu, ist1, ist2));
        }
        emis_vector(scat_p_index_local, 0, imu, ist1) =
            .5 * (abs_vector(scat_p_index_local, imu, ist1) +
                  abs_vector(scat_p_index_local + 1, imu, ist1));
        emis_vector(scat_p_index_local, 1, imu, ist1) =
            .5 * (abs_vector(scat_p_index_local, nummu + imu, ist1) +
                  abs_vector(scat_p_index_local + 1, nummu + imu, ist1));
      }
    //        }
  }
}

//! par_optpropCalc
/*!
  Calculates layer averaged particle extinction and absorption (extinct_matrix
  and emis_vector)). These variables are required as input for the RT4 subroutine.

  \param emis_vector           Layer averaged particle absorption for all particle layers
  \param extinct_matrix        Layer averaged particle extinction for all particle layers
  \param scat_data             as the WSV
  \param scat_za_grid          as the WSV
  \param f_index               index of frequency grid point handeled
  \param pnd_field             as the WSV
  \param t_field               as the WSV
  \param cloudbox_limits       as the WSV 
  \param stokes_dim            as the WSV
  \param nummu                 Number of single hemisphere polar angles.
  
  \author Jana Mendrok
  \date   2016-08-08
*/
void par_optpropCalc2(Tensor5View emis_vector,
                      Tensor6View extinct_matrix,
                      //VectorView scatlayers,
                      const ArrayOfArrayOfSingleScatteringData& scat_data,
                      const Vector& scat_za_grid,
                      const Index& f_index,
                      ConstTensor4View pnd_field,
                      ConstTensor3View t_field,
                      const ArrayOfIndex& cloudbox_limits,
                      const Index& stokes_dim) {
  // Initialization
  extinct_matrix = 0.;
  emis_vector = 0.;

  const Index Np_cloud = pnd_field.npages();
  const Index nummu = scat_za_grid.nelem() / 2;

  assert(emis_vector.nbooks() == Np_cloud - 1);
  assert(extinct_matrix.nshelves() == Np_cloud - 1);

  // preparing input data
  Vector T_array = t_field(Range(cloudbox_limits[0], Np_cloud), 0, 0);
  Matrix dir_array(scat_za_grid.nelem(), 2, 0.);
  dir_array(joker, 0) = scat_za_grid;

  // making output containers
  ArrayOfArrayOfTensor5 ext_mat_Nse;
  ArrayOfArrayOfTensor4 abs_vec_Nse;
  ArrayOfArrayOfIndex ptypes_Nse;
  Matrix t_ok;
  ArrayOfTensor5 ext_mat_ssbulk;
  ArrayOfTensor4 abs_vec_ssbulk;
  ArrayOfIndex ptype_ssbulk;
  Tensor5 ext_mat_bulk;
  Tensor4 abs_vec_bulk;
  Index ptype_bulk;

  opt_prop_NScatElems(ext_mat_Nse,
                      abs_vec_Nse,
                      ptypes_Nse,
                      t_ok,
                      scat_data,
                      stokes_dim,
                      T_array,
                      dir_array,
                      f_index);
  opt_prop_ScatSpecBulk(ext_mat_ssbulk,
                        abs_vec_ssbulk,
                        ptype_ssbulk,
                        ext_mat_Nse,
                        abs_vec_Nse,
                        ptypes_Nse,
                        pnd_field(joker, joker, 0, 0),
                        t_ok);
  opt_prop_Bulk(ext_mat_bulk,
                abs_vec_bulk,
                ptype_bulk,
                ext_mat_ssbulk,
                abs_vec_ssbulk,
                ptype_ssbulk);

  // Calculate layer averaged extinction and absorption and sort into RT4-format
  // data tensors
  for (Index ipc = 0; ipc < Np_cloud - 1; ipc++) {
    /*
    if ( (ext_mat_bulk(0,ipc,0,0,0)+
          ext_mat_bulk(0,ipc+1,0,0,0)) > 0. )
    {
      scatlayers[Np_cloud-2-cloudbox_limits[0]-ipc] = float(ipc);
*/
    for (Index fi = 0; fi < abs_vec_bulk.nbooks(); fi++)
      for (Index imu = 0; imu < nummu; imu++)
        for (Index ist1 = 0; ist1 < stokes_dim; ist1++) {
          for (Index ist2 = 0; ist2 < stokes_dim; ist2++) {
            extinct_matrix(fi, ipc, 0, imu, ist2, ist1) =
                .5 * (ext_mat_bulk(fi, ipc, imu, ist1, ist2) +
                      ext_mat_bulk(fi, ipc + 1, imu, ist1, ist2));
            extinct_matrix(fi, ipc, 1, imu, ist2, ist1) =
                .5 * (ext_mat_bulk(fi, ipc, nummu + imu, ist1, ist2) +
                      ext_mat_bulk(fi, ipc + 1, nummu + imu, ist1, ist2));
          }
          emis_vector(fi, ipc, 0, imu, ist1) =
              .5 * (abs_vec_bulk(fi, ipc, imu, ist1) +
                    abs_vec_bulk(fi, ipc + 1, imu, ist1));
          emis_vector(fi, ipc, 1, imu, ist1) =
              .5 * (abs_vec_bulk(fi, ipc, nummu + imu, ist1) +
                    abs_vec_bulk(fi, ipc + 1, nummu + imu, ist1));
        }
    //    }
  }
}

//! sca_optpropCalc
/*!
  Calculates layer (and azimuthal) averaged phase matrix (scatter_matrix). This
  variable is required as input for the RT4 subroutine.

  \param scatter_matrix        Layer averaged scattering matrix (azimuth mode 0) for all particle layers
  \param pfct_failed           Flag whether norm of scatter_matrix is sufficiently accurate
  \param emis_vector           Layer averaged particle absorption for all particle layers
  \param extinct_matrix        Layer averaged particle extinction for all particle layers
  \param f_index               as the WSV
  \param scat_data             as the (new-type, f_grid prepared) WSV
  \param pnd_field             as the WSV
  \param stokes_dim            as the WSV
  \param scat_za_grid          as the WSV
  \param quad_weights          Quadrature weights associated with scat_za_grid 
  \param pfct_method           Method for scattering matrix temperature dependance handling
  \param pfct_aa_grid_size     Number of azimuthal grid points in Fourier series decomposition of randomly oriented particles
  \param pfct_threshold        Requested scatter_matrix norm accuracy (in terms of single scat albedo)
  \param auto_inc_nstreams     as the WSV
  
  \author Jana Mendrok
  \date   2016-08-08
*/
void sca_optpropCalc(  //Output
    Tensor6View scatter_matrix,
    Index& pfct_failed,
    //Input
    ConstTensor4View emis_vector,
    ConstTensor5View extinct_matrix,
    const Index& f_index,
    const ArrayOfArrayOfSingleScatteringData& scat_data,
    ConstTensor4View pnd_field,
    const Index& stokes_dim,
    const Vector& scat_za_grid,
    ConstVectorView quad_weights,
    const String& pfct_method,
    const Index& pfct_aa_grid_size,
    const Numeric& pfct_threshold,
    const Index& auto_inc_nstreams,
    const Verbosity& verbosity) {
  // FIXME: this whole funtions needs revision/optimization regarding
  // - temperature dependence (using new-type scat_data)
  // - using redundancies in sca_mat data at least for totally random
  //   orientation particles (are there some for az. random, too? like
  //   upper/lower hemisphere equiv?) - we might at least have a flag
  //   (transported down from calling function?) whether we deal exclusively
  //   with totally random orient particles (and then take some shortcuts...)

  // FIXME: do we have numerical issues, too, here in case of tiny pnd? check
  // with Patrick's Disort-issue case.

  // Initialization
  scatter_matrix = 0.;

  const Index N_se = pnd_field.nbooks();
  const Index Np_cloud = pnd_field.npages();
  const Index nza_rt = scat_za_grid.nelem();

  assert(scatter_matrix.nvitrines() == Np_cloud - 1);

  // Check that total number of scattering elements in scat_data and pnd_field
  // agree.
  // FIXME: this should be done earlier. in calling method. outside freq- and
  // other possible loops. rather assert than runtime error.
  if (TotalNumberOfElements(scat_data) != N_se) {
    ostringstream os;
    os << "Total number of scattering elements in scat_data ("
       << TotalNumberOfElements(scat_data) << ") and pnd_field (" << N_se
       << ") disagree.";
    throw runtime_error(os.str());
  }

  if (pfct_aa_grid_size < 2) {
    ostringstream os;
    os << "Azimuth grid size for scatt matrix extraction"
       << " (*pfct_aa_grid_size*) must be >1.\n"
       << "Yours is " << pfct_aa_grid_size << ".\n";
    throw runtime_error(os.str());
  }
  Vector aa_grid;
  nlinspace(aa_grid, 0, 180, pfct_aa_grid_size);

  Index i_se_flat = 0;
  Tensor5 sca_mat(N_se, nza_rt, nza_rt, stokes_dim, stokes_dim, 0.);
  Matrix ext_fixT_spt(N_se, nza_rt, 0.), abs_fixT_spt(N_se, nza_rt, 0.);

  // Precalculate azimuth integration weights for totally randomly oriented
  // (they are only determined by pfct_aa_grid_size)
  Numeric daa_totrand =
      1. / float(pfct_aa_grid_size - 1);  // 2*180./360./(pfct_aa_grid_size-1)

  // first we extract Z at one T, integrate the azimuth data at each
  // za_inc/za_sca combi (to get the Fourier series 0.th mode), then interpolate
  // to the mu/mu' combis we need in RT.
  //
  // FIXME: are the stokes component assignments correct? for totally random?
  // and azimuthally random? (as they are done differently...)
  for (Index i_ss = 0; i_ss < scat_data.nelem(); i_ss++) {
    for (Index i_se = 0; i_se < scat_data[i_ss].nelem(); i_se++) {
      SingleScatteringData ssd = scat_data[i_ss][i_se];
      Index this_f_index = (ssd.pha_mat_data.nlibraries() == 1 ? 0 : f_index);
      Index i_pfct;
      if (pfct_method == "low")
        i_pfct = 0;
      else if (pfct_method == "high")
        i_pfct = ssd.T_grid.nelem() - 1;
      else  //if( pfct_method=="median" )
        i_pfct = ssd.T_grid.nelem() / 2;

      if (ssd.ptype == PTYPE_TOTAL_RND) {
        Matrix pha_mat(stokes_dim, stokes_dim, 0.);
        for (Index iza = 0; iza < nza_rt; iza++) {
          for (Index sza = 0; sza < nza_rt; sza++) {
            Matrix pha_mat_int(stokes_dim, stokes_dim, 0.);
            for (Index saa = 0; saa < pfct_aa_grid_size; saa++) {
              pha_matTransform(
                  pha_mat(joker, joker),
                  ssd.pha_mat_data(
                      this_f_index, i_pfct, joker, joker, joker, joker, joker),
                  ssd.za_grid,
                  ssd.aa_grid,
                  ssd.ptype,
                  sza,
                  saa,
                  iza,
                  0,
                  scat_za_grid,
                  aa_grid,
                  verbosity);

              if (saa == 0 || saa == pfct_aa_grid_size - 1)
                pha_mat *= (daa_totrand / 2.);
              else
                pha_mat *= daa_totrand;
              pha_mat_int += pha_mat;
            }
            sca_mat(i_se_flat, iza, sza, joker, joker) = pha_mat_int;
          }
          ext_fixT_spt(i_se_flat, iza) =
              ssd.ext_mat_data(this_f_index, i_pfct, 0, 0, 0);
          abs_fixT_spt(i_se_flat, iza) =
              ssd.abs_vec_data(this_f_index, i_pfct, 0, 0, 0);
        }
      } else if (ssd.ptype == PTYPE_AZIMUTH_RND) {
        Index nza_se = ssd.za_grid.nelem();
        Index naa_se = ssd.aa_grid.nelem();
        Tensor4 pha_mat_int(nza_se, nza_se, stokes_dim, stokes_dim, 0.);
        ConstVectorView za_datagrid = ssd.za_grid;
        ConstVectorView aa_datagrid = ssd.aa_grid;
        assert(aa_datagrid[0] == 0.);
        assert(aa_datagrid[naa_se - 1] == 180.);
        Vector daa(naa_se);

        // Precalculate azimuth integration weights for this azimuthally
        // randomly oriented scat element
        // (need to do this per scat element as ssd.aa_grid is scat
        //  element specific (might change between elements) and need to do
        //  this on actual grid instead of grid number since the grid can,
        //  at least theoretically be non-equidistant)
        daa[0] = (aa_datagrid[1] - aa_datagrid[0]) / 360.;
        for (Index saa = 1; saa < naa_se - 1; saa++)
          daa[saa] = (aa_datagrid[saa + 1] - aa_datagrid[saa - 1]) / 360.;
        daa[naa_se - 1] =
            (aa_datagrid[naa_se - 1] - aa_datagrid[naa_se - 2]) / 360.;

        // first, extracting the phase matrix at the scatt elements own
        // polar angle grid, deriving their respective azimuthal (Fourier
        // series) 0-mode
        for (Index iza = 0; iza < nza_se; iza++)
          for (Index sza = 0; sza < nza_se; sza++) {
            for (Index saa = 0; saa < naa_se; saa++) {
              for (Index ist1 = 0; ist1 < stokes_dim; ist1++)
                for (Index ist2 = 0; ist2 < stokes_dim; ist2++)
                  pha_mat_int(sza, iza, ist1, ist2) +=
                      daa[saa] * ssd.pha_mat_data(this_f_index,
                                                  i_pfct,
                                                  sza,
                                                  saa,
                                                  iza,
                                                  0,
                                                  ist1 * 4 + ist2);
            }
          }

        // second, interpolating the extracted azimuthal mode to the RT4
        // solver polar angles
        for (Index iza = 0; iza < nza_rt; iza++) {
          for (Index sza = 0; sza < nza_rt; sza++) {
            GridPos za_sca_gp;
            GridPos za_inc_gp;
            Vector itw(4);
            Matrix pha_mat_lab(stokes_dim, stokes_dim, 0.);
            Numeric za_sca = scat_za_grid[sza];
            Numeric za_inc = scat_za_grid[iza];

            gridpos(za_inc_gp, za_datagrid, za_inc);
            gridpos(za_sca_gp, za_datagrid, za_sca);
            interpweights(itw, za_sca_gp, za_inc_gp);

            for (Index ist1 = 0; ist1 < stokes_dim; ist1++)
              for (Index ist2 = 0; ist2 < stokes_dim; ist2++) {
                pha_mat_lab(ist1, ist2) =
                    interp(itw,
                           pha_mat_int(Range(joker), Range(joker), ist1, ist2),
                           za_sca_gp,
                           za_inc_gp);
                //if (ist1+ist2==1)
                //  pha_mat_lab(ist1,ist2) *= -1.;
              }

            sca_mat(i_se_flat, iza, sza, joker, joker) = pha_mat_lab;
          }
          ext_fixT_spt(i_se_flat, iza) =
              ssd.ext_mat_data(this_f_index, i_pfct, iza, 0, 0);
          abs_fixT_spt(i_se_flat, iza) =
              ssd.abs_vec_data(this_f_index, i_pfct, iza, 0, 0);
        }
      } else {
        ostringstream os;
        os << "Unsuitable particle type encountered.";
        throw runtime_error(os.str());
      }
      i_se_flat++;
    }
  }

  assert(i_se_flat == N_se);
  // now we sum up the Z(mu,mu') over the scattering elements weighted by the
  // pnd_field data, deriving Z(z,mu,mu') and sorting this into
  // scatter_matrix
  // FIXME: it seems that at least for totally random, corresponding upward and
  // downward directions have exactly the same (0,0) elements. Can we use this
  // to reduce calc efforts (for sca and its normalization as well as for ext
  // and abs?)
  Index nummu = nza_rt / 2;
  for (Index scat_p_index_local = 0; scat_p_index_local < Np_cloud - 1;
       scat_p_index_local++) {
    Vector ext_fixT(nza_rt, 0.), abs_fixT(nza_rt, 0.);

    for (Index i_se = 0; i_se < N_se; i_se++) {
      Numeric pnd_mean = 0.5 * (pnd_field(i_se, scat_p_index_local + 1, 0, 0) +
                                pnd_field(i_se, scat_p_index_local, 0, 0));
      if (pnd_mean != 0.)
        for (Index iza = 0; iza < nummu; iza++) {
          // JM171003: not clear to me anymore why this check. if pnd_mean
          // is non-zero, then extinction should also be non-zero by
          // default?
          //if ( (extinct_matrix(scat_p_index_local,0,iza,0,0)+
          //      extinct_matrix(scat_p_index_local,1,iza,0,0)) > 0. )
          for (Index sza = 0; sza < nummu; sza++) {
            for (Index ist1 = 0; ist1 < stokes_dim; ist1++)
              for (Index ist2 = 0; ist2 < stokes_dim; ist2++) {
                // we can't use stokes_dim jokers here since '*' doesn't
                // exist for Num*MatView. Also, order of stokes matrix
                // dimensions is inverted here (aka scat matrix is
                // transposed).
                scatter_matrix(scat_p_index_local, 0, iza, ist2, sza, ist1) +=
                    pnd_mean * sca_mat(i_se, iza, sza, ist1, ist2);
                scatter_matrix(scat_p_index_local, 1, iza, ist2, sza, ist1) +=
                    pnd_mean * sca_mat(i_se, nummu + iza, sza, ist1, ist2);
                scatter_matrix(scat_p_index_local, 2, iza, ist2, sza, ist1) +=
                    pnd_mean * sca_mat(i_se, iza, nummu + sza, ist1, ist2);
                scatter_matrix(scat_p_index_local, 3, iza, ist2, sza, ist1) +=
                    pnd_mean *
                    sca_mat(i_se, nummu + iza, nummu + sza, ist1, ist2);
              }
          }

          ext_fixT[iza] += pnd_mean * ext_fixT_spt(i_se, iza);
          abs_fixT[iza] += pnd_mean * abs_fixT_spt(i_se, iza);
        }
    }

    for (Index iza = 0; iza < nummu; iza++) {
      for (Index ih = 0; ih < 2; ih++) {
        if (extinct_matrix(scat_p_index_local, ih, iza, 0, 0) > 0.) {
          Numeric sca_mat_integ = 0.;

          // We need to calculate the nominal values for the fixed T, we
          // used above in the pha_mat extraction. Only this tells us whether
          // angular resolution is sufficient.
          //
          //Numeric ext_nom = extinct_matrix(scat_p_index_local,ih,iza,0,0);
          //Numeric sca_nom = ext_nom-emis_vector(scat_p_index_local,ih,iza,0);
          Numeric ext_nom = ext_fixT[iza];
          Numeric sca_nom = ext_nom - abs_fixT[iza];
          Numeric w0_nom = sca_nom / ext_nom;
          assert(w0_nom >= 0.);

          for (Index sza = 0; sza < nummu; sza++) {
            //                SUM2 = SUM2 + 2.0D0*PI*QUAD_WEIGHTS(J2)*
            //     .                 ( SCATTER_MATRIX(1,J2,1,J1, L,TSL)
            //     .                 + SCATTER_MATRIX(1,J2,1,J1, L+2,TSL) )
            sca_mat_integ +=
                quad_weights[sza] *
                (scatter_matrix(scat_p_index_local, ih, iza, 0, sza, 0) +
                 scatter_matrix(scat_p_index_local, ih + 2, iza, 0, sza, 0));
          }

          // compare integrated scatt matrix with ext-abs for respective
          // incident polar angle - consistently with scat_dataCheck, we do
          // this in terms of albedo deviation (since PFCT deviations at
          // small albedos matter less than those at high albedos)
          //            SUM1 = EMIS_VECTOR(1,J1,L,TSL)-EXTINCT_MATRIX(1,1,J1,L,TSL)
          Numeric w0_act = 2. * PI * sca_mat_integ / ext_nom;
          Numeric pfct_norm = 2. * PI * sca_mat_integ / sca_nom;
          /*
              cout << "sca_mat norm deviates " << 1e2*abs(1.-pfct_norm) << "%"
                   << " (" << abs(w0_act-w0_nom) << " in albedo).\n";
              */
          Numeric sca_nom_paropt =
              extinct_matrix(scat_p_index_local, ih, iza, 0, 0) -
              emis_vector(scat_p_index_local, ih, iza, 0);
          //Numeric w0_nom_paropt = sca_nom_paropt /
          //  extinct_matrix(scat_p_index_local,ih,iza,0,0);
          /*
              cout << "scat_p=" << scat_p_index_local
                   << ", iza=" << iza << ", hem=" << ih << "\n";
              cout << "  scaopt (paropt) w0_act= " << w0_act
                   << ", w0_nom = " << w0_nom
                   << " (" << w0_nom_paropt
                   << "), diff=" << w0_act-w0_nom
                   << " (" << w0_nom-w0_nom_paropt << ").\n";
              */

          if (abs(w0_act - w0_nom) > pfct_threshold) {
            if (pfct_failed >= 0) {
              if (auto_inc_nstreams) {
                pfct_failed = 1;
                return;
              } else {
                ostringstream os;
                os << "Bulk scattering matrix normalization deviates significantly\n"
                   << "from expected value (" << 1e2 * abs(1. - pfct_norm)
                   << "%,"
                   << " resulting in albedo deviation of "
                   << abs(w0_act - w0_nom) << ").\n"
                   << "Something seems wrong with your scattering data "
                   << " (did you run *scat_dataCheck*?)\n"
                   << "or your RT4 setup (try increasing *nstreams* and in case"
                   << " of randomly oriented particles possibly also"
                   << " pfct_aa_grid_size).";
                throw runtime_error(os.str());
              }
            }
          } else if (abs(w0_act - w0_nom) > pfct_threshold * 0.1 ||
                     abs(1. - pfct_norm) > 1e-2) {
            CREATE_OUT2;
            out2
                << "Warning: The bulk scattering matrix is not well normalized\n"
                << "Deviating from expected value by "
                << 1e2 * abs(1. - pfct_norm) << "% (and "
                << abs(w0_act - w0_nom) << " in terms of scattering albedo).\n";
          }
          // rescale scattering matrix to expected (0,0) value (and scale all
          // other elements accordingly)
          //
          // However, here we should not use the pfct_norm based on the
          // deviation from the fixed-temperature ext and abs. Instead, for
          // energy conservation reasons, this needs to be consistent with
          // extinct_matrix and emis_vector. Hence, recalculate pfct_norm
          // from them.
          //cout << "  scaopt (paropt) pfct_norm dev = " << 1e2*pfct_norm-1e2;
          pfct_norm = 2. * PI * sca_mat_integ / sca_nom_paropt;
          //cout << " (" << 1e2*pfct_norm-1e2 << ")%.\n";
          //
          // FIXME: not fully clear whether applying the same rescaling
          // factor to all stokes elements is the correct way to do. check
          // out, e.g., Vasilieva (JQSRT 2006) for better approaches.
          // FIXME: rather rescale Z(0,0) only as we should have less issues
          // for other Z elements as they are less peaked/featured.
          scatter_matrix(scat_p_index_local, ih, iza, joker, joker, joker) /=
              pfct_norm;
          scatter_matrix(
              scat_p_index_local, ih + 2, iza, joker, joker, joker) /=
              pfct_norm;
          //if (scat_p_index_local==49)
        }
      }
    }
  }
}

//! surf_optpropCalc
/*!
  Calculates bidirectional surface reflection matrices and emission direction
  dependent surface emission terms as required as input for the RT4 subroutine.

  \param ws                    Current workspace
  \param surf_refl_mat         Bidirectional surface reflection matrices on RT4 stream directions.
  \param surf_emis_vec         Directional surface emission vector on RT4 stream directions.
  \param surface_rtprop_agenda as the WSA
  \param f_grid                as the WSV
  \param scat_za_grid          as the WSV
  \param mu_values             Cosines of scat_za_grid angles.
  \param quad_weights          Quadrature weights associated with mu_values.
  \param stokes_dim            as the WSV
  \param surf_alt              Surface altitude.
  
  \author Jana Mendrok
  \date   2017-02-09
*/
void surf_optpropCalc(Workspace& ws,
                      //Output
                      Tensor5View surf_refl_mat,
                      Tensor3View surf_emis_vec,
                      //Input
                      const Agenda& surface_rtprop_agenda,
                      ConstVectorView f_grid,
                      ConstVectorView scat_za_grid,
                      ConstVectorView mu_values,
                      ConstVectorView quad_weights,
                      const Index& stokes_dim,
                      const Numeric& surf_alt) {
  // While proprietary RT4 - from the input/user control side - handles only
  // Lambertian and Fresnel, the Doubling&Adding solver core applies a surface
  // reflection matrix and a surface radiance term. The reflection matrix is
  // dependent on incident and reflection direction, ie forms a discrete
  // representation of the bidirectional reflection function; the radiance term
  // is dependent on outgoing polar angle. That is, the solver core can
  // basically handle ANY kind of surface reflection type.
  //
  // Here, we replace RT4's proprietary surface reflection and radiance term
  // calculation and use ARTS' surface_rtprop_agenda instead to set these
  // variables up.
  // That is, ARTS-RT4 is set up such that it can handle all surface reflection
  // types that ARTS itself can handle.
  // What is required here, is to derive the reflection matrix over all incident
  // and reflected polar angle directions. The surface_rtprop_agenda handles one
  // reflected direction at a time (rtp_los); it is sufficient here to simply
  // loop over all reflected angles as given by the stream directions. However,
  // the incident directions (surface_los) provided by surface_rtprop_agenda
  // depends on the specific, user-defined setup of the agenda. Out of what the
  // agenda call provides, we have to derive the reflection matrix entries for
  // the incident directions as defined by the RT4 stream directions. To be
  // completely general (handling everything from specular via semi-specular to
  // Lambertian) is a bit tricky. Here we decided to allow the ARTS-standard
  // 0.5-grid-spacing extrapolation and set everything outside that range to
  // zero.
  //
  // We do all frequencies here at once (assuming this is the faster variant as
  // the agenda anyway (can) provide output for full f_grid at once and as we
  // have to apply the same inter/extrapolation to all the frequencies).
  //
  // FIXME: Allow surface to be elsewhere than at lowest atm level (this
  // requires changes in the surface setting part and more extensive ones in the
  // atmospheric optical property prep part within the frequency loop further
  // below).

  chk_not_empty("surface_rtprop_agenda", surface_rtprop_agenda);

  const Index nf = f_grid.nelem();
  const Index nummu = scat_za_grid.nelem() / 2;
  const String B_unit = "R";

  // Local input of surface_rtprop_agenda.
  Vector rtp_pos(1, surf_alt);  //atmosphere_dim is 1

  for (Index rmu = 0; rmu < nummu; rmu++) {
    // Local output of surface_rtprop_agenda.
    Numeric surface_skin_t;
    Matrix surface_los;
    Tensor4 surface_rmatrix;
    Matrix surface_emission;

    // rtp_los is reflected direction, ie upwelling direction, which is >90deg
    // in ARTS. scat_za_grid is sorted here as downwelling (90->0) in 1st
    // half, upwelling (90->180) in second half. that is, here we have to take
    // the second half grid or, alternatively, use 180deg-za[imu].
    Vector rtp_los(1, scat_za_grid[nummu + rmu]);

    surface_rtprop_agendaExecute(ws,
                                 surface_skin_t,
                                 surface_emission,
                                 surface_los,
                                 surface_rmatrix,
                                 f_grid,
                                 rtp_pos,
                                 rtp_los,
                                 surface_rtprop_agenda);
    Index nsl = surface_los.nrows();
    assert(surface_los.ncols() == 1 || nsl == 0);

    // ARTS' surface_emission is equivalent to RT4's gnd_radiance (here:
    // surf_emis_vec) except for ARTS using Planck in terms of frequency,
    // while RT4 uses it in terms of wavelengths.
    // To derive gnd_radiance, we can either use ARTS surface_emission and
    // rescale it by B_lambda(surface_skin_t)/B_freq(surface_skin_t). Or we
    // can create it from surface_rmatrix and B_lambda(surface_skin_t). The
    // latter is more direct regarding use of B(T), but is requires
    // (re-)deriving diffuse reflectivity stokes components (which should be
    // the sum of the incident polar angle dependent reflectivities. so, that
    // might ensure better consistency. but then we should calculate
    // gnd_radiance only after surface_los to scat_za_grid conversion of the
    // reflection matrix). For now, we use the rescaling approach.
    for (Index f_index = 0; f_index < nf; f_index++) {
      Numeric freq = f_grid[f_index];
      Numeric B_freq = planck(freq, surface_skin_t);
      Numeric B_lambda;
      Numeric wave = 1e6 * SPEED_OF_LIGHT / freq;
      planck_function_(surface_skin_t, B_unit.c_str(), wave, B_lambda);
      Numeric B_ratio = B_lambda / B_freq;
      surf_emis_vec(f_index, rmu, joker) = surface_emission(f_index, joker);
      surf_emis_vec(f_index, rmu, joker) *= B_ratio;
    }

    // now we have to properly fill the RT4 stream directions in incident
    // angle dimension of the reflection matrix for RT4 with the agenda
    // output, which is given on/limited to surface_los. Only values inside
    // the (default) extrapolation range are filled; the rest is left as 0.
    // we do this for each incident stream separately as we need to check them
    // separately whether they are within allowed inter/extrapolation range
    // (ARTS can't do this for all elements of a vector at once. it will fail
    // for the whole vector if there's a single value outside.).

    // as we are rescaling surface_rmatrix within here, we need to keep its
    // original normalization for later renormalization. Create the container
    // here, fill in below.
    Vector R_arts(f_grid.nelem(), 0.);

    if (nsl > 1)  // non-blackbody, non-specular reflection
    {
      for (Index f_index = 0; f_index < nf; f_index++)
        R_arts[f_index] = surface_rmatrix(joker, f_index, 0, 0).sum();

      // Determine angle range weights in surface_rmatrix and de-scale
      // surface_rmatrix with those.
      Vector surf_int_grid(nsl + 1);
      surf_int_grid[0] =
          surface_los(0, 0) - 0.5 * (surface_los(1, 0) - surface_los(0, 0));
      surf_int_grid[nsl] =
          surface_los(nsl - 1, 0) +
          0.5 * (surface_los(nsl - 1, 0) - surface_los(nsl - 2, 0));
      for (Index imu = 1; imu < nsl; imu++)
        surf_int_grid[imu] =
            0.5 * (surface_los(imu - 1, 0) + surface_los(imu, 0));
      surf_int_grid *= DEG2RAD;
      for (Index imu = 0; imu < nsl; imu++) {
        //Numeric coslow = cos(2.*surf_int_grid[imu]);
        //Numeric coshigh = cos(2.*surf_int_grid[imu+1]);
        //Numeric w = 0.5*(coslow-coshigh);
        Numeric w = 0.5 * (cos(2. * surf_int_grid[imu]) -
                           cos(2. * surf_int_grid[imu + 1]));
        surface_rmatrix(imu, joker, joker, joker) /= w;
      }

      // Testing: interpolation in cos(za)
      //Vector mu_surf_los(nsl);
      //for (Index imu=0; imu<nsl; imu++)
      //  mu_surf_los[imu] = cos(surface_los(imu,0)*DEG2RAD);

      for (Index imu = 0; imu < nummu; imu++) {
        try {
          GridPos gp_za;
          gridpos(gp_za, surface_los(joker, 0), scat_za_grid[imu]);
          // Testing: interpolation in cos(za)
          //gridpos( gp_za, mu_surf_los, mu_values[imu] );
          Vector itw(2);
          interpweights(itw, gp_za);

          // now apply the interpolation weights. since we're not in
          // python, we can't do the whole tensor at once, but need to
          // loop over the dimensions.
          for (Index f_index = 0; f_index < nf; f_index++)
            for (Index sto1 = 0; sto1 < stokes_dim; sto1++)
              for (Index sto2 = 0; sto2 < stokes_dim; sto2++)
                surf_refl_mat(f_index, imu, sto2, rmu, sto1) = interp(
                    itw, surface_rmatrix(joker, f_index, sto1, sto2), gp_za);
          // Apply new angle range weights - as this is for RT4, we apply
          // the actual RT4 angle (aka quadrature) weights:
          surf_refl_mat(joker, imu, joker, rmu, joker) *=
              (quad_weights[imu] * mu_values[imu]);
        } catch (const runtime_error& e) {
          // nothing to do here. we just leave the reflection matrix
          // entry at the 0.0 it was initialized with.
        }
      }
    } else if (nsl > 0)  // specular reflection
                         // no interpolation, no angle weight rescaling,
                         // just setting diagonal elements of surf_refl_mat.
    {
      for (Index f_index = 0; f_index < nf; f_index++)
        R_arts[f_index] = surface_rmatrix(joker, f_index, 0, 0).sum();

      // surface_los angle should be identical to
      // 180. - (rtp_los=scat_za_grid[nummu+rmu]=180.-scat_za_grid[rmu])
      // = scat_za_grid[rmu].
      // check that, and if so, sort values into refmat(rmu,rmu).
      assert(
          is_same_within_epsilon(surface_los(0, 0), scat_za_grid[rmu], 1e-12));
      for (Index f_index = 0; f_index < nf; f_index++)
        for (Index sto1 = 0; sto1 < stokes_dim; sto1++)
          for (Index sto2 = 0; sto2 < stokes_dim; sto2++)
            surf_refl_mat(f_index, rmu, sto2, rmu, sto1) =
                surface_rmatrix(0, f_index, sto1, sto2);
    }
    //else {} // explicit blackbody
    // all surf_refl_mat elements to remain at 0., ie nothing to do.

    //eventually make sure the scaling of surf_refl_mat is correct
    for (Index f_index = 0; f_index < nf; f_index++) {
      Numeric R_scale = 1.;
      Numeric R_rt4 = surf_refl_mat(f_index, joker, 0, rmu, 0).sum();
      if (R_rt4 == 0.) {
        if (R_arts[f_index] != 0.) {
          ostringstream os;
          os << "Something went wrong.\n"
             << "At reflected stream #" << rmu
             << ", power reflection coefficient for RT4\n"
             << "became 0, although the one from surface_rtprop_agenda is "
             << R_arts << ".\n";
          throw runtime_error(os.str());
        }
      } else {
        R_scale = R_arts[f_index] / R_rt4;
        surf_refl_mat(f_index, joker, joker, rmu, joker) *= R_scale;
      }
    }
  }
}

//! Calculate radiation field using RT4
/*! 
  Calculate radiation field using Evans' RT4 model (part of PolRadTran).

  This is a direct interface to the (almost orignal) RT4 FORTRAN code. No checks
  of input are made. Function is only to be called through other
  functions/methods, which have to ensure input consistency.

  \param[out] name            descript
  \param[in]  name            descript [unit]

  \author Jana Mendrok
  \date 2016-05-24
*/
void rt4_test(Tensor4& out_rad,
              const String& datapath,
              const Verbosity& verbosity) {
  //emissivity.resize(4);
  //reflectivity.resize(4);

  Index nstokes = 2;
  Index nummu = 8;
  Index nuummu = 0;
  Numeric max_delta_tau = 1.0E-6;
  String quad_type = "L";
  Numeric ground_temp = 300.;
  String ground_type = "L";
  Numeric ground_albedo = 0.05;
  Complex ground_index;
  Numeric sky_temp = 0.;
  Numeric wavelength = 880.;
  //Index noutlevels=1;
  //ArrayOfIndex outlevels(1);
  //outlevels[0]=1;

  Vector height, temperatures, gas_extinct;
  Tensor5 sca_data;
  Tensor4 ext_data;
  Tensor3 abs_data;
  ReadXML(height, "height", datapath + "z.xml", "", verbosity);
  ReadXML(temperatures, "temperatures", datapath + "T.xml", "", verbosity);
  ReadXML(gas_extinct, "gas_extinct", datapath + "abs_gas.xml", "", verbosity);
  ReadXML(abs_data, "abs_data", datapath + "abs_par.xml", "", verbosity);
  ReadXML(ext_data, "ext_data", datapath + "ext_par.xml", "", verbosity);
  ReadXML(sca_data, "sca_data", datapath + "sca_par.xml", "", verbosity);
  Index num_layers = height.nelem() - 1;
  Index num_scatlayers = 3;
  Vector scatlayers(num_layers, 0.);
  scatlayers[3] = 1.;
  scatlayers[4] = 2.;
  scatlayers[5] = 3.;

  // the read in sca/ext/abs_data is the complete set (and it's in the wrong
  // order for passing it directly to radtrano). before handing over to
  // fortran, we need to reduce it to the number of stokes elements to be
  // used. we can't use views here as all data needs to be continuous in
  // memory; that is, we have to explicitly copy the data we need.
  Tensor6 scatter_matrix(num_scatlayers, 4, nummu, nstokes, nummu, nstokes);
  for (Index ii = 0; ii < 4; ii++)
    for (Index ij = 0; ij < nummu; ij++)
      for (Index ik = 0; ik < nstokes; ik++)
        for (Index il = 0; il < nummu; il++)
          for (Index im = 0; im < nstokes; im++)
            for (Index in = 0; in < num_scatlayers; in++)
              scatter_matrix(in, ii, ij, ik, il, im) =
                  sca_data(im, il, ik, ij, ii);
  Tensor5 extinct_matrix(num_scatlayers, 2, nummu, nstokes, nstokes);
  for (Index ii = 0; ii < 2; ii++)
    for (Index ij = 0; ij < nummu; ij++)
      for (Index ik = 0; ik < nstokes; ik++)
        for (Index il = 0; il < nstokes; il++)
          for (Index im = 0; im < num_scatlayers; im++)
            extinct_matrix(im, ii, ij, ik, il) = ext_data(il, ik, ij, ii);
  Tensor4 emis_vector(num_scatlayers, 2, nummu, nstokes);
  for (Index ii = 0; ii < 2; ii++)
    for (Index ij = 0; ij < nummu; ij++)
      for (Index ik = 0; ik < nstokes; ik++)
        for (Index il = 0; il < num_scatlayers; il++)
          emis_vector(il, ii, ij, ik) = abs_data(ik, ij, ii);

  // dummy parameters necessary due to modified, flexible surface handling
  Tensor4 surf_refl_mat(nummu, nstokes, nummu, nstokes, 0.);
  Matrix surf_emis_vec(nummu, nstokes, 0.);
  Matrix ground_reflec(nstokes, nstokes, 0.);

  // Output variables
  Vector mu_values(nummu);
  Tensor3 up_rad(num_layers + 1, nummu, nstokes, 0.);
  Tensor3 down_rad(num_layers + 1, nummu, nstokes, 0.);

  radtrano_(nstokes,
            nummu,
            nuummu,
            max_delta_tau,
            quad_type.c_str(),
            ground_temp,
            ground_type.c_str(),
            ground_albedo,
            ground_index,
            ground_reflec.get_c_array(),
            surf_refl_mat.get_c_array(),
            surf_emis_vec.get_c_array(),
            sky_temp,
            wavelength,
            num_layers,
            height.get_c_array(),
            temperatures.get_c_array(),
            gas_extinct.get_c_array(),
            num_scatlayers,
            scatlayers.get_c_array(),
            extinct_matrix.get_c_array(),
            emis_vector.get_c_array(),
            scatter_matrix.get_c_array(),
            //noutlevels,
            //outlevels.get_c_array(),
            mu_values.get_c_array(),
            up_rad.get_c_array(),
            down_rad.get_c_array());

  //so far, output is in
  //    units W/m^2 um sr
  //    dimensions [numlayers+1,nummu,nstokes]
  //    sorted from high to low (altitudes) and 0 to |1| (mu)
  //WriteXML( "ascii", up_rad, "up_rad.xml", 0, "up_rad", "", "", verbosity );
  //WriteXML( "ascii", down_rad, "down_rad.xml", 0, "down_rad", "", "", verbosity );

  //to be able to compare with RT4 reference results, reshape output into
  //RT4-output type table (specifically, resort up_rad such that it runs from
  //zenith welling to horizontal, thus forms a continuous angle grid with
  //down_rad. if later changing up_rad/down_rad sorting such that it is in
  //line with doit_i_field, then this has to be adapted as well...
  out_rad.resize(num_layers + 1, 2, nummu, nstokes);
  for (Index ii = 0; ii < nummu; ii++)
    out_rad(joker, 0, ii, joker) = up_rad(joker, nummu - 1 - ii, joker);
  //out_rad(joker,0,joker,joker) = up_rad;
  out_rad(joker, 1, joker, joker) = down_rad;
  //WriteXML( "ascii", out_rad, "out_rad.xml", 0, "out_rad", "", "", verbosity );
}

#endif /* ENABLE_RT4 */
