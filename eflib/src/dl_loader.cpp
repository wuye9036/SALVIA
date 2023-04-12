#include <eflib/platform/dl_loader.h>

#ifdef EFLIB_WINDOWS
#  if !defined NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

#if defined(EFLIB_LINUX) || defined(EFLIB_MACOS)
#  include <dlfcn.h>
#endif

using std::shared_ptr;

namespace eflib {

#ifdef EFLIB_WINDOWS

class win_dl : public dynamic_lib {
public:
  win_dl(std::string const& name) {
    mod = ::LoadLibraryA(name.c_str());
    void (*loaded_hook)() = nullptr;
    dynamic_lib::get_function(loaded_hook, "_eflib_dynlib_loaded");
    if (loaded_hook) {
      loaded_hook();
    }
  }

  ~win_dl() {
    void (*unloading_hook)() = nullptr;
    dynamic_lib::get_function(unloading_hook, "_eflib_dynlib_unloading");
    if (unloading_hook) {
      unloading_hook();
    }

    ::FreeLibrary(mod);
    mod = (HMODULE)0;
  }

  virtual void* get_function(std::string const& name) const {
    if (!mod)
      return nullptr;
    return (void*)(::GetProcAddress(mod, name.c_str()));
  }

  virtual bool available() const { return mod != nullptr; }

  HMODULE mod;
};
#elif defined(EFLIB_LINUX) || defined(EFLIB_MACOS)
class linux_dl : public dynamic_lib {
public:
  explicit linux_dl(std::string const& name) {
    mod = ::dlopen(name.c_str(), RTLD_NOW);
    void (*loaded_hook)() = nullptr;
    dynamic_lib::get_function(loaded_hook, "_eflib_dynlib_loaded");
    if (loaded_hook) {
      loaded_hook();
    }
  }

  ~linux_dl() override {
    if (mod == nullptr) {
      return;
    }

    void (*unloading_hook)() = nullptr;
    dynamic_lib::get_function(unloading_hook, "_eflib_dynlib_unloading");
    if (unloading_hook) {
      unloading_hook();
    }

    dlclose(mod);
    mod = nullptr;
  }

  [[nodiscard]] void* get_function(std::string const& name) const override {
    if (!mod)
      return nullptr;
    return (void*)(dlsym(mod, name.c_str()));
  }

  [[nodiscard]] bool available() const override { return mod != nullptr; }

  void* mod;
};
#endif

shared_ptr<dynamic_lib> dynamic_lib::load(std::string const& name) {
#if defined(EFLIB_WINDOWS)
  win_dl* dynlib = new win_dl(name);
#elif defined(EFLIB_LINUX) || defined(EFLIB_MACOS)
  auto* dynlib = new linux_dl(name);
#else
#  error "Platform is not supported."
#endif
  return shared_ptr<dynamic_lib>(dynlib);
}
}  // namespace eflib
