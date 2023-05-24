// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#include "AAdapt_Erosion.hpp"

#include <Teuchos_TimeMonitor.hpp>
#include <stk_util/parallel/ParallelReduce.hpp>

#include "Albany_GenericSTKMeshStruct.hpp"
#include "StateVarUtils.hpp"
#include "Topology_FailureCriterion.hpp"

namespace AAdapt {
AAdapt::Erosion::Erosion(
    Teuchos::RCP<Teuchos::ParameterList> const& params,
    Teuchos::RCP<ParamLib> const&               param_lib,
    Albany::StateManager const&                 state_mgr,
    Teuchos::RCP<Teuchos_Comm const> const&     comm)
    : AAdapt::AbstractAdapter(params, param_lib, state_mgr, comm), remesh_file_index_(1)
{
  discretization_     = state_mgr_.getDiscretization();
  auto* pdisc         = discretization_.get();
  stk_discretization_ = static_cast<Albany::STKDiscretization*>(pdisc);
  stk_mesh_struct_    = stk_discretization_->getSTKMeshStruct();
  bulk_data_          = stk_mesh_struct_->bulkData;
  meta_data_          = stk_mesh_struct_->metaData;
  num_dim_            = stk_mesh_struct_->numDim;

  // Save the initial output file name
  base_exo_filename_    = stk_mesh_struct_->exoOutFile;
  rename_exodus_output_ = params->get<bool>("Rename Exodus Output", false);
  enable_erosion_       = params->get<bool>("Enable Erosion", true);
  params->validateParameters(*(getValidAdapterParameters()));
  topology_               = Teuchos::rcp(new LCM::Topology(discretization_, "", ""));
  auto const lower_corner = topology_->minimumCoordinates();
  auto const upper_corner = topology_->maximumCoordinates();
  auto const bluff_height = upper_corner(2) - lower_corner(2);
  auto const bluff_width  = upper_corner(1) - lower_corner(1);
  cross_section_          = bluff_height * bluff_width;
  failure_state_name_     = "failure_state";
  failure_criterion_      = Teuchos::rcp(new LCM::BulkFailureCriterion(*topology_, failure_state_name_));
  topology_->set_failure_criterion(failure_criterion_);
}

bool
AAdapt::Erosion::queryAdaptationCriteria(int)
{
  return topology_->there_are_failed_cells_global();
}

namespace {

void
copyStateArray(Albany::StateArrayVec const& src, Albany::StateArrayVec& dst, AAdapt::StoreT& store)
{
  auto const num_ws = src.size();
  dst.resize(num_ws);
  store.resize(num_ws);
  for (auto ws = 0; ws < num_ws; ++ws) {
    auto&& src_map = src[ws];
    auto&& dst_map = dst[ws];
    for (auto&& kv : src_map) {
      auto&&     state_name = kv.first;
      auto&&     src_states = kv.second;
      auto&&     dst_states = dst_map[state_name];
      auto const num_states = src_states.size();
      auto const rank       = src_states.rank();
      using DimT            = decltype(src_states.dimension(0));
      using TagT            = decltype(src_states.tag(0));
      std::vector<DimT> dims(rank);
      std::vector<TagT> tags(rank);
      for (auto i = 0; i < rank; ++i) {
        dims[i] = src_states.dimension(i);
        tags[i] = src_states.tag(i);
      }
      store[ws][state_name].resize(num_states);
      auto*   pval = &store[ws][state_name][0];
      auto*   pdim = &dims[0];
      auto*   ptag = &tags[0];
      MDArray mda(pval, rank, pdim, ptag);
      dst_states = mda;
      for (auto s = 0; s < num_states; ++s) {
        dst_states[s] = src_states[s];
      }
    }
  }
}

}  // anonymous namespace

void
AAdapt::Erosion::copyStateArrays(Albany::StateArrays const& sa)
{
  auto&& src_esa = sa.elemStateArrays;
  auto&& dst_esa = state_arrays_.elemStateArrays;
  copyStateArray(src_esa, dst_esa, cell_state_store_);
  auto&& src_nsa = sa.nodeStateArrays;
  auto&& dst_nsa = state_arrays_.nodeStateArrays;
  copyStateArray(src_nsa, dst_nsa, node_state_store_);
}

void
AAdapt::Erosion::transferStateArrays()
{
  auto&&     new_sa       = this->state_mgr_.getStateArrays();
  auto       sis          = this->state_mgr_.getStateInfoStruct();
  auto&&     new_esa      = new_sa.elemStateArrays;
  auto&&     old_esa      = state_arrays_.elemStateArrays;
  auto&&     gidwslid_old = gidwslid_map_;
  auto&&     wslidgid_new = stk_discretization_->getElemWsLIDGIDMap();
  auto const num_ws       = new_esa.size();

  auto mapWsLID = [&](int ws, int lid) {
    auto wslid       = std::make_pair(ws, lid);
    auto wslidgid_it = wslidgid_new.find(wslid);
    assert(wslidgid_it != wslidgid_new.end());
    auto gid         = wslidgid_it->second;
    auto gidwslid_it = gidwslid_old.find(gid);
    assert(gidwslid_it != gidwslid_old.end());
    auto wslid_old = gidwslid_it->second;
    auto ws_old    = wslid_old.ws;
    auto lid_old   = wslid_old.LID;
    return std::make_pair(ws_old, lid_old);
  };

  auto oldValue1 = [&](int ws, std::string const& state, int lid) {
    auto old_wslid = mapWsLID(ws, lid);
    auto old_ws    = old_wslid.first;
    auto old_lid   = old_wslid.second;
    return old_esa[old_ws][state](old_lid);
  };

  auto oldValue2 = [&](int ws, std::string const& state, int lid, int qp) {
    auto old_wslid = mapWsLID(ws, lid);
    auto old_ws    = old_wslid.first;
    auto old_lid   = old_wslid.second;
    return old_esa[old_ws][state](old_lid, qp);
  };

  auto oldValue3 = [&](int ws, std::string const& state, int lid, int qp, int i) {
    auto old_wslid = mapWsLID(ws, lid);
    auto old_ws    = old_wslid.first;
    auto old_lid   = old_wslid.second;
    return old_esa[old_ws][state](old_lid, qp, i);
  };

  auto oldValue4 = [&](int ws, std::string const& state, int lid, int qp, int i, int j) {
    auto old_wslid = mapWsLID(ws, lid);
    auto old_ws    = old_wslid.first;
    auto old_lid   = old_wslid.second;
    return old_esa[old_ws][state](old_lid, qp, i, j);
  };

  auto oldValue5 = [&](int ws, std::string const& state, int lid, int qp, int i, int j, int k) {
    auto old_wslid = mapWsLID(ws, lid);
    auto old_ws    = old_wslid.first;
    auto old_lid   = old_wslid.second;
    return old_esa[old_ws][state](old_lid, qp, i, j, k);
  };

  for (auto ws = 0; ws < num_ws; ++ws) {
    for (auto s = 0; s < sis->size(); ++s) {
      std::string const&             state_name = (*sis)[s]->name;
      std::string const&             init_type  = (*sis)[s]->initType;
      Albany::StateStruct::FieldDims dims;
      new_esa[ws][state_name].dimensions(dims);
      int size = dims.size();
      if (size == 0) return;
      switch (size) {
        case 1:
          for (auto cell = 0; cell < dims[0]; ++cell) {
            double& value = new_esa[ws][state_name](cell);
            value         = oldValue1(ws, state_name, cell);
          }
          break;
        case 2:
          for (auto cell = 0; cell < dims[0]; ++cell) {
            for (auto qp = 0; qp < dims[1]; ++qp) {
              double& value = new_esa[ws][state_name](cell, qp);
              value         = oldValue2(ws, state_name, cell, qp);
            }
          }
          break;
        case 3:
          for (auto cell = 0; cell < dims[0]; ++cell) {
            for (auto qp = 0; qp < dims[1]; ++qp) {
              for (auto i = 0; i < dims[2]; ++i) {
                double& value = new_esa[ws][state_name](cell, qp, i);
                value         = oldValue3(ws, state_name, cell, qp, i);
              }
            }
          }
          break;
        case 4:
          for (int cell = 0; cell < dims[0]; ++cell) {
            for (int qp = 0; qp < dims[1]; ++qp) {
              for (int i = 0; i < dims[2]; ++i) {
                for (int j = 0; j < dims[3]; ++j) {
                  double& value = new_esa[ws][state_name](cell, qp, i, j);
                  value         = oldValue4(ws, state_name, cell, qp, i, j);
                }
              }
            }
          }
          break;
        case 5:
          for (int cell = 0; cell < dims[0]; ++cell) {
            for (int qp = 0; qp < dims[1]; ++qp) {
              for (int i = 0; i < dims[2]; ++i) {
                for (int j = 0; j < dims[3]; ++j) {
                  for (int k = 0; k < dims[4]; ++k) {
                    double& value = new_esa[ws][state_name](cell, qp, i, j, k);
                    value         = oldValue5(ws, state_name, cell, qp, i, j, k);
                  }
                }
              }
            }
          }
          break;
        default: ALBANY_ASSERT(1 <= size && size <= 5, ""); break;
      }
    }
  }
}

bool
AAdapt::Erosion::adaptMesh()
{
  if (enable_erosion_ == false) return true;

  *output_stream_ << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n"
                  << "Adapting mesh using AAdapt::Erosion method      \n"
                  << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";

  // Open the new exodus file for results
  if (rename_exodus_output_ == true) {
    // Save the current results and close the exodus file
    // Create a remeshed output file naming convention by
    // adding the remesh_file_index_ ahead of the period
    std::ostringstream ss;
    std::string        str = base_exo_filename_;
    ss << ".e-s." << remesh_file_index_;
    str.replace(str.find(".e"), std::string::npos, ss.str());
    *output_stream_ << "Remeshing: renaming output file to - " << str << '\n';
    stk_discretization_->reNameExodusOutput(str);
    remesh_file_index_++;
  } else {
    stk_discretization_->reNameExodusOutput(tmp_adapt_filename_);
  }

  // Start the mesh update process
  double const local_volume = topology_->erodeFailedElements();
  auto const   num_cells    = topology_->numberCells();
  auto const   rank         = topology_->get_proc_rank();
  // ALBANY_ASSERT(num_cells > 0, "Zero elements on processor #" << rank << '\n');
  double global_volume{0.0};
  auto   comm = static_cast<stk::ParallelMachine>(MPI_COMM_WORLD);
  stk::all_reduce_sum(comm, &local_volume, &global_volume, 1);
  erosion_volume_ += global_volume;

  // Throw away all the Albany data structures and re-build them from the mesh
  auto const rebalance = adapt_params_->get<bool>("Rebalance", false);
  if (rebalance == true) {
    auto stk_mesh_struct = Teuchos::rcp_dynamic_cast<Albany::GenericSTKMeshStruct>(stk_discretization_->getSTKMeshStruct());
    stk_mesh_struct->rebalanceAdaptedMeshT(adapt_params_, teuchos_comm_);
  }
  stk_discretization_->updateMesh();
  stk_discretization_->setOutputInterval(1);

  *output_stream_ << "*** ACE INFO: Eroded Volume : " << erosion_volume_ << '\n';
  *output_stream_ << "*** ACE INFO: Eroded Length : " << erosion_volume_ / cross_section_ << '\n';

  return true;
}

void
AAdapt::Erosion::postAdapt()
{
}

Teuchos::RCP<Teuchos::ParameterList const>
AAdapt::Erosion::getValidAdapterParameters() const
{
  auto valid_pl = this->getGenericAdapterParams("Valid Erosion Params");
  valid_pl->set<bool>("Equilibrate", false, "Perform a steady solve after adaptation");
  valid_pl->set<bool>("Rebalance", true, "Rebalance mesh after adaptation in parallel runs");
  valid_pl->set<bool>("Rename Exodus Output", false, "Use different exodus file names for adapted meshes");
  valid_pl->set<bool>("Enable Erosion", true, "Allows disabling of erosion, mostly for testing");
  return valid_pl;
}

}  // namespace AAdapt
