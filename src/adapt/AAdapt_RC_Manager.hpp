// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#ifndef AADAPT_RC_MANAGER_HPP
#define AADAPT_RC_MANAGER_HPP

#include "AAdapt_RC_DataTypes.hpp"
#include "Albany_DataTypes.hpp"

// Forward declarations.
namespace Albany {
class StateManager;
class Layouts;
}  // namespace Albany
namespace PHAL {
struct AlbanyTraits;
struct Workset;
}  // namespace PHAL
namespace PHX {
template <typename T>
class FieldManager;
}

namespace AAdapt {

// Forward declaration(s)
class AdaptiveSolutionManager;

namespace rc {

/*! \brief Manage reference configuration (RC) data for RC updating (RCU).
 *
 * Equations are written relative to a reference configuration of the mesh. If
 * the mesh deforms a lot, defining quantities relative to a single initial
 * reference configuration becomes a problem. One solution to this problem is to
 * update the reference configuration. Then certain quantities must be
 * maintained to make the reference configuration complete.
 *   rc::Manager is the interface between the problem, the evaluators, and
 * AAdapt::MeshAdapt. It maintains a database of accumulated quantites (q_accum)
 * that are combined with incremental quantities (q_incr).
 *   rc::Manager deploys the evaluators rc::Reader and rc::Writer to read from
 * the database and write back to it. These two evaluators make q_accum
 * quantities available to the physics evaluators.
 *   rc::Field is a wrapper to an MDField that holds the q_accum quantity for a
 * given q_incr. A physics evaluator containing a q_incr field uses an rc::Field
 * to gain access to the associated q_accum. rc::Field implements the various
 * operations needed to combine q_accum and q_incr.
 *   The BC field managers are given x (the solution, which is displacment) +
 * x_accum (the accumulated solution). This lets time-dependent BCs work
 * naturally. Volume field managers are given just x.
 *
 *   RCU is controlled by these parameters:
 * \code
 *     <ParameterList name="Adaptation">
 *       <Parameter name="Reference Configuration: Update"  type="bool"
 *        value="true"/>
 *       <Parameter name="Reference Configuration: Project" type="bool"
 *        value="true"/>
 *     </ParameterList>
 * \endcode
 */
class Manager
{
 public:
  /* Methods to set up the RCU framework. */

  //! Static constructor that may return Teuchos::null depending on the contents
  //  of the parameter list.
  static Teuchos::RCP<Manager>
  create(const Teuchos::RCP<Albany::StateManager>& state_mgr, Teuchos::ParameterList& problem_params);

  void
  setSolutionManager(const Teuchos::RCP<AdaptiveSolutionManager>& sol_mgr);

  //! Fill valid_pl with valid parameters for this class.
  static void
  getValidParameters(Teuchos::RCP<Teuchos::ParameterList>& valid_pl);

  /* Methods to maintain accumulated displacement, in part for use in the BC
   * evaluators. */

  //! Nonconst x getter.
  Teuchos::RCP<Thyra_Vector>&
  get_x();
  //! Initialize x_accum using this nonoverlapping map if x_accum has not
  //  already been initialized.
  void
  init_x_if_not(Teuchos::RCP<Thyra_VectorSpace const> const& vs);
  //! x += soln, where soln is nonoverlapping.
  void
  update_x(Thyra_Vector const& soln_nol);
  //! Return x + a for nonoverlapping a.
  Teuchos::RCP<Thyra_Vector const>
  add_x(Teuchos::RCP<Thyra_Vector const> const& a) const;

  /* Problems use these methods to set up RCU. */

  //! The problem registers the field.
  void
  registerField(
      std::string const&                          name,
      const Teuchos::RCP<PHX::DataLayout>&        dl,
      const Init::Enum                            init,
      const Transformation::Enum                  transformation,
      const Teuchos::RCP<Teuchos::ParameterList>& p);
  //! The problem creates the evaluators associated with RCU.
  template <typename EvalT>
  void
  createEvaluators(PHX::FieldManager<PHAL::AlbanyTraits>& fm, const Teuchos::RCP<Albany::Layouts>& dl);

  /* rc::Reader, rc::Writer, and AAdapt::MeshAdapt use these methods to read and
   * write data. */

  //! Append a decoration to the name indicating this is an RCU field.
  static inline std::string
  decorate(std::string const& name)
  {
    return name + "_RC";
  }
  //! Remove the decoration from the end of the name. (No error checking.)
  static inline std::string
  undecorate(std::string const& name_dec)
  {
    return name_dec.substr(0, name_dec.size() - 3);
  }

  struct Field
  {
    typedef std::vector<Teuchos::RCP<Field>>::iterator iterator;
    //! Field name, undecorated.
    std::string name;
    //! Field layout.
    Teuchos::RCP<PHX::DataLayout> layout;
    //! Number of g (Lie algebra) components used to represent this field.
    int num_g_fields;
    //! Get decorated name for i'th g component field.
    std::string
    get_g_name(int const i) const;
    // Opaque internal data for use by the implementation.
    struct Data;
    Teuchos::RCP<Data> data_;
  };

  Field::iterator
  fieldsBegin();
  Field::iterator
  fieldsEnd();

  typedef PHX::MDField<const RealType, Cell, Node, QuadPoint> BasisField;
  //! Reader<EvalT> uses these methods to load the data.
  void
  beginQpInterp();
  void
  interpQpField(PHX::MDField<RealType>& f, const PHAL::Workset& workset, const BasisField& bf);
  void
  endQpInterp();
  void
  readQpField(PHX::MDField<RealType>& f, const PHAL::Workset& workset);
  //! Writer<Residual> uses these methods to record the data.
  void
  beginQpWrite(const PHAL::Workset& workset, const BasisField& bf, const BasisField& wbf);
  void
  writeQpField(const PHX::MDField<const RealType>& f, const PHAL::Workset& workset, const BasisField& wbf);
  void
  endQpWrite();
  void
  testProjector(
      const PHAL::Workset&                                workset,
      const BasisField&                                   bf,
      const BasisField&                                   wbf,
      const PHX::MDField<RealType, Cell, Vertex, Dim>&    coord_vert,
      const PHX::MDField<RealType, Cell, QuadPoint, Dim>& coord_qp);
  //! MeshAdapt uses this method to read and write nodal data from the mesh
  // database before and after adaptation.
  Teuchos::RCP<Thyra_MultiVector> const&
  getNodalField(const Field& f, int const g_idx, bool const overlapped) const;
  //! MeshAdapt does this if usingProjection(). In the future, I may switch to
  //  keeping an RCP<AbstractDiscretization>, and then this call would be
  //  unnecessary.
  void
  initProjector(Teuchos::RCP<Thyra_VectorSpace const> const& node_vs, Teuchos::RCP<Thyra_VectorSpace const> const& ol_node_vs);

  /* Methods to inform Manager of what is happening. */

  //! Albany is building the state field manager.
  void
  beginBuildingSfm();
  //! Albany is done building the state field manager.
  void
  endBuildingSfm();
  //! Albany is evaluating the state field manager.
  void
  beginEvaluatingSfm();
  //! Albany is done evaluating the state field manager.
  void
  endEvaluatingSfm();
  //! The mesh is about to adapt.
  void
  beginAdapt();
  //! The mesh was just adapted. The maps are needed only if usingProjection().
  void
  endAdapt(Teuchos::RCP<Thyra_VectorSpace const> const& node_vs, Teuchos::RCP<Thyra_VectorSpace const> const& ol_node_vs);

  /* Methods to inform others of what is happening. */
  bool
  usingProjection() const;

 private:
  struct Impl;
  Teuchos::RCP<Impl> impl_;

  Manager(const Teuchos::RCP<Albany::StateManager>& state_mgr, bool const use_projection, bool const do_transform);
};

}  // namespace rc
}  // namespace AAdapt

#endif  // AADAPT_RC_MANAGER_HPP
