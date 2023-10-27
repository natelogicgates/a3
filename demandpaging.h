#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include <cmath>
#include <stdio.h>
#include <string.h>
#include <ctime>
#include "log_helpers.h" 

void allocateFrameToPage(int vpn, int frameNumber);

int translateAddress(int virtual_address);

void handlePageFault(int vpn);

int findFreeFrame();

int runWSClock();

