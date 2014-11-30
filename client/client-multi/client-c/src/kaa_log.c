/*
 * Copyright 2014 CyberVision, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "kaa_log.h"

#include <time.h>
#include <stdarg.h>
#include <string.h>

#include "kaa_common.h"
#include "kaa_mem.h"

#define KAA_LOG_PREFIX_FORMAT   "%04d/%02d/%02d %d:%02d:%02d [%s] [%s:%d] (%d) - "

/**
 * Printable loglevels
 * @see kaa_log_level_t
 */
static const char* kaa_log_level_name[] =
{
      "NONE"
    , "FATAL"
    , "ERROR"
    , "WARNING"
    , "INFO"
    , "DEBUG"
    , "TRACE"
};

struct kaa_logger_t {
    FILE           *sink;
    kaa_log_level_t max_log_level;
    char           *log_buffer;
    size_t          buffer_size;
};


kaa_error_t kaa_log_create(kaa_logger_t **logger_p, size_t buffer_size, kaa_log_level_t max_log_level, FILE* sink)
{
    if (!logger_p || (buffer_size < 2))
        return KAA_ERR_BADPARAM;

    *logger_p = KAA_MALLOC(kaa_logger_t);
    if (!*logger_p)
        return KAA_ERR_NOMEM;

    (*logger_p)->log_buffer = KAA_CALLOC(buffer_size, sizeof(char));
    if (!(*logger_p)->log_buffer) {
        KAA_FREE(*logger_p);
        *logger_p = NULL;
        return KAA_ERR_NOMEM;
    }

    (*logger_p)->buffer_size = buffer_size;
    (*logger_p)->sink = sink ? sink : stdout;
    (*logger_p)->max_log_level = max_log_level;
    return KAA_ERR_NONE;
}

kaa_error_t kaa_log_destroy(kaa_logger_t *logger)
{
    KAA_RETURN_IF_NIL(logger, KAA_ERR_BADPARAM);

    KAA_FREE(logger->log_buffer);
    KAA_FREE(logger);
    return KAA_ERR_NONE;
}

kaa_log_level_t kaa_get_max_log_level(const kaa_logger_t *this)
{
    return this ? this->max_log_level : KAA_LOG_NONE;
}

kaa_error_t kaa_set_max_log_level(kaa_logger_t *this, kaa_log_level_t max_log_level)
{
    KAA_RETURN_IF_NIL(this, KAA_ERR_BADPARAM);

    this->max_log_level = max_log_level;
    return KAA_ERR_NONE;
}

void kaa_log_write(kaa_logger_t *this, const char* source_file, int lineno, kaa_log_level_t log_level
        , kaa_error_t error_code, const char* format, ...)
{
    if (!this || (log_level > this->max_log_level))
        return;

    // Truncate the file name
    char* path_separator_pos = strrchr(source_file, '/');
    path_separator_pos = (path_separator_pos ? path_separator_pos : strrchr(source_file, '\\'));
    const char* truncated_name = (path_separator_pos ? path_separator_pos + 1 : source_file);

    // Convert to UTC time
    // TODO: Need to print milliseconds. For this purpose, timespec() from C11
    // standard may be used, but GCC 4.6.4 doesn't support this API.
    time_t t = time(NULL);
    struct tm* tp = gmtime(&t);

    size_t consumed_len = 0;
    // Print log message prefix
    int res_len = snprintf(this->log_buffer, this->buffer_size, KAA_LOG_PREFIX_FORMAT
            , 1900 + tp->tm_year, tp->tm_mon + 1, tp->tm_mday
            , tp->tm_hour, tp->tm_min, tp->tm_sec
            , kaa_log_level_name[log_level], truncated_name, lineno, error_code);

    if (res_len <= 0)   // Something terrible happened
        return;
    consumed_len += res_len;

    if (consumed_len > this->buffer_size - 1) {
        // Ran out of buffer space already (greedy log buffer:). Reserve space for '\n' at the end.
        consumed_len = this->buffer_size - 1;
    } else {
        // There's buffer space remaining: print log message body
        va_list args;
        va_start(args, format);
        res_len = vsnprintf(this->log_buffer + consumed_len
                , this->buffer_size - consumed_len
                , format
                , args);
        va_end(args);

        if (res_len <= 0)   // Something terrible happened
            return;
        consumed_len += res_len;
        if (consumed_len > this->buffer_size - 1) {
            // Ran out of buffer space
            consumed_len = this->buffer_size - 1;
        }
    }

    // Terminate buffer with '\n'. Null-termination is not important because the buffer length is specified in fwrite() below
    this->log_buffer[consumed_len++] = '\n';

    fwrite(this->log_buffer, sizeof(char), consumed_len, this->sink);
    fflush(this->sink);
}
