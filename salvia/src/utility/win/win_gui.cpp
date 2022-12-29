#include <salvia/utility/win/win_gui.h>

#include "resource.h"

#include <salvia/utility/common/window.h>

#include <eflib/platform/constant.h>

#include <boost/signals2.hpp>

#include <sstream>
#include <string>

#if defined(EFLIB_WINDOWS)
#include <Windows.h>

using boost::signals2::signal;
using std::string;

namespace salvia::utility {

class win_gui;
static win_gui *g_gui = nullptr;

class win_window : public window {
  signal<void()> on_idle;
  signal<void()> on_paint;
  signal<void()> on_create;

public:
  win_window(win_gui *app) : app_(app), hwnd_(nullptr) {}

  void show() { ShowWindow(hwnd_, SW_SHOWDEFAULT); }

  void set_idle_handler(idle_handler_t const &handler) { on_idle.connect(handler); }

  void set_draw_handler(draw_handler_t const &handler) { on_paint.connect(handler); }

  void set_create_handler(create_handler_t const &handler) { on_create.connect(handler); }

  void set_title(string const &title) { SetWindowText(hwnd_, eflib::to_tstring(title).c_str()); }

  void *view_handle_as_void() override { return reinterpret_cast<void *>(hwnd_); }

  std::any view_handle() override { return std::any(hwnd_); }

  void idle() { on_idle(); }

  void refresh() { InvalidateRect(hwnd_, nullptr, FALSE); }

  bool create(uint32_t width, uint32_t height);

private:
  ATOM register_window_class(HINSTANCE hinst) {
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = &win_proc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = GetModuleHandle(nullptr);
    wcex.hIcon = LoadIcon(hinst, MAKEINTRESOURCE(IDI_APP));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = wnd_class_name_;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_APP));

    return RegisterClassEx(&wcex);
  }

  LRESULT process_message(UINT message, WPARAM wparam, LPARAM lparam) {
    PAINTSTRUCT ps;
    HDC hdc;

    switch (message) {
    case WM_CREATE:
      on_create();
      break;
    case WM_PAINT:
      hdc = BeginPaint(hwnd_, &ps);
      on_paint();
      EndPaint(hwnd_, &ps);
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    default:
      return DefWindowProc(hwnd_, message, wparam, lparam);
    }
    return 0;
  }

  static LRESULT CALLBACK win_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
  static ATOM wnd_class_;
  static std::_tchar const *wnd_class_name_;
  HWND hwnd_;
  win_gui *app_;
};

ATOM win_window::wnd_class_ = 0;
std::_tchar const *win_window::wnd_class_name_ = _EFLIB_T("SalviaApp");

class win_gui : public gui {
public:
  win_gui() {
    ::DefWindowProc(nullptr, 0, 0, 0L);
    hinst_ = GetModuleHandle(nullptr);
    main_wnd_ = new win_window(this);
  }

  ~win_gui() { delete main_wnd_; }

  window *main_window() { return main_wnd_; }

  HINSTANCE instance() const { return hinst_; }

  int create_window(uint32_t width, uint32_t height) override {
    if (!main_wnd_->create(width, height)) {
      OutputDebugString(_EFLIB_T("Main window creation failed!\n"));
      return 0;
    }
    return 1;
  }

  int run() {
    // Message loop.
    main_wnd_->show();

    MSG msg;
    for (;;) {
      if (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (WM_QUIT == msg.message) {
          break; // WM_QUIT, exit message loop
        }

        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
      } else {
        main_wnd_->idle();
      }
    }

    return (int)msg.wParam;
  }

private:
  win_window *main_wnd_;
  HINSTANCE hinst_;
};

bool win_window::create(uint32_t width, uint32_t height) {
  if (wnd_class_ == 0) {
    wnd_class_ = register_window_class(app_->instance());
  }
  DWORD style = WS_OVERLAPPEDWINDOW;
  RECT rc = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
  AdjustWindowRect(&rc, style, false);
  hwnd_ = CreateWindow(wnd_class_name_, _EFLIB_T(""), style, CW_USEDEFAULT, 0, rc.right - rc.left,
                       rc.bottom - rc.top, nullptr, nullptr, app_->instance(), nullptr);
  if (!hwnd_) {
    auto err = GetLastError();

    LPTSTR lpMsgBuf;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, nullptr);

    // Display the error message and exit the process
    std::wstringstream ss;
    ss << L"Window created failed. Error: " << std::hex
       << eflib::to_wide_string(std::_tstring(lpMsgBuf)) << ".";
    OutputDebugStringW(ss.str().c_str());
    LocalFree(lpMsgBuf);
    return false;
  }

  ShowWindow(hwnd_, SW_SHOW);
  UpdateWindow(hwnd_);

  return true;
}

LRESULT CALLBACK win_window::win_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  win_window *wnd = static_cast<win_window *>(g_gui->main_window());
  wnd->hwnd_ = hwnd;
  return wnd->process_message(message, wparam, lparam);
}

}
#endif

namespace salvia::utility {
gui *create_win_gui() {
#if defined(EFLIB_WINDOWS)
  if (g_gui) {
    assert(false);
    exit(1);
  }
  g_gui = new win_gui();
  return g_gui;
#else
  return nullptr;
#endif
}

void delete_win_gui(gui *app) {
#if defined(EFLIB_WINDOWS)
  if (app == nullptr) {
    return;
  }

  ef_verify(dynamic_cast<win_gui *>(app) != nullptr);

  delete app;
#endif
  // Do nothing.
  (void)app;
}
}
