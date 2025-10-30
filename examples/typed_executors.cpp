#include <Windows.h>
#include <winternl.h>
#include <thread>

#include <cstring>

#define SHADOW_RELAXED_POINTER_COMPAT  // allows implicit pointer conversions
#include "shadowsyscall.hpp"

typedef void(CALLBACK* PRTL_THREAD_START_ROUTINE)(LPVOID);
typedef NTSTATUS(WINAPI* NtCreateThreadEx)(HANDLE*, ACCESS_MASK, OBJECT_ATTRIBUTES*, HANDLE,
                                           PRTL_THREAD_START_ROUTINE, void*, ULONG, ULONG_PTR,
                                           SIZE_T, SIZE_T, void*);

int main() {
  shadowcall<LoadLibrary>("user32");

  void* thread_handle;
  auto current_process = GetCurrentProcess();
  auto start_routine = [](void*) -> DWORD {
    std::cout << "Hello from thread " << std::this_thread::get_id() << "\n";
    return 0;
  };

  int return_code = shadowcall<int>("MessageBoxA", nullptr, "Text", "Caption 1", MB_OK);
  return_code = shadowcall<int>({"MessageBoxA", "user32"}, NULL, "Text", "Caption 2", MB_OK);
  return_code = shadowcall<MessageBoxA>(NULL, "Text", "Caption 3", MB_OK);
  return_code = shadowcall<MessageBoxA, "user32">(0, "Text", "Caption 4", MB_OK);

  // works on WinAPI defines
  // (FindWindow is a define that expands into FindWindowA depending on the encoding)
  HWND window = shadowcall<FindWindow>("", "");

  NTSTATUS status;
#if _WIN64
  // if the given template is a type rather than a linkable symbol - we
  // cannot deduce the name of this type (at least without reflection),
  // so the function name will have to be duplicated in the parameter list
  status = shadowsyscall<NtCreateThreadEx>(
      "NtCreateThreadEx", &thread_handle, THREAD_ALL_ACCESS, NULL, current_process,
      static_cast<LPTHREAD_START_ROUTINE>(start_routine), 0, FALSE, NULL, NULL, NULL, 0);

  status = shadowsyscall<NtClose>(thread_handle);
#endif

  status = shadowcall<NtCreateThreadEx>(
      "NtCreateThreadEx", &thread_handle, THREAD_ALL_ACCESS, NULL, current_process,
      static_cast<LPTHREAD_START_ROUTINE>(start_routine), 0, FALSE, NULL, NULL, NULL, 0);

  status = shadowcall<NtCreateThreadEx>(
      {"NtCreateThreadEx", "ntdll.dll"}, &thread_handle, THREAD_ALL_ACCESS, NULL, current_process,
      static_cast<LPTHREAD_START_ROUTINE>(start_routine), 0, FALSE, NULL, NULL, NULL, 0);

  shadowcall<Sleep>(1000);

  // APIs with a calling convention other than __stdcall
  // are not available on x86. memcpy = CRT = __cdecl
#if _WIN64
  int* val_arr = new int[4];
  std::array<int, 4> filled_val_arr{1, 2, 3, 4};

  shadowcall<std::memcpy>(val_arr, filled_val_arr.data(), filled_val_arr.size() * sizeof(int));
  std::cout << "val_arr[3] = " << val_arr[3] << "\n";
#endif
}
