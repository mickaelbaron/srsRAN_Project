/*
 *
 * Copyright 2021-2023 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#pragma once

#include "resource_grid.h"
#include "resource_grid_context.h"
#include <vector>

namespace srsgnb {

/// Describes a resource grid pool interface
class resource_grid_pool
{
public:
  /// Default destructor
  virtual ~resource_grid_pool() = default;

  /// Get a resource grid for the given context
  /// \param [in] context Provides the given context
  virtual resource_grid& get_resource_grid(const resource_grid_context& context) = 0;
};

} // namespace srsgnb
