// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.
#include "Albany_ObserverImpl.hpp"

#include "Albany_AbstractDiscretization.hpp"
#include "Albany_DistributedParameterLibrary.hpp"

namespace Albany {

ObserverImpl::ObserverImpl(const Teuchos::RCP<Application>& app) : StatelessObserverImpl(app) {}

void
ObserverImpl::observeSolution(
    double                                  stamp,
    Thyra_Vector const&                     nonOverlappedSolution,
    const Teuchos::Ptr<Thyra_Vector const>& nonOverlappedSolutionDot,
    const Teuchos::Ptr<Thyra_Vector const>& nonOverlappedSolutionDotDot)
{
  app_->evaluateStateFieldManager(stamp, nonOverlappedSolution, nonOverlappedSolutionDot, nonOverlappedSolutionDotDot);

  app_->getStateMgr().updateStates();

  //! update distributed parameters in the mesh
  auto distParamLib = app_->getDistributedParameterLibrary();
  auto disc         = app_->getDiscretization();
  distParamLib->scatter();
  for (auto it : *distParamLib) {
    disc->setField(
        *it.second->overlapped_vector(),
        it.second->name(),
        /*overlapped*/ true);
  }

  StatelessObserverImpl::observeSolution(stamp, nonOverlappedSolution, nonOverlappedSolutionDot, nonOverlappedSolutionDotDot);
}

void
ObserverImpl::observeSolution(double stamp, const Thyra_MultiVector& nonOverlappedSolution)
{
  app_->evaluateStateFieldManager(stamp, nonOverlappedSolution);
  app_->getStateMgr().updateStates();
  StatelessObserverImpl::observeSolution(stamp, nonOverlappedSolution);
}

void
ObserverImpl::parameterChanged(std::string const& param)
{
  //! If a parameter has changed in value, saved/unsaved fields must be updated
  auto out = Teuchos::VerboseObjectBase::getDefaultOStream();
  *out << param << " has changed!" << std::endl;
  app_->getPhxSetup()->init_unsaved_param(param);
}

}  // namespace Albany
