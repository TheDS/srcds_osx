/**
 * vim: set ts=4 :
 * =============================================================================
 * Source Dedicated Server NG - Game API Library
 * Copyright (C) 2011-2013 Scott Ehlert and AlliedModders LLC.
 * All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "Steamworks SDK," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.
 */

#include "HSGameLib.h"
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#if defined(PLATFORM_MACOSX)
#include <mach/task.h>
#include <mach-o/dyld_images.h>
#include <mach-o/loader.h>
#elif defined(PLATFORM_LINUX)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif

#if defined(PLATFORM_MACOSX)
static struct dyld_all_image_infos *GetDyldImageInfo()
{
    static struct dyld_all_image_infos *infos = nullptr;
    
    if (!infos)
    {
        task_dyld_info_data_t dyld_info;
        mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
        
        if (task_info(mach_task_self(), TASK_DYLD_INFO, (task_info_t)&dyld_info, &count)
            == KERN_SUCCESS)
        {
            infos = (struct dyld_all_image_infos *)dyld_info.all_image_info_addr;
        }
        else
        {
#if defined(PLATFORM_X64)
			// TODO: Fix this?
#else
            struct nlist symList[2];
            memset(symList, 0, sizeof(symList));

            symList[0].n_un.n_name = (char *)"_dyld_all_image_infos";
            nlist("/usr/lib/dyld", symList);

            infos = (struct dyld_all_image_infos *)symList[0].n_value;
#endif
        }
	}

    return infos;
}
#endif

int HSGameLib::SetLibraryPath(const char *path)
{
#if defined(PLATFORM_MACOSX)
	typedef void (*SetEnv_t)(const char *, const char *);
	static SetEnv_t DyldSetEnv = nullptr;
	
	if (!DyldSetEnv)
	{
		int ret = setenv("DYLD_LIBRARY_PATH", path, 1);
		if (ret != 0)
		{
			return ret;
		}
	
		SymbolTable dyldSyms;
		dyldSyms.Initialize();

#if defined(PLATFORM_X64)
    	struct mach_header_64 *fileHdr;
    	struct load_command *loadCmds;
    	struct segment_command_64 *linkEditHdr = nullptr;
    	struct symtab_command *symTableHdr = nullptr;
#else
    	struct mach_header *fileHdr;
    	struct load_command *loadCmds;
    	struct segment_command *linkEditHdr = nullptr;
    	struct symtab_command *symTableHdr = nullptr;
#endif
    	uint32_t loadCmdCount = 0;
    	uintptr_t linkEditAddr = 0;
    	RawSymbolTable symbolTable;
    	const char *stringTable;
    	uint32_t symbolCount;
    
    	struct dyld_all_image_infos *infos = GetDyldImageInfo();
    	//const struct dyld_image_info &info = infos->infoArray[0];
    	uintptr_t baseAddr = uintptr_t(infos->dyldImageLoadAddress);//uintptr_t(info.imageLoadAddress);
    	

#if defined(PLATFORM_X64)
    	fileHdr = (struct mach_header_64 *)baseAddr;
    	loadCmds = (struct load_command *)(baseAddr + sizeof(mach_header_64));		
#else
    	fileHdr = (struct mach_header *)baseAddr;
    	loadCmds = (struct load_command *)(baseAddr + sizeof(mach_header));
#endif
    	loadCmdCount = fileHdr->ncmds;
    
    	for (uint32_t i = 0; i < loadCmdCount; i++)
    	{
#if defined(PLATFORM_X64)
        	if (loadCmds->cmd == LC_SEGMENT_64 && !linkEditHdr)
        	{
            	struct segment_command_64 *seg = (struct segment_command_64 *)loadCmds;
#else
        	if (loadCmds->cmd == LC_SEGMENT && !linkEditHdr)
        	{
            	struct segment_command *seg = (struct segment_command *)loadCmds;
#endif
            	if (strcmp(seg->segname, "__LINKEDIT") == 0)
            	{
                	linkEditHdr = seg;
                
                	// If the symbol table header has also been found, then the search can stop
                	if (symTableHdr)
                    	break;
            	}
        	}
        	else if (loadCmds->cmd == LC_SYMTAB)
        	{
            	symTableHdr = (struct symtab_command *)loadCmds;
            
            	if (linkEditHdr)
                	break;
        	}
        
        	loadCmds = (struct load_command *)(uintptr_t(loadCmds) + loadCmds->cmdsize);
    	}
    
    	if (!linkEditHdr || !symTableHdr || !symTableHdr->symoff || !symTableHdr->stroff)
        	return -1;
    
    	linkEditAddr = baseAddr + linkEditHdr->vmaddr;
    	symbolTable = (RawSymbolTable)(linkEditAddr + symTableHdr->symoff - linkEditHdr->fileoff);
    	stringTable = (const char *)(linkEditAddr + symTableHdr->stroff - linkEditHdr->fileoff);
    	symbolCount = symTableHdr->nsyms;
    
    	Symbol *entry = nullptr;
    
    	for (uint32_t i = 0; i < symbolCount; i++)
    	{
    	#if defined(PLATFORM_X64)
        	struct nlist_64 &sym = symbolTable[i];
        #else
        	struct nlist &sym = symbolTable[i];
        #endif

        	// Ignore the prepended underscore on all symbols to match dlsym() functionality
        	const char *symName = stringTable + sym.n_un.n_strx + 1;

        
        	// Skip undefined symbls
        	if (sym.n_sect == NO_SECT)
            	continue;
        
        	Symbol *currentSymbol;
        	currentSymbol = dyldSyms.InternSymbol(symName,
            	                                  strlen(symName),
                	                              (void *)(baseAddr + sym.n_value));
        
        	if (strcmp("_ZL18_dyld_set_variablePKcS0_", symName) == 0)
        	{
            	entry = currentSymbol;
            	break;
        	}
    	}
    
    	DyldSetEnv = entry ? SetEnv_t(entry->address) : nullptr;
    }
    
    assert(DyldSetEnv);
    DyldSetEnv("DYLD_LIBRARY_PATH", path);
    return 0;
#else
	return 0;
#endif
}

HSGameLib::HSGameLib()
    : GameLib(), baseAddress_(0), lastPosition_(0), valid_(false), fileHeader_(nullptr), mapSize_(0), searchSize_(0)
{

}

HSGameLib::HSGameLib(const char *name)
    : GameLib(name), baseAddress_(0), lastPosition_(0), valid_(false), fileHeader_(nullptr), mapSize_(0), searchSize_(0)
{
    if (!IsLoaded())
        return;

    Initialize();
}

HSGameLib::~HSGameLib()
{
#if defined(PLATFORM_LINUX)
	if (fileHeader_ != nullptr && mapSize_ > 0)
		munmap(fileHeader_, mapSize_);
#endif
}

bool HSGameLib::Load(const char *name)
{
    if (IsLoaded())
        Close();
    
    if (GameLib::Load(name))
    {
        Invalidate();
        Initialize();
    }
    
    return IsValid();
}

bool HSGameLib::IsValid() const
{
    return valid_;
}

size_t HSGameLib::ResolveHiddenSymbols(SymbolInfo *list, const char **names)
{
    size_t invalid = 0;
    
    while (*names && *names[0])
    {
        SymbolInfo *info = list++;
        const char *name = *names;

        void *addr = GetHiddenSymbolAddr(name);
        
        info->name = name;
        
        if (addr)
            info->address = addr;
        else
            invalid++;
        
        names++;
    }
    
    return invalid;
}

void HSGameLib::Initialize()
{
#if defined(PLATFORM_MACOSX)
#if defined(PLATFORM_X64)
    struct mach_header_64 *fileHdr;
    struct load_command *loadCmds;
    struct segment_command_64 *linkEditHdr = nullptr;
    struct symtab_command *symTableHdr = nullptr;
#else
	struct mach_header *fileHdr;
	struct load_command *loadCmds;
    struct segment_command *linkEditHdr = nullptr;
    struct symtab_command *symTableHdr = nullptr;
#endif
    uint32_t loadCmdCount = 0;
    uintptr_t linkEditAddr = 0;
    
    baseAddress_ = GetBaseAddress();
    
    if (!baseAddress_)
        return;

	fileHeader_ = (void *)baseAddress_;
    
    // Initialize symbol hash table
    table_.Initialize();

#if defined(PLATFORM_X64)
    fileHdr = (struct mach_header_64 *)baseAddress_;
    loadCmds = (struct load_command *)(baseAddress_ + sizeof(mach_header_64));
#else
    fileHdr = (struct mach_header *)baseAddress_;
    loadCmds = (struct load_command *)(baseAddress_ + sizeof(mach_header));
#endif
    
    loadCmdCount = fileHdr->ncmds;
    
    for (uint32_t i = 0; i < loadCmdCount; i++)
    {
#if defined(PLATFORM_X64)
        if (loadCmds->cmd == LC_SEGMENT_64 && !linkEditHdr)
        {
            struct segment_command_64 *seg = (struct segment_command_64 *)loadCmds;
#else
        if (loadCmds->cmd == LC_SEGMENT && !linkEditHdr)
        {
            struct segment_command *seg = (struct segment_command *)loadCmds;
#endif
            if (strcmp(seg->segname, "__LINKEDIT") == 0)
            {
                linkEditHdr = seg;
                
                // If the symbol table header has also been found, then the search can stop
                if (symTableHdr)
                    break;
            }
        }
        else if (loadCmds->cmd == LC_SYMTAB)
        {
            symTableHdr = (struct symtab_command *)loadCmds;
            
            if (linkEditHdr)
                break;
        }
        
        loadCmds = (struct load_command *)(uintptr_t(loadCmds) + loadCmds->cmdsize);
    }
	
#if defined(PLATFORM_X64)
    segment_command_64 *seg = (struct segment_command_64 *)(baseAddress_ + sizeof(mach_header_64));
#else
    segment_command *seg = (struct segment_command *)(baseAddress_ + sizeof(mach_header));
#endif

	for (uint32_t i = 0; i < loadCmdCount; i++)
	{
#if defined(PLATFORM_X64)
		if (seg->cmd == LC_SEGMENT_64)
#else
		if (seg->cmd == LC_SEGMENT)
#endif
		{
			searchSize_ += seg->vmsize;
		}
	
#if defined(PLATFORM_X64)
		seg = (struct segment_command_64 *)(uintptr_t(seg) + seg->cmdsize);
#else
		seg = (struct segment_command *)(uintptr_t(seg) + seg->cmdsize);
#endif
	}

    if (!linkEditHdr || !symTableHdr || !symTableHdr->symoff || !symTableHdr->stroff)
        return;
    
    linkEditAddr = baseAddress_ + linkEditHdr->vmaddr;
    symbolTable_ = (RawSymbolTable)(linkEditAddr + symTableHdr->symoff - linkEditHdr->fileoff);
    stringTable_ = (const char *)(linkEditAddr + symTableHdr->stroff - linkEditHdr->fileoff);
    symbolCount_ = symTableHdr->nsyms;

    valid_ = true;
#elif defined(PLATFORM_LINUX)
	struct link_map *dlmap;
	struct stat dlstat;
	int dlfile;
	uintptr_t map_base;
#if defined(PLATFORM_X64)
	Elf64_Ehdr *file_hdr;
	Elf64_Shdr *sections, *shstrtab_hdr, *symtab_hdr = nullptr, *strtab_hdr = nullptr;
	Elf64_Sym *symtab;
	Elf64_Phdr *phdr;
#else
	Elf32_Ehdr *file_hdr;
	Elf32_Shdr *sections, *shstrtab_hdr, *symtab_hdr = nullptr, *strtab_hdr = nullptr;
	Elf32_Sym *symtab;
	Elf32_Phdr *phdr;
#endif
	const char *shstrtab, *strtab;
	uint16_t section_count;
	uint32_t symbol_count;
	uint16_t phdr_count;

    baseAddress_ = GetBaseAddress();
    
    if (!baseAddress_)
        return;
    
    // Initialize symbol hash table
    table_.Initialize();

	dlmap = (struct link_map *)handle_;

	dlfile = open(dlmap->l_name, O_RDONLY);
	if (dlfile == -1 || fstat(dlfile, &dlstat) == -1)
	{
		close(dlfile);
		return;
	}

#if defined(PLATFORM_X64)
	file_hdr = (Elf64_Ehdr *)mmap(NULL, dlstat.st_size, PROT_READ, MAP_PRIVATE, dlfile, 0);
#else
	file_hdr = (Elf32_Ehdr *)mmap(NULL, dlstat.st_size, PROT_READ, MAP_PRIVATE, dlfile, 0);
#endif

	map_base = (uintptr_t)file_hdr;

	if (file_hdr == MAP_FAILED)
	{
		close(dlfile);
		return;
	}
	close(dlfile);

	if (file_hdr->e_shoff == 0 || file_hdr->e_shstrndx == SHN_UNDEF)
	{
		munmap(file_hdr, dlstat.st_size);
		return;
	}

#if defined(PLATFORM_X64)
	sections = (Elf64_Shdr *)(map_base + file_hdr->e_shoff);
	phdr = (Elf64_Phdr *)(map_base + file_hdr->e_phoff);
#else
	sections = (Elf32_Shdr *)(map_base + file_hdr->e_shoff);
	phdr = (Elf32_Phdr *)(map_base + file_hdr->e_phoff);
#endif
	section_count = file_hdr->e_shnum;
	phdr_count = file_hdr->e_phnum;

	/* Get ELF section header string table */
	shstrtab_hdr = &sections[file_hdr->e_shstrndx];
	shstrtab = (const char *)(map_base + shstrtab_hdr->sh_offset);

	/* Iterate sections while looking for ELF symbol table and string table */
	for (uint16_t i = 0; i < section_count; i++)
	{
#if defined(PLATFORM_X64)
		Elf64_Shdr &hdr = sections[i];
#else
		Elf32_Shdr &hdr = sections[i];
#endif
		const char *section_name = shstrtab + hdr.sh_name;

		if (strcmp(section_name, ".symtab") == 0)
		{
			symtab_hdr = &hdr;
		}
		else if (strcmp(section_name, ".strtab") == 0)
		{
			strtab_hdr = &hdr;
		}
	}

	#define PAGE_SIZE			4096
	#define PAGE_ALIGN_UP(x)	((x + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

	for (uint16_t i = 0; i < phdr_count; i++)
	{
#if defined(PLATFORM_X64)
		Elf64_Phdr &hdr = phdr[i];
#else
		Elf32_Phdr &hdr = phdr[i];
#endif

		if (hdr.p_type == PT_LOAD && hdr.p_flags == (PF_X|PF_R))
			searchSize_ += PAGE_ALIGN_UP(hdr.p_filesz);
	}
	
	/* Uh oh, we don't have a symbol table or a string table */
	if (symtab_hdr == NULL || strtab_hdr == NULL)
	{
		munmap(file_hdr, dlstat.st_size);
		return;
	}

	fileHeader_ = (void *)file_hdr;
	mapSize_ = dlstat.st_size;
	symbolTable_ = (RawSymbolTable)(map_base + symtab_hdr->sh_offset);
	stringTable_ = (const char *)(map_base + strtab_hdr->sh_offset);
	symbolCount_ = symtab_hdr->sh_size / symtab_hdr->sh_entsize;

	valid_ = true;
#else
#error "Unsupported platform."
#endif
}

void HSGameLib::Invalidate()
{
    if (!table_.IsEmpty())
        table_.Destroy();

#if defined(PLATFORM_LINUX)
	if (fileHeader_ != nullptr && mapSize_ > 0)
		munmap(fileHeader_, mapSize_);
#endif

	fileHeader_ = nullptr;
	mapSize_ = 0;

    valid_ = false;
}

#if defined(PLATFORM_LINUX)
int HSGameLib::baseaddr_callback(struct dl_phdr_info *info, size_t size, void *data)
{
	void *handle = dlopen(info->dlpi_name, RTLD_NOLOAD);
	HSGameLib *lib = (HSGameLib *)data;
	if (handle == lib->handle_)
		lib->baseAddress_ = info->dlpi_addr;
	dlclose(handle);

	return lib->baseAddress_ ? 1 : 0;
}
#endif

uintptr_t HSGameLib::GetBaseAddress()
{
    Dl_info info;
    void *factory;
    uintptr_t base = 0;
    
    factory = (void *)GetFactory();
    
    // First try using dladdr() with the public factory symbol
    if (factory)
    {
        if (dladdr((void *)factory, &info) && info.dli_fbase && info.dli_fname)
            base = (uintptr_t)info.dli_fbase;
    }

#if defined(PLATFORM_MACOSX)
    // If the library doesn't have a factory symbol or dladdr() failed, try looking through all the
    // libraries loaded in the process for a matching handle.
    if (!base)
    {
        struct dyld_all_image_infos *infos = GetDyldImageInfo();
        
        if (infos)
        {
            uint32_t imageCount = infos->infoArrayCount;
        
            for (uint32_t i = 1; i < imageCount; i++)
            {
                const struct dyld_image_info &info = infos->infoArray[i];
            
                void *handle = dlopen(info.imageFilePath, RTLD_NOLOAD);
                if (handle == handle_)
                {
                    base = (uintptr_t)info.imageLoadAddress;
                    dlclose(handle);
                    break;
                }
            }
        }
    }
#elif defined(PLATFORM_LINUX)
	if (!base)
		dl_iterate_phdr(baseaddr_callback, this);
#endif
    
    return base;
}

void *HSGameLib::GetHiddenSymbolAddr(const char *symbol)
{
    Symbol *entry;

    if (!baseAddress_)
        return nullptr;
    
    // In the best case, the symbol has already been cached
    entry = table_.FindSymbol(symbol, strlen(symbol));
    if (entry)
        return entry->address;
    
    for (uint32_t i = lastPosition_; i < symbolCount_; i++)
    {
#if defined(PLATFORM_MACOSX)
    	#if defined(PLATFORM_X64)
        	struct nlist_64 &sym = symbolTable_[i];
        #else
        	struct nlist &sym = symbolTable_[i];
        #endif
        
        // Ignore the prepended underscore on all symbols to match dlsym() functionality
        const char *symName = stringTable_ + sym.n_un.n_strx + 1;
        
        // Skip undefined symbols
        if (sym.n_sect == NO_SECT)
            continue;
#elif defined(PLATFORM_LINUX)
	#if defined(PLATFORM_X64)
		Elf64_Sym &sym = symbolTable_[i];
		unsigned char symType = ELF64_ST_TYPE(sym.st_info);
	#else
		Elf32_Sym &sym = symbolTable_[i];
		unsigned char symType = ELF32_ST_TYPE(sym.st_info);
	#endif
	
	const char *symName = stringTable_ + sym.st_name;

	// Skip symbols that are undefined or do not refer to functions or objects
	if (sym.st_shndx == SHN_UNDEF || (symType != STT_FUNC && symType != STT_OBJECT))
		continue;
#endif
        
        Symbol *currentSymbol;
        currentSymbol = table_.InternSymbol(symName,
                                            strlen(symName),
#if defined(PLATFORM_MACOSX)
                                            (void *)(baseAddress_ + sym.n_value));
#elif defined(PLATFORM_LINUX)
                                            (void *)(baseAddress_ + sym.st_value));		
#endif
        
        if (strcmp(symbol, symName) == 0)
        {
            entry = currentSymbol;
            lastPosition_ = ++i;
            break;
        }
    }
    
    return entry ? entry->address : nullptr;
}

void *HSGameLib::FindPattern(const char *pattern, size_t len)
{
	// Algorithm based on Boyer-Moore-Horspool string search with addition of wildcard handling
	// See: https://en.wikipedia.org/wiki/Boyer%E2%80%93Moore%E2%80%93Horspool_algorithm
	
	const char wildcard = '\x2A';
	size_t bad_shift[UCHAR_MAX + 1];
	size_t last = len - 1;
	size_t idx = 0;
	size_t searchLen = searchSize_;
	char *ptr = reinterpret_cast<char *>(baseAddress_);
	
	// First locate the first rightmost wildcard
	// Loop over pattern until first wildcard is found or we reach the end
	while (idx < last && pattern[idx] != wildcard)
		idx++;
	// Loop over pattern wildcards until non-wildcard is reached
	while (idx < last && pattern[idx] == wildcard)
		idx++;

	if (idx == last)
		idx = -1;  // wildcard not found in pattern
	else
		idx--;     // wildcard found in pattern, adjust index

	// Initialize bad character shift table, accounting for wildcards in the pattern
	for (size_t i = 0; i <= UCHAR_MAX; i++)
		bad_shift[i] = last - idx;

	// Set values in bad character shift table for characters in pattern
	for (size_t i = 0; i < last; i++)
		bad_shift[(unsigned char)pattern[i]] = last - i;
	
	// Search memory for the pattern
	while (searchLen >= len)
	{
		// Search going backwards from last character
		size_t i;
		for (i = last; pattern[i] == wildcard || ptr[i] == pattern[i]; i--)
		{
			if (i == 0)
				return ptr;
		}

		// Skip ahead based on bad character shift table
		unsigned char lastChar = ptr[i];
		searchLen -= bad_shift[lastChar];
		ptr += bad_shift[lastChar];
	}

	return nullptr;
}

