#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "safe_read.hpp"
#include <array>
#include <cstring>
#include <limits>

namespace steveguard {
bool ReadBytes(std::uintptr_t address,void* destination,std::size_t size) noexcept {
    if(!destination || address<0x10000 || size>1024*1024 ||
        address>std::numeric_limits<std::uintptr_t>::max()-size) return false;
    __try { std::memcpy(destination,reinterpret_cast<const void*>(address),size); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool ComponentHasType(std::uintptr_t component,std::uintptr_t image,const char* expected) {
    // Inspect MSVC RTTI without invoking game functions or touching its caches.
    std::uintptr_t table=0,locator=0;
    if(!ReadBytes(component,&table,sizeof(table)) || table<image+8 || table-image>0x3000000 ||
        !ReadBytes(table-8,&locator,sizeof(locator)) || locator<image || locator-image>0x3000000) return false;
    std::array<std::uint32_t,6> info{};
    if(!ReadBytes(locator,info.data(),sizeof(info)) || info[0]!=1 || info[3]>0x3000000 ||
        image+info[5]!=locator) return false;
    std::array<char,100> actual{};
    const auto size=std::strlen(expected)+1;
    return size<=actual.size() && ReadBytes(image+info[3]+16,actual.data(),size) &&
        std::memcmp(actual.data(),expected,size)==0;
}
}
