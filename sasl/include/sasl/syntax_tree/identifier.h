#pragma once

#include <sasl/syntax_tree/syntax_tree_fwd.h>

#include <sasl/syntax_tree/node.h>
#include <string>

namespace sasl::syntax_tree {

using sasl::common::token;

struct identifier : public node_impl<identifier, node, node_ids::identifier> {
  std::string_view name;
};

}  // namespace sasl::syntax_tree
