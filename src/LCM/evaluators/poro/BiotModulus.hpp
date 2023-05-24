// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#ifndef BIOT_MODULUS_HPP
#define BIOT_MODULUS_HPP

#include "Albany_Types.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_MDField.hpp"
#include "Phalanx_config.hpp"
#include "Sacado_ParameterAccessor.hpp"
#include "Teuchos_Array.hpp"
#include "Teuchos_ParameterList.hpp"

namespace LCM {
/**
 * \brief Evaluates Biot modulus, either as a constant or a truncated
 * KL expansion.
 */

template <typename EvalT, typename Traits>
class BiotModulus : public PHX::EvaluatorWithBaseImpl<Traits>, public PHX::EvaluatorDerived<EvalT, Traits>, public Sacado::ParameterAccessor<EvalT, SPL_Traits>
{
 public:
  using ScalarT     = typename EvalT::ScalarT;
  using MeshScalarT = typename EvalT::MeshScalarT;

  BiotModulus(Teuchos::ParameterList& p);

  void
  postRegistrationSetup(typename Traits::SetupData d, PHX::FieldManager<Traits>& vm);

  void
  evaluateFields(typename Traits::EvalData d);

  ScalarT&
  getValue(std::string const& n);

 private:
  int                                                   numQPs;
  int                                                   numDims;
  PHX::MDField<const MeshScalarT, Cell, QuadPoint, Dim> coordVec;
  PHX::MDField<ScalarT, Cell, QuadPoint>                biotModulus;

  //! Is conductivity constant, or random field
  bool is_constant;

  //! Constant value
  ScalarT constant_value;

  //! Optional dependence on Temperature (E = E_const + dEdT * T)
  // PHX::MDField<ScalarT,Cell,QuadPoint> Temperature;
  PHX::MDField<ScalarT const, Cell, QuadPoint> porosity;
  PHX::MDField<ScalarT const, Cell, QuadPoint> biotCoefficient;
  // bool isThermoElastic;
  bool isPoroElastic;
  // ScalarT dEdT_value;
  ScalarT FluidBulkModulus;
  ScalarT GrainBulkModulus;

  //! Values of the random variables
  Teuchos::Array<ScalarT> rv;
};
}  // namespace LCM

#endif
