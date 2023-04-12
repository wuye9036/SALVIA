#pragma once

#include <sasl/common/diag_item.h>
#include <sasl/common/token.h>

#include <fmt/format.h>

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace sasl::common {

class diag_chat;
class diag_item;
struct code_span;

using report_handler_fn = std::function<bool(diag_chat*, diag_item*)>;

struct diag_context {
  std::string_view file_name;
  code_span span;
};

class diag_chat {
private:
  void report_impl(diag_template tmpl,
                   std::string_view file_name,
                   code_span span,
                   fmt::format_args args);

public:
  static std::shared_ptr<diag_chat> create();
  static diag_chat* merge(diag_chat* dest, diag_chat* src, bool trigger_callback);

  void add_report_raised_handler(report_handler_fn const& handler);
  void report(diag_template tmpl, token token_beg, token token_end);
  void report(diag_template tmpl, std::string_view file_name, code_span span);

  template <typename... Args>
  void report(diag_template tmpl, token token_beg, token token_end, Args&&... args) {
    report_impl(tmpl,
                token_beg.file_name(),
                sasl::common::merge(token_beg.span(), token_end.span()),
                fmt::make_format_args(std::forward<Args>(args)...));
  }

  template <typename... Args>
  void report(diag_template tmpl, std::string_view file_name, code_span span, Args&&... args) {
    report_impl(tmpl, file_name, span, fmt::make_format_args(std::forward<Args>(args)...));
  }

  decltype(auto) items() const noexcept { return (diags_); }
  void clear();

  void save();
  void restore();

  ~diag_chat();

private:
  std::vector<report_handler_fn> handlers_;
  std::vector<diag_item> diags_;
  std::vector<size_t> cursors_;
};

class chat_scope {
public:
  chat_scope(diag_chat* chat) : need_save(false), chat(chat) { chat->save(); }
  ~chat_scope() {
    if (!need_save)
      chat->restore();
  }
  void save_new_diags() { need_save = true; }

private:
  bool need_save;
  diag_chat* chat;
};

size_t error_count(diag_chat const* chat, bool warning_as_error);

}  // namespace sasl::common