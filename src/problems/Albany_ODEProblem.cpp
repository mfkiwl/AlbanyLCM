// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#include "Albany_ODEProblem.hpp"

#include "Albany_BCUtils.hpp"
#include "Albany_ProblemUtils.hpp"
#include "Albany_Utils.hpp"
#include "PHAL_FactoryTraits.hpp"
#include "Shards_CellTopology.hpp"

Albany::ODEProblem::ODEProblem(const Teuchos::RCP<Teuchos::ParameterList>& params_, const Teuchos::RCP<ParamLib>& paramLib_, int const numDim_)
    : Albany::AbstractProblem(params_, paramLib_, 2), numDim(numDim_), use_sdbcs_(false)
{
}

Albany::ODEProblem::~ODEProblem() {}

void
Albany::ODEProblem::buildProblem(Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct>> meshSpecs, Albany::StateManager& stateMgr)
{
  /* Construct All Phalanx Evaluators */
  ALBANY_PANIC(meshSpecs.size() != 1, "Problem supports one Material Block");
  fm.resize(1);
  fm[0] = Teuchos::rcp(new PHX::FieldManager<PHAL::AlbanyTraits>);
  buildEvaluators(*fm[0], *meshSpecs[0], stateMgr, BUILD_RESID_FM, Teuchos::null);
  constructDirichletEvaluators(*meshSpecs[0]);
}

Teuchos::Array<Teuchos::RCP<const PHX::FieldTag>>
Albany::ODEProblem::buildEvaluators(
    PHX::FieldManager<PHAL::AlbanyTraits>&      fm0,
    Albany::MeshSpecsStruct const&              meshSpecs,
    Albany::StateManager&                       stateMgr,
    Albany::FieldManagerChoice                  fmchoice,
    const Teuchos::RCP<Teuchos::ParameterList>& responseList)
{
  // Call constructeEvaluators<EvalT>(*rfm[0], *meshSpecs[0], stateMgr);
  // for each EvalT in PHAL::AlbanyTraits::BEvalTypes
  ConstructEvaluatorsOp<ODEProblem>                     op(*this, fm0, meshSpecs, stateMgr, fmchoice, responseList);
  Sacado::mpl::for_each<PHAL::AlbanyTraits::BEvalTypes> fe(op);
  return *op.tags;
}

void
Albany::ODEProblem::constructDirichletEvaluators(Albany::MeshSpecsStruct const& meshSpecs)
{
  // Construct Dirichlet evaluators for all nodesets and names
  std::vector<std::string> dirichletNames(neq);
  dirichletNames[0] = "X";
  dirichletNames[1] = "Y";
  Albany::BCUtils<Albany::DirichletTraits> dirUtils;
  dfm         = dirUtils.constructBCEvaluators(meshSpecs.nsNames, dirichletNames, this->params, this->paramLib);
  use_sdbcs_  = dirUtils.useSDBCs();
  offsets_    = dirUtils.getOffsets();
  nodeSetIDs_ = dirUtils.getNodeSetIDs();
}

Teuchos::RCP<Teuchos::ParameterList const>
Albany::ODEProblem::getValidProblemParameters() const
{
  Teuchos::RCP<Teuchos::ParameterList> validPL = this->getGenericProblemParams("ValidODEProblemParams");

  return validPL;
}
