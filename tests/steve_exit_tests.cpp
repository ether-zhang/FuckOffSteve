// Synthetic native memory and callbacks only; no real process or save access.
#include "../src/native_steve_exit.cpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {
using namespace steveguard::steve_exit;
template<std::size_t N> using Bytes=std::array<unsigned char,N>;
template<class T> std::uintptr_t At(T& v) { return reinterpret_cast<std::uintptr_t>(v.data()); }
template<class T> void Put(std::uintptr_t p,std::size_t off,const T& v) { std::memcpy(reinterpret_cast<void*>(p+off),&v,sizeof(v)); }
void Str(std::uintptr_t p,const char* text) {
    const auto n=std::strlen(text); assert(n<=15);
    std::memcpy(reinterpret_cast<void*>(p),text,n); Put(p,16,std::uint64_t(n)); Put(p,24,std::uint64_t(15));
}
struct Fixture {
    std::vector<unsigned char> executable=std::vector<unsigned char>(0x1400000);
    Bytes<0x740> dir_mem{};
    Bytes<0x180> engine_mem{};
    Bytes<0x600> pause_scene_mem{},prompt_scene_mem{};
    Bytes<0x200> pause_mem{},prompt_mem{},ui_mem{},root_mem{},clip_mem{},node_mem{},yes_button_mem{};
    Bytes<MEW_SIZE_BUTTON+8> button_mem{};
    Bytes<16> pause_list{},prompt_list{};
    std::array<std::uintptr_t,1> pause_members{},prompt_members{};
    Bytes<0x50> yes_closure{};
    Bytes<0x18> no_closure{};
    std::uintptr_t director=At(dir_mem)+8,engine=At(engine_mem)+8,pause_scene=At(pause_scene_mem)+8,
        scene=At(prompt_scene_mem)+8,pause=At(pause_mem)+8,prompt=At(prompt_mem)+8,
        ui=At(ui_mem)+8,root=At(root_mem)+8,clip=At(clip_mem)+8,node=At(node_mem)+8,
        button=At(button_mem)+8,yes_button=At(yes_button_mem)+8;
    int created=0,clicked=0,marker=0,writes=0,shown=-1;
    bool scene_exists=true,missing_asset=false,missing_yes=false,fail_create=false,raise_on_write=false;
    MewButtonCreateInfo last_info{};
    Fixture() {
        MewUI_Shutdown();
        image=At(executable); state={}; ready=true; api={};
        const std::uint32_t type=0x10000,locator=0x10100,vt=0x10200;
        const char name[]=".?AVYesNoPrompt@glaiel@@";
        std::memcpy(executable.data()+type+16,name,sizeof(name));
        Put(image,locator,std::uint32_t(1)); Put(image,locator+12,type); Put(image,locator+20,locator);
        Put(image,vt-8,image+locator); Put(prompt,0,image+vt);
        for(auto object:{director,pause_scene,scene,pause,prompt}) Put(object-8,0,std::uint64_t(77));
        Put(image,0x13DAC30,director); Put(image,0x13DAC38,std::uint64_t(77));
        Put(director,0x590,engine); Put(director,0x710,int(1)); Put(director,0x714,static_cast<unsigned char>(1));
        Put(engine,0xE0,int(3));
        Put(pause,0x20,pause_scene); Put(prompt,0x20,scene); Put(prompt,0x40,ui); Put(ui,0x38,root); Put(root,0x80,clip);
        Str(scene+0x4B8,"YesNoPrompt"); Str(pause_scene+0x4B8,"PauseMenu");
        // The modal pauses its owner scene. Membership must still be valid.
        Put(pause_scene,0x4D8,static_cast<unsigned char>(1));
        pause_members={pause}; prompt_members={prompt};
        Put(At(pause_list),0,std::uint32_t(1)); Put(At(pause_list),4,std::uint32_t(1)); Put(At(pause_list),8,At(pause_members));
        Put(At(prompt_list),0,std::uint32_t(1)); Put(At(prompt_list),4,std::uint32_t(1)); Put(At(prompt_list),8,At(prompt_members));
        Put(pause_scene,0x18,At(pause_list)); Put(scene,0x18,At(prompt_list));
        // Final live layout observed in the post-action warning: the two
        // closures capture the same PauseMenuScene through different offsets.
        Put(At(yes_closure),0,image+kExitYesTable); Put(At(yes_closure),0x48,pause);
        Put(At(no_closure),0,image+kExitNoTable); Put(At(no_closure),8,pause);
        Put(prompt,0x90,At(yes_closure)); Put(prompt,0xD0,At(no_closure));
        Put(yes_button,MEW_OFF_BUTTON_MOUSE_GROUP,std::uint32_t(1001));
        Put(yes_button,MEW_OFF_COMPONENT_LAYER,static_cast<unsigned char>(1));
    }
    ~Fixture() { MewUI_Shutdown(); }
};
Fixture* current=nullptr;
void* Scene(const char* name) {
    assert(std::string(name)=="YesNoPrompt");
    return current->scene_exists ? reinterpret_cast<void*>(current->scene) : nullptr;
}
void* Find(void* root,const char* node) {
    assert(std::uintptr_t(root)==current->root && std::string(node)=="fos_steve_exit");
    return current->missing_asset ? nullptr : reinterpret_cast<void*>(current->node);
}
void* FindYes(void* scene,const char* node) {
    assert(std::uintptr_t(scene)==current->scene && std::string(node)=="yes");
    return current->missing_yes ? nullptr : reinterpret_cast<void*>(current->yes_button);
}
int Play(void* node,int frame) {
    assert(std::uintptr_t(node)==current->node); current->shown=frame; return 1;
}
void* Create(const MewButtonCreateInfo* info) {
    ++current->created; current->last_info=*info;
    assert(std::string(info->label_text)=="FUCK OFF STEVE");
    assert(!info->context && std::uintptr_t(info->scene_manager)==current->scene);
    if(current->fail_create) return nullptr;
    auto* button=reinterpret_cast<void*>(current->button);
    // Match the bundled SDK's creation order: apply the initial label, then
    // register the button. Reopening reuses this fixture's address only.
    if(auto* previous=MewUI_GetButtonRecord(button)) *previous={};
    MewUI_SetButtonLabelText(button,info->label_text);
    assert(MewUI_RegisterExistingButton(button,info->role_name,nullptr,nullptr));
    return button;
}
int Enable(void*,int enabled) { assert(!enabled); return 1; }
int Ready(void* scene) {
    for(auto off:{0x4B0,0x4D8,0x4DA,0x4DB}) if(Value<unsigned char>(std::uintptr_t(scene)+off)) return 0;
    return 1;
}
void __fastcall ClickYes(void* capture) {
    assert(Ptr(std::uintptr_t(capture)+8)==current->prompt);
    ++current->clicked; Put(current->clip,9,static_cast<unsigned char>(2));
}
void __fastcall Mark(void* director) {
    ++current->writes; current->marker=Value<int>(std::uintptr_t(director)+0x710);
    if(current->raise_on_write) RaiseException(0xE0421337,0,0,nullptr);
}
void Setup(Fixture& f) {
    current=&f; next_mark=Mark; yes_click=ClickYes;
    find_scene=Scene; find_node=Find; find_button=FindYes; play_frame=Play;
    create_button=Create; enable_button=Enable; scene_ready=Ready;
}
void Open(Fixture& f) { Setup(f); Put(f.pause,0xC1,static_cast<unsigned char>(1)); }
void Choose(Fixture& f) {
    ButtonEvent(reinterpret_cast<void*>(f.button),MEW_BUTTON_EVENT_CLICK,MEW_BUTTON_STATE_IDLE,MEW_BUTTON_STATE_IDLE,nullptr);
}
bool ThrowingWriteRestores(Fixture& f) {
    const auto original=Value<int>(f.director+0x710);
    __try { WriteMarker(reinterpret_cast<void*>(f.director),kExitYesReturn); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return Value<int>(f.director+0x710)==original; }
    return false;
}
}

int TestSteveExit() {
    using steveguard::TickNativeSteveExit;
    {
        Fixture f; Open(f); TickNativeSteveExit();
        auto* button=reinterpret_cast<void*>(f.button);
        auto* record=MewUI_GetButtonRecord(button);
        // Exercise the real SDK cache and state tracking with no game base.
        // Native text rendering is deliberately unavailable in this test.
        if(!record || record->label_override_kind!=1 ||
            std::string(record->label_values[0])!="FUCK OFF STEVE") {
            std::cerr << "Steven exit failed: newly created button has no persistent literal label.\n";
            return 1;
        }
        for(auto next:{MEW_BUTTON_STATE_HOVERED,MEW_BUTTON_STATE_PRESSED,
            MEW_BUTTON_STATE_IDLE,MEW_BUTTON_STATE_HOVERED,MEW_BUTTON_STATE_SELECTED,
            MEW_BUTTON_STATE_DISABLED,MEW_BUTTON_STATE_IDLE}) {
            assert(MewUI_SetButtonState(button,next));
            MewUI_Tick();
            assert(record->last_state==next && record->label_override_kind==1);
            assert(std::string(record->label_values[0])=="FUCK OFF STEVE");
        }
        assert(f.clicked==0 && f.writes==0 && state.phase==Phase::Ready);
    }
    {
        Fixture f; Open(f);
        Put(f.scene,0x4D8,static_cast<unsigned char>(1)); TickNativeSteveExit(); assert(f.created==0 && state.phase==Phase::None);
        Put(f.scene,0x4D8,static_cast<unsigned char>(0)); TickNativeSteveExit(); TickNativeSteveExit();
        assert(state.phase==Phase::Ready && f.created==1 && f.shown==1 && f.writes==0);
        assert(Value<std::uint32_t>(f.button+MEW_OFF_BUTTON_MOUSE_GROUP)==1001 && Value<unsigned char>(f.button+MEW_OFF_COMPONENT_LAYER)==1);
        assert(!CanInteract(reinterpret_cast<void*>(f.button),0,nullptr));
        Choose(f); Choose(f); TickNativeSteveExit(); TickNativeSteveExit();
        assert(f.clicked==1 && state.phase==Phase::Closing && f.writes==0);
        // The animation can outlive the prompt. Only the still-live owning
        // PauseMenuScene and the exact exit caller may consume the request.
        Put(f.scene,0x4B0,static_cast<unsigned char>(1)); TickNativeSteveExit();
        WriteMarker(reinterpret_cast<void*>(f.director),0x12345);
        assert(f.marker==1 && state.phase==Phase::Closing);
        WriteMarker(reinterpret_cast<void*>(f.director),kExitYesReturn);
        assert(f.marker==0 && f.writes==2 && state.phase==Phase::None && Value<int>(f.director+0x710)==1 && Value<int>(f.engine+0xE0)==3);
        WriteMarker(reinterpret_cast<void*>(f.director),kExitYesReturn); assert(f.marker==1); // One exit only.
    }
    {
        // If an early tick observes initialization, retry once the owner sets
        // its modal byte; no constructor hook or raw message is needed.
        Fixture f; Setup(f); TickNativeSteveExit(); assert(state.phase==Phase::None && f.created==0);
        Put(f.pause,0xC1,static_cast<unsigned char>(1)); TickNativeSteveExit(); assert(state.phase==Phase::Ready && f.created==1);
    }
    {
        Fixture f; Open(f); f.scene_exists=false; TickNativeSteveExit(); assert(state.phase==Phase::None);
        f.scene_exists=true; TickNativeSteveExit(); assert(state.phase==Phase::Ready && f.created==1);
    }
    for(int cause:{1,2,3}) {
        Fixture f; Put(f.director,0x710,cause); Open(f); TickNativeSteveExit(); Choose(f); TickNativeSteveExit();
        WriteMarker(reinterpret_cast<void*>(f.director),kExitYesReturn);
        assert(f.marker==0 && Value<int>(f.director+0x710)==cause);
    }
    {
        Fixture f; Open(f); TickNativeSteveExit(); // Ordinary Yes preserves tracking.
        WriteMarker(reinterpret_cast<void*>(f.director),kExitYesReturn); assert(f.marker==1);
        Put(f.pause,0xC1,static_cast<unsigned char>(0)); TickNativeSteveExit(); assert(state.phase==Phase::None);
        Put(f.pause,0xC1,static_cast<unsigned char>(1)); Put(f.prompt-8,0,std::uint64_t(78));
        TickNativeSteveExit(); assert(state.phase==Phase::Ready && f.created==2); // Reopened warning binds anew.
    }
    {
        Fixture f; Open(f); TickNativeSteveExit(); Choose(f);
        Put(f.clip,9,static_cast<unsigned char>(2)); TickNativeSteveExit();
        assert(state.phase==Phase::None && f.clicked==0 && f.writes==0); // Original Yes/No won the race.
    }
    {
        Fixture f; Open(f); TickNativeSteveExit(); Choose(f); TickNativeSteveExit();
        Put(f.pause-8,0,std::uint64_t(78));
        WriteMarker(reinterpret_cast<void*>(f.director),kExitYesReturn); assert(f.marker==1);
        TickNativeSteveExit(); assert(state.phase==Phase::None);
    }
    {
        Fixture f; Open(f); TickNativeSteveExit(); Choose(f); TickNativeSteveExit();
        f.raise_on_write=true; assert(ThrowingWriteRestores(f)); assert(state.phase==Phase::None);
    }
    for(int mismatch=0;mismatch<6;++mismatch) {
        Fixture f; Open(f);
        if(mismatch==0) Put(f.director,0x714,static_cast<unsigned char>(0)); // No action: vanilla exit, no mod intervention.
        if(mismatch==1) Put(At(f.yes_closure),0,image+0x1234); // Unrelated dialog.
        if(mismatch==2) Put(f.prompt,0xD0,std::uintptr_t(0)); // OK-only dialog.
        if(mismatch==3) Put(At(f.no_closure),8,f.pause+8); // Mismatched owner.
        if(mismatch==4) Put(f.director,0x710,int(0));
        if(mismatch==5) Put(f.clip,9,static_cast<unsigned char>(2)); // Already closing.
        TickNativeSteveExit(); assert(state.phase==Phase::None && f.created==0 && f.writes==0);
    }
    for(int failure=0;failure<3;++failure) {
        Fixture f; Open(f);
        f.missing_asset=failure==0; f.missing_yes=failure==1; f.fail_create=failure==2;
        TickNativeSteveExit(); TickNativeSteveExit();
        assert(state.phase==Phase::Unavailable && f.created==(failure==2 ? 1 : 0));
        if(failure!=0) assert(f.shown==0);
        assert(f.writes==0 && f.clicked==0);
        Put(f.prompt-8,0,std::uint64_t(78)); TickNativeSteveExit(); assert(state.phase==Phase::None);
    }
    steveguard::StopNativeSteveExit();
    std::cout << "Steven exit passed: SDK label cache across hover/press/idle/selected/disabled states, completed post-action prompt discovery, delayed readiness, modal button group/layer, reopen, deferred single click, animation delay, one-exit marker bypass, cause/counter preservation, ordinary exit/cancel, unrelated dialogs, stale scenes, failure restoration and missing assets.\n";
    return 0;
}
