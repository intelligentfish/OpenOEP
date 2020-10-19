#pragma once
#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <glog/logging.h>

// Gloag初始化器
struct CGlogInitializer {
  // 构造方法
  CGlogInitializer(char* argv);
  // 析构方法
  virtual ~CGlogInitializer();
};
