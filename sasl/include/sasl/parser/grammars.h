#pragma once

#include <sasl/parser/generator.h>

namespace sasl::parser {

class lexer;
class grammars {
public:
  grammars(lexer& lxr);
  rule
      /* Program */
      prog,

      /* Declarations */
      decl, basic_decl, function_full_def, vardecl, function_decl, struct_decl, typedef_decl, param,
      function_body, decllist, init_declarator, sem, anno, named_struct_body, struct_body,

      /* Type specifiers */
      declspec, postqualed_type, prequaled_type, unqualed_type, postfix_typequal, prefix_typequal,
      func_typequal, array_typequal, param_typequal, access_typequal,

      /* Expressions */
      expr, exprlst, assignexpr, rhsexpr, condexpr, lorexpr, landexpr, borexpr, bandexpr, bxorexpr,
      eqlexpr, relexpr, addexpr, mulexpr, shfexpr, castexpr, typecastedexpr, unaryexpr, unariedexpr,
      postexpr, idxexpr, callexpr, memexpr, pmexpr, parenexpr,

      /* Initializers */
      init, c_style_init, paren_init, nullable_initlist, init_list,

      /* Statements */
      stmt, stmt_if, stmt_while, stmt_dowhile, stmt_for, stmt_switch, stmt_expr, stmt_decl,
      stmt_compound, stmt_flowctrl, stmt_break, stmt_continue, stmt_return, labeled_stmt,
      for_init_decl, for_looper,

      /* Terminators */
      lit_const, lit_int, lit_float, lit_bool,

      opadd, opmul, opshift, oprel, opequal, opband, opbxor, opbor, opland, oplor, opassign,
      opmember,

      lparen, rparen, lsbracket, rsbracket, labracket, rabracket, lbrace, rbrace, opinc, opunary,

      kw_typedef, kw_struct, kw_uniform, kw_const, kw_shared, kw_break, kw_continue, kw_return,
      kw_switch, kw_case, kw_default, kw_if, kw_else, kw_for, kw_do, kw_while,

      question, colon, comma, semicolon, equal,

      ident, eof;

private:
  grammars& operator=(grammars const&);
  void set_prog();
  void set_decls();
  void set_exprs();
  void set_typespecs();
  void set_inits();
  void set_stmts();
  void set_terms();

  lexer& lxr;
};

}  // namespace sasl::parser
