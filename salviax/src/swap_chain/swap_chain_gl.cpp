#if defined(SALVIAX_GL_ENABLED)

#include <salviax/include/swap_chain/swap_chain_impl.h>

#include <salviar/include/surface.h>
#include <salviar/include/renderer.h>
#include <salviar/include/mapped_resource.h>

#include <vector>
#include <memory>

#if defined(EFLIB_WINDOWS)
#	include <windows.h>
#	pragma comment(lib, "opengl32.lib")
#endif

#include <GL/gl.h>

using namespace salviar;

BEGIN_NS_SALVIAX();

class gl_swap_chain: public swap_chain_impl
{
public:
	gl_swap_chain(
		renderer_ptr const& renderer,
		renderer_parameters const& params)
		: swap_chain_impl(renderer, params)
		, window_(nullptr)
		, dc_(nullptr), glrc_(nullptr)
		, tex_(0)
		, width_(0), height_(0)
	{
		window_ = reinterpret_cast<HWND>(params.native_window);
		initialize();
	}

	~gl_swap_chain()
	{
		glDeleteTextures(1, &tex_);
	}

	void initialize()
	{
		dc_ = GetDC(window_);

		PIXELFORMATDESCRIPTOR pfd;
		memset(&pfd, 0, sizeof(pfd));
		pfd.nSize		= sizeof(pfd);
		pfd.nVersion	= 1;
		pfd.dwFlags		= PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		pfd.iPixelType	= PFD_TYPE_RGBA;
		pfd.cColorBits	= static_cast<BYTE>(32);
		pfd.cDepthBits	= static_cast<BYTE>(0);
		pfd.cStencilBits = static_cast<BYTE>(0);
		pfd.iLayerType	= PFD_MAIN_PLANE;

		int pixelFormat = ::ChoosePixelFormat(dc_, &pfd);
		assert(pixelFormat != 0);

		::SetPixelFormat(dc_, pixelFormat, &pfd);
		::DescribePixelFormat(dc_, pixelFormat, sizeof(pfd), &pfd);

		glrc_ = ::wglCreateContext(dc_);
		::wglMakeCurrent(dc_, glrc_);

		{
			std::string ext_str(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)));
			if (ext_str.find("WGL_EXT_swap_control") != std::string::npos)
			{
				typedef BOOL (APIENTRY *wglSwapIntervalEXTFUNC)(int interval);
				wglSwapIntervalEXTFUNC wglSwapIntervalEXT = reinterpret_cast<wglSwapIntervalEXTFUNC>((void*)(::wglGetProcAddress("wglSwapIntervalEXT")));
				wglSwapIntervalEXT(0);
			}
		}

		glDisable(GL_LIGHTING);
		glDisable(GL_CULL_FACE);
		glEnable(GL_TEXTURE_2D);

		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		glGenTextures(1, &tex_);
	}

	void present_impl()
	{
		size_t		 surface_width  = resolved_surface_->width();
		size_t		 surface_height = resolved_surface_->height();
		pixel_format surf_format = resolved_surface_->get_pixel_format();

		glViewport(
			0, 0,
			static_cast<uint32_t>(surface_width),
			static_cast<uint32_t>(surface_height)
			);

		mapped_resource mapped;
		renderer_->map(mapped, resolved_surface_, map_read);
		std::vector<byte> dest(surface_width * surface_height * 4);
		for(size_t y = 0; y < surface_height; ++y)
		{
			byte* dst_line = dest.data() + y * surface_width * 4;
			byte* src_line = static_cast<byte*>(mapped.data) + y * mapped.row_pitch;
			pixel_format_convertor::convert_array(
				pixel_format_color_rgba8, surf_format,
				dst_line, src_line, int(surface_width) );
		}
		renderer_->unmap();

		glBindTexture(GL_TEXTURE_2D, tex_);
		if ((width_ < surface_width) || (height_ < surface_height))
		{
			width_	= static_cast<uint32_t>(surface_width);
			height_ = static_cast<uint32_t>(surface_height);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, &dest[0]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
		else
		{
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
				static_cast<GLsizei>(surface_width),
				static_cast<GLsizei>(surface_height),
				GL_RGBA, GL_UNSIGNED_BYTE, dest.data()
			);
		}

		float fw = static_cast<float>(surface_width) / width_;
		float fh = static_cast<float>(surface_height) / height_;

		glBegin(GL_TRIANGLE_STRIP);

		glTexCoord2f(0, 0);
		glVertex3f(-1, +1, 0);

		glTexCoord2f(fw, 0);
		glVertex3f(+1, +1, 0);

		glTexCoord2f(0, fh);
		glVertex3f(-1, -1, 0);

		glTexCoord2f(fw, fh);
		glVertex3f(+1, -1, 0);

		glEnd();

		::SwapBuffers(dc_);
	}

private:
	HWND		window_;
	HDC			dc_;
	HGLRC		glrc_;
	GLuint		tex_;
	uint32_t	width_, height_;
};

swap_chain_ptr create_gl_swap_chain(
	renderer_ptr const& renderer,
	renderer_parameters const* params)
{
	if(!params)
	{
		return swap_chain_ptr();
	}

	return std::make_shared<gl_swap_chain>(renderer, *params);
}

END_NS_SALVIAX();

#endif
