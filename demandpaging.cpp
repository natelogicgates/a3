#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include <cmath>
#include <stdio.h>
#include <string.h>
#include <ctime>
#include "log_helpers.h" 
#include "demandpaging.h"

const int PAGE_SIZE = 4096;
const int PAGE_TABLE_ENTRIES = 1024;
const int ADDRESS_SPACE = 32;

class PTE {
public:
    bool valid;
    int PFN;
    int timestamp;
    PTE* next_level;

    PTE() : valid(false), PFN(-1), timestamp(-1), next_level(nullptr) {}
};

class Frame {
public:
    int PFN;
    bool free;
    int VPN;
    int timestamp;

    Frame(int pfn) : PFN(pfn), free(true), VPN(-1), timestamp(-1) {}
};

class MemoryManagement {
    std::vector<Frame> frames;
    PTE* topLevel;
    int clock_hand;
    LogOptionsType logOptions;

public:
    MemoryManagement(size_t num_frames, LogOptionsType logOpt) : clock_hand(0), logOptions(logOpt) {
        for (size_t i = 0; i < num_frames; i++) {
            frames.push_back(Frame(i));
        }
        topLevel = new PTE[PAGE_TABLE_ENTRIES];
    }
    void allocateFrameToPage(int vpn, int frameNumber) {
    PTE* currentLevel = topLevel;
    for (int i = ADDRESS_SPACE - 1; i >= 0; i--) {
        int index = (vpn >> i) & (PAGE_TABLE_ENTRIES - 1);
        
        // If we've reached the last level of the page table, we allocate the frame
        if (i == 0) {
            currentLevel[index].valid = true;
            currentLevel[index].PFN = frameNumber;
            frames[frameNumber].free = false;
            frames[frameNumber].VPN = vpn;
            frames[frameNumber].timestamp = std::time(nullptr);  // setting current time as timestamp for simplicity
            if (logOptions.vpn2pfn_with_pagereplace) {
                //log_frame_allocation(frames[frameNumber].PFN, frames[frameNumber].VPN);  // Logging frame allocation
            }
        } else {
            // If the next level page table is not yet allocated, we allocate it
            if (!currentLevel[index].next_level) {
                currentLevel[index].next_level = new PTE[PAGE_TABLE_ENTRIES];
            }
            currentLevel = currentLevel[index].next_level;
        }
    }
    }
    int translateAddress(int virtual_address) {
        int offset = virtual_address % PAGE_SIZE;
        int vpn = virtual_address / PAGE_SIZE;
        int pa = -1;  // Default to invalid address

        PTE* currentLevel = topLevel;
        for (int i = ADDRESS_SPACE - 1; i >= 0; i--) {
            int index = (vpn >> i) & (PAGE_TABLE_ENTRIES - 1);
            if (currentLevel[index].valid) {
                if (i == 0) {
                    pa = (currentLevel[index].PFN * PAGE_SIZE) + offset;
                    if (logOptions.addressTranslation) {
                        log_va2pa(virtual_address, pa);
                    }
                    return pa;
                } else {
                    currentLevel = currentLevel[index].next_level;
                }
            } else {
                handlePageFault(vpn);
                return -1;
            }
        }
        return -1;
    }

    void handlePageFault(int vpn) {
    int frameNumber = findFreeFrame();
    if (frameNumber == -1) {
        int replacedVpn = -1;  // Placeholder, you'd need to determine the replaced VPN from your page replacement algorithm
        frameNumber = runWSClock();
        log_mapping(vpn, frameNumber, replacedVpn, false);  // pagetable miss and possibly a page replacement
    } else {
        log_mapping(vpn, frameNumber, -1, false);  // pagetable miss but no page replacement
    }
    allocateFrameToPage(vpn, frameNumber);
}

    int findFreeFrame() {
        for (size_t i = 0; i < frames.size(); i++) {
            if (frames[i].free) {
                if (logOptions.vpn2pfn_with_pagereplace) {
                    //log_frame_allocation(frames[i].PFN, frames[i].VPN);  // Logging frame allocation
                }
                return i;
            }
        }
        return -1;
    }

    int runWSClock() {
        while (true) {
            Frame& frame = frames[clock_hand];
            if (frame.free) {
                return clock_hand;
            }
            // Logic for recently accessed and older-than-threshold pages skipped for brevity
            clock_hand = (clock_hand + 1) % frames.size();
        }
    }
};

int main(int argc, char *argv[]) {
    LogOptionsType logOptions;
    logOptions.pagetable_bitmasks = true;
    logOptions.addressTranslation = true;
    logOptions.vpns_pfn = true;
    logOptions.vpn2pfn_with_pagereplace = true;
    logOptions.offset = false;
    logOptions.summary = false;

    /*MemoryManagement memoryManagement(256, logOptions);
    int virtual_address = 0x12345678;
    int physical_address = memoryManagement.translateAddress(virtual_address);
    if (physical_address != -1) {
        std::cout << "Translated to physical address: " << std::hex << physical_address << std::endl;
    } else {
        std::cout << "Page fault occurred!" << std::endl;
    }*/
    std::string traceFilePath;
    int n;
    for (int i = 1; i < argc; i++) {
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
            i++;
        }
        
        else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            traceFilePath = argv[i + 1];
            i++;
        }
    }

    MemoryManagement memoryManagement(n, logOptions);

    if (!traceFilePath.empty()) {
        std::ifstream traceFile(traceFilePath);
        if (!traceFile.is_open()) {
            std::cerr << "Failed to open trace file: " << traceFilePath << std::endl;
            return 1;
        }

        std::string line;
        while (std::getline(traceFile, line)) {
            int virtual_address = std::stoi(line, nullptr, 16); // Convert hex string to int
            int physical_address = memoryManagement.translateAddress(virtual_address);
            
            if (physical_address != -1) {
                std::cout << "Virtual address: " << std::hex << virtual_address
                          << " translated to physical address: " << physical_address << std::endl;
            } else {
                std::cout << "Page fault occurred for virtual address: " << std::hex << virtual_address << "!" << std::endl;
            }
        }

        traceFile.close();
    }

    return 0;
}
