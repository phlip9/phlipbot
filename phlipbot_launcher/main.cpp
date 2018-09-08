#include <Windows.h>
#include <threadpoolapiset.h>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <boost/optional.hpp>
#include <boost/program_options.hpp>

#include <hadesmem/detail/self_path.hpp>
#include <hadesmem/detail/smart_handle.hpp>

#include <hadesmem/config.hpp>
#include <hadesmem/debug_privilege.hpp>
#include <hadesmem/injector.hpp>
#include <hadesmem/module.hpp>
#include <hadesmem/process.hpp>
#include <hadesmem/process_helpers.hpp>

// TODO(phlip9): take absolute path for dll_name
// TODO(phlip9): move SmartHandles into hadesmem/detail/smart_handle.hpp

namespace fs = std::filesystem;
namespace po = boost::program_options;

using std::condition_variable;
using std::lock_guard;
using std::mutex;
using std::unique_lock;

using std::cerr;
using std::cout;
using std::move;
using std::wcerr;
using std::wcout;

using std::map;
using std::vector;
using std::wstring;

using boost::make_optional;
using boost::none;
using boost::optional;

using hadesmem::CallResult;
using hadesmem::CallResult;
using hadesmem::ErrorCodeWinLast;
using hadesmem::ErrorString;
using hadesmem::FreeDll;
using hadesmem::GetProcessByName;
using hadesmem::GetSeDebugPrivilege;
using hadesmem::InjectDll;
using hadesmem::InjectFlags;
using hadesmem::Module;
using hadesmem::Process;
using hadesmem::detail::PtrToHexString;
using hadesmem::detail::SmartHandleImpl;
using hadesmem::detail::SmartLocalFreeHandle;

namespace
{
using CmdHandlerT = std::function<int(po::variables_map const&)>;

struct CallbackEnvironmentPolicy {
  using HandleT = PTP_CALLBACK_ENVIRON;

  static constexpr HandleT GetInvalid() noexcept { return nullptr; }

  static bool Cleanup(HandleT handle)
  {
    ::DestroyThreadpoolEnvironment(handle);
    return true;
  }
};
using SmartCallbackEnvironment = SmartHandleImpl<CallbackEnvironmentPolicy>;


struct ThreadpoolPolicy {
  using HandleT = PTP_POOL;

  static constexpr HandleT GetInvalid() noexcept { return nullptr; }

  static bool Cleanup(HandleT handle)
  {
    std::wcout << "CloseThreadpool(handle)\n";
    ::CloseThreadpool(handle);
    return true;
  }
};
using SmartThreadpool = SmartHandleImpl<ThreadpoolPolicy>;


struct CleanupGroupPolicy {
  using HandleT = PTP_CLEANUP_GROUP;

  static constexpr HandleT GetInvalid() noexcept { return nullptr; }

  static bool Cleanup(HandleT handle)
  {
    ::CloseThreadpoolCleanupGroup(handle);
    return true;
  }
};
using SmartCleanupGroup = SmartHandleImpl<CleanupGroupPolicy>;


struct ThreadpoolWaitPolicy {
  using HandleT = PTP_WAIT;

  static constexpr HandleT GetInvalid() noexcept { return nullptr; }

  static bool Cleanup(HandleT handle)
  {
    // Unregister the wait first by setting the event to nullptr
    ::SetThreadpoolWait(handle, nullptr, nullptr);
    // Then close it
    ::CloseThreadpoolWait(handle);
    return true;
  }
};
using SmartThreadpoolWait = SmartHandleImpl<ThreadpoolWaitPolicy>;


struct ChangeNotificationPolicy {
  using HandleT = HANDLE;

  static constexpr HandleT GetInvalid() noexcept
  {
    return INVALID_HANDLE_VALUE;
  }

  static bool Cleanup(HandleT handle)
  {
    return ::FindCloseChangeNotification(handle);
  }
};
using SmartChangeNotification = SmartHandleImpl<ChangeNotificationPolicy>;


optional<Process> get_proc_from_options(po::variables_map const& vm)
{
  try {
    if (vm.count("pid")) {
      DWORD const pid = vm["pid"].as<int>();
      return make_optional<Process>(Process{pid});
    } else {
      auto const& pname = vm["pname"].as<wstring>();
      return make_optional<Process>(GetProcessByName(pname));
    }
  } catch (hadesmem::Error const&) {
    // TODO(phlip9): possible false positives? check ErrorString?
    // TODO(phlip9): don't consume errors?
    return none;
  }
}

DWORD_PTR call_dll_load(Process const& proc, HMODULE const mod)
{
  // call remote Unload() function
  CallResult<DWORD_PTR> const load_res = CallExport(proc, mod, "Load");
  return load_res.GetReturnValue();
}

DWORD_PTR call_dll_unload(hadesmem::Process const& proc, HMODULE const mod)
{
  // call remote Unload() function
  CallResult<DWORD_PTR> const load_res = CallExport(proc, mod, "Unload");
  return load_res.GetReturnValue();
}

optional<Module const>
get_injected_dll(Process const& proc, wstring const& dll_name)
{
  // TODO(phlip9): use more robust manual iteration through module list
  try {
    return Module{proc, dll_name};
  } catch (hadesmem::Error const&) {
    // TODO(phlip9): possible false positives? check ErrorString?
    // TODO(phlip9): don't consume errors?
    return none;
  }
}

HMODULE inject_dll(Process const& proc, wstring const& dll_name)
{
  uint32_t flags = 0;
  flags |= InjectFlags::kPathResolution;
  flags |= InjectFlags::kAddToSearchOrder;
  return InjectDll(proc, dll_name, flags);
}

void eject_dll(Process const& proc, wstring const& dll_name)
{
  auto const o_mod = get_injected_dll(proc, dll_name);
  if (o_mod) {
    auto const res = call_dll_unload(proc, o_mod->GetHandle());
    wcout << "Called bot dll's Unload() function\n";
    wcout << "Return value = " << res << "\n";

    FreeDll(proc, o_mod->GetHandle());
    wcout << "Free'd the bot dll.\n";
  }
}

void reload_dll(Process const& proc, wstring const& dll_name)
{
  // Unload() a currently loaded dll
  eject_dll(proc, dll_name);

  // inject a fresh dll
  auto const mod = inject_dll(proc, dll_name);

  wcout << "Successfully injected bot dll at base address = "
        << PtrToHexString(mod) << "\n";

  // call remote Load() function
  auto const res = call_dll_load(proc, mod);

  wcout << "Called bot dll's Load() function\n";
  wcout << "Return value = " << res << "\n";
}


struct WatchContext {
  // is_done is used from a wait callback thread to signal the main thread to
  // cleanup and exit
  mutex is_done_mutex;
  condition_variable is_done_condvar;
  bool is_done = false;
  HANDLE main_thread = nullptr;

  // ensure that the file change callback is a critical section
  mutex watch_mutex;
  SmartChangeNotification dir_watch_handle;
  fs::path copy_dll_path;
  wstring copy_dll_name;
  fs::path orig_dll_path;
  wstring orig_dll_name;
  DWORD proc_id = 0;
  fs::file_time_type orig_dll_last_modified;
};

// TODO(phlip9): unfortunately, the signal handler takes no auxilliary arguments
//               so we have to pass it via a global (eww...)
// WARNING: Don't use this function outside of the signal handler and handler
// setup
WatchContext& signal_handler_get_watch_ctx() noexcept
{
  static WatchContext watch_ctx;
  return watch_ctx;
}

VOID CALLBACK dir_change_cb(PTP_CALLBACK_INSTANCE /* cb_inst */,
                            PVOID param,
                            PTP_WAIT wait,
                            TP_WAIT_RESULT wait_res)
{
  auto ctx = static_cast<WatchContext*>(param);

  wcout << "dir_change_cb(...)\n";

  try {
    if (wait_res == WAIT_FAILED) {
      HADESMEM_DETAIL_THROW_EXCEPTION(
        hadesmem::Error{} << ErrorString{"caught WAIT_FAILED in wait callback"}
                          << ErrorCodeWinLast{::GetLastError()});
    }

    {
      lock_guard<mutex> lock{ctx->watch_mutex};

      if (fs::exists(ctx->orig_dll_path)) {
        auto const orig_dll_last_modified =
          fs::last_write_time(ctx->orig_dll_path);

        // dll was modified -- reload the dll
        if (orig_dll_last_modified > ctx->orig_dll_last_modified) {
          Process proc{ctx->proc_id};
          wcout << "Unloading dll...\n";
          eject_dll(proc, ctx->copy_dll_name);

          wcout << "Copying " << ctx->orig_dll_name << " -> "
                << ctx->copy_dll_name << "\n";
          fs::copy_file(ctx->orig_dll_path, ctx->copy_dll_path,
                        fs::copy_options::overwrite_existing);

          wcout << "Reloading dll...\n";
          reload_dll(proc, ctx->copy_dll_name);
        }

        ctx->orig_dll_last_modified = orig_dll_last_modified;
      } else {
        wcout << "dll does not exist\n";
      }

      if (!::FindNextChangeNotification(ctx->dir_watch_handle.GetHandle())) {
        HADESMEM_DETAIL_THROW_EXCEPTION(
          hadesmem::Error{} << ErrorString{"FindNextChangeNotification failed"}
                            << ErrorCodeWinLast{::GetLastError()});
      }

      // reset the wait to the updated notifier event
      ::SetThreadpoolWait(wait, ctx->dir_watch_handle.GetHandle(), nullptr);
    }
  } catch (...) {
    cerr << boost::current_exception_diagnostic_information() << "\n";

    // Signal main thread to exit
    {
      lock_guard<mutex> lock{ctx->is_done_mutex};
      ctx->is_done = true;
    }
    ctx->is_done_condvar.notify_all();
  }
}

VOID CALLBACK proc_exit_cb(PTP_CALLBACK_INSTANCE /* cb_inst */,
                           PVOID param,
                           PTP_WAIT /* wait */,
                           TP_WAIT_RESULT /* wait_res */)
{
  auto ctx = static_cast<WatchContext*>(param);

  // Signal main thread to exit
  {
    lock_guard<mutex> lock{ctx->is_done_mutex};
    ctx->is_done = true;
  }
  ctx->is_done_condvar.notify_all();
}

// Handle <CTRL-C> and window close signal so we can gracefully cleanup before
// exiting the process.
BOOL WINAPI signal_handler(_In_ DWORD ctrl_type)
{
  switch (ctrl_type) {
  case CTRL_C_EVENT:
  case CTRL_BREAK_EVENT:
  case CTRL_CLOSE_EVENT: {
    auto& ctx = signal_handler_get_watch_ctx();

    wcout << "Caught signal handler, notifying watch thread...\n";

    // Signal main thread to exit
    {
      lock_guard<mutex> lock{ctx.is_done_mutex};
      ctx.is_done = true;
    }
    ctx.is_done_condvar.notify_all();

    // Try to wait for the main thread to finish cleanup, else forcefully
    // terminate if we pass the timemout threshold.
    if (ctx.main_thread != nullptr) {
      ::WaitForSingleObject(ctx.main_thread, 20000 /* 20 sec timeout */);
      wcout << "main thread failed to complete before timeout; "
               "terminating process.\n";
    }

    break;
  }
  default:
    break;
  }

  // ensure the process gets terminated by the default handler
  return FALSE;
}

struct Threadpool {
public:
  Threadpool() = default;
  ~Threadpool() = default;

  Threadpool(Threadpool const& other) = delete;
  Threadpool& operator=(Threadpool const& other) = delete;
  Threadpool(Threadpool&& other) noexcept = delete;
  Threadpool& operator=(Threadpool&& other) noexcept = delete;

  void Init()
  {
    // Create a new threadpool with 1 thread for listening to events
    ::InitializeThreadpoolEnvironment(&_cb_env);
    cb_env = SmartCallbackEnvironment{&_cb_env};

    pool = SmartThreadpool{::CreateThreadpool(nullptr)};

    if (!pool.IsValid()) {
      HADESMEM_DETAIL_THROW_EXCEPTION(
        hadesmem::Error{} << ErrorString{"Failed to create Threadpool"}
                          << ErrorCodeWinLast{::GetLastError()});
    }

    ::SetThreadpoolThreadMaximum(pool.GetHandle(), 1);

    if (!::SetThreadpoolThreadMinimum(pool.GetHandle(), 1)) {
      HADESMEM_DETAIL_THROW_EXCEPTION(
        hadesmem::Error{} << ErrorString{"Failed to set min thread count"}
                          << ErrorCodeWinLast{::GetLastError()});
    }

    ::SetThreadpoolCallbackPool(cb_env.GetHandle(), pool.GetHandle());
  }

  void RegisterWait(HANDLE event_handle, PTP_WAIT_CALLBACK cb, void* cb_data)
  {
    auto wait = SmartThreadpoolWait{
      ::CreateThreadpoolWait(cb, cb_data, cb_env.GetHandle())};

    if (!wait.IsValid()) {
      HADESMEM_DETAIL_THROW_EXCEPTION(
        hadesmem::Error{} << ErrorString{"Failed to create wait object"}
                          << ErrorCodeWinLast{::GetLastError()});
    }

    ::SetThreadpoolWait(wait.GetHandle(), event_handle, nullptr);

    waiters.emplace_back(move(wait));
  }

  // block until all active wait callbacks complete
  void WaitForCallbacks()
  {
    for (auto const& wait : waiters) {
      ::WaitForThreadpoolWaitCallbacks(
        wait.GetHandle(), FALSE /* don't cancel pending callbacks */);
    }
  }

  TP_CALLBACK_ENVIRON _cb_env{};
  SmartCallbackEnvironment cb_env;
  SmartThreadpool pool;

  vector<SmartThreadpoolWait> waiters;
};


int cmd_handler_inject(po::variables_map const& vm)
{
  // need privileges to inject
  GetSeDebugPrivilege();

  // get the WoW process handle
  auto const o_proc = get_proc_from_options(vm);

  // TODO(phlip9): optionally start new process if no existing process
  //               see hadesmem::CreateAndInject
  if (!o_proc) {
    wcerr << "Error: process not running.\n";
    return 1;
  }

  wcout << "Process id = " << o_proc->GetId() << "\n";

  auto const& dll_name = vm["dll"].as<wstring>();

  // do nothing if dll already injected
  if (get_injected_dll(o_proc.value(), dll_name)) {
    wcerr << "Error: " << dll_name
          << " already injected, please eject first.\n";
    return 1;
  }

  // inject bot dll into WoW process
  HMODULE const mod = inject_dll(*o_proc, dll_name);

  wcout << "Successfully injected bot dll at base address = "
        << PtrToHexString(mod) << "\n";

  // call remote Load() function
  auto const res = call_dll_load(*o_proc, mod);

  wcout << "Called bot dll's Load() function\n";
  wcout << "Return value = " << res << "\n";

  return 0;
}

int cmd_handler_eject(po::variables_map const& vm)
{
  // need privileges to eject
  GetSeDebugPrivilege();

  // get the WoW process handle
  auto const o_proc = get_proc_from_options(vm);
  if (!o_proc) {
    wcerr << "Error: process not running.\n";
    return 1;
  }

  wcout << "Process id = " << o_proc->GetId() << "\n";

  auto const& dll_name = vm["dll"].as<wstring>();

  // get the phlipbot.dll handle in the WoW process
  eject_dll(o_proc.value(), dll_name);

  return 0;
}

int cmd_handler_watch(po::variables_map const& vm)
{
  // need privileges to inject/eject
  hadesmem::GetSeDebugPrivilege();

  // get the WoW process handle
  auto const o_proc = get_proc_from_options(vm);

  // TODO(phlip9): optionally start new process if no existing process
  //               see hadesmem::CreateAndInject
  if (!o_proc) {
    wcerr << "Error: process not running.\n";
    return 1;
  }

  wcout << "Process id = " << o_proc->GetId() << "\n";

  // Use the global as the source of truth, but always try to pass it as a
  // parameter to handlers that accept parameters.
  WatchContext& ctx = signal_handler_get_watch_ctx();

  // add this thread so the signal handler can wait for cleanup to complete
  ctx.main_thread = ::GetCurrentThread();
  ctx.proc_id = o_proc->GetId();

  auto const dir_path = fs::absolute(hadesmem::detail::GetSelfDirPath());

  ctx.orig_dll_name = vm["dll"].as<wstring>();
  ctx.orig_dll_path = dir_path / fs::path{ctx.orig_dll_name};

  if (!fs::exists(ctx.orig_dll_path)) {
    HADESMEM_DETAIL_THROW_EXCEPTION(hadesmem::Error{}
                                    << ErrorString{"Dll does not exist"});
  }

  ctx.copy_dll_name = ctx.orig_dll_path.stem().wstring() + L"_copy" +
                      ctx.orig_dll_path.extension().wstring();
  ctx.copy_dll_path = dir_path / fs::path{ctx.copy_dll_name};

  // create a copy of the dll and inject the copy instead of the original
  // so the build system can actually write the dll target
  fs::copy_file(ctx.orig_dll_path, ctx.copy_dll_path,
                fs::copy_options::overwrite_existing);

  if (!fs::exists(ctx.copy_dll_path)) {
    HADESMEM_DETAIL_THROW_EXCEPTION(hadesmem::Error{}
                                    << ErrorString{"Failed to copy dll"});
  }

  // record the last modified time so we know when the dll has been rebuilt
  ctx.orig_dll_last_modified = fs::last_write_time(ctx.orig_dll_path);

  // eject both phlipbot.dll and phlipbot_copy.dll (if either are injected) and
  // then load phlipbot_copy.dll
  eject_dll(o_proc.value(), ctx.orig_dll_name);
  reload_dll(o_proc.value(), ctx.copy_dll_name);

  // register for change notifications to the launcher directory
  ctx.dir_watch_handle = SmartChangeNotification{::FindFirstChangeNotificationW(
    dir_path.c_str(), FALSE /* recursive */,
    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE)};

  if (!ctx.dir_watch_handle.IsValid()) {
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error{} << ErrorString{"Failed to set directory change handle"}
                        << ErrorCodeWinLast{::GetLastError()});
  }


  // Create a new threadpool with 1 thread for listening to events
  Threadpool thread_pool;
  thread_pool.Init();

  ::SetConsoleCtrlHandler(signal_handler, TRUE);
  wcout << "Waiting on process signals...\n";

  thread_pool.RegisterWait(o_proc->GetHandle(), proc_exit_cb, &ctx);
  wcout << "Waiting on WoW.exe process exit...\n";

  thread_pool.RegisterWait(ctx.dir_watch_handle.GetHandle(), dir_change_cb,
                           &ctx);
  wcout << "Waiting on file changes...\n";

  // wait until ctx.is_done flag is triggered from one of the pool threads
  {
    unique_lock<mutex> lock{ctx.is_done_mutex};
    ctx.is_done_condvar.wait(lock, [&ctx] { return ctx.is_done; });
  }

  thread_pool.WaitForCallbacks();

  eject_dll(o_proc.value(), ctx.copy_dll_name);

  return 0;
}

int main_inner(int argc, wchar_t** argv)
{
  map<wstring, CmdHandlerT> const cmd_handlers{{L"inject", cmd_handler_inject},
                                               {L"eject", cmd_handler_eject},
                                               {L"watch", cmd_handler_watch}};

  po::options_description desc("phlipbot_launcher [inject|eject|watch]");
  auto add = desc.add_options();
  add("help", "print usage");
  add("dll,d",
      po::wvalue<wstring>()->default_value(L"phlipbot.dll", "phlipbot.dll"),
      "filename or path to the dll");
  add("pid,p", po::value<int>(), "target process pid");
  add("pname,n", po::wvalue<wstring>()->default_value(L"WoW.exe", "WoW.exe"),
      "target process name");
  add("command", po::wvalue<wstring>()->default_value(L"watch", "watch"),
      "inject|eject|watch");

  po::positional_options_description pos_desc;
  pos_desc.add("command", 1);

  po::wcommand_line_parser parser(argc, argv);
  parser.options(desc);
  parser.positional(pos_desc);

  po::variables_map vm;
  po::store(parser.run(), vm);
  po::notify(vm);

  if (vm.count("help")) {
    cout << desc << "\n";
    return 0;
  }

  if (vm.count("command")) {
    auto const& command = vm["command"].as<wstring>();

    if (cmd_handlers.count(command) != 1) {
      wcerr << "Error: invalid command: \"" << command << "\"\n\n";
      cout << desc << "\n";
      return 1;
    }

    auto const& cmd_handler = cmd_handlers.at(command);
    return cmd_handler(vm);
  }

  wcerr << "Error: missing command\n\n";
  cout << desc << "\n";

  return 1;
}
}

int main()
{
  try {
    // Get the command line arguments as wide strings
    int argc;
    wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    SmartLocalFreeHandle smart_argv{argv};

    if (argv == nullptr) {
      wcerr << "Error: CommandLineToArgvW failed\n";
      return 1;
    }

    return main_inner(argc, argv);
  } catch (...) {
    cerr << boost::current_exception_diagnostic_information() << "\n";
    return 1;
  }
}
