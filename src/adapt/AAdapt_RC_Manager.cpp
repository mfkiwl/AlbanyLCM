// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#include "AAdapt_RC_Manager.hpp"

#include <MiniTensor.h>

#include <Phalanx_FieldManager.hpp>
#include <Teuchos_CommHelpers.hpp>

#include "AAdapt_AdaptiveSolutionManager.hpp"
#include "AAdapt_RC_DataTypes.hpp"
#include "AAdapt_RC_DataTypes_impl.hpp"
#include "AAdapt_RC_Projector_impl.hpp"
#include "AAdapt_RC_Reader.hpp"
#include "AAdapt_RC_Writer.hpp"
#include "Albany_GlobalLocalIndexer.hpp"
#include "Albany_ThyraTypes.hpp"
#include "Albany_ThyraUtils.hpp"
#include "Phalanx_DataLayout_MDALayout.hpp"

#define loop(a, i, dim) for (PHX::MDField<RealType>::size_type i = 0; i < static_cast<PHX::MDField<RealType>::size_type>(a.dimension(dim)); ++i)

namespace AAdapt {
namespace rc {

// Data for internal use attached to a Field.
struct Manager::Field::Data
{
  Transformation::Enum transformation;
  // Nodal data g. g has up to two components.
  Teuchos::RCP<Thyra_MultiVector> mv[2];
};

std::string
Manager::Field::get_g_name(int const g_field_idx) const
{
  std::stringstream ss;
  ss << this->name << "_" << g_field_idx;
  return ss.str();
}

namespace {
// f.dimension(0) in general can be larger than mda.dimension(0) because of the
// way workset data vs bucket data are allocated.
void
read(const Albany::MDArray& mda, PHX::MDField<RealType>& f)
{
  switch (f.rank()) {
    case 2: loop(mda, cell, 0) loop(f, qp, 1) f(cell, qp) = mda(cell, qp); break;
    case 3: loop(mda, cell, 0) loop(f, qp, 1) loop(f, i0, 2) f(cell, qp, i0) = mda(cell, qp, i0); break;
    case 4: loop(mda, cell, 0) loop(f, qp, 1) loop(f, i0, 2) loop(f, i1, 3) f(cell, qp, i0, i1) = mda(cell, qp, i0, i1); break;
    default: ALBANY_ABORT("dims.size() \notin {2,3,4}.");
  }
}

template <typename MDArray>
void
write(Albany::MDArray& mda, const MDArray& f)
{
  switch (f.rank()) {
    case 2: loop(mda, cell, 0) loop(f, qp, 1) mda(cell, qp) = f(cell, qp); break;
    case 3: loop(mda, cell, 0) loop(f, qp, 1) loop(f, i0, 2) mda(cell, qp, i0) = f(cell, qp, i0); break;
    case 4: loop(mda, cell, 0) loop(f, qp, 1) loop(f, i0, 2) loop(f, i1, 3) mda(cell, qp, i0, i1) = f(cell, qp, i0, i1); break;
    default: ALBANY_ABORT("dims.size() \notin {2,3,4}.");
  }
}

minitensor::Tensor<RealType>&
symmetrize(minitensor::Tensor<RealType>& a)
{
  const minitensor::Index dim = a.get_dimension();
  if (dim > 1) {
    a(0, 1) = a(1, 0) = 0.5 * (a(0, 1) + a(1, 0));
    if (dim > 2) {
      a(0, 2) = a(2, 0) = 0.5 * (a(0, 2) + a(2, 0));
      a(1, 2) = a(2, 1) = 0.5 * (a(1, 2) + a(2, 1));
    }
  }
  return a;
}

struct Direction
{
  enum Enum
  {
    g2G,
    G2g
  };
};

void
calc_right_polar_LieR_LieS_G2g(const minitensor::Tensor<RealType>& F, minitensor::Tensor<RealType> RS[2])
{
  {
    std::pair<minitensor::Tensor<RealType>, minitensor::Tensor<RealType>> RSpair = minitensor::polar_right(F);
    RS[0]                                                                        = RSpair.first;
    RS[1]                                                                        = RSpair.second;
  }
  RS[0] = minitensor::log_rotation(RS[0]);
  RS[1] = minitensor::log_sym(RS[1]);
  symmetrize(RS[1]);
}

void
calc_right_polar_LieR_LieS_g2G(minitensor::Tensor<RealType>& R, minitensor::Tensor<RealType>& S)
{
  R = minitensor::exp_skew_symmetric(R);
  S = minitensor::exp(S);
  symmetrize(S);
  R = minitensor::dot(R, S);
}

void
transformStateArray(const Direction::Enum dir, const Transformation::Enum transformation, Albany::MDArray& mda1, Albany::MDArray& mda2)
{
  switch (transformation) {
    case Transformation::none: {
      if (dir == Direction::G2g) {
        // Copy from the provisional to the primary field.
        write(mda1, mda2);
      } else {
        // In the g -> G direction, the values are already in the primary field,
        // so there's nothing to do.
      }
    } break;
    case Transformation::right_polar_LieR_LieS: {
      loop(mda1, cell, 0) loop(mda1, qp, 1)
      {
        if (dir == Direction::G2g) {
          // Copy mda2 (provisional) -> local.
          minitensor::Tensor<RealType> F(mda1.dimension(2));
          loop(mda2, i, 2) loop(mda2, j, 3) F(i, j) = mda2(cell, qp, i, j);
          minitensor::Tensor<RealType> RS[2];
          calc_right_polar_LieR_LieS_G2g(F, RS);
          // Copy local -> mda1, mda2.
          loop(mda1, i, 2) loop(mda1, j, 3)
          {
            mda1(cell, qp, i, j) = RS[0](i, j);
            mda2(cell, qp, i, j) = RS[1](i, j);
          }
        } else {
          // Copy mda1,2 -> local.
          minitensor::Tensor<RealType> R(mda1.dimension(2)), S(mda2.dimension(2));
          loop(mda1, i, 2) loop(mda1, j, 3)
          {
            R(i, j) = mda1(cell, qp, i, j);
            S(i, j) = mda2(cell, qp, i, j);
          }
          calc_right_polar_LieR_LieS_g2G(R, S);
          // Copy local -> mda1. mda2 is unused after g -> G.
          loop(mda1, i, 2) loop(mda1, j, 3) mda1(cell, qp, i, j) = R(i, j);
        }
      }
    } break;
  }
}

class Projector
{
  typedef PHX::MDField<const RealType, Cell, Node, QuadPoint> BasisField;
  typedef BasisField::size_type                               size_type;

  Teuchos::RCP<Thyra_VectorSpace const>          node_vs_, ol_node_vs_;
  Teuchos::RCP<Albany::ThyraCrsMatrixFactory>    M_factory_;
  Teuchos::RCP<Thyra_LinearOp>                   M_;
  Teuchos::RCP<Albany::CombineAndScatterManager> cas_manager_;
  Teuchos::RCP<Thyra_LinearOp>                   P_;
  // M_ persists over multiple state field manager evaluations if the mesh is
  // not adapted after every LOCA step. Indicate whether this part of M_ has
  // already been filled.
  std::vector<bool> filled_;

 public:
  Projector() {}
  void
  init(Teuchos::RCP<Thyra_VectorSpace const> const& node_vs, Teuchos::RCP<Thyra_VectorSpace const> const& ol_node_vs);
  void
  fillMassMatrix(const PHAL::Workset& workset, const BasisField& bf, const BasisField& wbf);
  void
  fillRhs(const PHX::MDField<const RealType>& f_G_qp, Manager::Field& f, const PHAL::Workset& workset, const BasisField& wbf);
  void
  project(Manager::Field& f);
  void
  interp(const Manager::Field& f, const PHAL::Workset& workset, const BasisField& bf, Albany::MDArray& mda1, Albany::MDArray& mda2);
  // For testing.
  Teuchos::RCP<Thyra_VectorSpace const> const&
  get_node_vs() const
  {
    return node_vs_;
  }
  Teuchos::RCP<Thyra_VectorSpace const> const&
  get_ol_node_vs() const
  {
    return ol_node_vs_;
  }

 private:
  bool
  is_filled(int wi);
};

void
Projector::init(Teuchos::RCP<Thyra_VectorSpace const> const& node_vs, Teuchos::RCP<Thyra_VectorSpace const> const& ol_node_vs)
{
  node_vs_                  = node_vs;
  ol_node_vs_               = ol_node_vs;
  int const max_num_entries = 27;  // Enough for first-order hex.
  M_factory_                = Teuchos::rcp(new Albany::ThyraCrsMatrixFactory(ol_node_vs_, ol_node_vs_, max_num_entries));
  M_                        = Teuchos::null;
  cas_manager_              = Albany::createCombineAndScatterManager(node_vs_, ol_node_vs_);
  P_                        = Teuchos::null;
  filled_.clear();
}

void
Projector::fillMassMatrix(const PHAL::Workset& workset, const BasisField& bf, const BasisField& wbf)
{
  if (is_filled(workset.wsIndex)) return;
  filled_[workset.wsIndex] = true;

  const size_type num_node = bf.dimension(1), num_qp = bf.dimension(2);
  for (unsigned int cell = 0; cell < workset.numCells; ++cell) {
    for (size_type rnode = 0; rnode < num_node; ++rnode) {
      const GO           row = workset.wsElNodeID[cell][rnode];
      Teuchos::Array<GO> cols;
      for (size_type cnode = 0; cnode < num_node; ++cnode) {
        cols.push_back(workset.wsElNodeID[cell][cnode]);
      }
      M_factory_->insertGlobalIndices(row, cols);
    }
  }
  M_factory_->fillComplete();
  auto indexer = Albany::createGlobalLocalIndexer(M_->range());
  for (unsigned int cell = 0; cell < workset.numCells; ++cell) {
    for (size_type rnode = 0; rnode < num_node; ++rnode) {
      Teuchos::Array<ST> vals;
      for (size_type cnode = 0; cnode < num_node; ++cnode) {
        ST v = 0;
        for (size_type qp = 0; qp < num_qp; ++qp) v += wbf(cell, rnode, qp) * bf(cell, cnode, qp);
        vals.push_back(v);
      }
      const GO grow = workset.wsElNodeID[cell][rnode];
      const LO lrow = indexer->getLocalElement(grow);
      Albany::setLocalRowValues(M_, lrow, vals);
    }
  }
}

void
Projector::fillRhs(const PHX::MDField<const RealType>& f_G_qp, Manager::Field& f, const PHAL::Workset& workset, const BasisField& wbf)
{
  int const rank = f.layout->rank() - 2, num_node = wbf.dimension(1), num_qp = wbf.dimension(2), ndim = rank >= 1 ? f_G_qp.dimension(2) : 1;

  if (f.data_->mv[0].is_null()) {
    int const ncol = rank == 0 ? 1 : rank == 1 ? ndim : ndim * ndim;
    for (int fi = 0; fi < f.num_g_fields; ++fi) {
      f.data_->mv[fi] = Thyra::createMembers(ol_node_vs_, ncol);
    }
  }

  auto                       indexer        = Albany::createGlobalLocalIndexer(f.data_->mv[0]->range());
  const Transformation::Enum transformation = f.data_->transformation;
  for (int cell = 0; cell < (int)workset.numCells; ++cell) {
    for (int node = 0; node < num_node; ++node) {
      const GO grow = workset.wsElNodeID[cell][node];
      const LO lrow = indexer->getLocalElement(grow);
      for (int qp = 0; qp < num_qp; ++qp) {
        switch (rank) {
          case 0:
          case 1: ALBANY_ABORT("!impl"); break;
          case 2: {
            switch (transformation) {
              case Transformation::none: {
                auto data = Albany::getNonconstLocalData(f.data_->mv[0]);
                for (int i = 0, col = 0; i < ndim; ++i)
                  for (int j = 0; j < ndim; ++j, ++col) data[col][lrow] += f_G_qp(cell, qp, i, j) * wbf(cell, node, qp);
              } break;
              case Transformation::right_polar_LieR_LieS: {
                minitensor::Tensor<RealType> F(ndim);
                loop(f_G_qp, i, 2) loop(f_G_qp, j, 3) F(i, j) = f_G_qp(cell, qp, i, j);
                minitensor::Tensor<RealType> RS[2];
                calc_right_polar_LieR_LieS_G2g(F, RS);
                for (int fi = 0; fi < f.num_g_fields; ++fi) {
                  auto data = Albany::getNonconstLocalData(f.data_->mv[fi]);
                  for (int i = 0, col = 0; i < ndim; ++i)
                    for (int j = 0; j < ndim; ++j, ++col) data[col][lrow] += RS[fi](i, j) * wbf(cell, node, qp);
                }
                break;
              }
            }
            break;
          }
          default:
            std::stringstream ss;
            ss << "invalid rank: " << f.name << " with rank " << rank;
            ALBANY_ABORT(ss.str());
        }
      }
    }
  }
}

void
Projector::project(Manager::Field& f)
{
  if (Albany::isFillActive(M_)) {
    // Export M_ so it has nonoverlapping rows and cols.
    Albany::fillComplete(M_);
    Albany::ThyraCrsMatrixFactory M_owned_factory(node_vs_, node_vs_, M_factory_);
    auto                          M = M_owned_factory.createOp();
    cas_manager_->combine(M_, M, Albany::CombineMode::ADD);
    M_ = M;
    Albany::fillComplete(M_);
  }
  Teuchos::RCP<Thyra_MultiVector> x[2];
  for (int fi = 0; fi < f.num_g_fields; ++fi) {
    int const nrhs = f.data_->mv[fi]->domain()->dim();
    // Export the rhs to the same row map.
    auto b = Thyra::createMembers(M_->range(), nrhs);
    cas_manager_->combine(f.data_->mv[fi], b, Albany::CombineMode::ADD);
    // Create x[fi] in M_ x[fi] = b[fi]. As a side effect, initialize P_ if
    // necessary.
    Teuchos::ParameterList pl;
    pl.set("Block Size", 1);  // Could be nrhs.
    pl.set("Maximum Iterations", 1000);
    pl.set("Convergence Tolerance", 1e-12);
    pl.set("Output Frequency", 10);
    pl.set("Output Style", 1);
    pl.set("Verbosity", 0);        // 33);
    x[fi] = solve(M_, P_, b, pl);  // in AAdapt_RC_Projector_impl
    // Import (reverse mode) to the overlapping MV.
    f.data_->mv[fi]->assign(0);
    cas_manager_->scatter(x[fi], f.data_->mv[fi], Albany::CombineMode::ADD);
  }
}

void
Projector::interp(const Manager::Field& f, const PHAL::Workset& workset, const BasisField& bf, Albany::MDArray& mda1, Albany::MDArray& mda2)
{
  int const rank = f.layout->rank() - 2, num_node = bf.dimension(1), num_qp = bf.dimension(2), ndim = rank >= 1 ? mda1.dimension(2) : 1;

  Albany::MDArray* mdas[2];
  mdas[0]       = &mda1;
  mdas[1]       = &mda2;
  int const nmv = f.num_g_fields;

  auto indexer = Albany::createGlobalLocalIndexer(ol_node_vs_);
  for (int cell = 0; cell < (int)workset.numCells; ++cell)
    for (int qp = 0; qp < num_qp; ++qp) {
      switch (rank) {
        case 0:
        case 1: ALBANY_ABORT("!impl"); break;
        case 2: {
          for (int i = 0; i < ndim; ++i)
            for (int j = 0; j < ndim; ++j) mda1(cell, qp, i, j) = 0;
          for (int node = 0; node < num_node; ++node) {
            const GO grow = workset.wsElNodeID[cell][node];
            const LO row  = indexer->getLocalElement(grow);
            for (int i = 0, col = 0; i < ndim; ++i) {
              for (int fi = 0; fi < nmv; ++fi) {
                auto data = Albany::getLocalData(f.data_->mv[fi].getConst());
                for (int j = 0; j < ndim; ++j, ++col) {
                  (*mdas[fi])(cell, qp, i, j) += data[col][row] * bf(cell, node, qp);
                }
              }
            }
          }
        } break;
        default:
          std::stringstream ss;
          ss << "invalid rank: " << f.name << " with rank " << rank;
          ALBANY_ABORT(ss.str());
      }
    }
}

bool
Projector::is_filled(int wi)
{
  if (static_cast<int>(filled_.size()) <= wi) {
    filled_.insert(filled_.end(), wi - filled_.size() + 1, false);
  }
  return filled_[wi];
}

namespace testing {
class ProjectorTester
{
  struct Impl;
  Teuchos::RCP<Impl> d;

 public:
  ProjectorTester();
  void
  init(Teuchos::RCP<Thyra_VectorSpace const> const& node_vs, Teuchos::RCP<Thyra_VectorSpace const> const& ol_node_vs);
  void
  eval(
      const PHAL::Workset&                                workset,
      const Manager::BasisField&                          bf,
      const Manager::BasisField&                          wbf,
      const PHX::MDField<RealType, Cell, QuadPoint, Dim>& coord_qp);
  void
  fillRhs(const PHAL::Workset& workset, const Manager::BasisField& wbf, const PHX::MDField<RealType, Cell, QuadPoint, Dim>& coord_qp);
  void
  project();
  void
  interp(const PHAL::Workset& workset, const Manager::BasisField& bf, const PHX::MDField<RealType, Cell, QuadPoint, Dim>& coord_qp);
  void
  finish();
};

void
testProjector(
    const Projector&                                    pc,
    const PHAL::Workset&                                workset,
    const Manager::BasisField&                          bf,
    const Manager::BasisField&                          wbf,
    const PHX::MDField<RealType, Cell, Vertex, Dim>&    coord_vert,
    const PHX::MDField<RealType, Cell, QuadPoint, Dim>& coord_qp);
}  // namespace testing
}  // namespace

struct Manager::Impl
{
  Teuchos::RCP<AdaptiveSolutionManager> sol_mgr_;
  Teuchos::RCP<Albany::StateManager>    state_mgr_;
  Teuchos::RCP<Thyra_Vector>            x_;
  Teuchos::RCP<Projector>               proj_;

 private:
  typedef unsigned int                                WsIdx;
  typedef std::pair<std::string, Teuchos::RCP<Field>> Pair;
  typedef std::map<std::string, Teuchos::RCP<Field>>  Map;

  Map                              field_map_;
  std::vector<Teuchos::RCP<Field>> fields_;
  bool                             building_sfm_, transform_;
  std::vector<short>               is_g_;

 public:
  Impl(const Teuchos::RCP<Albany::StateManager>& state_mgr, bool const use_projection, bool const do_transform) : state_mgr_(state_mgr)
  {
    init(use_projection, do_transform);
  }

  void
  registerField(
      std::string const&                          name,
      const Teuchos::RCP<PHX::DataLayout>&        dl,
      const Init::Enum                            init_G,
      Transformation::Enum                        transformation,
      const Teuchos::RCP<Teuchos::ParameterList>& p)
  {
    if (!transform_) transformation = Transformation::none;

    std::string const name_rc = decorate(name);
    p->set<std::string>(name_rc + " Name", name_rc);
    p->set<Teuchos::RCP<PHX::DataLayout>>(name_rc + " Data Layout", dl);

    Map::iterator it = field_map_.find(name_rc);
    if (it != field_map_.end()) return;

    Teuchos::RCP<Field> f = Teuchos::rcp(new Field());
    fields_.push_back(f);
    f->name                  = name;
    f->layout                = dl;
    f->num_g_fields          = transformation == Transformation::none ? 1 : 2;
    f->data_                 = Teuchos::rcp(new Field::Data());
    f->data_->transformation = transformation;

    field_map_.insert(Pair(name_rc, f));

    // Depending on the state variable, different quantities need to be read
    // and written. In all cases, we need two fields.
    //   Holds G and g1.
    registerStateVariable(name_rc, f->layout, init_G);
    //   Holds provisional G and, if needed, g2. If g2 is not needed, then
    // this provisional field is a waste of space and also incurs wasted work
    // in the QP transfer. However, I would need LOCA::AdaptiveSolver to
    // always correctly say, before printSolution is called, whether the mesh
    // will be adapted to avoid this extra storage and work. Maybe in the
    // future.
    registerStateVariable(name_rc + "_1", f->layout, Init::zero);
  }

  void
  beginAdapt()
  {
    // Transform G -> g and write to the primary or, depending on state, primary
    // and provisional fields.
    if (proj_.is_null())
      for (Map::const_iterator it = field_map_.begin(); it != field_map_.end(); ++it)
        for (WsIdx wi = 0; wi < is_g_.size(); ++wi) transformStateArray(it->first, wi, Direction::G2g);
    else {
      for (Map::iterator it = field_map_.begin(); it != field_map_.end(); ++it) proj_->project(*it->second);
    }
  }

  void
  endAdapt(Teuchos::RCP<Thyra_VectorSpace const> const& node_vs, Teuchos::RCP<Thyra_VectorSpace const> const& ol_node_vs)
  {
    init_g(state_mgr_->getStateArrays().elemStateArrays.size(), true);
    if (Teuchos::nonnull(proj_)) {
      proj_->init(node_vs, ol_node_vs);
      for (Map::iterator it = field_map_.begin(); it != field_map_.end(); ++it) {
        Field& f = *it->second;
        for (int i = 0; i < f.num_g_fields; ++i) f.data_->mv[i] = Thyra::createMembers(ol_node_vs, f.data_->mv[i]->domain()->dim());
      }
    }
  }

  void
  initProjector(Teuchos::RCP<Thyra_VectorSpace const> const& node_vs, Teuchos::RCP<Thyra_VectorSpace const> const& ol_node_vs)
  {
    if (Teuchos::nonnull(proj_)) {
      proj_->init(node_vs, ol_node_vs);
    }
  }

  void
  interpQpField(PHX::MDField<RealType>& f_G_qp, const PHAL::Workset& workset, const BasisField& bf)
  {
    if (proj_.is_null()) return;
    if (is_g_.empty()) {
      // Special case at startup.
      init_g(state_mgr_->getStateArrays().elemStateArrays.size(), false);
      return;
    }
    if (!is_g(workset.wsIndex)) return;
    // Interpolate g at NP to g at QP.
    std::string const          name_rc = f_G_qp.fieldTag().name();
    const Teuchos::RCP<Field>& f       = field_map_[name_rc];
    proj_->interp(*f, workset, bf, getMDArray(name_rc, workset.wsIndex), getMDArray(name_rc + "_1", workset.wsIndex));
    // Transform g -> G at QP.
    transformStateArray(name_rc, workset.wsIndex, Direction::g2G);
    set_G(workset.wsIndex);
    // If this is the last workset, we're done interpolating, so release the
    // memory.
    if (workset.wsIndex == is_g_.size() - 1)
      for (int i = 0; i < f->num_g_fields; ++i) f->data_->mv[i] = Teuchos::null;
  }

  void
  readQpField(PHX::MDField<RealType>& f, const PHAL::Workset& workset)
  {
    // At startup, is_g_.size() is 0. We also initialized fields to their G, not
    // g, values.
    if (is_g_.empty()) init_g(state_mgr_->getStateArrays().elemStateArrays.size(), false);
    if (proj_.is_null()) {
      if (is_g(workset.wsIndex)) {
        // If this is the first read after an RCU, transform g -> G.
        transformStateArray(f.fieldTag().name(), workset.wsIndex, Direction::g2G);
        set_G(workset.wsIndex);
      }
    } else {
      // The most obvious reason this exception could be thrown is because
      // EvalT=Jacobian is run before Residual, which I think should not happen.
      ALBANY_PANIC(is_g(workset.wsIndex), "If usingProjection(), then readQpField should always see G, not g.");
    }
    // Read from the primary field.
    read(getMDArray(f.fieldTag().name(), workset.wsIndex), f);
  }

  void
  writeQpField(const PHX::MDField<const RealType>& f, const PHAL::Workset& workset, const BasisField& wbf)
  {
    std::string const name_rc = decorate(f.fieldTag().name());
    if (proj_.is_null()) {
      // Write to the provisional field.
      write(getMDArray(name_rc + "_1", workset.wsIndex), f);
    } else
      proj_->fillRhs(f, *field_map_[name_rc], workset, wbf);
  }

  Manager::Field::iterator
  fieldsBegin()
  {
    return fields_.begin();
  }
  Manager::Field::iterator
  fieldsEnd()
  {
    return fields_.end();
  }

  void
  set_building_sfm(bool const value)
  {
    building_sfm_ = value;
  }
  bool
  building_sfm() const
  {
    return building_sfm_;
  }

  void
  set_evaluating_sfm(bool const before)
  {
    if (before && Teuchos::nonnull(proj_)) {
      // Zero the nodal values in prep for fillRhs.
      for (Field::iterator it = fields_.begin(); it != fields_.end(); ++it)
        for (int i = 0; i < (*it)->num_g_fields; ++i)
          if (Teuchos::nonnull((*it)->data_->mv[i])) (*it)->data_->mv[i]->assign(0);
    }
  }

  Transformation::Enum
  get_transformation(std::string const& name_rc)
  {
    return field_map_[name_rc]->data_->transformation;
  }

  int
  numWorksets() const
  {
    return is_g_.size();
  }

 private:
  void
  init(bool const use_projection, bool const do_transform)
  {
    transform_    = do_transform;
    building_sfm_ = false;
    if (use_projection) {
      proj_ = Teuchos::rcp(new Projector());
    }
  }

  void
  registerStateVariable(std::string const& name, const Teuchos::RCP<PHX::DataLayout>& dl, const Init::Enum init)
  {
    state_mgr_->registerStateVariable(name, dl, "", init == Init::zero ? "scalar" : "identity", 0, false, false);
  }

  Albany::MDArray&
  getMDArray(std::string const& name, const WsIdx wi)
  {
    Albany::StateArray&          esa = state_mgr_->getStateArrays().elemStateArrays[wi];
    Albany::StateArray::iterator it  = esa.find(name);
    ALBANY_PANIC(it == esa.end(), "elemStateArrays is missing " + name);
    return it->second;
  }

  void
  init_g(int const n, bool const is_g)
  {
    is_g_.clear();
    is_g_.resize(n, is_g ? 0 : fields_.size());
  }
  bool
  is_g(int const ws_idx) const
  {
    return is_g_[ws_idx] < static_cast<int>(fields_.size());
  }
  void
  set_G(int const ws_idx)
  {
    ++is_g_[ws_idx];
  }

  void
  transformStateArray(std::string const& name_rc, const WsIdx wi, const Direction::Enum dir)
  {
    // Name decoration coordinates with registerField's calls to
    // registerStateVariable.
    const Transformation::Enum transformation = get_transformation(name_rc);
    rc::transformStateArray(dir, transformation, getMDArray(name_rc, wi), getMDArray(name_rc + "_1", wi));
  }
};

Teuchos::RCP<Manager>
Manager::create(const Teuchos::RCP<Albany::StateManager>& state_mgr, Teuchos::ParameterList& problem_params)
{
  if (!problem_params.isSublist("Adaptation")) return Teuchos::null;

  Teuchos::ParameterList& adapt_params = problem_params.sublist("Adaptation", true);

  if (adapt_params.isType<bool>("Reference Configuration: Update")) {
    if (adapt_params.get<bool>("Reference Configuration: Update")) {
      bool const use_projection = adapt_params.get<bool>("Reference Configuration: Project", false);
      bool const do_transform   = adapt_params.get<bool>("Reference Configuration: Transform", false);
      return Teuchos::rcp(new Manager(state_mgr, use_projection, do_transform));
    }
  }

  return Teuchos::null;
}

void
Manager::setSolutionManager(const Teuchos::RCP<AdaptiveSolutionManager>& sol_mgr)
{
  impl_->sol_mgr_ = sol_mgr;
}

void
Manager::getValidParameters(Teuchos::RCP<Teuchos::ParameterList>& valid_pl)
{
  valid_pl->set<bool>("Reference Configuration: Update", false, "Send coordinates + solution to SCOREC.");
}

void
Manager::init_x_if_not(Teuchos::RCP<Thyra_VectorSpace const> const& vs)
{
  if (Teuchos::nonnull(impl_->x_)) {
    return;
  }
  impl_->x_ = Thyra::createMember(vs);
  impl_->x_->assign(0);
}

static void
update_x(const Teuchos::ArrayRCP<double>& x, const Teuchos::ArrayRCP<double const>& s, const Teuchos::RCP<Albany::AbstractDiscretization>& disc)
{
  int const spdim = disc->getNumDim(), neq = disc->getNumEq();
  for (int i = 0; i < x.size(); i += neq)
    for (int j = 0; j < spdim; ++j) x[i + j] += s[i + j];
}

void
Manager::update_x(Thyra_Vector const& soln_nol)
{
  // By convention (e.g., in MechanicsProblem), the displacement DOFs are before
  // any other DOFs.
  auto x_data   = Albany::getNonconstLocalData(impl_->x_);
  auto sol_data = Albany::getLocalData(soln_nol);
  AAdapt::rc::update_x(x_data, sol_data, impl_->state_mgr_->getDiscretization());
}

Teuchos::RCP<Thyra_Vector const>
Manager::add_x(Teuchos::RCP<Thyra_Vector const> const& a) const
{
  Teuchos::RCP<Thyra_Vector> c = Thyra::createMember(a->space());
  c->assign(*a);

  auto c_data = Albany::getNonconstLocalData(c);
  auto s_data = Albany::getLocalData(impl_->x_.getConst());

  AAdapt::rc::update_x(c_data, s_data, impl_->state_mgr_->getDiscretization());
  return c;
}

Teuchos::RCP<Thyra_Vector>&
Manager::get_x()
{
  return impl_->x_;
}

template <typename EvalT>
void
Manager::createEvaluators(PHX::FieldManager<PHAL::AlbanyTraits>& fm, const Teuchos::RCP<Albany::Layouts>&)
{
  fm.registerEvaluator<EvalT>(Teuchos::rcp(new Reader<EvalT, PHAL::AlbanyTraits>(Teuchos::rcp(this, false))));
}

template <>
void
Manager::createEvaluators<PHAL::AlbanyTraits::Residual>(PHX::FieldManager<PHAL::AlbanyTraits>& fm, const Teuchos::RCP<Albany::Layouts>& dl)
{
  typedef PHAL::AlbanyTraits::Residual Residual;
  fm.registerEvaluator<Residual>(Teuchos::rcp(new Reader<Residual, PHAL::AlbanyTraits>(Teuchos::rcp(this, false), dl)));
  if (impl_->building_sfm()) {
    Teuchos::RCP<Writer<Residual, PHAL::AlbanyTraits>> writer = Teuchos::rcp(new Writer<Residual, PHAL::AlbanyTraits>(Teuchos::rcp(this, false), dl));
    fm.registerEvaluator<Residual>(writer);
    fm.requireField<Residual>(*writer->getNoOutputTag());
  }
}

void
Manager::registerField(
    std::string const&                          name,
    const Teuchos::RCP<PHX::DataLayout>&        dl,
    const Init::Enum                            init,
    const Transformation::Enum                  transformation,
    const Teuchos::RCP<Teuchos::ParameterList>& p)
{
  impl_->registerField(name, dl, init, transformation, p);
}

// The asymmetry in naming scheme (interp + read vs just write) emerges from the
// asymmetry in reading and writing. Every EvalT needs access to the RealType
// states for use in computations. In addition, Reader<Residual> needs to do
// interp if projection is used. In contrast, only Writer<Residual> is used, so
// it does both the (optional) projection and write.
void
Manager::beginQpInterp()
{ /* Do nothing. */
}
void
Manager::interpQpField(PHX::MDField<RealType>& f, const PHAL::Workset& workset, const BasisField& bf)
{
  impl_->interpQpField(f, workset, bf);
}
void
Manager::endQpInterp()
{ /* Do nothing. */
}

void
Manager::readQpField(PHX::MDField<RealType>& f, const PHAL::Workset& workset)
{
  impl_->readQpField(f, workset);
}

void
Manager::beginQpWrite(const PHAL::Workset& workset, const BasisField& bf, const BasisField& wbf)
{
  if (impl_->proj_.is_null()) return;
  impl_->proj_->fillMassMatrix(workset, bf, wbf);
}
void
Manager::writeQpField(const PHX::MDField<const RealType>& f, const PHAL::Workset& workset, const BasisField& wbf)
{
  impl_->writeQpField(f, workset, wbf);
}
void
Manager::endQpWrite()
{ /* Do nothing. */
}

void
Manager::testProjector(
    const PHAL::Workset&                                workset,
    const BasisField&                                   bf,
    const BasisField&                                   wbf,
    const PHX::MDField<RealType, Cell, Vertex, Dim>&    coord_vert,
    const PHX::MDField<RealType, Cell, QuadPoint, Dim>& coord_qp)
{
  (void)workset;
  (void)bf;
  (void)wbf;
  (void)coord_vert;
  (void)coord_qp;
}

Teuchos::RCP<Thyra_MultiVector> const&
Manager::getNodalField(const Field& f, int const g_idx, bool const overlapped) const
{
  ALBANY_PANIC(!overlapped, "must be overlapped");
  return f.data_->mv[g_idx];
}

Manager::Field::iterator
Manager::fieldsBegin()
{
  return impl_->fieldsBegin();
}
Manager::Field::iterator
Manager::fieldsEnd()
{
  return impl_->fieldsEnd();
}

void
Manager::beginBuildingSfm()
{
  impl_->set_building_sfm(true);
}
void
Manager::endBuildingSfm()
{
  impl_->set_building_sfm(false);
}

void
Manager::beginEvaluatingSfm()
{
  impl_->set_evaluating_sfm(true);
}
void
Manager::endEvaluatingSfm()
{
  impl_->set_evaluating_sfm(false);
}

void
Manager::beginAdapt()
{
  impl_->beginAdapt();
}
void
Manager::endAdapt(Teuchos::RCP<Thyra_VectorSpace const> const& node_vs, Teuchos::RCP<Thyra_VectorSpace const> const& ol_node_vs)
{
  impl_->endAdapt(node_vs, ol_node_vs);
}

void
Manager::initProjector(Teuchos::RCP<Thyra_VectorSpace const> const& node_vs, Teuchos::RCP<Thyra_VectorSpace const> const& ol_node_vs)
{
  impl_->initProjector(node_vs, ol_node_vs);
}

bool
Manager::usingProjection() const
{
  return Teuchos::nonnull(impl_->proj_);
}

Manager::Manager(const Teuchos::RCP<Albany::StateManager>& state_mgr, bool const use_projection, bool const do_transform)
    : impl_(Teuchos::rcp(new Impl(state_mgr, use_projection, do_transform)))
{
}

#define eti_fn(EvalT) template void Manager::createEvaluators<EvalT>(PHX::FieldManager<PHAL::AlbanyTraits> & fm, const Teuchos::RCP<Albany::Layouts>& dl);
aadapt_rc_apply_to_all_eval_types(eti_fn)
#undef eti_fn

    namespace
{
  namespace testing {
  typedef minitensor::Tensor<RealType> Tensor;

  // Some deformation gradient tensors with det(F) > 0 for use in testing.
  static double const Fs[3][3][3] = {
      {{-7.382752820294219e-01, -1.759182226321058e+00, 1.417301043170359e+00},
       {7.999093048231801e-01, 5.295155264305610e-01, -3.075207765325406e-02},
       {6.283454283198379e-02, 4.117063384659416e-01, -1.243061703605918e-01}},
      {{4.929646496030746e-01, -1.672547330507927e+00, 1.374629761307942e-01},
       {9.785301515971359e-01, 8.608882413324722e-01, 6.315167262108045e-01},
       {-5.339914726510328e-01, -1.559378791976819e+00, 1.242404824706601e-01}},
      {{1.968477583454205e+00, 1.805729439108956e+00, -2.759426722073080e-01},
       {7.787416415696722e-01, -5.361220317998502e-03, 1.838993634875665e-01},
       {-1.072168271881842e-02, 3.771872253769205e-01, -9.553540517889956e-01}}};

  // Some sample functions. Only the constant and linear ones should be
  // interpolated exactly.
  double
  eval_f(double const x, double const y, double const z, int ivec)
  {
    static double const R = 0.15;
    switch (ivec + 1) {
      case 1: return 2;
      case 2: return 1.5 * x + 2 * y + 3 * z;
      case 3: return x * x + y * y + z;
      case 4: return x * x * x - x * x * y + x * y * y - y * y * y;
      case 5: return cos(2 * M_PI * x / R) + sin(2 * M_PI * y / R) + z;
      case 6: return x * x * x * x;
      case 7: return x * x - y * y + x * y + z;
      case 8: return x * x;
      case 9: return x * x * x;
    }
    ALBANY_ABORT("Error: unhandled argument in evalf() in AAdapt_RC_Manager.cpp" << std::endl);
  }

  // Axis-aligned bounding box on the vertices.
  void
  getBoundingBox(const PHX::MDField<RealType, Cell, Vertex, Dim>& vs, RealType lo[3], RealType hi[3])
  {
    bool first = true;
    for (unsigned cell = 0; cell < vs.dimension(0); ++cell)
      for (unsigned iv = 0; iv < vs.dimension(1); ++iv) {
        for (unsigned id = 0; id < vs.dimension(2); ++id) {
          const RealType v = vs(cell, iv, id);
          if (first)
            lo[id] = hi[id] = v;
          else {
            lo[id] = std::min(lo[id], v);
            hi[id] = std::max(hi[id], v);
          }
        }
        first = false;
      }
  }

  // F field.
  minitensor::Tensor<RealType>
  eval_F(const RealType p[3])
  {
#define in01(u) (0 <= (u) && (u) <= 1)
    TEUCHOS_ASSERT(in01(p[0]) && in01(p[1]) && in01(p[2]));
#undef in01
#define lpij                  \
  for (int i = 0; i < 3; ++i) \
    for (int j = 0; j < 3; ++j)
    Tensor r(3), s(3);
    lpij   r(i, j) = s(i, j) = 0;
    for (int k = 0; k < 3; ++k) {
      Tensor                    F(3);
      lpij                      F(i, j) = Fs[k][i][j];
      std::pair<Tensor, Tensor> RS      = minitensor::polar_right(F);
      RS.first                          = minitensor::log_rotation(RS.first);
      RS.second                         = minitensor::log_sym(RS.second);
      symmetrize(RS.second);
      // Right now, we are not doing anything to handle phase wrapping in r =
      // logm(R). That means that projection with Lie transformation does not
      // work in general right now. But test the correctness of the projector in
      // the case that no phase wrap occurs. Here I do that by using only one
      // F's r.
      if (k == 0) r += p[k] * RS.first;
      s += p[k] * RS.second;
    }
    const Tensor R = minitensor::exp_skew_symmetric(r);
    Tensor       S = minitensor::exp(s);
    symmetrize(S);
    return minitensor::dot(R, S);
#undef lpij
  }

  // The following methods test whether q == interp(M \ b(q)). q is a linear
  // function of space and that lives on the integration points. M is the mass
  // matrix. b(q) is the integral over each element. q* = M \ b(q) is the L_2
  // projection onto the nodal points. interp(q*) is the interpolation back to
  // the integration points.
  //   This first method runs from start to finish but works for only one
  //   workset
  // and in serial. I'll probably remove this one at some point.
  //   ProjectorTester works in all cases.
  void
  testProjector(
      const Projector&                                    pc,
      const PHAL::Workset&                                workset,
      const Manager::BasisField&                          bf,
      const Manager::BasisField&                          wbf,
      const PHX::MDField<RealType, Cell, Vertex, Dim>&    coord_vert,
      const PHX::MDField<RealType, Cell, QuadPoint, Dim>& coord_qp)
  {
    // Works only in the case of one workset.
    TEUCHOS_ASSERT(workset.wsIndex == 0);

    // Set up the data containers.
    typedef PHX::MDALayout<Cell, QuadPoint, Dim, Dim> Layout;
    Teuchos::RCP<Layout>   layout     = Teuchos::rcp(new Layout(workset.numCells, coord_qp.dimension(1), coord_qp.dimension(2), coord_qp.dimension(2)));
    PHX::MDField<RealType> f_mdf      = PHX::MDField<RealType>("f_mdf", layout);
    auto                   f_mdf_data = PHX::KokkosViewFactory<RealType, typename PHX::DevLayout<RealType>::type, PHX::Device>::buildView(f_mdf.fieldTag());
    f_mdf.setFieldData(f_mdf_data);

    std::vector<Albany::MDArray> mda;
    std::vector<double>          mda_data[2];
    for (int i = 0; i < 2; ++i) {
      typedef Albany::MDArray::size_type size_t;
      mda_data[i].resize(f_mdf.dimension(0) * f_mdf.dimension(1) * f_mdf.dimension(2) * f_mdf.dimension(3));
      shards::Array<RealType, shards::NaturalOrder, Cell, QuadPoint, Dim, Dim> a;
      a.assign(&mda_data[i][0], (size_t)f_mdf.dimension(0), (size_t)f_mdf.dimension(1), (size_t)f_mdf.dimension(2), (size_t)f_mdf.dimension(3));
      mda.push_back(a);
    }

    Projector p;
    p.init(pc.get_node_vs(), pc.get_ol_node_vs());

    // M.
    p.fillMassMatrix(workset, bf, wbf);

    for (int test = 0; test < 2; ++test) {
      Manager::Field f;
      f.name   = "";
      f.layout = layout;
      f.data_  = Teuchos::rcp(new Manager::Field::Data());

      // b.
      if (test == 0) {
        f.data_->transformation = Transformation::none;
        f.num_g_fields          = 1;
        loop(f_mdf, cell, 0) loop(f_mdf, qp, 1) for (unsigned i = 0, k = 0; i < f_mdf.dimension(2); ++i) for (unsigned j = 0; j < f_mdf.dimension(3); ++j, ++k)
            f_mdf(cell, qp, i, j) = eval_f(coord_qp(cell, qp, 0), coord_qp(cell, qp, 1), coord_qp(cell, qp, 2), k);
      } else {
        f.data_->transformation = Transformation::right_polar_LieR_LieS;
        f.num_g_fields          = 2;
        RealType lo[3], hi[3];
        getBoundingBox(coord_vert, lo, hi);
        loop(f_mdf, cell, 0) loop(f_mdf, qp, 1)
        {
          RealType pt[3];
          for (int k = 0; k < 3; ++k) pt[k] = (coord_qp(cell, qp, k) - lo[k]) / (hi[k] - lo[k]);
          const minitensor::Tensor<RealType> F                      = eval_F(pt);
          loop(f_mdf, i, 2) loop(f_mdf, j, 3) f_mdf(cell, qp, i, j) = F(i, j);
        }
      }

      // @ibaned: the following is a workaround, @rppawlo will get
      // `f_mdf_const = f_mdf` to work soon.
      PHX::MDField<const RealType> f_mdf_const("f_mdf_const", layout);
      f_mdf_const.setFieldData(f_mdf_data);

      p.fillRhs(f_mdf_const, f, workset, wbf);

      // Solve M x = b.
      p.project(f);

      if (test == 0) {  // Compare with true values at NP.
        auto                  indexer = Albany::createGlobalLocalIndexer(pc.get_node_vs());
        int const             ncol = 9, nverts = pc.get_node_vs()->dim();
        std::vector<RealType> f_true(ncol * nverts);
        {
          std::vector<bool> evaled(nverts, false);
          loop(f_mdf, cell, 0) loop(coord_vert, node, 1)
          {
            const GO gid = workset.wsElNodeID[cell][node];
            const LO lid = indexer->getLocalElement(gid);
            if (!evaled[lid]) {
              for (int k = 0; k < ncol; ++k)
                f_true[ncol * lid + k] = eval_f(coord_vert(cell, node, 0), coord_vert(cell, node, 1), coord_vert(cell, node, 2), k);
              evaled[lid] = true;
            }
          }
        }
        double err1[9], errmax[9], scale[9];
        auto   data = Albany::getLocalData(f.data_->mv[0].getConst());
        for (int k = 0; k < ncol; ++k) {
          err1[k] = errmax[k] = scale[k] = 0;
        }
        for (int iv = 0; iv < nverts; ++iv)
          for (int k = 0; k < ncol; ++k) {
            double const d = std::abs(data[k][iv] - f_true[ncol * iv + k]);
            err1[k] += d;
            errmax[k] = std::max(errmax[k], d);
            scale[k]  = std::max(scale[k], std::abs(f_true[ncol * iv + k]));
          }
        printf("err np (test %d):", test);
        int const n = f_mdf.dimension(0) * f_mdf.dimension(1);
        for (int k = 0; k < 9; ++k) printf(" %1.2e %1.2e (%1.2e)", err1[k] / (n * scale[k]), errmax[k] / scale[k], scale[k]);
        std::cout << "\n";
      }

      // Interpolate to IP.
      p.interp(f, workset, bf, mda[0], mda[1]);
      transformStateArray(Direction::g2G, f.data_->transformation, mda[0], mda[1]);

      {  // Compare with true values at IP.
        double err1[9], errmax[9], scale[9];
        for (int k = 0; k < 9; ++k) {
          err1[k] = errmax[k] = scale[k] = 0;
        }
        loop(f_mdf, cell, 0) loop(f_mdf, qp, 1) for (int i = 0, k = 0; i < static_cast<int>(f_mdf.dimension(2));
                                                     ++i) for (int j = 0; j < static_cast<int>(f_mdf.dimension(3)); ++j, ++k)
        {
          double const d = std::abs(mda[0]((int)cell, (int)qp, i, j) - f_mdf(cell, qp, i, j));
          err1[k] += d;
          errmax[k] = std::max(errmax[k], d);
          scale[k]  = std::max(scale[k], std::abs(f_mdf(cell, qp, i, j)));
        }
        printf("err ip (test %d):", test);
        int const n = f_mdf.dimension(0) * f_mdf.dimension(1);
        for (int k = 0; k < 9; ++k) printf(" %1.2e %1.2e (%1.2e)", err1[k] / (n * scale[k]), errmax[k] / scale[k], scale[k]);
        std::cout << "\n";
      }
    }
  }

  struct ProjectorTester::Impl
  {
    enum
    {
      ntests = 2
    };
    bool      projected, finished;
    Projector p;
    struct Point
    {
      RealType x[3];
      bool
      operator<(const Point& pt) const
      {
        for (int i = 0; i < 3; ++i) {
          if (x[i] < pt.x[i]) return true;
          if (x[i] > pt.x[i]) return false;
        }
        return false;
      }
    };
    struct FValues
    {
      RealType f[9];
    };
    typedef std::map<Point, FValues> Map;
    struct TestData
    {
      Manager::Field f;
      Map            f_true_qp, f_interp_qp;
    };
    TestData td[ntests];
  };

  ProjectorTester::ProjectorTester()
  {
    d = Teuchos::rcp(new Impl());
    for (int test = 0; test < Impl::ntests; ++test) {
      Impl::TestData& td = d->td[test];
      Manager::Field& f  = td.f;
      f.name             = "";
      f.data_            = Teuchos::rcp(new Manager::Field::Data());
      if (test == 0) {
        f.data_->transformation = Transformation::none;
        f.num_g_fields          = 1;
      } else {
        f.data_->transformation = Transformation::right_polar_LieR_LieS;
        f.num_g_fields          = 2;
      }
    }
  }

  void
  ProjectorTester::init(Teuchos::RCP<Thyra_VectorSpace const> const& node_vs, Teuchos::RCP<Thyra_VectorSpace const> const& ol_node_vs)
  {
    d->p.init(node_vs, ol_node_vs);
    d->projected = d->finished = false;
  }

  // Figure out what needs to be done given the current state.
  void
  ProjectorTester::eval(
      const PHAL::Workset&                                workset,
      const Manager::BasisField&                          bf,
      const Manager::BasisField&                          wbf,
      const PHX::MDField<RealType, Cell, QuadPoint, Dim>& coord_qp)
  {
    if (d->finished) {
      return;
    }
    int const num_qp = coord_qp.dimension(1);
    if (workset.numCells > 0 && num_qp > 0) {
      Impl::Point p;
      for (int i = 0; i < 3; ++i) p.x[i] = coord_qp(0, 0, i);
      Impl::Map::const_iterator it = d->td[0].f_true_qp.find(p);
      if (it == d->td[0].f_true_qp.end()) {
        d->p.fillMassMatrix(workset, bf, wbf);
        fillRhs(workset, wbf, coord_qp);
      } else {
        if (!d->projected) {
          project();
          d->projected = true;
        }
        it = d->td[0].f_interp_qp.find(p);
        if (it == d->td[0].f_interp_qp.end()) {
          interp(workset, bf, coord_qp);
        } else {
          finish();
          d->finished = true;
        }
      }
    }
  }

  void
  ProjectorTester::fillRhs(const PHAL::Workset& workset, const Manager::BasisField& wbf, const PHX::MDField<RealType, Cell, QuadPoint, Dim>& coord_qp)
  {
    int const num_qp = coord_qp.dimension(1), num_dim = coord_qp.dimension(2);

    typedef PHX::MDALayout<Cell, QuadPoint, Dim, Dim> Layout;
    Teuchos::RCP<Layout>                              layout = Teuchos::rcp(new Layout(workset.numCells, num_qp, num_dim, num_dim));
    PHX::MDField<RealType>                            f_mdf  = PHX::MDField<RealType>("f_mdf", layout);
    auto f_mdf_data = PHX::KokkosViewFactory<RealType, typename PHX::DevLayout<RealType>::type, PHX::Device>::buildView(f_mdf.fieldTag());
    f_mdf.setFieldData(f_mdf_data);

    for (int test = 0; test < Impl::ntests; ++test) {
      Impl::TestData& td = d->td[test];
      Manager::Field& f  = td.f;
      f.layout           = layout;

      // Fill f_mdf and f_true_qp.
      loop(f_mdf, cell, 0) loop(f_mdf, qp, 1)
      {
        Impl::Point p;
        for (int i = 0; i < 3; ++i) p.x[i] = coord_qp(cell, qp, i);
        Impl::FValues fv;
        if (test == 0)
          for (int k = 0; k < 9; ++k) fv.f[k] = eval_f(p.x[0], p.x[1], p.x[2], k);
        else {
          // I don't have a bounding box, so come up with something reasonable.
          RealType alpha[3]                                         = {0, 0, 0};
          alpha[0]                                                  = (100 + p.x[0]) / 200;
          const minitensor::Tensor<RealType> F                      = eval_F(alpha);
          loop(f_mdf, i, 2) loop(f_mdf, j, 3) fv.f[num_dim * i + j] = F(i, j);
        }
        td.f_true_qp[p]                                           = fv;
        loop(f_mdf, i, 2) loop(f_mdf, j, 3) f_mdf(cell, qp, i, j) = fv.f[num_dim * i + j];
      }

      PHX::MDField<const RealType> f_mdf_const("f_mdf_const", layout);
      f_mdf_const.setFieldData(f_mdf_data);

      d->p.fillRhs(f_mdf_const, f, workset, wbf);
    }
  }

  void
  ProjectorTester::project()
  {
    for (int test = 0; test < Impl::ntests; ++test) d->p.project(d->td[test].f);
  }

  void
  ProjectorTester::interp(const PHAL::Workset& workset, const Manager::BasisField& bf, const PHX::MDField<RealType, Cell, QuadPoint, Dim>& coord_qp)
  {
    int const num_qp = coord_qp.dimension(1), num_dim = coord_qp.dimension(2);

    // Quick exit if we've already done this workset.
    if (workset.numCells > 0 && num_qp > 0) {
      Impl::Point p;
      for (int i = 0; i < 3; ++i) p.x[i] = coord_qp(0, 0, i);
      const Impl::Map::const_iterator it = d->td[0].f_interp_qp.find(p);
      if (it != d->td[0].f_interp_qp.end()) return;
    }

    std::vector<Albany::MDArray> mda;
    std::vector<double>          mda_data[2];
    for (int i = 0; i < 2; ++i) {
      typedef Albany::MDArray::size_type size_t;
      mda_data[i].resize(workset.numCells * num_qp * num_dim * num_dim);
      shards::Array<RealType, shards::NaturalOrder, Cell, QuadPoint, Dim, Dim> a;
      a.assign(&mda_data[i][0], workset.numCells, num_qp, num_dim, num_dim);
      mda.push_back(a);
    }

    for (int test = 0; test < Impl::ntests; ++test) {
      Impl::TestData& td = d->td[test];
      Manager::Field& f  = td.f;
      // Interpolate to IP.
      d->p.interp(f, workset, bf, mda[0], mda[1]);
      transformStateArray(Direction::g2G, f.data_->transformation, mda[0], mda[1]);
      // Record for later comparison.
      loop(mda[0], cell, 0) loop(mda[0], qp, 1)
      {
        Impl::Point p;
        for (int i = 0; i < 3; ++i) p.x[i] = coord_qp(cell, qp, i);
        Impl::FValues fv;
        loop(mda[0], i, 2) loop(mda[0], j, 3) fv.f[num_dim * i + j] = mda[0](cell, qp, i, j);
        td.f_interp_qp[p]                                           = fv;
      }
    }
  }

  void
  ProjectorTester::finish()
  {
    for (int test = 0; test < Impl::ntests; ++test) {
      Impl::TestData& td = d->td[test];
      // Compare with true values at IP.
      double err1[9], errmax[9], scale[9];
      for (int k = 0; k < 9; ++k) {
        err1[k] = errmax[k] = scale[k] = 0;
      }
      for (Impl::Map::const_iterator it = td.f_true_qp.begin(); it != td.f_true_qp.end(); ++it) {
        const Impl::Point&              p         = it->first;
        const Impl::FValues&            fv_true   = it->second;
        const Impl::Map::const_iterator it_interp = td.f_interp_qp.find(p);
        if (it_interp == td.f_interp_qp.end()) {
          break;
        }
        const Impl::FValues& fv_interp = it_interp->second;
        for (int k = 0; k < 9; ++k) {
          double const diff = std::abs(fv_true.f[k] - fv_interp.f[k]);
          err1[k] += diff;
          errmax[k] = std::max(errmax[k], diff);
          scale[k]  = std::max(scale[k], std::abs(fv_true.f[k]));
        }
      }

      double                                       gerr1[9], gerrmax[9], gscale[9];
      int                                          gn;
      Teuchos::RCP<Teuchos::Comm<int> const> const comm = Teuchos::DefaultComm<int>::getComm();
      Teuchos::reduceAll(*comm, Teuchos::REDUCE_MAX, 9, err1, gerr1);
      Teuchos::reduceAll(*comm, Teuchos::REDUCE_MAX, 9, errmax, gerrmax);
      Teuchos::reduceAll(*comm, Teuchos::REDUCE_MAX, 9, scale, gscale);
      int const n = td.f_true_qp.size();
      Teuchos::reduceAll(*comm, Teuchos::REDUCE_SUM, 1, &n, &gn);

      if (comm->getRank() == 0) {
        printf("err ip (test %d):", test);
        for (int k = 0; k < 9; ++k) printf(" %1.2e %1.2e (%1.2e)", gerr1[k] / (gn * gscale[k]), gerrmax[k] / gscale[k], gscale[k]);
        std::cout << "\n";
      }

      // Reset for next one.
      td.f_true_qp.clear();
      td.f_interp_qp.clear();
      td.f.data_->mv[0] = td.f.data_->mv[1] = Teuchos::null;
    }
  }
  }  // namespace testing
}  // namespace
}  // namespace rc
}  // namespace AAdapt

#undef loop
