#ifndef SASL_CODEGEN_CG_TYPE_BUILDER_H
#define SASL_CODEGEN_CG_TYPE_BUILDER_H

#include <vector>

namespace llvm_tags
{
	struct tag_base{};

	template <typename ElementT, int sz>
	struct tag_vector: public tag_base
	{
		typedef ElementT element_t;
		static int const size = sz;
	};

	template <typename ElementT, int vsz, int vcnt>
	struct tag_matrix: public tag_base
	{
		typedef ElementT element_t;
		static int const v_size = vsz;
		static int const v_count = vcnt;
	};

	typedef tag_vector<float, 4> float4;
}

namespace llvm
{
	template < typename ElementT, int sz, bool cross>
	class TypeBuilder< llvm_tags::tag_vector<ElementT, sz>, cross >
	{
		static VectorType const* get()
		{
			return VectorType::get( TypeBuilder<ElementT>::get(), static_cast<unsigned>(sz) );
		}
	};
}
#endif