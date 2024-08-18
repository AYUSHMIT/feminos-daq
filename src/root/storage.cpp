
#include "storage.h"
#include "prometheus.h"
#include <iostream>

using namespace std;
using namespace mclient_storage;

void mclient_storage::StorageManager::Checkpoint(bool force) {
    if (!file) {
        return;
    }

    constexpr auto time_interval = std::chrono::seconds(20);
    auto now = std::chrono::system_clock::now();
    if (force || now - lastCheckpointTime > time_interval) {
        lastCheckpointTime = now;
        file->Write("", TObject::kOverwrite);
        file->Flush();
    }
}

StorageManager::StorageManager() = default;

double StorageManager::GetSpeedEventsPerSecond() const {
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - millisSinceEpochForSpeedCalculation;
    if (millis <= 0) {
        return 0.0;
    }
    return 1000.0 * GetNumberOfEntries() / millis;
}

void StorageManager::Initialize(const string& filename) {
    if (file != nullptr) {
        cerr << "StorageManager already initialized" << endl;
        throw std::runtime_error("StorageManager already initialized");
    }

    file = std::make_unique<TFile>(filename.c_str(), "RECREATE");

    if (compression_algorithm == "ZLIB") {
        file->SetCompressionAlgorithm(ROOT::kZLIB); // good compression ratio and fast (old root default)
    } else if (compression_algorithm == "LZ4") {
        file->SetCompressionAlgorithm(ROOT::kLZ4); // good compression ratio and fast (new root default)
    } else if (compression_algorithm == "LZMA") {
        file->SetCompressionAlgorithm(ROOT::kLZMA); // biggest compression ratio but slowest
    } else {
        throw std::runtime_error("Unknown compression algorithm: " + compression_algorithm);
    }

    // file->SetCompressionLevel(9);               // max compression level, but it's very slow, probably not worth it

    cout << "ROOT file will be saved to " << file->GetName() << endl;

    event_tree = std::make_unique<TTree>("events", "Signal events. Each entry is an event which contains multiple signals");

    event_tree->Branch("timestamp", &event.timestamp, "timestamp/L");
    event_tree->Branch("signal_ids", &event.signal_ids);
    event_tree->Branch("signal_data", &event.signal_data);

    run_tree = std::make_unique<TTree>("run", "Run metadata");

    run_tree->Branch("number", &run_number, "run_number/L");
    run_tree->Branch("name", &run_name);
    run_tree->Branch("timestamp", &run_time_start, "timestamp/L");
    run_tree->Branch("detector", &run_detector_name);
    run_tree->Branch("tag", &run_tag);
    run_tree->Branch("drift_field_V_cm_bar", &run_drift_field_V_cm_bar, "drift_field_V_cm_bar/F");
    run_tree->Branch("mesh_voltage_V", &run_mesh_voltage_V, "mesh_voltage_V/F");
    run_tree->Branch("detector_pressure_bar", &run_detector_pressure_bar, "detector_pressure_bar/F");
    run_tree->Branch("comments", &run_comments);
    run_tree->Branch("commands", &run_commands);

    // millis since epoch
    run_time_start = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    auto& prometheus_manager = mclient_prometheus::PrometheusManager::Instance();
    prometheus_manager.ExposeRootOutputFilename(filename);

    prometheus_manager.UpdateOutputRootFileSize();
}

std::pair<unsigned short, std::array<unsigned short, MAX_POINTS>> Event::get_signal_id_data_pair(size_t index) const {
    unsigned short channel = signal_ids[index];
    std::array<unsigned short, MAX_POINTS> data{};
    for (size_t i = 0; i < MAX_POINTS; ++i) {
        data[i] = signal_data[index * 512 + i];
    }
    return {channel, data};
}

void Event::add_signal(unsigned short id, const array<unsigned short, MAX_POINTS>& data) {
    signal_ids.push_back(id);
    signal_data.insert(signal_data.end(), data.begin(), data.end());
}
