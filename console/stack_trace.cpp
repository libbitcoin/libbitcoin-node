/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "stack_trace.hpp"

#include <algorithm>
#include <exception>
#include <iterator>
#include <memory>
#include <new>
#include <sstream>
#include <bitcoin/system.hpp>

#if defined(HAVE_MSC)

#include <windows.h>
#include <psapi.h>
#include <dbghelp.h>

// This pulls in libs required for APIs used below.
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "dbghelp.lib")

// Must define pdb_path() and handle_stack_trace when using dump_stack_trace.
extern std::string pdb_path() NOEXCEPT;
extern void handle_stack_trace(const std::string& trace) NOEXCEPT;

constexpr size_t depth_limit{ 10 };

using namespace bc;
using namespace system;

struct module_data
{
    std::string image;
    std::string module;
    DWORD dll_size;
    void* dll_base;
};

inline STACKFRAME64 get_stack_frame(const CONTEXT& context) NOEXCEPT
{
    STACKFRAME64 frame{};

#if defined(HAVE_X64)
    frame.AddrPC.Offset = context.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
#else
    frame.AddrPC.Offset = context.Eip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Esp;
    frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrFrame.Mode = AddrModeFlat;
#endif

    return frame;
}

static std::string get_undecorated(HANDLE process, DWORD64 address) NOEXCEPT
{
    constexpr size_t maximum{ 1024 };
    constexpr auto buffer_size = possible_narrow_cast<DWORD>(maximum);
    constexpr size_t struct_size{ sizeof(IMAGEHLP_SYMBOL64) + maximum };

    BC_PUSH_WARNING(NO_NEW_OR_DELETE)
    const auto symbol = std::unique_ptr<IMAGEHLP_SYMBOL64>(
        pointer_cast<IMAGEHLP_SYMBOL64>(operator new(struct_size)));
    BC_POP_WARNING()

    if (!symbol)
        return {};

    symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
    symbol->MaxNameLength = maximum;
    symbol->Name[0] = '\0';
    symbol->Address = 0;
    symbol->Flags = 0;
    symbol->Size = 0;

    DWORD64 displace{};
    if (SymGetSymFromAddr64(process, address, &displace,
        symbol.get()) == FALSE || is_zero(symbol->MaxNameLength))
    {
        return {};
    }

    std::string undecorated{};
    undecorated.resize(maximum);
    undecorated.resize(UnDecorateSymbolName(symbol->Name,
        undecorated.data(), buffer_size, UNDNAME_COMPLETE));

    return undecorated;
}

inline DWORD get_machine(HANDLE process) THROWS
{
    constexpr size_t module_buffer_size{ 4096 };

    // Get required bytes by passing zero.
    DWORD bytes{};
    if (EnumProcessModules(process, NULL, 0u, &bytes) == FALSE)
        throw(std::logic_error("EnumProcessModules"));

    std::vector<HMODULE> handles{};
    handles.resize(bytes / sizeof(HMODULE));
    const auto handles_buffer_size = possible_narrow_cast<DWORD>(
        handles.size() * sizeof(HMODULE));

    // Get all elements into contiguous byte vector.
    if (EnumProcessModules(process, &handles.front(), handles_buffer_size,
        &bytes) == FALSE)
        throw(std::logic_error("EnumProcessModules"));

    std::vector<module_data> modules{};
    std::transform(handles.begin(), handles.end(),
        std::back_inserter(modules), [process](const auto& handle) THROWS
        {
            MODULEINFO info{};
            if (GetModuleInformation(process, handle, &info,
                sizeof(MODULEINFO)) == FALSE)
                throw(std::logic_error("GetModuleInformation"));

            module_data data{};
            data.dll_base = info.lpBaseOfDll;
            data.dll_size = info.SizeOfImage;

            data.image.resize(module_buffer_size);
            if (GetModuleFileNameExA(process, handle, data.image.data(),
                module_buffer_size) == FALSE)
                throw(std::logic_error("GetModuleFileNameExA"));

            data.module.resize(module_buffer_size);
            if (GetModuleBaseNameA(process, handle, data.module.data(),
                module_buffer_size) == FALSE)
                throw(std::logic_error("GetModuleBaseNameA"));

            SymLoadModule64(process, NULL, data.image.data(), data.module.data(),
                reinterpret_cast<DWORD64>(data.dll_base), data.dll_size);

            return data;
        });

    const auto header = ImageNtHeader(modules.front().dll_base);

    if (is_null(header))
        throw(std::logic_error("ImageNtHeader"));

    return header->FileHeader.Machine;
}

DWORD dump_stack_trace(EXCEPTION_POINTERS* exception) THROWS
{
    if (is_null(exception) || is_null(exception->ContextRecord))
        return EXCEPTION_EXECUTE_HANDLER;

    const auto process = GetCurrentProcess();
    if (SymInitialize(process, pdb_path().c_str(), false) == FALSE)
        throw(std::logic_error("SymInitialize"));

    SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);

    const auto thread = GetCurrentThread();
    const auto machine = get_machine(process);
    const auto context = exception->ContextRecord;
    auto it = get_stack_frame(*context);

    DWORD displace{};
    IMAGEHLP_LINE64 line{};
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    std::ostringstream tracer{};

    auto iteration = zero;
    while ((iteration++ < depth_limit) && !is_zero(it.AddrReturn.Offset))
    {
        // Get undecorated function name.
        const auto name = get_undecorated(process, it.AddrPC.Offset);

        if (name == "main")
            break;

        if (name == "RaiseException")
            return EXCEPTION_EXECUTE_HANDLER;

        // Get line.
        if (SymGetLineFromAddr64(process, it.AddrPC.Offset, &displace,
            &line) == FALSE)
            break;

        // Write line.
        tracer
            << name << ":"
            << line.FileName
            << "(" << line.LineNumber << ")"
            << std::endl;

        // Advance interator.
        if (StackWalk64(machine, process, thread, &it, context, NULL,
            SymFunctionTableAccess64, SymGetModuleBase64, NULL) == FALSE)
            break;
    }

    handle_stack_trace(tracer.str());

    if (SymCleanup(process) == FALSE)
        throw(std::logic_error("SymCleanup"));

    return EXCEPTION_EXECUTE_HANDLER;
}

#endif
