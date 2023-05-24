// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#include <MiniTensor.h>
#include <MiniTensor_Mechanics.h>

#include <Phalanx_DataLayout.hpp>
#include <Sacado_ParameterRegistration.hpp>

#if defined(ALBANY_TIMER)
#include <chrono>
#endif

// **********************************************************************
// Base Class Generic Implemtation
// **********************************************************************
namespace LCM {

template <typename EvalT, typename Traits>
AnalyticMassResidualBase<EvalT, Traits>::AnalyticMassResidualBase(Teuchos::ParameterList const& p, const Teuchos::RCP<Albany::Layouts>& dl)
    : w_bf_(p.get<std::string>("Weighted BF Name"), dl->node_qp_scalar),
      weights_("Weights", dl->qp_scalar),
      mass_(p.get<std::string>("Analytic Mass Name"), dl->node_vector),
      out_(Teuchos::VerboseObjectBase::getDefaultOStream())
{
  if (p.isParameter("Density")) density_ = p.get<RealType>("Density");

  resid_using_cub_    = p.get<bool>("Residual Computed Using Cubature");
  use_composite_tet_  = p.get<bool>("Use Composite Tet 10");
  use_analytic_mass_  = p.get<bool>("Use Analytic Mass");
  lump_analytic_mass_ = p.get<bool>("Lump Analytic Mass");

  this->addDependentField(w_bf_);
  this->addDependentField(weights_);
  this->addEvaluatedField(mass_);

  if (p.isType<bool>("Disable Dynamics"))
    enable_dynamics_ = !p.get<bool>("Disable Dynamics");
  else
    enable_dynamics_ = true;

  if (enable_dynamics_) {
    accel_qps_ = decltype(accel_qps_)(p.get<std::string>("Acceleration Name"), dl->qp_vector);
    this->addDependentField(accel_qps_);
    accel_nodes_ = decltype(accel_nodes_)(p.get<std::string>("Acceleration Name"), dl->node_vector);
    this->addDependentField(accel_nodes_);
  }

  this->setName("AnalyticMassResidual" + PHX::print<EvalT>());

  Teuchos::RCP<PHX::DataLayout>           vector_dl = dl->node_qp_vector;
  std::vector<PHX::DataLayout::size_type> dims;
  vector_dl->dimensions(dims);
  num_nodes_ = dims[1];
  num_pts_   = dims[2];
  num_dims_  = dims[3];
  num_cells_ = dims[0];

  // Infer what is element type based on # of nodes
  switch (num_nodes_) {
    case 4:
      if (!lump_analytic_mass_)
        elt_type = ELT_TYPE::TET4;
      else
        elt_type = ELT_TYPE::LUMPED_TET4;
      break;
    case 8:
      if (!lump_analytic_mass_)
        elt_type = ELT_TYPE::HEX8;
      else
        elt_type = ELT_TYPE::LUMPED_HEX8;
      break;
    case 10:
      if (!use_composite_tet_) {
        if (!lump_analytic_mass_)
          elt_type = ELT_TYPE::TET10;
        else
          elt_type = ELT_TYPE::LUMPED_TET10;
      } else {
        if (!lump_analytic_mass_)
          elt_type = ELT_TYPE::CT10;
        else
          elt_type = ELT_TYPE::LUMPED_CT10;
      }
      break;
    default: elt_type = ELT_TYPE::UNSUPPORTED;
  }

  Teuchos::RCP<ParamLib> paramLib = p.get<Teuchos::RCP<ParamLib>>("Parameter Library");
}

// **********************************************************************
template <typename EvalT, typename Traits>
void
AnalyticMassResidualBase<EvalT, Traits>::postRegistrationSetup(typename Traits::SetupData d, PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(w_bf_, fm);
  this->utils.setFieldData(weights_, fm);
  this->utils.setFieldData(mass_, fm);
  if (enable_dynamics_) {
    this->utils.setFieldData(accel_qps_, fm);
    this->utils.setFieldData(accel_nodes_, fm);
  }
}

template <typename EvalT, typename Traits>
std::vector<RealType>
AnalyticMassResidualBase<EvalT, Traits>::tet4LocalMassRow(int const cell, int const row) const
{
  std::vector<RealType> mass_row(4);
  switch (row) {
    case 0:
      mass_row[0] = 2.0;
      mass_row[1] = 1.0;
      mass_row[2] = 1.0;
      mass_row[3] = 1.0;
      break;
    case 1:
      mass_row[0] = 1.0;
      mass_row[1] = 2.0;
      mass_row[2] = 1.0;
      mass_row[3] = 1.0;
      break;
    case 2:
      mass_row[0] = 1.0;
      mass_row[1] = 1.0;
      mass_row[2] = 2.0;
      mass_row[3] = 1.0;
      break;
    case 3:
      mass_row[0] = 1.0;
      mass_row[1] = 1.0;
      mass_row[2] = 1.0;
      mass_row[3] = 2.0;
      break;
    default:
      ALBANY_ABORT(
          "Error! invalid value row = " << row << " to tet4LocalMassRow! \n"
                                        << "Row must be between 0 and 3.\n");
  }
  const RealType elt_vol = computeElementVolume(cell);
  const RealType scale   = elt_vol * 6.0 * density_;
  for (int i = 0; i < mass_row.size(); i++) {
    mass_row[i] /= 120.0;
    mass_row[i] *= scale;
  }
  return mass_row;
}

template <typename EvalT, typename Traits>
std::vector<RealType>
AnalyticMassResidualBase<EvalT, Traits>::tet4LocalMassRowLumped(int const cell, int const row) const
{
  std::vector<RealType> mass_row(4);
  switch (row) {
    case 0: mass_row[0] = 1.0; break;
    case 1: mass_row[1] = 1.0; break;
    case 2: mass_row[2] = 1.0; break;
    case 3: mass_row[3] = 1.0; break;
    default:
      ALBANY_ABORT(
          "Error! invalid value row = " << row << " to tet4LocalMassRowLumped! \n"
                                        << "Row must be between 0 and 3.\n");
  }
  const RealType elt_vol = computeElementVolume(cell);
  const RealType scale   = elt_vol * 6.0 * density_;
  for (int i = 0; i < mass_row.size(); i++) {
    mass_row[i] /= 24.0;
    mass_row[i] *= scale;
  }
  return mass_row;
}

template <typename EvalT, typename Traits>
std::vector<RealType>
AnalyticMassResidualBase<EvalT, Traits>::tet10LocalMassRow(int const cell, int const row) const
{
  std::vector<RealType> mass_row(10);
  switch (row) {
    case 0:
      mass_row[0] = 6.0;
      mass_row[1] = 1.0;
      mass_row[2] = 1.0;
      mass_row[3] = 1.0;
      mass_row[4] = -4.0;
      mass_row[5] = -6.0;
      mass_row[6] = -4.0;
      mass_row[7] = -4.0;
      mass_row[8] = -6.0;
      mass_row[9] = -6.0;
      break;
    case 1:
      mass_row[0] = 1.0;
      mass_row[1] = 6.0;
      mass_row[2] = 1.0;
      mass_row[3] = 1.0;
      mass_row[4] = -4.0;
      mass_row[5] = -4.0;
      mass_row[6] = -6.0;
      mass_row[7] = -6.0;
      mass_row[8] = -4.0;
      mass_row[9] = -6.0;
      break;
    case 2:
      mass_row[0] = 1.0;
      mass_row[1] = 1.0;
      mass_row[2] = 6.0;
      mass_row[3] = 1.0;
      mass_row[4] = -6.0;
      mass_row[5] = -4.0;
      mass_row[6] = -4.0;
      mass_row[7] = -6.0;
      mass_row[8] = -6.0;
      mass_row[9] = -4.0;
      break;
    case 3:
      mass_row[0] = 1.0;
      mass_row[1] = 1.0;
      mass_row[2] = 1.0;
      mass_row[3] = 6.0;
      mass_row[4] = -6.0;
      mass_row[5] = -6.0;
      mass_row[6] = -6.0;
      mass_row[7] = -4.0;
      mass_row[8] = -4.0;
      mass_row[9] = -4.0;
      break;
    case 4:
      mass_row[0] = -4.0;
      mass_row[1] = -4.0;
      mass_row[2] = -6.0;
      mass_row[3] = -6.0;
      mass_row[4] = 32.0;
      mass_row[5] = 16.0;
      mass_row[6] = 16.0;
      mass_row[7] = 16.0;
      mass_row[8] = 16.0;
      mass_row[9] = 8.0;
      break;
    case 5:
      mass_row[0] = -6.0;
      mass_row[1] = -4.0;
      mass_row[2] = -4.0;
      mass_row[3] = -6.0;
      mass_row[4] = 16.0;
      mass_row[5] = 32.0;
      mass_row[6] = 16.0;
      mass_row[7] = 8.0;
      mass_row[8] = 16.0;
      mass_row[9] = 16.0;
      break;
    case 6:
      mass_row[0] = -4.0;
      mass_row[1] = -6.0;
      mass_row[2] = -4.0;
      mass_row[3] = -6.0;
      mass_row[4] = 16.0;
      mass_row[5] = 16.0;
      mass_row[6] = 32.0;
      mass_row[7] = 16.0;
      mass_row[8] = 8.0;
      mass_row[9] = 16.0;
      break;
    case 7:
      mass_row[0] = -4.0;
      mass_row[1] = -6.0;
      mass_row[2] = -6.0;
      mass_row[3] = -4.0;
      mass_row[4] = 16.0;
      mass_row[5] = 8.0;
      mass_row[6] = 16.0;
      mass_row[7] = 32.0;
      mass_row[8] = 16.0;
      mass_row[9] = 16.0;
      break;
    case 8:
      mass_row[0] = -6.0;
      mass_row[1] = -4.0;
      mass_row[2] = -6.0;
      mass_row[3] = -4.0;
      mass_row[4] = 16.0;
      mass_row[5] = 16.0;
      mass_row[6] = 8.0;
      mass_row[7] = 16.0;
      mass_row[8] = 32.0;
      mass_row[9] = 16.0;
      break;
    case 9:
      mass_row[0] = -6.0;
      mass_row[1] = -6.0;
      mass_row[2] = -4.0;
      mass_row[3] = -4.0;
      mass_row[4] = 8.0;
      mass_row[5] = 16.0;
      mass_row[6] = 16.0;
      mass_row[7] = 16.0;
      mass_row[8] = 16.0;
      mass_row[9] = 32.0;
      break;
    default:
      ALBANY_ABORT(
          "Error! invalid value row = " << row << " to tet10LocalMassRow! \n"
                                        << "Row must be between 0 and 9.\n");
  }
  const RealType elt_vol = computeElementVolume(cell);
  const RealType scale   = elt_vol * 6.0 * density_;
  for (int i = 0; i < mass_row.size(); i++) {
    mass_row[i] /= 2520.0;
    mass_row[i] *= scale;
  }
  return mass_row;
}

template <typename EvalT, typename Traits>
std::vector<RealType>
AnalyticMassResidualBase<EvalT, Traits>::tet10LocalMassRowLumped(int const cell, int const row) const
{
  std::vector<RealType> mass_row(10);
  switch (row) {
    case 0: mass_row[0] = -1.0; break;
    case 1: mass_row[1] = -1.0; break;
    case 2: mass_row[2] = -1.0; break;
    case 3: mass_row[3] = -1.0; break;
    case 4: mass_row[4] = 4.0; break;
    case 5: mass_row[5] = 4.0; break;
    case 6: mass_row[6] = 4.0; break;
    case 7: mass_row[7] = 4.0; break;
    case 8: mass_row[8] = 4.0; break;
    case 9: mass_row[9] = 4.0; break;
    default:
      ALBANY_ABORT(
          "Error! invalid value row = " << row << " to tet10LocalMassRowLumped! \n"
                                        << "Row must be between 0 and 9.\n");
  }
  const RealType elt_vol = computeElementVolume(cell);
  const RealType scale   = elt_vol * 6.0 * density_;
  for (int i = 0; i < mass_row.size(); i++) {
    mass_row[i] /= 120.0;
    mass_row[i] *= scale;
  }
  return mass_row;
}

template <typename EvalT, typename Traits>
std::vector<RealType>
AnalyticMassResidualBase<EvalT, Traits>::hex8LocalMassRow(int const cell, int const row) const
{
  std::vector<RealType> mass_row(8);
  switch (row) {
    case 0:
      mass_row[0] = 8.0;
      mass_row[1] = 4.0;
      mass_row[2] = 2.0;
      mass_row[3] = 4.0;
      mass_row[4] = 4.0;
      mass_row[5] = 2.0;
      mass_row[6] = 1.0;
      mass_row[7] = 2.0;
      break;
    case 1:
      mass_row[0] = 4.0;
      mass_row[1] = 8.0;
      mass_row[2] = 4.0;
      mass_row[3] = 2.0;
      mass_row[4] = 2.0;
      mass_row[5] = 4.0;
      mass_row[6] = 2.0;
      mass_row[7] = 1.0;
      break;
    case 2:
      mass_row[0] = 2.0;
      mass_row[1] = 4.0;
      mass_row[2] = 8.0;
      mass_row[3] = 4.0;
      mass_row[4] = 1.0;
      mass_row[5] = 2.0;
      mass_row[6] = 4.0;
      mass_row[7] = 2.0;
      break;
    case 3:
      mass_row[0] = 4.0;
      mass_row[1] = 2.0;
      mass_row[2] = 4.0;
      mass_row[3] = 8.0;
      mass_row[4] = 2.0;
      mass_row[5] = 1.0;
      mass_row[6] = 2.0;
      mass_row[7] = 4.0;
      break;
    case 4:
      mass_row[0] = 4.0;
      mass_row[1] = 2.0;
      mass_row[2] = 1.0;
      mass_row[3] = 2.0;
      mass_row[4] = 8.0;
      mass_row[5] = 4.0;
      mass_row[6] = 2.0;
      mass_row[7] = 4.0;
      break;
    case 5:
      mass_row[0] = 2.0;
      mass_row[1] = 4.0;
      mass_row[2] = 2.0;
      mass_row[3] = 1.0;
      mass_row[4] = 4.0;
      mass_row[5] = 8.0;
      mass_row[6] = 4.0;
      mass_row[7] = 2.0;
      break;
    case 6:
      mass_row[0] = 1.0;
      mass_row[1] = 2.0;
      mass_row[2] = 4.0;
      mass_row[3] = 2.0;
      mass_row[4] = 2.0;
      mass_row[5] = 4.0;
      mass_row[6] = 8.0;
      mass_row[7] = 4.0;
      break;
    case 7:
      mass_row[0] = 2.0;
      mass_row[1] = 1.0;
      mass_row[2] = 2.0;
      mass_row[3] = 4.0;
      mass_row[4] = 4.0;
      mass_row[5] = 2.0;
      mass_row[6] = 4.0;
      mass_row[7] = 8.0;
      break;
    default:
      ALBANY_ABORT(
          "Error! invalid value row = " << row << " to hex8LocalMassRow! \n"
                                        << "Row must be between 0 and 7.\n");
  }
  const RealType elt_vol = computeElementVolume(cell);
  const RealType scale   = elt_vol / 8.0 * density_;
  for (int i = 0; i < mass_row.size(); i++) {
    mass_row[i] /= 27.0;
    mass_row[i] *= scale;
  }
  return mass_row;
}

template <typename EvalT, typename Traits>
std::vector<RealType>
AnalyticMassResidualBase<EvalT, Traits>::hex8LocalMassRowLumped(int const cell, int const row) const
{
  std::vector<RealType> mass_row(8);
  switch (row) {
    case 0: mass_row[0] = 1.0; break;
    case 1: mass_row[1] = 1.0; break;
    case 2: mass_row[2] = 1.0; break;
    case 3: mass_row[3] = 1.0; break;
    case 4: mass_row[4] = 1.0; break;
    case 5: mass_row[5] = 1.0; break;
    case 6: mass_row[6] = 1.0; break;
    case 7: mass_row[7] = 1.0; break;
    default:
      ALBANY_ABORT(
          "Error! invalid value row = " << row << " to hex8LocalMassRowLumped! \n"
                                        << "Row must be between 0 and 7.\n");
  }
  const RealType elt_vol = computeElementVolume(cell);
  const RealType scale   = elt_vol / 8.0 * density_;
  for (int i = 0; i < mass_row.size(); i++) {
    mass_row[i] *= scale;
  }
  return mass_row;
}

template <typename EvalT, typename Traits>
std::vector<RealType>
AnalyticMassResidualBase<EvalT, Traits>::compositeTet10LocalMassRow(int const cell, int const row) const
{
  std::vector<RealType> mass_row(10);
  switch (row) {
    case 0:
      mass_row[0] = 18.0;
      mass_row[1] = 0.0;
      mass_row[2] = 0.0;
      mass_row[3] = 0.0;
      mass_row[4] = 9.0;
      mass_row[5] = 0.0;
      mass_row[6] = 9.0;
      mass_row[7] = 9.0;
      mass_row[8] = 0.0;
      mass_row[9] = 0.0;
      break;
    case 1:
      mass_row[0] = 0.0;
      mass_row[1] = 18.0;
      mass_row[2] = 0.0;
      mass_row[3] = 0.0;
      mass_row[4] = 9.0;
      mass_row[5] = 9.0;
      mass_row[6] = 0.0;
      mass_row[7] = 0.0;
      mass_row[8] = 9.0;
      mass_row[9] = 0.0;
      break;
    case 2:
      mass_row[0] = 0.0;
      mass_row[1] = 0.0;
      mass_row[2] = 18.0;
      mass_row[3] = 0.0;
      mass_row[4] = 0.0;
      mass_row[5] = 9.0;
      mass_row[6] = 9.0;
      mass_row[7] = 0.0;
      mass_row[8] = 0.0;
      mass_row[9] = 9.0;
      break;
    case 3:
      mass_row[0] = 0.0;
      mass_row[1] = 0.0;
      mass_row[2] = 0.0;
      mass_row[3] = 18.0;
      mass_row[4] = 0.0;
      mass_row[5] = 0.0;
      mass_row[6] = 0.0;
      mass_row[7] = 9.0;
      mass_row[8] = 9.0;
      mass_row[9] = 9.0;
      break;
    case 4:
      mass_row[0] = 9.0;
      mass_row[1] = 9.0;
      mass_row[2] = 0.0;
      mass_row[3] = 0.0;
      mass_row[4] = 80.0;
      mass_row[5] = 26.0;
      mass_row[6] = 26.0;
      mass_row[7] = 26.0;
      mass_row[8] = 26.0;
      mass_row[9] = 8.0;
      break;
    case 5:
      mass_row[0] = 0.0;
      mass_row[1] = 9.0;
      mass_row[2] = 9.0;
      mass_row[3] = 0.0;
      mass_row[4] = 26.0;
      mass_row[5] = 80.0;
      mass_row[6] = 26.0;
      mass_row[7] = 8.0;
      mass_row[8] = 26.0;
      mass_row[9] = 26.0;
      break;
    case 6:
      mass_row[0] = 9.0;
      mass_row[1] = 0.0;
      mass_row[2] = 9.0;
      mass_row[3] = 0.0;
      mass_row[4] = 26.0;
      mass_row[5] = 26.0;
      mass_row[6] = 80.0;
      mass_row[7] = 26.0;
      mass_row[8] = 8.0;
      mass_row[9] = 26.0;
      break;
    case 7:
      mass_row[0] = 9.0;
      mass_row[1] = 0.0;
      mass_row[2] = 0.0;
      mass_row[3] = 9.0;
      mass_row[4] = 26.0;
      mass_row[5] = 8.0;
      mass_row[6] = 26.0;
      mass_row[7] = 80.0;
      mass_row[8] = 26.0;
      mass_row[9] = 26.0;
      break;
    case 8:
      mass_row[0] = 0.0;
      mass_row[1] = 9.0;
      mass_row[2] = 0.0;
      mass_row[3] = 9.0;
      mass_row[4] = 26.0;
      mass_row[5] = 26.0;
      mass_row[6] = 8.0;
      mass_row[7] = 26.0;
      mass_row[8] = 80.0;
      mass_row[9] = 26.0;
      break;
    case 9:
      mass_row[0] = 0.0;
      mass_row[1] = 0.0;
      mass_row[2] = 9.0;
      mass_row[3] = 9.0;
      mass_row[4] = 8.0;
      mass_row[5] = 26.0;
      mass_row[6] = 26.0;
      mass_row[7] = 26.0;
      mass_row[8] = 26.0;
      mass_row[9] = 80.0;
      break;
    default:
      ALBANY_ABORT(
          "Error! invalid value row = " << row << " to compositeTet10LocalMassRow! \n"
                                        << "Row must be between 0 and 9.\n");
  }
  const RealType elt_vol = computeElementVolume(cell);
  const RealType scale   = elt_vol * density_;
  for (int i = 0; i < mass_row.size(); i++) {
    mass_row[i] /= 1440.0;
    mass_row[i] *= scale;
  }
  return mass_row;
}

template <typename EvalT, typename Traits>
std::vector<RealType>
AnalyticMassResidualBase<EvalT, Traits>::compositeTet10LocalMassRowLumped(int const cell, int const row) const
{
  std::vector<RealType> mass_row(10);
  switch (row) {
    case 0: mass_row[0] = 3.0; break;
    case 1: mass_row[1] = 3.0; break;
    case 2: mass_row[2] = 3.0; break;
    case 3: mass_row[3] = 3.0; break;
    case 4: mass_row[4] = 14.0; break;
    case 5: mass_row[5] = 14.0; break;
    case 6: mass_row[6] = 14.0; break;
    case 7: mass_row[7] = 14.0; break;
    case 8: mass_row[8] = 14.0; break;
    case 9: mass_row[9] = 14.0; break;
    default:
      ALBANY_ABORT(
          "Error! invalid value row = " << row << " to compositeTet10LocalMassRowLumped! \n"
                                        << "Row must be between 0 and 9.\n");
  }
  const RealType elt_vol = computeElementVolume(cell);
  const RealType scale   = elt_vol * density_;
  for (int i = 0; i < mass_row.size(); i++) {
    mass_row[i] /= 96.0;
    mass_row[i] *= scale;
  }
  return mass_row;
}

template <typename EvalT, typename Traits>
RealType
AnalyticMassResidualBase<EvalT, Traits>::computeElementVolScaling(int const cell, int const node) const
{
  RealType elt_vol_scale_at_node = 0.0;
  for (int pt = 0; pt < num_pts_; ++pt) {
    elt_vol_scale_at_node += w_bf_(cell, node, pt);
  }
  return elt_vol_scale_at_node;
}

template <typename EvalT, typename Traits>
RealType
AnalyticMassResidualBase<EvalT, Traits>::computeElementVolume(int const cell) const
{
  RealType elt_vol = 0.0;
  for (int pt = 0; pt < num_pts_; ++pt) {
    elt_vol += Albany::ADValue(weights_(cell, pt));
  }
  return elt_vol;
}

template <typename EvalT, typename Traits>
void
AnalyticMassResidualBase<EvalT, Traits>::computeResidualValue(typename Traits::EvalData workset) const
{
  // Zero out mass_
  for (int cell = 0; cell < workset.numCells; ++cell) {
    for (int node = 0; node < this->num_nodes_; ++node) {
      for (int dim = 0; dim < this->num_dims_; ++dim) {
        (this->mass_)(cell, node, dim) = ScalarT(0.0);
      }
    }
  }
  if (resid_using_cub_ == true) {
    // Approach 1: uses numerical cubature to compute residual contribution
    for (int cell = 0; cell < workset.numCells; ++cell) {
      for (int node = 0; node < this->num_nodes_; ++node) {
        for (int pt = 0; pt < this->num_pts_; ++pt) {
          for (int dim = 0; dim < this->num_dims_; ++dim) {
            (this->mass_)(cell, node, dim) += (this->density_) * (this->accel_qps_)(cell, pt, dim) * (this->w_bf_)(cell, node, pt);
          }
        }
      }
    }
  } else {
    // Approach 2: uses mass matrix to compute residual contribution (r =
    // rho*M*a)
    for (int cell = 0; cell < workset.numCells; ++cell) {
      for (int node = 0; node < this->num_nodes_; ++node) {  // loop over rows
        std::vector<RealType> mass_row;
        switch (this->elt_type) {
          case ELT_TYPE::TET4: mass_row = this->tet4LocalMassRow(cell, node); break;
          case ELT_TYPE::LUMPED_TET4: mass_row = this->tet4LocalMassRowLumped(cell, node); break;
          case ELT_TYPE::HEX8: mass_row = this->hex8LocalMassRow(cell, node); break;
          case ELT_TYPE::LUMPED_HEX8: mass_row = this->hex8LocalMassRowLumped(cell, node); break;
          case ELT_TYPE::TET10: mass_row = this->tet10LocalMassRow(cell, node); break;
          case ELT_TYPE::LUMPED_TET10: mass_row = this->tet10LocalMassRowLumped(cell, node); break;
          case ELT_TYPE::CT10: mass_row = this->compositeTet10LocalMassRow(cell, node); break;
          case ELT_TYPE::LUMPED_CT10: mass_row = this->compositeTet10LocalMassRowLumped(cell, node); break;
          default: break;
        }
        for (int dim = 0; dim < this->num_dims_; ++dim) {
          ScalarT val = 0.0;
          for (int i = 0; i < this->num_nodes_; ++i) {  // loop over columns
            val += mass_row[i] * accel_nodes_(cell, i, dim);
          }
          (this->mass_)(cell, node, dim) += val;
        }
      }
    }
  }
}

// **********************************************************************
// Specialization: Residual
// **********************************************************************
template <typename Traits>
AnalyticMassResidual<PHAL::AlbanyTraits::Residual, Traits>::AnalyticMassResidual(Teuchos::ParameterList const& p, const Teuchos::RCP<Albany::Layouts>& dl)
    : AnalyticMassResidualBase<PHAL::AlbanyTraits::Residual, Traits>(p, dl)
{
}

// **********************************************************************
template <typename Traits>
void
AnalyticMassResidual<PHAL::AlbanyTraits::Residual, Traits>::evaluateFields(typename Traits::EvalData workset)
{
  if (this->use_analytic_mass_ == false) return;

  // Throw error is trying to call with unsupported element type
  if (this->elt_type == AnalyticMassResidualBase<PHAL::AlbanyTraits::Residual, Traits>::ELT_TYPE::UNSUPPORTED) {
    ALBANY_ABORT(
        "Error! AnalyticMassResidual is being run with unsupported element "
        "having \n"
        << this->num_nodes_ << " nodes.  Please re-run with 'Use Analytic Mass' = 'false'.\n");
  }

  this->computeResidualValue(workset);
}

// **********************************************************************
// Specialization: Jacobian
// **********************************************************************

template <typename Traits>
AnalyticMassResidual<PHAL::AlbanyTraits::Jacobian, Traits>::AnalyticMassResidual(Teuchos::ParameterList const& p, const Teuchos::RCP<Albany::Layouts>& dl)
    : AnalyticMassResidualBase<PHAL::AlbanyTraits::Jacobian, Traits>(p, dl)
{
}

// **********************************************************************
template <typename Traits>
void
AnalyticMassResidual<PHAL::AlbanyTraits::Jacobian, Traits>::evaluateFields(typename Traits::EvalData workset)
{
  if (this->use_analytic_mass_ == false) return;

  // Throw error is trying to call with unsupported element type
  if (this->elt_type == AnalyticMassResidualBase<PHAL::AlbanyTraits::Jacobian, Traits>::ELT_TYPE::UNSUPPORTED) {
    ALBANY_ABORT(
        "Error! AnalyticMassResidual is being run with unsupported element "
        "having \n"
        << this->num_nodes_ << " nodes.  Please re-run with 'Use Analytic Mass' = 'false'.\n");
  }

  // Compute residual value
  this->computeResidualValue(workset);

  // Set local Jacobian entries
  double n_coeff = workset.n_coeff;
  for (int cell = 0; cell < workset.numCells; ++cell) {
    for (int node = 0; node < this->num_nodes_; ++node) {  // loop over Jacobian rows
      std::vector<RealType> mass_row;
      switch (this->elt_type) {
        case AnalyticMassResidualBase<PHAL::AlbanyTraits::Jacobian, Traits>::ELT_TYPE::TET4: mass_row = this->tet4LocalMassRow(cell, node); break;
        case AnalyticMassResidualBase<PHAL::AlbanyTraits::Jacobian, Traits>::ELT_TYPE::LUMPED_TET4: mass_row = this->tet4LocalMassRowLumped(cell, node); break;
        case AnalyticMassResidualBase<PHAL::AlbanyTraits::Jacobian, Traits>::ELT_TYPE::HEX8: mass_row = this->hex8LocalMassRow(cell, node); break;
        case AnalyticMassResidualBase<PHAL::AlbanyTraits::Jacobian, Traits>::ELT_TYPE::LUMPED_HEX8: mass_row = this->hex8LocalMassRowLumped(cell, node); break;
        case AnalyticMassResidualBase<PHAL::AlbanyTraits::Jacobian, Traits>::ELT_TYPE::TET10: mass_row = this->tet10LocalMassRow(cell, node); break;
        case AnalyticMassResidualBase<PHAL::AlbanyTraits::Jacobian, Traits>::ELT_TYPE::LUMPED_TET10:
          mass_row = this->tet10LocalMassRowLumped(cell, node);
          break;
        case AnalyticMassResidualBase<PHAL::AlbanyTraits::Jacobian, Traits>::ELT_TYPE::CT10: mass_row = this->compositeTet10LocalMassRow(cell, node); break;
        case AnalyticMassResidualBase<PHAL::AlbanyTraits::Jacobian, Traits>::ELT_TYPE::LUMPED_CT10:
          mass_row = this->compositeTet10LocalMassRowLumped(cell, node);
          break;
        default: break;
      }
      for (int dim = 0; dim < this->num_dims_; ++dim) {
        typename PHAL::Ref<ScalarT>::type valref = (this->mass_)(cell, node, dim);  // get Jacobian row
        int                               k;
        for (int i = 0; i < this->num_nodes_; ++i) {  // loop over Jacobian cols
          k                      = i * this->num_dims_ + dim;
          valref.fastAccessDx(k) = n_coeff * mass_row[i];
        }
      }
    }
  }
}

}  // namespace LCM
