#include <sasl/enums/enums_utility.h>
#include <sasl/enums/builtin_types.h>
#include <sasl/enums/operators.h>

#include <eflib/include/math/math.h>
#include <eflib/include/utility/enum.h>

#include <mutex>

namespace sasl{
	namespace utility{
		bool is_none( const builtin_types& btc ){
			return btc == builtin_types::none;
		}

		bool is_void( const builtin_types& btc ){
			return btc == builtin_types::_void;
		}

		bool is_sampler( const builtin_types& btc ){
			return btc == builtin_types::_sampler;
		}

		bool is_integer( const builtin_types& btc )
		{
			return ( btc & builtin_types::_generic_type_mask ) == builtin_types::_integer;
		}

		bool is_real( const builtin_types& btc )
		{
			return ( btc & builtin_types::_generic_type_mask ) == builtin_types::_real;
		}

		bool is_signed( const builtin_types& btc )
		{
			return ( btc & builtin_types::_sign_mask ) == builtin_types::_signed;
		}

		bool is_unsigned( const builtin_types& btc )
		{
			return ( btc & builtin_types::_sign_mask ) == builtin_types::_unsigned;
		}

		bool is_scalar( const builtin_types& btc )
		{
			bool scalar = ( ( btc & builtin_types::_dimension_mask ) == builtin_types::_scalar );
			return scalar && !is_void(btc) && !is_none(btc) && !is_sampler(btc) ;
		}

		bool is_numeric( builtin_types const& btc )
		{
			return is_integer(btc) || is_real(btc);
		}

		bool is_numeric_scalar( builtin_types const& btc )
		{
			return is_scalar(btc) && is_numeric(btc);
		}

		bool is_vector( const builtin_types& btc )
		{
			return ( btc & builtin_types::_dimension_mask ) == builtin_types::_vector;
		}

		bool is_matrix( const builtin_types& btc )
		{
			return ( btc & builtin_types::_dimension_mask ) == builtin_types::_matrix;
		}

		bool is_storagable( const builtin_types& btc ){
			return 
				is_scalar(btc) || is_vector( btc ) || is_matrix( btc )
				;
		}

		builtin_types scalar_of( const builtin_types& btc ){
			return ( btc & builtin_types::_scalar_type_mask );
		}

		builtin_types vector_of( const builtin_types& btc, size_t len )
		{
			if ( !is_scalar(btc) )
			{
				return builtin_types::none;
			}
			builtin_types ret = (btc | builtin_types::_vector) | static_cast<builtin_types>(len << builtin_types::_dim0_field_shift);
			return ret;
		}

		builtin_types matrix_of( const builtin_types& btc, size_t vec_size, size_t vec_cnt )
		{
			if ( !is_scalar(btc) )
			{
				return builtin_types::none;
			}
			builtin_types ret = 
				(btc | builtin_types::_matrix)
				| static_cast<builtin_types>(vec_size << builtin_types::_dim0_field_shift)
				| static_cast<builtin_types>(vec_cnt << builtin_types::_dim1_field_shift);

			return ret;
		}

		builtin_types row_vector_of( const builtin_types& btc )
		{
			if( !is_matrix(btc) ){
				return builtin_types::none;
			}
			
			return vector_of( scalar_of(btc), vector_size(btc) );
		}

		size_t vector_size( const builtin_types& btc )
		{
			if( is_sampler(btc) || is_scalar(btc) )
			{
				return 1;
			}
			return static_cast<size_t>(btc & builtin_types::_dim0_mask) >> builtin_types::_dim0_field_shift;
		}

		size_t vector_count( const builtin_types& btc )
		{
			if( is_sampler(btc) || is_scalar(btc) || is_vector(btc) )
			{
				return 1;
			}
			return static_cast<size_t>(btc & builtin_types::_dim1_mask) >> builtin_types::_dim1_field_shift;
		}

		size_t reg_storage_size(builtin_types const& btc)
		{
			if( is_none(btc) || is_void(btc) )
			{
				return 0;
			}

			builtin_types s_btc = scalar_of( btc );

			size_t comp_size = 0;

			switch(s_btc)
			{
			case builtin_types::_sint32:
			case builtin_types::_uint32:
			case builtin_types::_float:
			case builtin_types::_boolean:
				comp_size = 4;
				break;
			case builtin_types::_sampler:
				comp_size = sizeof(void*);
				break;
			default:
				assert(false);
			}

			if( is_matrix(btc) )
			{
				return vector_count(btc) * comp_size * 4;
			}
			else if( is_vector(btc) )
			{
				return vector_size(btc) * comp_size;
			}
			return comp_size;
		}

		size_t storage_size( const builtin_types& btc ){
			if( is_none(btc) || is_void(btc) )
			{
				return 0;
			}
			if( is_sampler(btc) )
			{
				return sizeof(void*);
			}
			size_t component_count = vector_size(btc) * vector_count(btc);
			size_t component_size = 0;
			builtin_types s_btc = scalar_of( btc );
			if(	   s_btc == builtin_types::_sint8 
				|| s_btc == builtin_types::_uint8
				|| s_btc == builtin_types::_boolean )
			{
				component_size = 1;
			}
			else if(s_btc == builtin_types::_sint16 || s_btc == builtin_types::_uint16)
			{
				component_size = 2;
			}
			else if( s_btc == builtin_types::_sint32 || s_btc == builtin_types::_uint32 || s_btc == builtin_types::_float || s_btc == builtin_types::_boolean)
			{
				component_size = 4;
			} 
			else if( s_btc == builtin_types::_sint64 || s_btc == builtin_types::_uint64 || s_btc == builtin_types::_double)
			{
				component_size = 8;
			}

			return component_size * component_count;
		}
        
		std::mutex mtx_btlist_init;
		std::vector<builtin_types> btc_list;

		const std::vector<builtin_types>& list_of_builtin_types(){
			std::scoped_lock<std::mutex> locker(mtx_btlist_init);
			if( btc_list.empty() ){
				// add scalars.
                btc_list = decltype(btc_list)
                {
                    builtin_types::_sint8,
                    builtin_types::_sint16,
                    builtin_types::_sint32,
                    builtin_types::_sint64,
                    builtin_types::_uint8,
                    builtin_types::_uint16,
                    builtin_types::_uint32,
                    builtin_types::_uint64,
                    builtin_types::_boolean,
                    builtin_types::_float,
                    builtin_types::_double
                };

				// add vectors & matrixs
				size_t scalar_count = btc_list.size();
				for( size_t i_scalar = 0; i_scalar < scalar_count; ++i_scalar){
					for( int i = 1; i <= 4; ++i ){
						for( int j = 1; j <=4; ++j ){
							btc_list.push_back(matrix_of(btc_list[i_scalar], i, j));
						}
						btc_list.push_back(vector_of(btc_list[i_scalar], i));
					}
				}

				// add other types.
                btc_list.push_back(builtin_types::none);
                btc_list.push_back(builtin_types::_void);
                btc_list.push_back(builtin_types::_sampler);
			}
			return btc_list;
		}

		bool is_arithmetic( const operators& op ){
			return 
				op == operators::add ||
				op == operators::sub ||
				op == operators::mul ||
				op == operators::div ||
				op == operators::mod
				;
		}

		bool is_relationship( const operators& op ){
			return 
				op == operators::greater ||
				op == operators::greater_equal ||
				op == operators::equal ||
				op == operators::less ||
				op == operators::less_equal ||
				op == operators::not_equal
				;
		}

		bool is_bit( const operators& op ){
			return
				op == operators::bit_and ||
				op == operators::bit_or ||
				op == operators::bit_xor
				;
		}

		bool is_shift( const operators& op ){
			return
				op == operators::left_shift ||
				op == operators::right_shift
				;
		}

		bool is_bool_arith( const operators& op ){
			return
				op == operators::logic_and ||
				op == operators::logic_or
				;
		}

		bool is_prefix( const operators& op ){
			return
				op == operators::prefix_decr ||
				op == operators::prefix_incr
				;
		}

		bool is_postfix( const operators& op ){
			return
				op == operators::postfix_decr ||
				op == operators::postfix_incr
				;
		}

		bool is_unary_arith( const operators& op ){
			return
				op == operators::positive ||
				op == operators::negative
				;
		}

		bool is_arith_assign( const operators& op ){
			return
				op == operators::add_assign ||
				op == operators::sub_assign ||
				op == operators::mul_assign ||
				op == operators::div_assign ||
				op == operators::mod_assign
				;
		}

		bool is_bit_assign( const operators& op ){
			return 
				op == operators::bit_and_assign ||
				op == operators::bit_or_assign ||
				op == operators::bit_xor_assign
				;
		}

		bool is_shift_assign( const operators& op ){
			return
				op == operators::lshift_assign ||
				op == operators::rshift_assign
				;
		}

		bool is_assign( const operators& op ){
			return op == operators::assign;
		}

		bool is_general_assign(operators const& op)
		{
			return is_assign(op) 
				|| is_arith_assign(op)
				|| is_bit_assign(op)
				|| is_shift_assign(op);
		}

		std::mutex mtx_oplist_init;
		std::vector<operators> op_list;

		const std::vector<operators>& list_of_operators(){
			std::scoped_lock<std::mutex> locker(mtx_oplist_init);
			if ( op_list.empty() ){
                op_list = decltype(op_list)
                {
                    operators::add, operators::add_assign,
                    operators::assign,
                    operators::bit_and, operators::bit_and_assign,
                    operators::bit_not,
                    operators::bit_or, operators::bit_or_assign,
                    operators::bit_xor, operators::bit_xor_assign,
                    operators::div, operators::div_assign,
                    operators::equal, operators::greater, operators::greater_equal,
                    operators::left_shift,
                    operators::less, operators::less_equal,
                    operators::logic_and, operators::logic_or, operators::logic_not,
                    operators::lshift_assign,
                    operators::mod, operators::mod_assign, operators::mul, operators::mul_assign,
                    operators::negative,
                    operators::none,
                    operators::not_equal,
                    operators::positive,
                    operators::postfix_decr, operators::postfix_incr,
                    operators::prefix_decr, operators::prefix_incr,
                    operators::right_shift, operators::rshift_assign,
                    operators::sub, operators::sub_assign
                };
			}
			return op_list;
		}

		bool is_standard( const builtin_types& btc ){
			if (btc == builtin_types::_sint32 || 
				btc == builtin_types::_uint32 ||
				btc == builtin_types::_sint64 ||
				btc == builtin_types::_uint64 ||
				btc == builtin_types::_float  ||
				btc == builtin_types::_double) 
			{
				return true;
			}

			if ( is_vector(btc) || is_matrix(btc) ){
				return is_standard(scalar_of(btc));
			}

			return false;
		}

		builtin_types replace_scalar( builtin_types const& btc, builtin_types const& scalar_btc )
		{
			assert( is_scalar(scalar_btc) );
			if( !is_scalar(scalar_btc) ) { return scalar_btc; }

			if( is_vector(btc) )
			{
				return vector_of( scalar_btc, vector_size(btc) );
			}
			else if ( is_matrix(btc) )
			{
				return matrix_of( scalar_btc, vector_size(btc), vector_count(btc) );
			}
			else
			{
				return scalar_btc;
			}
		}

	}
}

