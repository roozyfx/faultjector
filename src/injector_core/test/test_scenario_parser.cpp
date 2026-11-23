#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include "fault_params.h"
#include "scenario.h"
#include "scenario_parser.h"
#include "timestamp.h"
#include "window.h"

using namespace std::chrono_literals;
using namespace fxcore;

namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Top-level entries of a scenario as YAML text, in document order.
using Entries = std::vector<std::pair<std::string, std::string>>;

// A minimal valid scenario. Every test below that expects a rejection starts
// from it and changes one thing, so the rejection can only come from that
// change.
Entries valid_entries() {
  return {
      {"name", "test_scenario"},
      {"seed", "42"},
      {"timeout", "10s"},
      {"injections",
       "[{topic: /odom, faults: [{id: f, type: drop, params: {every_nth: "
       "2}}]}]"},
      {"record", "[/odom, /scan]"},
  };
}

std::string to_yaml(const Entries& entries) {
  std::string yaml;
  for (const auto& [key, value] : entries)
    yaml += std::format("{}: {}\n", key, value);
  return yaml;
}

// The valid scenario with top-level @p key set to @p value (appended if the
// key is not there yet).
std::string scenario_with(std::string_view key, std::string_view value) {
  Entries entries = valid_entries();
  auto it = std::ranges::find_if(
      entries, [&](const auto& entry) { return entry.first == key; });
  if (it == entries.end())
    entries.emplace_back(key, value);
  else
    it->second = value;
  return to_yaml(entries);
}

// The valid scenario without top-level @p key.
std::string scenario_without(std::string_view key) {
  Entries entries = valid_entries();
  std::erase_if(entries, [&](const auto& entry) { return entry.first == key; });
  return to_yaml(entries);
}

// The valid scenario with a single injection on /odom holding @p faults, the
// inside of a flow sequence: "{id: a, ...}, {id: b, ...}".
std::string scenario_with_faults(std::string_view faults) {
  return scenario_with("injections",
                       std::format("[{{topic: /odom, faults: [{}]}}]", faults));
}

// A single fault 'f' of @p type. @p params is the inside of the params map.
std::string scenario_with_params(std::string_view type,
                                 std::string_view params) {
  return scenario_with_faults(
      std::format("{{id: f, type: {}, params: {{{}}}}}", type, params));
}

// A single drop fault 'f'. @p window is the inside of the window map.
std::string scenario_with_window(std::string_view window) {
  return scenario_with_faults(std::format(
      "{{id: f, type: drop, params: {{every_nth: 2}}, window: {{{}}}}}",
      window));
}

const Fault& first_fault(const Scenario& scenario) {
  return scenario.injections.at(0).faults.at(0);
}

// Pointer to the @p T alternative of an optional variant, or nullptr if it is
// unset or holds another alternative.
template <typename T, typename Variant>
const T* alternative(const std::optional<Variant>& value) {
  return value ? std::get_if<T>(&*value) : nullptr;
}

// parse_scenario() only reads files, so each test writes its YAML to its own
// temporary file.
class ScenarioParserTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    std::string file = std::format("scenario_parser_{}.{}.yaml",
                                   info->test_suite_name(), info->name());
    std::ranges::replace(file, '/', '_');
    path_ = std::filesystem::path(::testing::TempDir()) / file;
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }

  std::expected<Scenario, std::string> parse(std::string_view yaml) {
    std::ofstream(path_) << yaml;
    return parse_scenario(path_.string());
  }

  ::testing::AssertionResult accepts(std::string_view yaml) {
    const auto result = parse(yaml);
    if (result) return ::testing::AssertionSuccess();
    return ::testing::AssertionFailure()
           << "rejected with: " << result.error() << "\n"
           << yaml;
  }

  ::testing::AssertionResult rejects(std::string_view yaml) {
    if (!parse(yaml)) return ::testing::AssertionSuccess();
    return ::testing::AssertionFailure() << "accepted:\n" << yaml;
  }

  std::filesystem::path path_;
};

// Suite names only group the output; they all share the fixture above.
using ScenarioFile = ScenarioParserTest;
using ScenarioTopLevel = ScenarioParserTest;
using ScenarioDuration = ScenarioParserTest;
using ScenarioInjection = ScenarioParserTest;
using ScenarioFault = ScenarioParserTest;
using DelayFault = ScenarioParserTest;
using DropFault = ScenarioParserTest;
using DuplicateFault = ScenarioParserTest;
using ReorderFault = ScenarioParserTest;
using ClockSkewFault = ScenarioParserTest;
using FaultWindow = ScenarioParserTest;

// ---------------------------------------------------------------------------
// File
// ---------------------------------------------------------------------------

TEST_F(ScenarioFile, MissingFileIsAnError) {
  ASSERT_FALSE(std::filesystem::exists(path_));
  const auto result = parse_scenario(path_.string());
  ASSERT_FALSE(result.has_value());
  EXPECT_FALSE(result.error().empty());
}

TEST_F(ScenarioFile, EmptyFileIsAnError) { EXPECT_TRUE(rejects("")); }

TEST_F(ScenarioFile, MalformedYamlIsAnError) {
  EXPECT_TRUE(rejects(scenario_with("name", "[unterminated")));
}

TEST_F(ScenarioFile, RootMustBeAMap) {
  EXPECT_TRUE(rejects("- name\n- seed\n"));
  EXPECT_TRUE(rejects("just a string\n"));
}

// ---------------------------------------------------------------------------
// Top-level keys
// ---------------------------------------------------------------------------

TEST_F(ScenarioTopLevel, ParsesValidScenario) {
  const auto result = parse(to_yaml(valid_entries()));
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_EQ(result->name, "test_scenario");
  EXPECT_EQ(result->seed, 42u);
  EXPECT_EQ(result->timeout, 10s);
  EXPECT_EQ(result->record, (RecordList{"/odom", "/scan"}));
  ASSERT_EQ(result->injections.size(), 1u);
  ASSERT_EQ(result->injections[0].faults.size(), 1u);
}

// The example in scenario.h, minus `assert`, which is not supported yet.
TEST_F(ScenarioTopLevel, ParsesDocumentedExample) {
  const auto result = parse(R"(
name: odom_lag_during_navigation
seed: 20260907
timeout: 120s

injections:
  - topic: /odom            # republished under this name
    source: /odom_raw       # subscribed here; default is <topic>_raw
    faults:
      - id: odom_lag        # names this fault in logs, reports and assertions
        type: delay
        params: { mean: 200ms, jitter: 20ms }
        window:
          start: { event: goal_accepted, after: 5s }
          end:   { duration: 15s }

record: [/odom, /scan, /tf, /cmd_vel, /plan]
)");
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_EQ(result->name, "odom_lag_during_navigation");
  EXPECT_EQ(result->seed, 20260907u);
  EXPECT_EQ(result->timeout, 120s);
  EXPECT_EQ(result->record,
            (RecordList{"/odom", "/scan", "/tf", "/cmd_vel", "/plan"}));

  ASSERT_EQ(result->injections.size(), 1u);
  const Injection& injection = result->injections[0];
  EXPECT_EQ(injection.topic, "/odom");
  EXPECT_EQ(injection.source, "/odom_raw");

  ASSERT_EQ(injection.faults.size(), 1u);
  const Fault& fault = injection.faults[0];
  EXPECT_EQ(fault.id, "odom_lag");
  EXPECT_EQ(fault.type, "delay");

  const auto* params = std::get_if<DelayParams>(&fault.params);
  ASSERT_NE(params, nullptr);
  EXPECT_EQ(params->mean, 200ms);
  EXPECT_EQ(params->jitter, 20ms);
  EXPECT_FALSE(params->distribution.has_value());

  const auto* start = alternative<AfterEvent>(fault.window.start);
  ASSERT_NE(start, nullptr);
  EXPECT_EQ(start->event, "goal_accepted");
  EXPECT_EQ(start->after, 5s);

  const auto* end = alternative<ForDuration>(fault.window.end);
  ASSERT_NE(end, nullptr);
  EXPECT_EQ(end->duration, 15s);
}

TEST_F(ScenarioTopLevel, EveryKeyIsRequired) {
  for (const auto* key : {"name", "seed", "timeout", "injections", "record"})
    EXPECT_TRUE(rejects(scenario_without(key))) << "missing " << key;
}

TEST_F(ScenarioTopLevel, UnknownKeyIsAnError) {
  EXPECT_TRUE(rejects(scenario_with("unexpected", "1")));
}

TEST_F(ScenarioTopLevel, SeedCoversFullUint64Range) {
  for (const uint64_t seed :
       {uint64_t{0}, std::numeric_limits<uint64_t>::max()}) {
    const auto result = parse(scenario_with("seed", std::to_string(seed)));
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result->seed, seed);
  }
}

TEST_F(ScenarioTopLevel, InvalidSeedIsAnError) {
  for (const auto* seed : {"-1", "1.5", "abc", "18446744073709551616", "~"})
    EXPECT_TRUE(rejects(scenario_with("seed", seed)));
}

TEST_F(ScenarioTopLevel, RecordMustBeASequence) {
  EXPECT_TRUE(rejects(scenario_with("record", "/odom")));
}

// ---------------------------------------------------------------------------
// Durations, exercised through `timeout` unless noted otherwise
// ---------------------------------------------------------------------------

TEST_F(ScenarioDuration, ParsesEveryUnit) {
  const std::pair<std::string_view, Duration> cases[]{
      {"0s", 0s},       {"7ns", 7ns}, {"100us", 100us},
      {"250ms", 250ms}, {"90s", 90s}, {"3min", 3min},
  };
  for (const auto& [text, expected] : cases) {
    const auto result = parse(scenario_with("timeout", text));
    ASSERT_TRUE(result.has_value()) << text << ": " << result.error();
    EXPECT_EQ(result->timeout, expected) << text;
  }
}

TEST_F(ScenarioDuration, AcceptsUpTo365DaysInAnyUnit) {
  constexpr Duration kMax{std::chrono::days{365}};
  for (const auto* text : {"525600min", "31536000s", "31536000000ms",
                           "31536000000000us", "31536000000000000ns"}) {
    const auto result = parse(scenario_with("timeout", text));
    ASSERT_TRUE(result.has_value()) << text << ": " << result.error();
    EXPECT_EQ(result->timeout, kMax) << text;
  }
}

TEST_F(ScenarioDuration, RejectsMoreThan365DaysInAnyUnit) {
  for (const auto* text : {"525601min", "31536001s", "31536000001ms",
                           "31536000000001us", "31536000000000001ns",
                           "18446744073709551615ns",  // fits u64, not i64
                           "99999999999999999999s"})  // fits neither
    EXPECT_TRUE(rejects(scenario_with("timeout", text)));
}

TEST_F(ScenarioDuration, MalformedIsAnError) {
  for (const auto* text : {
           "10",      // no unit
           "s",       // no value
           "\"\"",    // empty
           "abc",     // not a number
           "10x",     // unknown unit
           "10m",     // 'm' is not a unit
           "10mins",  // units must match exactly
           "10 s",    // space before the unit
           "10S",     // units are lower case
           "1.5s",    // fractional
           "~",       // null
           "[10s]",   // not a scalar
       })
    EXPECT_TRUE(rejects(scenario_with("timeout", text)));
}

// Only clock_skew's offset may be negative; see ClockSkewFault.
TEST_F(ScenarioDuration, NegativeIsAnError) {
  for (const auto& yaml : {
           scenario_with("timeout", "-1s"),
           scenario_with_params("delay", "mean: -1ms, jitter: 1ms"),
           scenario_with_params("delay", "mean: 1ms, jitter: -1ms"),
           scenario_with_params("duplicate", "probability: 0.5, spacing: -1ms"),
           scenario_with_window("start: {time: -1s}"),
           scenario_with_window("start: {event: e, after: -1s}"),
           scenario_with_window("end: {time: -1s}"),
           scenario_with_window("end: {duration: -1s}"),
       })
    EXPECT_TRUE(rejects(yaml));
}

// ---------------------------------------------------------------------------
// Injections
// ---------------------------------------------------------------------------

TEST_F(ScenarioInjection, ParsesInjectionsAndFaultsInOrder) {
  const auto result = parse(scenario_with("injections", R"(
  - topic: /odom
    faults:
      - {id: a, type: delay, params: {mean: 1ms, jitter: 1ms}}
      - {id: b, type: drop, params: {every_nth: 3}}
  - topic: /scan
    faults:
      - {id: c, type: clock_skew, params: {offset: 5ms}})"));
  ASSERT_TRUE(result.has_value()) << result.error();

  ASSERT_EQ(result->injections.size(), 2u);
  const Injection& odom = result->injections[0];
  const Injection& scan = result->injections[1];

  EXPECT_EQ(odom.topic, "/odom");
  ASSERT_EQ(odom.faults.size(), 2u);
  EXPECT_EQ(odom.faults[0].id, "a");
  EXPECT_TRUE(std::holds_alternative<DelayParams>(odom.faults[0].params));
  EXPECT_EQ(odom.faults[1].id, "b");
  EXPECT_TRUE(std::holds_alternative<DropParams>(odom.faults[1].params));

  EXPECT_EQ(scan.topic, "/scan");
  ASSERT_EQ(scan.faults.size(), 1u);
  EXPECT_EQ(scan.faults[0].id, "c");
  EXPECT_TRUE(std::holds_alternative<ClockSkewParams>(scan.faults[0].params));
}

TEST_F(ScenarioInjection, SourceDefaultsToTopicWithRawSuffix) {
  const auto result = parse(to_yaml(valid_entries()));
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_EQ(result->injections[0].source, "/odom_raw");
}

TEST_F(ScenarioInjection, ExplicitSourceIsKept) {
  const auto result = parse(
      scenario_with("injections",
                    "[{topic: /odom, source: /wheel_odom, "
                    "faults: [{id: f, type: drop, params: {every_nth: 2}}]}]"));
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_EQ(result->injections[0].topic, "/odom");
  EXPECT_EQ(result->injections[0].source, "/wheel_odom");
}

TEST_F(ScenarioInjection, TopicAndFaultsAreRequired) {
  EXPECT_TRUE(rejects(scenario_with(
      "injections",
      "[{faults: [{id: f, type: drop, params: {every_nth: 2}}]}]")));
  EXPECT_TRUE(rejects(scenario_with("injections", "[{topic: /odom}]")));
}

TEST_F(ScenarioInjection, UnknownKeyIsAnError) {
  EXPECT_TRUE(rejects(scenario_with(
      "injections",
      "[{topic: /odom, sink: /odom_out, "
      "faults: [{id: f, type: drop, params: {every_nth: 2}}]}]")));
}

TEST_F(ScenarioInjection, InjectionsMustBeASequenceOfMaps) {
  EXPECT_TRUE(rejects(scenario_with("injections", "nope")));
  EXPECT_TRUE(rejects(scenario_with("injections", "[/odom]")));
  // A single injection that is not wrapped in a sequence.
  EXPECT_TRUE(rejects(scenario_with("injections",
                                    "{topic: /odom, faults: [{id: f, type: "
                                    "drop, params: {every_nth: 2}}]}")));
}

// ---------------------------------------------------------------------------
// Fault: common fields
// ---------------------------------------------------------------------------

TEST_F(ScenarioFault, KeepsIdAndType) {
  const auto result = parse(scenario_with_params("drop", "every_nth: 2"));
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_EQ(first_fault(*result).id, "f");
  EXPECT_EQ(first_fault(*result).type, "drop");
}

TEST_F(ScenarioFault, IdTypeAndParamsAreRequired) {
  EXPECT_TRUE(
      rejects(scenario_with_faults("{type: drop, params: {every_nth: 2}}")));
  EXPECT_TRUE(rejects(scenario_with_faults("{id: f, params: {every_nth: 2}}")));
  EXPECT_TRUE(rejects(scenario_with_faults("{id: f, type: drop}")));
}

TEST_F(ScenarioFault, UnknownKeyIsAnError) {
  EXPECT_TRUE(rejects(scenario_with_faults(
      "{id: f, type: drop, params: {every_nth: 2}, priority: 1}")));
}

TEST_F(ScenarioFault, UnknownTypeIsAnError) {
  // Type names are matched exactly.
  for (const auto* type : {"dleay", "Delay", "\"\""}) {
    const auto result =
        parse(scenario_with_params(type, "mean: 1ms, jitter: 1ms"));
    ASSERT_FALSE(result.has_value()) << "type " << type << " was accepted";
    EXPECT_TRUE(result.error().contains("Invalid fault type"))
        << result.error();
  }
}

TEST_F(ScenarioFault, ParamsMustMatchType) {
  EXPECT_TRUE(rejects(scenario_with_params("delay", "every_nth: 2")));
  EXPECT_TRUE(rejects(scenario_with_params("drop", "mean: 1ms, jitter: 1ms")));
}

TEST_F(ScenarioFault, ParamsMustBeAMap) {
  EXPECT_TRUE(rejects(scenario_with_faults("{id: f, type: drop, params: 2}")));
}

TEST_F(ScenarioFault, IdsMustBeUniqueWithinAnInjection) {
  const auto result = parse(
      scenario_with_faults("{id: dup, type: drop, params: {every_nth: 2}}, "
                           "{id: dup, type: drop, params: {every_nth: 3}}"));
  ASSERT_FALSE(result.has_value());
  EXPECT_TRUE(result.error().contains("'dup'")) << result.error();
}

TEST_F(ScenarioFault, IdsMustBeUniqueAcrossInjections) {
  const auto result =
      parse(scenario_with("injections",
                          "[{topic: /odom, faults: [{id: dup, type: drop, "
                          "params: {every_nth: 2}}]},"
                          " {topic: /scan, faults: [{id: dup, type: drop, "
                          "params: {every_nth: 2}}]}]"));
  ASSERT_FALSE(result.has_value());
  EXPECT_TRUE(result.error().contains("'dup'")) << result.error();
}

// Uniqueness is per file: ids seen in one parse must not leak into the next.
TEST_F(ScenarioFault, SameFileCanBeParsedTwice) {
  const std::string yaml = to_yaml(valid_entries());
  EXPECT_TRUE(accepts(yaml));
  EXPECT_TRUE(accepts(yaml));
}

// ---------------------------------------------------------------------------
// Fault: delay
// ---------------------------------------------------------------------------

TEST_F(DelayFault, ParsesMeanAndJitter) {
  const auto result =
      parse(scenario_with_params("delay", "mean: 200ms, jitter: 20ms"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const auto* params = std::get_if<DelayParams>(&first_fault(*result).params);
  ASSERT_NE(params, nullptr);
  EXPECT_EQ(params->mean, 200ms);
  EXPECT_EQ(params->jitter, 20ms);
  EXPECT_FALSE(params->distribution.has_value());
}

TEST_F(DelayFault, ParsesOptionalDistribution) {
  const auto result = parse(scenario_with_params(
      "delay", "mean: 1s, jitter: 0s, distribution: gaussian"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const auto* params = std::get_if<DelayParams>(&first_fault(*result).params);
  ASSERT_NE(params, nullptr);
  EXPECT_EQ(params->distribution, "gaussian");
}

TEST_F(DelayFault, RejectsBadParams) {
  for (const auto* params : {
           "jitter: 20ms",                      // no mean
           "mean: 200ms",                       // no jitter
           "mean: 200, jitter: 20ms",           // mean without unit
           "mean: 200ms, jitter: 20",           // jitter without unit
           "mean: 200ms, jitter: 20ms, sd: 1",  // unknown key
       })
    EXPECT_TRUE(rejects(scenario_with_params("delay", params)));
}

// ---------------------------------------------------------------------------
// Fault: drop
// ---------------------------------------------------------------------------

TEST_F(DropFault, ParsesProbability) {
  const auto result = parse(scenario_with_params("drop", "probability: 0.3"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const auto* params = std::get_if<DropParams>(&first_fault(*result).params);
  ASSERT_NE(params, nullptr);
  ASSERT_TRUE(params->probability.has_value());
  EXPECT_DOUBLE_EQ(params->probability->value(), 0.3);
  EXPECT_FALSE(params->every_nth.has_value());
}

TEST_F(DropFault, ParsesEveryNth) {
  const auto result = parse(scenario_with_params("drop", "every_nth: 4"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const auto* params = std::get_if<DropParams>(&first_fault(*result).params);
  ASSERT_NE(params, nullptr);
  EXPECT_FALSE(params->probability.has_value());
  EXPECT_EQ(params->every_nth, 4u);
}

TEST_F(DropFault, AcceptsBoundaryValues) {
  EXPECT_TRUE(accepts(scenario_with_params("drop", "probability: 0")));
  EXPECT_TRUE(accepts(scenario_with_params("drop", "probability: 1")));
  EXPECT_TRUE(accepts(scenario_with_params("drop", "every_nth: 1")));
}

TEST_F(DropFault, RequiresExactlyOneOfProbabilityAndEveryNth) {
  EXPECT_TRUE(rejects(scenario_with_params("drop", "")));
  EXPECT_TRUE(
      rejects(scenario_with_params("drop", "probability: 0.5, every_nth: 2")));
}

TEST_F(DropFault, RejectsInvalidProbability) {
  for (const auto* probability : {"-0.1", "1.01", ".nan", ".inf", "lots"})
    EXPECT_TRUE(rejects(scenario_with_params(
        "drop", std::format("probability: {}", probability))));
}

TEST_F(DropFault, RejectsInvalidEveryNth) {
  for (const auto* every_nth : {"0", "-3", "2.5", "often"})
    EXPECT_TRUE(rejects(
        scenario_with_params("drop", std::format("every_nth: {}", every_nth))));
}

TEST_F(DropFault, RejectsUnknownKey) {
  EXPECT_TRUE(rejects(scenario_with_params("drop", "every_nth: 2, burst: 3")));
}

// ---------------------------------------------------------------------------
// Fault: duplicate
// ---------------------------------------------------------------------------

TEST_F(DuplicateFault, ParsesProbabilityAndSpacing) {
  const auto result = parse(
      scenario_with_params("duplicate", "probability: 0.25, spacing: 5ms"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const auto* params =
      std::get_if<DuplicateParams>(&first_fault(*result).params);
  ASSERT_NE(params, nullptr);
  EXPECT_DOUBLE_EQ(params->probability.value(), 0.25);
  EXPECT_EQ(params->spacing, 5ms);
}

TEST_F(DuplicateFault, RejectsBadParams) {
  for (const auto* params : {
           "spacing: 5ms",                              // no probability
           "probability: 0.5",                          // no spacing
           "probability: 1.5, spacing: 5ms",            // probability > 1
           "probability: 0.5, spacing: 5",              // spacing without unit
           "probability: 0.5, spacing: 5ms, count: 2",  // unknown key
       })
    EXPECT_TRUE(rejects(scenario_with_params("duplicate", params)));
}

// ---------------------------------------------------------------------------
// Fault: reorder
// ---------------------------------------------------------------------------

TEST_F(ReorderFault, ParsesWindowSizeAndProbability) {
  const auto result = parse(
      scenario_with_params("reorder", "window_size: 5, probability: 0.5"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const auto* params = std::get_if<ReorderParams>(&first_fault(*result).params);
  ASSERT_NE(params, nullptr);
  EXPECT_EQ(params->window_size, 5u);
  EXPECT_DOUBLE_EQ(params->probability.value(), 0.5);
}

TEST_F(ReorderFault, AcceptsMinimumWindowSize) {
  EXPECT_TRUE(accepts(
      scenario_with_params("reorder", "window_size: 2, probability: 0.5")));
}

TEST_F(ReorderFault, RejectsBadParams) {
  for (const auto* params : {
           "window_size: 1, probability: 0.5",           // below minimum
           "window_size: 0, probability: 0.5",           // below minimum
           "window_size: -2, probability: 0.5",          // negative
           "window_size: 2.5, probability: 0.5",         // fractional
           "probability: 0.5",                           // no window_size
           "window_size: 3",                             // no probability
           "window_size: 3, probability: -0.5",          // probability < 0
           "window_size: 3, probability: 0.5, seed: 1",  // unknown key
       })
    EXPECT_TRUE(rejects(scenario_with_params("reorder", params)));
}

// ---------------------------------------------------------------------------
// Fault: clock_skew
// ---------------------------------------------------------------------------

TEST_F(ClockSkewFault, OffsetMayBeNegative) {
  const std::pair<std::string_view, Duration> cases[]{
      {"50ms", 50ms},
      {"-50ms", -50ms},
      {"0ns", 0ns},
      {"-525600min", -std::chrono::days{365}},
  };
  for (const auto& [offset, expected] : cases) {
    const auto result = parse(
        scenario_with_params("clock_skew", std::format("offset: {}", offset)));
    ASSERT_TRUE(result.has_value()) << offset << ": " << result.error();

    const auto* params =
        std::get_if<ClockSkewParams>(&first_fault(*result).params);
    ASSERT_NE(params, nullptr);
    EXPECT_EQ(params->offset, expected) << offset;
  }
}

TEST_F(ClockSkewFault, RejectsBadParams) {
  for (const auto* params : {
           "",                         // no offset
           "offset: soon",             // not a duration
           "offset: 5",                // no unit
           "offset: 525601min",        // more than 365 days
           "offset: -525601min",       // more than 365 days back
           "offset: 5ms, drift: 1ms",  // unknown key
       })
    EXPECT_TRUE(rejects(scenario_with_params("clock_skew", params)));
}

// ---------------------------------------------------------------------------
// Fault: window
// ---------------------------------------------------------------------------

TEST_F(FaultWindow, MissingWindowSpansWholeRun) {
  const auto result = parse(scenario_with_params("drop", "every_nth: 2"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const Window& window = first_fault(*result).window;
  const auto* start = alternative<AtTime>(window.start);
  ASSERT_NE(start, nullptr);
  EXPECT_EQ(start->time, Timestamp{});
  const auto* end = alternative<AtTime>(window.end);
  ASSERT_NE(end, nullptr);
  EXPECT_EQ(end->time, Timestamp::max());
}

TEST_F(FaultWindow, EmptyWindowSpansWholeRun) {
  const auto result = parse(scenario_with_window(""));
  ASSERT_TRUE(result.has_value()) << result.error();

  const Window& window = first_fault(*result).window;
  const auto* start = alternative<AtTime>(window.start);
  ASSERT_NE(start, nullptr);
  EXPECT_EQ(start->time, Timestamp{});
  const auto* end = alternative<AtTime>(window.end);
  ASSERT_NE(end, nullptr);
  EXPECT_EQ(end->time, Timestamp::max());
}

// A null start or end counts as unset.
TEST_F(FaultWindow, NullStartAndEndSpanWholeRun) {
  const auto result = parse(scenario_with_window("start: ~, end: ~"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const Window& window = first_fault(*result).window;
  const auto* start = alternative<AtTime>(window.start);
  ASSERT_NE(start, nullptr);
  EXPECT_EQ(start->time, Timestamp{});
  const auto* end = alternative<AtTime>(window.end);
  ASSERT_NE(end, nullptr);
  EXPECT_EQ(end->time, Timestamp::max());
}

TEST_F(FaultWindow, StartAtTime) {
  const auto result = parse(scenario_with_window("start: {time: 5s}"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const auto* start = alternative<AtTime>(first_fault(*result).window.start);
  ASSERT_NE(start, nullptr);
  EXPECT_EQ(start->time, Timestamp{5s});
}

TEST_F(FaultWindow, StartAfterEvent) {
  const auto result =
      parse(scenario_with_window("start: {event: goal_accepted, after: 5s}"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const auto* start =
      alternative<AfterEvent>(first_fault(*result).window.start);
  ASSERT_NE(start, nullptr);
  EXPECT_EQ(start->event, "goal_accepted");
  EXPECT_EQ(start->after, 5s);
}

TEST_F(FaultWindow, EndAtTime) {
  const auto result = parse(scenario_with_window("end: {time: 30s}"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const auto* end = alternative<AtTime>(first_fault(*result).window.end);
  ASSERT_NE(end, nullptr);
  EXPECT_EQ(end->time, Timestamp{30s});
}

TEST_F(FaultWindow, EndForDuration) {
  const auto result = parse(scenario_with_window("end: {duration: 15s}"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const auto* end = alternative<ForDuration>(first_fault(*result).window.end);
  ASSERT_NE(end, nullptr);
  EXPECT_EQ(end->duration, 15s);
}

TEST_F(FaultWindow, OnlyStartEndsAtEndOfRun) {
  const auto result = parse(scenario_with_window("start: {time: 5s}"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const auto* end = alternative<AtTime>(first_fault(*result).window.end);
  ASSERT_NE(end, nullptr);
  EXPECT_EQ(end->time, Timestamp::max());
}

TEST_F(FaultWindow, OnlyEndStartsAtBeginningOfRun) {
  const auto result = parse(scenario_with_window("end: {duration: 15s}"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const auto* start = alternative<AtTime>(first_fault(*result).window.start);
  ASSERT_NE(start, nullptr);
  EXPECT_EQ(start->time, Timestamp{});
}

TEST_F(FaultWindow, StartAndEnd) {
  const auto result = parse(scenario_with_window(
      "start: {event: goal_accepted, after: 5s}, end: {time: 30s}"));
  ASSERT_TRUE(result.has_value()) << result.error();

  const Window& window = first_fault(*result).window;
  const auto* start = alternative<AfterEvent>(window.start);
  ASSERT_NE(start, nullptr);
  EXPECT_EQ(start->event, "goal_accepted");
  EXPECT_EQ(start->after, 5s);
  const auto* end = alternative<AtTime>(window.end);
  ASSERT_NE(end, nullptr);
  EXPECT_EQ(end->time, Timestamp{30s});
}

TEST_F(FaultWindow, RejectsBadStart) {
  for (const auto* window : {
           "start: {}",                                 // neither form
           "start: {time: ~}",                          // null time
           "start: {event: goal_accepted}",             // event without after
           "start: {after: 5s}",                        // after without event
           "start: {time: 1s, event: e, after: 5s}",    // both forms
           "start: {time: 1s, offset: 1s}",             // unknown key
           "start: {event: e, after: 5s, offset: 1s}",  // unknown key
           "start: {time: soon}",                       // not a duration
           "start: {event: e, after: 5}",               // no unit
       })
    EXPECT_TRUE(rejects(scenario_with_window(window)));
}

TEST_F(FaultWindow, RejectsBadEnd) {
  for (const auto* window : {
           "end: {}",                          // neither form
           "end: {time: ~}",                   // null time
           "end: {duration: ~}",               // null duration
           "end: {time: 30s, duration: 15s}",  // both forms
           "end: {duration: 15s, until: e}",   // unknown key
           "end: {duration: 10}",              // no unit
       })
    EXPECT_TRUE(rejects(scenario_with_window(window)));
}

TEST_F(FaultWindow, UnknownKeyIsAnError) {
  EXPECT_TRUE(
      rejects(scenario_with_window("start: {time: 1s}, stop: {time: 2s}")));
}

TEST_F(FaultWindow, WindowMustBeAMap) {
  EXPECT_TRUE(rejects(scenario_with_faults(
      "{id: f, type: drop, params: {every_nth: 2}, window: 5s}")));
}

}  // namespace
