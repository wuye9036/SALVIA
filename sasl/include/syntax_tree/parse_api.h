#ifndef SASL_SYNTAX_TREE_PARSE_API_H
#define SASL_SYNTAX_TREE_PARSE_API_H

#include <sasl/include/syntax_tree/syntax_tree_fwd.h>

#include <memory>
#include <string>

namespace sasl{
	namespace common{
		class lex_context;
		class code_source;
		class diag_chat;
	}
}

BEGIN_NS_SASL_SYNTAX_TREE();

struct program;

std::shared_ptr<program> parse(
	const std::string& code_text,
	std::shared_ptr<sasl::common::lex_context> ctxt,
	sasl::common::diag_chat* diags
	);

std::shared_ptr<program> parse(
	sasl::common::code_source* src,
	std::shared_ptr<sasl::common::lex_context> ctxt,
	sasl::common::diag_chat* diags
	);

END_NS_SASL_SYNTAX_TREE();

#endif