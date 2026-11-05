#include "logging.hpp"

popolog::Logger *LoggerFromOtherTranslationUnit() {
    INF_LOG("peer translation unit");
    return loopforge::GetLogger().get();
}
