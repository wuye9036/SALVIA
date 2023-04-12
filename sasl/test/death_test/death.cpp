#define ALL_TESTS_ENABLED 1

#include <boost/test/unit_test.hpp>
#include <eflib/platform/boost_begin.h>
#include <eflib/platform/boost_end.h>

#include <salvia/shader/reflection.h>
#include <sasl/codegen/cg_api.h>
#include <sasl/common/diag_chat.h>
#include <sasl/common/diag_formatter.h>
#include <sasl/drivers/drivers_api.h>
#include <sasl/semantic/semantics.h>
#include <sasl/semantic/symbol.h>

#include <eflib/math/matrix.h>
#include <eflib/math/vector.h>

#include <boost/format.hpp>
#include <eflib/platform/boost_begin.h>
#include <eflib/platform/boost_end.h>

#include <eflib/platform/cpuinfo.h>

#include <filesystem>
#include <fstream>
#include <iostream>

using namespace eflib;

EFLIB_USING_SHARED_PTR(sasl::codegen, module_vmcode);

using sasl::codegen::module_vmcode;
using sasl::common::diag_chat;
using sasl::common::diag_item;
using sasl::drivers::compiler;
using sasl::semantic::symbol;

using salviar::PACKAGE_ELEMENT_COUNT;
using salviar::PACKAGE_LINE_ELEMENT_COUNT;

using boost::format;
using std::dynamic_pointer_cast;
using std::shared_ptr;
namespace fs = std::filesystem;

using std::cout;
using std::endl;
using std::fstream;
using std::make_pair;
using std::pair;
using std::string;
using std::vector;

BOOST_AUTO_TEST_SUITE(death)

string make_command(string const& file_name, string const& options) {
  return (format("--input=\"%s\" %s") % file_name % options).str();
}

string make_command(string const& inc,
                    string const& sysinc,
                    string const& file_name,
                    string const& options) {
  return (format("--input=\"%s\" -I \"%s\" -S \"%s\" %s") % file_name % inc % sysinc % options)
      .str();
}

bool print_diagnostic(diag_chat*, diag_item* item) {
  BOOST_TEST_MESSAGE(sasl::common::str(item));
  return true;
}

struct jit_fixture {
  jit_fixture() {}

  void init_g(string const& file_name) { init(file_name, "--lang=g"); }

  void init_vs(string const& file_name) { init(file_name, "--lang=vs"); }

  void init_ps(string const& file_name) { init(file_name, "--lang=ps"); }

  void add_virtual_file(char const* name, char const* content) {
    vfiles.push_back(make_pair(name, content));
  }

  void init(string const& file_name, string const& options) {
    init_cmd(make_command(file_name, options));
  }

  void init_cmd(string const& cmd) {
    diags = diag_chat::create();
    diags->add_report_raised_handler(print_diagnostic);
    sasl_create_compiler(drv);
    BOOST_REQUIRE(drv);
    drv->set_parameter(cmd);
    for (size_t i = 0; i < vfiles.size(); ++i) {
      drv->add_virtual_file(vfiles[i].first, vfiles[i].second, true);
    }

    shared_ptr<diag_chat> results = drv->compile(true);
    diag_chat::merge(diags.get(), results.get(), true);

    BOOST_REQUIRE(drv->get_root());
    if (drv->get_semantic()) {
      root_sym = drv->get_semantic()->root_symbol();
    }
    BOOST_REQUIRE(!drv->get_vmcode());
  }
  ~jit_fixture() {}

  shared_ptr<compiler> drv;
  symbol* root_sym;
  shared_ptr<diag_chat> diags;
  vector<pair<char const*, char const*>> vfiles;
};

#if ALL_TESTS_ENABLED
BOOST_FIXTURE_TEST_CASE(incomplete, jit_fixture) {
  init_g("repo/incomplete.ss");
}
#endif

#if ALL_TESTS_ENABLED
BOOST_FIXTURE_TEST_CASE(semantic_errors, jit_fixture) {
  init_g("repo/semantic_errors.ss");
}
#endif

#if ALL_TESTS_ENABLED
BOOST_FIXTURE_TEST_CASE(include_test, jit_fixture) {
  const char* virtual_include_content = "float virtual_include_add(float a, float b){ \r\n"
                                        "	return a+b; \r\n"
                                        "} \r\n";
  add_virtual_file("virtual_include.ss", virtual_include_content);
  init_g("repo/include_main.ss");
}
#endif

#if 1 || ALL_TESTS_ENABLED
BOOST_FIXTURE_TEST_CASE(include_dir_test, jit_fixture) {
  fs::path include_path = fs::current_path() / "repo/include";
  fs::path sysincl_path = fs::current_path() / "repo/sysincl";

  BOOST_REQUIRE(fs::is_directory(include_path) && fs::is_directory(sysincl_path));

  std::string file_name("repo/include_search_path.ss");
  std::string cmd =
      make_command(include_path.string(), sysincl_path.string(), file_name, "--lang=g");
  init_cmd(cmd);
}
#endif

#if 1 || ALL_TESTS_ENABLED
BOOST_FIXTURE_TEST_CASE(include_dir_test_need_failed, jit_fixture) {
  fs::path include_path = fs::current_path() / "repo/include";
  fs::path sysincl_path = fs::current_path() / "repo/sysincl";

  BOOST_REQUIRE(fs::is_directory(include_path) && fs::is_directory(sysincl_path));

  std::string file_name("repo/include_search_path.ss");
  std::string cmd =
      make_command(sysincl_path.string(), include_path.string(), file_name, "--lang=g");
  init_cmd(cmd);
}
#endif

BOOST_AUTO_TEST_SUITE_END();
