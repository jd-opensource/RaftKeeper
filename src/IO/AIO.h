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
#pragma once

#include <boost/noncopyable.hpp>

#if defined(OS_LINUX)

/// https://stackoverflow.com/questions/20759750/resolving-redefinition-of-timespec-in-time-h
#    define timespec linux_timespec
#    define timeval linux_timeval
#    define itimerspec linux_itimerspec
#    define sigset_t linux_sigset_t

#    include <linux/aio_abi.h>

#    undef timespec
#    undef timeval
#    undef itimerspec
#    undef sigset_t


/** Small wrappers for asynchronous I/O.
  */

int io_setup(unsigned nr, aio_context_t * ctxp);

int io_destroy(aio_context_t ctx);

/// last argument is an array of pointers technically speaking
int io_submit(aio_context_t ctx, long nr, struct iocb * iocbpp[]);

int io_getevents(aio_context_t ctx, long min_nr, long max_nr, io_event * events, struct timespec * timeout);


struct AIOContext : private boost::noncopyable
{
    aio_context_t ctx;

    AIOContext(unsigned int nr_events = 128);
    ~AIOContext();
};

#elif defined(OS_FREEBSD)

#    include <aio.h>
#    include <sys/event.h>
#    include <sys/time.h>
#    include <sys/types.h>

typedef struct kevent io_event;
typedef int aio_context_t;

struct iocb
{
    struct aiocb aio;
    long aio_data;
};

int io_setup(void);

int io_destroy(void);

/// last argument is an array of pointers technically speaking
int io_submit(int ctx, long nr, struct iocb * iocbpp[]);

int io_getevents(int ctx, long min_nr, long max_nr, struct kevent * events, struct timespec * timeout);


struct AIOContext : private boost::noncopyable
{
    int ctx;

    AIOContext(unsigned int nr_events = 128);
    ~AIOContext();
};

#endif
