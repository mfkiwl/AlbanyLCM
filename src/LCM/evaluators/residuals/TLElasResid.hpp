// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#ifndef TLELASRESID_HPP
#define TLELASRESID_HPP

#include "Albany_Types.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_MDField.hpp"
#include "Phalanx_config.hpp"
#include "Sacado_ParameterAccessor.hpp"

namespace LCM {
/** \brief Total Lagrangian (Non-linear) Elasticity Residual

    This evaluator computes a nonlinear elasticity residual

*/

template <typename EvalT, typename Traits>
class TLElasResid : public PHX::EvaluatorWithBaseImpl<Traits>, public PHX::EvaluatorDerived<EvalT, Traits>, public Sacado::ParameterAccessor<EvalT, SPL_Traits>
{
 public:
  using ScalarT     = typename EvalT::ScalarT;
  using MeshScalarT = typename EvalT::MeshScalarT;

  TLElasResid(Teuchos::ParameterList const& p);

  void
  postRegistrationSetup(typename Traits::SetupData d, PHX::FieldManager<Traits>& vm);

  void
  evaluateFields(typename Traits::EvalData d);

  ScalarT&
  getValue(std::string const& n);

 private:
  // Input:
  PHX::MDField<ScalarT const, Cell, QuadPoint, Dim, Dim>      stress;
  PHX::MDField<ScalarT const, Cell, QuadPoint>                J;
  PHX::MDField<ScalarT const, Cell, QuadPoint, Dim, Dim>      defgrad;
  PHX::MDField<const MeshScalarT, Cell, Node, QuadPoint, Dim> wGradBF;
  PHX::MDField<const MeshScalarT, Cell, Node, QuadPoint>      wBF;
  ScalarT                                                     zGrav;

  // Output:
  PHX::MDField<ScalarT, Cell, Node, Dim> Residual;

  int worksetSize;
  int numNodes;
  int numQPs;
  int numDims;

  // Material Name
  std::string matModel;

  // Work space FCs
  Kokkos::DynRankView<ScalarT, PHX::Device> F_inv;
  Kokkos::DynRankView<ScalarT, PHX::Device> F_invT;
  Kokkos::DynRankView<ScalarT, PHX::Device> JF_invT;
  Kokkos::DynRankView<ScalarT, PHX::Device> P;
};
}  // namespace LCM

#endif
