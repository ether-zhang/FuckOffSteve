#include "../src/native_args.hpp"
#include <cassert>
#include <string>
#include <vector>

void TestNativeArgs() {
    using Strings = std::vector<std::string>;
    const std::string folder = "F:/Game Folder/mods/FuckOffSteve";
    const std::string base = "F:/Game Folder";
    assert((steveguard::AddOwnModPath({"Mewgenics.exe"}, folder, base) ==
        Strings{"Mewgenics.exe", "-modpaths", folder}));
    const Strings existing{"Mewgenics.exe", "-modpaths", "OtherOne", "OtherTwo", "-fullscreen", "0"};
    assert((steveguard::AddOwnModPath(existing, folder, base) ==
        Strings{"Mewgenics.exe", "-modpaths", folder, "OtherOne", "OtherTwo", "-fullscreen", "0"}));
    const Strings relative{"Mewgenics.exe", "-modpaths", ".\\mods\\FuckOffSteve", "-devmode"};
    assert(steveguard::AddOwnModPath(relative, folder, base) == relative);
    const Strings mixed_case{"Mewgenics.exe", "-MODPATHS", "f:\\game folder\\mods\\FUCKOFFSTEVE\\"};
    assert(steveguard::AddOwnModPath(mixed_case, folder, base) == mixed_case);
    const Strings similar{"Mewgenics.exe", "-modpaths", "mods/FuckOffSteveOther"};
    assert(steveguard::AddOwnModPath(similar, folder, base).size() == similar.size() + 1);
    const Strings elsewhere{"Mewgenics.exe", "-output", folder};
    assert(steveguard::AddOwnModPath(elsewhere, folder, base).size() == elsewhere.size() + 2);
    const std::string unicode_folder = "F:/中文 目录/mods/FuckOffSteve";
    assert(steveguard::NormalizeModPath(".\\mods\\FuckOffSteve", "F:/中文 目录") ==
           steveguard::NormalizeModPath(unicode_folder));
    assert(steveguard::AddOwnModPath({}, folder).empty());
    // Each mod registers only its own path; either hook order preserves both.
    const auto other=base+"/mods/RoomCatList";
    for(bool first:{false,true}) {
        auto args=steveguard::AddOwnModPath({"Mewgenics.exe","-fullscreen","0"},first ? folder : other,base);
        args=steveguard::AddOwnModPath(args,first ? other : folder,base);
        assert(std::count(args.begin(),args.end(),folder)==1);
        assert(std::count(args.begin(),args.end(),other)==1);
        assert(std::count(args.begin(),args.end(),"-modpaths")==1);
        assert(steveguard::AddOwnModPath(args,folder,base)==args);
        assert(args[args.size()-2]=="-fullscreen" && args.back()=="0");
    }
}
