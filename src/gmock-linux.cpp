// gmock-linux.cpp — PLT/GOT patching implementation for Linux
// Enables runtime replacement of dynamically-linked library functions
// (e.g., libc functions like open, read, write) with mock stubs.
//
// License: MIT

#include "gmock-linux.h"

#include <dlfcn.h>        // dlsym, dladdr, RTLD_DEFAULT, RTLD_NEXT
#include <link.h>         // dl_iterate_phdr, dl_phdr_info, ElfW
#include <elf.h>          // ELF types
#include <sys/mman.h>     // mprotect, PROT_*
#include <unistd.h>       // sysconf, _SC_PAGESIZE
#include <cstring>        // strcmp
#include <stdexcept>
#include <string>
#include <memory> 

namespace {

// ============================================================================
// ELF GOT patching internals
// ============================================================================

struct PatchContext
{
    const char* func_name;       // symbol name to find
    void*       real_func_addr;  // resolved address from dlsym
    void*       new_func_addr;   // stub to replace with
    int         patch_count;     // [output] number of GOT entries patched
};

struct RestoreContext
{
    const char* func_name;       // symbol name to find
    void*       orig_func_addr;  // original function to restore
    void*       stub_func_addr;  // current stub in GOT (to match)
    int         restore_count;
};

// Get system page size (cached)
static long get_page_size()
{
    static long page_size = sysconf(_SC_PAGESIZE);
    return page_size;
}

// Write a new value into a GOT entry
static void write_got_entry(void** got_entry, void* new_value)
{
    const long page_size = get_page_size();
    uintptr_t page = reinterpret_cast<uintptr_t>(got_entry) & ~(page_size - 1);

    // Make the page writable. We don't restore it to read-only because
    // .got.plt pages must remain writable for the dynamic linker to resolve
    // other lazy PLT entries on the same page.
    if (mprotect(reinterpret_cast<void*>(page), page_size, PROT_READ | PROT_WRITE) != 0)
    {
        throw std::runtime_error{
            std::string{"gmock_linux: mprotect failed for "} +
            std::to_string(reinterpret_cast<uintptr_t>(got_entry)) };
    }

    *got_entry = new_value;
}

template <typename TRel>
static void process_relocations_patch(
    const TRel* rel_array, size_t rel_sz,
    const ElfW(Sym)* symtab, const char* strtab, uintptr_t base,
    PatchContext* ctx, const char* dlpi_name)
{
    if (!rel_array || rel_sz == 0) return;

    const size_t rel_count = rel_sz / sizeof(TRel);
    for (size_t i = 0; i < rel_count; i++)
    {
#if __SIZEOF_POINTER__ == 8
        const unsigned sym_idx = ELF64_R_SYM(rel_array[i].r_info);
#else
        const unsigned sym_idx = ELF32_R_SYM(rel_array[i].r_info);
#endif
        if (sym_idx == 0) continue;

        const char* sym_name = strtab + symtab[sym_idx].st_name;
        if (std::strcmp(sym_name, ctx->func_name) == 0)
        {
            void** got_entry = reinterpret_cast<void**>(base + rel_array[i].r_offset);
            
            // Only patch if not already pointing to our stub
            if (*got_entry != ctx->new_func_addr)
            {
                write_got_entry(got_entry, ctx->new_func_addr);
                ctx->patch_count++;
            }
        }
    }
}

// dl_iterate_phdr callback: scan ELF objects to find and patch ALL GOT entries for a symbol
static int find_and_patch_got_callback(struct dl_phdr_info* info, size_t /*size*/, void* data)
{
    auto* ctx = static_cast<PatchContext*>(data);
    const uintptr_t base = info->dlpi_addr;

    // 1. Find PT_DYNAMIC segment
    const ElfW(Dyn)* dyn_section = nullptr;
    for (int i = 0; i < info->dlpi_phnum; i++)
    {
        if (info->dlpi_phdr[i].p_type == PT_DYNAMIC)
        {
            dyn_section = reinterpret_cast<const ElfW(Dyn)*>(
                base + info->dlpi_phdr[i].p_vaddr);
            break;
        }
    }
    if (!dyn_section)
        return 0; // continue iteration

    // 2. Extract dynamic linking tables
    const ElfW(Sym)*  symtab     = nullptr;
    const char*       strtab     = nullptr;
    const ElfW(Rela)* jmprel     = nullptr;
    const ElfW(Rel)*  jmprel_rel = nullptr;
    size_t            pltrelsz   = 0;
    int               pltrel_type = 0;
    const ElfW(Rela)* rela       = nullptr;
    size_t            relasz     = 0;
    const ElfW(Rel)*  rel        = nullptr;
    size_t            relsz      = 0;

    for (const ElfW(Dyn)* d = dyn_section; d->d_tag != DT_NULL; d++)
    {
        switch (d->d_tag)
        {
            case DT_SYMTAB:   symtab     = reinterpret_cast<const ElfW(Sym)*>(d->d_un.d_ptr);  break;
            case DT_STRTAB:   strtab     = reinterpret_cast<const char*>(d->d_un.d_ptr);        break;
            case DT_JMPREL:   jmprel     = reinterpret_cast<const ElfW(Rela)*>(d->d_un.d_ptr);
                              jmprel_rel = reinterpret_cast<const ElfW(Rel)*>(d->d_un.d_ptr);    break;
            case DT_PLTRELSZ: pltrelsz   = d->d_un.d_val;                                       break;
            case DT_PLTREL:   pltrel_type = static_cast<int>(d->d_un.d_val);                     break;
            case DT_RELA:     rela       = reinterpret_cast<const ElfW(Rela)*>(d->d_un.d_ptr); break;
            case DT_RELASZ:   relasz     = d->d_un.d_val; break;
            case DT_REL:      rel        = reinterpret_cast<const ElfW(Rel)*>(d->d_un.d_ptr);  break;
            case DT_RELSZ:    relsz      = d->d_un.d_val; break;
        }
    }

    if (!symtab || !strtab)
        return 0; // continue

    // 3. Scan PLT relocations (DT_JMPREL)
    if (pltrelsz > 0)
    {
        if (pltrel_type == DT_RELA || pltrel_type == 0)
            process_relocations_patch(jmprel, pltrelsz, symtab, strtab, base, ctx, info->dlpi_name);
        else
            process_relocations_patch(jmprel_rel, pltrelsz, symtab, strtab, base, ctx, info->dlpi_name);
    }

    // 4. Scan regular GOT relocations (DT_RELA / DT_REL)
    process_relocations_patch(rela, relasz, symtab, strtab, base, ctx, info->dlpi_name);
    process_relocations_patch(rel, relsz, symtab, strtab, base, ctx, info->dlpi_name);

    return 0; // ALWAYS continue to next ELF object
}

template <typename TRel>
static void process_relocations_restore(
    const TRel* rel_array, size_t rel_sz,
    const ElfW(Sym)* symtab, const char* strtab, uintptr_t base,
    RestoreContext* ctx)
{
    if (!rel_array || rel_sz == 0) return;

    const size_t rel_count = rel_sz / sizeof(TRel);
    for (size_t i = 0; i < rel_count; i++)
    {
#if __SIZEOF_POINTER__ == 8
        const unsigned sym_idx = ELF64_R_SYM(rel_array[i].r_info);
#else
        const unsigned sym_idx = ELF32_R_SYM(rel_array[i].r_info);
#endif
        if (sym_idx == 0) continue;

        const char* sym_name = strtab + symtab[sym_idx].st_name;
        if (std::strcmp(sym_name, ctx->func_name) == 0)
        {
            void** got_entry = reinterpret_cast<void**>(base + rel_array[i].r_offset);
            if (*got_entry == ctx->stub_func_addr)
            {
                write_got_entry(got_entry, ctx->orig_func_addr);
                ctx->restore_count++;
            }
        }
    }
}

// dl_iterate_phdr callback: restore ALL GOT entries currently pointing to stub
static int restore_got_callback(struct dl_phdr_info* info, size_t /*size*/, void* data)
{
    auto* ctx = static_cast<RestoreContext*>(data);
    const uintptr_t base = info->dlpi_addr;

    const ElfW(Dyn)* dyn_section = nullptr;
    for (int i = 0; i < info->dlpi_phnum; i++)
    {
        if (info->dlpi_phdr[i].p_type == PT_DYNAMIC)
        {
            dyn_section = reinterpret_cast<const ElfW(Dyn)*>(
                base + info->dlpi_phdr[i].p_vaddr);
            break;
        }
    }
    if (!dyn_section)
        return 0;

    const ElfW(Sym)*  symtab     = nullptr;
    const char*       strtab     = nullptr;
    const ElfW(Rela)* jmprel     = nullptr;
    const ElfW(Rel)*  jmprel_rel = nullptr;
    size_t            pltrelsz   = 0;
    int               pltrel_type = 0;
    const ElfW(Rela)* rela       = nullptr;
    size_t            relasz     = 0;
    const ElfW(Rel)*  rel        = nullptr;
    size_t            relsz      = 0;

    for (const ElfW(Dyn)* d = dyn_section; d->d_tag != DT_NULL; d++)
    {
        switch (d->d_tag)
        {
            case DT_SYMTAB:   symtab     = reinterpret_cast<const ElfW(Sym)*>(d->d_un.d_ptr);  break;
            case DT_STRTAB:   strtab     = reinterpret_cast<const char*>(d->d_un.d_ptr);        break;
            case DT_JMPREL:   jmprel     = reinterpret_cast<const ElfW(Rela)*>(d->d_un.d_ptr);
                              jmprel_rel = reinterpret_cast<const ElfW(Rel)*>(d->d_un.d_ptr);    break;
            case DT_PLTRELSZ: pltrelsz   = d->d_un.d_val;                                       break;
            case DT_PLTREL:   pltrel_type = static_cast<int>(d->d_un.d_val);                     break;
            case DT_RELA:     rela       = reinterpret_cast<const ElfW(Rela)*>(d->d_un.d_ptr); break;
            case DT_RELASZ:   relasz     = d->d_un.d_val; break;
            case DT_REL:      rel        = reinterpret_cast<const ElfW(Rel)*>(d->d_un.d_ptr);  break;
            case DT_RELSZ:    relsz      = d->d_un.d_val; break;
        }
    }

    if (!symtab || !strtab)
        return 0;

    if (pltrelsz > 0)
    {
        if (pltrel_type == DT_RELA || pltrel_type == 0)
            process_relocations_restore(jmprel, pltrelsz, symtab, strtab, base, ctx);
        else
            process_relocations_restore(jmprel_rel, pltrelsz, symtab, strtab, base, ctx);
    }

    process_relocations_restore(rela, relasz, symtab, strtab, base, ctx);
    process_relocations_restore(rel, relsz, symtab, strtab, base, ctx);

    return 0; // ALWAYS continue to next object
}

bool initialized = false;

} // anonymous namespace

// ============================================================================
// Public API implementation
// ============================================================================

namespace gmock_linux {
namespace detail {

    thread_local int lock = 0;

    void patch_module_func(
        const char* funcName, void* funcAddr, void* newFunc, void** oldFunc)
    {
        if (!initialized)
            throw std::runtime_error{ "gmock_linux is not initialized" };

        if (!funcName || !newFunc || !oldFunc)
            throw std::runtime_error{ "gmock_linux: invalid arguments to patch_module_func" };

        // Step 1: Resolve the real function address via dlsym.
        // This also handles lazy binding — forces the symbol to be resolved
        // even if the GOT entry hasn't been lazily resolved yet.
        void* resolved = dlsym(RTLD_DEFAULT, funcName);
        if (!resolved)
        {
            // Try RTLD_NEXT as fallback
            resolved = dlsym(RTLD_NEXT, funcName);
        }
        if (!resolved)
        {
            throw std::runtime_error{
                std::string{"gmock_linux: failed to resolve symbol '"} + funcName +
                "': " + (dlerror() ? dlerror() : "unknown error") };
        }

        // Step 2: Find and patch ALL GOT entries for this symbol across all ELF objects
        *oldFunc = resolved;

        PatchContext ctx{};
        ctx.func_name      = funcName;
        ctx.real_func_addr = resolved;
        ctx.new_func_addr  = newFunc;
        ctx.patch_count    = 0;

        dl_iterate_phdr(find_and_patch_got_callback, &ctx);

        if (ctx.patch_count == 0)
        {
            throw std::runtime_error{
                std::string{"gmock_linux: failed to find GOT entry for '"} + funcName + "'" };
        }
    }

    void restore_module_func(
        const char* funcName, void* origFunc, void* stubFunc, void** oldFunc)
    {
        if (!initialized)
            throw std::runtime_error{ "gmock_linux is uninitialized" };

        if (!funcName || !origFunc || !stubFunc)
            throw std::runtime_error{ "gmock_linux: invalid arguments to restore_module_func" };

        RestoreContext ctx{};
        ctx.func_name      = funcName;
        ctx.orig_func_addr = origFunc;
        ctx.stub_func_addr = stubFunc;
        ctx.restore_count  = 0;

        dl_iterate_phdr(restore_got_callback, &ctx);

        if (ctx.restore_count == 0)
        {
            throw std::runtime_error{
                std::string{"gmock_linux: failed to restore GOT entry for '"} + funcName + "'" };
        }

        if (oldFunc)
            *oldFunc = nullptr;
    }

    proxy_base::~proxy_base() noexcept
    {
        --lock;
    }

} // namespace detail

    void initialize()
    {
        if (initialized)
            throw std::runtime_error{ "gmock_linux: already initialized" };

        initialized = true;
    }

    void uninitialize() noexcept
    {
        initialized = false;
    }

    bypass_mocks::bypass_mocks() noexcept
    {
        ++detail::lock;
    }

    bypass_mocks::~bypass_mocks() noexcept
    {
        --detail::lock;
    }

} // namespace gmock_linux
