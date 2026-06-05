#include "lldb/API/LLDB.h"

#include <cstdio>

using namespace lldb;

namespace
{
const char* nz(const char* s) { return s ? s : "<null>"; }

const char* state_name(StateType s)
{
    switch (s)
    {
        case eStateInvalid: return "invalid";
        case eStateUnloaded: return "unloaded";
        case eStateConnected: return "connected";
        case eStateAttaching: return "attaching";
        case eStateLaunching: return "launching";
        case eStateStopped: return "stopped";
        case eStateRunning: return "running";
        case eStateStepping: return "stepping";
        case eStateCrashed: return "crashed";
        case eStateDetached: return "detached";
        case eStateExited: return "exited";
        case eStateSuspended: return "suspended";
    }
    return "?";
}
}  // namespace

int main(int argc, char** argv)
{
    const char* target_path = argc > 1 ? argv[1] : "dans_vk_debugger_sample.exe";

    SBError error = SBDebugger::InitializeWithErrorHandling();
    if (error.Fail())
    {
        std::printf("Initialize failed: %s\n", nz(error.GetCString()));
        return 1;
    }

    SBDebugger debugger = SBDebugger::Create();
    debugger.SetAsync(false);

    SBTarget target = debugger.CreateTarget(target_path);
    if (!target.IsValid())
    {
        std::printf("could not create target from '%s'\n", target_path);
        return 1;
    }
    std::printf("target: %s [%s]\n", target_path, nz(target.GetTriple()));

    SBBreakpoint bp = target.BreakpointCreateByName("add", target.GetExecutable().GetFilename());
    std::printf("breakpoint on 'add' -> %zu location(s)\n", bp.GetNumLocations());

    SBLaunchInfo launch_info(nullptr);
    SBProcess process = target.Launch(launch_info, error);
    if (!process.IsValid() or error.Fail())
    {
        std::printf("launch failed: %s\n", nz(error.GetCString()));
        return 1;
    }
    std::printf(
        "launched pid %llu, state = %s\n",
        static_cast<unsigned long long>(process.GetProcessID()),
        state_name(process.GetState())
    );

    SBThread thread = process.GetSelectedThread();
    SBFrame frame = thread.GetSelectedFrame();
    std::printf("stopped in: %s\n", nz(frame.GetFunctionName()));

    SBValue a = frame.FindVariable("a");
    SBValue b = frame.FindVariable("b");
    std::printf("  %s = %s\n", nz(a.GetName()), nz(a.GetValue()));
    std::printf("  %s = %s\n", nz(b.GetName()), nz(b.GetValue()));

    thread.StepOver();
    frame = thread.GetSelectedFrame();
    SBValue s = frame.FindVariable("s");
    std::printf("after step: %s = %s\n", nz(s.GetName()), nz(s.GetValue()));

    process.Continue();
    std::printf(
        "final state = %s, exit = %d\n", state_name(process.GetState()), process.GetExitStatus()
    );

    SBDebugger::Destroy(debugger);
    SBDebugger::Terminate();
    return 0;
}
