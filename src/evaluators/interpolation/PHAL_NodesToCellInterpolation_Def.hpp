// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#include "Phalanx_DataLayout.hpp"
#include "Phalanx_Print.hpp"

namespace PHAL {

//**********************************************************************
template <typename EvalT, typename Traits, typename ScalarT>
NodesToCellInterpolationBase<EvalT, Traits, ScalarT>::NodesToCellInterpolationBase(Teuchos::ParameterList const& p, const Teuchos::RCP<Albany::Layouts>& dl)
    : BF(p.get<std::string>("BF Variable Name"), dl->node_qp_scalar), w_measure(p.get<std::string>("Weighted Measure Name"), dl->qp_scalar)
{
  isVectorField = p.get<bool>("Is Vector Field");
  if (isVectorField) {
    field_node = decltype(field_node)(p.get<std::string>("Field Node Name"), dl->node_vector);
    field_cell = decltype(field_cell)(p.get<std::string>("Field Cell Name"), dl->cell_vector);

    vecDim = dl->node_vector->extent(2);
  } else {
    field_node = decltype(field_node)(p.get<std::string>("Field Node Name"), dl->node_scalar);
    field_cell = decltype(field_cell)(p.get<std::string>("Field Cell Name"), dl->cell_scalar2);
  }

  numQPs   = dl->qp_scalar->extent(1);
  numNodes = dl->node_scalar->extent(1);

  this->addDependentField(BF.fieldTag());
  this->addDependentField(field_node.fieldTag());
  this->addDependentField(w_measure.fieldTag());

  this->addEvaluatedField(field_cell);

  this->setName("NodesToCellInterpolation" + PHX::print<EvalT>());
}

//**********************************************************************
template <typename EvalT, typename Traits, typename ScalarT>
void
NodesToCellInterpolationBase<EvalT, Traits, ScalarT>::postRegistrationSetup(typename Traits::SetupData d, PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(field_node, fm);
  this->utils.setFieldData(BF, fm);
  this->utils.setFieldData(w_measure, fm);

  this->utils.setFieldData(field_cell, fm);

  d.fill_field_dependencies(this->dependentFields(), this->evaluatedFields());
}

//**********************************************************************
template <typename EvalT, typename Traits, typename ScalarT>
void
NodesToCellInterpolationBase<EvalT, Traits, ScalarT>::evaluateFields(typename Traits::EvalData workset)
{
  MeshScalarT meas;
  ScalarT     field_qp;

  for (int cell = 0; cell < workset.numCells; ++cell) {
    meas = 0.0;
    for (int qp(0); qp < numQPs; ++qp) {
      meas += w_measure(cell, qp);
    }

    if (isVectorField) {
      for (int dim(0); dim < vecDim; ++dim) {
        field_cell(cell, dim) = 0;
        for (int qp(0); qp < numQPs; ++qp) {
          field_qp = 0;
          for (int node(0); node < numNodes; ++node) field_qp += field_node(cell, node, dim) * BF(cell, node, qp);
          field_cell(cell, dim) += field_qp * w_measure(cell, qp);
        }
        field_cell(cell, dim) /= meas;
      }
    } else {
      field_cell(cell) = 0;
      for (int qp(0); qp < numQPs; ++qp) {
        field_qp = 0;
        for (int node(0); node < numNodes; ++node) field_qp += field_node(cell, node) * BF(cell, node, qp);
        field_cell(cell) += field_qp * w_measure(cell, qp);
      }
      field_cell(cell) /= meas;
    }
  }
}

}  // Namespace PHAL
