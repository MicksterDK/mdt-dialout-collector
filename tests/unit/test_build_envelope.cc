#include "cfg_test_common.h"
#include "dataManipulation/data_manipulation.h"

namespace {

class BuildEnvelopeTest : public ::testing::Test {
protected:
    void SetUp() override {
        mdt_test::EnsureNullLoggers();
        data_manipulation_cfg_parameters.clear();
        data_manipulation_cfg_parameters["telemetry_data_format"] = "string";
    }

    void TearDown() override {
        data_manipulation_cfg_parameters.clear();
    }

    static Json::Value ParseEnvelope(const std::string &json) {
        Json::CharReaderBuilder builder;
        Json::Value root;
        std::string errors;
        const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        EXPECT_TRUE(reader->parse(json.data(), json.data() + json.size(),
            &root, &errors)) << errors;
        return root;
    }
};

TEST_F(BuildEnvelopeTest, LegacyStringModeKeepsTelemetryAsString) {
    DataManipulation dm;
    std::string envelope;
    const std::string payload = R"({"system_id":"router1","cpu":5})";

    ASSERT_TRUE(dm.BuildEnvelope(payload, "192.0.2.1", "50051", nullptr,
        "writer-a", envelope));

    const Json::Value root = ParseEnvelope(envelope);
    EXPECT_EQ(root["event_type"].asString(), "gRPC");
    EXPECT_EQ(root["serialization"].asString(), "json_string");
    EXPECT_TRUE(root["telemetry_data"].isString());
    EXPECT_EQ(root["telemetry_data"].asString(), payload);
    EXPECT_EQ(root["writer_id"].asString(), "writer-a");
    EXPECT_EQ(root["telemetry_node"].asString(), "192.0.2.1");
    EXPECT_EQ(root["telemetry_port"].asUInt(), 50051U);
}

TEST_F(BuildEnvelopeTest, ObjectModeEmbedsTelemetryAsObject) {
    data_manipulation_cfg_parameters["telemetry_data_format"] = "object";

    DataManipulation dm;
    std::string envelope;
    const std::string payload = R"({"system_id":"router1","cpu":5})";

    ASSERT_TRUE(dm.BuildEnvelope(payload, "192.0.2.1", "50051", nullptr,
        "writer-a", envelope));

    const Json::Value root = ParseEnvelope(envelope);
    EXPECT_EQ(root["serialization"].asString(), "json_object");
    EXPECT_TRUE(root["telemetry_data"].isObject());
    EXPECT_EQ(root["telemetry_data"]["system_id"].asString(), "router1");
    EXPECT_EQ(root["telemetry_data"]["cpu"].asInt(), 5);
}

TEST_F(BuildEnvelopeTest, ObjectModeFallsBackToStringOnMalformedJson) {
    data_manipulation_cfg_parameters["telemetry_data_format"] = "object";

    DataManipulation dm;
    std::string envelope;
    const std::string payload = R"({"system_id":"router1","cpu":)";

    ASSERT_TRUE(dm.BuildEnvelope(payload, "192.0.2.1", "50051", nullptr,
        "writer-a", envelope));

    const Json::Value root = ParseEnvelope(envelope);
    EXPECT_EQ(root["serialization"].asString(), "json_string");
    EXPECT_TRUE(root["telemetry_data"].isString());
    EXPECT_EQ(root["telemetry_data"].asString(), payload);
}

}  // namespace
