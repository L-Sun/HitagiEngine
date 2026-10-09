module;
#include <taskflow/taskflow.hpp>

export module interop.taskflow;

#if defined(_MSC_VER) && !defined(__clang__)
// Serialize the node container while Node is complete. Without this, MSVC 14.51
// recursively imports Graph -> vector<Node*> -> Node -> Graph as an incomplete type.
template class std::vector<tf::Node*>;
#endif

export namespace tf {
using ::tf::AsyncTask;
using ::tf::DefaultTaskParams;
using ::tf::Executor;
using ::tf::FlowBuilder;
using ::tf::Future;
using ::tf::Graph;
using ::tf::NonpreemptiveRuntime;
using ::tf::ObserverInterface;
using ::tf::Runtime;
using ::tf::Semaphore;
using ::tf::Subflow;
using ::tf::Task;
using ::tf::Taskflow;
using ::tf::TaskGroup;
using ::tf::TaskParams;
using ::tf::TaskType;
using ::tf::TaskView;
using ::tf::TFProfObserver;
using ::tf::version;
using ::tf::WorkerView;
}  // namespace tf

export namespace hitagi::interop {
namespace tf = ::tf;
}  // namespace hitagi::interop
