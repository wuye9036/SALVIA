#pragma once

#include <salvia/ext/swap_chain/swap_chain.h>

namespace salvia::ext{

class swap_chain_impl : public swap_chain {
public:
  virtual salvia::resource::surface_ptr get_surface();
  virtual void present();

protected:
  swap_chain_impl(salvia::core::renderer_ptr const &renderer,
                  salvia::core::renderer_parameters const &render_params);

  virtual void present_impl() = 0;

protected:
  salvia::core::renderer_ptr renderer_;
  salvia::resource::surface_ptr surface_;
  salvia::resource::surface_ptr resolved_surface_;
};

#if defined(SALVIAX_GL_ENABLED)
swap_chain_ptr create_gl_swap_chain(salvia::core::renderer_ptr const &renderer,
                                    salvia::core::renderer_parameters const *);
#endif

#if defined(SALVIAX_D3D11_ENABLED)
swap_chain_ptr create_d3d11_swap_chain(salvia::core::renderer_ptr const &renderer,
                                       salvia::core::renderer_parameters const *);
#endif

}
