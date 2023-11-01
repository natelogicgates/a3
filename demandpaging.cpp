#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include <cmath>
#include <cstring>
#include <stdio.h>
#include <string.h>
#include <ctime>
#include "log_helpers.h" 
#include "demandpaging.h"

// Define constants
const int PAGE_SIZE = 4096;
const int PAGE_TABLE_ENTRIES = 1024;
const int ADDRESS_SPACE = 32;

// Page Table Entry class
class PTE {
public:
    bool valid;
    int PFN;
    int timestamp;
    PTE* next_level;

    // Constructor initializes PTE
    PTE() : valid(false), PFN(-1), timestamp(-1), next_level(nullptr) {}
};

// Frame class represents a frame in physical memory
class Frame {
public:
    int PFN;
    bool free;
    int VPN;
    int timestamp;

    // Constructor initializes a frame with a given PFN
    Frame(int pfn) : PFN(pfn), free(true), VPN(-1), timestamp(-1) {}
};

// Memory Management class handles page table and frame allocation
class MemoryManagement {
    std::vector<Frame> frames;
    PTE* topLevel;
    int clock_hand;
    LogOptionsType logOptions;

public:
    int numOfPageReplaces = 0;
    int pageTableHits = 0;
    int numOfAddresses = 0;
    int numOfFramesAllocated = 0;
    unsigned long totalBytesUsed = 0;

    // Constructor initializes memory management
    MemoryManagement(size_t num_frames, LogOptionsType logOpt) : clock_hand(0), logOptions(logOpt) {
        for (size_t i = 0; i < num_frames; i++) {
            frames.push_back(Frame(i));
        }
        topLevel = new PTE[PAGE_TABLE_ENTRIES];
    }

    // Allocates a frame to a page
    void allocateFrameToPage(int vpn, int frameNumber) {
        numOfFramesAllocated++;
        totalBytesUsed = numOfFramesAllocated * PAGE_SIZE;

        PTE* currentLevel = topLevel;
        for (int i = ADDRESS_SPACE - 1; i >= 0; i--) {
            int index = (vpn >> i) & (PAGE_TABLE_ENTRIES - 1);
            
            if (i == 0) {
                currentLevel[index].valid = true;
                currentLevel[index].PFN = frameNumber;
                frames[frameNumber].free = false;
                frames[frameNumber].VPN = vpn;
                frames[frameNumber].timestamp = std::time(nullptr);
            } else {
                if (!currentLevel[index].next_level) {
                    currentLevel[index].next_level = new PTE[PAGE_TABLE_ENTRIES];
                }
                currentLevel = currentLevel[index].next_level;
            }
        }

        logPageTableMapping(vpn, frameNumber, false);
    }

    // Translates a virtual address to a physical address
    int translateAddress(int virtual_address, char accessMode) {
        numOfAddresses++;

        int offset = virtual_address % PAGE_SIZE;
        int vpn = virtual_address / PAGE_SIZE;
        int pa = -1;

        PTE* currentLevel = topLevel;
        for (int i = ADDRESS_SPACE - 1; i >= 0; i--) {
            int index = (vpn >> i) & (PAGE_TABLE_ENTRIES - 1);
            if (currentLevel[index].valid) {
                if (i == 0) {
                    pa = (currentLevel[index].PFN * PAGE_SIZE) + offset;
                    pageTableHits++;
                } else {
                    currentLevel = currentLevel[index].next_level;
                }
            } else {
                handlePageFault(vpn);
                return -1;
            }
        }

        logVirtualToPhysicalAddressTranslation(virtual_address, pa);
        return pa;
    }

    // Handles a page fault
    void handlePageFault(int vpn) {
        int frameNumber = findFreeFrame();
        if (frameNumber == -1) {
            frameNumber = runWSClock();
            int replacedVpn = frames[frameNumber].VPN;
            numOfPageReplaces++;
            logPageTableMapping(vpn, frameNumber, true);
        } else {
            logPageTableMapping(vpn, frameNumber, false);
        }
        allocateFrameToPage(vpn, frameNumber);
    }

    // Finds a free frame
    int findFreeFrame() {
        for (size_t i = 0; i < frames.size(); i++) {
            if (frames[i].free) {
                return i;
            }
        }
        return -1;
    }

    // Runs the WSClock algorithm to find a frame to replace
    int runWSClock() {
        while (true) {
            Frame& frame = frames[clock_hand];
            if (frame.free) {
                return clock_hand;
            }
            clock_hand = (clock_hand + 1) % frames.size();
        }
    }

    // Logs page table mappings
    void logPageTableMapping(int vpn, int frameNumber, bool isPageReplacement) {
        if (logOptions.vpn2pfn_with_pagereplace && isPageReplacement) {
            log_mapping(vpn, frameNumber, frames[frameNumber].VPN, false);
        } else if (logOptions.vpns_pfn) {
            log_mapping(vpn, frameNumber, -1, true);
        }
    }

    // Logs virtual to physical address translations
    void logVirtualToPhysicalAddressTranslation(int virtualAddress, int physicalAddress) {
        if (logOptions.addressTranslation) {
            log_va2pa(virtualAddress, physicalAddress);
        }
    }
};

// Main function handles command line arguments and runs the simulation
int main(int argc, char *argv[]) {
    
    // Check for correct number of arguments
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " trace_file readwrite_file num_frames" << std::endl;
        return 1;
    }

    // Parse command line arguments
    std::string traceFilePath = argv[1];
    std::string readWriteFilePath = argv[2];
    int num_frames = std::stoi(argv[3]);

    LogOptionsType logOptions;
    logOptions.pagetable_bitmasks = true;
    logOptions.addressTranslation = true;
    logOptions.vpns_pfn = true;
    logOptions.vpn2pfn_with_pagereplace = true;
    logOptions.offset = false;
    logOptions.summary = false;
    
    int n = 0; // Number of memory accesses
    int ageOfLastAccess = 0; // Age of last access considered recent

    int cnt = 0;
    for (int i = 4; i < argc; i++) {
        if (strcmp(argv[i], "readwrites.txt") == 0) {
            int bit1 = atoi(argv[i+1]); 
            int bit2 = atoi(argv[i+2]);
            int bit3 = atoi(argv[i+3]);
            cnt = bit1 + bit2 + bit3;
            if (cnt > 28) {
                std::cerr << "Too many bits used in page tables." << std::endl;
                return 1;
            }
        }
        if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            if (strcmp(argv[i + 1], "bitmasks") == 0) {
                logOptions.pagetable_bitmasks = true;
            } else if (strcmp(argv[i + 1], "offset") == 0) {
                logOptions.offset = true;
            } else if (strcmp(argv[i + 1], "addressTranslation") == 0) {
                logOptions.addressTranslation = true;
            } else if (strcmp(argv[i + 1], "vpns_pfn") == 0) {
                logOptions.vpns_pfn = true;
            } else if (strcmp(argv[i + 1], "vpn2pfn_with_pagereplace") == 0) {
                logOptions.vpn2pfn_with_pagereplace = true;
            } else if (strcmp(argv[i + 1], "summary") == 0) {
                logOptions.summary = true;
            }
            i++;
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            n = atoi(argv[i + 1]);
            if (n <= 0) {
                std::cerr << "Number of memory accesses must be a number, greater than 0." << std::endl;
                return 1;
            }
            i++;
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            traceFilePath = argv[i + 1];
            i++;
        } else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            ageOfLastAccess = atoi(argv[i + 1]);
            if (ageOfLastAccess <= 0) {
                std::cerr << "Age of last access considered recent must be a number, greater than 0." << std::endl;
                return 1;
            }
            i++;
        }
    }

    // Initialize memory management
    MemoryManagement memoryManagement(num_frames, logOptions);

    // Open trace file
    std::ifstream traceFile(traceFilePath);
    if (!traceFile.is_open()) {
        std::cerr << "Number of available frames must be a number, greater than 0" << std::endl;
        return 1;
    }

    // Open read/write file
    std::ifstream readWriteFile(readWriteFilePath);
    if (!readWriteFile.is_open()) {
        std::cerr << "Failed to open read/write file: " << readWriteFilePath << std::endl;
        return 1;
    }

    // Main simulation loop
    std::string line;
    char accessMode;
    while (std::getline(traceFile, line)) {
    // Check if line is a valid hexadecimal number
    if (line.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
        std::cerr << "Invalid address in trace file: " << line << std::endl;
        continue;
    }

    int virtual_address = std::stoi(line, nullptr, 16);
    readWriteFile.get(accessMode);
    int physical_address = memoryManagement.translateAddress(virtual_address, accessMode);
    
    if (physical_address != -1) {
        std::cout << "Virtual address: " << std::hex << virtual_address
                  << " translated to physical address: " << physical_address << std::endl;
    } else {
        std::cout << "Page fault occurred for virtual address: " << std::hex << virtual_address << "!" << std::endl;
    }
}

    // Close files
    traceFile.close();
    readWriteFile.close();

    // Log summary
    if (logOptions.summary) {
        log_summary(PAGE_SIZE, memoryManagement.numOfPageReplaces, memoryManagement.pageTableHits, memoryManagement.numOfAddresses, memoryManagement.numOfFramesAllocated, memoryManagement.totalBytesUsed);
    }

    return 0;
}
