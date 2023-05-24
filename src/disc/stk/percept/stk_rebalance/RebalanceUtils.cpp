/*
  Copyright 2002, 2010, 2011 National Technology & Engineering Solutions of
  Sandia, LLC (NTESS) Under the terms of Contract DE-AC04-94AL85000, there is
  a non-exclusive license for use of this work by or on behalf of the U.S.
  Government. Export of this program may require a license from the United
  States Government.
*/

#include <percept/stk_rebalance_utils/RebalanceUtils.hpp>
#include <stk_mesh/base/BulkData.hpp>
#include <stk_mesh/base/Entity.hpp>
#include <stk_mesh/base/Field.hpp>
#include <stk_mesh/base/GetEntities.hpp>
#include <stk_mesh/base/MetaData.hpp>
#include <stk_util/parallel/ParallelReduce.hpp>

double
stk::rebalance::check_balance(
    mesh::BulkData&             bulk_data,
    const mesh::Field<double>*  load_measure,
    const stk::mesh::EntityRank rank,
    const mesh::Selector*       selector)
{
  const ParallelMachine& comm    = bulk_data.parallel();
  double                 my_load = 0.0;

  const mesh::MetaData& meta_data = stk::mesh::MetaData::get(bulk_data);

  mesh::EntityVector local_elems;
  if (selector) {
    mesh::get_selected_entities(*selector, bulk_data.buckets(rank), local_elems);
  } else {
    mesh::Selector select_owned(meta_data.locally_owned_part());
    // Determine imbalance based on current element decomposition
    mesh::get_selected_entities(select_owned, bulk_data.buckets(rank), local_elems);
  }

  for (mesh::EntityVector::iterator elem_it = local_elems.begin(); elem_it != local_elems.end(); ++elem_it) {
    if (load_measure) {
      double const* load_val = mesh::field_data(*load_measure, *elem_it);
      my_load += *load_val;
    } else {
      my_load += 1;
    }
  }

  double max_load = my_load;
  double tot_load = my_load;

  all_reduce(comm, ReduceMax<1>(&max_load) & ReduceSum<1>(&tot_load));

  int const    proc_size = parallel_machine_size(comm);
  double const avg_load  = tot_load / proc_size;

  double const imbalance_threshold = max_load / avg_load;

  return imbalance_threshold;
}

bool
stk::rebalance::verify_dependent_ownership(stk::mesh::BulkData& bulk_data, const stk::mesh::EntityRank& parent_rank, stk::mesh::EntityVector& entities)
{
  bool is_with_elem = true;
  for (size_t i = 0; i < entities.size(); ++i) {
    // is_with_elem = false;

    stk::mesh::Entity                 entity     = entities[i];
    unsigned                          owner_proc = bulk_data.parallel_owner_rank(entity);
    const percept::MyPairIterRelation rel(bulk_data, entity, parent_rank);
    // const stk::mesh::PairIterRelation rel = entity->relations( parent_rank );
    const unsigned num_elems = rel.size();

    if (num_elems) is_with_elem = false;
    for (unsigned j = 0; j < num_elems; ++j) {
      stk::mesh::Entity elem = rel[j].entity();
      if ((int)owner_proc == bulk_data.parallel_owner_rank(elem)) {
        is_with_elem = true;
        break;
      }
    }
    if (!is_with_elem) return false;
  }

  return is_with_elem;
}

void
stk::rebalance::check_ownership(mesh::BulkData& bulk_data, stk::mesh::EntityVector& entities, std::string const& msg)
{
  const unsigned entity_iter_len = entities.size();
  std::cout << "P[" << bulk_data.parallel_rank() << "] entity_iter_len= " << entity_iter_len << std::endl;
  for (unsigned entity_iter = 0; entity_iter < entity_iter_len; ++entity_iter) {
    mesh::Entity mesh_ent = entities[entity_iter];
    if (bulk_data.parallel_owner_rank(mesh_ent) != bulk_data.parallel_rank()) {
      std::cout << "check_ownership: an input mesh entity is not owned msg= " + msg << " key= " << bulk_data.entity_key(mesh_ent) << std::endl;
      throw std::runtime_error("check_ownership: an input mesh entity is not owned msg= " + msg);
    }
  }
}
