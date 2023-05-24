// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#ifndef ALBANY_DISTRIBUTED_PARAMETER_DERIVATIVE_OP_HPP
#define ALBANY_DISTRIBUTED_PARAMETER_DERIVATIVE_OP_HPP

#include "Albany_Application.hpp"
#include "Albany_DistributedParameterLibrary.hpp"
#include "Albany_ThyraTypes.hpp"
#include "Teuchos_RCP.hpp"

namespace Albany {

//! Thyra_LinearOp implementing the action of df/dp (transpose)
/*!
 * This class implements the Thyra::LinearOpBase interface for
 * op(df/dp)*v where op() is the identity or tranpose, f is the Albany
 * residual vector, p is a distributed parameter vector, and v is a given
 * vector.
 */
class DistributedParameterDerivativeOp : public Thyra_LinearOp
{
 public:
  // Constructor
  DistributedParameterDerivativeOp(const Teuchos::RCP<Application>& app_, std::string const& param_name_) : app(app_), param_name(param_name_) {}

  //! Destructor
  virtual ~DistributedParameterDerivativeOp() {}

  //! Set values needed for apply()
  void
  set(double const                                  time_,
      Teuchos::RCP<Thyra_Vector const> const&       x_,
      Teuchos::RCP<Thyra_Vector const> const&       xdot_,
      Teuchos::RCP<Thyra_Vector const> const&       xdotdot_,
      const Teuchos::RCP<Teuchos::Array<ParamVec>>& scalar_params_)
  {
    time          = time_;
    xdot          = xdot_;
    xdotdot       = xdotdot_;
    x             = x_;
    scalar_params = scalar_params_;
  }

  //! Overrides Thyra::LinearOpBase purely virtual method
  Teuchos::RCP<Thyra_VectorSpace const>
  domain() const
  {
    return app->getDistributedParameterLibrary()->get(param_name)->vector_space();
  }

  //! Overrides Thyra::LinearOpBase purely virtual method
  Teuchos::RCP<Thyra_VectorSpace const>
  range() const
  {
    return app->getVectorSpace();
  }

  //@}

 protected:
  //! Overrides Thyra::LinearOpBase purely virtual method
  bool
  opSupportedImpl(Thyra::EOpTransp /*M_trans*/) const
  {
    // The underlying scalar type is not complex, and we support transpose, so
    // we support everything.
    return true;
  }

  //! Overrides Thyra::LinearOpBase purely virtual method
  void
  applyImpl(const Thyra::EOpTransp M_trans, const Thyra_MultiVector& X, const Teuchos::Ptr<Thyra_MultiVector>& Y, const ST /* alpha */, const ST /* beta */)
      const
  {
    bool use_transpose = (Thyra::real_trans(M_trans) == Thyra::TRANS);
    app->applyGlobalDistParamDerivImpl(time, x, xdot, xdotdot, *scalar_params, param_name, use_transpose, Teuchos::rcpFromRef(X), Teuchos::rcpFromPtr(Y));
  }

  //! Albany applications
  Teuchos::RCP<Application> app;

  //! Name of distributed parameter we are differentiating w.r.t.
  std::string param_name;

  //! @name Data needed for apply()
  //@{

  //! Current time
  double time;

  //! Velocity vector
  Teuchos::RCP<Thyra_Vector const> xdot;

  //! Acceleration vector
  Teuchos::RCP<Thyra_Vector const> xdotdot;

  //! Solution vector
  Teuchos::RCP<Thyra_Vector const> x;

  //! Scalar parameters
  Teuchos::RCP<Teuchos::Array<ParamVec>> scalar_params;

  //@}

};  // class DistributedParameterDerivativeOp

}  // namespace Albany

#endif  // ALBANY_DISTRIBUTED_PARAMETER_DERIVATIVE_OP_HPP
