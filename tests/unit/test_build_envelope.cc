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

TEST_F(BuildEnvelopeTest, JuniperUpdateDecodesJsonAndNativeScalarValues) {
    DataManipulation dm;
    juniper_gnmi::SubscribeResponse response;
    auto *notification = response.mutable_update();
    notification->set_timestamp(123456789);
    notification->mutable_prefix()->add_elem()->set_name("interfaces");

    auto add_update = [notification](const std::string &name) {
        auto *update = notification->add_update();
        update->mutable_path()->add_elem()->set_name(name);
        return update;
    };

    add_update("json-string")->mutable_val()->set_json_val("\"up\"");
    add_update("json-number")->mutable_val()->set_json_ietf_val("42");
    add_update("string")->mutable_val()->set_string_val("active");
    add_update("signed")->mutable_val()->set_int_val(-7);
    add_update("unsigned")->mutable_val()->set_uint_val(99);
    add_update("boolean")->mutable_val()->set_bool_val(true);
    add_update("float")->mutable_val()->set_float_val(2.5F);
    add_update("ascii")->mutable_val()->set_ascii_val("ready");

    Json::Value telemetry;
    std::string encoded;
    ASSERT_TRUE(dm.JuniperUpdate(response, encoded, telemetry));

    const Json::Value root = ParseEnvelope(encoded);
    EXPECT_EQ(root["notification_timestamp"].asUInt64(), 123456789U);
    EXPECT_EQ(root["/json-string"].asString(), "up");
    EXPECT_EQ(root["/json-number"].asInt(), 42);
    EXPECT_EQ(root["/string"].asString(), "active");
    EXPECT_EQ(root["/signed"].asInt64(), -7);
    EXPECT_EQ(root["/unsigned"].asUInt64(), 99U);
    EXPECT_TRUE(root["/boolean"].asBool());
    EXPECT_FLOAT_EQ(root["/float"].asFloat(), 2.5F);
    EXPECT_EQ(root["/ascii"].asString(), "ready");
}

TEST_F(BuildEnvelopeTest, JuniperUpdateDecodesDecimalAndLeafListValues) {
    DataManipulation dm;
    juniper_gnmi::SubscribeResponse response;
    auto *notification = response.mutable_update();

    auto *decimal_update = notification->add_update();
    decimal_update->mutable_path()->add_elem()->set_name("temperature");
    auto *decimal = decimal_update->mutable_val()->mutable_decimal_val();
    decimal->set_digits(25367);
    decimal->set_precision(3);

    auto *list_update = notification->add_update();
    list_update->mutable_path()->add_elem()->set_name("values");
    auto *list = list_update->mutable_val()->mutable_leaflist_val();
    list->add_element()->set_string_val("one");
    list->add_element()->set_uint_val(2);
    list->add_element()->set_bool_val(true);

    Json::Value telemetry;
    std::string encoded;
    ASSERT_TRUE(dm.JuniperUpdate(response, encoded, telemetry));

    const Json::Value root = ParseEnvelope(encoded);
    EXPECT_DOUBLE_EQ(root["/temperature"].asDouble(), 25.367);
    ASSERT_TRUE(root["/values"].isArray());
    ASSERT_EQ(root["/values"].size(), 3U);
    EXPECT_EQ(root["/values"][0].asString(), "one");
    EXPECT_EQ(root["/values"][1].asUInt(), 2U);
    EXPECT_TRUE(root["/values"][2].asBool());
}

TEST_F(BuildEnvelopeTest, JuniperUpdateRejectsMessagesWithoutMetricUpdates) {
    DataManipulation dm;
    juniper_gnmi::SubscribeResponse response;
    response.set_sync_response(true);

    Json::Value telemetry;
    std::string encoded;
    EXPECT_FALSE(dm.JuniperUpdate(response, encoded, telemetry));
    EXPECT_TRUE(encoded.empty());
}

}  // namespace
