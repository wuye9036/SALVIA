#ifndef SALVIA_EXT_RESOURCE_MODEL_H
#define SALVIA_EXT_RESOURCE_MODEL_H

#include <slaviax/include/resource/resource_forward.h>

namespace salvia::ext::resource {

class model {
public:
  void add_mesh(salviar::h_mesh const&);

  void render();

private:
  std::vector<h_mesh> meshes;
};

}
}
;

#endif