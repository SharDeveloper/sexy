#define _GNU_SOURCE
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <locale.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

struct {
    uint64_t data;
    uint32_t type;
} typedef type;

struct {
    uint64_t  id;
    type      data;
    uint32_t* message;
} typedef error;

struct {
    uint64_t        use_counter;
    uint64_t        capacity;
    uint64_t        index_of_first;
    uint64_t        count;
    pthread_mutex_t mutex;
    type*           items;
} typedef pipeline;

struct {
    uint64_t id;
    uint64_t random_number_source[3];
    uint64_t cryptographic_random_number_buffer[64];
    uint64_t cryptographic_random_number_index;
} typedef thread_data;

struct {
    type (*worker)(type, type, void*, bool);
    type in;
    type out;
} typedef worker;

#define nothing__type_number 0
#define error__type_number   1
#define bool__type_numer     2
#define int__type_number     3
#define string__type_number  4

#define error__id_no_error 2
#define error__id_fail     3

static type (*shar__rc_free)(type, void*, bool);
static type (*shar__rc_use)(type, void*, bool);

#pragma region Alloc
static inline void* safe_realloc(void* pointer, uint64_t size) {
    void* const result = realloc(pointer, size);
    if (__builtin_expect(result == NULL, false)) {
        fprintf(stderr, "Not enough memory.\n");
        exit(EXIT_FAILURE);
    }
    return result;
}

static inline void* safe_malloc(uint64_t size) {
    void* const result = malloc(size);
    if (__builtin_expect(result == NULL, false)) {
        fprintf(stderr, "Not enough memory.\n");
        exit(EXIT_FAILURE);
    }
    return result;
}
#pragma endregion Alloc

#define realloc(p, size) safe_realloc(p, size)
#define malloc(size) safe_malloc(size)

#pragma region String
static type const string__empty = (type){.data = (uint64_t)(const uint64_t[]) {0, 0}, .type = string__type_number};

static inline uint8_t char__utf32_to_utf8(uint32_t utf32_char, uint8_t* utf8_char) {
    if (utf32_char > 0x10FFFF || utf32_char == 0) {
        utf8_char[0] = 239;
        utf8_char[1] = 191;
        utf8_char[2] = 189;
        return 3;
    }
    if (utf32_char < 0x80) {
        utf8_char[0] = utf32_char;
        return 1;
    }
    if (utf32_char < 0x800) {
        utf8_char[0] = (utf32_char >> 6) | 192;
        utf8_char[1] = (utf32_char & 63) | 128;
        return 2;
    }
    if (utf32_char < 0x10000) {
        utf8_char[0] = (utf32_char >> 12) | 224;
        utf8_char[1] = ((utf32_char >> 6) & 63) | 128;
        utf8_char[2] = (utf32_char & 63) | 128;
        return 3;
    }
    utf8_char[0] = (utf32_char >> 18) | 240;
    utf8_char[1] = ((utf32_char >> 12) & 63) | 128;
    utf8_char[2] = ((utf32_char >> 6) & 63) | 128;
    utf8_char[3] = (utf32_char & 63) | 128;
    return 4;
}

static inline uint32_t char__utf8_to_utf32(const uint8_t** utf8_char) {
    uint8_t const first_byte = **utf8_char;
    if (first_byte < 128) {
        *utf8_char = &((*utf8_char)[1]);
        return first_byte;
    }
    uint8_t const second_byte = (*utf8_char)[1];
    if ((first_byte & 224) == 192) {
        if (__builtin_expect((second_byte & 192) == 128, true)) {
            *utf8_char = &((*utf8_char)[2]);
            return (((uint32_t)first_byte ^ 192) << 6) | ((uint32_t)second_byte ^ 128);
        }
        return -1;
    }
    uint8_t const third_byte = (*utf8_char)[2];
    if ((first_byte & 240) == 224) {
        if (__builtin_expect((second_byte & 192) == 128 && (third_byte & 192) == 128, true)) {
            *utf8_char = &((*utf8_char)[3]);
            return (((uint32_t)first_byte ^ 224) << 12) | (((uint32_t)second_byte ^ 128) << 6) | ((uint32_t)third_byte ^ 128);
        }
        return -1;
    }
    if ((first_byte & 248) == 240) {
        uint8_t const fourth_byte = (*utf8_char)[3];
        if (__builtin_expect((second_byte & 192) == 128 && (third_byte & 192) == 128 && (fourth_byte & 192) == 128, true)) {
            *utf8_char = &((*utf8_char)[4]);
            return (((uint32_t)first_byte ^ 240) << 18) | (((uint32_t)second_byte ^ 128) << 12) | (((uint32_t)third_byte ^ 128) << 6) |  ((uint32_t)fourth_byte ^ 128);
        }
    }
    return -1;
}

uint8_t* string__utf32_to_utf8(type string) {
    uint8_t* result = NULL;
    uint8_t buffer[256];
    uint64_t buffer_index = 0;
    uint64_t result_index = 0;
    uint64_t const string_length = ((const uint64_t*)string.data)[1];
    const uint32_t* const chars = &(((const uint32_t* )string.data)[4]);
    for (uint64_t char_index = 0; char_index < string_length; char_index++) {
        uint8_t const offset = char__utf32_to_utf8(chars[char_index], &(buffer[buffer_index]));
        buffer_index += offset;
        if (buffer_index > 252) {
            result = realloc(result, result_index + buffer_index);
            memcpy(&(result[result_index]), buffer, buffer_index);
            result_index += buffer_index;
            buffer_index = 0;
        }
    }
    result = realloc(result, result_index + buffer_index + 1);
    if (buffer_index != 0) {memcpy(&(result[result_index]), buffer, buffer_index);}
    result[result_index + buffer_index] = 0;
    return result;
}

type string__utf8_to_utf32(const uint8_t* utf8_string) {
    uint32_t buffer[256];
    uint64_t buffer_index = 0;
    const uint8_t* curretn_utf8_char_ptr = utf8_string;
    uint32_t* result = malloc(2 * sizeof(uint64_t));
    uint64_t length = 0;
    for (;;) {
        uint32_t const curretn_utf32_char = char__utf8_to_utf32(&curretn_utf8_char_ptr);
        if (curretn_utf32_char == 0) {break;}
        if (__builtin_expect(curretn_utf32_char == -1, false)) {
            free(result);
            return (type){.data = 0, .type = nothing__type_number};
        }
        buffer[buffer_index] = curretn_utf32_char;
        buffer_index++;
        if (buffer_index == 256) {
            result = realloc(result, (length + 260) * sizeof(uint32_t));
            memcpy(&(result[length + 4]), buffer, 256 * sizeof(uint32_t));
            buffer_index = 0;
            length += 256;
        }
    }
    if (buffer_index != 0) {
        result = realloc(result, (length + buffer_index + 4) * sizeof(uint32_t));
        memcpy(&(result[length + 4]), buffer, buffer_index * sizeof(uint32_t));
        length += buffer_index;
    }
    ((uint64_t*)result)[0] = 1;
    ((uint64_t*)result)[1] = length;
    return (type){.data = (uint64_t)result, .type = string__type_number};
}

static void print(type string, FILE* file, bool end_is_new_line) {
    uint64_t const string_length = ((const uint64_t*)string.data)[1];
    const uint32_t* const chars = &(((const uint32_t* )string.data)[4]);
    if (string_length > 0 || end_is_new_line) {
        uint8_t buffer[256];
        uint64_t buffer_index = 0;
        for (uint64_t char_index = 0; char_index < string_length; char_index++) {
            uint32_t current_char = chars[char_index];
            buffer_index += char__utf32_to_utf8(current_char, &(buffer[buffer_index]));
            if (buffer_index > 252) {
                fwrite(buffer, 1, buffer_index, file);
                buffer_index = 0;
            }
        }
        if (end_is_new_line) {
            buffer[buffer_index] = '\n';
            buffer_index++;
        }
        if (buffer_index != 0) {
            fwrite(buffer, 1, buffer_index, file);
        }
        fflush(file);
    }
}

// The function prints a string to the terminal.
void string__print(type string) {print(string, stdout, false);}

// The function prints a string, with a newline appended at the end, to the terminal.
void string__println(type string) {print(string, stdout, true);}

// The function prints the string as an error.
void string__print_as_error(type string) {print(string, stderr, false);}

// The function prints the string as an error, with a newline appended at the end.
void string__println_as_error(type string) {print(string, stderr, true);}

// The function prints the string as an error and terminates the program.
__attribute__((noreturn, cold)) void string__print_error(type string) {
    fwrite("Error: ", 1, 7, stderr);
    print(string, stderr, true);
    exit(EXIT_FAILURE);
}

// The function prints the utf8 string as an error and terminates the program.
__attribute__((noreturn, cold)) void string__print_utf8_error(const uint8_t* message) {
    fprintf(stderr, "Error: %s\n", message);
    exit(EXIT_FAILURE);
}
#pragma endregion String

#pragma region Error
type error__create(type id, type message, type data) {
    error* const error_mem = malloc(sizeof(error));
    *error_mem = (error) {
        .id   = id.data,
        .message = (uint32_t*)message.data,
        .data = data
    };
    return (type){.data = (uint64_t)error_mem, .type = error__type_number};
}

type error__create_utf8_message(type id, type data, const uint8_t* message) {
    type const message_obj = string__utf8_to_utf32(message);
    error* const error_mem = malloc(sizeof(error));
    *error_mem = (error) {
        .id   = id.data,
        .data = data,
        .message = (uint32_t*)message_obj.data
    };
    return (type){.data = (uint64_t)error_mem, .type = error__type_number};
}

void error__add_utf8_string_to_message(type error_obj, const uint8_t* utf8_string) {
    type const utf32_string = string__utf8_to_utf32(utf8_string);
    uint32_t* const utf32_string_data = (uint32_t*)utf32_string.data;
    uint64_t const utf32_string_len = ((uint64_t*)utf32_string_data)[1];
    if (utf32_string_len == 0) {
        free(utf32_string_data);
        return;
    }
    uint32_t* error_message = ((error*)error_obj.data)->message;
    uint64_t const error_message_rc = ((uint64_t*)error_message)[0];
    uint64_t const error_message_len = ((uint64_t*)error_message)[1];
    if (error_message_rc == 1) {
        error_message = realloc(error_message, (error_message_len + utf32_string_len + 4) * sizeof(uint32_t));
    } else {
        uint32_t* new_error_message = malloc((error_message_len + utf32_string_len + 4) * sizeof(uint32_t));
        memcpy(&(new_error_message[4]), &(error_message[4]), error_message_len * sizeof(uint32_t));
        if (error_message_rc != 0) {((uint64_t*)error_message)[0] = error_message_rc - 1;}
        ((uint64_t*)new_error_message)[0] = 1;
        error_message = new_error_message;
    }
    ((uint64_t*)error_message)[1] = error_message_len + utf32_string_len;
    memcpy(&(error_message[error_message_len + 4]), &(utf32_string_data[4]), utf32_string_len * sizeof(uint32_t));
    ((error*)error_obj.data)->message = error_message;
    free(utf32_string_data);
}

type error__get_id(type error_obj) {
    return (type){.data = ((const error*)error_obj.data)->id, .type = int__type_number};
}

type error__get_message(type error_obj) {
    return (type){.data = (uint64_t)(((const error*)error_obj.data)->message), .type = string__type_number};
}

type error__get_data(type error_obj) {
    return ((const error*)error_obj.data)->data;
}

void error__free(type error_obj, void* th_data) {
    error* err = (error*)error_obj.data;
    shar__rc_free(err->data, th_data, false);
    uint64_t const error_message_rc = ((uint64_t*)(err->message))[0];
    switch (error_message_rc) {
    case 0:
        break;
    case 1:
        free(err->message);
        break;
    default:
        ((uint64_t*)(err->message))[0] = error_message_rc - 1;
    }
    free(err);
}
#pragma endregion Error

#pragma region Thread
static bool allow_threads = false;
static _Atomic uint64_t number_of_threads = 1;
static _Atomic bool ignored_errors = false;

static inline void mutex__init(pthread_mutex_t* mutex) {
    if (__builtin_expect(pthread_mutex_init(mutex, NULL) != 0, false)) {
        fprintf(stderr, "Mutex initialization error.\n");
        exit(EXIT_FAILURE);
    }
}

static inline void mutex__lock(pthread_mutex_t* mutex) {
    if (__builtin_expect(pthread_mutex_lock(mutex) != 0, false)) {
        fprintf(stderr, "Mutex lock error.\n");
        exit(EXIT_FAILURE);
    }
}

static inline void mutex__unlock(pthread_mutex_t* mutex) {
    if (__builtin_expect(pthread_mutex_unlock(mutex) != 0, false)) {
        fprintf(stderr, "Mutex unlock error.\n");
        exit(EXIT_FAILURE);
    }
}

static inline void mutex__destroy(pthread_mutex_t* mutex) {
    if (__builtin_expect(pthread_mutex_destroy(mutex) != 0, false)) {
        fprintf(stderr, "Error destroying mutex.\n");
        exit(EXIT_FAILURE);
    }
}

uint64_t pipeline__create() {
    pipeline* result = malloc(sizeof(pipeline));
    *result = (pipeline) {
        .use_counter    = 1,
        .capacity       = 32,
        .index_of_first = 0,
        .count          = 0,
        .items          = (type*)malloc(32 * sizeof(type))
    };
    mutex__init(&(result->mutex));
    return (uint64_t)result;
}

void pipeline__use(uint64_t pipe) {
    pipeline* pipeline_ptr = (pipeline*)pipe;
    mutex__lock(&(pipeline_ptr->mutex));
    if (pipeline_ptr->use_counter != 0) {pipeline_ptr->use_counter++;}
    mutex__unlock(&(pipeline_ptr->mutex));
}

void pipeline__free(uint64_t pipe, void* th_data) {
    pipeline* pipeline_ptr = (pipeline*)pipe;
    mutex__lock(&(pipeline_ptr->mutex));
    switch (pipeline_ptr->use_counter) {
    case 0:
        break;
    case 1:
        mutex__unlock(&(pipeline_ptr->mutex));
        mutex__destroy(&(pipeline_ptr->mutex));
        for (uint64_t offset = 0; offset < pipeline_ptr->count; offset++) {
            type const item = pipeline_ptr->items[pipeline_ptr->index_of_first + offset];
            if (item.type != error__type_number) {shar__rc_free(item, th_data, false);}
            else {
                string__println_as_error(error__get_message(item));
                error__free(item, th_data);
                ignored_errors = true;
            }
        }
        free(pipeline_ptr->items);
        free(pipeline_ptr);
        return;
    default:
        pipeline_ptr->use_counter--;
    }
    mutex__unlock(&(pipeline_ptr->mutex));
}

void pipeline__to_const(uint64_t pipe) {
    pipeline* pipeline_ptr = (pipeline*)pipe;
    mutex__lock(&(pipeline_ptr->mutex));
    if (pipeline_ptr->use_counter != 0) {pipeline_ptr->use_counter = 0;}
    mutex__unlock(&(pipeline_ptr->mutex));
}

void pipeline__push(uint64_t pipe, type pushed_object) {
    pipeline* pipeline_ptr = (pipeline*)pipe;
    mutex__lock(&(pipeline_ptr->mutex));
    if (pipeline_ptr->capacity == (pipeline_ptr->index_of_first + pipeline_ptr->count)) {
        if (pipeline_ptr->index_of_first == 0) {
            pipeline_ptr->capacity *= 2;
            pipeline_ptr->items = realloc(pipeline_ptr->items, pipeline_ptr->capacity * sizeof(type));
        } else {
            memmove(pipeline_ptr->items, &(pipeline_ptr->items[pipeline_ptr->index_of_first]), pipeline_ptr->count * sizeof(type));
            pipeline_ptr->index_of_first = 0;
        }
    }
    pipeline_ptr->items[pipeline_ptr->index_of_first + pipeline_ptr->count] = pushed_object;
    pipeline_ptr->count++;
    mutex__unlock(&(pipeline_ptr->mutex));
}

type pipeline__pop(uint64_t pipe) {
    pipeline* pipeline_ptr = (pipeline*)pipe;
    mutex__lock(&(pipeline_ptr->mutex));
    type result;
    if (pipeline_ptr->count == 0) {
        result = (type){.data = 0, .type = nothing__type_number};
    } else {
        result = pipeline_ptr->items[pipeline_ptr->index_of_first];
        pipeline_ptr->index_of_first++;
        pipeline_ptr->count--;
    }
    mutex__unlock(&(pipeline_ptr->mutex));
    return result;
}

type pipeline__items_count(uint64_t pipe) {
    pipeline* pipeline_ptr = (pipeline*)pipe;
    mutex__lock(&(pipeline_ptr->mutex));
    type const result = (type){.data = pipeline_ptr->count, .type = int__type_number};
    mutex__unlock(&(pipeline_ptr->mutex));
    return result;
}

static void* worker__run(void* args) {
    worker worker_var = *(worker*)args;
    type (*function)(type, type, void*, bool) = worker_var.worker;
    thread_data* th_data = malloc(sizeof(thread_data));
    th_data->cryptographic_random_number_index = 64;
    th_data->id = getpid();
    {
        FILE* file = fopen("/dev/urandom", "r");
        if (__builtin_expect((file == NULL) || (fread(th_data->random_number_source, sizeof(uint64_t), 3, file) != 3), false)) {
            fprintf(stderr, "Can't read the file \x22/dev/urandom\x22.\n");
            exit(EXIT_FAILURE);
        }
        fclose(file);
    }
    number_of_threads++;
    type in_pipe = worker_var.in;
    type out_pipe = worker_var.out;
    type result = function(in_pipe, out_pipe, th_data, true);
    pipeline__push(out_pipe.data, result);
    pipeline__free(in_pipe.data, th_data);
    pipeline__free(out_pipe.data, th_data);
    number_of_threads--;
    free(th_data);
    free(args);
    return NULL;
}

void worker__create(type (*function)(type, type, void*, bool), type in_pipe, type out_pipe) {
    if (__builtin_expect(!allow_threads, false)) {
        fprintf(stderr, "At the stage of calculating constants, it is forbidden to use threads.\n");
        exit(EXIT_FAILURE);
    }
    pthread_t thread;
    worker* worker_var = malloc(sizeof(worker));
    *worker_var = (worker) {.worker = function, .in = in_pipe, .out = out_pipe};
    pipeline__use(in_pipe.data);
    pipeline__use(out_pipe.data);
    if (__builtin_expect(pthread_create(&thread, NULL, worker__run, worker_var) != 0, false)) {
        fprintf(stderr, "Failed to start new thread.\n");
        exit(EXIT_FAILURE);
    }
}

void worker__yield() {sched_yield();}

void worker__sleep(type milliseconds) {
    struct timespec time;
    time.tv_sec = milliseconds.data / 1000;
    time.tv_nsec = (milliseconds.data % 1000) * 1000000;
    int result;
    do {
        result = nanosleep(&time, &time);
    } while (result != 0 && errno == EINTR);
}

// The function returns a unique worker ID.
type worker__id(void* th_data) {return (type){.data = ((thread_data*)th_data)->id, .type = int__type_number};}

// The function returns the number of running threads.
type get_number_of_threads() {return (type){.data = number_of_threads, .type = int__type_number};}
#pragma endregion Thread

#pragma region Env
static uint64_t __argc__;
static uint8_t** __argv__;
static uint64_t cpu_cores_number;
static type const env__platform_name = (type){.data = (uint64_t)(const uint32_t[]) {0, 0, 12, 0, 'l', 'i', 'n', 'u', 'x', ' ', 'x', '8', '6', '_', '6', '4',}, .type = string__type_number};

type env__get_cmd_argument(type index) {
    if (index.data < __argc__) {return string__utf8_to_utf32(__argv__[index.data]);}
    return (type){.data = 0, .type = nothing__type_number};
}

// The function returns the number of command line arguments, including the name of the program executable.
type env__get_cmd_arguments_count() {return (type){.data = __argc__, .type = int__type_number};}

type env__get_variable(type variable_name) {
    uint8_t* const utf8_variable_name = string__utf32_to_utf8(variable_name);
    const uint8_t* const utf8_result = (uint8_t*)getenv((char *)utf8_variable_name);
    free(utf8_variable_name);
    if (utf8_result == NULL) {return (type){.data = 0, .type = nothing__type_number};}
    return string__utf8_to_utf32(utf8_result);
}

// The function gets a string from the command line.
type env__get_string_from_cmd_line() {
    uint8_t buffer[256];
    uint64_t length = 0;
    uint32_t* result = (uint32_t*)malloc(16);
    uint64_t count_of_previous_bytes = 0;
    for (;;) {
        uint8_t* const buffer_offset = &(buffer[count_of_previous_bytes]);
        if (fgets((char*)buffer_offset, 256 - count_of_previous_bytes, stdin) == (char*)buffer_offset) {
            uint64_t utf8_string_index = 0;
            count_of_previous_bytes = 0;
            for (;;) {
                uint8_t const first_byte = buffer[utf8_string_index];
                if (first_byte == '\n' || first_byte == 0) {
                    ((uint64_t*)result)[0] = 1;
                    ((uint64_t*)result)[1] = length;
                    return (type){.data = (uint64_t)result, .type = string__type_number};
                }
                uint8_t char_size = 0;
                if (first_byte < 128) {
                    char_size = 1;
                } else if ((first_byte & 224) == 192) {
                    char_size = 2;
                } else if ((first_byte & 240) == 224) {
                    char_size = 3;
                } else if ((first_byte & 248) == 240) {
                    char_size = 4;
                }
                if (utf8_string_index > 256 - char_size) {
                    count_of_previous_bytes = 256 - utf8_string_index;
                    break;
                }
                const uint8_t* utf8_char_pointer = &(buffer[utf8_string_index]);
                uint32_t const utf32_char = char__utf8_to_utf32(&utf8_char_pointer);
                if (__builtin_expect(utf32_char == -1, false)) {
                    free(result);
                    return (type){.data = 0, .type = nothing__type_number};
                }
                utf8_string_index += char_size;
                length++;
                result = realloc(result, length * 4 + 16);
                result[length + 3] = utf32_char;
                if (utf8_string_index == 256) {break;}
            }
            for (uint64_t offset = 0; offset < count_of_previous_bytes; offset++) {buffer[offset] = buffer[256 - count_of_previous_bytes + offset];}
        } else if (length != 0 && count_of_previous_bytes == 0) {
            ((uint64_t*)result)[0] = 1;
            ((uint64_t*)result)[1] = length;
            return (type){.data = (uint64_t)result, .type = string__type_number};
        } else {break;}
    }
    free(result);
    return (type){.data = 0, .type = nothing__type_number};
}

// The function returns the name of the platform on which the program was launched.
type env__get_platform_name() {return env__platform_name;}

// The function executes the command in the host environment.
// If the execution was successful, then the function returns "true", otherwise "false".
type env__execute_command(type command) {
    if (((uint64_t*)(command.data))[1] == 0) {return (type){.data = 0, .type = bool__type_numer};}
    uint8_t* const utf8_command = string__utf32_to_utf8(command);
    bool const result = system((const char*)utf8_command) == 0;
    free(utf8_command);
    return (type){.data = result, .type = bool__type_numer};
}

// The function returns the number of processor cores.
type env__get_cpu_cores_number() {return (type){.data = cpu_cores_number, .type = int__type_number};}
#pragma endregion Env

#pragma region Random
// The function returns a random number.
type int__get_random(void* th_data) {
    thread_data* data = (thread_data*)th_data;
    uint64_t const bits0_14 = (data->random_number_source[0] >> 16ull) & 0x7fffull;
    uint64_t const bits15_46 = (data->random_number_source[1] >> 32ull) & 0xffffffffull;
    uint64_t const bits47_63 = (data->random_number_source[2] >> 16ull) & 0x1ffffull;
    data->random_number_source[0] = data->random_number_source[0] * 1103515245ull + 12345;
    data->random_number_source[1] = data->random_number_source[1] * 6364136223846793005ull + 1;
    data->random_number_source[2] = data->random_number_source[2] * 25214903917ull + 11;
    return (type){.data = bits0_14 | (bits15_46 << 14) | (bits47_63 << 46), .type = int__type_number};
}

// The function returns a random number suitable for cryptographic purposes.
type int__get_cryptographic__random(void* th_data) {
    thread_data* data = (thread_data*)th_data;
    if (data->cryptographic_random_number_index == 64) {
        FILE *file = fopen("/dev/urandom", "r");
        if (__builtin_expect((file == NULL) || (fread(data->cryptographic_random_number_buffer, 8, 64, file) != 64), false)) {
            fprintf(stderr, "Can't read the file \x22/dev/urandom\x22.\n");
            exit(EXIT_FAILURE);
        }
        fclose(file);
        data->cryptographic_random_number_index = 0;
    }
    uint64_t result = data->cryptographic_random_number_buffer[data->cryptographic_random_number_index];
    data->cryptographic_random_number_index++;
    return (type){.data = result, .type = int__type_number};
}
#pragma endregion Random

#pragma region FS
#define file_buffer_size 131072

#define fs__copy__problem__stat           (type){.data = 1,  .type = int__type_number}
#define fs__copy__problem__read_link      (type){.data = 2,  .type = int__type_number}
#define fs__copy__problem__create_symlink (type){.data = 3,  .type = int__type_number}
#define fs__copy__problem__open_dir       (type){.data = 4,  .type = int__type_number}
#define fs__copy__problem__make_dir       (type){.data = 5,  .type = int__type_number}
#define fs__copy__problem__open_file      (type){.data = 6,  .type = int__type_number}
#define fs__copy__problem__create_file    (type){.data = 7,  .type = int__type_number}
#define fs__copy__problem__read_from_file (type){.data = 8,  .type = int__type_number}
#define fs__copy__problem__write_to_file  (type){.data = 9,  .type = int__type_number}

static type const fs__tmp_dir_name = (type){.data = (uint64_t)(const uint32_t[]) {0, 0, 5, 0, '/', 't', 'm', 'p', '/'}, .type = string__type_number};

// The function deletes the file at the specified path.
// If the delete was successful, then the function returns "true", otherwise "false".
type fs__delete_file(type file_name) {
    uint8_t* const utf8_file_name = string__utf32_to_utf8(file_name);
    struct stat file_stat;
    bool const result =
        lstat((char*)utf8_file_name, &file_stat) == 0 &&
        (file_stat.st_mode & S_IFDIR) != S_IFDIR &&
        remove((char*)utf8_file_name) == 0;
    free(utf8_file_name);
    return (type){.data = result, .type = bool__type_numer};
}

// The function deletes the empty directory at the specified path.
// If the delete was successful, then the function returns "true", otherwise "false".
type fs__delete_empty_dir(type dir_name) {
    uint8_t* const utf8_dir_name = string__utf32_to_utf8(dir_name);
    struct stat file_stat;
    bool const result =
        lstat((char*)utf8_dir_name, &file_stat) == 0 &&
        (file_stat.st_mode & S_IFDIR) == S_IFDIR &&
        remove((char*)utf8_dir_name) == 0;
    free(utf8_dir_name);
    return (type){.data = result, .type = bool__type_numer};
}

// If the file exists at the specified path, the function returns "true" otherwise "false".
type fs__file_is_exist(type file_name) {
    uint8_t* const utf8_file_name = string__utf32_to_utf8(file_name);
    struct stat file_stat;
    bool const result =
        lstat((char*)utf8_file_name, &file_stat) == 0 &&
        (file_stat.st_mode & S_IFDIR) != S_IFDIR;
    free(utf8_file_name);
    return (type){.data = result, .type = bool__type_numer};
}

// If the directory exists at the specified path, the function returns "true" otherwise "false".
type fs__dir_is_exist(type dir_name) {
    uint8_t* const utf8_dir_name = string__utf32_to_utf8(dir_name);
    struct stat file_stat;
    bool const result =
        lstat((char*)utf8_dir_name, &file_stat) == 0 &&
        (file_stat.st_mode & S_IFDIR) == S_IFDIR;
    free(utf8_dir_name);
    return (type){.data = result, .type = bool__type_numer};
}

// The function opens the file in the specified mode.
// If the file can be opened, the function returns "true", otherwise "false".
// Modes:
// 25202 (rb) - opens a file for reading. The file must exist.
// 25207 (wb) - opens a file for writing. If the file does not exist, then the function will create it, if the file already exists, then its contents will be deleted.
// 25185 (ab) - opens a file for writing. If the file does not exist, then the function will create it, if the file already exists, then writing will be made to the end of the file.
// 6433650 (r+b) - opens a file for reading and writing. The file must exist. The read and write position is at the beginning.
// 6433655 (w+b) - opens a file for reading and writing. If the file does not exist, then the function will create it, if the file already exists, then its contents will be deleted.
// 6433633 (a+b) - opens a file for reading and writing. If the file does not exist, then the function will create it, if the file already exists, the read and write position is at the end.
bool fs__open_file(type file_name, uint32_t mode, void** out_file) {
    uint8_t* const utf8_file_name = string__utf32_to_utf8(file_name);
    FILE *file = fopen((char*)utf8_file_name, (char*)(&mode));
    free(utf8_file_name);
    *out_file = file;
    return file != NULL;
}

// The function flushes the output buffer.
// If the flushed was successful, then the function returns "true", otherwise "false".
type fs__flush_file(void* file) {return (type){.data = (fflush(file) == 0), .type = bool__type_numer};}

// The function flushes the output buffer and close the file.
// If the closed was successful, then the function returns "true", otherwise "false".
type fs__close_file(void* file) {return (type){.data = (fclose(file) == 0), .type = bool__type_numer};}

// The function reading data from the file into memory, which should already be allocated.
type fs__read_from_file(void* file, type count_of_bytes, uint8_t* memory) {
    uint64_t result = fread(memory, 1, count_of_bytes.data, file);
    return (type){.data = result, .type = int__type_number};
}

// The function writing data from memory to the file.
type fs__write_to_file(void* file, type count_of_bytes, const uint8_t* memory) {
    uint64_t result = fwrite(memory, 1, count_of_bytes.data, file);
    return (type){.data = result, .type = int__type_number};
}

// The function gets the size of the file at the specified path.
// If the function could not find out the size of the file, then "nothing" is returned as a result.
type fs__get_file_size(type file_name) {
    uint8_t* const utf8_file_name = string__utf32_to_utf8(file_name);
    struct stat file_stat;
    type result;
    if (
        lstat((char*)utf8_file_name, &file_stat) == 0 &&
        (file_stat.st_mode & S_IFDIR) != S_IFDIR
    ) {result = (type){.data = file_stat.st_size, .type = int__type_number};}
    else {result = (type){.data = 0, .type = nothing__type_number};}
    free(utf8_file_name);
    return result;
}

// The function returns the current position in the file.
// If the function could not find the position in the file, then "nothing" is returned as a result.
type fs__get_position_in_file(void* file) {
    int64_t const position = ftell(file);
    if (position == -1) {return (type){.data = 0, .type = nothing__type_number};}
    return (type){.data = position, .type = int__type_number};
}

// The function sets the current position in the file.
// If the setting was successful, then the function returns "true", otherwise "false".
type fs__set_position_in_file(void* file, type new_position) {return (type){.data = fseek(file, new_position.data, SEEK_SET) == 0, .type = bool__type_numer};}

// The function renames the file.
// If the renaming was successful, then the function returns "true", otherwise "false".
type fs__file_rename(type old_file_name, type new_file_name) {
    uint8_t* const utf8_old_file_name = string__utf32_to_utf8(old_file_name);
    uint8_t* const utf8_new_file_name = string__utf32_to_utf8(new_file_name);
    type const result = (type){.data = rename((char*)utf8_old_file_name, (char*)utf8_new_file_name) == 0, .type = bool__type_numer};
    free(utf8_old_file_name);
    free(utf8_new_file_name);
    return result;
}

// The function renames the directory.
// If the renaming was successful, then the function returns "true", otherwise "false".
type fs__dir_rename(type old_dir_name, type new_dir_name) {return fs__file_rename(old_dir_name, new_dir_name);}

// The function prepares data for parsing the contents of a directory.
// If the function was unable to open the directory, then "nothing" is returned as a result.
type fs__open_dir(type dir_name) {
    uint8_t* const utf8_dir_name = string__utf32_to_utf8(dir_name);
    DIR* dir = opendir((char*)utf8_dir_name);
    free(utf8_dir_name);
    if (dir == NULL) {return (type){.data = 0, .type = nothing__type_number};}
    return (type){.data = (uint64_t)dir, .type = int__type_number};
}

// The function gets information about one of the objects from the directory.
type fs__read_dir(type dir, type* object_type) {
    for (;;) {
        struct dirent* const dir_entry = readdir((DIR*)dir.data);
        if (dir_entry == NULL) {return (type){.data = 0, .type = nothing__type_number};}
        char* const object_name = dir_entry->d_name;
        const uint64_t object_name_length = strlen(object_name);
        if (
            object_name[0] == '.' &&
            (object_name_length == 1 || (object_name_length == 2 && object_name[1] == '.'))
        ) {continue;}
        switch (dir_entry->d_type) {
        case DT_BLK:
            *object_type = (type){.data = 0, .type = int__type_number};
            break;
        case DT_CHR:
            *object_type = (type){.data = 1, .type = int__type_number};
            break;
        case DT_DIR:
            *object_type = (type){.data = 2, .type = int__type_number};
            break;
        case DT_FIFO:
            *object_type = (type){.data = 3, .type = int__type_number};
            break;
        case DT_LNK:
            *object_type = (type){.data = 4, .type = int__type_number};
            break;
        case DT_REG:
            *object_type = (type){.data = 5, .type = int__type_number};
            break;
        case DT_SOCK:
            *object_type = (type){.data = 6, .type = int__type_number};
            break;
        default:
            *object_type = (type){.data = 255, .type = int__type_number};
        }
        return string__utf8_to_utf32((const uint8_t*)object_name);
    }
}

// The function stops parsing the contents of the directory.
void fs__close_dir(type dir) {closedir((DIR*)dir.data);}

// The function creates a directory and if it succeeds, it returns "true".
// If the specified directory already exists and "ignore_existed_directory" is equal to "true", then the function returns "true".
type fs__make_dir(type dir_name, type ignore_existed_directory) {
    uint8_t* const utf8_dir_name = string__utf32_to_utf8(dir_name);
    bool result = mkdir((char*)utf8_dir_name, S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO) == 0;
    if (!result && (ignore_existed_directory.data & 1) == 1) {result = errno == EEXIST;}
    free(utf8_dir_name);
    return (type){.data = result, .type = bool__type_numer};
}

// The function return the name of the directory used to store temporary files.
type fs__get_tmp_dir_name() {return fs__tmp_dir_name;}

__attribute__((cold)) static type fs__problem_solver(const char* destination, const char* source, type* problem_solver, type (problem_solver_func)(type*, type, type, uint64_t, uint32_t, void*, bool), type problem_code, void* th_data) {
    type destination_utf32 = string__utf8_to_utf32((uint8_t*)destination);
    type source_utf32 = string__utf8_to_utf32((uint8_t*)source);
    type const result = problem_solver_func(problem_solver, destination_utf32, source_utf32, problem_code.data, problem_code.type, th_data, false);
    shar__rc_free(destination_utf32, th_data, false);
    shar__rc_free(source_utf32, th_data, false);
    return result;
}

static type fs__copy_link_utf8(const char* destination, const char* source, uint8_t** buffer, type* problem_solver, type (problem_solver_func)(type*, type, type, uint64_t, uint32_t, void*, bool), type (int_to_cptype)(type, void*, bool), void* th_data) {
    type result = (type){.data = 0, .type = nothing__type_number};
    if (*buffer == NULL) {*buffer = malloc(file_buffer_size);}
    uint64_t readed_len;
    for (;;) {
        readed_len = readlink(source, (char*)(*buffer), file_buffer_size);
        if (readed_len == -1) {
            result = fs__problem_solver(destination, source, problem_solver, problem_solver_func, int_to_cptype(fs__copy__problem__read_link, th_data, false), th_data);
            if (result.type == error__type_number) {return result;}
        } else {
            (*buffer)[readed_len] = 0;
            if (symlink((char*)(*buffer), destination) == 0) {break;}
            result = fs__problem_solver(destination, source, problem_solver, problem_solver_func, int_to_cptype(fs__copy__problem__create_symlink, th_data, false), th_data);
            break;
        }
    }
    return result;
}

static type fs__copy_file_utf8(const char* destination, const char* source, struct stat file_stat, int* pipefd, bool* allow_splice, uint8_t** buffer, type* problem_solver, type (problem_solver_func)(type*, type, type, uint64_t, uint32_t, void*, bool), type (int_to_cptype)(type, void*, bool), void* th_data) {
    type result = (type){.data = 0, .type = nothing__type_number};
    int src_fd;
    int dest_fd;
    src_fd = open(source, O_RDONLY);
    if (src_fd == -1) {
        result = fs__problem_solver(destination, source, problem_solver, problem_solver_func, int_to_cptype(fs__copy__problem__open_file, th_data, false), th_data);
        return result;
    }
    dest_fd = open(destination, O_CREAT | O_EXCL | O_WRONLY, file_stat.st_mode & ~S_IFMT);
    if (dest_fd == -1) {
        close(src_fd);
        result = fs__problem_solver(destination, source, problem_solver, problem_solver_func, int_to_cptype(fs__copy__problem__create_file, th_data, false), th_data);
        return result;
    }
    if (allow_splice) {
        for (;;) {
            uint64_t const readed_bytes_len = splice(src_fd, NULL, pipefd[1], NULL, file_buffer_size, SPLICE_F_MOVE);
            if (readed_bytes_len == 0) {break;}
            if (__builtin_expect(readed_bytes_len == -1, false)) {
                *allow_splice = false;
                if (*buffer == NULL) {*buffer = malloc(file_buffer_size);}
                break;
            }
            uint64_t const writed_bytes_len = splice(pipefd[0], NULL, dest_fd, NULL, file_buffer_size, SPLICE_F_MOVE);
            if (__builtin_expect(readed_bytes_len != writed_bytes_len, false)) {
                *allow_splice = false;
                if (*buffer == NULL) {*buffer = malloc(file_buffer_size);}
                break;
            }
        }
    }
    if (!allow_splice) {
        for (;;) {
            uint64_t const readed_bytes_len = read(src_fd, *buffer, file_buffer_size);
            if (readed_bytes_len == 0) {break;}
            if (readed_bytes_len == -1) {
                close(src_fd);
                close(dest_fd);
                result = fs__problem_solver(destination, source, problem_solver, problem_solver_func, int_to_cptype(fs__copy__problem__read_from_file, th_data, false), th_data);
                return result;
            }
            uint64_t const writed_bytes_len = write(dest_fd, *buffer, readed_bytes_len);
            if (writed_bytes_len != readed_bytes_len) {
                close(src_fd);
                close(dest_fd);
                result = fs__problem_solver(destination, source, problem_solver, problem_solver_func, int_to_cptype(fs__copy__problem__write_to_file, th_data, false), th_data);
                return result;
            }
        }
    }
    close(src_fd);
    close(dest_fd);
    chmod(destination, file_stat.st_mode & ~S_IFMT);
    chown(destination, file_stat.st_uid, file_stat.st_gid);
    return result;
}

static type fs__copy_dir_utf8(const char* destination, const char* source, struct stat dir_stat, int* pipefd, bool* allow_splice, uint8_t** buffer, type* problem_solver, type (problem_solver_func)(type*, type, type, uint64_t, uint32_t, void*, bool), type (int_to_cptype)(type, void*, bool), void* th_data) {
    type result = (type){.data = 0, .type = nothing__type_number};
    if (mkdir(destination, S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO) == 0) {
        chmod(destination, dir_stat.st_mode & ~S_IFMT);
        chown(destination, dir_stat.st_uid, dir_stat.st_gid);
    } else {
        result = fs__problem_solver(destination, source, problem_solver, problem_solver_func, int_to_cptype(fs__copy__problem__make_dir, th_data, false), th_data);
        if (result.type == error__type_number) {return result;}
    }
    uint64_t dirs_count = 0;
    char** dest_dirs_list = NULL;
    char** src_dirs_list = NULL;
    {
        DIR* dir;
        for (;;) {
            dir = opendir(source);
            if (dir != NULL) {break;}
            result = fs__problem_solver(destination, source, problem_solver, problem_solver_func, int_to_cptype(fs__copy__problem__open_dir, th_data, false), th_data);
            if (result.type == error__type_number) {return result;}
        }
        for (;;) {
            struct dirent* const dir_entry = readdir(dir);
            if (dir_entry == NULL) {break;}
            char* const object_name = dir_entry->d_name;
            uint64_t const object_name_length = strlen(object_name);
            if (
                object_name[0] == '.' &&
                (object_name_length == 1 || (object_name_length == 2 && object_name[1] == '.'))
            ) {continue;}
            uint64_t const dest_length = strlen(destination);
            char* dest_full_name = malloc(dest_length + object_name_length + 2);
            strcpy(dest_full_name, destination);
            dest_full_name[dest_length] = '/';
            dest_full_name[dest_length + 1] = 0;
            strcat(dest_full_name, object_name);
            uint64_t const src_length = strlen(source);
            char* src_full_name = malloc(src_length + object_name_length + 2);
            strcpy(src_full_name, source);
            src_full_name[src_length] = '/';
            src_full_name[src_length + 1] = 0;
            strcat(src_full_name, object_name);
            switch (dir_entry->d_type) {
            case DT_DIR:{
                dest_dirs_list = realloc(dest_dirs_list, (dirs_count + 1) * sizeof(char*));
                src_dirs_list = realloc(src_dirs_list, (dirs_count + 1) * sizeof(char*));
                dest_dirs_list[dirs_count] = dest_full_name;
                src_dirs_list[dirs_count] = src_full_name;
                dirs_count++;
                break;}
            case DT_LNK:{
                result = fs__copy_link_utf8(dest_full_name, src_full_name, buffer, problem_solver, problem_solver_func, int_to_cptype, th_data);
                free(dest_full_name);
                free(src_full_name);
                if (result.type == error__type_number) {goto endloop;}
                break;}
            default:{
                struct stat file_stat;
                lstat(src_full_name, &file_stat);
                result = fs__copy_file_utf8(dest_full_name, src_full_name, file_stat, pipefd, allow_splice, buffer, problem_solver, problem_solver_func, int_to_cptype, th_data);
                free(dest_full_name);
                free(src_full_name);
                if (result.type == error__type_number) {goto endloop;}}
            }
        }
        endloop:
        closedir(dir);
    }
    for (; dirs_count != 0; dirs_count--) {
        if (result.type == nothing__type_number) {
            struct stat dir_stat;
            lstat(src_dirs_list[dirs_count - 1], &dir_stat);
            result = fs__copy_dir_utf8(dest_dirs_list[dirs_count - 1], src_dirs_list[dirs_count - 1], dir_stat, pipefd, allow_splice, buffer, problem_solver, problem_solver_func, int_to_cptype, th_data);
        }
        free(dest_dirs_list[dirs_count - 1]);
        free(src_dirs_list[dirs_count - 1]);
    }
    if (dest_dirs_list != NULL) {free(dest_dirs_list);}
    if (src_dirs_list != NULL) {free(src_dirs_list);}
    return result;
}

// The function copies a file system object.
// If a problem occurs during copying, then control is transferred to the problem solver, if the solver solved the problem, the function continues its work.
type fs__copy(type destination, type source, type* problem_solver, type (problem_solver_func)(type*, type, type, /*type -> uint64_t, uint32_t (because of a clang bug)*/ uint64_t, uint32_t, void*, bool), type (int_to_cptype)(type, void*, bool), void* th_data) {
    bool allow_splice = true;
    uint8_t* buffer = NULL;
    struct stat fso_stat;
    int pipefd[2];
    if (__builtin_expect(pipe(pipefd) != 0, false)) {
        fprintf(stderr, "Failed to create a one-way communication channel (pipe).\n");
        exit(EXIT_FAILURE);
    }
    type result = (type){.data = 0, .type = nothing__type_number};
    char* source_utf8 = (char*)string__utf32_to_utf8(source);
    for (;;) {
        if (lstat(source_utf8, &fso_stat) == 0) {
            if ((fso_stat.st_mode & S_IFLNK) == S_IFLNK) {
                uint64_t source_len = strlen(source_utf8) + 1;
                uint64_t readed_len;
                for (;;) {
                    readed_len = readlink(source_utf8, source_utf8, source_len);
                    if (readed_len < source_len) {break;}
                    source_len *= 2;
                    source_utf8 = realloc(source_utf8, source_len);
                }
                if (readed_len == -1) {
                    result = problem_solver_func(problem_solver, destination, source, int_to_cptype(fs__copy__problem__read_link, th_data, false).data, int__type_number, th_data, false);
                    if (result.type == error__type_number) {
                        free(source_utf8);
                        close(pipefd[0]);
                        close(pipefd[1]);
                        return result;
                    }
                } else {source_utf8[readed_len] = 0;}
            } else {break;}
        } else {
            result = problem_solver_func(problem_solver, destination, source, int_to_cptype(fs__copy__problem__stat, th_data, false).data, int__type_number, th_data, false);
            if (result.type == error__type_number) {
                free(source_utf8);
                close(pipefd[0]);
                close(pipefd[1]);
                return result;
            }
        }
    }
    char* const destination_utf8 = (char*)string__utf32_to_utf8(destination);
    if ((fso_stat.st_mode & S_IFDIR) == S_IFDIR) {
        result = fs__copy_dir_utf8(destination_utf8, source_utf8, fso_stat, pipefd, &allow_splice, &buffer, problem_solver, problem_solver_func, int_to_cptype, th_data);
    } else {
        result = fs__copy_file_utf8(destination_utf8, source_utf8, fso_stat, pipefd, &allow_splice, &buffer, problem_solver, problem_solver_func, int_to_cptype, th_data);
    }
    free(destination_utf8);
    free(source_utf8);
    if (buffer != NULL) {free(buffer);}
    close(pipefd[0]);
    close(pipefd[1]);
    return result;
}

type fs__read_symlink(type link) {
    uint8_t* const link_utf8 = string__utf32_to_utf8(link);
    uint8_t stack_buffer[256];
    uint8_t* heap_buffer = NULL;
    uint8_t* buffer = stack_buffer;
    uint64_t buffer_size = 256;
    type result = (type){.data = 0, .type = nothing__type_number};
    for (;;) {
        uint64_t const readed_bytes_len = readlink((const char*)link_utf8, (char*)(buffer), buffer_size);
        if (readed_bytes_len == -1) {break;}
        if (readed_bytes_len < buffer_size) {
            buffer[readed_bytes_len] = 0;
            result = string__utf8_to_utf32(buffer);
            break;
        }
        buffer_size *= 2;
        heap_buffer = realloc(heap_buffer, buffer_size);
        buffer = heap_buffer;
    }
    free(link_utf8);
    if (heap_buffer != NULL) {free(heap_buffer);}
    return result;
}

type fs__create_symlink(type link_path, type src_object) {
    uint8_t* const link_path_utf8 = string__utf32_to_utf8(link_path);
    uint8_t* const src_object_utf8 = string__utf32_to_utf8(src_object);
    type result = (type){.data = 1 + symlink((const char*)src_object_utf8, (const char*)link_path_utf8), .type = bool__type_numer};
    free(link_path_utf8);
    free(src_object_utf8);
    return result;
}

// The function copies attributes from one file system object to another.
// The objects must be of the same type (file, directory, etc.).
// The function returns "true" if successful, otherwise "false".
type fs__copy_attributes(type destination, type source) {
    type result = (type) {.data = 0, .type = bool__type_numer};
    char* const destination_utf8 = (char*)string__utf32_to_utf8(destination);
    char* const source_utf8 = (char*)string__utf32_to_utf8(source);
    struct stat dest_stat;
    struct stat src_stat;
    if (
        (stat(destination_utf8, &dest_stat) == 0) &&
        (stat(source_utf8, &src_stat) == 0) &&
        ((dest_stat.st_mode & S_IFMT) == (src_stat.st_mode & S_IFMT)) &&
        ((chmod(destination_utf8, src_stat.st_mode & ~S_IFMT) | chown(destination_utf8, src_stat.st_uid, src_stat.st_gid)) == 0)
    ) {result = (type) {.data = 1, .type = bool__type_numer};}
    free(destination_utf8);
    free(source_utf8);
    return result;
}
#pragma endregion FS

#pragma region Time
// The function returns the current time.
uint64_t time__current() {
    struct timeval time;
    gettimeofday(&time, NULL);
    return ((uint64_t)time.tv_sec - timezone) * 1000000ull + time.tv_usec;
}

// The function returns the current time. (UTC)
uint64_t time__current_utc() {
    struct timeval time;
    gettimeofday(&time, NULL);
    return ((uint64_t)time.tv_sec) * 1000000ull + time.tv_usec;
}
#pragma endregion Time

#pragma region Libs
type lib__load(type file_name) {
    uint8_t* const utf8_file_name = string__utf32_to_utf8(file_name);
    void* lib = dlopen((char*)utf8_file_name, RTLD_LAZY | RTLD_GLOBAL);
    free(utf8_file_name);
    if (lib == NULL) {return (type){.data = 0, .type = nothing__type_number};}
    return (type){.data = (uint64_t)lib, .type = int__type_number};
}

type lib__get_object_address(type lib, type object_name) {
    uint8_t* const utf8_object_name = string__utf32_to_utf8(object_name);
    void* object = dlsym((void*)(lib.data), (char*)utf8_object_name);
    free(utf8_object_name);
    if (object == NULL) {return (type){.data = 0, .type = nothing__type_number};}
    return (type){.data = (uint64_t)object, .type = int__type_number};
}

type lib__unload_lib(type lib) {return (type){.data = (uint64_t)(dlclose((void*)(lib.data)) == 0), .type = bool__type_numer};}
#pragma endregion Libs

#pragma region Locale
static inline uint16_t lang_code(uint8_t char0, uint8_t char1) {
    return ((uint16_t)char1 << 8) | (uint16_t)char0;
}

// The function returns the system language code according to the "ISO 639-1" standard, in the form of 2 bytes.
type locale__lang_code() {
    char* locale = setlocale(LC_ALL, "");
    size_t lang_str_len = strcspn(locale, "_ ");
    uint16_t result = lang_code('e', 'n');
    if (lang_str_len == 2) {
        char part[3] = {locale[0], locale[1], 0};
        static const char* langs =
            "aa ab ae af ak am ar as av ay az "
            "ba be bg bi bm bn bo br bs "
            "ca ce ch co cs cu cv cy "
            "da de dv dz "
            "ee el en eo es et eu "
            "fa ff fi fj fl fo fr fy "
            "ga gd gl gn gu gv "
            "ha he hi ho hr hu hy hz "
            "ia id ie ig ik is it iu "
            "ja jv "
            "ka kg ki kj kk kl km kn ko kr ks ku kv kw ky "
            "la lb lg ln lo lt lu lv "
            "md me mg mh mi mk ml mn mr ms mt my "
            "na nd ne ng nl nn no nr nv ny "
            "oc oj om or os "
            "pa pi pl ps pt "
            "qu "
            "rm rn ro ru rw "
            "sa sc sd sg si sk sl sm sn so sq sr ss st su sv sw "
            "ta te tg th ti tk tl tn to tr ts tt tw ty "
            "ug uk ur uz "
            "ve vi vo "
            "wo "
            "xh "
            "yi yo "
            "za zh zu";
        if (strstr(langs, part) != NULL) {result = lang_code(part[0], part[1]);}
    }
    return (type){.data = result, .type = int__type_number};
}
#pragma endregion Locale

#pragma region Main
void* shar__init(int argc, char** argv, type (free_func)(type, void*, bool), type (use_func)(type, void*, bool)) {
    __argv__ = (uint8_t**) argv;
    __argc__ = argc;
    shar__rc_free = free_func;
    shar__rc_use = use_func;
    thread_data* const th_data = (thread_data*)malloc(sizeof(thread_data));
    th_data->cryptographic_random_number_index = 64;
    th_data->id = getpid();
    th_data->random_number_source[0] = int__get_cryptographic__random(th_data).data;
    th_data->random_number_source[1] = int__get_cryptographic__random(th_data).data;
    th_data->random_number_source[2] = int__get_cryptographic__random(th_data).data;
    cpu_cores_number = get_nprocs();
    tzset();
    return th_data;
}

// The use of threads is allowed only after this function has been executed.
void shar__enable__threads() {allow_threads = true;}

bool shar__end(type main_func_result, void* th_data) {
    bool result = false;
    if (main_func_result.type == error__type_number) {
        result = ((error*)(main_func_result.data))->id != error__id_no_error;
        if (result && (((error*)(main_func_result.data))->id != error__id_fail)) {string__println_as_error(error__get_message(main_func_result));}
        error__free(main_func_result, th_data);
    }
    for (;number_of_threads != 1;) {worker__sleep((type){.data = 100, .type = int__type_number});}
    free(th_data);
    return result || ignored_errors;
}

// The function terminates the program immediately without any error.
__attribute__((noreturn, cold)) void shar__exit() { exit(EXIT_SUCCESS); }

// The function terminates the program immediately, but the program is considered to have terminated incorrectly.
__attribute__((noreturn, cold)) void shar__fail() { exit(EXIT_FAILURE); }
#pragma endregion Main
