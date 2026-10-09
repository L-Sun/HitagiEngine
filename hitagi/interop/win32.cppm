module;
#include <Windows.h>
#include <windowsx.h>
#include <psapi.h>
#include <timeapi.h>
#include <objbase.h>
#include <wrl.h>

export module interop.win32;

namespace hitagi::interop {
inline constexpr auto max_path            = MAX_PATH;
inline constexpr auto cfs_candidatepos    = CFS_CANDIDATEPOS;
inline constexpr auto cfs_force_position  = CFS_FORCE_POSITION;
inline constexpr auto cs_hredraw          = CS_HREDRAW;
inline constexpr auto cs_vredraw          = CS_VREDRAW;
inline constexpr auto event_all_access    = EVENT_ALL_ACCESS;
inline constexpr auto gwlp_userdata       = GWLP_USERDATA;
inline constexpr auto htbottom            = HTBOTTOM;
inline constexpr auto htbottomleft        = HTBOTTOMLEFT;
inline constexpr auto htbottomright       = HTBOTTOMRIGHT;
inline constexpr auto htclient            = HTCLIENT;
inline constexpr auto htleft              = HTLEFT;
inline constexpr auto htright             = HTRIGHT;
inline constexpr auto httop               = HTTOP;
inline constexpr auto httopleft           = HTTOPLEFT;
inline constexpr auto httopright          = HTTOPRIGHT;
inline const auto     idc_arrow           = IDC_ARROW;
inline const auto     idc_hand            = IDC_HAND;
inline const auto     idc_ibeam           = IDC_IBEAM;
inline const auto     idc_no              = IDC_NO;
inline const auto     idc_sizeall         = IDC_SIZEALL;
inline const auto     idc_sizenesw        = IDC_SIZENESW;
inline const auto     idc_sizens          = IDC_SIZENS;
inline const auto     idc_sizenwse        = IDC_SIZENWSE;
inline const auto     idc_sizewe          = IDC_SIZEWE;
inline constexpr auto kf_repeat           = KF_REPEAT;
inline constexpr auto pm_remove           = PM_REMOVE;
inline constexpr auto size_minimized      = SIZE_MINIMIZED;
inline constexpr auto sw_hide             = SW_HIDE;
inline constexpr auto sw_maximize         = SW_MAXIMIZE;
inline constexpr auto sw_show             = SW_SHOW;
inline constexpr auto swp_noactivate      = SWP_NOACTIVATE;
inline constexpr auto swp_nozorder        = SWP_NOZORDER;
inline constexpr auto vk_lcontrol         = VK_LCONTROL;
inline constexpr auto vk_lmenu            = VK_LMENU;
inline constexpr auto vk_lshift           = VK_LSHIFT;
inline constexpr auto vk_rcontrol         = VK_RCONTROL;
inline constexpr auto vk_rmenu            = VK_RMENU;
inline constexpr auto vk_rshift           = VK_RSHIFT;
inline constexpr auto wait_timeout        = WAIT_TIMEOUT;
inline constexpr auto wm_char             = WM_CHAR;
inline constexpr auto wm_destroy          = WM_DESTROY;
inline constexpr auto wm_ime_char         = WM_IME_CHAR;
inline constexpr auto wm_keydown          = WM_KEYDOWN;
inline constexpr auto wm_keyup            = WM_KEYUP;
inline constexpr auto wm_killfocus        = WM_KILLFOCUS;
inline constexpr auto wm_lbuttondblclk    = WM_LBUTTONDBLCLK;
inline constexpr auto wm_lbuttondown      = WM_LBUTTONDOWN;
inline constexpr auto wm_lbuttonup        = WM_LBUTTONUP;
inline constexpr auto wm_mbuttondblclk    = WM_MBUTTONDBLCLK;
inline constexpr auto wm_mbuttondown      = WM_MBUTTONDOWN;
inline constexpr auto wm_mbuttonup        = WM_MBUTTONUP;
inline constexpr auto wm_mousehwheel      = WM_MOUSEHWHEEL;
inline constexpr auto wm_mousemove        = WM_MOUSEMOVE;
inline constexpr auto wm_mousewheel       = WM_MOUSEWHEEL;
inline constexpr auto wm_nccreate         = WM_NCCREATE;
inline constexpr auto wm_nchittest        = WM_NCHITTEST;
inline constexpr auto wm_rbuttondblclk    = WM_RBUTTONDBLCLK;
inline constexpr auto wm_rbuttondown      = WM_RBUTTONDOWN;
inline constexpr auto wm_rbuttonup        = WM_RBUTTONUP;
inline constexpr auto wm_size             = WM_SIZE;
inline constexpr auto wm_syskeydown       = WM_SYSKEYDOWN;
inline constexpr auto wm_syskeyup         = WM_SYSKEYUP;
inline constexpr auto wm_xbuttondblclk    = WM_XBUTTONDBLCLK;
inline constexpr auto wm_xbuttondown      = WM_XBUTTONDOWN;
inline constexpr auto ws_overlappedwindow = WS_OVERLAPPEDWINDOW;
inline constexpr auto cw_usedefault       = CW_USEDEFAULT;
inline constexpr auto color_window        = COLOR_WINDOW;
inline constexpr auto wheel_delta         = WHEEL_DELTA;
inline constexpr auto false_value         = FALSE;
inline constexpr auto true_value          = TRUE;
inline constexpr auto infinite            = INFINITE;
inline constexpr auto drive_removable     = DRIVE_REMOVABLE;
inline constexpr auto drive_fixed         = DRIVE_FIXED;
inline constexpr auto drive_remote        = DRIVE_REMOTE;
export inline int   get_x_lparam(LPARAM value) { return GET_X_LPARAM(value); }
export inline int   get_y_lparam(LPARAM value) { return GET_Y_LPARAM(value); }
export inline short get_wheel_delta_wparam(WPARAM value) { return GET_WHEEL_DELTA_WPARAM(value); }
export inline WORD  hiword(DWORD_PTR value) { return HIWORD(value); }
export inline WORD  loword(DWORD_PTR value) { return LOWORD(value); }
export inline void  zero_memory(void* pointer, SIZE_T size) { ZeroMemory(pointer, size); }
}  // namespace hitagi::interop
#undef CFS_CANDIDATEPOS
#undef CFS_FORCE_POSITION
#undef CS_HREDRAW
#undef CS_VREDRAW
#undef EVENT_ALL_ACCESS
#undef GWLP_USERDATA
#undef HTBOTTOM
#undef HTBOTTOMLEFT
#undef HTBOTTOMRIGHT
#undef HTCLIENT
#undef HTLEFT
#undef HTRIGHT
#undef HTTOP
#undef HTTOPLEFT
#undef HTTOPRIGHT
#undef IDC_ARROW
#undef IDC_HAND
#undef IDC_IBEAM
#undef IDC_NO
#undef IDC_SIZEALL
#undef IDC_SIZENESW
#undef IDC_SIZENS
#undef IDC_SIZENWSE
#undef IDC_SIZEWE
#undef KF_REPEAT
#undef PM_REMOVE
#undef SIZE_MINIMIZED
#undef SW_HIDE
#undef SW_MAXIMIZE
#undef SW_SHOW
#undef SWP_NOACTIVATE
#undef SWP_NOZORDER
#undef VK_LCONTROL
#undef VK_LMENU
#undef VK_LSHIFT
#undef VK_RCONTROL
#undef VK_RMENU
#undef VK_RSHIFT
#undef WAIT_TIMEOUT
#undef WM_CHAR
#undef WM_DESTROY
#undef WM_IME_CHAR
#undef WM_KEYDOWN
#undef WM_KEYUP
#undef WM_KILLFOCUS
#undef WM_LBUTTONDBLCLK
#undef WM_LBUTTONDOWN
#undef WM_LBUTTONUP
#undef WM_MBUTTONDBLCLK
#undef WM_MBUTTONDOWN
#undef WM_MBUTTONUP
#undef WM_MOUSEHWHEEL
#undef WM_MOUSEMOVE
#undef WM_MOUSEWHEEL
#undef WM_NCCREATE
#undef WM_NCHITTEST
#undef WM_RBUTTONDBLCLK
#undef WM_RBUTTONDOWN
#undef WM_RBUTTONUP
#undef WM_SIZE
#undef WM_SYSKEYDOWN
#undef WM_SYSKEYUP
#undef WM_XBUTTONDBLCLK
#undef WM_XBUTTONDOWN
#undef WS_OVERLAPPEDWINDOW
#undef CW_USEDEFAULT
#undef COLOR_WINDOW
#undef WHEEL_DELTA
#undef FALSE
#undef TRUE
#undef INFINITE
#undef DRIVE_REMOVABLE
#undef DRIVE_FIXED
#undef DRIVE_REMOTE

#undef MAX_PATH

export {
    inline constexpr auto MAX_PATH = hitagi::interop::max_path;
    using ::AdjustWindowRect;
    using ::BOOL;
    using ::BYTE;
    using ::CANDIDATEFORM;
    using ::ClipCursor;
    using ::CloseHandle;
    using ::CoCreateGuid;
    using ::COMPOSITIONFORM;
    using ::CreateEventExW;
    using ::CREATESTRUCTW;
    using ::CreateWindowExW;
    using ::DefWindowProcW;
    using ::DispatchMessageW;
    using ::DWORD;
    using ::GetClientRect;
    using ::GetCurrentProcess;
    using ::GetDpiForWindow;
    using ::GetDriveTypeA;
    using ::GetKeyState;
    using ::GetLastError;
    using ::GetLogicalDrives;
    using ::GetModuleFileNameA;
    using ::GetModuleHandleW;
    using ::GetProcessMemoryInfo;
    using ::GetWindowLongPtrW;
    using ::GetWindowRect;
    using ::GUID;
    using ::HANDLE;
    using ::HBRUSH;
    using ::HIMC;
    using ::HINSTANCE;
    using ::HRESULT;
    using ::HWND;
    using ::IID_PPV_ARGS_Helper;
    using ::ImmGetContext;
    using ::ImmReleaseContext;
    using ::ImmSetCandidateWindow;
    using ::ImmSetCompositionWindow;
    using ::INT;
    using ::IsWindow;
    using ::IsZoomed;
    using ::LoadCursorW;
    using ::LONG;
    using ::LONG_PTR;
    using ::LPARAM;
    using ::LPCSTR;
    using ::LPTSTR;
    using ::LRESULT;
    using ::MapWindowPoints;
    using ::MSG;
    using ::MulDiv;
    using ::PeekMessageW;
    using ::POINT;
    using ::PostQuitMessage;
    using ::PROCESS_MEMORY_COUNTERS;
    using ::PROCESS_MEMORY_COUNTERS_EX;
    using ::RECT;
    using ::RegisterClassExW;
    using ::ReleaseCapture;
    using ::SetCapture;
    using ::SetCursor;
    using ::SetCursorPos;
    using ::SetLastError;
    using ::SetProcessDPIAware;
    using ::SetWindowLongPtrW;
    using ::SetWindowPos;
    using ::SetWindowTextW;
    using ::ShowWindow;
    using ::SIZE_T;
    using ::timeBeginPeriod;
    using ::TranslateMessage;
    using ::UINT;
    using ::UINT64;
    using ::ULONG;
    using ::WaitForSingleObject;
    using ::WNDCLASSEXW;
    using ::WPARAM;
    inline constexpr auto CFS_CANDIDATEPOS    = hitagi::interop::cfs_candidatepos;
    inline constexpr auto CFS_FORCE_POSITION  = hitagi::interop::cfs_force_position;
    inline constexpr auto CS_HREDRAW          = hitagi::interop::cs_hredraw;
    inline constexpr auto CS_VREDRAW          = hitagi::interop::cs_vredraw;
    inline constexpr auto EVENT_ALL_ACCESS    = hitagi::interop::event_all_access;
    inline constexpr auto GWLP_USERDATA       = hitagi::interop::gwlp_userdata;
    inline constexpr auto HTBOTTOM            = hitagi::interop::htbottom;
    inline constexpr auto HTBOTTOMLEFT        = hitagi::interop::htbottomleft;
    inline constexpr auto HTBOTTOMRIGHT       = hitagi::interop::htbottomright;
    inline constexpr auto HTCLIENT            = hitagi::interop::htclient;
    inline constexpr auto HTLEFT              = hitagi::interop::htleft;
    inline constexpr auto HTRIGHT             = hitagi::interop::htright;
    inline constexpr auto HTTOP               = hitagi::interop::httop;
    inline constexpr auto HTTOPLEFT           = hitagi::interop::httopleft;
    inline constexpr auto HTTOPRIGHT          = hitagi::interop::httopright;
    inline const auto     IDC_ARROW           = hitagi::interop::idc_arrow;
    inline const auto     IDC_HAND            = hitagi::interop::idc_hand;
    inline const auto     IDC_IBEAM           = hitagi::interop::idc_ibeam;
    inline const auto     IDC_NO              = hitagi::interop::idc_no;
    inline const auto     IDC_SIZEALL         = hitagi::interop::idc_sizeall;
    inline const auto     IDC_SIZENESW        = hitagi::interop::idc_sizenesw;
    inline const auto     IDC_SIZENS          = hitagi::interop::idc_sizens;
    inline const auto     IDC_SIZENWSE        = hitagi::interop::idc_sizenwse;
    inline const auto     IDC_SIZEWE          = hitagi::interop::idc_sizewe;
    inline constexpr auto KF_REPEAT           = hitagi::interop::kf_repeat;
    inline constexpr auto PM_REMOVE           = hitagi::interop::pm_remove;
    inline constexpr auto SIZE_MINIMIZED      = hitagi::interop::size_minimized;
    inline constexpr auto SW_HIDE             = hitagi::interop::sw_hide;
    inline constexpr auto SW_MAXIMIZE         = hitagi::interop::sw_maximize;
    inline constexpr auto SW_SHOW             = hitagi::interop::sw_show;
    inline constexpr auto SWP_NOACTIVATE      = hitagi::interop::swp_noactivate;
    inline constexpr auto SWP_NOZORDER        = hitagi::interop::swp_nozorder;
    inline constexpr auto VK_LCONTROL         = hitagi::interop::vk_lcontrol;
    inline constexpr auto VK_LMENU            = hitagi::interop::vk_lmenu;
    inline constexpr auto VK_LSHIFT           = hitagi::interop::vk_lshift;
    inline constexpr auto VK_RCONTROL         = hitagi::interop::vk_rcontrol;
    inline constexpr auto VK_RMENU            = hitagi::interop::vk_rmenu;
    inline constexpr auto VK_RSHIFT           = hitagi::interop::vk_rshift;
    inline constexpr auto WAIT_TIMEOUT        = hitagi::interop::wait_timeout;
    inline constexpr auto WM_CHAR             = hitagi::interop::wm_char;
    inline constexpr auto WM_DESTROY          = hitagi::interop::wm_destroy;
    inline constexpr auto WM_IME_CHAR         = hitagi::interop::wm_ime_char;
    inline constexpr auto WM_KEYDOWN          = hitagi::interop::wm_keydown;
    inline constexpr auto WM_KEYUP            = hitagi::interop::wm_keyup;
    inline constexpr auto WM_KILLFOCUS        = hitagi::interop::wm_killfocus;
    inline constexpr auto WM_LBUTTONDBLCLK    = hitagi::interop::wm_lbuttondblclk;
    inline constexpr auto WM_LBUTTONDOWN      = hitagi::interop::wm_lbuttondown;
    inline constexpr auto WM_LBUTTONUP        = hitagi::interop::wm_lbuttonup;
    inline constexpr auto WM_MBUTTONDBLCLK    = hitagi::interop::wm_mbuttondblclk;
    inline constexpr auto WM_MBUTTONDOWN      = hitagi::interop::wm_mbuttondown;
    inline constexpr auto WM_MBUTTONUP        = hitagi::interop::wm_mbuttonup;
    inline constexpr auto WM_MOUSEHWHEEL      = hitagi::interop::wm_mousehwheel;
    inline constexpr auto WM_MOUSEMOVE        = hitagi::interop::wm_mousemove;
    inline constexpr auto WM_MOUSEWHEEL       = hitagi::interop::wm_mousewheel;
    inline constexpr auto WM_NCCREATE         = hitagi::interop::wm_nccreate;
    inline constexpr auto WM_NCHITTEST        = hitagi::interop::wm_nchittest;
    inline constexpr auto WM_RBUTTONDBLCLK    = hitagi::interop::wm_rbuttondblclk;
    inline constexpr auto WM_RBUTTONDOWN      = hitagi::interop::wm_rbuttondown;
    inline constexpr auto WM_RBUTTONUP        = hitagi::interop::wm_rbuttonup;
    inline constexpr auto WM_SIZE             = hitagi::interop::wm_size;
    inline constexpr auto WM_SYSKEYDOWN       = hitagi::interop::wm_syskeydown;
    inline constexpr auto WM_SYSKEYUP         = hitagi::interop::wm_syskeyup;
    inline constexpr auto WM_XBUTTONDBLCLK    = hitagi::interop::wm_xbuttondblclk;
    inline constexpr auto WM_XBUTTONDOWN      = hitagi::interop::wm_xbuttondown;
    inline constexpr auto WS_OVERLAPPEDWINDOW = hitagi::interop::ws_overlappedwindow;
    inline constexpr auto CW_USEDEFAULT       = hitagi::interop::cw_usedefault;
    inline constexpr auto COLOR_WINDOW        = hitagi::interop::color_window;
    inline constexpr auto WHEEL_DELTA         = hitagi::interop::wheel_delta;
    inline constexpr auto FALSE               = hitagi::interop::false_value;
    inline constexpr auto TRUE                = hitagi::interop::true_value;
    inline constexpr auto INFINITE            = hitagi::interop::infinite;
    inline constexpr auto DRIVE_REMOVABLE     = hitagi::interop::drive_removable;
    inline constexpr auto DRIVE_FIXED         = hitagi::interop::drive_fixed;
    inline constexpr auto DRIVE_REMOTE        = hitagi::interop::drive_remote;
}
