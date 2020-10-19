#pragma once

#include <csignal>
#include <functional>
#include <memory>
#include <set>

struct CSignal {
  typedef std::function<void(int sig)> handler_type_t;
  struct Internal;
  static CSignal& instance();
  static const std::set<int>& defaultExitSignalSet();
  virtual ~CSignal();
  CSignal& addExitSignal(const std::set<int>& sigVec);
  bool isExistedSignal(int sig);
  CSignal& add(int sig, handler_type_t handler);
  CSignal& add(const std::set<int>& sigVec, handler_type_t handler);
  CSignal& raise(int sig);

 private:
  static void handler(int sig);
  CSignal();
  std::shared_ptr<Internal> _internal;
};
