///==========================================================================
/// File: ArtsAbstractMachine.cpp
///
/// Implementation of ARTS Abstract Machine with comprehensive configuration
/// parsing and machine information capabilities.
///==========================================================================

#include "arts/Utils/ArtsAbstractMachine.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace mlir {
namespace arts {

ArtsAbstractMachine::ArtsAbstractMachine() {
  std::error_code ec;
  auto cwd = std::filesystem::current_path(ec);
  assert(!ec && "Failed to get current working directory");

  auto path = (cwd / "arts.cfg").string();
  auto parsed = parseFromFile(path);
  assert(parsed.has_value() &&
         "Failed to parse arts.cfg from current directory");

  *this = std::move(parsed.value());
  assert(isValid() && "Invalid arts.cfg: threads and nodeCount must be > 0");
}

std::optional<ArtsAbstractMachine>
ArtsAbstractMachine::parseFromFile(const std::string &path) {
  std::ifstream in(path);
  if (!in.is_open())
    return std::nullopt;

  ArtsAbstractMachine machine;
  std::string line;
  bool inArtsSection = false;

  while (std::getline(in, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#')
      continue;
    if (line.front() == '[' && line.back() == ']') {
      inArtsSection = (line == "[ARTS]");
      continue;
    }
    if (!inArtsSection)
      continue;

    auto pos = line.find('=');
    if (pos == std::string::npos)
      continue;
    std::string key = trim(line.substr(0, pos));
    std::string val = trim(line.substr(pos + 1));

    /// Core Configuration
    if (key == "threads")
      machine.threads = parseInt(val);
    else if (key == "tMT")
      machine.tMT = parseInt(val, 0);
    else if (key == "nodeCount")
      machine.nodeCount = parseInt(val);
    else if (key == "nodes")
      machine.nodes = splitCSV(val);
    else if (key == "masterNode")
      machine.masterNode = val;
    else if (key == "launcher")
      machine.launcher = val;

    /// GPU Configuration
    else if (key == "scheduler")
      machine.scheduler = parseInt(val, 0);
    else if (key == "gpu")
      machine.gpu = parseInt(val, 0);
    else if (key == "gpuRouteTableSize")
      machine.gpuRouteTableSize = parseInt(val, 12);
    else if (key == "freeDbAfterGpuRun")
      machine.freeDbAfterGpuRun = parseBool(val, false);
    else if (key == "deleteZerosGpuGc")
      machine.deleteZerosGpuGc = parseBool(val, true);
    else if (key == "runGpuGcIdle")
      machine.runGpuGcIdle = parseBool(val, true);
    else if (key == "runGpuGcPreEdt")
      machine.runGpuGcPreEdt = parseBool(val, false);
    else if (key == "gpuLocality")
      machine.gpuLocality = parseInt(val, 0);
    else if (key == "gpuFit")
      machine.gpuFit = parseInt(val, 0);
    else if (key == "gpuLCSync")
      machine.gpuLCSync = parseInt(val, 0);
    else if (key == "gpuBufferOn")
      machine.gpuBufferOn = parseBool(val, true);
    else if (key == "gpuMaxMemory")
      machine.gpuMaxMemory = parseInt(val, -1);
    else if (key == "gpuMaxEdts")
      machine.gpuMaxEdts = parseInt(val, -1);
    else if (key == "gpuP2P")
      machine.gpuP2P = parseBool(val, false);

    /// Network Configuration
    else if (key == "outgoing")
      machine.outgoing = parseInt(val, 1);
    else if (key == "incoming")
      machine.incoming = parseInt(val, 1);
    else if (key == "ports")
      machine.ports = parseInt(val, 1);
    else if (key == "protocol")
      machine.protocol = val;
    else if (key == "port")
      machine.port = parseInt(val, 34739);
    else if (key == "netInterface")
      machine.netInterface = val;

    /// Hardware Configuration
    else if (key == "pinStride")
      machine.pinStride = parseInt(val, 1);
    else if (key == "printTopology")
      machine.printTopology = parseBool(val, false);
    else if (key == "workerInitDequeSize")
      machine.workerInitDequeSize = parseInt(val, 2048);
    else if (key == "routeTableSize")
      machine.routeTableSize = parseInt(val, 16);
    else if (key == "coreDump")
      machine.coreDump = parseBool(val, true);

    /// Performance Monitoring
    else if (key == "counterFolder")
      machine.counterFolder = val;
    else if (key == "counterStartPoint")
      machine.counterStartPoint = parseInt(val, 1);
    else if (key == "killMode")
      machine.killMode = parseBool(val, false);

    /// Introspection
    else if (key == "introspectiveConf")
      machine.introspectiveConf = val;
    else if (key == "introspectiveFolder")
      machine.introspectiveFolder = val;
    else if (key == "introspectiveTraceLevel")
      machine.introspectiveTraceLevel = parseInt(val, 0);
    else if (key == "introspectiveStartPoint")
      machine.introspectiveStartPoint = parseInt(val, 1);
  }
  return machine;
}

std::string ArtsAbstractMachine::getMachineDescription() const {
  std::ostringstream oss;
  oss << "ARTS Abstract Machine Configuration:\n";
  oss << "  Execution Mode: " << (isLocalExecution() ? "Local" : "Distributed")
      << "\n";
  oss << "  Worker Threads: " << threads << " per node\n";
  oss << "  Total Nodes: " << nodeCount << "\n";
  oss << "  Total Worker Threads: " << getTotalWorkerThreads() << "\n";

  if (hasGpuSupport()) {
    oss << "  GPU Support: Enabled (" << gpu << " GPUs)\n";
    oss << "  GPU Scheduler: " << scheduler << "\n";
    oss << "  Total GPU Threads: " << getTotalGpuThreads() << "\n";
  } else {
    oss << "  GPU Support: Disabled\n";
  }

  oss << "  Temporal Multi-threading: " << tMT << "\n";
  oss << "  Launcher: " << launcher << "\n";
  oss << "  Master Node: " << masterNode << "\n";

  if (!nodes.empty()) {
    oss << "  Nodes: ";
    for (size_t i = 0; i < nodes.size(); ++i) {
      if (i > 0)
        oss << ", ";
      oss << nodes[i];
    }
    oss << "\n";
  }

  return oss.str();
}

std::string ArtsAbstractMachine::getConfigurationSummary() const {
  std::ostringstream oss;
  oss << "Configuration Summary:\n";
  oss << "  Threading: " << threads << " threads, tMT=" << tMT << "\n";
  oss << "  Networking: " << outgoing << " outgoing, " << incoming
      << " incoming threads\n";
  oss << "  Protocol: " << protocol << " on port " << port << "\n";
  oss << "  Hardware: pinStride=" << pinStride
      << ", dequeSize=" << workerInitDequeSize << "\n";
  oss << "  Monitoring: counters in " << counterFolder << "\n";

  if (hasGpuSupport()) {
    oss << "  GPU: " << gpu << " devices, locality=" << gpuLocality
        << ", fit=" << gpuFit << "\n";
  }

  return oss.str();
}

std::string ArtsAbstractMachine::trim(const std::string &s) {
  size_t i = 0, j = s.size();
  while (i < j && std::isspace(static_cast<unsigned char>(s[i])))
    ++i;
  while (j > i && std::isspace(static_cast<unsigned char>(s[j - 1])))
    --j;
  return s.substr(i, j - i);
}

bool ArtsAbstractMachine::startsWith(const std::string &s, const char *prefix) {
  size_t n = std::strlen(prefix);
  return s.size() >= n && std::equal(prefix, prefix + n, s.begin());
}

std::vector<std::string> ArtsAbstractMachine::splitCSV(const std::string &s) {
  std::vector<std::string> out;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ',')) {
    out.push_back(trim(item));
  }
  if (out.empty() && !s.empty())
    out.push_back(trim(s));
  return out;
}

int ArtsAbstractMachine::parseInt(const std::string &value, int defaultValue) {
  if (value.empty())
    return defaultValue;
  return std::atoi(value.c_str());
}

bool ArtsAbstractMachine::parseBool(const std::string &value,
                                    bool defaultValue) {
  if (value.empty())
    return defaultValue;
  std::string lower = value;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  return lower == "1" || lower == "true" || lower == "yes" || lower == "on";
}

} // namespace arts
} // namespace mlir