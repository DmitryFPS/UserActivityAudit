#pragma once

#include "useraudit/jsonl_writer.hpp"

#include <windows.h>

#include <atomic>
#include <thread>

namespace useraudit {

class PipeIngestServer {
public:
    explicit PipeIngestServer(JsonlWriter& writer);
    ~PipeIngestServer();

    PipeIngestServer(const PipeIngestServer&) = delete;
    PipeIngestServer& operator=(const PipeIngestServer&) = delete;

    bool start();
    void stop();

private:
    void accept_loop();
    void handle_client(HANDLE pipe);

    JsonlWriter& writer_;
    std::thread accept_thread_;
    std::atomic<bool> running_{false};
    HANDLE stop_event_ = nullptr;
};

}  // namespace useraudit
