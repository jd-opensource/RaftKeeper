/**
 * Copyright 2016-2023 ClickHouse, Inc.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <Common/MemoryTracker.h>
#include <Common/CurrentThread.h>

#include <Common/CurrentMemoryTracker.h>

namespace
{

MemoryTracker * getMemoryTracker()
{
    if (auto * thread_memory_tracker = RK::CurrentThread::getMemoryTracker())
        return thread_memory_tracker;

    /// Once the main thread is initialized,
    /// total_memory_tracker is initialized too.
    /// And can be used, since MainThreadStatus is required for profiling.
    if (RK::MainThreadStatus::get())
        return &total_memory_tracker;

    return nullptr;
}

}

namespace CurrentMemoryTracker
{

using RK::current_thread;

void alloc(Int64 size)
{
    if (auto * memory_tracker = getMemoryTracker())
    {
        if (current_thread)
        {
            current_thread->untracked_memory += size;
            if (current_thread->untracked_memory > current_thread->untracked_memory_limit)
            {
                /// Zero untracked before track. If tracker throws out-of-limit we would be able to alloc up to untracked_memory_limit bytes
                /// more. It could be useful to enlarge Exception message in rethrow logic.
                Int64 tmp = current_thread->untracked_memory;
                current_thread->untracked_memory = 0;
                memory_tracker->alloc(tmp);
            }
        }
        /// total_memory_tracker only, ignore untracked_memory
        else
        {
            memory_tracker->alloc(size);
        }
    }
}

void realloc(Int64 old_size, Int64 new_size)
{
    Int64 addition = new_size - old_size;
    addition > 0 ? alloc(addition) : free(-addition);
}

void free(Int64 size)
{
    if (auto * memory_tracker = getMemoryTracker())
    {
        if (current_thread)
        {
            current_thread->untracked_memory -= size;
            if (current_thread->untracked_memory < -current_thread->untracked_memory_limit)
            {
                memory_tracker->free(-current_thread->untracked_memory);
                current_thread->untracked_memory = 0;
            }
        }
        /// total_memory_tracker only, ignore untracked_memory
        else
        {
            memory_tracker->free(size);
        }
    }
}

}
