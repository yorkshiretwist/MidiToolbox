#pragma once
#include <cstdint>
#include <vector>

// desktop stand-in for the Teensyduino EEPROM library - backed by an
// in-memory buffer rather than real persistent storage, just for
// syntax/behaviour checking outside the real hardware.
class EEPROMClass {
  public:
    EEPROMClass() : data_(4096, 0xFF) {}
    uint8_t read(int address) {
      if (address < 0 || address >= (int)data_.size()) return 0xFF;
      return data_[address];
    }
    void write(int address, uint8_t value) {
      if (address < 0 || address >= (int)data_.size()) return;
      data_[address] = value;
    }
    void update(int address, uint8_t value) {
      if (address < 0 || address >= (int)data_.size()) return;
      if (data_[address] != value) {
        data_[address] = value;
      }
    }
    int length() { return (int)data_.size(); }
    // not part of the real EEPROM API - lets tests start each case with a
    // guaranteed-blank chip, instead of leftover state from a previous test
    void reset() {
      for (size_t i = 0; i < data_.size(); i++) {
        data_[i] = 0xFF;
      }
    }
  private:
    std::vector<uint8_t> data_;
};
extern EEPROMClass EEPROM;
