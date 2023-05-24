
// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

//---------------------------------------------------------------------------//
/*!
 * \file   interpolation_volume_to_ns.cpp
 * \author Irina Tezaur (ikalash@sandia.gov)
 * \brief  Projection of solution from source (volume) to target (nodeset).
 *         Currently this can be used in the implementation of the alternating
 *         Schwarz method.
 */
//---------------------------------------------------------------------------//

#include <Ionit_Initializer.h>
#include <Ioss_SubSystem.h>

#include <Intrepid_FieldContainer.hpp>
#include <Teuchos_Array.hpp>
#include <Teuchos_ArrayRCP.hpp>
#include <Teuchos_CommHelpers.hpp>
#include <Teuchos_DefaultComm.hpp>
#include <Teuchos_DefaultMpiComm.hpp>
#include <Teuchos_GlobalMPISession.hpp>
#include <Teuchos_OpaqueWrapper.hpp>
#include <Teuchos_RCP.hpp>
#include <Teuchos_StandardCatchMacros.hpp>
#include <Teuchos_TimeMonitor.hpp>
#include <Teuchos_TypeTraits.hpp>
#include <Teuchos_VerboseObject.hpp>
#include <Tpetra_MultiVector.hpp>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>
#include <stk_io/IossBridge.hpp>
#include <stk_io/StkMeshIoBroker.hpp>
#include <stk_mesh/base/BulkData.hpp>
#include <stk_mesh/base/CoordinateSystems.hpp>
#include <stk_mesh/base/Field.hpp>
#include <stk_mesh/base/FieldBase.hpp>
#include <stk_mesh/base/GetEntities.hpp>
#include <stk_mesh/base/MetaData.hpp>
#include <stk_mesh/base/Selector.hpp>
#include <stk_mesh/base/Types.hpp>
#include <stk_topology/topology.hpp>
#include <stk_util/parallel/Parallel.hpp>
#include <vector>

#include "Albany_Macros.hpp"
#include "DTK_MapOperatorFactory.hpp"
#include "DTK_STKMeshHelpers.hpp"
#include "DTK_STKMeshManager.hpp"
#include "Teuchos_CommandLineProcessor.hpp"
#include "Teuchos_ParameterList.hpp"
#include "Teuchos_XMLParameterListCoreHelpers.hpp"
#include "Teuchos_YamlParameterListCoreHelpers.hpp"

template <typename FieldType>
void
interpolate(Teuchos::RCP<Teuchos::Comm<int> const> comm, Teuchos::RCP<Teuchos::ParameterList> plist)
{
  Teuchos::RCP<Teuchos::FancyOStream> out = Teuchos::fancyOStream(Teuchos::VerboseObjectBase::getDefaultOStream());
  // Read command-line options
  std::string source_mesh_input_file  = plist->get<std::string>("Source Mesh Input File");
  int         src_snap_no             = plist->get<int>("Source Mesh Snapshot Number", 1);  // this value is 1-based
  std::string target_mesh_input_file  = plist->get<std::string>("Target Mesh Input File");
  std::string target_mesh_output_file = plist->get<std::string>("Target Mesh Output File");
  int         tgt_snap_no             = plist->get<int>("Target Mesh Snapshot Number", 1);  // this value is 1-based
  std::string target_mesh_part_name   = plist->get<std::string>("Target Mesh Part");
  std::string source_field_name       = plist->get<std::string>("Source Field Name", "solution");
  std::string target_field_name       = plist->get<std::string>("Target Field Name", "solution");
  bool        write_dirichlet_field   = plist->get<bool>("Write dirichlet_field to Exodus", false);
  std::string tgt_interp_field_name   = source_field_name + "Ref";

  // Get the raw mpi communicator (basic typedef in STK).
  Teuchos::RCP<const Teuchos::MpiComm<int>>            mpi_comm         = Teuchos::rcp_dynamic_cast<const Teuchos::MpiComm<int>>(comm);
  Teuchos::RCP<const Teuchos::OpaqueWrapper<MPI_Comm>> opaque_comm      = mpi_comm->getRawMpiComm();
  stk::ParallelMachine                                 parallel_machine = (*opaque_comm)();

  // SOURCE MESH READ
  // ----------------
  stk::io::StkMeshIoBroker src_broker(parallel_machine);
  std::size_t              src_input_index = src_broker.add_mesh_database(source_mesh_input_file, "exodus", stk::io::READ_MESH);
  src_broker.set_active_mesh(src_input_index);
  src_broker.create_input_mesh();

  // number of intervals to divide each input time step into
  int                                 interpolation_intervals = 1;
  stk::io::MeshField::TimeMatchOption tmo                     = stk::io::MeshField::CLOSEST;
  if (interpolation_intervals > 1) tmo = stk::io::MeshField::LINEAR_INTERPOLATION;

  src_broker.add_all_mesh_fields_as_input_fields(tmo);
  src_broker.populate_bulk_data();
  Teuchos::RCP<stk::mesh::BulkData> src_bulk_data = Teuchos::rcpFromRef(src_broker.bulk_data());

  stk::util::ParameterList   parameters;
  Teuchos::RCP<Ioss::Region> io_region = src_broker.get_input_io_region();
  STKIORequire(!Teuchos::is_null(io_region));

  // Get number of time steps in source mesh
  int timestep_count = io_region->get_property("state_count").get_int();
  int step           = src_snap_no;
  if (step > timestep_count)
    ALBANY_ABORT(
        std::endl
        << "Invalid value of Source Mesh Snapshot Number = " << src_snap_no << " > total number of snapshots in " << source_mesh_input_file << " = "
        << timestep_count << "." << std::endl);
  if (step <= 0) ALBANY_ABORT(std::endl << "Invalid value of Source Mesh Snapshot Number = " << src_snap_no << "; value must be > 0." << std::endl);

  if (timestep_count > 0) {
    double time = io_region->get_state_time(step);
    if (step == timestep_count) interpolation_intervals = 1;

    int    step_end = step < timestep_count ? step + 1 : step;
    double tend     = io_region->get_state_time(step_end);
    double tbeg     = time;
    double delta    = (tend - tbeg) / static_cast<double>(interpolation_intervals);

    for (int interval = 0; interval < interpolation_intervals; interval++) {
      time = tbeg + delta * static_cast<double>(interval);
      src_broker.read_defined_input_fields(time);
    }
  }

  // DEFINE PARTS/SELECTOR
  // ----------------

  stk::mesh::Selector            src_stk_selector = stk::mesh::Selector(src_broker.meta_data().universal_part());
  stk::mesh::BucketVector        src_part_buckets = src_stk_selector.get_buckets(stk::topology::NODE_RANK);
  std::vector<stk::mesh::Entity> src_part_nodes;
  stk::mesh::get_selected_entities(src_stk_selector, src_part_buckets, src_part_nodes);
  Intrepid::FieldContainer<double> src_node_coords =
      DataTransferKit::STKMeshHelpers::getEntityNodeCoordinates(Teuchos::Array<stk::mesh::Entity>(src_part_nodes), *src_bulk_data);

  // TARGET MESH READ
  // ----------------

  // Load the target mesh.
  stk::io::StkMeshIoBroker tgt_broker(parallel_machine);
  std::size_t              tgt_input_index = tgt_broker.add_mesh_database(target_mesh_input_file, "exodus", stk::io::READ_MESH);
  tgt_broker.set_active_mesh(tgt_input_index);
  tgt_broker.create_input_mesh();
  tgt_broker.add_all_mesh_fields_as_input_fields(tmo);

  // Get source_field from source mesh
  FieldType* source_field = src_broker.meta_data().get_field<FieldType>(stk::topology::NODE_RANK, source_field_name);
  if (source_field != 0)
    *out << "   Field with name " << source_field_name << " found in source mesh file!" << std::endl;
  else
    ALBANY_ABORT(std::endl << "   Field with name " << source_field_name << " NOT found in source mesh file!" << std::endl);

  int neq = source_field->max_size(stk::topology::NODE_RANK);

  // Put fields on target mesh
  // Add a nodal field to the interpolated target part.
  FieldType& target_interp_field = tgt_broker.meta_data().declare_field<FieldType>(stk::topology::NODE_RANK, tgt_interp_field_name);
  stk::mesh::put_field_on_mesh(target_interp_field, tgt_broker.meta_data().universal_part(), neq, nullptr);
  FieldType& dirichlet_field = tgt_broker.meta_data().declare_field<FieldType>(stk::topology::NODE_RANK, "dirichlet_field");
  stk::mesh::put_field_on_mesh(dirichlet_field, tgt_broker.meta_data().universal_part(), neq, nullptr);

  // Create the target bulk data.
  tgt_broker.populate_bulk_data();
  Teuchos::RCP<stk::mesh::BulkData> tgt_bulk_data = Teuchos::rcpFromRef(tgt_broker.bulk_data());

  // Add a nodal field to the interpolated target part.
  // Populate target_field
  FieldType* target_field = tgt_broker.meta_data().get_field<FieldType>(stk::topology::NODE_RANK, target_field_name);
  if (target_field != 0)
    *out << "   Field with name " << target_field_name << " found in target mesh file!" << std::endl;
  else
    ALBANY_ABORT(std::endl << "   Field with name " << target_field_name << " NOT found in target mesh file!" << std::endl);

  io_region = tgt_broker.get_input_io_region();
  STKIORequire(!Teuchos::is_null(io_region));

  // Get number of time steps in source mesh
  timestep_count = io_region->get_property("state_count").get_int();
  step           = tgt_snap_no;
  if (step > timestep_count)
    ALBANY_ABORT(
        std::endl
        << "Invalid value of Target Mesh Snapshot Number = " << tgt_snap_no << " > total number of snapshots in " << target_mesh_input_file << " = "
        << timestep_count << "." << std::endl);
  if (step <= 0) ALBANY_ABORT(std::endl << "Invalid value of Target Mesh Snapshot Number = " << tgt_snap_no << "; value must be > 0." << std::endl);
  if (timestep_count > 0) {
    double time = io_region->get_state_time(step);
    if (step == timestep_count) interpolation_intervals = 1;

    int    step_end = step < timestep_count ? step + 1 : step;
    double tend     = io_region->get_state_time(step_end);
    double tbeg     = time;
    double delta    = (tend - tbeg) / static_cast<double>(interpolation_intervals);

    for (int interval = 0; interval < interpolation_intervals; interval++) {
      time = tbeg + delta * static_cast<double>(interval);
      tgt_broker.read_defined_input_fields(time);
    }
  }

  // SOLUTION TRANSFER SETUP
  // -----------------------

  // Create a manager for the source part elements.
  DataTransferKit::STKMeshManager src_manager(src_bulk_data, src_stk_selector);

  // Create a manager for the target part nodes.
  stk::mesh::Part*                tgt_part = tgt_broker.meta_data().get_part(target_mesh_part_name);
  stk::mesh::Selector             tgt_stk_selector(*tgt_part);
  DataTransferKit::STKMeshManager tgt_manager(tgt_bulk_data, tgt_stk_selector);

  // Create a solution vector for the source.
  Teuchos::RCP<Tpetra::MultiVector<double, int, DataTransferKit::SupportId>> src_vector =
      src_manager.createFieldMultiVector<FieldType>(Teuchos::ptr(source_field), neq);

  // Create a solution vector for the target.
  Teuchos::RCP<Tpetra::MultiVector<double, int, DataTransferKit::SupportId>> tgt_vector =
      tgt_manager.createFieldMultiVector<FieldType>(Teuchos::ptr(&target_interp_field), neq);

  // SOLUTION TRANSFER
  // -----------------

  // Create a map operator. The operator settings are in the
  // "DataTransferKit" parameter list.
  Teuchos::ParameterList&                    dtk_list = plist->sublist("DataTransferKit");
  DataTransferKit::MapOperatorFactory        op_factory;
  Teuchos::RCP<DataTransferKit::MapOperator> map_op = op_factory.create(src_vector->getMap(), tgt_vector->getMap(), dtk_list);

  // Setup the map operator. This creates the underlying linear operators.
  map_op->setup(src_manager.functionSpace(), tgt_manager.functionSpace());

  // Apply the map operator. This interpolates the data from one STK field
  // to the other.
  map_op->apply(*src_vector, *tgt_vector);

  double* tgt_field_data;
  // Copy interpolated solution on the Schwarz nodeset onto the target
  // solution's Schwarz nodeset
  double*                        gold_value;  // target_interp_field
  stk::mesh::BucketVector        tgt_part_buckets = tgt_stk_selector.get_buckets(stk::topology::NODE_RANK);
  std::vector<stk::mesh::Entity> tgt_part_nodes;
  stk::mesh::get_selected_entities(tgt_stk_selector, tgt_part_buckets, tgt_part_nodes);
  int num_tgt_part_nodes = tgt_part_nodes.size();  // number nodes (owned + overlap)

  for (int component = 0; component < neq; component++) {
    for (int n = 0; n < num_tgt_part_nodes; ++n) {
      gold_value                = stk::mesh::field_data(target_interp_field, tgt_part_nodes[n]);
      tgt_field_data            = stk::mesh::field_data(*target_field, tgt_part_nodes[n]);
      tgt_field_data[component] = gold_value[component];
    }
  }
  if (write_dirichlet_field) {
    // Copy tgt_field_data into dirichlet_field for output
    double*                        dirichlet_data;
    stk::mesh::Selector            tgt_all_stk_selector = stk::mesh::Selector(tgt_broker.meta_data().universal_part());
    stk::mesh::BucketVector        tgt_all_buckets      = tgt_all_stk_selector.get_buckets(stk::topology::NODE_RANK);
    std::vector<stk::mesh::Entity> tgt_all_nodes;
    stk::mesh::get_selected_entities(tgt_all_stk_selector, tgt_all_buckets, tgt_all_nodes);
    int num_tgt_all_nodes = tgt_all_nodes.size();  // number nodes (owned + overlap)
    for (int component = 0; component < neq; component++) {
      for (int n = 0; n < num_tgt_all_nodes; ++n) {
        tgt_field_data            = stk::mesh::field_data(*target_field, tgt_all_nodes[n]);
        dirichlet_data            = stk::mesh::field_data(dirichlet_field, tgt_all_nodes[n]);
        dirichlet_data[component] = tgt_field_data[component];
      }
    }
  }
  // TARGET MESH WRITE
  // -----------------
  std::size_t tgt_output_index = tgt_broker.create_output_mesh(target_mesh_output_file, stk::io::WRITE_RESULTS);
  // Uncomment the following if you want to write target_interp_field to the
  // exodus output mesh
  // tgt_broker.add_field( tgt_output_index, target_interp_field );
  if (write_dirichlet_field == false)
    tgt_broker.add_field(tgt_output_index, *target_field);
  else
    tgt_broker.add_field(tgt_output_index, dirichlet_field);
  tgt_broker.begin_output_step(tgt_output_index, 0.0);
  tgt_broker.write_defined_output_fields(tgt_output_index);
  tgt_broker.end_output_step(tgt_output_index);
}

namespace {

std::string
getFileExtension(std::string const& filename)
{
  auto const pos = filename.find_last_of(".");
  return filename.substr(pos + 1);
}

}  // anonymous namespace

int
main(int argc, char* argv[])
{
  // INITIALIZATION
  // --------------

  std::cout << "" << std::endl;

  // Setup communication.
  Teuchos::GlobalMPISession mpiSession(&argc, &argv);

  Teuchos::RCP<Teuchos::Comm<int> const> comm = Teuchos::DefaultComm<int>::getComm();

  // Read in command line options.
  std::string                   yaml_input_filename;
  Teuchos::CommandLineProcessor clp(false);
  clp.setOption("yaml-in-file", &yaml_input_filename, "The XML file to read into a parameter list");
  clp.parse(argc, argv);

  Teuchos::RCP<Teuchos::FancyOStream> out = Teuchos::fancyOStream(Teuchos::VerboseObjectBase::getDefaultOStream());

  // Build the parameter list from the yaml input.
  Teuchos::RCP<Teuchos::ParameterList> plist = Teuchos::rcp(new Teuchos::ParameterList());

  std::string const input_extension = getFileExtension(yaml_input_filename);
  if (input_extension == "yaml" || input_extension == "yml") {
    Teuchos::updateParametersFromYamlFile(yaml_input_filename, Teuchos::inoutArg(*plist));
  } else {
    Teuchos::updateParametersFromXmlFile(yaml_input_filename, Teuchos::inoutArg(*plist));
  }

  std::string field_type = plist->get<std::string>("Field Type", "Node Vector");
  int         field_type_num;
  if (field_type == "Node Vector")
    field_type_num = 0;
  else if (field_type == "Node Scalar")
    field_type_num = 1;
  else if (field_type == "Node Tensor")
    field_type_num = 2;
  else
    ALBANY_ABORT(
        std::endl
        << "Error in interpolation_volume_to_ns.cpp: invalid field_type = " << field_type
        << "!  Valid field_types are 'Node Vector', 'Node "
           "Scalar' and 'Node Tensor'."
        << std::endl);

  switch (field_type_num) {
    case 0:  // VectorFieldType
    {
      *out << " Interpolating fields of type Node Vector..." << std::endl;
      interpolate<stk::mesh::Field<double, stk::mesh::Cartesian>>(comm, plist);
      break;
    }
    case 1:  // ScalarFieldType
    {
      *out << " Interpolating fields of type Node Scalar..." << std::endl;
      interpolate<stk::mesh::Field<double>>(comm, plist);
      break;
    }
    case 2:  // TensorFieldType
    {
      *out << " Interpolating fields of type Node Scalar..." << std::endl;
      interpolate<stk::mesh::Field<double, shards::ArrayDimension>>(comm, plist);
      break;
    }
  }

  *out << " ...done!" << std::endl;

}  // end file interpolation_volume_to_ns.cpp
