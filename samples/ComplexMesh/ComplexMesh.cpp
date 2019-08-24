#include <salviax/include/resource/mesh/sa/mesh_io.h>
#include <salviax/include/resource/mesh/sa/mesh_impl.h>
#include <salviax/include/resource/texture/tex_io.h>
#include <salviax/include/swap_chain/swap_chain.h>

#include <salviau/include/common/sample_app.h>
#include <salviau/include/common/path.h>

#include <salviar/include/shader.h>
#include <salviar/include/shader_regs.h>
#include <salviar/include/sync_renderer.h>
#include <salviar/include/resource_manager.h>
#include <salviar/include/rasterizer.h>

#include <eflib/include/utility/unref_declarator.h>
#include <eflib/include/platform/main.h>

#include <iostream>
#include <fstream>
#include <string>

#include <tchar.h>

using namespace eflib;
using namespace std;
using namespace salviar;
using namespace salviax;
using namespace salviax::resource;
using namespace salviau;

struct vert
{
	vec4 pos;
};

typedef std::vector<std::pair<std::string, std::string> > TextureSlotsType;
struct Material
{
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
	vec3 emit;
	float opacity;
	float specular_level;
	float shininess;

	TextureSlotsType texture_slots;
};

void ReadShortString(std::istream& file, std::string& str)
{
	uint8_t len;
	file.read(reinterpret_cast<char*>(&len), sizeof(len));
	str.resize(len);
	file.read(reinterpret_cast<char*>(&str[0]), len * sizeof(str[0]));
}

mesh_ptr LoadModel(salviar::renderer_ptr hsr, std::string const & mesh_name)
{
	std::vector<Material> mtls;
	std::vector<std::string> mesh_names;

	mesh_impl* pmesh = new mesh_impl(hsr.get());

	size_t const geometry_slot = 0;
	size_t const normal_slot = 1;

	std::ifstream file(mesh_name.c_str(), std::ios::binary);
	uint32_t fourcc;
	file.read(reinterpret_cast<char*>(&fourcc), sizeof(fourcc));

	uint64_t original_len, len;
	file.read(reinterpret_cast<char*>(&original_len), sizeof(original_len));
	file.read(reinterpret_cast<char*>(&len), sizeof(len));

	uint32_t num_mtls;
	file.read(reinterpret_cast<char*>(&num_mtls), sizeof(num_mtls));
	uint32_t num_meshes;
	file.read(reinterpret_cast<char*>(&num_meshes), sizeof(num_meshes));
	uint32_t num_joints;
	file.read(reinterpret_cast<char*>(&num_joints), sizeof(num_joints));
	uint32_t num_kfs;
	file.read(reinterpret_cast<char*>(&num_kfs), sizeof(num_kfs));

	mtls.resize(num_mtls);
	for (uint32_t mtl_index = 0; mtl_index < num_mtls; ++ mtl_index)
	{
		Material& mtl = mtls[mtl_index];
		file.read(reinterpret_cast<char*>(&mtl.ambient.x ()), sizeof(float));
		file.read(reinterpret_cast<char*>(&mtl.ambient.y ()), sizeof(float));
		file.read(reinterpret_cast<char*>(&mtl.ambient.z ()), sizeof(float));
		file.read(reinterpret_cast<char*>(&mtl.diffuse.x ()), sizeof(float));
		file.read(reinterpret_cast<char*>(&mtl.diffuse.y ()), sizeof(float));
		file.read(reinterpret_cast<char*>(&mtl.diffuse.z ()), sizeof(float));
		file.read(reinterpret_cast<char*>(&mtl.specular.x()), sizeof(float));
		file.read(reinterpret_cast<char*>(&mtl.specular.y()), sizeof(float));
		file.read(reinterpret_cast<char*>(&mtl.specular.z()), sizeof(float));
		file.read(reinterpret_cast<char*>(&mtl.emit.x()), sizeof(float));
		file.read(reinterpret_cast<char*>(&mtl.emit.y()), sizeof(float));
		file.read(reinterpret_cast<char*>(&mtl.emit.z()), sizeof(float));
		file.read(reinterpret_cast<char*>(&mtl.opacity), sizeof(float));
		file.read(reinterpret_cast<char*>(&mtl.specular_level), sizeof(float));
		file.read(reinterpret_cast<char*>(&mtl.shininess), sizeof(float));

		uint32_t num_texs;
		file.read(reinterpret_cast<char*>(&num_texs), sizeof(num_texs));

		for (uint32_t tex_index = 0; tex_index < num_texs; ++ tex_index)
		{
			std::string type, name;
			ReadShortString(file, type);
			ReadShortString(file, name);
			mtl.texture_slots.push_back(std::make_pair(type, name));
		}
	}

	mesh_names.resize(num_meshes);

	vector<input_element_desc> elem_descs;

	for (uint32_t mesh_index = 0; mesh_index < num_meshes; ++ mesh_index)
	{
		ReadShortString(file, mesh_names[mesh_index]);

		int32_t mtl_id;
		file.read(reinterpret_cast<char*>(&mtl_id), sizeof(mtl_id));

		uint32_t num_ves;
		file.read(reinterpret_cast<char*>(&num_ves), sizeof(num_ves));

		for (uint32_t ve_index = 0; ve_index < num_ves; ++ ve_index)
		{
			struct vertex_element
			{
				uint32_t usage;
				uint8_t usage_index;
				uint64_t format;
			};

			vertex_element ve;
			file.read(reinterpret_cast<char*>(&ve), sizeof(ve));
		}

		elem_descs.push_back( input_element_desc( "POSITION", 0, format_r32g32b32_float, 0, 0, input_per_vertex, 0 ) );
		elem_descs.push_back( input_element_desc( "NORMAL",   0, format_r32g32b32_float, 1, 0, input_per_vertex, 0 ) );

		// Read vertex buffers
		uint32_t num_vertices;
		file.read(reinterpret_cast<char*>(&num_vertices), sizeof(num_vertices));

		uint32_t max_num_blend;
		file.read(reinterpret_cast<char*>(&max_num_blend), sizeof(max_num_blend));

		salviar::buffer_ptr verts		= pmesh->create_buffer( sizeof(vec3) * num_vertices );
		salviar::buffer_ptr normals	= pmesh->create_buffer( sizeof(vec3) * num_vertices );
		vec3* verts_data   = reinterpret_cast<vec3*>(verts->raw_data(0));
		vec3* normals_data = reinterpret_cast<vec3*>(normals->raw_data(0));
		file.read(reinterpret_cast<char*>(verts_data), num_vertices * sizeof(vec3));
		file.read(reinterpret_cast<char*>(normals_data), num_vertices * sizeof(vec3));
		pmesh->add_vertex_buffer( geometry_slot, verts,   sizeof(vec3), 0 );
		pmesh->add_vertex_buffer( normal_slot,   normals, sizeof(vec3), 0 );

		// Read index buffer.
		uint32_t num_triangles;
		file.read(reinterpret_cast<char*>(&num_triangles), sizeof(num_triangles));

		salviar::buffer_ptr indices = pmesh->create_buffer( sizeof(uint16_t) * num_triangles * 3);
		uint16_t* indices_data = reinterpret_cast<uint16_t*>(indices->raw_data(0));

		char is_index_16_bit;
		file.read(&is_index_16_bit, sizeof(is_index_16_bit));
		pmesh->set_index_type(is_index_16_bit ? format_r16_uint : format_r32_uint);

		file.read(reinterpret_cast<char*>(indices_data), sizeof(uint16_t) * num_triangles * 3);

		pmesh->set_index_buffer( indices );

		// Set other parameters
		pmesh->set_primitive_count(num_triangles);
		pmesh->set_input_element_descs( elem_descs );
	}

	return mesh_ptr(pmesh);
}

class vs_mesh : public cpp_vertex_shader
{
	mat44 wv;
	mat44 proj;
	vec3 light_pos;
	vec3 eye_pos;
public:
	vs_mesh():wv(mat44::identity()), proj(mat44::identity())
	{
		declare_constant(_T("WorldViewMat"), wv);
		declare_constant(_T("ProjMat"), proj);
		declare_constant(_T("LightPos"), light_pos);
		declare_constant(_T("EyePos"), eye_pos);

		bind_semantic( "POSITION", 0, 0 );
		bind_semantic( "NORMAL", 0, 1 );
	}

	void shader_prog(const vs_input& in, vs_output& out)
	{
		vec4 pos = in.attribute(0);
		vec4 pos_es, normal_es;
		transform(pos_es, pos, wv);
		transform33(normal_es, in.attribute(1), wv);
		transform(out.position(), pos_es, proj);
		out.attribute(0) = vec4(light_pos - pos_es.xyz(), 1);
		out.attribute(1) = vec4(eye_pos - pos_es.xyz(), 1);
		out.attribute(2) = normal_es;
	}

	uint32_t num_output_attributes() const
	{
		return 3;
	}

	uint32_t output_attribute_modifiers(uint32_t index) const
	{
		switch (index)
		{
		case 0:
			return salviar::vs_output::am_linear;

		case 1:
			return salviar::vs_output::am_linear;

		case 2:
			return salviar::vs_output::am_linear;

		default:
			return salviar::vs_output::am_linear;
		}
	}
    
    virtual cpp_shader_ptr clone()
	{
        typedef std::remove_pointer<decltype(this)>::type this_type;
		return cpp_shader_ptr(new this_type(*this));
	}

};

class ps_mesh : public cpp_pixel_shader
{
public:
	ps_mesh()
	{
	}
	bool shader_prog(const vs_output& in, ps_output& out)
	{
		vec3 l = normalize3(in.attribute(0).xyz());
		vec3 e = normalize3(in.attribute(1).xyz());
		vec3 n = normalize3(in.attribute(2).xyz());
		float n_dot_l = dot_prod3(n, l);
		float roughness = 5;
		float spec = (roughness + 2) / 2 * pow(max(dot_prod3(normalize3(l + e), n), 0.0f), roughness);
		float clr = n_dot_l * (0.8f + spec * 0.4f);
		out.color[0] = vec4(clr, clr, clr, 1);
		return true;
	}

    virtual cpp_shader_ptr clone()
	{
        typedef std::remove_pointer<decltype(this)>::type this_type;
		return cpp_shader_ptr(new this_type(*this));
	}
};

class ts_blend_off : public cpp_blend_shader
{
public:
	bool shader_prog(size_t sample, pixel_accessor& inout, const ps_output& in)
	{
		inout.color(0, sample, color_rgba32f(in.color[0]));
		return true;
	}
    
    virtual cpp_shader_ptr clone()
	{
        typedef std::remove_pointer<decltype(this)>::type this_type;
		return cpp_shader_ptr(new this_type(*this));
	}

};

int const BENCHMARK_FRAME_COUNT = eflib::is_debug_mode ? 3 : 300;
int const TEST_FRAME_COUNT		= 3;

class complex_mesh: public sample_app
{
public:
	complex_mesh(): sample_app("ComplexMesh")
	{
	}

	void on_init() override
	{
		create_devices_and_targets(data_->screen_width, data_->screen_height, 1, pixel_format_color_bgra8, pixel_format_color_rg32f);
        data_->renderer->set_viewport(data_->screen_vp);

		raster_desc rs_desc;
		rs_desc.cm = cull_back;
		rs_back.reset(new raster_state(rs_desc));

		// Loading mesh
		cout << "Loading mesh ... " << endl;
		mesh_ = LoadModel(data_->renderer, find_path("complex_mesh/M134 Predator.MESHML.model_bin"));
		cout << "Loading pixel and blend shader... " << endl;

		// Initialize shader
		cpp_vs.reset(new vs_mesh());
		cpp_ps.reset(new ps_mesh());
		cpp_bs.reset(new ts_blend_off());

		switch(data_->mode)
		{
		case app_modes::benchmark:
			quit_at_frame(BENCHMARK_FRAME_COUNT);
			break;
		case app_modes::test:
			quit_at_frame(TEST_FRAME_COUNT);
			break;
		}
	}

	void on_frame() override
	{
		profiling("BackBufferClearing", [this](){
			data_->renderer->clear_color(data_->color_target, color_rgba32f(0.2f, 0.2f, 0.5f, 1.0f));
			data_->renderer->clear_depth_stencil(data_->ds_target, clear_depth | clear_stencil, 1.0f, 0);
		});

		mat44 world(mat44::identity()), view, proj, wv;
		static float s_angle = -1;
		vec3 camera(cos(s_angle) * 400.0f, 600.0f, sin(s_angle) * 400.0f);
		vec3 eye(0.0f, 0.0f, 0.0f);
		mat_lookat(view, camera, eye, vec3(0.0f, 1.0f, 0.0f));
		mat_perspective_fov(proj, static_cast<float>(HALF_PI), data_->screen_aspect_ratio, 0.1f, 1000.0f);
		mat_mul(wv, world, view);
		
		vec3 light_pos(vec3(-4, 2, 0));

		profiling("Rendering", [&](){
			data_->renderer->set_vertex_shader(cpp_vs);
			data_->renderer->set_pixel_shader(cpp_ps);
			data_->renderer->set_blend_shader(cpp_bs);

			data_->renderer->set_rasterizer_state(rs_back);

			cpp_vs->set_constant(_T("WorldViewMat"), &wv);
			cpp_vs->set_constant(_T("ProjMat"), &proj);
			cpp_vs->set_constant(_T("LightPos"), &light_pos);
			cpp_vs->set_constant(_T("EyePos"), &eye);

			mesh_->render();
		});
	}

protected:	
    cpp_vertex_shader_ptr	cpp_vs;
	cpp_pixel_shader_ptr	cpp_ps;
	cpp_blend_shader_ptr	cpp_bs;
	mesh_ptr				mesh_;

	raster_state_ptr		rs_back;
};

EFLIB_MAIN(argc, argv)
{
    complex_mesh loader;
	loader.init(argc, const_cast<std::_tchar const**>(argv));
	loader.run();

	return 0;
}
