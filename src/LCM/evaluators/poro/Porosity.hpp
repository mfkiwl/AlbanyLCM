// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#if !defined(LCM_Porosity_hpp)
#define LCM_Porosity_hpp

#include "Albany_Layouts.hpp"
#include "Albany_SacadoTypes.hpp"
#include "PHAL_Dimension.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_MDField.hpp"
#include "Phalanx_config.hpp"
#include "Sacado_ParameterAccessor.hpp"
#include "Teuchos_Array.hpp"
#include "Teuchos_ParameterList.hpp"

namespace LCM {
///
/// \brief Evaluates porosity, either as a constant or a truncated
/// KL expansion.
///
/// Porosity update is the most important part for the poromechanics
/// formulation. All poroelasticity parameters (Biot Coefficient,
/// Biot modulus, permeability, and consistent tangential tensor)
/// depend on porosity. The definition we used here is from
/// Coussy's poromechanics p.85.
///
template <typename EvalT, typename Traits>
class Porosity : public PHX::EvaluatorWithBaseImpl<Traits>, public PHX::EvaluatorDerived<EvalT, Traits>, public Sacado::ParameterAccessor<EvalT, SPL_Traits>
{
 public:
  using ScalarT     = typename EvalT::ScalarT;
  using MeshScalarT = typename EvalT::MeshScalarT;

  ///
  /// Constructor
  ///
  Porosity(Teuchos::ParameterList& p, const Teuchos::RCP<Albany::Layouts>& dl);

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

  ///
  /// Sacado method to access parameters
  ///
  ScalarT&
  getValue(std::string const& n);

 private:
  ///
  /// Number of integration points
  ///
  int numQPs;

  ///
  /// Number of problem dimensions
  ///
  int numDims;

  ///
  /// Container for coordinates
  ///
  PHX::MDField<const MeshScalarT, Cell, QuadPoint, Dim> coordVec;

  ///
  /// Container for porosity
  ///
  PHX::MDField<ScalarT, Cell, QuadPoint> porosity;

  ///
  /// Is porosity constant, or random field
  ///
  bool is_constant;

  ///
  /// Constant value
  ///
  ScalarT constant_value;

  ///
  /// Optional dependence on strain and porePressure
  ///
  /// porosity holds linear relation to volumetric strain
  PHX::MDField<ScalarT const, Cell, QuadPoint, Dim, Dim> strain;

  ///
  /// Optional dependence on det(F)
  ///
  PHX::MDField<ScalarT const, Cell, QuadPoint> J;

  ///
  /// flag to indicated usage in poroelastic context
  ///
  bool isPoroElastic;

  ///
  /// flag
  ///
  bool isCompressibleSolidPhase;

  ///
  /// flag
  ///
  bool isCompressibleFluidPhase;

  ///
  /// initial state
  ///
  ScalarT initialPorosityValue;

  ///
  /// For compressible grain
  ///
  PHX::MDField<ScalarT const, Cell, QuadPoint> biotCoefficient;

  ///
  /// For compressible grain
  ///
  PHX::MDField<ScalarT const, Cell, QuadPoint> porePressure;

  ///
  /// For compressible grain
  ///
  ScalarT GrainBulkModulus;

  ///
  /// For THM porous media
  ///
  PHX::MDField<ScalarT const, Cell, QuadPoint> Temperature;

  ///
  /// For THM porous media
  ///
  PHX::MDField<ScalarT const, Cell, QuadPoint> skeletonThermalExpansion;

  ///
  /// For THM porous media
  ///
  PHX::MDField<ScalarT const, Cell, QuadPoint> refTemperature;

  ///
  /// Values of the random variables
  ///
  Teuchos::Array<ScalarT> rv;

  ///
  /// Strain flag
  ///
  bool hasStrain;

  ///
  /// J flag
  ///
  bool hasJ;

  ///
  /// J flag
  ///
  bool hasTemp;
};
}  // namespace LCM

#endif
