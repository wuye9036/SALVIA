#include <sasl/include/drivers/code_sources.h>

#include <sasl/include/common/diag_chat.h>
#include <sasl/include/parser/diags.h>

#include <eflib/include/diagnostics/assert.h>
#include <eflib/include/string/ustring.h>

using sasl::common::diag_chat;
using sasl::common::code_span;
using sasl::common::diag_item_committer;
using eflib::fixed_string;
using std::shared_ptr;
using std::unique_ptr;
using boost::wave::preprocess_exception;
using std::unordered_map;
using std::string;
using std::vector;
using std::cout;
using std::endl;

BEGIN_NS_SASL_DRIVERS();

std::unordered_map<void*, compiler_code_source*> compiler_code_source::ctxt_to_source;

bool compiler_code_source::set_code( string const& code )
{
	this->code = code;
	fixes_file_end_with_newline(this->code);
	this->filename = "<In Memory>";
	return process();
}

bool compiler_code_source::set_file( string const& file_name )
{
	std::ifstream in(file_name.c_str(), std::ios_base::in);
	if (!in){
		return false;
	} else {
		in.unsetf(std::ios::skipws);
		code.assign( std::istream_iterator<char>(in), std::istream_iterator<char>() );
		fixes_file_end_with_newline( code );
		filename = file_name;
	}
	in.close();

	return process();
}

bool compiler_code_source::eof()
{
	try
	{
		return (next_it == wctxt_wrapper->get_wctxt()->end());
	}
	catch(preprocess_exception& e)
	{
		report_wave_pp_exception(diags, e);
		next_it = wctxt_wrapper->get_wctxt()->end();
		return false;
	}
}

fixed_string compiler_code_source::next()
{
	cur_it = next_it;

	try
	{
		++next_it;
	}
	catch ( preprocess_exception& e )
	{
		report_wave_pp_exception(diags, e);
		errtok = to_fixed_string( cur_it->get_value() );
		next_it = wctxt_wrapper->get_wctxt()->end();
	}
	catch( wave_reported_fatal_error& )
	{
		// Error has been reported to diag_chat.
		errtok = to_fixed_string( cur_it->get_value() );
		next_it = wctxt_wrapper->get_wctxt()->end();
		is_failed = true;
	}

	return to_fixed_string( cur_it->get_value() ) ;
}

fixed_string compiler_code_source::error()
{
	if( errtok.empty() ){
		errtok = to_fixed_string( cur_it->get_value() );
	}
	return errtok;
}

void compiler_code_source::report_wave_pp_exception(
	diag_chat* diags, preprocess_exception& e
	)
{
	diag_chat::committer_pointer committer;
	switch( e.get_errorcode() )
	{
	case preprocess_exception::no_error:
		break;
	case preprocess_exception::last_line_not_terminated:
		committer = diags->report(sasl::parser::boost_wave_exception_warning);
		break;
	case preprocess_exception::ill_formed_directive:
		committer = diags->report(sasl::parser::boost_wave_exception_fatal_error);
		is_failed = true;
		break;
	default:
		is_failed = true;
		EFLIB_ASSERT_UNIMPLEMENTED();
		break;
	}


	if( committer )
	{
		fixed_string file_name;
		try
		{
			file_name = to_fixed_string(cur_it->get_position().get_file());
		}
		catch(...)
		{
			file_name = to_fixed_string(wctxt_wrapper->get_wctxt()->get_current_filename());
		}

		committer
			->p( preprocess_exception::error_text( e.get_errorcode() ) )
			->span( current_span() )
			->file(file_name);
	}
}

fixed_string const& compiler_code_source::file_name() const
{
	assert(is_failed || cur_it != wctxt_wrapper->get_wctxt()->end() );

	filename = to_fixed_string( cur_it->get_position().get_file() );
	return filename;
}

size_t compiler_code_source::column() const
{
	assert(is_failed || cur_it != wctxt_wrapper->get_wctxt()->end() );
	return cur_it->get_position().get_column();
}

size_t compiler_code_source::line() const
{
	assert(is_failed || cur_it != wctxt_wrapper->get_wctxt()->end() );
	return cur_it->get_position().get_line();
}

void compiler_code_source::update_position( const fixed_string& /*lit*/ )
{
	// Do nothing.
	return;
}

bool compiler_code_source::process()
{
	wctxt_wrapper.reset( new wave_context_wrapper( this, new wcontext_t( code.begin(), code.end(), filename.c_str() ) ) );

	size_t lang_flag = boost::wave::support_cpp;
	lang_flag &= ~(boost::wave::support_option_emit_line_directives );
	lang_flag &= ~(boost::wave::support_option_single_line );
	lang_flag &= ~(boost::wave::support_option_emit_pragma_directives );
	wctxt_wrapper->get_wctxt()->set_language( static_cast<boost::wave::language_support>( lang_flag ) );

	cur_it = wctxt_wrapper->get_wctxt()->begin();
	next_it = wctxt_wrapper->get_wctxt()->begin();
	is_failed = false;

	return true;
}

void compiler_code_source::set_diag_chat( sasl::common::diag_chat* diags )
{
	this->diags = diags;
}

compiler_code_source::compiler_code_source()
	: diags(NULL)
{

}

compiler_code_source::~compiler_code_source()
{

}

code_span compiler_code_source::current_span() const
{
	return code_span( cur_it->get_position().get_line(), cur_it->get_position().get_column(), 1 );
}

void compiler_code_source::add_virtual_file( std::string const& file_name, std::string const& content, bool high_priority )
{
	virtual_files[file_name] = make_pair( high_priority, content );
}

void compiler_code_source::set_include_handler( include_handler_fn handler )
{
	inc_handler = handler;
}

compiler_code_source* compiler_code_source::get_code_source( void* ctxt )
{
	if( ctxt )
	{
		unordered_map<void*, compiler_code_source*>::iterator it = ctxt_to_source.find(ctxt);
		if( it != ctxt_to_source.end() )
		{
			return it->second;
		}
	}

	return NULL;
}

bool compiler_code_source::failed()
{
	return is_failed;
}

bool compiler_code_source::add_sys_include_path( string const& path )
{
	wcontext_t* wctxt = wctxt_wrapper->get_wctxt();
	assert(wctxt);
	return wctxt->add_sysinclude_path( path.c_str() );
}

bool compiler_code_source::add_sys_include_path( vector<string> const& paths )
{
	bool ret = true;
	for(size_t i = 0; i < paths.size(); ++i)
	{
		bool path_ret = add_sys_include_path(paths[i]);
		ret = ret && path_ret;
	}
	return ret;
}

bool compiler_code_source::add_include_path( std::string const& path )
{
	wcontext_t* wctxt = wctxt_wrapper->get_wctxt();
	assert(wctxt);
	return wctxt->add_include_path( path.c_str() );
}

bool compiler_code_source::add_include_path( std::vector<std::string> const& paths )
{
	bool ret = true;
	for(size_t i = 0; i < paths.size(); ++i)
	{
		ret = add_include_path(paths[i]) && ret;
	}
	return ret;
}

bool compiler_code_source::add_macro( std::string const& macro_def, bool predef )
{
	wcontext_t* wctxt = wctxt_wrapper->get_wctxt();
	assert(wctxt);
	return wctxt->add_macro_definition(macro_def, predef);
}

bool compiler_code_source::add_macro( vector<string> const& macros, bool predef )
{
	bool ret = true;
	for( size_t i = 0; i < macros.size(); ++i ) {
		ret = add_macro(macros[i], predef) && ret;
	}
	return ret;
}

bool compiler_code_source::clear_macros()
{
	wcontext_t* wctxt = wctxt_wrapper->get_wctxt();
	assert(wctxt);
	wctxt->reset_macro_definitions();
	return true;
}

bool compiler_code_source::remove_macro( std::string const& macro_def )
{
	wcontext_t* wctxt = wctxt_wrapper->get_wctxt();
	assert(wctxt);
	return wctxt->remove_macro_definition(macro_def);
}

bool compiler_code_source::remove_macro( vector<string> const& macros )
{
	bool ret = true;
	for( size_t i = 0; i < macros.size(); ++i ) {
		ret = remove_macro(macros[i]) && ret;
	}
	return ret;
}

void report_load_file_failed( void* ctxt, std::string const& name, bool /*is_system*/ )
{
	compiler_code_source* code_src = compiler_code_source::get_code_source(ctxt);
	code_src->diags
		->report( sasl::parser::cannot_open_include_file )
		->file(code_src->file_name() )->span( code_src->current_span() )
		->p(name);
}

void load_virtual_file(
	bool& is_succeed, bool& is_exclusive, std::string& content,
	void* ctxt, std::string const& name, bool is_system,
	bool before_file_load )
{
	is_exclusive = false;
	is_succeed = false;

	compiler_code_source* code_src = compiler_code_source::get_code_source(ctxt);
	assert( code_src );
	if( code_src->inc_handler )
	{
		std::string native_name;
		is_exclusive = true;
		is_succeed = code_src->inc_handler( content, native_name, name, is_system, false );
		fixes_file_end_with_newline(content);
		return;
	}

	if( !is_system ) { return; }

	compiler_code_source::virtual_file_dict::iterator it
		= code_src->virtual_files.find(name);
	if ( it == code_src->virtual_files.end() )
	{
		return;
	}

	bool hi_prior = it->second.first;
	if( hi_prior == before_file_load )
	{
		content = it->second.second;
		fixes_file_end_with_newline(content);
		is_succeed = true;
		return;
	}

	return;
}

void check_file( bool& is_succeed, bool& is_exclusive, string& native_name,
	void* ctxt, string const& name, bool is_system,
	bool is_before_include )
{
	is_exclusive = false;
	is_succeed = false;

	compiler_code_source* code_src = compiler_code_source::get_code_source(ctxt);
	assert( code_src );

	// If inc handler is enabled, it is exclusive.
	if( code_src->inc_handler )
	{
		std::string content;
		is_exclusive = true;
		is_succeed = code_src->inc_handler( content, native_name, name, is_system, true );
		return;
	}

	compiler_code_source::virtual_file_dict::iterator it
		= code_src->virtual_files.find(name);
	if ( it == code_src->virtual_files.end() )
	{
		return;
	}

	bool hi_prior = it->second.first;
	if( hi_prior == is_before_include )
	{
		native_name = name;
		is_succeed = true;
		return;
	}

	return;
}

wave_context_wrapper::wave_context_wrapper( compiler_code_source* src, wcontext_t* ctx )
{
	if( wctxt )
	{
		compiler_code_source::ctxt_to_source.erase(wctxt.get());
	}
	compiler_code_source::ctxt_to_source[ctx] = src;
	wctxt.reset(ctx);
}

wave_context_wrapper::~wave_context_wrapper()
{
	if( wctxt )
	{
		compiler_code_source::ctxt_to_source.erase(wctxt.get());
	}
	wctxt.reset(NULL);
}

wcontext_t* wave_context_wrapper::get_wctxt() const
{
	return wctxt.get();
}

void fixes_file_end_with_newline( std::string& content )
{
	if ( content.empty() ) { return; }

	char end_ch = *content.rbegin();
	if( end_ch == '\r' || end_ch == '\n' )
	{
		return;
	}

#if defined(EFLIB_WINDOWS)
	content.append( "\r\n" );
#elif defined(EFLIB_OSX)
	content.append( "\r" );
#elif defined(EFLIB_UNIX) || defined(EFLIB_LINUX)
	content.append( "\n" );
#endif
}

END_NS_SASL_DRIVERS();
