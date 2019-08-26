#ifndef EFLIB_PLATFORM_DL_LOADER_H
#define EFLIB_PLATFORM_DL_LOADER_H

#include <eflib/include/string/string.h>

#include <memory>

namespace eflib
{
	class dynamic_lib
	{
	public:
		static std::shared_ptr<dynamic_lib> load( std::string const& name );

		template <typename PFnT>
		bool get_function( PFnT& fn, std::string const& name ) const
		{
			void* pfn = get_function(name);
			if( pfn ) {
				fn = (PFnT)(pfn);
				return true;
			}
			fn = NULL;
			return false;
		}

		virtual bool available() const = 0;
		virtual ~dynamic_lib() {}

	private:
		virtual void* get_function( std::string const& name ) const = 0;
	};
}

#define EFLIB_IMPORT_DLL_FUNCTION( fn_type, fn_name, dy_lib, sym_name )	\
	std::type_identity<fn_type>::type fn_name = NULL;	\
	(dy_lib)->get_function( (fn_name), #sym_name );

#if defined(EFLIB_WINDOWS)
#	define EFLIB_DYNAMIC_LIB_EXT ".dll"
#elif defined(EFLIB_LINUX)
#	define EFLIB_DYNAMIC_LIB_EXT ".so"
#endif

#if defined(EFLIB_DEBUG)
#	define EFLIB_DYNAMIC_LIB_SUFFIX "_d"
#else
#	define EFLIB_DYNAMIC_LIB_SUFFIX ""
#endif

#define EFLIB_DYNAMIC_LIB_NAME( lib_basic_name ) lib_basic_name EFLIB_DYNAMIC_LIB_SUFFIX EFLIB_DYNAMIC_LIB_EXT

#endif
