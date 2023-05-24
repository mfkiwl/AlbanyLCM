// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#if !defined(LCM_StabilizedPressure_Residual_hpp)
#define LCM_StabilizedPressure_Residual_hpp

#include "Albany_Layouts.hpp"
#include "Albany_Types.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_MDField.hpp"
#include "Phalanx_config.hpp"
#include "Sacado_ParameterAccessor.hpp"

namespace LCM {
///
/// \brief StabilizedPressure Residual
///
/// This evaluator computes the residual
/// for the equal order pressure stabilization
///
template <typename EvalT, typename Traits>
class StabilizedPressureResidual : public PHX::EvaluatorWithBaseImpl<Traits>, public PHX::EvaluatorDerived<EvalT, Traits>
{
 public:
  using ScalarT     = typename EvalT::ScalarT;
  using MeshScalarT = typename EvalT::MeshScalarT;

  ///
  /// Constructor
  ///
  StabilizedPressureResidual(Teuchos::ParameterList& p, const Teuchos::RCP<Albany::Layouts>& dl);

  ///
  /// Phalanx method to allocate space
  ///
  void
  postRegistrationSetup(typename Traits::SetupData d, PHX::FieldManager<Traits>& vm);

  ///
  /// Implementation of physics
  ///
  void
  evaluateFields(typename Traits::EvalData d);

 private:
  ///
  /// Input: Shear Modulus
  ///
  PHX::MDField<ScalarT const, Cell, QuadPoint> shear_modulus_;

  ///
  /// Input: Bulk Modulus
  ///
  PHX::MDField<ScalarT const, Cell, QuadPoint> bulk_modulus_;

  ///
  /// Input: Deformation Gradient
  ///
  PHX::MDField<ScalarT const, Cell, QuadPoint, Dim, Dim> def_grad_;

  ///
  /// Input: Cauchy Stress
  ///
  PHX::MDField<ScalarT const, Cell, QuadPoint, Dim, Dim> stress_;

  ///
  /// Input: Pressure
  ///
  PHX::MDField<ScalarT const, Cell, QuadPoint> pressure_;

  ///
  /// Input: Pressure Gradient
  ///
  PHX::MDField<ScalarT const, Cell, QuadPoint, Dim> pressure_grad_;

  ///
  /// Input: Weighted Basis Function Gradients
  ///
  PHX::MDField<const MeshScalarT, Cell, Node, QuadPoint, Dim> w_grad_bf_;

  ///
  /// Input: Weighted Basis Functions
  ///
  PHX::MDField<const MeshScalarT, Cell, Node, QuadPoint> w_bf_;

  ///
  /// Input: Weighted Basis Functions
  ///
  PHX::MDField<ScalarT const, Cell, QuadPoint> h_;

  ///
  /// Output: Residual Forces
  ///
  PHX::MDField<ScalarT, Cell, Node> residual_;

  ///
  /// Number of element nodes
  ///
  int num_nodes_;

  ///
  /// Number of integration points
  ///
  int num_pts_;

  ///
  /// Number of spatial dimensions
  ///
  int num_dims_;

  ///
  /// Small strain flag
  ///
  bool small_strain_;

  ///
  /// Stabilization parameter
  ///
  RealType alpha_;
};
}  // namespace LCM

#endif
