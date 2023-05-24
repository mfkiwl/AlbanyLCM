// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#ifndef PHAL_SAVE_SIDE_SET_STATE_FIELD_HPP
#define PHAL_SAVE_SIDE_SET_STATE_FIELD_HPP

#include "Albany_Layouts.hpp"
#include "PHAL_AlbanyTraits.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_MDField.hpp"
#include "Phalanx_config.hpp"
#include "Teuchos_ParameterList.hpp"

namespace PHAL {
/** \brief SaveSideSetStatField

*/

template <typename EvalT, typename Traits>
class SaveSideSetStateField : public PHX::EvaluatorWithBaseImpl<Traits>, public PHX::EvaluatorDerived<EvalT, Traits>
{
 public:
  SaveSideSetStateField(Teuchos::ParameterList const& p, const Teuchos::RCP<Albany::Layouts>& dl);

  void
  postRegistrationSetup(typename Traits::SetupData d, PHX::FieldManager<Traits>& fm);

  void
  evaluateFields(typename Traits::EvalData workset);
};

// =========================== SPECIALIZATION ========================= //

template <typename Traits>
class SaveSideSetStateField<PHAL::AlbanyTraits::Residual, Traits> : public PHX::EvaluatorWithBaseImpl<Traits>,
                                                                    public PHX::EvaluatorDerived<PHAL::AlbanyTraits::Residual, Traits>
{
 public:
  SaveSideSetStateField(Teuchos::ParameterList const& p, const Teuchos::RCP<Albany::Layouts>& dl);

  void
  postRegistrationSetup(typename Traits::SetupData d, PHX::FieldManager<Traits>& fm);

  void
  evaluateFields(typename Traits::EvalData d);

 private:
  void
  saveElemState(typename Traits::EvalData d);
  void
  saveNodeState(typename Traits::EvalData d);

  typedef typename PHAL::AlbanyTraits::Residual::ScalarT ScalarT;

  Teuchos::RCP<PHX::FieldTag> savestate_operation;
  PHX::MDField<ScalarT const> field;

  std::string sideSetName;
  std::string fieldName;
  std::string stateName;

  bool                          nodalState;
  std::vector<std::vector<int>> sideNodes;
};

}  // Namespace PHAL

#endif  // PHAL_SAVE_SIDE_SET_STATE_FIELD_HPP
