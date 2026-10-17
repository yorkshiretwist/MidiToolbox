#pragma once
#include "Arduino.h"
#include "Stream.h"
#include <cstdio>
#include <string>
#include <dirent.h>
#include <sys/stat.h>

#define BUILTIN_SDCARD 254

// open modes, as on the real SD library (FILE_WRITE creates the file if needed)
#define FILE_READ 0
#define FILE_WRITE 1

// desktop stand-in for the Teensyduino SD library, backed by the real host
// filesystem (rooted wherever the process's cwd is), for functional testing
// of directory listing / multi-file config loading outside real hardware.
class File : public Stream {
  public:
    File() : fp_(nullptr), dir_(nullptr) {}
    explicit File(FILE *fp, std::string path) : fp_(fp), dir_(nullptr), path_(path) {}
    explicit File(DIR *dir, std::string path) : fp_(nullptr), dir_(dir), path_(path) {}

    operator bool() const { return fp_ != nullptr || dir_ != nullptr; }

    size_t size() {
      if (!fp_) return 0;
      long pos = ftell(fp_);
      fseek(fp_, 0, SEEK_END);
      long sz = ftell(fp_);
      fseek(fp_, pos, SEEK_SET);
      return (size_t)sz;
    }

    void close() {
      if (fp_) fclose(fp_);
      if (dir_) closedir(dir_);
      fp_ = nullptr;
      dir_ = nullptr;
    }

    const char *name() {
      size_t slash = path_.find_last_of('/');
      staticName_ = (slash == std::string::npos) ? path_ : path_.substr(slash + 1);
      return staticName_.c_str();
    }

    bool isDirectory() { return dir_ != nullptr; }

    File openNextFile() {
      if (!dir_) return File();
      struct dirent *entry;
      while ((entry = readdir(dir_)) != nullptr) {
        std::string entryName = entry->d_name;
        if (entryName == "." || entryName == "..") continue;
        std::string fullPath = path_ + "/" + entryName;
        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
          DIR *subdir = opendir(fullPath.c_str());
          if (subdir) return File(subdir, fullPath);
        } else {
          FILE *f = fopen(fullPath.c_str(), "rb");
          if (f) return File(f, fullPath);
        }
      }
      return File();
    }

    int read() override { if (!fp_) return -1; return fgetc(fp_); }
    // block read, as on the real library: returns the number of bytes read
    int read(void *buffer, size_t length) {
      if (!fp_) return -1;
      return (int)fread(buffer, 1, length, fp_);
    }
    size_t write(const uint8_t *buffer, size_t length) {
      if (!fp_) return 0;
      return fwrite(buffer, 1, length, fp_);
    }
    bool seek(uint32_t position) { return fp_ && fseek(fp_, (long)position, SEEK_SET) == 0; }
    size_t readBytes(char *buffer, size_t length) override {
      if (!fp_) return 0;
      return fread(buffer, 1, length, fp_);
    }

  private:
    FILE *fp_;
    DIR *dir_;
    std::string path_;
    std::string staticName_;
};

// strips a leading '/' so SD-style absolute paths (relative to the SD card
// root) map onto the host process's current working directory instead of
// the real filesystem root
static std::string toHostPath(std::string sdPath) {
  if (!sdPath.empty() && sdPath[0] == '/') {
    sdPath = sdPath.substr(1);
  }
  // the SD root ("/") is the current directory on the host
  return sdPath.empty() ? "." : sdPath;
}

class SDClass {
  public:
    bool begin(uint8_t) { return true; }
    bool exists(const char *sdPath) {
      struct stat st;
      return stat(toHostPath(sdPath).c_str(), &st) == 0;
    }
    bool remove(const char *sdPath) { return ::remove(toHostPath(sdPath).c_str()) == 0; }
    bool rename(const char *from, const char *to) {
      return ::rename(toHostPath(from).c_str(), toHostPath(to).c_str()) == 0;
    }
    bool mkdir(const char *sdPath) {
#ifdef _WIN32
      return ::mkdir(toHostPath(sdPath).c_str()) == 0 || exists(sdPath);
#else
      return ::mkdir(toHostPath(sdPath).c_str(), 0777) == 0 || exists(sdPath);
#endif
    }
    File open(const char *sdPath, uint8_t mode) {
      if (mode == FILE_WRITE) {
        // like the real library, FILE_WRITE appends to (or creates) the file
        FILE *f = fopen(toHostPath(sdPath).c_str(), "ab");
        return f ? File(f, toHostPath(sdPath)) : File();
      }
      return open(sdPath);
    }
    File open(const char *sdPath) {
      std::string path = toHostPath(sdPath);
      struct stat st;
      if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path.c_str());
        if (dir) return File(dir, path);
        return File();
      }
      FILE *f = fopen(path.c_str(), "rb");
      if (f) return File(f, path);
      return File();
    }
};
extern SDClass SD;
