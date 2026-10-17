//----------------------------------------------------------------------------//
//                                                                            //
//    MIDI Toolbox                                                            //
//                                                                            //
//    A highly-configurable MIDI controller operated by footswitches,         //
//    designed for use by a musician already using their hands (e.g. a        //
//    guitarist).                                                             //
//                                                                            //
//    Copyright (C) 2024 Chris Taylor                                         //
//    chris@stillbreathing.co.uk                                              //
//    https://stillbreathing.co.uk                                            //
//                                                                            //
//----------------------------------------------------------------------------//
//                                                                            //
//    This program is free software: you can redistribute it and/or modify    //
//    it under the terms of the GNU General Public License as published by    //
//    the Free Software Foundation, either version 3 of the License, or       //
//    (at your option) any later version.                                     //
//                                                                            //
//    This program is distributed in the hope that it will be useful,         //
//    but WITHOUT ANY WARRANTY; without even the implied warranty of          //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           //
//    GNU General Public License for more details.                            //
//                                                                            //
//    You should have received a copy of the GNU General Public License       //
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.  //
//                                                                            //
//----------------------------------------------------------------------------//
//                                                                            //
//    The file-transfer protocol the desktop app uses over USB serial to      //
//    manage the SD card (device.json and config files), reboot the unit,     //
//    and put it into the Teensy bootloader so new firmware can be flashed.   //
//    The full description, with examples, is in docs/serial-protocol.md.     //
//                                                                            //
//    It is written against a tiny ProtocolPort interface instead of the      //
//    Arduino `Serial` object, so the host unit tests can run it against an   //
//    in-memory stand-in. The .ino supplies the real port and the few         //
//    hardware actions (reboot, bootloader) as hooks.                         //
//                                                                            //
//----------------------------------------------------------------------------//

// include guard - see the top of Note.h for a full explanation
#ifndef SerialProtocol_h
#define SerialProtocol_h

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// the name and version a unit reports in reply to HELLO. The version only
// changes when the protocol itself changes in a way an older app could not
// cope with.
#define PROTOCOL_NAME "MidiToolbox"
#define PROTOCOL_VERSION 1

// reported by INFO so the app can show (and later compare) what is installed
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "0.1.0"
#endif

// no uploaded file may be bigger than this
const uint32_t PROTOCOL_MAX_FILE_SIZE = 256UL * 1024UL;
// an upload that goes quiet for this long is abandoned
const uint32_t PROTOCOL_STALL_TIMEOUT_MS = 3000;
// the half-finished upload lives here until its CRC has been checked; it is
// never a valid target for any command
const char PROTOCOL_TEMP_PATH[] = "/UPLOAD.TMP";

// ----------------------------------------------------------------------------
// what the protocol needs from the outside world

// "class ... { virtual ... = 0; }" declares an abstract interface: a list of
// functions someone else must supply, like a C# interface or a Python
// abstract base class. The .ino implements it on top of Serial; the unit
// tests implement it on top of two in-memory buffers.
class ProtocolPort {
  public:
    virtual ~ProtocolPort() {}
    // how many bytes can be read right now without waiting
    virtual int available() = 0;
    // reads one byte (only call when available() > 0)
    virtual int read() = 0;
    // reads up to `max` of the bytes that are available now; never waits
    virtual size_t readAvailable(uint8_t *buffer, size_t max) = 0;
    virtual size_t write(const uint8_t *buffer, size_t length) = 0;
    // blocks until everything written has been sent
    virtual void flush() = 0;
    // milliseconds since power-on (millis() on the real unit)
    virtual unsigned long now() = 0;
};

// The few things that depend on the rest of the firmware. Plain function
// pointers (rather than virtual functions) because each is a single free
// function in the .ino.
struct ProtocolHooks {
  // whether the SD card started up
  bool (*sdReady)();
  // true while any button is on - a restarting unit would leave its notes
  // stuck on, so REBOOT and BOOTLOADER are refused while this is true
  // (unless the sender says "force")
  bool (*notesActive)();
  // turns every active button off, sending the note-offs, and sends the
  // MIDI out before returning; used by "REBOOT force" / "BOOTLOADER force"
  void (*releaseAll)();
  // the one-line JSON that INFO replies with
  String (*infoJson)();
  // restart the unit / reboot into the Teensy bootloader; neither returns
  void (*reboot)();
  void (*bootloader)();
};

// ----------------------------------------------------------------------------
// CRC-32 (the same one zlib and PNG use), computed a nibble at a time

static uint32_t protocolCrcUpdate(uint32_t crc, const uint8_t *data, size_t length) {
  static const uint32_t table[16] = {
    0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
    0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
    0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
    0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
  };
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    crc = (crc >> 4) ^ table[crc & 0x0F];
    crc = (crc >> 4) ^ table[crc & 0x0F];
  }
  return crc;
}

// ----------------------------------------------------------------------------
// which paths a command may touch

// "/name.ext" or "/dir/name.ext": letters, digits, space . _ - only, at most
// one sub-directory, no "..", never the temp file. Keeping this strict means
// a mistake (or a hostile program on the other end of the cable) cannot
// reach outside the files the app is meant to manage.
static bool protocolPathAllowed(const char *path) {
  size_t length = strlen(path);
  if (length < 2 || length > 60 || path[0] != '/') {
    return false;
  }
  if (strstr(path, "..") != nullptr || strcmp(path, PROTOCOL_TEMP_PATH) == 0) {
    return false;
  }
  int slashes = 0;
  for (size_t i = 0; i < length; i++) {
    char c = path[i];
    if (c == '/') {
      slashes++;
      if (i > 0 && path[i - 1] == '/') {
        return false;
      }
      continue;
    }
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
        || c == ' ' || c == '.' || c == '_' || c == '-';
    if (!ok) {
      return false;
    }
  }
  return slashes <= 2 && path[length - 1] != '/';
}

// "/" or "/name"
static bool protocolDirAllowed(const char *path) {
  if (strcmp(path, "/") == 0) {
    return true;
  }
  size_t length = strlen(path);
  if (length < 2 || length > 40 || path[0] != '/') {
    return false;
  }
  for (size_t i = 1; i < length; i++) {
    char c = path[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
        || c == ' ' || c == '_' || c == '-';
    if (!ok) {
      return false;
    }
  }
  return true;
}

// ----------------------------------------------------------------------------
// the protocol

class SerialProtocol {
  public:
    SerialProtocol(ProtocolPort &port, ProtocolHooks hooks) : port_(port), hooks_(hooks) {}

    // Call this often (every pass of loop()). Reads whatever has arrived and
    // runs each complete command line. Does nothing, and costs almost
    // nothing, when no command is waiting. The one command that takes
    // noticeable time is an upload, which holds the loop for the few
    // milliseconds the file takes to arrive and be written.
    void poll() {
      // a cap on how much one call reads, so a flood of junk cannot starve loop()
      int budget = 512;
      while (budget-- > 0 && port_.available() > 0) {
        char c = (char)port_.read();
        if (c == '\r') {
          continue;
        }
        if (c == '\n') {
          if (overflow_) {
            error("line too long");
          } else if (length_ > 0) {
            line_[length_] = 0;
            handle(line_);
          }
          length_ = 0;
          overflow_ = false;
          continue;
        }
        if (length_ < sizeof(line_) - 1) {
          line_[length_++] = c;
        } else {
          overflow_ = true;
        }
      }
    }

  private:
    ProtocolPort &port_;
    ProtocolHooks hooks_;
    char line_[160];
    size_t length_ = 0;
    bool overflow_ = false;

    // ---- replies ----

    void send(const String &text) {
      String withNewline = text + "\n";
      port_.write((const uint8_t *)withNewline.c_str(), withNewline.length());
    }

    void error(const char *message) {
      send(String("ERR ") + message);
    }

    // ---- dispatch ----

    void handle(char *command) {
      char *argument = strchr(command, ' ');
      if (argument) {
        *argument = 0;
        argument++;
      } else {
        argument = command + strlen(command);
      }

      if (strcmp(command, "HELLO") == 0) hello();
      else if (strcmp(command, "INFO") == 0) info();
      else if (strcmp(command, "LIST") == 0) list(*argument ? argument : "/");
      else if (strcmp(command, "GET") == 0) get(argument);
      else if (strcmp(command, "PUT") == 0) put(argument);
      else if (strcmp(command, "DELETE") == 0) deleteFile(argument);
      else if (strcmp(command, "REBOOT") == 0) reboot(argument);
      else if (strcmp(command, "BOOTLOADER") == 0) bootloader(argument);
      else error("unknown command");
    }

    // ---- commands ----

    void hello() {
      send(String("OK HELLO ") + PROTOCOL_NAME + " " + String(PROTOCOL_VERSION) + " sd=" + (hooks_.sdReady() ? "1" : "0"));
    }

    void info() {
      send("OK " + hooks_.infoJson());
    }

    void list(const char *directory) {
      if (!hooks_.sdReady()) return error("no SD card");
      if (!protocolDirAllowed(directory)) return error("bad directory");
      File root = SD.open(directory);
      if (!root || !root.isDirectory()) {
        if (root) root.close();
        send("OK 0"); // a directory that does not exist is simply empty
        return;
      }
      int count = 0;
      while (true) {
        File entry = root.openNextFile();
        if (!entry) break;
        if (!entry.isDirectory() && strcmp(entry.name(), "UPLOAD.TMP") != 0) {
          send("F " + String((unsigned long)entry.size()) + " " + entry.name());
          count++;
        }
        entry.close();
      }
      root.close();
      send("OK " + String(count));
    }

    void get(const char *path) {
      if (!hooks_.sdReady()) return error("no SD card");
      if (!protocolPathAllowed(path)) return error("bad path");
      File file = SD.open(path, FILE_READ);
      if (!file || file.isDirectory()) {
        if (file) file.close();
        return error("not found");
      }
      uint32_t size = file.size();

      // first pass works out the CRC (the reply has to state it before the
      // bytes), second pass sends the bytes
      uint8_t buffer[512];
      uint32_t crc = 0xFFFFFFFF;
      int got;
      while ((got = file.read(buffer, sizeof(buffer))) > 0) {
        crc = protocolCrcUpdate(crc, buffer, got);
      }
      crc ^= 0xFFFFFFFF;
      file.seek(0);

      send("OK " + String((unsigned long)size) + " " + String((unsigned long)crc, HEX));
      uint32_t sent = 0;
      while (sent < size && (got = file.read(buffer, sizeof(buffer))) > 0) {
        port_.write(buffer, got);
        sent += got;
      }
      file.close();
      port_.flush();
    }

    // PUT <size> <crc32 hex> <path>   then <size> raw bytes
    void put(char *arguments) {
      char *sizeText = arguments;
      char *space1 = strchr(sizeText, ' ');
      if (!space1) return error("usage: PUT size crc path");
      *space1 = 0;
      char *crcText = space1 + 1;
      char *space2 = strchr(crcText, ' ');
      if (!space2) return error("usage: PUT size crc path");
      *space2 = 0;
      const char *path = space2 + 1;

      if (!hooks_.sdReady()) return error("no SD card");
      if (!protocolPathAllowed(path)) return error("bad path");
      uint32_t size = strtoul(sizeText, nullptr, 10);
      uint32_t expectedCrc = strtoul(crcText, nullptr, 16);
      if (size > PROTOCOL_MAX_FILE_SIZE) return error("file too large");

      // Write to a temporary file first and only replace the real one once
      // every byte has arrived and the CRC matches, so an interrupted or
      // corrupted upload can never damage a file that already works.
      SD.remove(PROTOCOL_TEMP_PATH);
      File file = SD.open(PROTOCOL_TEMP_PATH, FILE_WRITE);
      if (!file) return error("cannot create temp file");

      send("READY");
      port_.flush();

      uint8_t buffer[512];
      uint32_t remaining = size;
      uint32_t crc = 0xFFFFFFFF;
      unsigned long lastData = port_.now();
      bool failed = false;
      bool writeFailed = false;

      while (remaining > 0) {
        if (port_.available() <= 0) {
          if (port_.now() - lastData > PROTOCOL_STALL_TIMEOUT_MS) {
            failed = true;
            break;
          }
          continue;
        }
        size_t want = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        size_t got = port_.readAvailable(buffer, want);
        if (got == 0) continue;
        if (file.write(buffer, got) != got) {
          failed = true;
          writeFailed = true;
          break;
        }
        crc = protocolCrcUpdate(crc, buffer, got);
        remaining -= got;
        lastData = port_.now();
      }
      file.close();

      if (failed) {
        SD.remove(PROTOCOL_TEMP_PATH);
        discardIncoming();
        return error(writeFailed ? "SD write failed" : "timeout waiting for data");
      }
      if ((crc ^ 0xFFFFFFFF) != expectedCrc) {
        SD.remove(PROTOCOL_TEMP_PATH);
        return error("CRC mismatch");
      }

      makeParentDirectory(path);
      SD.remove(path);
      if (!SD.rename(PROTOCOL_TEMP_PATH, path)) {
        SD.remove(PROTOCOL_TEMP_PATH);
        return error("rename failed");
      }
      send("OK");
    }

    void deleteFile(const char *path) {
      if (!hooks_.sdReady()) return error("no SD card");
      if (!protocolPathAllowed(path)) return error("bad path");
      if (!SD.exists(path)) return error("not found");
      if (!SD.remove(path)) return error("delete failed");
      send("OK");
    }

    void reboot(const char *argument) {
      if (!readyToRestart(argument)) return;
      send("OK");
      port_.flush();
      hooks_.reboot();
    }

    void bootloader(const char *argument) {
      if (!readyToRestart(argument)) return;
      send("OK");
      port_.flush();
      hooks_.bootloader();
    }

    // A restart would never send the note-offs for anything still on, leaving
    // notes stuck on the synth. So if any button is on, refuse - unless the
    // sender said "force", in which case release everything cleanly first.
    // Returns whether it is now fine to restart.
    bool readyToRestart(const char *argument) {
      if (!hooks_.notesActive()) {
        return true;
      }
      if (strcmp(argument, "force") != 0) {
        error("busy: buttons are active (send \"force\" to release them and restart)");
        return false;
      }
      hooks_.releaseAll();
      return true;
    }

    // ---- helpers ----

    // For "/configs/x.json" makes sure "/configs" exists.
    static void makeParentDirectory(const char *path) {
      const char *lastSlash = strrchr(path, '/');
      if (lastSlash == nullptr || lastSlash == path) return;
      char directory[64];
      size_t length = lastSlash - path;
      memcpy(directory, path, length);
      directory[length] = 0;
      SD.mkdir(directory);
    }

    // After a failed upload the sender may still be sending the file: read and
    // throw away until the line has been quiet for a moment, so the leftover
    // bytes are not mistaken for the start of the next command.
    void discardIncoming() {
      unsigned long quietSince = port_.now();
      while (port_.now() - quietSince < 50) {
        if (port_.available() > 0) {
          port_.read();
          quietSince = port_.now();
        }
      }
    }
};

#endif
