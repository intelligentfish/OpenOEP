#pragma once
#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <glog/logging.h>

// Gloag��ʼ����
struct CGlogInitializer {
  // ���췽��
  CGlogInitializer(char* argv);
  // ��������
  virtual ~CGlogInitializer();
};
