#pragma once

#include <sasl/common/diag_item.h>
#include <sasl/common/lex_context.h>

#include <boost/wave.hpp>
#include <boost/wave/cpplexer/cpp_lex_iterator.hpp>
#include <boost/wave/cpplexer/cpp_lex_token.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace sasl::common {
class diag_chat;
}

namespace sasl::drivers {

class wave_reported_fatal_error : public boost::exception {};

void fixes_file_end_with_newline(std::string& content);

typedef std::function<bool /*succeed*/ (std::string& /*[out]content*/,
                                        std::string& /*[out]native file name*/,
                                        std::string const& /*file name*/,
                                        bool /*is system header*/,
                                        bool /*check only*/)>
    include_handler_fn;

void load_virtual_file(bool& is_succeed,
                       bool& is_exclusive,
                       std::string& content,
                       void* ctxt,
                       std::string const& name,
                       bool is_system,
                       bool before_file_load);
void check_file(bool& is_succeed,
                bool& is_exclusive,
                std::string& native_name,
                void* ctxt,
                std::string const& name,
                bool is_system,
                bool is_before_include);

void report_load_file_failed(void* ctxt, std::string const& name, bool is_system);

struct wave_hooks : public boost::wave::context_policies::default_preprocessing_hooks {
  typedef boost::wave::context_policies::default_preprocessing_hooks base_type;
  template <typename ContextT>
  bool locate_include_file(ContextT& ctx,
                           std::string& file_path,
                           bool is_system,
                           char const* current_name,
                           std::string& dir_path,
                           std::string& native_name) {
    bool is_succeed = false;
    bool is_exclusive = false;

    check_file(is_succeed, is_exclusive, native_name, &ctx, file_path, is_system, true);
    if (is_exclusive || is_succeed) {
      return is_succeed;
    }

    if (ctx.find_include_file(file_path, dir_path, is_system, current_name)) {
      namespace fs = std::filesystem;

      fs::path native_path(file_path);
      if (fs::exists(native_path)) {
        native_name = native_path.string();
        return true;
      }
    }

    check_file(is_succeed, is_exclusive, native_name, &ctx, file_path, is_system, false);
    if (!is_succeed) {
      report_load_file_failed(&ctx, file_path, is_system);
      throw wave_reported_fatal_error();
    }

    return true;
  }
};

struct load_file_to_string {
  template <typename IterContext>
  class inner {
  public:
    // expose the 'begin' and 'end' iterators for the
    // included file
    template <typename Position>
    static void init_iterators(IterContext& iter_ctx,
                               Position const& /*act_pos*/,
                               boost::wave::language_support language) {
      typedef typename IterContext::iterator_type iterator_type;
      bool is_system = (iter_ctx.type == IterContext::system_header);
      bool is_exclusive = false;
      bool is_succeed = false;
      load_virtual_file(is_succeed,
                        is_exclusive,
                        iter_ctx.instring,
                        &iter_ctx.ctx,
                        iter_ctx.filename.c_str(),
                        is_system,
                        true);

      if (!(is_exclusive || is_succeed)) {
        std::ifstream instream(iter_ctx.filename.c_str());
        if (instream.is_open()) {
          instream.unsetf(std::ios::skipws);

          iter_ctx.instring.assign(std::istreambuf_iterator<char>(instream.rdbuf()),
                                   std::istreambuf_iterator<char>());
          fixes_file_end_with_newline(iter_ctx.instring);
        } else {
          load_virtual_file(is_succeed,
                            is_exclusive,
                            iter_ctx.instring,
                            &iter_ctx.ctx,
                            iter_ctx.filename.c_str(),
                            is_system,
                            false);

          if (!is_succeed) {
            report_load_file_failed(&iter_ctx.ctx, iter_ctx.filename.c_str(), is_system);
            throw wave_reported_fatal_error();
          }
        }
      }

      iter_ctx.first = iterator_type(iter_ctx.instring.begin(),
                                     iter_ctx.instring.end(),
                                     Position(iter_ctx.filename),
                                     language);
      iter_ctx.last = iterator_type();
    }

  private:
    std::string instring;
  };
};

using wave_lex_iterator = boost::wave::cpplexer::lex_iterator<boost::wave::cpplexer::lex_token<>>;
using wave_context =
    boost::wave::context<std::string::iterator, wave_lex_iterator, load_file_to_string, wave_hooks>;

class compiler_code_source;

class wave_context_wrapper {
public:
  wave_context_wrapper(compiler_code_source* src, wave_context* ctx);
  ~wave_context_wrapper();
  wave_context* get_wave_ctxt() const;

private:
  std::unique_ptr<wave_context> wave_ctxt;
};

class compiler_code_source
  : public sasl::common::lex_context
  , public sasl::common::code_source {
private:
  friend class wave_context_wrapper;

public:
  compiler_code_source();
  ~compiler_code_source();

  void set_diag_chat(sasl::common::diag_chat*);
  bool set_code(std::string const&);
  bool set_file(std::string const&);

  // Inherited from code_source
  bool eof() override;
  std::string_view next() override;
  std::string_view error() override;
  bool failed() override;

  // Inherited from lex_context
  std::string_view file_name() const override;
  size_t column() const override;
  size_t line() const override;
  void update_position(std::string_view /*lit*/) override;

  // hooks. It's enabled after function is called.
  virtual void
  add_virtual_file(std::string const& file_name, std::string const& content, bool high_priority);
  virtual void set_include_handler(include_handler_fn handler);

  // Inputs of preprocessor.
  bool add_include_path(std::string const&);
  bool add_include_path(std::vector<std::string> const&);
  bool add_sys_include_path(std::string const&);
  bool add_sys_include_path(std::vector<std::string> const&);

  bool add_macro(std::string const&, bool predef);
  bool add_macro(std::vector<std::string> const&, bool predef);
  bool remove_macro(std::string const&);
  bool remove_macro(std::vector<std::string> const&);
  bool clear_macros();

private:
  void report_wave_pp_exception(sasl::common::diag_chat* diags,
                                boost::wave::preprocess_exception& exc);

  // Utilities
  sasl::common::code_span current_span() const;
  bool process();

  template <typename StringT>
  std::string to_string(StringT const& str) const {
    return std::string(str.begin(), str.end());
  }

  template <typename StringT>
  std::string_view to_string_view(StringT const& str) const {
    return std::string_view(str.begin(), str.end());
  }

  friend void load_virtual_file(bool& is_succeed,
                                bool& is_exclusive,
                                std::string& content,
                                void* ctxt,
                                std::string const& name,
                                bool is_system,
                                bool before_file_load);
  friend void check_file(bool& is_succeed,
                         bool& is_exclusive,
                         std::string& native_name,
                         void* ctxt,
                         std::string const& name,
                         bool is_system,
                         bool is_before_include);
  friend void report_load_file_failed(void* ctxt, std::string const& name, bool is_system);

  std::unique_ptr<wave_context_wrapper> wave_ctxt_wrapper;
  sasl::common::diag_chat* diags;

  mutable std::string filename;
  bool is_failed;
  std::string code;
  std::string err_token;

  wave_context::iterator_type cur_it;
  wave_context::iterator_type next_it;

  std::unordered_map<std::string, std::pair<bool, std::string>> virtual_files;
  include_handler_fn do_include;

  static std::unordered_map<void*, compiler_code_source*> ctxt_to_source;
  static compiler_code_source* get_code_source(void*);
};

}  // namespace sasl::drivers