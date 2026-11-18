#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "scenario_parser.h"

namespace {

using namespace std::chrono_literals;

constexpr std::string_view kHeader = R"(name: test_scenario
seed: 42
timeout: 10s
record: [/odom, /scan]
)";

// Scenario with a single injection on /odom holding the given fault(s).
// @p faults is the inside of a flow-style YAML sequence.
std::string scenario_with_faults(const std::string& faults) {
  return std::string(kHeader) + "injections:\n  - topic: /odom\n    faults: [" +
         faults + "]\n";
}

// Flow-style YAML for one fault. @p window is the inside of the window map and
// is omitted when empty.
std::string make_fault(const std::string& type, const std::string& params,
                       const std::string& window = "") {
  std::string fault = "{id: f1, type: " + type + ", params: {" + params + "}";
  if (!window.empty()) fault += ", window: {" + window + "}";
  return fault + "}";
}

std::string scenario_with_fault(const std::string& type,
                                const std::string& params,
                                const std::string& window = "") {
  return scenario_with_faults(make_fault(type, params, window));
}

const Fault& first_fault(const Scenario& scenario) {
  return scenario.injections.at(0).faults.at(0);
}

// Writes scenario text to a per-test temporary directory, since the parser
// only reads from files.
class ScenarioParserTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto templ =
        (std::filesystem::temp_directory_path() / "scenario_XXXXXX").string();
    ASSERT_NE(mkdtemp(templ.data()), nullptr);
    dir_ = templ;
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(dir_, ec);
  }

  std::expected<Scenario, std::string> parse(const std::string& yaml) {
    const auto path = dir_ / "scenario.yaml";
    std::ofstream(path) << yaml;
    return parse_scenario(path.string());
  }

  bool parses(const std::string& yaml) { return parse(yaml).has_value(); }

  std::filesystem::path dir_;
};

// ---------------------------------------------------------------------------
// File level
// ---------------------------------------------------------------------------

TEST_F(ScenarioParserTest, MissingFileIsAnError) {
  EXPECT_FALSE(parse_scenario((dir_ / "does_not_exist.yaml").string()));
}

TEST_F(ScenarioParserTest, EmptyFileIsAnError) { EXPECT_FALSE(parses("")); }

TEST_F(ScenarioParserTest, MalformedYamlIsAnError) {
  EXPECT_FALSE(parses("name: [unterminated\nseed: 1\n"));
}

TEST_F(ScenarioParserTest, RootThatIsNotAMapIsAnError) {
  EXPECT_FALSE(parses("- a\n- b\n"));
  EXPECT_FALSE(parses("just a string\n"));
}

// ---------------------------------------------------------------------------
// Top-level keys
// ---------------------------------------------------------------------------

TEST_F(ScenarioParserTest, ParsesMinimalScenario) {
  const auto result =
      parse(scenario_with_fault("delay", "mean: 200ms, jitter: 20ms"));
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_EQ(result->name, "test_scenario");
  EXPECT_EQ(result->seed, 42u);
  EXPECT_EQ(result->timeout, 10s);
  EXPECT_EQ(result->record, (RecordList{"/odom", "/scan"}));
  ASSERT_EQ(result->injections.size(), 1u);
  EXPECT_EQ(result->injections[0].topic, "/odom");
  ASSERT_EQ(result->injections[0].faults.size(), 1u);
}

TEST_F(ScenarioParserTest, ParsesMultipleInjectionsAndFaultsInOrder) {
  const std::string yaml = std::string(kHeader) + R"(injections:
  - topic: /odom
    faults:
      - {id: a, type: delay, params: {mean: 1ms, jitter: 1ms}}
      - {id: b, type: drop, params: {every_nth: 3}}
  - topic: /scan
    source: /scan_in
    faults:
      - {id: c, type: clock_skew, params: {offset: 5ms}}
)";
  const auto result = parse(yaml);
  ASSERT_TRUE(result.has_value()) << result.error();

  ASSERT_EQ(result->injections.size(), 2u);
  EXPECT_EQ(result->injections[0].topic, "/odom");
  ASSERT_EQ(result->injections[0].faults.size(), 2u);
  EXPECT_EQ(result->injections[0].faults[0].id, "a");
  EXPECT_EQ(result->injections[0].faults[1].id, "b");
  EXPECT_EQ(result->injections[1].topic, "/scan");
  ASSERT_EQ(result->injections[1].faults.size(), 1u);
  EXPECT_EQ(result->injections[1].faults[0].id, "c");
}

TEST_F(ScenarioParserTest, SeedAcceptsFullUint64Range) {
  std::string yaml = scenario_with_fault("drop", "every_nth: 2");
  yaml.replace(yaml.find("seed: 42"), 8, "seed: 18446744073709551615");
  const auto result = parse(yaml);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_EQ(result->seed, std::numeric_limits<uint64_t>::max());
}

TEST_F(ScenarioParserTest, InvalidSeedIsAnError) {
  for (const std::string seed : {"-1", "abc", "1.5", "18446744073709551616"}) {
    SCOPED_TRACE(seed);
    std::string yaml = scenario_with_fault("drop", "every_nth: 2");
    yaml.replace(yaml.find("seed: 42"), 8, "seed: " + seed);
    EXPECT_FALSE(parses(yaml));
  }
}

TEST_F(ScenarioParserTest, EachRequiredTopLevelKeyIsRequired) {
  for (const std::string key : {"name", "seed", "timeout", "injections"}) {
    SCOPED_TRACE(key);
    const std::string yaml = std::string(kHeader) +
                             "injections:\n  - topic: /odom\n    faults: "
                             "[{id: f, type: drop, params: {every_nth: 2}}]\n";
    // Drop the whole line that starts with the key (injections is multi-line,
    // so cut from its start to the end of the document).
    const auto pos = yaml.find(key + ":");
    ASSERT_NE(pos, std::string::npos);
    std::string cut = yaml;
    if (key == "injections") {
      cut.erase(pos);
    } else {
      cut.erase(pos, cut.find('\n', pos) - pos + 1);
    }
    EXPECT_FALSE(parses(cut));
  }
}

TEST_F(ScenarioParserTest, UnknownTopLevelKeyIsAnError) {
  EXPECT_FALSE(
      parses(scenario_with_fault("drop", "every_nth: 2") + "unexpected: 1\n"));
}

// ---------------------------------------------------------------------------
// Durations (exercised through `timeout`)
// ---------------------------------------------------------------------------

class DurationTest : public ScenarioParserTest,
                     public ::testing::WithParamInterface<
                         std::pair<std::string, nanoseconds>> {
 protected:
  std::string with_timeout(const std::string& timeout) {
    std::string yaml = scenario_with_fault("drop", "every_nth: 2");
    yaml.replace(yaml.find("timeout: 10s"), 12, "timeout: " + timeout);
    return yaml;
  }
};

TEST_P(DurationTest, ParsesUnit) {
  const auto& [text, expected] = GetParam();
  const auto result = parse(with_timeout(text));
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_EQ(result->timeout, expected);
}

INSTANTIATE_TEST_SUITE_P(
    Units, DurationTest,
    ::testing::Values(std::pair{"3min", nanoseconds{3min}},
                      std::pair{"90s", nanoseconds{90s}},
                      std::pair{"250ms", nanoseconds{250ms}},
                      std::pair{"100us", nanoseconds{100us}},
                      std::pair{"7ns", nanoseconds{7ns}},
                      std::pair{"0s", nanoseconds{0}}));

class InvalidDurationTest : public DurationTest {};

TEST_P(InvalidDurationTest, IsRejected) {
  EXPECT_FALSE(parses(with_timeout(GetParam().first)));
}

INSTANTIATE_TEST_SUITE_P(
    Malformed, InvalidDurationTest,
    ::testing::Values(std::pair{"10", nanoseconds{}},    // no unit
                      std::pair{"s", nanoseconds{}},     // no value
                      std::pair{"abc", nanoseconds{}},   // not a number
                      std::pair{"10x", nanoseconds{}},   // unknown unit
                      std::pair{"10 s", nanoseconds{}},  // space before unit
                      std::pair{"10S", nanoseconds{}},   // units are lowercase
                      std::pair{"-5s", nanoseconds{}},   // negative
                      std::pair{"1.5s", nanoseconds{}},  // fractional
                      std::pair{"99999999999999999999s",
                                nanoseconds{}}));  // > u64

// ---------------------------------------------------------------------------
// Injection
// ---------------------------------------------------------------------------

TEST_F(ScenarioParserTest, SourceDefaultsToTopicWithRawSuffix) {
  const auto result = parse(scenario_with_fault("drop", "every_nth: 2"));
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_EQ(result->injections[0].source, "/odom_raw");
}

TEST_F(ScenarioParserTest, ExplicitSourceIsKept) {
  const std::string yaml = std::string(kHeader) + R"(injections:
  - topic: /odom
    source: /my_odom
    faults: [{id: f, type: drop, params: {every_nth: 2}}]
)";
  const auto result = parse(yaml);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_EQ(result->injections[0].source, "/my_odom");
}

TEST_F(ScenarioParserTest, InjectionRequiresTopicAndFaults) {
  EXPECT_FALSE(parses(std::string(kHeader) + R"(injections:
  - faults: [{id: f, type: drop, params: {every_nth: 2}}]
)"));
  EXPECT_FALSE(parses(std::string(kHeader) + R"(injections:
  - topic: /odom
)"));
}

TEST_F(ScenarioParserTest, InjectionsThatIsNotASequenceIsAnError) {
  EXPECT_FALSE(parses(std::string(kHeader) + "injections: nope\n"));
}

// ---------------------------------------------------------------------------
// Fault: common fields
// ---------------------------------------------------------------------------

TEST_F(ScenarioParserTest, FaultKeepsIdAndType) {
  const auto result = parse(scenario_with_fault("drop", "every_nth: 2"));
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_EQ(first_fault(*result).id, "f1");
  EXPECT_EQ(first_fault(*result).type, "drop");
}

TEST_F(ScenarioParserTest, FaultRequiresIdTypeAndParams) {
  EXPECT_FALSE(parses(
      scenario_with_faults("{type: drop, params: {every_nth: 2}}")));  // no id
  EXPECT_FALSE(parses(
      scenario_with_faults("{id: f, params: {every_nth: 2}}")));      // no type
  EXPECT_FALSE(parses(scenario_with_faults("{id: f, type: drop}")));  // params
}

// ---------------------------------------------------------------------------
// Fault: delay
// ---------------------------------------------------------------------------

TEST_F(ScenarioParserTest, DelayParsesMeanAndJitter) {
  const auto result =
      parse(scenario_with_fault("delay", "mean: 200ms, jitter: 20ms"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const auto* p = std::get_if<DelayParams>(&first_fault(*result).params);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->mean, 200ms);
  EXPECT_EQ(p->jitter, 20ms);
  EXPECT_FALSE(p->distribution.has_value());
}

TEST_F(ScenarioParserTest, DelayParsesOptionalDistribution) {
  const auto result = parse(scenario_with_fault(
      "delay", "mean: 1s, jitter: 0s, distribution: gaussian"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const auto* p = std::get_if<DelayParams>(&first_fault(*result).params);
  ASSERT_NE(p, nullptr);
  ASSERT_TRUE(p->distribution.has_value());
  EXPECT_EQ(*p->distribution, "gaussian");
}

TEST_F(ScenarioParserTest, DelayRejectsBadParams) {
  EXPECT_FALSE(parses(scenario_with_fault("delay", "jitter: 20ms")));
  EXPECT_FALSE(parses(scenario_with_fault("delay", "mean: 200ms")));
  EXPECT_FALSE(parses(scenario_with_fault("delay", "mean: 200, jitter: 1ms")));
  EXPECT_FALSE(
      parses(scenario_with_fault("delay", "mean: 1ms, jitter: 1ms, bogus: 1")));
}

// ---------------------------------------------------------------------------
// Fault: drop
// ---------------------------------------------------------------------------

TEST_F(ScenarioParserTest, DropParsesProbability) {
  const auto result = parse(scenario_with_fault("drop", "probability: 0.3"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const auto* p = std::get_if<DropParams>(&first_fault(*result).params);
  ASSERT_NE(p, nullptr);
  ASSERT_TRUE(p->probability.has_value());
  EXPECT_DOUBLE_EQ(p->probability->value(), 0.3);
  EXPECT_FALSE(p->every_nth.has_value());
}

TEST_F(ScenarioParserTest, DropParsesEveryNth) {
  const auto result = parse(scenario_with_fault("drop", "every_nth: 4"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const auto* p = std::get_if<DropParams>(&first_fault(*result).params);
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->probability.has_value());
  ASSERT_TRUE(p->every_nth.has_value());
  EXPECT_EQ(*p->every_nth, 4u);
}

TEST_F(ScenarioParserTest, DropAcceptsProbabilityBoundaries) {
  EXPECT_TRUE(parses(scenario_with_fault("drop", "probability: 0")));
  EXPECT_TRUE(parses(scenario_with_fault("drop", "probability: 1")));
}

TEST_F(ScenarioParserTest, DropRejectsProbabilityOutOfRange) {
  EXPECT_FALSE(parses(scenario_with_fault("drop", "probability: 1.01")));
  EXPECT_FALSE(parses(scenario_with_fault("drop", "probability: -0.1")));
  EXPECT_FALSE(parses(scenario_with_fault("drop", "probability: lots")));
}

TEST_F(ScenarioParserTest, DropRequiresExactlyOneOfProbabilityAndEveryNth) {
  EXPECT_FALSE(parses(scenario_with_fault("drop", "")));
  EXPECT_FALSE(
      parses(scenario_with_fault("drop", "probability: 0.5, every_nth: 2")));
}

TEST_F(ScenarioParserTest, DropRejectsInvalidEveryNth) {
  EXPECT_FALSE(parses(scenario_with_fault("drop", "every_nth: 0")));
  EXPECT_FALSE(parses(scenario_with_fault("drop", "every_nth: -3")));
  EXPECT_FALSE(parses(scenario_with_fault("drop", "every_nth: 2.5")));
  EXPECT_FALSE(parses(scenario_with_fault("drop", "every_nth: often")));
}

TEST_F(ScenarioParserTest, DropRejectsUnknownKey) {
  EXPECT_FALSE(parses(scenario_with_fault("drop", "every_nth: 2, bogus: 1")));
}

// ---------------------------------------------------------------------------
// Fault: duplicate
// ---------------------------------------------------------------------------

TEST_F(ScenarioParserTest, DuplicateParsesProbabilityAndSpacing) {
  const auto result = parse(
      scenario_with_fault("duplicate", "probability: 0.25, spacing: 5ms"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const auto* p = std::get_if<DuplicateParams>(&first_fault(*result).params);
  ASSERT_NE(p, nullptr);
  EXPECT_DOUBLE_EQ(p->probability.value(), 0.25);
  EXPECT_EQ(p->spacing, 5ms);
}

TEST_F(ScenarioParserTest, DuplicateRejectsBadParams) {
  EXPECT_FALSE(parses(scenario_with_fault("duplicate", "spacing: 5ms")));
  EXPECT_FALSE(parses(scenario_with_fault("duplicate", "probability: 0.5")));
  EXPECT_FALSE(
      parses(scenario_with_fault("duplicate", "probability: 2, spacing: 5ms")));
  EXPECT_FALSE(
      parses(scenario_with_fault("duplicate", "probability: 0.5, spacing: 5")));
  EXPECT_FALSE(parses(scenario_with_fault(
      "duplicate", "probability: 0.5, spacing: 5ms, bogus: 1")));
}

// ---------------------------------------------------------------------------
// Fault: reorder
// ---------------------------------------------------------------------------

TEST_F(ScenarioParserTest, ReorderParsesWindowSizeAndProbability) {
  const auto result =
      parse(scenario_with_fault("reorder", "window_size: 5, probability: 0.5"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const auto* p = std::get_if<ReorderParams>(&first_fault(*result).params);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->window_size, 5u);
  EXPECT_DOUBLE_EQ(p->probability.value(), 0.5);
}

TEST_F(ScenarioParserTest, ReorderAcceptsMinimumWindowSize) {
  EXPECT_TRUE(parses(
      scenario_with_fault("reorder", "window_size: 2, probability: 0.5")));
}

TEST_F(ScenarioParserTest, ReorderRejectsBadParams) {
  EXPECT_FALSE(parses(
      scenario_with_fault("reorder", "window_size: 1, probability: 0.5")));
  EXPECT_FALSE(parses(
      scenario_with_fault("reorder", "window_size: 0, probability: 0.5")));
  EXPECT_FALSE(parses(
      scenario_with_fault("reorder", "window_size: -2, probability: 0.5")));
  EXPECT_FALSE(parses(scenario_with_fault("reorder", "probability: 0.5")));
  EXPECT_FALSE(parses(scenario_with_fault("reorder", "window_size: 3")));
  EXPECT_FALSE(parses(
      scenario_with_fault("reorder", "window_size: 3, probability: 1.5")));
  EXPECT_FALSE(parses(scenario_with_fault(
      "reorder", "window_size: 3, probability: 0.5, bogus: 1")));
}

// ---------------------------------------------------------------------------
// Fault: clock_skew
// ---------------------------------------------------------------------------

TEST_F(ScenarioParserTest, ClockSkewParsesOffset) {
  const auto result = parse(scenario_with_fault("clock_skew", "offset: 50ms"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const auto* p = std::get_if<ClockSkewParams>(&first_fault(*result).params);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->offset, 50ms);
}

TEST_F(ScenarioParserTest, ClockSkewRejectsBadParams) {
  EXPECT_FALSE(parses(scenario_with_fault("clock_skew", "")));
  EXPECT_FALSE(parses(scenario_with_fault("clock_skew", "offset: soon")));
  EXPECT_FALSE(
      parses(scenario_with_fault("clock_skew", "offset: 5ms, bogus: 1")));
}

// ---------------------------------------------------------------------------
// Fault: window
// ---------------------------------------------------------------------------

TEST_F(ScenarioParserTest, MissingWindowSpansWholeRun) {
  const auto result = parse(scenario_with_fault("drop", "every_nth: 2"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const Window& w = first_fault(*result).window;
  ASSERT_TRUE(w.start.has_value());
  ASSERT_TRUE(w.end.has_value());
  ASSERT_TRUE(std::holds_alternative<AtTime>(*w.start));
  ASSERT_TRUE(std::holds_alternative<AtTime>(*w.end));
  EXPECT_EQ(std::get<AtTime>(*w.start).time, nanoseconds{0});
  EXPECT_EQ(std::get<AtTime>(*w.end).time, nanoseconds::max());
}

TEST_F(ScenarioParserTest, WindowWithOnlyStartEndsAtEndOfRun) {
  const auto result =
      parse(scenario_with_fault("drop", "every_nth: 2", "start: {time: 0s}"));
  ASSERT_TRUE(result.has_value()) << result.error();
  // end is omitted, so it defaults to the end of the run.
  const Window& w = first_fault(*result).window;
  ASSERT_TRUE(w.end.has_value());
  ASSERT_TRUE(std::holds_alternative<AtTime>(*w.end));
  EXPECT_EQ(std::get<AtTime>(*w.end).time, nanoseconds::max());
}

TEST_F(ScenarioParserTest, WindowStartAtTime) {
  const auto result =
      parse(scenario_with_fault("drop", "every_nth: 2", "start: {time: 5s}"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const Window& w = first_fault(*result).window;
  ASSERT_TRUE(w.start.has_value());
  ASSERT_TRUE(std::holds_alternative<AtTime>(*w.start));
  EXPECT_EQ(std::get<AtTime>(*w.start).time, 5s);
}

TEST_F(ScenarioParserTest, WindowStartAfterEvent) {
  const auto result = parse(scenario_with_fault(
      "drop", "every_nth: 2", "start: {event: goal_accepted, after: 5s}"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const Window& w = first_fault(*result).window;
  ASSERT_TRUE(w.start.has_value());
  ASSERT_TRUE(std::holds_alternative<AfterEvent>(*w.start));
  EXPECT_EQ(std::get<AfterEvent>(*w.start).event, "goal_accepted");
  EXPECT_EQ(std::get<AfterEvent>(*w.start).after, 5s);
}

TEST_F(ScenarioParserTest, WindowEndAtTime) {
  const auto result =
      parse(scenario_with_fault("drop", "every_nth: 2", "end: {time: 30s}"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const Window& w = first_fault(*result).window;
  ASSERT_TRUE(w.end.has_value());
  ASSERT_TRUE(std::holds_alternative<AtTime>(*w.end));
  EXPECT_EQ(std::get<AtTime>(*w.end).time, 30s);
}

TEST_F(ScenarioParserTest, WindowEndForDuration) {
  const auto result = parse(
      scenario_with_fault("drop", "every_nth: 2", "end: {duration: 15s}"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const Window& w = first_fault(*result).window;
  ASSERT_TRUE(w.end.has_value());
  ASSERT_TRUE(std::holds_alternative<ForDuration>(*w.end));
  EXPECT_EQ(std::get<ForDuration>(*w.end).duration, 15s);
}

TEST_F(ScenarioParserTest, WindowWithOnlyEndStartsAtBeginning) {
  const auto result = parse(
      scenario_with_fault("drop", "every_nth: 2", "end: {duration: 15s}"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const Window& w = first_fault(*result).window;
  ASSERT_TRUE(w.start.has_value());
  ASSERT_TRUE(std::holds_alternative<AtTime>(*w.start));
  EXPECT_EQ(std::get<AtTime>(*w.start).time, nanoseconds{0});
}

TEST_F(ScenarioParserTest, WindowWithStartAndEnd) {
  const auto result = parse(scenario_with_fault(
      "drop", "every_nth: 2",
      "start: {event: goal_accepted, after: 5s}, end: {duration: 15s}"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const Window& w = first_fault(*result).window;
  ASSERT_TRUE(w.start.has_value());
  ASSERT_TRUE(w.end.has_value());
  EXPECT_TRUE(std::holds_alternative<AfterEvent>(*w.start));
  EXPECT_TRUE(std::holds_alternative<ForDuration>(*w.end));
}

TEST_F(ScenarioParserTest, WindowRejectsInvalidDurations) {
  EXPECT_FALSE(parses(
      scenario_with_fault("drop", "every_nth: 2", "start: {time: soon}")));
  EXPECT_FALSE(parses(
      scenario_with_fault("drop", "every_nth: 2", "end: {duration: 10}")));
  EXPECT_FALSE(parses(scenario_with_fault("drop", "every_nth: 2",
                                          "start: {event: e, after: 5}")));
}

TEST_F(ScenarioParserTest, WindowThatIsNotAMapIsAnError) {
  const std::string yaml = scenario_with_faults(
      "{id: f, type: drop, params: {every_nth: 2}, window: 5s}");
  EXPECT_FALSE(parses(yaml));
}

}  // namespace
