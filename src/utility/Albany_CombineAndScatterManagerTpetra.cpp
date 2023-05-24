#include "Albany_CombineAndScatterManagerTpetra.hpp"

#include "Albany_Macros.hpp"
#include "Albany_ThyraUtils.hpp"
#include "Albany_TpetraThyraUtils.hpp"

namespace {
Tpetra::CombineMode
combineModeT(const Albany::CombineMode modeA)
{
  Tpetra::CombineMode modeT;
  switch (modeA) {
    case Albany::CombineMode::ADD: modeT = Tpetra::CombineMode::ADD; break;
    case Albany::CombineMode::INSERT: modeT = Tpetra::CombineMode::INSERT; break;
    case Albany::CombineMode::ZERO: modeT = Tpetra::CombineMode::ZERO; break;
    case Albany::CombineMode::ABSMAX: modeT = Tpetra::CombineMode::ABSMAX; break;
    default: ALBANY_ABORT("Error! Unknown Albany combine mode. Please, contact developers.\n");
  }
  return modeT;
}

}  // anonymous namespace

namespace Albany {

CombineAndScatterManagerTpetra::CombineAndScatterManagerTpetra(
    Teuchos::RCP<Thyra_VectorSpace const> const& owned,
    Teuchos::RCP<Thyra_VectorSpace const> const& overlapped)
    : CombineAndScatterManager(owned, overlapped)
{
  auto ownedT      = Albany::getTpetraMap(owned);
  auto overlappedT = Albany::getTpetraMap(overlapped);

  importer = Teuchos::rcp(new Tpetra_Import(ownedT, overlappedT));
}

void
CombineAndScatterManagerTpetra::combine(Thyra_Vector const& src, Thyra_Vector& dst, const CombineMode CM) const
{
  auto cmT  = combineModeT(CM);
  auto srcT = Albany::getConstTpetraVector(src);
  auto dstT = Albany::getTpetraVector(dst);

  dstT->doExport(*srcT, *importer, cmT);
}

void
CombineAndScatterManagerTpetra::combine(const Thyra_MultiVector& src, Thyra_MultiVector& dst, const CombineMode CM) const
{
  // There's a catch here!
  // Legend: V = Vector, MV = MultiVector, TV = Tpetra_Vector, TMV =
  // Tpetra_MultiVector, T_xyz = Thyra_xyz One can create a T_TV, then pass it
  // to routines expecting a T_MV, since T_TV inherits from T_V, which inherits
  // from T_MV. However, T_TV does NOT inherit from T_TMV, so such routines
  // would try to cast the input to T_TMV and fail. This would be solved if T_TV
  // also inherited from T_TMV, but that's hard to do (without code
  // duplication), since T_T(M)V store ConstNonConstObj containers to the Tpetra
  // objects, which I _think_ do not support polymorphism. So, given what we
  // have, we _try_ to extract a TMV from the T_MV, and, if we fail, we try
  // again, this time extracting a TV. If we still fail, then we can error out.
  Teuchos::RCP<const Tpetra_MultiVector> srcT = Albany::getConstTpetraMultiVector(src, false);
  Teuchos::RCP<Tpetra_MultiVector>       dstT = Albany::getTpetraMultiVector(dst, false);

  if (srcT.is_null()) {
    // Try to cast to Thyra_Vector, then extract the Tpetra_Vector
    Thyra_Vector const* srcV = dynamic_cast<Thyra_Vector const*>(&src);

    ALBANY_PANIC(
        srcV == nullptr,
        "Error! Input src does not seem to be a Tpetra_MultiVector or a "
        "Tpetra_Vector.\n");

    // This time throw if extraction fails
    srcT = Albany::getConstTpetraVector(*srcV);
  }

  if (dstT.is_null()) {
    // Try to cast to Thyra_Vector, then extract the Tpetra_Vector
    Thyra_Vector* dstV = dynamic_cast<Thyra_Vector*>(&dst);

    ALBANY_PANIC(
        dstV == nullptr,
        "Error! Input dst does not seem to be a Tpetra_MultiVector or a "
        "Tpetra_Vector.\n");

    // This time throw if extraction fails
    dstT = Albany::getTpetraVector(*dstV);
  }

  auto cmT = combineModeT(CM);
  dstT->doExport(*srcT, *importer, cmT);
}

void
CombineAndScatterManagerTpetra::combine(const Thyra_LinearOp& src, Thyra_LinearOp& dst, const CombineMode CM) const
{
  auto cmT  = combineModeT(CM);
  auto srcT = Albany::getConstTpetraMatrix(src);
  auto dstT = Albany::getTpetraMatrix(dst);

  dstT->doExport(*srcT, *importer, cmT);
}

void
CombineAndScatterManagerTpetra::combine(Teuchos::RCP<Thyra_Vector const> const& src, Teuchos::RCP<Thyra_Vector> const& dst, const CombineMode CM) const
{
  auto cmT  = combineModeT(CM);
  auto srcT = Albany::getConstTpetraVector(src);
  auto dstT = Albany::getTpetraVector(dst);

  dstT->doExport(*srcT, *importer, cmT);
}

void
CombineAndScatterManagerTpetra::combine(const Teuchos::RCP<const Thyra_MultiVector>& src, Teuchos::RCP<Thyra_MultiVector> const& dst, const CombineMode CM)
    const
{
  // There's a catch here!
  // Legend: V = Vector, MV = MultiVector, TV = Tpetra_Vector, TMV =
  // Tpetra_MultiVector, T_xyz = Thyra_xyz One can create a T_TV, then pass it
  // to routines expecting a T_MV, since T_TV inherits from T_V, which inherits
  // from T_MV. However, T_TV does NOT inherit from T_TMV, so such routines
  // would try to cast the input to T_TMV and fail. This would be solved if T_TV
  // also inherited from T_TMV, but that's hard to do (without code
  // duplication), since T_T(M)V store ConstNonConstObj containers to the Tpetra
  // objects, which I _think_ do not support polymorphism. So, given what we
  // have, we _try_ to extract a TMV from the T_MV, and, if we fail, we try
  // again, this time extracting a TV. If we still fail, then we can error out.
  Teuchos::RCP<const Tpetra_MultiVector> srcT = Albany::getConstTpetraMultiVector(src, false);
  Teuchos::RCP<Tpetra_MultiVector>       dstT = Albany::getTpetraMultiVector(dst, false);

  if (srcT.is_null()) {
    // Try to cast to Thyra_Vector, then extract the Tpetra_Vector
    Teuchos::RCP<Thyra_Vector const> srcV = Teuchos::rcp_dynamic_cast<Thyra_Vector const>(src);

    ALBANY_PANIC(
        srcV.is_null(),
        "Error! Input src does not seem to be a Tpetra_MultiVector or a "
        "Tpetra_Vector.\n");

    // This time throw if extraction fails
    srcT = Albany::getConstTpetraVector(srcV);
  }

  if (dstT.is_null()) {
    // Try to cast to Thyra_Vector, then extract the Tpetra_Vector
    Teuchos::RCP<Thyra_Vector> dstV = Teuchos::rcp_dynamic_cast<Thyra_Vector>(dst);

    ALBANY_PANIC(
        dstV.is_null(),
        "Error! Input dst does not seem to be a Tpetra_MultiVector or a "
        "Tpetra_Vector.\n");

    // This time throw if extraction fails
    dstT = Albany::getTpetraVector(dstV);
  }

  auto cmT = combineModeT(CM);
  dstT->doExport(*srcT, *importer, cmT);
}

void
CombineAndScatterManagerTpetra::combine(const Teuchos::RCP<const Thyra_LinearOp>& src, const Teuchos::RCP<Thyra_LinearOp>& dst, const CombineMode CM) const
{
  auto cmT  = combineModeT(CM);
  auto srcT = Albany::getConstTpetraMatrix(src);
  auto dstT = Albany::getTpetraMatrix(dst);

  dstT->doExport(*srcT, *importer, cmT);
}

// Scatter methods
void
CombineAndScatterManagerTpetra::scatter(Thyra_Vector const& src, Thyra_Vector& dst, const CombineMode CM) const
{
  auto cmT  = combineModeT(CM);
  auto srcT = Albany::getConstTpetraVector(src);
  auto dstT = Albany::getTpetraVector(dst);

  dstT->doImport(*srcT, *importer, cmT);
}

void
CombineAndScatterManagerTpetra::scatter(const Thyra_MultiVector& src, Thyra_MultiVector& dst, const CombineMode CM) const
{
  // There's a catch here!
  // Legend: V = Vector, MV = MultiVector, TV = Tpetra_Vector, TMV =
  // Tpetra_MultiVector, T_xyz = Thyra_xyz One can create a T_TV, then pass it
  // to routines expecting a T_MV, since T_TV inherits from T_V, which inherits
  // from T_MV. However, T_TV does NOT inherit from T_TMV, so such routines
  // would try to cast the input to T_TMV and fail. This would be solved if T_TV
  // also inherited from T_TMV, but that's hard to do (without code
  // duplication), since T_T(M)V store ConstNonConstObj containers to the Tpetra
  // objects, which I _think_ do not support polymorphism. So, given what we
  // have, we _try_ to extract a TMV from the T_MV, and, if we fail, we try
  // again, this time extracting a TV. If we still fail, then we can error out.
  Teuchos::RCP<const Tpetra_MultiVector> srcT = Albany::getConstTpetraMultiVector(src, false);
  Teuchos::RCP<Tpetra_MultiVector>       dstT = Albany::getTpetraMultiVector(dst, false);

  if (srcT.is_null()) {
    // Try to cast to Thyra_Vector, then extract the Tpetra_Vector
    Thyra_Vector const* srcV = dynamic_cast<Thyra_Vector const*>(&src);

    ALBANY_PANIC(
        srcV == nullptr,
        "Error! Input src does not seem to be a Tpetra_MultiVector or a "
        "Tpetra_Vector.\n");

    // This time throw if extraction fails
    srcT = Albany::getConstTpetraVector(*srcV);
  }

  if (dstT.is_null()) {
    // Try to cast to Thyra_Vector, then extract the Tpetra_Vector
    Thyra_Vector* dstV = dynamic_cast<Thyra_Vector*>(&dst);

    ALBANY_PANIC(
        dstV == nullptr,
        "Error! Input dst does not seem to be a Tpetra_MultiVector or a "
        "Tpetra_Vector.\n");

    // This time throw if extraction fails
    dstT = Albany::getTpetraVector(*dstV);
  }

  auto cmT = combineModeT(CM);
  dstT->doImport(*srcT, *importer, cmT);
}

void
CombineAndScatterManagerTpetra::scatter(const Thyra_LinearOp& src, Thyra_LinearOp& dst, const CombineMode CM) const
{
  auto cmT  = combineModeT(CM);
  auto srcT = Albany::getConstTpetraMatrix(src);
  auto dstT = Albany::getTpetraMatrix(dst);

  dstT->doImport(*srcT, *importer, cmT);
}

void
CombineAndScatterManagerTpetra::scatter(Teuchos::RCP<Thyra_Vector const> const& src, Teuchos::RCP<Thyra_Vector> const& dst, const CombineMode CM) const
{
  auto cmT  = combineModeT(CM);
  auto srcT = Albany::getConstTpetraVector(src);
  auto dstT = Albany::getTpetraVector(dst);

  dstT->doImport(*srcT, *importer, cmT);
}

void
CombineAndScatterManagerTpetra::scatter(const Teuchos::RCP<const Thyra_MultiVector>& src, Teuchos::RCP<Thyra_MultiVector> const& dst, const CombineMode CM)
    const
{
  // There's a catch here!
  // Legend: V = Vector, MV = MultiVector, TV = Tpetra_Vector, TMV =
  // Tpetra_MultiVector, T_xyz = Thyra_xyz One can create a T_TV, then pass it
  // to routines expecting a T_MV, since T_TV inherits from T_V, which inherits
  // from T_MV. However, T_TV does NOT inherit from T_TMV, so such routines
  // would try to cast the input to T_TMV and fail. This would be solved if T_TV
  // also inherited from T_TMV, but that's hard to do (without code
  // duplication), since T_T(M)V store ConstNonConstObj containers to the Tpetra
  // objects, which I _think_ do not support polymorphism. So, given what we
  // have, we _try_ to extract a TMV from the T_MV, and, if we fail, we try
  // again, this time extracting a TV. If we still fail, then we can error out.
  Teuchos::RCP<const Tpetra_MultiVector> srcT = Albany::getConstTpetraMultiVector(src, false);
  Teuchos::RCP<Tpetra_MultiVector>       dstT = Albany::getTpetraMultiVector(dst, false);

  if (srcT.is_null()) {
    // Try to cast to Thyra_Vector, then extract the Tpetra_Vector
    Teuchos::RCP<Thyra_Vector const> srcV = Teuchos::rcp_dynamic_cast<Thyra_Vector const>(src);

    ALBANY_PANIC(
        srcV.is_null(),
        "Error! Input src does not seem to be a Tpetra_MultiVector or a "
        "Tpetra_Vector.\n");

    // This time throw if extraction fails
    srcT = Albany::getConstTpetraVector(srcV);
  }

  if (dstT.is_null()) {
    // Try to cast to Thyra_Vector, then extract the Tpetra_Vector
    Teuchos::RCP<Thyra_Vector> dstV = Teuchos::rcp_dynamic_cast<Thyra_Vector>(dst);

    ALBANY_PANIC(
        dstV.is_null(),
        "Error! Input dst does not seem to be a Tpetra_MultiVector or a "
        "Tpetra_Vector.\n");

    // This time throw if extraction fails
    dstT = Albany::getTpetraVector(dstV);
  }

  auto cmT = combineModeT(CM);
  dstT->doImport(*srcT, *importer, cmT);
}

void
CombineAndScatterManagerTpetra::scatter(const Teuchos::RCP<const Thyra_LinearOp>& src, const Teuchos::RCP<Thyra_LinearOp>& dst, const CombineMode CM) const
{
  auto cmT  = combineModeT(CM);
  auto srcT = Albany::getConstTpetraMatrix(src);
  auto dstT = Albany::getTpetraMatrix(dst);

  dstT->doImport(*srcT, *importer, cmT);
}

void
CombineAndScatterManagerTpetra::create_ghosted_aura_owners() const
{
  // Use the getter, so it creates the vs is if it's null
  auto ga_vs = getGhostedAuraVectorSpace();

  // Get the gids in the ghosted vs
  auto               gids  = getGlobalElements(ga_vs);
  auto               tgids = Teuchos::arrayView(reinterpret_cast<Tpetra_GO*>(gids.getRawPtr()), gids.size());
  Teuchos::Array<LO> lids(gids.size());
  ghosted_aura_owners.resize(lids.size());

  // Ask the owned map the pids that own the gids
  auto map = getTpetraMap(getOwnedAuraVectorSpace());
  map->getRemoteIndexList(tgids(), ghosted_aura_owners(), lids());
}

void
CombineAndScatterManagerTpetra::create_owned_aura_users() const
{
  // Get the pid to which each of the exported pids goes
  // Note: we can use the importer we already created for this
  auto pids = importer->getExportPIDs();
  auto lids = importer->getExportLIDs();

  owned_aura_users.resize(lids.size());
  auto owned_map = getTpetraMap(owned_vs);
  for (int i = 0; i < lids.size(); ++i) {
    owned_aura_users[i].first  = owned_map->getGlobalElement(lids[i]);
    owned_aura_users[i].second = pids[i];
  }
}

}  // namespace Albany
