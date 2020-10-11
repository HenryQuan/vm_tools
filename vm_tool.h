/* 
 Originally based on Razzile & HackJack's vm_writeData,
 Modified and formatted by HenryQuan.
 Repo: https://github.com/HenryQuan/zhao
 
 This header includes functions necessary for virtual memory searching and writing.
 This is built to search only the binary section.
 Only tested on iOS 13.7 ARM64, Mac OS and ARMv7 are not tested.
 Use this only for educational or research purposes.
 MIT LICENSE

 References
 - PsychoBird's SearchKit, https://github.com/PsychoBird/RevelariOS
*/

#ifndef VM_TOOL_H
#define VM_TOOL_H

#include <mach/mach.h>
#include <mach-o/dyld.h>

#define MAX_DATA_LENGTH 128
#define FALSE 0
#define NOT_FOUND 0
// If you are using objc, use the second one
//#define LOG printf(
#define LOG NSLog(@

typedef unsigned char byte_t;
typedef unsigned long long hex_t;

typedef struct module
{
    /// The address/offset for this module
    vm_address_t address;
    /// The original data at address, this shouldn't be changed
    byte_t original[MAX_DATA_LENGTH / 2];
    /// Hex string for searching
    char search[MAX_DATA_LENGTH];
    /// Hex string to replace the original one, MUST be the same length as search
    char replace[MAX_DATA_LENGTH];
    /// The offset to the true address
    int offset;
} Module;

/// Print a single module
static void printModule(Module m) {
    LOG"[VM_TOOL] Address: 0x%lx\nSearch - %lu\nReplace - %lu\nOffset - %d\n",
        m.address, strlen(m.search), strlen(m.replace), m.offset);
}

/// Print a list of modules
void printModuleList(Module *list, int size) {
    for (int i = 0; i < size; i++)
        printModule(list[i]);
}

/// Check if the process has ASLR/offset
static bool hasASLR()
{
    const struct mach_header *mach;
    mach = _dyld_get_image_header(0);
    // check the flag here
    return mach->flags & MH_PIE;
}

/// Return the offset of the process
static vm_address_t getOffset()
{
    return _dyld_get_image_vmaddr_slide(0);
}

/// Add offset to the address if available and return the address in memory
static vm_address_t memoryAddress(vm_address_t address)
{
    if (hasASLR())
        return getOffset() + address;
    return address;
}

/// Convert string to bytes
static byte_t *convert(char data[MAX_DATA_LENGTH])
{
    LOG"[VM_TOOL] Converting %s\n", data);
    unsigned long dataLen = strlen(data);
    // The character count must be even and not over the max length
    if (dataLen == 0 || dataLen > MAX_DATA_LENGTH || dataLen % 2 != 0)
    {
        LOG"[VM_TOOL] Conversion failed\n");
        return NULL;
    }

    unsigned long hexLen = dataLen / 2;
    byte_t *hex = (byte_t *)calloc(0, sizeof(byte_t) * hexLen);
    // Join two char together and convert it to a byte
    for (int i = 0; i < hexLen; i++)
    {
        int index = i * 2;
        char hexBytes[2] = {data[index], data[index + 1]};
        hex[i] = (byte_t)strtol(hexBytes, NULL, 16);
    }
    return hex;
}

/// Write data to a module
/// m - module
/// replace - use the replace string if true and use the original if false
void vm_writeData(Module m, int replace)
{
    // No address found
    if (m.address == NOT_FOUND)
        return;

    kern_return_t err;
    byte_t *hex;
    mach_port_t port = mach_task_self();
    // Add offset to get the true address
    vm_address_t address = memoryAddress(m.address);
    // the size should be the string length / 2 and that's it, shared by both
    unsigned long hexSize;

    LOG"[VM_TOOL] Write to 0x%lx (0x%lx)\n", address, m.address);

    if (replace > 0)
    {
        hexSize = strlen(m.replace) / 2;
        // add offset only in replace mode
        address += m.offset;
        // Override to the value we want
        hex = convert(m.replace);
        if (hex == NULL)
            return;
    }
    else
    {
        // original save everything
        hexSize = strlen(m.search) / 2;
        // Write back the original value or replacing the original
        hex = m.original;
    }

    // set memory protection
    err = vm_protect(port, address, hexSize, FALSE, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
    if (err != KERN_SUCCESS)
        return;

    // write and remove protection
    vm_write(port, address, (vm_offset_t)hex, (mach_msg_type_number_t)hexSize);
    vm_protect(port, address, hexSize, FALSE, VM_PROT_READ | VM_PROT_EXECUTE);

    // We only want to free up for replacing because we might use original later, it will be freed up when the program stops
    if (replace > 0)
        free(hex);
}

/// Read the original hex at the address for all modules
/// moduleList - an array of modules
/// size - the size of the module list
void vm_readData(Module *moduleList, int size)
{
    mach_port_t port = mach_task_self();
    
    for (int i = 0; i < size; i++)
    {
        Module *curr = moduleList + i;
        // Check if address is valid
        if (curr->address == 0)
        {
            LOG"[VM_TOOL] Module %d's address is not set", i);
            continue;;
        }
        
        // Check if string is entered
        unsigned long hexLen = strlen((char *)curr->search);
        if (hexLen == 0 || hexLen % 2 != 0)
        {
            LOG"[VM_TOOL] Module %d's original string is not set or not valid", i);
            continue;
        }
        
        vm_size_t bytes = hexLen / 2;
        vm_address_t currAddress = memoryAddress(curr->address);
        
        // Clear original
        memset(curr->original, 0, hexLen);
        // Read hex to original
        kern_return_t err = vm_read_overwrite(port, currAddress, bytes, (vm_offset_t)curr->original, &bytes);
        if (err != KERN_SUCCESS)
        {
            LOG"[VM_TOOL] Error while reading at address 0x%lx", currAddress);
            return;
        }
    }
}

/// Free a byte list
/// list - the byte list
/// size - the size of list
static void freeByteList(byte_t **list, int size)
{
    if (list == NULL)
        return;

    // Free everything inside the list
    for (int i = 0; i < size; i++)
        free(list[i]);
    free(list);
}

/// Search and set the address for all modules
/// moduleList - an array of modules
/// size - the size of the module list
/// binarySize - the size of the app binary or any number for the end address
void vm_searchData(Module *moduleList, int size, hex_t binarySize)
{
    // Convert search to actual hex values
    byte_t **hex = (byte_t **)malloc(sizeof(byte_t *) * size);
    int errorCount = 0;
    for (int i = 0; i < size; i++)
    {
        hex[i] = convert(moduleList[i].search);
        if (hex[i] == NULL)
            errorCount++;
    }

    // Return early if everything are NULL
    if (errorCount == size)
    {
        freeByteList(hex, size);
        return;
    }

    kern_return_t err;
    mach_port_t port = mach_task_self();
    // Get offset, start and end addresses
    vm_address_t aslr = getOffset();
    vm_address_t offset = aslr + 0x100000000;
    vm_address_t start = offset;
    vm_address_t end = start + binarySize;

    LOG"[VM_TOOL] Binary: 0x%lx - 0x%lx\n", start, end);

    vm_address_t chunk = getpagesize();
    LOG"[VM_TOOL] Reading 0x%lx per chunk\n", chunk);
    vm_size_t bytes = chunk;
    byte_t binary[chunk];
    // Check how many addresses we have found, setting it to error count ignores error
    int found = errorCount;

    for (vm_address_t currAddress = start; currAddress < end; currAddress += chunk)
    {
        err = vm_read_overwrite(port, currAddress, bytes, (vm_offset_t)&binary, &bytes);
        if (err != KERN_SUCCESS)
        {
            LOG"[VM_TOOL] Error while reading at address 0x%lx", currAddress);
            return;
        }
        
        for (int i = 0; i < chunk; i++)
        {
            // Check if anything matches with the list
            for (int j = 0; j < size; j++)
            {
                // This is the only way to read
                Module *currModule = moduleList + j;
                unsigned long hexLen = strlen(currModule->search) / 2;
                byte_t *currHex = hex[j];
                // Ignore incorrect hex
                if (currHex == NULL)
                    continue;

                // Check if it matches with the first
                if (currHex[0] == binary[i])
                {
                    currModule->original[0] = binary[i];
                    // only if it matches, increase k so using it as a counter
                    int k = 1;
                    while (k < hexLen)
                    {
                        if (currHex[k] != binary[i + k])
                            break;

                        // Save the byte
                        currModule->original[k] = binary[i + k];
                        k++;
                    }

                    // found the address
                    if (k == hexLen)
                    {
                        // A temp fix so that we won't find duplicate addresses
                        if (currModule->address == 0)
                        {
                            // (memory address - aslr) gets the right address in IDA
                            // currAddress is the start of this chunk so we need to add the current offset
                            currModule->address = (currAddress + i) - aslr;
                            LOG"[VM_TOOL] Found module %d at 0x%lx\n", j, currModule->address);
                            found++;
                        }
                    }

                    // Everything has found so return early
                    if (found == size)
                    {
                        freeByteList(hex, size);
                        return;
                    }
                }
            }
        }
    }

    freeByteList(hex, size);
    return;
}

#endif
