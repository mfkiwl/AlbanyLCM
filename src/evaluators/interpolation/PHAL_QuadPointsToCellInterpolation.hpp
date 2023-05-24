// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#ifndef PHAL_QUAD_POINT_TO_CELL_INTERPOLATION_HPP
#define PHAL_QUAD_POINT_TO_CELL_INTERPOLATION_HPP

#include "Albany_Layouts.hpp"
#include "Albany_SacadoTypes.hpp"
#include "PHAL_Utilities.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_MDField.hpp"
#include "Phalanx_config.hpp"

namespace PHAL {
/** \brief Average from Qp to Cell

    This evaluator averages the quadrature points values to
    obtain a single value for the whole cell

*/

template <typename EvalT, typename Traits, typename ScalarT>
class QuadPointsToCellInterpolationBase : public PHX::EvaluatorWithBaseImpl<Traits>, public PHX::EvaluatorDerived<EvalT, Traits>
{
 public:
  QuadPointsToCellInterpolationBase(
      Teuchos::ParameterList const&        p,
      const Teuchos::RCP<Albany::Layouts>& dl,
      const Teuchos::RCP<PHX::DataLayout>& qp_layout,
      const Teuchos::RCP<PHX::DataLayout>& cell_layout);

  void
  postRegistrationSetup(typename Traits::SetupData d, PHX::FieldManager<Traits>& fm);

  void
  evaluateFields(typename Traits::EvalData d);

 private:
  typedef typename EvalT::MeshScalarT                                      MeshScalarT;
  typedef typename Albany::StrongestScalarType<ScalarT, MeshScalarT>::type OutputScalarT;

  std::vector<PHX::DataLayout::size_type> qp_dims;

  // Input:
  PHX::MDField<ScalarT const>                      field_qp;
  PHX::MDField<const MeshScalarT, Cell, QuadPoint> w_measure;

  // Output:
  PHX::MDField<OutputScalarT> field_cell;
};

// Some shortcut names
template <typename EvalT, typename Traits>
using QuadPointsToCellInterpolation = QuadPointsToCellInterpolationBase<EvalT, Traits, typename EvalT::ScalarT>;

template <typename EvalT, typename Traits>
using QuadPointsToCellInterpolationMesh = QuadPointsToCellInterpolationBase<EvalT, Traits, typename EvalT::MeshScalarT>;

template <typename EvalT, typename Traits>
using QuadPointsToCellInterpolationParam = QuadPointsToCellInterpolationBase<EvalT, Traits, typename EvalT::ParamScalarT>;

}  // Namespace PHAL

#endif  // PHAL_QUAD_POINT_TO_CELL_INTERPOLATION_HPP
