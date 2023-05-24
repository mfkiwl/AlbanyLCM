// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#ifndef PHAL_SAVENODALFIELD_HPP
#define PHAL_SAVENODALFIELD_HPP

#include "Albany_Layouts.hpp"
#include "PHAL_AlbanyTraits.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_MDField.hpp"
#include "Teuchos_ParameterList.hpp"

namespace Albany {
class StateManager;
}

namespace PHAL {

template <typename EvalT, typename Traits>
class SaveNodalFieldBase : public PHX::EvaluatorWithBaseImpl<Traits>, public PHX::EvaluatorDerived<EvalT, Traits>
{
 public:
  typedef typename EvalT::ScalarT     ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;

  SaveNodalFieldBase(Teuchos::ParameterList& p, const Teuchos::RCP<Albany::Layouts>& dl);

  void
  postRegistrationSetup(typename Traits::SetupData d, PHX::FieldManager<Traits>& vm);

  // These functions are defined in the specializations
  void
  preEvaluate(typename Traits::PreEvalData d) = 0;
  void
  postEvaluate(typename Traits::PostEvalData d) = 0;
  void
  evaluateFields(typename Traits::EvalData d) = 0;

  Teuchos::RCP<const PHX::FieldTag>
  getEvaluatedFieldTag() const
  {
    return nodal_field_tag;
  }

  Teuchos::RCP<const PHX::FieldTag>
  getResponseFieldTag() const
  {
    return nodal_field_tag;
  }

 protected:
  std::string              xName;
  std::string              xdotName;
  std::string              xdotdotName;
  static std::string const className;

  Teuchos::RCP<PHX::Tag<ScalarT>> nodal_field_tag;
  Albany::StateManager*           pStateMgr;
};

template <typename EvalT, typename Traits>
class SaveNodalField : public SaveNodalFieldBase<EvalT, Traits>
{
 public:
  SaveNodalField(Teuchos::ParameterList& p, const Teuchos::RCP<Albany::Layouts>& dl) : SaveNodalFieldBase<EvalT, Traits>(p, dl) {}

  void preEvaluate(typename Traits::PreEvalData /* d */) {}
  void postEvaluate(typename Traits::PostEvalData /* d */) {}
  void evaluateFields(typename Traits::EvalData /* d */) {}
};

// **************************************************************
// **************************************************************
// * Specializations
// **************************************************************
// **************************************************************

// **************************************************************
// Residual
// **************************************************************
template <typename Traits>
class SaveNodalField<PHAL::AlbanyTraits::Residual, Traits> : public SaveNodalFieldBase<PHAL::AlbanyTraits::Residual, Traits>
{
 public:
  SaveNodalField(Teuchos::ParameterList& p, const Teuchos::RCP<Albany::Layouts>& dl);
  void
  preEvaluate(typename Traits::PreEvalData d);
  void
  postEvaluate(typename Traits::PostEvalData d);
  void
  evaluateFields(typename Traits::EvalData d);
};

}  // namespace PHAL

#endif  // PHAL_SAVE_NODAL_FIELD_HPP
