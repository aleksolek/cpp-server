#pragma once

#include <string.h>
#include <string>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>

extern std::atomic<bool> serverRunning;
extern std::condition_variable queueCV;
extern std::mutex queueMutex;
extern std::queue<int> socketQueue;

void workerThread(void);
void connection_handler(int sock);