// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#ifndef PHAL_SAVECELLSTATEFIELD_HPP
#define PHAL_SAVECELLSTATEFIELD_HPP

#include "Albany_Types.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_MDField.hpp"
#include "Phalanx_config.hpp"
#include "Teuchos_ParameterList.hpp"

namespace PHAL {
/** \brief SaveCellStateField

*/

template <typename EvalT, typename Traits>
class SaveCellStateField : public PHX::EvaluatorWithBaseImpl<Traits>, public PHX::EvaluatorDerived<EvalT, Traits>
{
 public:
  SaveCellStateField(Teuchos::ParameterList const& p);

  void
  postRegistrationSetup(typename Traits::SetupData d, PHX::FieldManager<Traits>& vm);

  void
  evaluateFields(typename Traits::EvalData d);

 private:
  typedef typename EvalT::ScalarT     ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;
};

template <typename Traits>
class SaveCellStateField<PHAL::AlbanyTraits::Residual, Traits> : public PHX::EvaluatorWithBaseImpl<Traits>,
                                                                 public PHX::EvaluatorDerived<PHAL::AlbanyTraits::Residual, Traits>
{
 public:
  SaveCellStateField(Teuchos::ParameterList const& p);

  void
  postRegistrationSetup(typename Traits::SetupData d, PHX::FieldManager<Traits>& vm);

  void
  evaluateFields(typename Traits::EvalData d);

 private:
  typedef typename PHAL::AlbanyTraits::Residual::ScalarT     ScalarT;
  typedef typename PHAL::AlbanyTraits::Residual::MeshScalarT MeshScalarT;

  Teuchos::RCP<PHX::FieldTag>                      savestate_operation;
  PHX::MDField<ScalarT const>                      field;
  PHX::MDField<const MeshScalarT, Cell, QuadPoint> weights;
  std::string                                      fieldName;
  std::string                                      stateName;
  int                                              i_index;
  int                                              j_index;
  int                                              k_index;
};
}  // namespace PHAL

#endif
