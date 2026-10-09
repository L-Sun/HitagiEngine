#pragma once
#include <pxr/pxr.h>

#if defined(_MSC_VER) && !defined(__clang__)
// Keep both overloads in the template definition's ordinary lookup set.
// MSVC otherwise loses the Sdf overload when importing TfDelegatedCountPtr.
PXR_NAMESPACE_OPEN_SCOPE
// Imported inline functions can odr-use these tags. Give this bridge's copies
// external linkage so MSVC emits the symbols required by its importers.
struct TfDelegatedCountIncrementTagType;
struct TfDelegatedCountDoNotIncrementTagType;
extern const TfDelegatedCountIncrementTagType      TfDelegatedCountIncrementTag;
extern const TfDelegatedCountDoNotIncrementTagType TfDelegatedCountDoNotIncrementTag;

class Sdf_PathNode;
class Usd_PrimData;
void TfDelegatedCountIncrement(const Sdf_PathNode*) noexcept;
void TfDelegatedCountDecrement(const Sdf_PathNode*) noexcept;
void TfDelegatedCountIncrement(const Usd_PrimData*) noexcept;
void TfDelegatedCountDecrement(const Usd_PrimData*) noexcept;
PXR_NAMESPACE_CLOSE_SCOPE
#endif
