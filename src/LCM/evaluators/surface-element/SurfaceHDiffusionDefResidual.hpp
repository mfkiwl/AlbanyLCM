// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#ifndef SURFACE_H_DIFFUSION_DEF_RESIDUAL_HPP
#define SURFACE_H_DIFFUSION_DEF_RESIDUAL_HPP

#include "Albany_Layouts.hpp"
#include "Albany_Types.hpp"
#include "Intrepid2_CellTools.hpp"
#include "Intrepid2_Cubature.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_MDField.hpp"
#include "Phalanx_config.hpp"

namespace LCM {
/** \brief

    Compute the balance of mass residual on the surface

**/

template <typename EvalT, typename Traits>
class SurfaceHDiffusionDefResidual : public PHX::EvaluatorWithBaseImpl<Traits>, public PHX::EvaluatorDerived<EvalT, Traits>
{
 public:
  SurfaceHDiffusionDefResidual(Teuchos::ParameterList const& p, const Teuchos::RCP<Albany::Layouts>& dl);

  void
  postRegistrationSetup(typename Traits::SetupData d, PHX::FieldManager<Traits>& vm);

  void
  evaluateFields(typename Traits::EvalData d);

 private:
  using ScalarT     = typename EvalT::ScalarT;
  using MeshScalarT = typename EvalT::MeshScalarT;

  // Input:
  //! Length scale parameter for localization zone
  ScalarT thickness;
  //! Numerical integration rule
  Teuchos::RCP<Intrepid2::Cubature<PHX::Device>> cubature;
  //! Finite element basis for the midplane
  Teuchos::RCP<Intrepid2::Basis<PHX::Device, RealType, RealType>> intrepidBasis;
  //! Scalar Gradient
  PHX::MDField<ScalarT const, Cell, QuadPoint, Dim> scalarGrad;
  //! Scalar Gradient Operator
  PHX::MDField<const MeshScalarT, Cell, Node, QuadPoint, Dim> surface_Grad_BF;
  //! Scalar Jump
  PHX::MDField<ScalarT const, Cell, QuadPoint> scalarJump;
  //! Reference configuration dual basis
  PHX::MDField<const MeshScalarT, Cell, QuadPoint, Dim, Dim> refDualBasis;
  //! Reference configuration normal
  PHX::MDField<const MeshScalarT, Cell, QuadPoint, Dim> refNormal;
  //! Reference configuration area
  PHX::MDField<const MeshScalarT, Cell, QuadPoint> refArea;
  //! Determinant of the surface deformation gradient
  PHX::MDField<ScalarT const, Cell, QuadPoint> J;
  //! Pore Pressure at the 2D integration point location
  PHX::MDField<ScalarT const, Cell, QuadPoint> transport_;
  //! Nodal Pore Pressure at the 2D integration point location
  PHX::MDField<ScalarT const, Cell, Node> nodal_transport_;

  //! diffusion coefficient at the 2D integration point location
  PHX::MDField<ScalarT const, Cell, QuadPoint> dL_;
  //! effective diffusion constant at the 2D integration point location
  PHX::MDField<ScalarT const, Cell, QuadPoint> eff_diff_;
  //! strain rate factor at the 2D integration point location
  PHX::MDField<ScalarT const, Cell, QuadPoint> strain_rate_factor_;
  //! Convection-like term with hydrostatic stress at the 2D integration point
  //! location
  PHX::MDField<ScalarT const, Cell, QuadPoint> convection_coefficient_;
  //! Hydrostatic stress gradient at the 2D integration point location
  PHX::MDField<ScalarT const, Cell, QuadPoint, Dim> hydro_stress_gradient_;
  //! Equvialent plastic strain at the 2D integration point location
  PHX::MDField<ScalarT const, Cell, QuadPoint> eqps_;
  //! Elelement length parameter for stabilization procedure
  PHX::MDField<ScalarT const, Cell, QuadPoint> element_length_;

  //! Deformation Gradient
  PHX::MDField<ScalarT const, Cell, QuadPoint, Dim, Dim> defGrad;

  // Data from previous time step
  std::string transportName, JName, CLGradName, eqpsName;

  // Time
  PHX::MDField<ScalarT const, Dummy> deltaTime;

  //! Reference Cell Views
  Kokkos::DynRankView<RealType, PHX::Device> refValues;
  Kokkos::DynRankView<RealType, PHX::Device> refGrads;
  Kokkos::DynRankView<RealType, PHX::Device> refPoints;
  Kokkos::DynRankView<RealType, PHX::Device> refWeights;

  Kokkos::DynRankView<ScalarT, PHX::Device> artificalDL;
  Kokkos::DynRankView<ScalarT, PHX::Device> stabilizedDL;

  Kokkos::DynRankView<ScalarT, PHX::Device> pterm;

  // Temporary Views
  Kokkos::DynRankView<ScalarT, PHX::Device> flux;

  ScalarT trialPbar;

  // Stabilization Parameter
  RealType stab_param_;

  // Output:
  PHX::MDField<ScalarT, Cell, Node> transport_residual_;

  unsigned int worksetSize;
  unsigned int numNodes;
  unsigned int numQPs;
  unsigned int numDims;
  unsigned int numPlaneNodes;
  unsigned int numPlaneDims;

  bool haveMech;
};
}  // namespace LCM

#endif
