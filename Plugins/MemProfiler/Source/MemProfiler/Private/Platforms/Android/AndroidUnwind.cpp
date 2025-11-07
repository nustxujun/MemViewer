#if PLATFORM_ANDROID
#include "AndroidUnwind.h"

// #include "unwindstack/Unwinder.h"
// #include "unwindstack/Regs.h"
// #include "unwindstack/RegsArm.h"
// #include "unwindstack/RegsArm64.h"
// #include "unwindstack/Maps.h"
// #include "MemoryLocal.h"
// #include "unwindstack/RegsGetLocal.h"

// #define Ensure(x) if (!(x)) {__builtin_trap();}; 
PRAGMA_DISABLE_OPTIMIZATION
int CaptureStack(unsigned long long * BackTrace, unsigned int MaxDepth)
{
    // struct LocalMaps
    // {
    //     unwindstack::LocalMaps local_maps;
    //     LocalMaps()
    //     {
    //         Ensure(local_maps.Parse());
    //     }
    // };

	// static LocalMaps local_maps;
	// static std::shared_ptr<unwindstack::Memory> memory_local = std::make_shared<unwindstack::MemoryLocal>();

	// unwindstack::RegsArm64 regs;
	// RegsGetLocal(&regs);

	// static unwindstack::Unwinder unwinder(MaxDepth, &local_maps.local_maps,memory_local);
	// auto& frames = unwinder.frames();
	// ((std::vector<unwindstack::FrameData>&)frames).clear();
	// //auto jit_debug = wechat_backtrace::GetJitDebug(process_memory);
	// //unwinder.SetJitDebug(jit_debug.get(), regs->Arch());
	// unwinder.SetResolveNames(false);
	// unwinder.SetRegs(&regs);
	// unwinder.Unwind();

    // const int skip_num = 3;
    // auto count = frames.size() - skip_num;
    // auto frame = frames.data() + skip_num;
    // int idx = 0;
    // for (; idx < count; ++idx)
    // {
    //     if (frame->is_dex_pc)
    //         break;
    //     BackTrace[idx] = frame->pc;
    // }

	// return idx;

    typedef int (*UnwindType)(unsigned long long * BackTrace, unsigned int MaxDepth);
    struct GetUnwindFunc
    {
        GetUnwindFunc()
        {
            void* Module = dlopen("libunwindstack.so", 0);
            if (Module == 0)
            {
                __builtin_trap();
            }
            UnwindFunc = (UnwindType)dlsym(Module, "EasyUnwind");
            if (UnwindFunc == 0) 
            {
                __builtin_trap();
            }
        }

        UnwindType UnwindFunc;
        inline int Unwind(unsigned long long * BackTrace, unsigned int MaxDepth)
        {
            return UnwindFunc(BackTrace, MaxDepth);  
        }
    };

    static GetUnwindFunc UnwindFunc;

    return UnwindFunc.Unwind(BackTrace, MaxDepth);
}
PRAGMA_ENABLE_OPTIMIZATION
#endif