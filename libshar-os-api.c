#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <locale.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/types.h>

// Functions from this library work with strings that consist of two parts :
//    1 - string length
//    2 - pointer to string characters
// If the string length is 0, then the string is considered empty.
// The character in the string is a 16-bit unsigned number that represents the
// unicode character number (from 0 to 65535).
// If any function returns a string, and has an argument "reservedSpace", then
// this argument means how many bytes to allocate in memory before the
// characters of the string.

typedef struct {
  int64_t type;
  int64_t value;
} shar__type;

typedef struct {
  int64_t useCounter;
  int64_t capacity;
  int64_t indexOfFirst;
  int64_t count;
  pthread_mutex_t mutex;
  shar__type *data;
} shar__pipeline;

#pragma region Alloc
static __attribute__((always_inline)) void *safe_realloc(void *pointer,
                                                         uint64_t size) {
  void *const result = realloc(pointer, size);
  if (__builtin_expect(result == NULL, false)) {
    fprintf(stderr, "Not enough memory.\n");
    exit(EXIT_FAILURE);
  }
  return result;
}

static __attribute__((always_inline)) void *safe_malloc(uint64_t size) {
  void *const result = malloc(size);
  if (__builtin_expect(result == NULL, false)) {
    fprintf(stderr, "Not enough memory.\n");
    exit(EXIT_FAILURE);
  }
  return result;
}
#pragma endregion Alloc

#pragma region Convert
static __attribute__((always_inline)) uint8_t *
shar__string__to__c__string(uint64_t length, const uint16_t *chars) {
  uint8_t *result = NULL;
  uint64_t resultIndex = 0;
  uint64_t memorySize = 0;
  for (uint64_t charIndex = 0; charIndex < length; charIndex++) {
    uint16_t const currentChar = chars[charIndex];
    uint8_t addCharsSize = 0;
    if (currentChar == 0) {
      continue;
    }
    if (currentChar < 0x80) {
      addCharsSize = 1;
    } else if (currentChar < 0x800) {
      addCharsSize = 2;
    } else {
      addCharsSize = 3;
    }
    memorySize += addCharsSize;
    result = safe_realloc(result, memorySize);
    switch (addCharsSize) {
    case 1:
      result[resultIndex] = currentChar;
      break;
    case 2:
      result[resultIndex] = (currentChar >> 6) | 192;
      result[resultIndex + 1] = (currentChar & 63) | 128;
      break;
    case 3:
      result[resultIndex] = (currentChar >> 12) | 224;
      result[resultIndex + 1] = ((currentChar >> 6) & 63) | 128;
      result[resultIndex + 2] = (currentChar & 63) | 128;
      break;
    }
    resultIndex += addCharsSize;
  }
  result = safe_realloc(result, memorySize + 1);
  result[resultIndex] = 0;
  return result;
}

static __attribute__((always_inline)) bool
shar__c__string__to__string(uint64_t reservedSpace, const uint8_t *cString,
                            uint64_t *outLength, uint16_t **outChars) {
  uint16_t *resultChars;
  uint64_t resultLength = 0;
  if (__builtin_expect(reservedSpace == 0, false)) {
    resultChars = NULL;
  } else {
    resultChars = (uint16_t *)safe_malloc(reservedSpace);
  }
  uint64_t cStringIndex = 0;
  bool result = true;
  for (;;) {
    uint8_t currentCharPart = cString[cStringIndex];
    if (currentCharPart == 0) {
      break;
    }
    uint8_t charSize = 0;
    uint16_t newChar = currentCharPart;
    if (currentCharPart < 128) {
      charSize = 1;
    } else if ((currentCharPart & 224) == 192) {
      charSize = 2;
      currentCharPart = cString[cStringIndex + 1];
      if (__builtin_expect((currentCharPart & 192) != 128, false)) {
        result = false;
        break;
      }
      newChar ^= 192;
      newChar <<= 6;
      newChar |= currentCharPart ^ 128;
    } else if ((currentCharPart & 240) == 224) {
      charSize = 3;
      currentCharPart = cString[cStringIndex + 1];
      if (__builtin_expect((currentCharPart & 192) != 128, false)) {
        result = false;
        break;
      }
      newChar ^= 224;
      newChar <<= 6;
      newChar |= currentCharPart ^ 128;
      currentCharPart = cString[cStringIndex + 2];
      if (__builtin_expect((currentCharPart & 192) != 128, false)) {
        result = false;
        break;
      }
      newChar <<= 6;
      newChar |= currentCharPart ^ 128;
    } else {
      result = false;
      break;
    }
    resultLength++;
    resultChars = safe_realloc(resultChars, resultLength * 2 + reservedSpace);
    ((uint16_t *)(&(
        ((uint8_t *)resultChars)[reservedSpace])))[resultLength - 1] = newChar;
    cStringIndex += charSize;
  }
  if (__builtin_expect(!result, false)) {
    if (resultChars != NULL) {
      free(resultChars);
    }
    resultChars = NULL;
    resultLength = 0;
  }
  *outChars = resultChars;
  *outLength = resultLength;
  return result;
}
#pragma endregion Convert

#pragma region Env
static uint64_t __argc__;
static char **__argv__;
static int64_t cpuCoresNumber;

// The function gets the command line argument by index.
// The argument, at index 0, is the name of the program's executable file.
// If getting the argument was successful, then the function returns "true",
// otherwise "false".
// If the function returns "false", then "NULL" is written to the characters,
// and 0 is written to the length of the string.
bool shar__get__cmd__argument(uint64_t index, uint64_t reservedSpace,
                              uint64_t *outLength, uint16_t **outChars) {
  bool result = false;
  if (index < __argc__) {
    result = shar__c__string__to__string(
        reservedSpace, (uint8_t *)(__argv__[index]), outLength, outChars);
  } else {
    *outLength = 0;
    *outChars = NULL;
  }
  return result;
}

// The function returns the number of command line arguments, including the name
// of the program executable.
uint64_t shar__get__cmd__arguments__count() { return __argc__; }

// The function returns the value of the environment variable.
// If getting the value of the environment variable was successful, then the
// function returns "true", otherwise "false".
// If the function returns "false", then "NULL" is written to the characters,
// and 0 is written to the length of the string.
bool shar__get__env__variable(uint64_t nameLength, const uint16_t *nameChars,
                              uint64_t reservedSpace, uint64_t *outLength,
                              uint16_t **outChars) {
  uint8_t *varName = shar__string__to__c__string(nameLength, nameChars);
  const uint8_t *resultAsCString = (uint8_t *)getenv((char *)varName);
  free(varName);
  if (resultAsCString == NULL) {
    *outLength = 0;
    *outChars = NULL;
    return false;
  }
  bool result = shar__c__string__to__string(reservedSpace, resultAsCString,
                                            outLength, outChars);
  return result;
}

// The function gets a string from the command line.
// If getting the value of the string was successful, then the
// function returns "true", otherwise "false".
// If the function returns "false", then "NULL" is written to the characters,
// and 0 is written to the length of the string.
bool shar__get__string__from__cmd__line(uint64_t reservedSpace,
                                        uint64_t *outLength,
                                        uint16_t **outChars) {
  uint8_t buffer[256];
  uint16_t *resultChars;
  uint64_t resultLength = 0;
  if (__builtin_expect(reservedSpace == 0, false)) {
    resultChars = NULL;
  } else {
    resultChars = (uint16_t *)safe_malloc(reservedSpace);
  }
  uint64_t countOfPreviousBytes = 0;
  bool allOk = true;
  for (;;) {
    uint8_t *bufferOffset = &(buffer[countOfPreviousBytes]);
    if (fgets((char *)bufferOffset, 256 - countOfPreviousBytes, stdin) ==
        (char *)bufferOffset) {
      uint64_t cStringIndex = 0;
      countOfPreviousBytes = 0;
      for (;;) {
        uint8_t currentCharPart = buffer[cStringIndex];
        if (currentCharPart == '\n' || currentCharPart == 0) {
          *outChars = resultChars;
          *outLength = resultLength;
          return true;
        }
        uint8_t charSize = 0;
        uint16_t newChar = currentCharPart;
        if (currentCharPart < 128) {
          charSize = 1;
        } else if ((currentCharPart & 224) == 192) {
          charSize = 2;
          if (cStringIndex == 255) {
            countOfPreviousBytes = 1;
            break;
          }
          currentCharPart = buffer[cStringIndex + 1];
          if (__builtin_expect((currentCharPart & 192) != 128, false)) {
            allOk = false;
            break;
          }
          newChar ^= 192;
          newChar <<= 6;
          newChar |= currentCharPart ^ 128;
        } else if ((currentCharPart & 240) == 224) {
          charSize = 3;
          if (cStringIndex > 253) {
            countOfPreviousBytes = 256 - cStringIndex;
            break;
          }
          currentCharPart = buffer[cStringIndex + 1];
          if (__builtin_expect((currentCharPart & 192) != 128, false)) {
            allOk = false;
            break;
          }
          newChar ^= 224;
          newChar <<= 6;
          newChar |= currentCharPart ^ 128;
          currentCharPart = buffer[cStringIndex + 2];
          if (__builtin_expect((currentCharPart & 192) != 128, false)) {
            allOk = false;
            break;
          }
          newChar <<= 6;
          newChar |= currentCharPart ^ 128;
        } else {
          allOk = false;
          break;
        }
        cStringIndex += charSize;
        resultLength++;
        resultChars =
            safe_realloc(resultChars, resultLength * 2 + reservedSpace);
        ((uint16_t *)(&(
            ((uint8_t *)resultChars)[reservedSpace])))[resultLength - 1] =
            newChar;
        if (cStringIndex == 256) {
          break;
        }
      }
      if (__builtin_expect(!allOk, false)) {
        break;
      }
      for (uint64_t offset = 0; offset < countOfPreviousBytes; offset++) {
        buffer[offset] = buffer[256 - countOfPreviousBytes + offset];
      }
    } else if (resultLength != 0 && countOfPreviousBytes == 0) {
      *outChars = resultChars;
      *outLength = resultLength;
      return true;
    } else {
      break;
    }
  }
  if (resultChars != NULL) {
    free(resultChars);
  }
  *outChars = NULL;
  *outLength = 0;
  return false;
}

// The function returns the name of the platform on which the program was
// launched.
void shar__get__platform__name(uint64_t reservedSpace, uint64_t *outLength,
                               uint16_t **outChars) {
  *outChars = (uint16_t *)safe_malloc(reservedSpace + 24);
  *outLength = 12;
  uint16_t *realChars = (uint16_t *)(&(((uint8_t *)*outChars)[reservedSpace]));
  realChars[0] = 'L';
  realChars[1] = 'i';
  realChars[2] = 'n';
  realChars[3] = 'u';
  realChars[4] = 'x';
  realChars[5] = ' ';
  realChars[6] = 'x';
  realChars[7] = '8';
  realChars[8] = '6';
  realChars[9] = '_';
  realChars[10] = '6';
  realChars[11] = '4';
}

// The function executes the command in the host environment.
// If the execution was successful, then the
// function returns "true", otherwise "false".
bool shar__execute__command(uint64_t commandLength,
                            const uint16_t *commandChars) {
  if (commandLength == 0) {
    return false;
  }
  uint8_t *cCommand = shar__string__to__c__string(commandLength, commandChars);
  bool result = system((char *)cCommand) == 0;
  free(cCommand);
  return result;
}

// The function returns the number of processor cores.
int64_t shar__get__cpu__cores__number() { return cpuCoresNumber; }
#pragma endregion Env

#pragma region FS
// The function deletes the file at the specified path.
// If the delete was successful, then the
// function returns "true", otherwise "false".
bool shar__delete__file(uint64_t fileNameLength,
                        const uint16_t *fileNameChars) {
  uint8_t *cFileName =
      shar__string__to__c__string(fileNameLength, fileNameChars);
  struct stat fileStat;
  bool result = stat((char *)cFileName, &fileStat) == 0;
  result = result && (fileStat.st_mode & S_IFDIR) == 0;
  if (result) {
    result = remove((char *)cFileName) == 0;
  }
  free(cFileName);
  return result;
}

// The function deletes the empty directory at the specified path.
// If the delete was successful, then the
// function returns "true", otherwise "false".
bool shar__delete__empty__directory(uint64_t fileNameLength,
                                    const uint16_t *fileNameChars) {
  uint8_t *cDirectoryName =
      shar__string__to__c__string(fileNameLength, fileNameChars);
  struct stat fileStat;
  bool result = stat((char *)cDirectoryName, &fileStat) == 0;
  result = result && (fileStat.st_mode & S_IFDIR) != 0;
  if (result) {
    result = remove((char *)cDirectoryName) == 0;
  }
  free(cDirectoryName);
  return result;
}

// If the file exists at the specified path, the function returns "true"
// otherwise "false".
bool shar__file__is__exist(uint64_t fileNameLength,
                           const uint16_t *fileNameChars) {
  uint8_t *cFileName =
      shar__string__to__c__string(fileNameLength, fileNameChars);
  struct stat fileStat;
  bool result = stat((char *)cFileName, &fileStat) == 0;
  free(cFileName);
  result = result && (fileStat.st_mode & S_IFDIR) == 0;
  return result;
}

// If the directory exists at the specified path, the function returns "true"
// otherwise "false".
bool shar__directory__is__exist(uint64_t fileNameLength,
                                const uint16_t *fileNameChars) {
  uint8_t *cDirectoryName =
      shar__string__to__c__string(fileNameLength, fileNameChars);
  struct stat fileStat;
  bool result = stat((char *)cDirectoryName, &fileStat) == 0;
  free(cDirectoryName);
  result = result && (fileStat.st_mode & S_IFDIR) != 0;
  return result;
}

// The function opens the file in the specified mode.
// If the file can be opened, the function returns "true", otherwise "false".
// Modes:
// 25202 (rb) - opens a file for reading. The file must exist.
// 25207 (wb) - opens a file for writing. If the file does not exist, then the
// function will create it, if the file already exists, then its contents will
// be deleted.
// 25185 (ab) - opens a file for writing. If the file does not exist, then the
// function will create it, if the file already exists, then writing will be
// made to the end of the file.
// 6433650 (r+b) - opens a file for reading and writing. The file must exist.
// The read and write position is at the beginning.
// 6433655 (w+b) - opens a file for reading and writing. If the file does not
// exist, then the function will create it, if the file already exists, then its
// contents will be deleted.
// 6433633 (a+b) - opens a file for reading and writing. If the file does not
// exist, then the function will create it, if the file already exists, the read
// and write position is at the end.
bool shar__open__file(uint64_t fileNameLength, const uint16_t *fileNameChars,
                      uint32_t mode, void **outFile) {
  uint8_t *cFileName =
      shar__string__to__c__string(fileNameLength, fileNameChars);
  FILE *file = fopen((char *)cFileName, (char *)(&mode));
  free(cFileName);
  *outFile = file;
  return file != NULL;
}

// The function flushes the output buffer.
// If the flushed was successful, then the
// function returns "true", otherwise "false".
bool shar__flush__file(void *file) { return fflush(file) == 0; }

// The function flushes the output buffer and close the file.
// If the closed was successful, then the
// function returns "true", otherwise "false".
bool shar__close__file(void *file) { return fclose(file) == 0; }

// The function reading data from the file into memory, which should already be
// allocated.
uint64_t shar__read__from__file(void *file, uint64_t countOfBytes,
                                uint8_t *memory) {
  uint64_t result = fread(memory, 1, countOfBytes, file);
  return result;
}

// The function writing data from memory to the file.
uint64_t shar__write__to__file(void *file, uint64_t countOfBytes,
                               const uint8_t *memory) {
  uint64_t result = fwrite(memory, 1, countOfBytes, file);
  return result;
}

// The function gets the size of the file at the specified path.
// If the function was unable to find out the file size, then a number less than
// zero is returned as a result.
int64_t shar__get__file__size(uint64_t fileNameLength,
                              const uint16_t *fileNameChars) {
  uint8_t *cFileName =
      shar__string__to__c__string(fileNameLength, fileNameChars);
  struct stat fileStat;
  bool allOk = stat((char *)cFileName, &fileStat) == 0;
  allOk = allOk && (fileStat.st_mode & S_IFDIR) == 0;
  int64_t result;
  if (allOk) {
    result = fileStat.st_size;
  } else {
    result = -1;
  }
  free(cFileName);
  return result;
}

// The function returns the current position in the file.
// If the function failed to find out the position in the file, then a number
// less than zero is returned as a result.
int64_t shar__get__position__in__file(void *file) { return ftell(file); }

// The function sets the current position in the file.
// If the setting was successful, then
// the function returns "true", otherwise "false".
bool shar__set__position__in__file(void *file, uint64_t newPosition) {
  return fseek(file, newPosition, SEEK_SET) == 0;
}

// The function renames the file.
// If the renaming was successful, then
// the function returns "true", otherwise "false".
bool shar__file__rename(uint64_t oldFileNameLength,
                        const uint16_t *oldFileNameChars,
                        uint64_t newFileNameLength,
                        const uint16_t *newFileNameChars) {
  uint8_t *cOldFileName =
      shar__string__to__c__string(oldFileNameLength, oldFileNameChars);
  uint8_t *cNewFileName =
      shar__string__to__c__string(newFileNameLength, newFileNameChars);
  bool result = rename((char *)cOldFileName, (char *)cNewFileName) == 0;
  free(cOldFileName);
  free(cNewFileName);
  return result;
}

// The function prepares data for parsing the contents of a directory.
// If the preparation was successful, the function returns "true".
bool shar__open__directory(uint64_t dirNameLength, const uint16_t *dirNameChars,
                           int64_t *outDir) {
  uint8_t *cDirName = shar__string__to__c__string(dirNameLength, dirNameChars);
  DIR *directory = opendir((char *)cDirName);
  *outDir = (int64_t)directory;
  free(cDirName);
  return directory != NULL;
}

// The function gets information about one of the objects from the directory.
// If the information was obtained function returns "true".
bool shar__read__directory(int64_t dir, uint64_t reservedSpace,
                           uint64_t *fsObjectNameLength,
                           uint16_t **fsObjectNameChars, uint8_t *objectType) {
  bool result;
  for (;;) {
    struct dirent *directoryEntry = readdir((DIR *)dir);
    result = directoryEntry != NULL;
    if (!result) {
      break;
    }
    char *objectName = directoryEntry->d_name;
    const unsigned long objectNameLength = strlen(objectName);
    if (objectName[0] == '.' &&
        (objectNameLength == 1 ||
         (objectNameLength == 2 && objectName[1] == '.'))) {
      continue;
    }
    shar__c__string__to__string(reservedSpace, (uint8_t *)objectName,
                                fsObjectNameLength, fsObjectNameChars);
    switch (directoryEntry->d_type) {
    case DT_BLK:
      *objectType = 0;
      break;
    case DT_CHR:
      *objectType = 1;
      break;
    case DT_DIR:
      *objectType = 2;
      break;
    case DT_FIFO:
      *objectType = 3;
      break;
    case DT_LNK:
      *objectType = 4;
      break;
    case DT_REG:
      *objectType = 5;
      break;
    case DT_SOCK:
      *objectType = 6;
      break;
    default:
      *objectType = 255;
      break;
    }
    break;
  }
  return result;
}

// The function stops parsing the contents of the directory.
void shar__close__directory(int64_t dir) { closedir((DIR *)dir); }

// The function creates a directory and if it succeeds, it returns "true".
// If the specified directory already exists and "ignoreExistedDirectory" is
// equal to "true", then the function returns "true".
bool shar__make__directory(uint64_t directoryNameLength,
                           const uint16_t *directoryNameChars,
                           bool ignoreExistedDirectory) {
  uint8_t *cDirectoryName =
      shar__string__to__c__string(directoryNameLength, directoryNameChars);
  bool result =
      mkdir((char *)cDirectoryName, S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO) == 0;
  if (!result && ignoreExistedDirectory) {
    result = errno == EEXIST;
  }
  free(cDirectoryName);
  return result;
}

// The function writes the name of the directory used to store temporary files.
void shar__get__tmp__dir__name(uint64_t reservedSpace, uint64_t *outLength,
                               uint16_t **outChars) {
  shar__c__string__to__string(reservedSpace, (uint8_t *)"/tmp", outLength,
                              outChars);
}
#pragma endregion FS

#pragma region Print
static __attribute__((always_inline)) void
print(uint64_t length, const uint16_t *chars, FILE *file, bool endIsNewLine) {
  if (length > 0 || endIsNewLine) {
    uint8_t buffer[256];
    uint64_t bufferIndex = 0;
    for (uint64_t charIndex = 0; charIndex < length; charIndex++) {
      uint16_t currentChar = chars[charIndex];
      if (currentChar == 0) {
        continue;
      }
      if (currentChar < 0x80) {
        buffer[bufferIndex] = currentChar;
        bufferIndex++;
      } else if (currentChar < 0x800) {
        buffer[bufferIndex] = (currentChar >> 6) | 192;
        bufferIndex++;
        buffer[bufferIndex] = (currentChar & 63) | 128;
        bufferIndex++;
      } else {
        buffer[bufferIndex] = (currentChar >> 12) | 224;
        bufferIndex++;
        buffer[bufferIndex] = ((currentChar >> 6) & 63) | 128;
        bufferIndex++;
        buffer[bufferIndex] = (currentChar & 63) | 128;
        bufferIndex++;
      }
      if (bufferIndex > 253) {
        fwrite(buffer, 1, bufferIndex, file);
        bufferIndex = 0;
      }
    }
    if (endIsNewLine) {
      buffer[bufferIndex] = '\n';
      bufferIndex++;
    };
    if (bufferIndex != 0) {
      fwrite(buffer, 1, bufferIndex, file);
    }
    fflush(file);
  }
}

// The function prints a string to the terminal.
void shar__print__string(uint64_t length, const uint16_t *chars) {
  print(length, chars, stdout, false);
}

// The function prints a string, with a newline appended at the end, to the
// terminal.
void shar__println__string(uint64_t length, const uint16_t *chars) {
  print(length, chars, stdout, true);
}

// The function prints the string as an error.
void shar__print__as__error(uint64_t length, const uint16_t *chars) {
  print(length, chars, stderr, false);
}

// The function prints the string as an error, with a newline appended at the
// end.
void shar__println__as__error(uint64_t length, const uint16_t *chars) {
  print(length, chars, stderr, true);
}

// The function prints the string as an error and terminates the program.
__attribute__((noreturn, cold)) void shar__print__error(uint64_t length,
                                                        const uint16_t *chars) {
  fwrite("Error: ", 1, 7, stderr);
  print(length, chars, stderr, true);
  exit(EXIT_FAILURE);
}

// The function prints the C string as an error and terminates the program.
__attribute__((noreturn, cold)) void
shar__print__builtin__error(const char *message) {
  fprintf(stderr, "Error: %s\n", message);
  exit(EXIT_FAILURE);
}
#pragma endregion Print

#pragma region Random
static _Atomic(uint64_t) randomNumberSource[5] = {0, 0, 0, 0, 0};
static _Atomic uint64_t currentRandomNumberIndex = 4096;
static uint64_t randomNumbers[4096];
static pthread_mutex_t cryptographic__random__number__mutex =
    PTHREAD_MUTEX_INITIALIZER;

// The function returns a random number.
uint64_t shar__get__random__number() {
  uint64_t result = ((randomNumberSource[0] >> 16ull) & 32767ull) |
                    (((randomNumberSource[1] >> 1ull) & (32767ull << 15ull))) |
                    (((randomNumberSource[2] << 14ull) & (32767ull << 30ull))) |
                    (((randomNumberSource[3] << 29ull) & (32767ull << 45ull))) |
                    (((randomNumberSource[4] << 44ull) & (32767ull << 60ull)));
  randomNumberSource[0] = randomNumberSource[0] * 1103515245 + 12345;
  randomNumberSource[1] = randomNumberSource[0] * 1103515245 + 12345;
  randomNumberSource[2] = randomNumberSource[0] * 1103515245 + 12345;
  randomNumberSource[3] = randomNumberSource[0] * 1103515245 + 12345;
  randomNumberSource[4] = randomNumberSource[0] * 1103515245 + 12345;
  return result;
}

// The function returns a random number suitable for cryptographic purposes.
uint64_t shar__get__cryptographic__random__number() {
  if (currentRandomNumberIndex == 4096) {
    pthread_mutex_lock(&cryptographic__random__number__mutex);
    if (currentRandomNumberIndex == 4096) {
      FILE *file = fopen("/dev/urandom", "r");
      if (__builtin_expect((file == NULL) ||
                               (fread(randomNumbers, 8, 4096, file) != 4096),
                           false)) {
        fprintf(stderr, "Can't read the file \x22/dev/urandom\x22.\n");
        exit(EXIT_FAILURE);
      }
      fclose(file);
      currentRandomNumberIndex = 0;
    }
    pthread_mutex_unlock(&cryptographic__random__number__mutex);
  }
  uint64_t result = randomNumbers[currentRandomNumberIndex];
  currentRandomNumberIndex++;
  return result;
}
#pragma endregion Random

#pragma region Time
// The function returns the current time.
uint64_t shar__get__current__time() {
  struct timeval time;
  gettimeofday(&time, NULL);
  return ((int64_t)time.tv_sec - timezone) * 1000000 + time.tv_usec;
}

// The function returns the current time. (UTC)
uint64_t shar__get__current__utc__time() {
  struct timeval time;
  gettimeofday(&time, NULL);
  return ((int64_t)time.tv_sec) * 1000000 + time.tv_usec;
}
#pragma endregion Time

#pragma region Libs
bool shar__load__lib(uint64_t fileNameLength, const uint16_t *fileNameChars,
                     void **outLib) {
  uint8_t *cFileName =
      shar__string__to__c__string(fileNameLength, fileNameChars);
  void *lib = dlopen((char *)cFileName, RTLD_LAZY | RTLD_GLOBAL);
  free(cFileName);
  *outLib = lib;
  return lib != NULL;
}

bool shar__get__from__lib(uint64_t objectNameLength,
                          const uint16_t *objectNameChars, void *lib,
                          void **outObject) {
  uint8_t *cObjectName =
      shar__string__to__c__string(objectNameLength, objectNameChars);
  void *object = dlsym(lib, (char *)cObjectName);
  free(cObjectName);
  *outObject = object;
  return object != NULL;
}

bool shar__unload__lib(void *lib) { return dlclose(lib) == 0; }
#pragma endregion Libs

#pragma region Thread
static bool allowThreads = false;
static _Atomic int64_t numberOfActiveThreads = 1;

void shar__pipeline__use__counter__inc(int64_t pipeline) {
  shar__pipeline *pipelinePointer = (shar__pipeline *)pipeline;
  pthread_mutex_lock(&(pipelinePointer->mutex));
  pipelinePointer->useCounter++;
  pthread_mutex_unlock(&(pipelinePointer->mutex));
}

// The function returns true if the pipeline should be destroyed.
bool shar__pipeline__use__counter__dec(int64_t pipeline) {
  bool result;
  shar__pipeline *pipelinePointer = (shar__pipeline *)pipeline;
  pthread_mutex_lock(&(pipelinePointer->mutex));
  pipelinePointer->useCounter--;
  result = pipelinePointer->useCounter == 0;
  pthread_mutex_unlock(&(pipelinePointer->mutex));
  return result;
}

bool shar__pipeline__is__use(int64_t pipeline) {
  bool result;
  shar__pipeline *pipelinePointer = (shar__pipeline *)pipeline;
  pthread_mutex_lock(&(pipelinePointer->mutex));
  result = pipelinePointer->useCounter != 1;
  pthread_mutex_unlock(&(pipelinePointer->mutex));
  return result;
}

int64_t shar__pipeline__get__data__for__free(int64_t pipeline) {
  shar__pipeline *pipelinePointer = (shar__pipeline *)pipeline;
  return (int64_t)(&(pipelinePointer->data[pipelinePointer->indexOfFirst]));
}

int64_t shar__pipeline__get__count__for__free(int64_t pipeline) {
  shar__pipeline *pipelinePointer = (shar__pipeline *)pipeline;
  return pipelinePointer->count;
}

int64_t shar__create__pipeline() {
  shar__pipeline *result = safe_malloc(sizeof(shar__pipeline));
  *result = (shar__pipeline){
      .useCounter = 1,
      .capacity = 32,
      .indexOfFirst = 0,
      .count = 0,
      .data = (shar__type *)safe_malloc(32 * sizeof(shar__type))};
  if (__builtin_expect(pthread_mutex_init(&(result->mutex), NULL) != 0,
                       false)) {
    fprintf(stderr, "Unable to create mutex.\n");
    exit(EXIT_FAILURE);
  }
  return (int64_t)result;
}

void shar__pipeline__push(int64_t pipeline, shar__type newValue) {
  shar__pipeline *pipelinePointer = (shar__pipeline *)pipeline;
  pthread_mutex_lock(&(pipelinePointer->mutex));
  if (pipelinePointer->capacity ==
      (pipelinePointer->indexOfFirst + pipelinePointer->count)) {
    if (pipelinePointer->indexOfFirst == 0) {
      pipelinePointer->capacity *= 2;
      pipelinePointer->data =
          safe_realloc(pipelinePointer->data,
                       sizeof(shar__type) * pipelinePointer->capacity);
    } else {
      memmove(pipelinePointer->data,
              &(pipelinePointer->data[pipelinePointer->indexOfFirst]),
              sizeof(shar__type) * pipelinePointer->count);
      pipelinePointer->indexOfFirst = 0;
    }
  }
  pipelinePointer
      ->data[pipelinePointer->indexOfFirst + pipelinePointer->count] = newValue;
  pipelinePointer->count++;
  pthread_mutex_unlock(&(pipelinePointer->mutex));
}

shar__type shar__pipeline__pop(int64_t pipeline) {
  shar__pipeline *pipelinePointer = (shar__pipeline *)pipeline;
  shar__type result;
  pthread_mutex_lock(&(pipelinePointer->mutex));
  if (pipelinePointer->count == 0) {
    result = (shar__type){.type = 0, .value = 0};
  } else {
    result = pipelinePointer->data[pipelinePointer->indexOfFirst];
    pipelinePointer->indexOfFirst++;
    pipelinePointer->count--;
  }
  pthread_mutex_unlock(&(pipelinePointer->mutex));
  return result;
}

void shar__destroy__pipeline(int64_t pipeline) {
  shar__pipeline *pipelinePointer = (shar__pipeline *)pipeline;
  pthread_mutex_destroy(&(pipelinePointer->mutex));
  free(pipelinePointer->data);
  free(pipelinePointer);
}

static void *run_worker(void *args) {
  numberOfActiveThreads++;
  int64_t *workerArgs = (int64_t *)args;
  shar__type (*function)(shar__type, shar__type) =
      (shar__type(*)(shar__type, shar__type))(workerArgs[0]);
  shar__type (*freeFunction)(shar__type) =
      (shar__type(*)(shar__type))(workerArgs[1]);
  shar__type in = (shar__type){.type = workerArgs[2], .value = workerArgs[3]};
  shar__type out = (shar__type){.type = workerArgs[4], .value = workerArgs[5]};
  shar__pipeline__use__counter__inc(in.value);
  shar__pipeline__use__counter__inc(out.value);
  shar__type result = function(in, out);
  shar__pipeline__push(out.value, result);
  freeFunction(in);
  freeFunction(out);
  numberOfActiveThreads--;
  return NULL;
}

// The function returns "true" if the worker was launched successfully.
void shar__create__worker(shar__type (*function)(shar__type, shar__type),
                          shar__type in, shar__type out,
                          shar__type (*freeFunction)(shar__type)) {
  if (__builtin_expect(!allowThreads, false)) {
    fprintf(stderr, "At the stage of calculating constants, it is forbidden to "
                    "use threads.\n");
    exit(EXIT_FAILURE);
  }
  pthread_t thread;
  int64_t *workerArgs = safe_malloc(48);
  workerArgs[0] = (int64_t)function;
  workerArgs[1] = (int64_t)freeFunction;
  workerArgs[2] = in.type;
  workerArgs[3] = in.value;
  workerArgs[4] = out.type;
  workerArgs[5] = out.value;
  if (__builtin_expect(
          pthread_create(&thread, NULL, run_worker, workerArgs) != 0, false)) {
    fprintf(stderr, "Failed to start new thread.\n");
    exit(EXIT_FAILURE);
  }
}

void shar__yield() { sched_yield(); }

void shar__sleep(int64_t milliseconds) {
  struct timespec time;
  time.tv_sec = milliseconds / (int64_t)1000;
  time.tv_nsec = (milliseconds % (int64_t)1000) * (int64_t)1000000;
  int result;
  do {
    result = nanosleep(&time, &time);
  } while (result != 0 && errno == EINTR);
}

// The function returns the number of running threads.
int64_t shar__get__threads__number() { return numberOfActiveThreads; }

int64_t shar__get__pipeline__items__count(int64_t pipelineAsInt) {
  shar__pipeline *pipeline = (shar__pipeline *)pipelineAsInt;
  int64_t result;
  pthread_mutex_lock(&(pipeline->mutex));
  result = pipeline->count;
  pthread_mutex_unlock(&(pipeline->mutex));
  return result;
}
#pragma endregion Thread

#pragma region Locale
static uint64_t langAsUInt16(uint8_t char0, uint8_t char1) {
  return ((uint16_t)char1 << 8) | (uint16_t)char0;
}

// The function returns the system language code according to the "ISO 639-1"
// standard, in the form of 2 bytes.
uint16_t shar__get__language__code() {
  char *locale = setlocale(LC_ALL, "");
  size_t langStrLen = strcspn(locale, "_ ");
  uint16_t result = langAsUInt16('e', 'n');
  if (langStrLen == 2) {
    char part[3] = {locale[0], locale[1], 0};
    static const char *langs =
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
        "za zh zu ";
    if (strstr(langs, part) != NULL) {
      result = langAsUInt16(part[0], part[1]);
    }
  }
  return result;
}
#pragma endregion Locale

#pragma region Main
// Before calling the rest of the functions from this library, this function
// must be called (once).
void shar__init(int argc, char **argv) {
  __argc__ = argc;
  __argv__ = argv;
  randomNumberSource[0] = shar__get__cryptographic__random__number();
  randomNumberSource[1] = shar__get__cryptographic__random__number();
  randomNumberSource[2] = shar__get__cryptographic__random__number();
  randomNumberSource[3] = shar__get__cryptographic__random__number();
  randomNumberSource[4] = shar__get__cryptographic__random__number();
  cpuCoresNumber = get_nprocs();
  tzset();
}

// The use of threads is allowed only after this function has been executed.
void shar__enable__threads() { allowThreads = true; }

// After the functions from this library are no longer needed, you need to
// call this function.
void shar__end() {
  pthread_mutex_destroy(&cryptographic__random__number__mutex);
}

// The function terminates the program immediately without any error.
__attribute__((noreturn, cold)) void shar__exit() { exit(EXIT_SUCCESS); }

// The function terminates the program immediately, but the program is
// considered to have terminated incorrectly.
__attribute__((noreturn, cold)) void shar__fail() { exit(EXIT_FAILURE); }
#pragma endregion Main
