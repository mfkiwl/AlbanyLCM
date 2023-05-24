// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#ifndef ADAPT_ELEMENT_SIZE_HPP
#define ADAPT_ELEMENT_SIZE_HPP

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

namespace Adapt {

template <typename EvalT, typename Traits>
class ElementSizeFieldBase : public PHX::EvaluatorWithBaseImpl<Traits>, public PHX::EvaluatorDerived<EvalT, Traits>
{
 public:
  typedef typename EvalT::ScalarT     ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;
  ElementSizeFieldBase(Teuchos::ParameterList& p, const Teuchos::RCP<Albany::Layouts>& dl);

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
    return size_field_tag;
  }

  Teuchos::RCP<const PHX::FieldTag>
  getResponseFieldTag() const
  {
    return size_field_tag;
  }

 protected:
  typedef enum
  {
    NOTSCALED,
    SCALAR,
    VECTOR
  } ScalingType;

  Teuchos::RCP<Teuchos::ParameterList const>
  getValidSizeFieldParameters() const;

  void
  getCellRadius(std::size_t const cell, MeshScalarT& cellRadius) const;

  std::string scalingName;
  std::string className;

  std::size_t numQPs;
  std::size_t numDims;
  std::size_t numVertices;

  PHX::MDField<const MeshScalarT, Cell, QuadPoint>      qp_weights;
  PHX::MDField<const MeshScalarT, Cell, QuadPoint, Dim> coordVec;
  PHX::MDField<const MeshScalarT, Cell, Node, Dim>      coordVec_vertices;

  bool        outputToExodus;
  bool        outputCellAverage;
  bool        outputQPData;
  bool        outputNodeData;
  bool        isAnisotropic;
  ScalingType scalingType;

  Teuchos::RCP<PHX::Tag<ScalarT>> size_field_tag;
  Albany::StateManager*           pStateMgr;
};

template <typename EvalT, typename Traits>
class ElementSizeField : public ElementSizeFieldBase<EvalT, Traits>
{
 public:
  ElementSizeField(Teuchos::ParameterList& p, const Teuchos::RCP<Albany::Layouts>& dl) : ElementSizeFieldBase<EvalT, Traits>(p, dl) {}

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
class ElementSizeField<PHAL::AlbanyTraits::Residual, Traits> : public ElementSizeFieldBase<PHAL::AlbanyTraits::Residual, Traits>
{
 public:
  ElementSizeField(Teuchos::ParameterList& p, const Teuchos::RCP<Albany::Layouts>& dl);
  void
  preEvaluate(typename Traits::PreEvalData d);
  void
  postEvaluate(typename Traits::PostEvalData d);
  void
  evaluateFields(typename Traits::EvalData d);
};

}  // namespace Adapt

#endif  // ADAPT_ELEMENT_SIZE_HPP
