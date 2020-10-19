#include "CSignal.h"

#include <csignal>
#include <mutex>
#include <unordered_map>
#include <vector>

struct CSignal::Internal {
  std::mutex m_exitedSigSetMutex;
  std::set<int> m_exitedSigSet;
  std::mutex m_sigMapMutex;
  std::unordered_map<int, std::vector<handler_type_t>> m_sigMap;
};
const std::set<int>& CSignal::defaultExitSignalSet() {
  static const std::set<int> set{SIGINT, SIGTERM, SIGABRT, SIGBREAK};
  return set;
}
CSignal& CSignal::instance() {
  static CSignal object;
  return object;
}
CSignal::~CSignal() {}
CSignal& CSignal::addExitSignal(const std::set<int>& sigVec) {
  std::lock_guard<std::mutex> lg(_internal->m_exitedSigSetMutex);
  for (auto sig : sigVec) _internal->m_exitedSigSet.insert(sig);
  return *this;
}
bool CSignal::isExistedSignal(int sig) {
  std::lock_guard<std::mutex> lg(_internal->m_exitedSigSetMutex);
  return 0 < _internal->m_exitedSigSet.count(sig);
}
CSignal& CSignal::add(int sig, handler_type_t handler) {
  std::lock_guard<std::mutex> lg(_internal->m_sigMapMutex);
  if (0 < _internal->m_sigMap.count(sig))
    _internal->m_sigMap[sig].push_back(handler);
  else
    _internal->m_sigMap.insert(
        std::make_pair(sig, std::vector<handler_type_t>{handler}));
  return *this;
}
CSignal& CSignal::add(const std::set<int>& sigVec, handler_type_t handler) {
  std::lock_guard<std::mutex> lg(_internal->m_sigMapMutex);
  for (auto sig : sigVec) {
    signal(sig, CSignal::handler);
    if (0 < _internal->m_sigMap.count(sig))
      _internal->m_sigMap[sig].push_back(handler);
    else
      _internal->m_sigMap.insert(
          std::make_pair(sig, std::vector<handler_type_t>{handler}));
  }
  return *this;
}
CSignal& CSignal::raise(int sig) {
  std::lock_guard<std::mutex> lg(_internal->m_sigMapMutex);
  auto it = _internal->m_sigMap.find(sig);
  if (_internal->m_sigMap.end() != it) {
    for (const auto& handler : it->second) handler(sig);
  }
  return *this;
}
void CSignal::handler(int sig) { CSignal::instance().raise(sig); }
CSignal::CSignal() {
  _internal = std::make_shared<Internal>();
  _internal->m_exitedSigSet = defaultExitSignalSet();
}