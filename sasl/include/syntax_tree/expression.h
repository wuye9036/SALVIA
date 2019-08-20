#ifndef SASL_SYNTAX_TREE_EXPRESSION_H
#define SASL_SYNTAX_TREE_EXPRESSION_H

#include <sasl/include/syntax_tree/syntax_tree_fwd.h>
#include <sasl/include/syntax_tree/node.h>
#include <sasl/enums/operators.h>
#include <sasl/enums/literal_classifications.h>

#include <eflib/include/platform/boost_begin.h>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <eflib/include/platform/boost_end.h>

#include <string>
#include <vector>

namespace sasl {
	namespace common {
		struct token_t;
	}
}

BEGIN_NS_SASL_SYNTAX_TREE();

class operator_traits
{
public:
	bool is_prefix( operators op );
	bool is_binary( operators op );
	bool is_postfix( operators op );
	bool is_unary( operators op );
private:
	bool include( const std::vector<operators>&, operators );
	operator_traits();
	typedef ::std::vector<operators> oplist_t;
	oplist_t
		prefix_ops, postfix_ops, binary_ops;
};

using  sasl::common::token_t;
struct tynode;
class  syntax_tree_visitor;

struct expression: public node{
protected:
	expression( node_ids nodetype, boost::shared_ptr<token_t> const& tok_beg, boost::shared_ptr<token_t> const& tok_end );
};

struct constant_expression: public expression{
	SASL_SYNTAX_NODE_CREATORS();

	SASL_SYNTAX_NODE_ACCEPT_METHOD_DECL();

	boost::shared_ptr<token_t>	value_tok;
	literal_classifications		ctype;
protected:
	constant_expression( boost::shared_ptr<token_t> const& tok_beg, boost::shared_ptr<token_t> const& tok_end );
};

struct variable_expression: public expression{
	SASL_SYNTAX_NODE_CREATORS();

	SASL_SYNTAX_NODE_ACCEPT_METHOD_DECL();

	boost::shared_ptr< token_t > var_name;
protected:
	variable_expression( boost::shared_ptr<token_t> const& tok_beg, boost::shared_ptr<token_t> const& tok_end );
};

struct unary_expression: public expression{
	SASL_SYNTAX_NODE_CREATORS();

	SASL_SYNTAX_NODE_ACCEPT_METHOD_DECL();

	boost::shared_ptr<expression> expr;
	boost::shared_ptr<token_t> op_token;
	operators op;
protected:
	unary_expression( boost::shared_ptr<token_t> const& tok_beg, boost::shared_ptr<token_t> const& tok_end );
};

struct cast_expression: public expression{
	SASL_SYNTAX_NODE_CREATORS();

	SASL_SYNTAX_NODE_ACCEPT_METHOD_DECL();

	boost::shared_ptr<tynode>		casted_type;
	boost::shared_ptr<expression>	expr;
protected:
	cast_expression( boost::shared_ptr<token_t> const& tok_beg, boost::shared_ptr<token_t> const& tok_end );
};

struct binary_expression: public expression {
	SASL_SYNTAX_NODE_CREATORS();

	SASL_SYNTAX_NODE_ACCEPT_METHOD_DECL();

	operators op;
	boost::shared_ptr<token_t> op_token;
	boost::shared_ptr<expression> left_expr;
	boost::shared_ptr<expression> right_expr;
protected:
	binary_expression( boost::shared_ptr<token_t> const& tok_beg, boost::shared_ptr<token_t> const& tok_end );
};

struct expression_list: public expression{
	SASL_SYNTAX_NODE_CREATORS();

	SASL_SYNTAX_NODE_ACCEPT_METHOD_DECL();

	std::vector< boost::shared_ptr<expression> > exprs;
protected:
	expression_list( boost::shared_ptr<token_t> const& tok_beg, boost::shared_ptr<token_t> const& tok_end );
};

struct cond_expression: public expression{
	SASL_SYNTAX_NODE_CREATORS();

	SASL_SYNTAX_NODE_ACCEPT_METHOD_DECL();

	boost::shared_ptr<expression> cond_expr;
	boost::shared_ptr<expression> yes_expr;
	boost::shared_ptr<expression> no_expr;
protected:
	cond_expression( boost::shared_ptr<token_t> const& tok_beg, boost::shared_ptr<token_t> const& tok_end );
};

struct index_expression: public expression{
	SASL_SYNTAX_NODE_CREATORS();

	SASL_SYNTAX_NODE_ACCEPT_METHOD_DECL();

	boost::shared_ptr<expression> expr;
	boost::shared_ptr<expression> index_expr;
protected:
	index_expression( boost::shared_ptr<token_t> const& tok_beg, boost::shared_ptr<token_t> const& tok_end );
};

struct call_expression: public expression{
	SASL_SYNTAX_NODE_CREATORS();

	SASL_SYNTAX_NODE_ACCEPT_METHOD_DECL();

	boost::shared_ptr<expression>				expr;
	std::vector<boost::shared_ptr<expression> >	args;

protected:
	call_expression( boost::shared_ptr<token_t> const& tok_beg, boost::shared_ptr<token_t> const& tok_end );
};

struct member_expression: public expression{
	SASL_SYNTAX_NODE_CREATORS();

	SASL_SYNTAX_NODE_ACCEPT_METHOD_DECL();

	boost::shared_ptr<expression> expr;
	boost::shared_ptr<token_t> member;
protected:
	member_expression( boost::shared_ptr<token_t> const& tok_beg, boost::shared_ptr<token_t> const& tok_end );
};

END_NS_SASL_SYNTAX_TREE();

#endif //SASL_SYNTAX_TREE_EXPRESSION_H
