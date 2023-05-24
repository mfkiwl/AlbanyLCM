// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#include "Albany_DiscretizationUtils.hpp"
#include "PHAL_SideQuadPointsToSideInterpolation.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Phalanx_Print.hpp"

namespace PHAL {

//**********************************************************************
template <typename EvalT, typename Traits, typename ScalarT>
SideQuadPointsToSideInterpolationBase<EvalT, Traits, ScalarT>::SideQuadPointsToSideInterpolationBase(
    Teuchos::ParameterList const&        p,
    const Teuchos::RCP<Albany::Layouts>& dl_side)
    : w_measure(p.get<std::string>("Weighted Measure Name"), dl_side->qp_scalar)
{
  fieldDim = p.isParameter("Field Dimension") ? p.get<int>("Field Dimension") : 0;

  sideSetName = p.get<std::string>("Side Set Name");
  if (fieldDim == 0) {
    field_qp   = decltype(field_qp)(p.get<std::string>("Field QP Name"), dl_side->qp_scalar);
    field_side = decltype(field_side)(p.get<std::string>("Field Side Name"), dl_side->cell_scalar2);
  } else if (fieldDim == 1) {
    field_qp   = decltype(field_qp)(p.get<std::string>("Field QP Name"), dl_side->qp_vector);
    field_side = decltype(field_side)(p.get<std::string>("Field Side Name"), dl_side->cell_vector);
  } else if (fieldDim == 2) {
    field_qp   = decltype(field_qp)(p.get<std::string>("Field QP Name"), dl_side->qp_tensor);
    field_side = decltype(field_side)(p.get<std::string>("Field Side Name"), dl_side->cell_tensor);
  } else {
    ALBANY_ABORT("Error! Field dimension not supported.\n");
  }

  this->addDependentField(field_qp.fieldTag());
  this->addDependentField(w_measure.fieldTag());
  this->addEvaluatedField(field_side);

  this->setName("SideQuadPointsToSideInterpolation" + PHX::print<EvalT>());
}

template <typename EvalT, typename Traits, typename ScalarT>
void
SideQuadPointsToSideInterpolationBase<EvalT, Traits, ScalarT>::postRegistrationSetup(typename Traits::SetupData /* d */, PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(field_qp, fm);
  this->utils.setFieldData(w_measure, fm);

  this->utils.setFieldData(field_side, fm);
  field_qp.dimensions(dims);
}

template <typename EvalT, typename Traits, typename ScalarT>
void
SideQuadPointsToSideInterpolationBase<EvalT, Traits, ScalarT>::evaluateFields(typename Traits::EvalData workset)
{
  if (workset.sideSets->find(sideSetName) == workset.sideSets->end()) return;

  std::vector<Albany::SideStruct> const& sideSet = workset.sideSets->at(sideSetName);
  for (auto const& it_side : sideSet) {
    // Get the local data of side and cell
    int const cell = it_side.elem_LID;
    int const side = it_side.side_local_id;

    MeshScalarT meas = 0.0;
    for (int qp(0); qp < dims[2]; ++qp) {
      meas += w_measure(cell, side, qp);
    }

    switch (fieldDim) {
      case 0:
        field_side(cell, side) = 0.0;
        for (int qp(0); qp < dims[2]; ++qp) {
          field_side(cell, side) += field_qp(cell, side, qp) * w_measure(cell, side, qp);
        }
        field_side(cell, side) /= meas;
        break;

      case 1:
        for (int i(0); i < dims[3]; ++i) {
          field_side(cell, side, i) = 0;
          for (int qp(0); qp < dims[2]; ++qp) field_side(cell, side, i) += field_qp(cell, side, qp, i) * w_measure(cell, side, qp);
          field_side(cell, side, i) /= meas;
        }
        break;

      case 2:
        for (int i(0); i < dims[3]; ++i) {
          for (int j(0); j < dims[4]; ++j) {
            field_side(cell, side, i, j) = 0;
            for (int qp(0); qp < dims[2]; ++qp) field_side(cell, side, i, j) += field_qp(cell, side, qp, i, j) * w_measure(cell, side, qp);
            field_side(cell, side, i, j) /= meas;
          }
        }
        break;

      default:
        ALBANY_ABORT(
            "Error! Field dimension not supported (this error should have "
            "already appeared).\n");
    }
  }
}

}  // Namespace PHAL
