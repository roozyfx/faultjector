#include "scenario_parser.h"

#include <chrono>
#include <cstring>
#include <exception>
#include <expected>
#include <optional>
#include <print>
#include <string>

#include "utils.h"
#include "yaml-cpp/yaml.h"

// Find a given string @p key in the YAML::Node @p map, or
// std::nullopt if null or doesn't exist
std::optional<YAML::Node> find(const YAML::Node& map, const std::string& key) {
  const YAML::Node value = map[key];
  if (!value or value.IsNull()) return std::nullopt;
  return value;
}

void check_keys(
    const YAML::Node& map,
    const std::initializer_list<std::string_view>& acceptable_keys) {
  for (const auto& key : map) {
    if (std::ranges::find(acceptable_keys, key.first.as<std::string>()) ==
        acceptable_keys.end())
      throw YAML::InvalidNode(key.first.as<std::string>());
  }
}

Duration to_duration(const YAML::Node& node) {
  const std::string str{node.as<std::string>()};
  uint64_t value;
  auto [unit, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
  if (ec != std::errc()) {
    throw YAML::RepresentationException(node.Mark(), "invalid value");
  }
  if (std::strcmp(unit, "min") == 0) {
    return std::chrono::minutes(value);
  } else if (std::strcmp(unit, "s") == 0) {
    return std::chrono::seconds(value);
  } else if (std::strcmp(unit, "ms") == 0) {
    return std::chrono::milliseconds(value);
  } else if (std::strcmp(unit, "us") == 0) {
    return std::chrono::microseconds(value);
  } else if (std::strcmp(unit, "ns") == 0) {
    return std::chrono::nanoseconds(value);
  }
  throw YAML::RepresentationException(node.Mark(),
                                      "not a valid time duration format");
}

namespace YAML {
template <>
struct convert<WindowStart> {
  static Node encode(const WindowStart& ws) {
    // TODO
    Node node;
    return node;
  };

  static bool decode(const Node& node, WindowStart& win_start) {
    const bool is_at_time{node["time"]};
    const bool is_after_event{(node["after"] && node["event"])};
    if (!node.IsMap() && !(is_at_time || is_after_event)) return false;

    if (is_at_time) {
      AtTime at_time;
      at_time.time = to_duration(node["time"]);
      win_start = at_time;
    }
    if (is_after_event) {
      AfterEvent after_event;
      after_event.event = node["event"].as<std::string>();
      after_event.after = to_duration(node["after"]);
      win_start = after_event;
    }
    return true;
  }
};

template <>
struct convert<WindowEnd> {
  static Node encode(const WindowEnd& ws) {
    // TODO
    Node node;
    return node;
  };

  static bool decode(const Node& node, WindowEnd& win_end) {
    // nodes
    if (!node.IsMap() && !(node["duration"] || node["time"])) return false;

    if (node["time"]) {
      AtTime at_time;
      at_time.time = to_duration(node["time"]);
      win_end = at_time;
    }
    if (node["duration"]) {
      ForDuration for_duration;
      for_duration.duration = to_duration(node["duration"]);
      win_end = for_duration;
    }
    return true;
  }
};

template <>
struct convert<Window> {
  static Node encode(const Window& win) {
    // TODO
    Node node;
    return node;
  };

  static bool decode(const Node& node, Window& win) {
    if (!node.IsMap()) return false;

    // If no window.start is specified, default is from the beginning
    win.start = node["start"].as<WindowStart>(AtTime{Duration{0}});
    // If no window.end is specified, default is to the end
    win.end = node["end"].as<WindowEnd>(AtTime{Duration{Duration::max()}});

    return true;
  }
};

template <>
struct convert<Fault> {
  static Node encode(const Fault& fault) {
    Node node;
    // TODO
    return node;
  }

  static bool decode(const Node& node, Fault& fault) {
    // Since both 'start' and 'end' are optional in 'window', window can be
    // non-existant and it'd still be valid scenario file
    if (!node.IsMap() || !node["id"] || !node["params"]) return false;

    fault.id = node["id"].as<std::string>();
    fault.type = node["type"].as<std::string>();
    const auto& params{node["params"]};
    if (fault.type.compare("delay") == 0) {
      check_keys(params, {"mean", "jitter", "distribution"});
      DelayParams p;
      p.mean = to_duration(params["mean"]);
      p.jitter = to_duration(params["jitter"]);
      if (params["distribution"])
        p.distribution = params["distribution"].as<std::string>();
      else
        p.distribution = std::nullopt;

      fault.params = p;
    }

    if (fault.type.compare("drop") == 0) {
      check_keys(params, {"probability", "every_nth"});
      if (params["probability"] && params["every_nth"])
        throw YAML::InvalidNode(
            "Only one of 'probability' and 'every_nth' should be set.");
      if (!params["probability"] && !params["every_nth"])
        throw YAML::InvalidNode(
            "One of 'probability' or 'every_nth' should be set.");

      DropParams p;
      if (params["probability"])
        p.probability = Probability(params["probability"].as<double>());
      if (params["every_nth"]) {
        p.every_nth = params["every_nth"].as<uint64_t>();
        if (p.every_nth == 0)
          throw YAML::RepresentationException(node.Mark(),
                                              "Should be a positive integer.");
      }
      fault.params = p;
    }

    if (fault.type.compare("duplicate") == 0) {
      check_keys(params, {"probability", "spacing"});
      DuplicateParams p;
      p.probability = Probability(params["probability"].as<double>());
      p.spacing = to_duration(params["spacing"]);

      fault.params = p;
    }

    if (fault.type.compare("reorder") == 0) {
      check_keys(params, {"window_size", "probability"});
      ReorderParams p;
      p.window_size = params["window_size"].as<size_t>();
      if (p.window_size < 2)
        throw YAML::RepresentationException(
            node.Mark(), "Value should be greater than or equal to 2");
      p.probability = Probability(params["probability"].as<double>());

      fault.params = p;
    }

    if (fault.type.compare("clock_skew") == 0) {
      check_keys(params, {"offset"});
      ClockSkewParams p;
      // FIXME offset can take negative values, but here is only non-negative
      p.offset = to_duration(params["offset"]);

      fault.params = p;
    }

    if (!node["window"]) {
      fault.window.start = AtTime{Duration{0}};
      // If no window.end is specified, default is to the end
      fault.window.end = AtTime{Duration{Duration::max()}};
    } else {
      fault.window = node["window"].as<Window>();
    }
    return true;
  }
};

template <>
struct convert<Injection> {
  static Node encode(const Injection& inj) {
    Node node;
    node["topic"] = inj.topic;
    node["source"] = inj.source;
    node["faults"] = inj.faults;

    return node;
  }

  static bool decode(const Node& node, Injection& injection) {
    if (!node.IsMap() || !node["topic"] || !node["faults"]) return false;
    injection.topic = node["topic"].as<std::string>();
    injection.source = node["source"].as<std::string>(injection.topic + "_raw");
    if (node["faults"]) injection.faults = node["faults"].as<Faults>();

    return true;
  }
};
}  // namespace YAML

std::expected<Scenario, std::string> parse_root(const YAML::Node& root) {
  try {
    // FIXME add "assert"
    // check_keys(root,{"name", "seed", "timeout", "injections", "record",
    // "assert"});
    check_keys(root, {"name", "seed", "timeout", "injections", "record"});
    Scenario scenario;
    scenario.name = root["name"].as<std::string>();
    scenario.seed = root["seed"].as<uint64_t>();
    scenario.timeout = to_duration(root["timeout"]);
    scenario.injections = root["injections"].as<Injections>();
    scenario.record = root["record"].as<std::vector<std::string>>();
    // FIXME missing YAML decoder for assertion
    // scenario.assert = root["assert"].as<Assertion>();
    return scenario;
  } catch (const YAML::Exception& e) {
    std::println(stderr, "{}", e.what());
    // TODO
    return std::unexpected(std::format("Error at line: {}", e.what()));
  } catch (const std::exception& e) {
    throw e;
  }
}

std::expected<Scenario, std::string> parse_scenario(
    std::string_view scenario_file) {
  try {
    YAML::Node root = YAML::LoadFile(std::string(scenario_file));

    return parse_root(root);
  } catch (const YAML::Exception& e) {
    std::println(stderr, "{}", e.what());
  } catch (const std::exception& e) {
    std::println(stderr, "{}", e.what());
  }
  return std::unexpected("Error!");
}
