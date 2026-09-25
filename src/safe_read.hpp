#pragma once
#include <cstddef>
#include <cstdint>

namespace steveguard {
bool ReadBytes(std::uintptr_t address,void* destination,std::size_t size) noexcept;
bool ComponentHasType(std::uintptr_t component,std::uintptr_t image,const char* expected);
}
