/*
 *
 * Copyright 2021-2024 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "metrics_handler_impl.h"
#include "srsran/support/executors/sync_task_executor.h"
#include "srsran/support/srsran_assert.h"
#include <thread>

using namespace srsran;
using namespace srs_cu_cp;

metrics_handler_impl::metrics_handler_impl(task_executor& cu_cp_exec_, timer_manager& timers_) :
  cu_cp_exec(cu_cp_exec_), timers(timers_), logger(srslog::fetch_basic_logger("CU-CP"))
{
}

std::unique_ptr<metrics_report_session>
metrics_handler_impl::create_periodic_report_session(const periodic_metric_report_request& request)
{
  class periodic_metrics_report_session_impl final : public metrics_report_session
  {
  public:
    periodic_metrics_report_session_impl(metrics_handler_impl& handler_, unsigned session_id_) :
      handler(&handler_), session_id(session_id_)
    {
    }
    ~periodic_metrics_report_session_impl() override { stop(); }

    void reconfigure_request(const periodic_metric_report_request& request) override
    {
      srsran_assert(handler != nullptr, "Using invalid metric report session");
      handler->request_session_reconfiguration(request);
    }

    /// Close the session, explicitly stopping the reporting of new metrics.
    void stop() override
    {
      if (handler != nullptr) {
        handler->request_session_deletion(session_id);
      }
    }

  private:
    metrics_handler_impl* handler = nullptr;
    unsigned              session_id;
  };

  // Allocate new Session context
  unsigned session_id = create_periodic_session(request);

  // Return handler to the session
  return std::make_unique<periodic_metrics_report_session_impl>(*this, session_id);
}

metrics_report metrics_handler_impl::handle_metrics_report_request(const metric_report_request& request)
{
  metrics_report report;

  force_blocking_execute(
      cu_cp_exec,
      [&]() { report = create_report(); },
      [this]() {
        logger.warning("Postponing metrics report request. Cause: CU-CP task queue is full");
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
      });

  return report;
}

metrics_report metrics_handler_impl::create_report()
{
  metrics_report report;

  // TODO

  return report;
}

unsigned metrics_handler_impl::create_periodic_session(const periodic_metric_report_request& request)
{
  // Allocate new session
  unsigned session_id;
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (free_list.empty()) {
      sessions.emplace_back();
      sessions.back().timer = timers.create_unique_timer(cu_cp_exec);
      session_id            = sessions.size() - 1;
    } else {
      session_id = free_list.back();
      free_list.pop_back();
    }
  }

  // Setup session timer and handler.
  sessions[session_id].report_notifier = request.report_notifier;
  sessions[session_id].timer.set(request.period, [this, session_id](timer_id_t tid) {
    // Generate a report.
    metrics_report report = create_report();

    // Notify report.
    sessions[session_id].report_notifier->notify_metrics_report_request(report);
  });

  return session_id;
}

void metrics_handler_impl::request_session_reconfiguration(const periodic_metric_report_request& request) {}

void metrics_handler_impl::request_session_deletion(unsigned session_id)
{
  // Deallocate session.
  std::lock_guard<std::mutex> lock(mutex);
  sessions[session_id].timer.stop();
  free_list.push_back(session_id);
}
