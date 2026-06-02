#pragma once

// The Haiku shell implementation lives in hemera::haiku but works primarily
// with core model types declared in namespace hermes. Import that namespace
// once here so lookup does not depend on header include order.
namespace hermes {}

namespace hemera::haiku {

using namespace hermes;

}  // namespace hemera::haiku
