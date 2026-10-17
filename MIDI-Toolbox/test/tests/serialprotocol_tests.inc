//----------------------------------------------------------------------------//
//    Tests for SerialProtocol.h - the USB-serial file-transfer protocol.     //
//    The protocol runs against an in-memory ProtocolPort, with a temporary   //
//    directory standing in for the SD card (as the ConfigStore tests do).    //
//----------------------------------------------------------------------------//

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

// A fake USB serial port: `input` is what the computer has sent, `output` is
// everything the unit has replied. Every call to now() moves a fake clock on
// by one millisecond, so a stalled upload times out instantly in test time.
class MemoryPort : public ProtocolPort {
  public:
    std::string input;
    size_t readPosition = 0;
    std::string output;
    unsigned long clock = 0;

    int available() override { return (int)(input.size() - readPosition); }
    int read() override { return (unsigned char)input[readPosition++]; }
    size_t readAvailable(uint8_t *buffer, size_t max) override {
      size_t count = std::min(max, input.size() - readPosition);
      memcpy(buffer, input.data() + readPosition, count);
      readPosition += count;
      return count;
    }
    size_t write(const uint8_t *buffer, size_t length) override {
      output.append((const char *)buffer, length);
      return length;
    }
    void flush() override {}
    unsigned long now() override { return clock++; }
};

// what the hooks report, and how often the hardware actions were asked for
static bool protocolTestSdReady = true;
static bool protocolTestNotesActive = false;
static int protocolTestReboots = 0;
static int protocolTestBootloaders = 0;
static int protocolTestReleases = 0;
// the order the hooks ran in, e.g. "release,reboot,"
static std::string protocolTestOrder;

static ProtocolHooks protocolTestHooks() {
  ProtocolHooks hooks;
  hooks.sdReady = []() { return protocolTestSdReady; };
  hooks.notesActive = []() { return protocolTestNotesActive; };
  hooks.infoJson = []() { return String("{\"firmware\":\"test\"}"); };
  hooks.releaseAll = []() { protocolTestReleases++; protocolTestOrder += "release,"; };
  hooks.reboot = []() { protocolTestReboots++; protocolTestOrder += "reboot,"; };
  hooks.bootloader = []() { protocolTestBootloaders++; protocolTestOrder += "bootloader,"; };
  return hooks;
}

static std::string crcHex(const std::string &data) {
  uint32_t crc = protocolCrcUpdate(0xFFFFFFFF, (const uint8_t *)data.data(), data.size()) ^ 0xFFFFFFFF;
  char text[16];
  snprintf(text, sizeof(text), "%x", (unsigned)crc);
  return text;
}

static std::string putCommand(const std::string &path, const std::string &data) {
  return "PUT " + std::to_string(data.size()) + " " + crcHex(data) + " " + path + "\n" + data;
}

class SerialProtocolTest : public ::testing::Test {
  protected:
    std::filesystem::path testDir;
    MemoryPort port;
    SerialProtocol *protocol = nullptr;

    void SetUp() override {
      protocolTestSdReady = true;
      protocolTestNotesActive = false;
      protocolTestReboots = 0;
      protocolTestBootloaders = 0;
      protocolTestReleases = 0;
      protocolTestOrder.clear();
      std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
      testDir = std::filesystem::temp_directory_path() / ("miditoolbox_proto_test_" + testName);
      std::filesystem::remove_all(testDir);
      std::filesystem::create_directories(testDir / "configs");
      previousCwd = std::filesystem::current_path();
      std::filesystem::current_path(testDir);
      protocol = new SerialProtocol(port, protocolTestHooks());
    }

    void TearDown() override {
      delete protocol;
      std::filesystem::current_path(previousCwd);
      std::filesystem::remove_all(testDir);
    }

    // sends `text` to the unit, lets it run, and returns everything it replied
    std::string send(const std::string &text) {
      port.input += text;
      while (port.available() > 0) {
        protocol->poll();
      }
      std::string reply = port.output;
      port.output.clear();
      return reply;
    }

    void writeFile(const std::string &path, const std::string &content) {
      std::filesystem::create_directories(std::filesystem::path(path).parent_path().empty() ? "." : std::filesystem::path(path).parent_path());
      std::ofstream out(path, std::ios::binary);
      out << content;
    }

    std::string readFile(const std::string &path) {
      std::ifstream in(path, std::ios::binary);
      std::stringstream text;
      text << in.rdbuf();
      return text.str();
    }

    bool fileExists(const std::string &path) { return std::filesystem::exists(path); }

  private:
    std::filesystem::path previousCwd;
};

// ---- the simple commands ----

TEST_F(SerialProtocolTest, HelloReportsTheProtocolAndTheSdCard) {
  EXPECT_EQ(send("HELLO\n"), "OK HELLO MidiToolbox 1 sd=1\n");
  protocolTestSdReady = false;
  EXPECT_EQ(send("HELLO\n"), "OK HELLO MidiToolbox 1 sd=0\n");
}

TEST_F(SerialProtocolTest, InfoRepliesWithTheFirmwaresJson) {
  EXPECT_EQ(send("INFO\n"), "OK {\"firmware\":\"test\"}\n");
}

TEST_F(SerialProtocolTest, UnknownCommandsAreRefused) {
  EXPECT_EQ(send("FORMAT\n"), "ERR unknown command\n");
}

TEST_F(SerialProtocolTest, CommandsMayArriveTogetherOrInPieces) {
  EXPECT_EQ(send("HELLO\nINFO\n"), "OK HELLO MidiToolbox 1 sd=1\nOK {\"firmware\":\"test\"}\n");
  EXPECT_EQ(send("HEL"), "");
  EXPECT_EQ(send("LO\r\n"), "OK HELLO MidiToolbox 1 sd=1\n");
}

TEST_F(SerialProtocolTest, ATooLongLineIsRefusedAndTheNextCommandStillWorks) {
  EXPECT_EQ(send(std::string(500, 'x') + "\n"), "ERR line too long\n");
  EXPECT_EQ(send("HELLO\n"), "OK HELLO MidiToolbox 1 sd=1\n");
}

TEST_F(SerialProtocolTest, WithoutAnSdCardTheFileCommandsSayWhy) {
  protocolTestSdReady = false;
  EXPECT_EQ(send("LIST /\n"), "ERR no SD card\n");
  EXPECT_EQ(send("GET /a.json\n"), "ERR no SD card\n");
  EXPECT_EQ(send("DELETE /a.json\n"), "ERR no SD card\n");
  EXPECT_EQ(send(putCommand("/a.json", "{}")), "ERR no SD card\n");
}

// ---- LIST ----

TEST_F(SerialProtocolTest, ListShowsFilesWithSpacesAndSkipsDirectoriesAndTheTempFile) {
  writeFile("configs/Live Rig.json", "{\"a\":1}");
  writeFile("configs/Practice.json", "{}");
  std::filesystem::create_directories("configs/sub");
  writeFile("configs/UPLOAD.TMP", "half an upload");

  std::string reply = send("LIST /configs\n");

  EXPECT_NE(reply.find("F 7 Live Rig.json\n"), std::string::npos) << reply;
  EXPECT_NE(reply.find("F 2 Practice.json\n"), std::string::npos) << reply;
  EXPECT_EQ(reply.find("sub"), std::string::npos) << reply;
  EXPECT_EQ(reply.find("UPLOAD"), std::string::npos) << reply;
  EXPECT_NE(reply.find("OK 2\n"), std::string::npos) << reply;
}

TEST_F(SerialProtocolTest, ListOfTheRootAndOfAMissingDirectory) {
  writeFile("device.json", "{}");
  EXPECT_EQ(send("LIST /\n"), "F 2 device.json\nOK 1\n");
  EXPECT_EQ(send("LIST /nothing\n"), "OK 0\n");
  EXPECT_EQ(send("LIST /a/b\n"), "ERR bad directory\n");
  EXPECT_EQ(send("LIST ..\n"), "ERR bad directory\n");
}

// ---- GET ----

TEST_F(SerialProtocolTest, GetSendsTheSizeCrcAndBytes) {
  writeFile("configs/Live Rig.json", "{\"a\":1}");
  EXPECT_EQ(send("GET /configs/Live Rig.json\n"), "OK 7 " + crcHex("{\"a\":1}") + "\n{\"a\":1}");
}

TEST_F(SerialProtocolTest, GetRefusesMissingFilesAndBadPaths) {
  EXPECT_EQ(send("GET /configs/none.json\n"), "ERR not found\n");
  EXPECT_EQ(send("GET /configs\n"), "ERR not found\n");
  EXPECT_EQ(send("GET /../secret\n"), "ERR bad path\n");
  EXPECT_EQ(send("GET configs/a.json\n"), "ERR bad path\n");
  EXPECT_EQ(send("GET /UPLOAD.TMP\n"), "ERR bad path\n");
}

// ---- PUT ----

TEST_F(SerialProtocolTest, PutStoresTheFileAndAnswersReadyThenOk) {
  EXPECT_EQ(send(putCommand("/configs/My Rig.json", "{\"hello\":true}")), "READY\nOK\n");
  EXPECT_EQ(readFile("configs/My Rig.json"), "{\"hello\":true}");
  EXPECT_FALSE(fileExists("UPLOAD.TMP"));
}

TEST_F(SerialProtocolTest, PutReplacesAFileThatAlreadyExists) {
  writeFile("configs/a.json", "old contents that are longer");
  EXPECT_EQ(send(putCommand("/configs/a.json", "new")), "READY\nOK\n");
  EXPECT_EQ(readFile("configs/a.json"), "new");
}

TEST_F(SerialProtocolTest, PutCreatesTheDirectoryIfItIsMissing) {
  std::filesystem::remove_all("configs");
  EXPECT_EQ(send(putCommand("/configs/a.json", "{}")), "READY\nOK\n");
  EXPECT_TRUE(fileExists("configs/a.json"));
}

TEST_F(SerialProtocolTest, PutCarriesAnyBytesAcrossSeveralBuffers) {
  std::string data(5000, '\0');
  for (size_t i = 0; i < data.size(); i++) data[i] = (char)((i * 31) & 0xff);
  EXPECT_EQ(send(putCommand("/blob.bin", data)), "READY\nOK\n");
  EXPECT_EQ(readFile("blob.bin"), data);
}

TEST_F(SerialProtocolTest, PutWithTheWrongCrcLeavesTheExistingFileAlone) {
  writeFile("configs/a.json", "keep me");
  std::string reply = send("PUT 3 deadbeef /configs/a.json\nnew");
  EXPECT_EQ(reply, "READY\nERR CRC mismatch\n");
  EXPECT_EQ(readFile("configs/a.json"), "keep me");
  EXPECT_FALSE(fileExists("UPLOAD.TMP"));
}

TEST_F(SerialProtocolTest, AnUploadThatStopsPartWayTimesOutAndLeavesTheExistingFileAlone) {
  writeFile("configs/a.json", "keep me");
  std::string reply = send("PUT 100 " + crcHex(std::string(100, 'x')) + " /configs/a.json\n" + std::string(10, 'x'));
  EXPECT_EQ(reply, "READY\nERR timeout waiting for data\n");
  EXPECT_EQ(readFile("configs/a.json"), "keep me");
  EXPECT_FALSE(fileExists("UPLOAD.TMP"));
  // and the unit carries on with the next command
  EXPECT_EQ(send("HELLO\n"), "OK HELLO MidiToolbox 1 sd=1\n");
}

TEST_F(SerialProtocolTest, PutRefusesBadPathsTooBigFilesAndBadUsage) {
  EXPECT_EQ(send("PUT 2 abc ../x.json\n"), "ERR bad path\n");
  EXPECT_EQ(send("PUT 2 abc /a/b/c.json\n"), "ERR bad path\n");
  EXPECT_EQ(send("PUT 2 abc /UPLOAD.TMP\n"), "ERR bad path\n");
  EXPECT_EQ(send("PUT 999999999 abc /big.bin\n"), "ERR file too large\n");
  EXPECT_EQ(send("PUT 2\n"), "ERR usage: PUT size crc path\n");
  EXPECT_EQ(send("PUT 2 abc\n"), "ERR usage: PUT size crc path\n");
}

TEST_F(SerialProtocolTest, AnUploadedDeviceJsonIsAcceptedByTheFirmwareLoader) {
  // the whole chain: the app uploads the Designer's file, the firmware reads it
  std::string json = liveRigDeviceJson();
  EXPECT_EQ(send(putCommand("/device.json", json)), "READY\nOK\n");
  DeviceProfile profile = deviceProfileFromHeader();
  String error;
  EXPECT_EQ(loadDeviceProfile(profile, error), DeviceLoadResult::Loaded) << error.c_str();
}

// ---- DELETE ----

TEST_F(SerialProtocolTest, DeleteRemovesAFile) {
  writeFile("configs/a.json", "{}");
  EXPECT_EQ(send("DELETE /configs/a.json\n"), "OK\n");
  EXPECT_FALSE(fileExists("configs/a.json"));
}

TEST_F(SerialProtocolTest, DeleteRefusesMissingFilesAndBadPaths) {
  EXPECT_EQ(send("DELETE /configs/none.json\n"), "ERR not found\n");
  EXPECT_EQ(send("DELETE /../x\n"), "ERR bad path\n");
}

// ---- REBOOT and BOOTLOADER ----

TEST_F(SerialProtocolTest, RebootAndBootloaderRunTheirHooksWhenNothingIsPlaying) {
  EXPECT_EQ(send("REBOOT\n"), "OK\n");
  EXPECT_EQ(protocolTestReboots, 1);
  EXPECT_EQ(send("BOOTLOADER\n"), "OK\n");
  EXPECT_EQ(protocolTestBootloaders, 1);
}

TEST_F(SerialProtocolTest, RebootAndBootloaderAreRefusedWhileButtonsAreActive) {
  // restarting now would leave notes stuck on the synth
  protocolTestNotesActive = true;
  const std::string busy = "ERR busy: buttons are active (send \"force\" to release them and restart)\n";
  EXPECT_EQ(send("REBOOT\n"), busy);
  EXPECT_EQ(send("BOOTLOADER\n"), busy);
  EXPECT_EQ(protocolTestReboots, 0);
  EXPECT_EQ(protocolTestBootloaders, 0);
  EXPECT_EQ(protocolTestReleases, 0);
}

TEST_F(SerialProtocolTest, ForceReleasesTheActiveButtonsBeforeRestarting) {
  protocolTestNotesActive = true;
  EXPECT_EQ(send("REBOOT force\n"), "OK\n");
  EXPECT_EQ(protocolTestOrder, "release,reboot,");

  protocolTestOrder.clear();
  EXPECT_EQ(send("BOOTLOADER force\n"), "OK\n");
  EXPECT_EQ(protocolTestOrder, "release,bootloader,");
}

TEST_F(SerialProtocolTest, ForceWithNothingActiveJustRestarts) {
  EXPECT_EQ(send("REBOOT force\n"), "OK\n");
  EXPECT_EQ(protocolTestReleases, 0);
  EXPECT_EQ(protocolTestReboots, 1);
}

TEST_F(SerialProtocolTest, AnyOtherArgumentDoesNotForce) {
  protocolTestNotesActive = true;
  EXPECT_NE(send("REBOOT please\n").find("ERR busy"), std::string::npos);
  EXPECT_EQ(protocolTestReboots, 0);
}
