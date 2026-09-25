#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <intrin.h>
#include "native_steve_exit.hpp"
#include "safe_read.hpp"
#include <array>

namespace steveguard {
namespace steve_exit {
constexpr std::uintptr_t kMark=0x3BBE60, kYesClick=0x77F7A0;
constexpr std::uintptr_t kExitYesTable=0xF055C0, kExitNoTable=0xF05588, kExitYesReturn=0x29AD65;
using MarkFn=void (__fastcall*)(void*);
using ClickFn=void (__fastcall*)(void*);
MarkFn next_mark=nullptr;
ClickFn yes_click=nullptr;
MewjectorAPI api{};
std::uintptr_t image=0;
bool ready=false;
enum class Phase { None, Bind, Ready, Queued, Closing, Unavailable };
struct Prompt {
    Phase phase=Phase::None;
    std::uintptr_t prompt=0,scene=0,pause=0,pause_scene=0,director=0,engine=0,button=0;
    std::uint64_t prompt_generation=0,scene_generation=0,pause_generation=0,director_generation=0;
    int cause=0;
} state;
auto find_node=MewUI_FindChildByName;
auto find_button=MewUI_FindButtonByNodeName;
auto find_scene=MewUI_GetSceneByName;
auto play_frame=MewUI_PlayMovieClipFrame;
auto create_button=MewUI_CreateButtonFromNode;
auto enable_button=MewUI_SetButtonEnabled;
auto scene_ready=MewUI_IsSceneReadyForUITick;

template<class T> T Value(std::uintptr_t p) { T v{}; ReadBytes(p,&v,sizeof(v)); return v; }
std::uintptr_t Ptr(std::uintptr_t p) { return Value<std::uintptr_t>(p); }
void Log(const char* text) { if(api.Log) api.Log("FuckOffSteve","%s",text); }
bool ContextLive() {
    return ready && state.phase!=Phase::None && state.director && state.pause && state.engine &&
        Ptr(image+0x13DAC30)==state.director && Ptr(state.director+0x590)==state.engine &&
        Value<std::uint64_t>(state.director-8)==state.director_generation &&
        Value<std::uint64_t>(image+0x13DAC38)==state.director_generation &&
        Value<std::uint64_t>(state.pause-8)==state.pause_generation && Ptr(state.pause+0x20)==state.pause_scene &&
        MewUI_IsComponentInScene(reinterpret_cast<void*>(state.pause_scene),reinterpret_cast<void*>(state.pause)) &&
        Value<unsigned char>(state.pause+0xC1) && Value<int>(state.director+0x710)==state.cause;
}
bool PromptLive() {
    return ContextLive() && Value<std::uint64_t>(state.scene-8)==state.scene_generation &&
        Value<std::uint64_t>(state.prompt-8)==state.prompt_generation && Ptr(state.prompt+0x20)==state.scene &&
        MewUI_IsComponentInScene(reinterpret_cast<void*>(state.scene),reinterpret_cast<void*>(state.prompt));
}
std::uintptr_t Root() { return Ptr(Ptr(state.prompt+0x40)+0x38); }
bool Closing() {
    const auto clip=Ptr(Root()+0x80);
    return !clip || (Value<unsigned char>(clip+9)&2)!=0;
}
bool DiscoverPrompt() {
    // Inspect the completed scene rather than a consuming constructor's
    // transient arguments. The game may set the owner's modal byte later.
    const auto scene=std::uintptr_t(find_scene("YesNoPrompt"));
    if(!scene || !scene_ready(reinterpret_cast<void*>(scene))) return false;
    const auto list=Ptr(scene+0x18), data=Ptr(list+8);
    const auto count=Value<std::uint32_t>(list+4);
    if(!data || count>4096) return false;
    for(std::uint32_t i=0;i<count;++i) {
        const auto prompt=Ptr(data+i*sizeof(std::uintptr_t));
        if(!ComponentHasType(prompt,image,".?AVYesNoPrompt@glaiel@@")) continue;
        const auto yes=Ptr(prompt+0x90), no=Ptr(prompt+0xD0);
        if(Ptr(yes)!=image+kExitYesTable || Ptr(no)!=image+kExitNoTable) continue;
        const auto pause=Ptr(yes+0x48), director=Ptr(image+0x13DAC30);
        const int cause=Value<int>(director+0x710);
        if(!pause || Ptr(no+8)!=pause || !director || cause<1 || cause>3 ||
            !Value<unsigned char>(director+0x714) || !Value<unsigned char>(pause+0xC1)) continue;
        // Both closures must belong to the original SL-exit owner, not merely
        // to a dialog with matching visible text.
        state={}; state.phase=Phase::Bind; state.prompt=prompt; state.scene=scene;
        state.pause=pause; state.pause_scene=Ptr(pause+0x20); state.director=director;
        state.engine=Ptr(director+0x590); state.cause=cause;
        state.prompt_generation=Value<std::uint64_t>(prompt-8); state.scene_generation=Value<std::uint64_t>(scene-8);
        state.pause_generation=Value<std::uint64_t>(pause-8); state.director_generation=Value<std::uint64_t>(director-8);
        if(!PromptLive() || Closing()) { state={}; return false; }
        Log("Steven exit warning discovered in the completed native prompt.");
        return true;
    }
    return false;
}
bool MatchNativeButton(void* button,std::uintptr_t yes) noexcept {
    __try {
        // Native yes/no use the modal input group (1001 in the observed game),
        // which differs from an ordinary newly-created Button's default group.
        *reinterpret_cast<std::uint32_t*>(std::uintptr_t(button)+MEW_OFF_BUTTON_MOUSE_GROUP)=Value<std::uint32_t>(yes+MEW_OFF_BUTTON_MOUSE_GROUP);
        *reinterpret_cast<unsigned char*>(std::uintptr_t(button)+MEW_OFF_COMPONENT_LAYER)=Value<unsigned char>(yes+MEW_OFF_COMPONENT_LAYER);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
unsigned char __cdecl CanInteract(void* button,unsigned char original,void*) {
    return original && std::uintptr_t(button)==state.button && state.phase==Phase::Ready && PromptLive() && !Closing();
}
void __cdecl ButtonEvent(void* button,MewButtonEvent event,MewButtonState,MewButtonState,void*) {
    if(event==MEW_BUTTON_EVENT_CLICK && CanInteract(button,1,nullptr)) state.phase=Phase::Queued;
}
bool BeginExit() noexcept {
    std::array<std::uintptr_t,2> capture{0,state.prompt};
    __try {
        // The game's original Yes click closes its animation and schedules the
        // original exit continuation. It does not end the process immediately.
        yes_click(capture.data());
        return Closing();
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void WriteMarker(void* director,std::uintptr_t caller_rva) {
    const bool bypass=state.phase==Phase::Closing && caller_rva==kExitYesReturn &&
        std::uintptr_t(director)==state.director && ContextLive();
    if(!bypass) { next_mark(director); return; }
    // Consume before calling the game. The counter and all previous penalties
    // are untouched. The native setter writes the same zero marker as Cancel.
    state={};
    auto* cause=reinterpret_cast<int*>(std::uintptr_t(director)+0x710);
    const int original=*cause;
    __try {
        *cause=0;
        next_mark(director);
    } __finally {
        *cause=original; // Restore before the native quit/save continuation runs.
    }
    Log("Steven exit option: this exit was saved without a new save-scum marker.");
}
void __fastcall MarkHook(void* director) {
    WriteMarker(director,reinterpret_cast<std::uintptr_t>(_ReturnAddress())-image);
}
}

bool StartNativeSteveExit(const MewjectorAPI& source) {
    using namespace steve_exit;
    api=source; image=api.GetGameBase();
    if(Ptr(image+kExitYesTable+0x10)!=image+0x29AD50 ||
        Ptr(image+kExitNoTable+0x10)!=image+0x29AC40) return false;
    void* next=nullptr;
    if(!api.InstallHook(kMark,17,reinterpret_cast<void*>(&MarkHook),&next,30,"FuckOffSteve Steven exit marker") || !next) return false;
    next_mark=reinterpret_cast<MarkFn>(next);
    yes_click=reinterpret_cast<ClickFn>(image+kYesClick); ready=true;
    return true;
}
void TickNativeSteveExit() {
    using namespace steve_exit;
    if(!ready) return;
    if(state.phase==Phase::None && !DiscoverPrompt()) return;
    if(!ContextLive()) {
        if(state.phase==Phase::Closing) Log("Steven exit request canceled because its native owner changed before saving.");
        state={}; return;
    }
    if(state.phase==Phase::Closing) return; // Original callback may wait for the closing animation.
    if(!PromptLive() || Closing()) { state={}; return; }
    if(state.phase==Phase::Unavailable) return;
    if(!scene_ready(reinterpret_cast<void*>(state.scene))) return;
    if(state.phase==Phase::Bind) {
        auto* node=find_node(reinterpret_cast<void*>(Root()),"fos_steve_exit");
        if(!node || !play_frame(node,1)) { state.phase=Phase::Unavailable; Log("Steven exit option asset unavailable; original prompt retained."); return; }
        const auto yes=std::uintptr_t(find_button(reinterpret_cast<void*>(state.scene),"yes"));
        if(!yes) { play_frame(node,0); state.phase=Phase::Unavailable; Log("Steven exit option could not locate the native Yes button."); return; }
        MewButtonCreateInfo info{};
        info.scene_manager=reinterpret_cast<void*>(state.scene);
        info.root_node=reinterpret_cast<void*>(Root()); info.button_node=node;
        info.node_name="fos_steve_exit"; info.role_name="FOS_SteveExit"; info.label_text="FUCK OFF STEVE";
        info.enabled=1; info.activate_enabled=1; info.interact_override=MEW_BUTTON_INTERACT_GAME_DEFAULT;
        info.callback=ButtonEvent; info.can_interact_callback=CanInteract;
        auto* button=create_button(&info);
        if(!button || !MatchNativeButton(button,yes)) {
            if(button) enable_button(button,0);
            play_frame(node,0); state.phase=Phase::Unavailable; Log("Steven exit button could not be created; original prompt retained."); return;
        }
        // The SDK applies create_info.label_text before allocating its tracking
        // record. Cache it after creation so newly loaded state frames retain it.
        MewUI_SetButtonLabelText(button,info.label_text);
        state.button=std::uintptr_t(button); state.phase=Phase::Ready;
        Log("Steven exit option attached to the native confirmation prompt.");
    } else if(state.phase==Phase::Queued) {
        state.phase=Phase::Closing;
        Log("Steven exit option selected; waiting for the native exit confirmation to finish.");
        enable_button(reinterpret_cast<void*>(state.button),0);
        if(!BeginExit()) { state={}; Log("Steven exit option canceled: native prompt was no longer available."); }
    }
}
void StopNativeSteveExit() { steve_exit::ready=false; steve_exit::state={}; }
}
