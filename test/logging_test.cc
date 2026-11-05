#include "logging.hpp"
#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#endif
#include <iterator>
#include <set>
#include <stdexcept>

#if defined(info) || defined(error) || defined(debug) || defined(INFO) || defined(ERROR)
#error "The framework adapter must not export popolog's generic macros"
#endif

popolog::Logger *LoggerFromOtherTranslationUnit();
static int failed = 0;

static void Check(bool condition, const char *message) {
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << message << '\n';
    if (!condition) ++failed;
}

static std::string Read(const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

static popolog::Logger::ptr FileLogger(const std::string &path, bool async) {
    popolog::LocalLoggerBuilder builder;
    builder.buildLoggerName("test");
    builder.buildFormatter("%m%n");
    builder.buildLoggerType(async ? popolog::LoggerType::LOGGER_ASYNC
                                 : popolog::LoggerType::LOGGER_SYNC);
    builder.buildSink<popolog::FileSink>(path);
    return builder.build();
}

static void TestAdapter() {
    loopforge::LogConfig config;
    config.level = loopforge::LogLevel::WARN;
    config.asynchronous = false;
    config.file_prefix.clear();
    loopforge::InitLogging(config);
    std::ostringstream capture;
    auto original = std::cout.rdbuf(capture.rdbuf());
    int evaluated = 0;
    DBG_LOG("hidden-debug %d", ++evaluated);
    INF_LOG("hidden-info");
    errno = EAGAIN;
    WRN_LOG("visible-warning %d", 42);
    bool preserved_errno = errno == EAGAIN;
    ERR_LOG("visible-error");
    FTL_LOG("visible-fatal");
    auto peer = LoggerFromOtherTranslationUnit();
    loopforge::FlushLogging();
    std::cout.rdbuf(original);
    const std::string output = capture.str();
    Check(evaluated == 0 && output.find("hidden-") == std::string::npos,
          "level filtering skips message argument evaluation");
    Check(output.find("[WARN]") != std::string::npos &&
          output.find("visible-warning 42") != std::string::npos &&
          output.find("[ERROR]") != std::string::npos &&
          output.find("[FATAL]") != std::string::npos &&
          output.find("logging_test.cc:") != std::string::npos,
          "legacy macros preserve levels, printf arguments and call-site locations");
    Check(preserved_errno, "logging preserves errno");
    Check(peer == loopforge::GetLogger().get(), "one logger is shared across translation units");

    auto previous = loopforge::GetLogger();
    config.console = false;
    bool rejected = false;
    try { loopforge::InitLogging(config); }
    catch (const std::invalid_argument &) { rejected = true; }
    Check(rejected && previous == loopforge::GetLogger(),
          "invalid configuration preserves the current logger");
}

static void TestConcurrentFile(const std::string &directory) {
    const std::string path = directory + "/concurrent.log";
    auto logger = FileLogger(path, true);
    std::vector<std::thread> threads;
    for (int t = 0; t < 6; ++t) {
        threads.emplace_back([logger, t] {
            for (int i = 0; i < 2000; ++i)
                logger->info(__FILE__, __LINE__, "record-%d-%d", t, i);
        });
    }
    for (auto &thread : threads) thread.join();
    logger->flush();
    std::istringstream records(Read(path));
    std::set<std::string> unique;
    size_t count = 0;
    std::string line;
    while (std::getline(records, line)) { ++count; unique.insert(line); }
    bool complete = count == 12000 && unique.size() == 12000;
    for (int t = 0; t < 6; ++t)
        for (int i = 0; i < 2000; ++i)
            complete = complete && unique.count("record-" + std::to_string(t) + "-" + std::to_string(i));
    Check(complete, "six concurrent producers write all 12000 distinct records exactly once");

    const std::string tail_path = directory + "/shutdown.log";
    {
        auto tail = FileLogger(tail_path, true);
        for (int i = 0; i < 2000; ++i) tail->info(__FILE__, __LINE__, "tail-%d", i);
    }
    Check(Read(tail_path).find("tail-1999\n") != std::string::npos,
          "logger destruction drains pending records without explicit flush");
}

static void TestLargeRecord(const std::string &directory) {
    const std::string path = directory + "/large.log";
    auto logger = FileLogger(path, true);
    const std::string payload(25 * 1024 * 1024, 'X');
    logger->info(__FILE__, __LINE__, "%s", payload.c_str());
    logger->flush();
    Check(Read(path) == payload + "\n", "a 25 MiB record grows the buffer and does not hang or truncate");
}

static void TestRolling(const std::string &directory) {
    loopforge::LogConfig config;
    config.console = false;
    config.file_prefix = directory + "/roll-";
    config.max_file_size = 128;
    config.level = loopforge::LogLevel::INFO;
    loopforge::InitLogging(config);
    for (int i = 0; i < 8; ++i) {
        INF_LOG("rotation-record-%d-abcdefghijklmnopqrstuvwxyz", i);
        loopforge::FlushLogging();
    }
    loopforge::ShutdownLogging();
    size_t files = 0;
    std::string contents;
#ifdef _WIN32
    struct _finddata_t entry;
    intptr_t handle = _findfirst((directory + "/roll-*").c_str(), &entry);
    if (handle != -1) {
        do {
            ++files;
            contents += Read(directory + "/" + entry.name);
        } while (_findnext(handle, &entry) == 0);
        _findclose(handle);
    }
#else
    DIR *dir = opendir(directory.c_str());
    if (dir) {
        while (auto entry = readdir(dir)) {
            if (std::string(entry->d_name).find("roll-") == 0) {
                ++files;
                contents += Read(directory + "/" + entry->d_name);
            }
        }
        closedir(dir);
    }
#endif
    bool complete = files > 1;
    for (int i = 0; i < 8; ++i)
        complete = complete && contents.find("rotation-record-" + std::to_string(i)) != std::string::npos;
    Check(complete, "adapter writes all records across rolling files and shuts down cleanly");
}

static void TestFailures(const std::string &directory) {
    std::string blocker = directory + "/not-a-directory";
    { std::ofstream file(blocker); file << "blocker"; }
    auto previous = loopforge::GetLogger();
    loopforge::LogConfig config;
    config.file_prefix = blocker + "/fail-";
    bool rejected = false;
    try { loopforge::InitLogging(config); }
    catch (const std::runtime_error &) { rejected = true; }
    Check(rejected && previous == loopforge::GetLogger(),
          "unwritable log destination is reported even in Release mode");
    popolog::AsyncLooper worker([](popolog::Buffer &) { throw std::runtime_error("sink failed"); });
    worker.push("data", 4);
    rejected = false;
    try { worker.flush(); } catch (const std::runtime_error &) { rejected = true; }
    worker.stop();
    worker.stop();
    Check(rejected, "background sink failure reaches flush and repeated stop is safe");
    popolog::Formatter formatter("%m-tail");
    popolog::LogMsg message(popolog::LogLevel::value::INFO, 1, "file", "test", "body");
    Check(formatter.format(message) == "body-tail", "formatter parses patterns in Release and preserves literal suffixes");
}

int main(int argc, char **argv) {
    const std::string base = argc > 1 ? argv[1] : ".logging-test-output";
    const std::string directory = base + "/run-" + std::to_string(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    popolog::util::File::createDirectory(directory);
    try {
        TestAdapter();
        TestConcurrentFile(directory);
        TestLargeRecord(directory);
        TestRolling(directory);
        TestFailures(directory);
        loopforge::ShutdownLogging();
    } catch (const std::exception &error) {
        std::cerr << "Unexpected failure: " << error.what() << '\n';
        return 2;
    }
    return failed ? 1 : 0;
}
