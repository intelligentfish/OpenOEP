#include "CGlogInitializer.h"

#include <boost/filesystem.hpp>

#define DEFAULT_LOG_PREFIX "AVAGENT"
#define DEFAULT_LOG_DIR "logs"

// 构造方法
CGlogInitializer::CGlogInitializer(char* argv) {
  FLAGS_colorlogtostderr = true;
  FLAGS_alsologtostderr = true;
  FLAGS_minloglevel = google::GLOG_INFO;
  FLAGS_log_prefix = DEFAULT_LOG_PREFIX;
  FLAGS_log_dir = DEFAULT_LOG_DIR;
  boost::filesystem::path logDir(DEFAULT_LOG_DIR);
  if (!boost::filesystem::exists(logDir))
    boost::filesystem::create_directory(logDir);
  google::InitGoogleLogging(argv);
}
// 析构方法
CGlogInitializer ::~CGlogInitializer() {
  google::FlushLogFiles(google::GLOG_INFO);
  google::ShutdownGoogleLogging();
}
