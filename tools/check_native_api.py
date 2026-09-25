"""Validate the pinned MewUI API against the installed executable, without running it."""
from pathlib import Path
import importlib.util
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / "vendor" / "mew-ui-api"
SPEC = importlib.util.spec_from_file_location("mew_ui_resolver", UPSTREAM / "re_tools" / "update_mew_ui_api_offsets.py")
resolver = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = resolver
SPEC.loader.exec_module(resolver)


def main():
    image = resolver.PeImage(ROOT.parents[1] / "Mewgenics.exe")
    header = (UPSTREAM / "src" / "native" / "mew_ui_api.h").read_text(encoding="utf-8")
    config = resolver.load_signatures(UPSTREAM / "re_tools" / "mew_ui_api_signatures.json")
    resolver.validate_signature_coverage(UPSTREAM / "src" / "native" / "mew_ui_api.h", config)
    values = {}
    guards = {}
    for name, signature in config["symbols"].items():
        resolved = resolver.resolve_symbol(image, name, signature)
        define = re.search(r"^\s*#define\s+" + re.escape(name) + r"\s+(0x[\da-fA-F]+|\d+)", header, re.M)
        if not define or int(define.group(1), 0) != resolved.value:
            raise RuntimeError(f"Pinned API offset mismatch: {name} resolved to {resolved.value:#x}")
        values[name] = resolved.value
        if name.startswith("MEW_RVA_") and signature.get("kind") == "rva":
            offset, section = image.rva_to_file_offset(resolved.value)
            if section.name == ".text":
                guards[resolved.value] = (name, image.data[offset:offset + 16])
    extra = {
        "FOS_RVA_PROCESS_ARGS": (0x9B8BB0, "4C 89 44 24 18 89 54 24 10 48 89 4C 24 08 55"),
        "FOS_LAYOUT_PROMPT_YES_CALLBACK": (0x77F7BD, "48 8B 8B 90 00 00 00 48 85 C9 74 0F 48 8B 01 48"),
        "FOS_LAYOUT_PROMPT_NO_CALLBACK": (0x77F73D, "48 8B 8B D0 00 00 00 48 85 C9 74 0F 48 8B 01 48"),
        "FOS_RVA_SAVESCUM_MARKER": (0x3BBE60, "48 89 5C 24 10 57 48 83 EC 40 48 63 99 10 07 00 00"),
        "FOS_RVA_PROMPT_YES_CLICK": (0x77F7A0, "40 53 48 83 EC 60 48 8B 59 08 48 8D 44 24 20 48"),
        "FOS_LAYOUT_EXIT_YES_MARKER": (0x29AD59, "48 8B 0D D0 FE 13 01 E8 FB 10 12 00 48 8B 4B 40"),
        "FOS_LAYOUT_PROMPT_CLOSING": (0x77DCB3, "F6 40 09 02 75 46 48 8D 8F D8 00 00 00 48 8B D3"),
    }
    for name, (rva, expected) in extra.items():
        offset, _ = image.rva_to_file_offset(rva)
        prefix = bytes.fromhex(expected)
        if image.data[offset:offset + len(prefix)] != prefix:
            raise RuntimeError(f"Native entry-point signature mismatch: {name}")
        guards[rva] = (name, image.data[offset:offset + 16])
    lines = ["// Generated only after upstream signature resolution matches the pinned header.",
             "#pragma once", "#include <cstdint>",
             "struct NativeSignature { std::uintptr_t rva; unsigned char bytes[16]; const char* name; };",
             "inline constexpr NativeSignature kNativeSignatures[] = {"]
    for rva, (name, data) in sorted(guards.items()):
        lines.append("    {0x%X, {%s}, \"%s\"}," % (rva, ", ".join(f"0x{x:02X}" for x in data), name))
    lines += ["};", ""]
    destination = ROOT / "src" / "native_signatures.generated.hpp"
    text = "\n".join(lines)
    if not destination.exists() or destination.read_text(encoding="utf-8") != text:
        destination.write_text(text, encoding="utf-8")
    print(f"Native API verified: {len(values)} upstream symbols; {len(guards)} code guards. Game was not launched.")


if __name__ == "__main__":
    main()
