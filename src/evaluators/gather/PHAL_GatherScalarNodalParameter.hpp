// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#ifndef PHAL_GATHER_SCALAR_NODAL_PARAMETER_HPP
#define PHAL_GATHER_SCALAR_NODAL_PARAMETER_HPP

#include "Albany_Layouts.hpp"
#include "PHAL_AlbanyTraits.hpp"
#include "PHAL_Dimension.hpp"
#include "PHAL_Utilities.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_MDField.hpp"
#include "Phalanx_config.hpp"
#include "Teuchos_ParameterList.hpp"

namespace PHAL {
/** \brief Gathers parameter values from distributed vectors into
    scalar nodal fields of the field manager

    Currently makes an assumption that the stride is constant for dofs
    and that the nmber of dofs is equal to the size of the solution
    names vector.

*/
// **************************************************************
// Base Class with Generic Implementations: Specializations for
// Automatic Differentiation Below
// **************************************************************

template <typename EvalT, typename Traits>
class GatherScalarNodalParameterBase : public PHX::EvaluatorWithBaseImpl<Traits>, public PHX::EvaluatorDerived<EvalT, Traits>
{
 public:
  GatherScalarNodalParameterBase(Teuchos::ParameterList const& p, const Teuchos::RCP<Albany::Layouts>& dl);
  void
  postRegistrationSetup(typename Traits::SetupData d, PHX::FieldManager<Traits>& vm);

  // This function requires template specialization, in derived class below
  virtual void
  evaluateFields(typename Traits::EvalData d) = 0;
  virtual ~GatherScalarNodalParameterBase()   = default;

 protected:
  typedef typename EvalT::ParamScalarT ParamScalarT;

  std::size_t const numNodes;
  std::string const param_name;

  // Output:
  PHX::MDField<ParamScalarT, Cell, Node> val;
};

// General version for most evaluation types
template <typename EvalT, typename Traits>
class GatherScalarNodalParameter : public GatherScalarNodalParameterBase<EvalT, Traits>
{
 public:
  GatherScalarNodalParameter(Teuchos::ParameterList const& p, const Teuchos::RCP<Albany::Layouts>& dl);
  // Old constructor, still needed by BCs that use PHX Factory
  GatherScalarNodalParameter(Teuchos::ParameterList const& p);
  //  void postRegistrationSetup(typename Traits::SetupData d,
  //  PHX::FieldManager<Traits>& vm);
  void
  evaluateFields(typename Traits::EvalData d);

 private:
  typedef typename EvalT::ParamScalarT ParamScalarT;
};

// General version for most evaluation types
template <typename EvalT, typename Traits>
class GatherScalarExtruded2DNodalParameter : public GatherScalarNodalParameterBase<EvalT, Traits>
{
 public:
  GatherScalarExtruded2DNodalParameter(Teuchos::ParameterList const& p, const Teuchos::RCP<Albany::Layouts>& dl);
  void
  evaluateFields(typename Traits::EvalData d);

 private:
  typedef typename EvalT::ParamScalarT ParamScalarT;
  int const                            fieldLevel;
};

}  // namespace PHAL

#endif  // PHAL_GATHER_SCALAR_NODAL_PARAMETER_HPP
