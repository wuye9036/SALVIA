#include <sasl/include/syntax_tree/expression.h>
#include <sasl/include/syntax_tree/visitor.h>

using std::shared_ptr;

BEGIN_NS_SASL_SYNTAX_TREE();

expression::expression( node_ids ntype, shared_ptr<token_t> const& tok_beg, shared_ptr<token_t> const& tok_end )
	: node( ntype, tok_beg, tok_end )
{
}

constant_expression::constant_expression( shared_ptr<token_t> const& tok_beg, shared_ptr<token_t> const& tok_end )
	: expression( node_ids::constant_expression, tok_beg, tok_end ), ctype( literal_classifications::none) { }

SASL_SYNTAX_NODE_ACCEPT_METHOD_IMPL( constant_expression );

unary_expression::unary_expression( shared_ptr<token_t> const& tok_beg, shared_ptr<token_t> const& tok_end )
	: expression( node_ids::unary_expression, tok_beg, tok_end ), op( operators::none ) { }

SASL_SYNTAX_NODE_ACCEPT_METHOD_IMPL( unary_expression );

cast_expression::cast_expression( shared_ptr<token_t> const& tok_beg, shared_ptr<token_t> const& tok_end )
	: expression( node_ids::cast_expression, tok_beg, tok_end ){
}

SASL_SYNTAX_NODE_ACCEPT_METHOD_IMPL( cast_expression );

binary_expression::binary_expression( shared_ptr<token_t> const& tok_beg, shared_ptr<token_t> const& tok_end )
	: expression( node_ids::binary_expression, tok_beg, tok_end ), op( operators::none) { }

SASL_SYNTAX_NODE_ACCEPT_METHOD_IMPL( binary_expression );

expression_list::expression_list( shared_ptr<token_t> const& tok_beg, shared_ptr<token_t> const& tok_end )
	: expression( node_ids::expression_list, tok_beg, tok_end )
{
}

SASL_SYNTAX_NODE_ACCEPT_METHOD_IMPL( expression_list );

cond_expression::cond_expression( shared_ptr<token_t> const& tok_beg, shared_ptr<token_t> const& tok_end )
	: expression( node_ids::cond_expression, tok_beg, tok_end ){
}

SASL_SYNTAX_NODE_ACCEPT_METHOD_IMPL( cond_expression );

index_expression::index_expression( shared_ptr<token_t> const& tok_beg, shared_ptr<token_t> const& tok_end )
	: expression( node_ids::index_expression, tok_beg, tok_end ){
}

SASL_SYNTAX_NODE_ACCEPT_METHOD_IMPL( index_expression );

call_expression::call_expression( shared_ptr<token_t> const& tok_beg, shared_ptr<token_t> const& tok_end )
	: expression( node_ids::call_expression, tok_beg, tok_end ){
}

SASL_SYNTAX_NODE_ACCEPT_METHOD_IMPL( call_expression );

member_expression::member_expression( shared_ptr<token_t> const& tok_beg, shared_ptr<token_t> const& tok_end )
	: expression( node_ids::member_expression, tok_beg, tok_end )
{
}

SASL_SYNTAX_NODE_ACCEPT_METHOD_IMPL( member_expression );

variable_expression::variable_expression( shared_ptr<token_t> const& tok_beg, shared_ptr<token_t> const& tok_end )
: expression( node_ids::variable_expression, tok_beg, tok_end )
{	
}

SASL_SYNTAX_NODE_ACCEPT_METHOD_IMPL( variable_expression );

operator_traits::operator_traits()
{
    prefix_ops = decltype(prefix_ops) 
        {
            operators::positive, operators::negative,
            operators::bit_not, operators::logic_not,
            operators::prefix_incr, operators::prefix_decr
        };

	postfix_ops = decltype(postfix_ops)
        {
		    operators::postfix_incr,
            operators::postfix_decr
        };

	binary_ops = decltype(binary_ops)
        {
		    operators::add, operators::sub,
		    operators::mul, operators::div, operators::mod,
		    operators::assign,
		    operators::add_assign, operators::sub_assign,
		    operators::mul_assign, operators::div_assign, operators::mod_assign,
		    operators::bit_and, operators::bit_or, operators::bit_xor,
		    operators::bit_and_assign, operators::bit_or_assign, operators::bit_xor_assign,
		    operators::logic_and, operators::logic_or,
		    operators::equal, operators::not_equal,
		    operators::greater, operators::greater_equal,
		    operators::less, operators::less_equal,
		    operators::left_shift, operators::right_shift,
		    operators::lshift_assign, operators::rshift_assign
        };

}

bool operator_traits::is_prefix( operators op )
{
	return include( prefix_ops, op );
}

bool operator_traits::is_binary( operators op )
{
	return include( binary_ops, op );
}

bool operator_traits::is_postfix( operators op )
{
	return include( postfix_ops, op );
}

bool operator_traits::is_unary( operators op )
{
	return is_prefix(op) || is_postfix(op);
}

bool operator_traits::include( const std::vector<operators>& c, operators op)
{
	return std::find( c.begin(), c.end(), op) != c.end();
}

END_NS_SASL_SYNTAX_TREE()
