/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __LOG_H__
#define __LOG_H__


#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>


#define PRINTIP(x) ((uint8_t*)&x)[0], ((uint8_t*)&x)[1], \
                   ((uint8_t*)&x)[2], ((uint8_t*)&x)[3]

#define IPFMT "%u.%u.%u.%u"

#define DECLARE_LOGGER(varName)

#define DEFINE_LOGGER(varName, logName)

#define MAX_BUFFER_SIZE 20000

#define SPRINTF_LOG_MSG(buffer, fmt, args...) \
    char buffer[MAX_BUFFER_SIZE]; \
    snprintf( buffer, MAX_BUFFER_SIZE, fmt, ##args );

#define LOG_TRACE(logger, fmt, args...)

#define LOG_DEBUG(logger, fmt, args...)

#define LOG_INFO(logger, fmt, args...)

#define LOG_WARN(logger, fmt, args...)

#define LOG_ERROR(logger, fmt, args...)

#define LOG_FATAL(logger, fmt, args...)

#ifdef DISABLE_TRACE
#   define TRACE(logger, x)
#else   
#   define TRACE(logger, x) \
class Trace { \
 public: \
    Trace(const void* p) : _p(p) { \
        LOG_TRACE(logger, "%s %p Enter", __PRETTY_FUNCTION__, p); \
    } \
    ~Trace() { \
        LOG_TRACE(logger, "%s %p Exit", __PRETTY_FUNCTION__, _p); \
    } \
    const void* _p; \
} traceObj(x);
#endif  /* DISABLE_TRACE */
    
#endif  /* __LOG_H__ */

