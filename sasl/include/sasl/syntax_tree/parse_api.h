#pragma once

#include <sasl/syntax_tree/syntax_tree_fwd.h>

#include <memory>
#include <string>

namespace sasl {
namespace common {
class lex_context;
class code_source;
class diag_chat;
}  // namespace common
}  // namespace sasl

namespace sasl::syntax_tree {

struct program;

std::shared_ptr<program> parse(const std::string& code_text,
                               std::shared_ptr<sasl::common::lex_context> ctxt,
                               sasl::common::diag_chat* diags);

std::shared_ptr<program> parse(sasl::common::code_source* src,
                               std::shared_ptr<sasl::common::lex_context> ctxt,
                               sasl::common::diag_chat* diags);

}  // namespace sasl::syntax_tree