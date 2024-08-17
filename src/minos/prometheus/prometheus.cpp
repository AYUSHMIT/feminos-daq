
#include "prometheus.h"

#include <chrono>
#include <thread>

mclient_prometheus::PrometheusManager::PrometheusManager() {
    exposer = std::make_unique<Exposer>("localhost:8080");

    registry = std::make_shared<Registry>();

    // Gauge to track free disk space
    auto free_disk_space_metric = &BuildGauge()
                                           .Name("daq_speed_mb_per_sec")
                                           .Help("DAQ speed in megabytes per second")
                                           .Register(*registry)
                                           .Add({{"path", "/"}});

    std::thread([this, &free_disk_space_metric]() {
        while (true) {
            double free_disk_space = GetFreeDiskSpaceGigabytes("/");
            if (free_disk_space >= 0) {
                free_disk_space_metric->Set(free_disk_space);
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }).detach();

    daq_speed_mb_per_s = &BuildGauge()
                                  .Name("daq_speed_mb_per_sec")
                                  .Help("DAQ speed in megabytes per second")
                                  .Register(*registry)
                                  .Add({});

    daq_speed_events_per_s = &BuildGauge()
                                      .Name("daq_speed_events_per_sec")
                                      .Help("DAQ speed in events per second")
                                      .Register(*registry)
                                      .Add({});

    event_id = &BuildGauge()
                        .Name("event_id")
                        .Help("Event ID of last event")
                        .Register(*registry)
                        .Add({});

    run_number = &BuildGauge()
                          .Name("run_number")
                          .Help("Run number")
                          .Register(*registry)
                          .Add({});

    number_of_events = &BuildGauge()
                                .Name("number_of_events")
                                .Help("Number of events processed")
                                .Register(*registry)
                                .Add({});

    number_of_signals_in_event = &BuildGauge()
                                          .Name("number_of_signals_in_last_event")
                                          .Help("Number of signals in last event")
                                          .Register(*registry)
                                          .Add({});

    {
        auto number_of_signals_in_event_histogram_bucket_boundaries = Histogram::BucketBoundaries{};
        for (int i = 0; i <= 500; i += 25) {
            number_of_signals_in_event_histogram_bucket_boundaries.push_back(i);
        }

        number_of_signals_in_event_histogram = &BuildHistogram()
                                                        .Name("number_of_signals_in_event")
                                                        .Help("Histogram of number of signals per event")
                                                        .Register(*registry)
                                                        .Add({}, number_of_signals_in_event_histogram_bucket_boundaries);
    }

    exposer->RegisterCollectable(registry);
}

mclient_prometheus::PrometheusManager::~PrometheusManager() = default;

void mclient_prometheus::PrometheusManager::SetDaqSpeed(double speed) {
    lock_guard<mutex> lock(mutex_);

    if (daq_speed_mb_per_s) {
        daq_speed_mb_per_s->Set(speed);
    }
}

void mclient_prometheus::PrometheusManager::SetEventId(unsigned int id) {
    lock_guard<mutex> lock(mutex_);

    if (event_id) {
        event_id->Set(id);
    }
}

void mclient_prometheus::PrometheusManager::SetNumberOfSignalsInEvent(unsigned int number) {
    lock_guard<mutex> lock(mutex_);

    if (number_of_signals_in_event) {
        number_of_signals_in_event->Set(number);
    }

    if (number_of_signals_in_event_histogram) {
        number_of_signals_in_event_histogram->Observe(number);
    }
}

void mclient_prometheus::PrometheusManager::SetNumberOfEvents(unsigned int id) {
    lock_guard<mutex> lock(mutex_);

    if (number_of_events) {
        number_of_events->Set(id);
    }
}

void mclient_prometheus::PrometheusManager::SetRunNumber(unsigned int id) {
    lock_guard<mutex> lock(mutex_);

    if (run_number) {
        run_number->Set(id);
    }
}

double mclient_prometheus::GetFreeDiskSpaceGigabytes(const std::string& path) {
    std::error_code ec;
    std::filesystem::space_info space = std::filesystem::space(path, ec);
    if (ec) {
        // Handle error
        return -1;
    }
    return static_cast<double>(space.free) / (1024 * 1024 * 1024);
}
