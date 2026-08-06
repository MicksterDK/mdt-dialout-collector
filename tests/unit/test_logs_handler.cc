// Copyright(c) 2022-2025, Salvatore Cuzzilla (Swisscom AG)
// Copyright(c) 2026-present, Salvatore Cuzzilla (Avaloq, an NEC Company)
// Distributed under the MIT License (http://opensource.org/licenses/MIT)


#include "utils/logs_handler.h"
#include "utils/cfg_handler.h"

#include <gtest/gtest.h>
#include <filesystem>
#include <spdlog/spdlog.h>


namespace {

// Each case starts from a clean spdlog registry so we control which
// loggers exist when LogsHandler runs.
class LogsHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        spdlog::drop("multi-logger");
        spdlog::drop("multi-logger-boot");
    }
    void TearDown() override {
        spdlog::drop("multi-logger");
        spdlog::drop("multi-logger-boot");
    }
};

TEST_F(LogsHandlerTest, ConstructorRegistersBootLogger) {
    LogsHandler h;
    EXPECT_NE(spdlog::get("multi-logger-boot"), nullptr);
}

TEST_F(LogsHandlerTest, SetSpdlogSinksRegistersMultiLogger) {
    LogsHandler h;
    // Minimum config the post-cfg sink setup needs.
    logs_cfg_parameters["syslog"]          = "false";
    logs_cfg_parameters["syslog_facility"] = "NONE";
    logs_cfg_parameters["console_log"]     = "true";
    logs_cfg_parameters["spdlog_level"]    = "info";
    logs_cfg_parameters["rotating_file_log"] = "false";
    EXPECT_TRUE(h.set_spdlog_sinks());
    EXPECT_NE(spdlog::get("multi-logger"), nullptr);
}

TEST_F(LogsHandlerTest, DestructorIsNullSafeWithoutMultiLogger) {
    // Construct then destruct without ever calling set_spdlog_sinks().
    // Dtor must NOT segfault when multi-logger is unregistered. If the
    // null-safe fallback is broken, this test crashes the process.
    {
        LogsHandler h;
        EXPECT_EQ(spdlog::get("multi-logger"), nullptr);
    }
    SUCCEED();
}

TEST_F(LogsHandlerTest, RotatingFileCanCaptureDebugIndependently) {
    const std::string path = "/tmp/mdt-dialout-collector-log-test.log";
    std::filesystem::remove(path);
    LogsHandler h;
    logs_cfg_parameters["syslog"] = "false";
    logs_cfg_parameters["syslog_facility"] = "NONE";
    logs_cfg_parameters["console_log"] = "false";
    logs_cfg_parameters["spdlog_level"] = "warn";
    logs_cfg_parameters["rotating_file_log"] = "true";
    logs_cfg_parameters["rotating_file_path"] = path;
    logs_cfg_parameters["rotating_file_max_size_mb"] = "1";
    logs_cfg_parameters["rotating_file_max_files"] = "2";
    logs_cfg_parameters["rotating_file_level"] = "debug";

    ASSERT_TRUE(h.set_spdlog_sinks());
    auto logger = spdlog::get("multi-logger");
    ASSERT_NE(logger, nullptr);
    EXPECT_TRUE(logger->should_log(spdlog::level::debug));
    logger->debug("rotating debug test");
    logger->flush();
    EXPECT_TRUE(std::filesystem::exists(path));
    std::filesystem::remove(path);
}

}  // namespace
