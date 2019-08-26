#include <salviar/include/renderer.h>

#include <salviar/include/binary_modules.h>
#include <salviar/include/sync_renderer.h>
#include <salviar/include/sampler_api.h>
#include <salviar/include/shader_impl.h>
#include <salviar/include/shader_object.h>
#include <salviar/include/async_renderer.h>

#include <eflib/include/platform/dl_loader.h>

#include <vector>
#include <string>
#include <iostream>
#include <memory>

using eflib::dynamic_lib;
using std::shared_ptr;
using std::vector;
using std::string;
using std::cout;
using std::endl;

BEGIN_NS_SALVIAR();

#define USE_ASYNC_RENDERER
renderer_ptr create_software_renderer()
{
#if defined(USE_ASYNC_RENDERER)
	return create_async_renderer();
#else
	return create_sync_renderer();
#endif
}

renderer_ptr create_benchmark_renderer()
{
	return create_sync_renderer();
}

shader_object_ptr	compile(std::string const& code, shader_profile const& profile, shader_log_ptr& logs)
{
	vector<external_function_desc> external_funcs;
	external_funcs.push_back( external_function_desc((void*)&tex2Dlod,		"sasl.vs.tex2d.lod",	true) );
	external_funcs.push_back( external_function_desc((void*)&texCUBElod,	"sasl.vs.texCUBE.lod",	true) );
	external_funcs.push_back( external_function_desc((void*)&tex2Dlod_ps,	"sasl.ps.tex2d.lod" ,	true) );
	external_funcs.push_back( external_function_desc((void*)&tex2Dgrad_ps,	"sasl.ps.tex2d.grad",	true) );
	external_funcs.push_back( external_function_desc((void*)&tex2Dbias_ps,	"sasl.ps.tex2d.bias",	true) );
	external_funcs.push_back( external_function_desc((void*)&tex2Dproj_ps,	"sasl.ps.tex2d.proj",	true) );

	shader_object_ptr ret;
	modules::host::compile(ret, logs, code, profile, external_funcs);

	return ret;
}

shader_object_ptr	compile(std::string const& code, shader_profile const& profile)
{
	shader_log_ptr log;
	shader_object_ptr ret = compile(code, profile, log);

	if(!ret)
	{
		cout << "Shader was compiled failed!" << endl;
		for( size_t i = 0; i < log->count(); ++i )
		{
			cout << log->log_string(i) << endl;
		}
	}

	return ret;
}

shader_object_ptr	compile(std::string const& code, languages lang)
{
	shader_profile prof;
	prof.language = lang;
	return compile(code, prof);
}

shader_object_ptr	compile_from_file(std::string const& file_name, shader_profile const& profile, shader_log_ptr& logs)
{
	vector<external_function_desc> external_funcs;
	external_funcs.push_back( external_function_desc((void*)&tex2Dlod,		"sasl.vs.tex2d.lod",	true) );
	external_funcs.push_back( external_function_desc((void*)&texCUBElod,	"sasl.vs.texCUBE.lod",	true) );
	external_funcs.push_back( external_function_desc((void*)&tex2Dlod_ps,	"sasl.ps.tex2d.lod" ,	true) );
	external_funcs.push_back( external_function_desc((void*)&tex2Dgrad_ps,	"sasl.ps.tex2d.grad",	true) );
	external_funcs.push_back( external_function_desc((void*)&tex2Dbias_ps,	"sasl.ps.tex2d.bias",	true) );
	external_funcs.push_back( external_function_desc((void*)&tex2Dproj_ps,	"sasl.ps.tex2d.proj",	true) );

	shader_object_ptr ret;
	modules::host::compile_from_file(ret, logs, file_name, profile, external_funcs);

	return ret;
}

shader_object_ptr	compile_from_file(std::string const& file_name, shader_profile const& profile)
{
	shader_log_ptr log;
	shader_object_ptr ret = compile_from_file(file_name, profile, log);

	if(!ret)
	{
		cout << "Shader was compiled failed!" << endl;
		for( size_t i = 0; i < log->count(); ++i )
		{
			cout << log->log_string(i) << endl;
		}
	}

	return ret;
}

shader_object_ptr	compile_from_file(std::string const& file_name, languages lang)
{
	shader_profile prof;
	prof.language = lang;
	return compile_from_file(file_name, prof);
}

END_NS_SALVIAR();
