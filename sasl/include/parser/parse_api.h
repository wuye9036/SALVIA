#ifndef SASL_PARSER_PARSE_API_H
#define SASL_PARSER_PARSE_API_H

#include <sasl/include/parser/parser_forward.h>

#include <string>
#include <memory>

namespace sasl{
	namespace common{
		class lex_context;
		class code_source;
		class diag_chat;
	}
	namespace parser{
		class attribute;
		class lexer;
		class grammars;

		bool parse(
			std::shared_ptr<attribute>& pt_root,
			const std::string& code,
			std::shared_ptr< ::sasl::common::lex_context > ctxt,
			lexer& l, grammars& g,
			sasl::common::diag_chat* diags
			);

		bool parse(
			std::shared_ptr<attribute>& pt_root,
			sasl::common::code_source* src,
			std::shared_ptr< ::sasl::common::lex_context > ctxt,
			lexer& l, grammars& g,
			sasl::common::diag_chat* diags
			);
	}
}

#endif