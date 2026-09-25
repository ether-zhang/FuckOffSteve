#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "native_args.hpp"
#include "native_steve_exit.hpp"
#include "native_signatures.generated.hpp"
#include "safe_read.hpp"
#include <array>
#include <atomic>
#include <cstring>

namespace {
constexpr const char* kOwner="FuckOffSteve";
using ProcessArgs=std::intptr_t(__fastcall*)(void*,int,const char**);
MewjectorAPI api{};
ProcessArgs next_args=nullptr;
std::string mod_folder,game_folder;
std::atomic<bool> assets_registered{false};
bool started=false,attempted=false;

bool VerifyCode() {
    for(const auto& check:kNativeSignatures) {
        if(api.QueryHook(check.rva)>0) continue; // Shared hook entries are owned by Mewjector.
        std::array<unsigned char,16> bytes{};
        if(!steveguard::ReadBytes(api.GetGameBase()+check.rva,bytes.data(),bytes.size()) ||
            std::memcmp(bytes.data(),check.bytes,bytes.size())!=0) {
            api.Log(kOwner,"Unsupported native code: %s",check.name); return false;
        }
    }
    return true;
}
std::string ModuleFolder(HMODULE module) {
    std::array<wchar_t,32768> path{};
    const auto length=GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size()));
    if(!length || length>=path.size()) return {};
    auto* slash=wcsrchr(path.data(),L'\\');
    if(!slash) return {};
    *slash=0;
    return std::filesystem::path(path.data()).u8string();
}
void __cdecl UiTick(void*) {
    if(!assets_registered.load()) return;
    if(!attempted) {
        attempted=true;
        started=steveguard::StartNativeSteveExit(api);
        api.Log(kOwner,"FuckOffSteve 0.1.2 native exit option: %s",started ? "ready" : "unavailable");
    }
    if(started) steveguard::TickNativeSteveExit();
}
std::intptr_t __fastcall ProcessArgsHook(void* application,int count,const char** argv) {
    if(count<1 || count>4096 || !argv) return next_args(application,count,argv);
    std::vector<std::string> args;
    std::vector<const char*> pointers;
    try {
        std::vector<std::string> original;
        original.reserve(count);
        for(int i=0;i<count;++i) original.emplace_back(argv[i] ? argv[i] : "");
        args=steveguard::AddOwnModPath(original,mod_folder,game_folder);
        pointers.reserve(args.size());
        for(const auto& arg:args) pointers.push_back(arg.c_str());
    } catch(...) {
        api.Log(kOwner,"Could not register resources; preserving startup arguments.");
        return next_args(application,count,argv);
    }
    const auto result=next_args(application,static_cast<int>(pointers.size()),pointers.data());
    if(!assets_registered.exchange(true)) {
        // Start the SDK's bootstrap timer after argument parsing, outside DllMain.
        MewUI_SetDebugLogsEnabled(false);
        api.Log(kOwner,"Standalone resource path registered; native UI bootstrap: %s",
            MewUI_Start(kOwner,40,100,16,UiTick,nullptr) ? "ready" : "failed");
    }
    return result;
}
void Bootstrap(HMODULE module) {
    if(!MJ_Resolve(&api) || !VerifyCode()) return;
    mod_folder=ModuleFolder(module); game_folder=ModuleFolder(nullptr);
    if(mod_folder.empty() || game_folder.empty()) return;
    // Engine callbacks last for this process; disabling the mod takes effect on restart.
    HMODULE pinned=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&Bootstrap),&pinned)) return;
    void* next=nullptr;
    if(!api.InstallHook(0x9B8BB0,15,reinterpret_cast<void*>(&ProcessArgsHook),&next,26,kOwner) || !next) return;
    next_args=reinterpret_cast<ProcessArgs>(next);
}
}

BOOL WINAPI DllMain(HMODULE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        try { Bootstrap(module); }
        catch(...) { if(api.Log) api.Log(kOwner,"Bootstrap failed; exit option disabled."); }
    }
    return TRUE;
}
