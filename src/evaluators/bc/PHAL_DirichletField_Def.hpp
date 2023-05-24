// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#include "Albany_AbstractDiscretization.hpp"
#include "Albany_DistributedParameterLibrary.hpp"
#include "Albany_GlobalLocalIndexer.hpp"
#include "Albany_Macros.hpp"
#include "Albany_NodalDOFManager.hpp"
#include "Albany_ThyraUtils.hpp"
#include "PHAL_DirichletField.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Sacado_ParameterRegistration.hpp"

// **********************************************************************
// Genereric Template Code for Constructor and PostRegistrationSetup
// **********************************************************************

namespace PHAL {

template <typename EvalT, typename Traits>
DirichletField_Base<EvalT, Traits>::DirichletField_Base(Teuchos::ParameterList& p) : PHAL::DirichletBase<EvalT, Traits>(p)
{
  // Get field type and corresponding layouts
  field_name = p.get<std::string>("Field Name");
}

// **********************************************************************
// Specialization: Residual
// **********************************************************************
template <typename Traits>
DirichletField<PHAL::AlbanyTraits::Residual, Traits>::DirichletField(Teuchos::ParameterList& p) : DirichletField_Base<PHAL::AlbanyTraits::Residual, Traits>(p)
{
}

// **********************************************************************
template <typename Traits>
void
DirichletField<PHAL::AlbanyTraits::Residual, Traits>::evaluateFields(typename Traits::EvalData dirichletWorkset)
{
  const Albany::NodalDOFManager& fieldDofManager = dirichletWorkset.disc->getDOFManager(this->field_name);
  // MP: If the parameter is scalar, then the parameter offset is seto to zero.
  // Otherwise the parameter offset is the same of the solution's one.
  auto                   fieldNodeVs   = dirichletWorkset.disc->getNodeVectorSpace(this->field_name);
  auto                   fieldVs       = dirichletWorkset.disc->getVectorSpace(this->field_name);
  bool                   isFieldScalar = (fieldNodeVs->dim() == fieldVs->dim());
  int                    fieldOffset   = isFieldScalar ? 0 : this->offset;
  std::vector<GO> const& nsNodesGIDs   = dirichletWorkset.disc->getNodeSetGIDs().find(this->nodeSetID)->second;

  Teuchos::RCP<Thyra_Vector const> pvec        = dirichletWorkset.distParamLib->get(this->field_name)->vector();
  Teuchos::ArrayRCP<const ST>      p_constView = Albany::getLocalData(pvec);

  Teuchos::RCP<Thyra_Vector const> x = dirichletWorkset.x;
  Teuchos::RCP<Thyra_Vector>       f = dirichletWorkset.f;

  Teuchos::ArrayRCP<const ST> x_constView    = Albany::getLocalData(x);
  Teuchos::ArrayRCP<ST>       f_nonconstView = Albany::getNonconstLocalData(f);

  std::vector<std::vector<int>> const& nsNodes            = dirichletWorkset.nodeSets->find(this->nodeSetID)->second;
  auto                                 field_node_indexer = Albany::createGlobalLocalIndexer(fieldNodeVs);
  for (unsigned int inode = 0; inode < nsNodes.size(); inode++) {
    int lunk             = nsNodes[inode][this->offset];
    GO  node_gid         = nsNodesGIDs[inode];
    int lfield           = fieldDofManager.getLocalDOF(field_node_indexer->getLocalElement(node_gid), fieldOffset);
    f_nonconstView[lunk] = x_constView[lunk] - p_constView[lfield];
  }
}

// **********************************************************************
// Specialization: Jacobian
// **********************************************************************
template <typename Traits>
DirichletField<PHAL::AlbanyTraits::Jacobian, Traits>::DirichletField(Teuchos::ParameterList& p) : DirichletField_Base<PHAL::AlbanyTraits::Jacobian, Traits>(p)
{
}

// **********************************************************************
template <typename Traits>
void
DirichletField<PHAL::AlbanyTraits::Jacobian, Traits>::evaluateFields(typename Traits::EvalData dirichletWorkset)
{
  const Albany::NodalDOFManager& fieldDofManager = dirichletWorkset.disc->getDOFManager(this->field_name);
  auto                           fieldNodeVs     = dirichletWorkset.disc->getNodeVectorSpace(this->field_name);
  auto                           fieldVs         = dirichletWorkset.disc->getVectorSpace(this->field_name);
  bool                           isFieldScalar   = (fieldNodeVs->dim() == fieldVs->dim());
  int                            fieldOffset     = isFieldScalar ? 0 : this->offset;
  std::vector<GO> const&         nsNodesGIDs     = dirichletWorkset.disc->getNodeSetGIDs().find(this->nodeSetID)->second;

  Teuchos::RCP<Thyra_Vector const> pvec        = dirichletWorkset.distParamLib->get(this->field_name)->vector();
  Teuchos::ArrayRCP<const ST>      p_constView = Albany::getLocalData(pvec);

  Teuchos::RCP<Thyra_Vector const> x   = dirichletWorkset.x;
  Teuchos::RCP<Thyra_Vector>       f   = dirichletWorkset.f;
  Teuchos::RCP<Thyra_LinearOp>     jac = dirichletWorkset.Jac;

  Teuchos::ArrayRCP<const ST> x_constView;
  Teuchos::ArrayRCP<ST>       f_nonconstView;

  const RealType                       j_coeff = dirichletWorkset.j_coeff;
  std::vector<std::vector<int>> const& nsNodes = dirichletWorkset.nodeSets->find(this->nodeSetID)->second;

  bool fillResid = (f != Teuchos::null);
  if (fillResid) {
    x_constView    = Albany::getLocalData(x);
    f_nonconstView = Albany::getNonconstLocalData(f);
  }

  Teuchos::Array<LO> index(1);
  Teuchos::Array<ST> value(1);
  value[0] = j_coeff;
  Teuchos::Array<ST> matrixEntries;
  Teuchos::Array<LO> matrixIndices;

  auto field_node_indexer = Albany::createGlobalLocalIndexer(fieldNodeVs);
  for (unsigned int inode = 0; inode < nsNodes.size(); inode++) {
    int lunk = nsNodes[inode][this->offset];
    index[0] = lunk;

    // Extract the row, zero it out, then put j_coeff on diagonal
    Albany::getLocalRowValues(jac, lunk, matrixIndices, matrixEntries);
    for (auto& val : matrixEntries) {
      val = 0.0;
    }
    Albany::setLocalRowValues(jac, lunk, matrixIndices(), matrixEntries());
    Albany::setLocalRowValues(jac, lunk, index(), value());

    if (fillResid) {
      GO  node_gid         = nsNodesGIDs[inode];
      int lfield           = fieldDofManager.getLocalDOF(field_node_indexer->getLocalElement(node_gid), fieldOffset);
      f_nonconstView[lunk] = x_constView[lunk] - p_constView[lfield];
    }
  }
}

}  // namespace PHAL
