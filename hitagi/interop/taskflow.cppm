module;
#include <taskflow/taskflow.hpp>

export module interop.taskflow;

export namespace tf {
using ::tf::Executor;
using ::tf::Task;
using ::tf::Taskflow;
}  // namespace tf
