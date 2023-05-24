// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#include "Albany_AdvDiffProblem.hpp"

#include <string>

#include "Albany_BCUtils.hpp"
#include "Albany_ProblemUtils.hpp"
#include "Albany_Utils.hpp"
#include "Intrepid2_DefaultCubatureFactory.hpp"
#include "PHAL_FactoryTraits.hpp"
#include "Shards_CellTopology.hpp"

Albany::AdvDiffProblem::AdvDiffProblem(const Teuchos::RCP<Teuchos::ParameterList>& params_, const Teuchos::RCP<ParamLib>& paramLib_, int const numDim_)
    : Albany::AbstractProblem(params_, paramLib_), numDim(numDim_), use_sdbcs_(false)
{
  // Get number of species equations from Problem specifications
  neq             = params_->get("Number of PDE Equations", numDim);
  bool useAugForm = params_->sublist("Options").get<bool>("Use Augmented Form", false);
  if (useAugForm)  // if we're using the augmented form of the equations, there
                   // are 2 extra auxiliary dofs / node (in 2D).
    neq = neq + 2;
  std::cout << "useAugForm, neq: " << useAugForm << ", " << neq << std::endl;
}

Albany::AdvDiffProblem::~AdvDiffProblem() {}

void
Albany::AdvDiffProblem::buildProblem(Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct>> meshSpecs, Albany::StateManager& stateMgr)
{
  using Teuchos::rcp;

  /* Construct All Phalanx Evaluators */
  ALBANY_PANIC(meshSpecs.size() != 1, "Problem supports one Material Block");
  fm.resize(1);
  fm[0] = rcp(new PHX::FieldManager<PHAL::AlbanyTraits>);
  buildEvaluators(*fm[0], *meshSpecs[0], stateMgr, BUILD_RESID_FM, Teuchos::null);
  constructDirichletEvaluators(*meshSpecs[0]);
}

Teuchos::Array<Teuchos::RCP<const PHX::FieldTag>>
Albany::AdvDiffProblem::buildEvaluators(
    PHX::FieldManager<PHAL::AlbanyTraits>&      fm0,
    Albany::MeshSpecsStruct const&              meshSpecs,
    Albany::StateManager&                       stateMgr,
    Albany::FieldManagerChoice                  fmchoice,
    const Teuchos::RCP<Teuchos::ParameterList>& responseList)
{
  // Call constructeEvaluators<EvalT>(*rfm[0], *meshSpecs[0], stateMgr);
  // for each EvalT in PHAL::AlbanyTraits::BEvalTypes
  ConstructEvaluatorsOp<AdvDiffProblem>                 op(*this, fm0, meshSpecs, stateMgr, fmchoice, responseList);
  Sacado::mpl::for_each<PHAL::AlbanyTraits::BEvalTypes> fe(op);
  return *op.tags;
}

void
Albany::AdvDiffProblem::constructDirichletEvaluators(Albany::MeshSpecsStruct const& meshSpecs)
{
  // Construct Dirichlet evaluators for all nodesets and names
  std::vector<std::string> dirichletNames(neq);
  for (int i = 0; i < neq; i++) {
    std::stringstream s;
    s << "U" << i;
    dirichletNames[i] = s.str();
  }
  Albany::BCUtils<Albany::DirichletTraits> dirUtils;
  dfm         = dirUtils.constructBCEvaluators(meshSpecs.nsNames, dirichletNames, this->params, this->paramLib);
  use_sdbcs_  = dirUtils.useSDBCs();
  offsets_    = dirUtils.getOffsets();
  nodeSetIDs_ = dirUtils.getNodeSetIDs();
}

Teuchos::RCP<Teuchos::ParameterList const>
Albany::AdvDiffProblem::getValidProblemParameters() const
{
  Teuchos::RCP<Teuchos::ParameterList> validPL = this->getGenericProblemParams("ValidAdvDiffProblemParams");

  validPL->set("Number of PDE Equations", 1, "Number of PDE Equations in AdvDiff equation set");
  validPL->sublist("Options", false, "");

  return validPL;
}
