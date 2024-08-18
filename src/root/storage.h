
#ifndef MCLIENT_STORAGE_H
#define MCLIENT_STORAGE_H

#include <TFile.h>
#include <TTree.h>
#include <array>
#include <string>
#include <vector>

namespace mclient_storage {

constexpr int MAX_SIGNALS = 1152; // To cover up to 4 Feminos boards 72 * 4 * 4
constexpr int MAX_POINTS = 512;

class Event {
public:
    unsigned long long timestamp = 0;
    unsigned int id = 0;
    std::vector<unsigned short> signal_ids;
    std::vector<unsigned short> signal_data; // all data points from all signals concatenated (same order as signal_ids)

    Event() {
        // reserve space for the maximum number of signals and points
        signal_ids.reserve(MAX_SIGNALS);
        signal_data.reserve(MAX_POINTS * MAX_POINTS);
    }

    size_t size() const {
        return signal_ids.size();
    }

    std::pair<unsigned short, std::array<unsigned short, MAX_POINTS>> get_signal_id_data_pair(size_t index) const;

    void add_signal(unsigned short id, const std::array<unsigned short, MAX_POINTS>& data);
};

class StorageManager {
public:
    static StorageManager& Instance() {
        static StorageManager instance;
        return instance;
    }

    StorageManager(const StorageManager&) = delete;

    StorageManager& operator=(const StorageManager&) = delete;

    StorageManager();

    void Initialize(const std::string& filename);

    void Clear() {
        event.timestamp = 0;
        event.id = 0;
        event.signal_ids.clear();
        event.signal_data.clear();
    }

    Long64_t GetNumberOfEntries() const {
        if (!event_tree) {
            return 0;
        }
        return event_tree->GetEntries();
    }

    void Checkpoint(bool force = false);

    std::unique_ptr<TFile> file;
    std::unique_ptr<TTree> event_tree;
    std::unique_ptr<TTree> run_tree;
    Event event;

    std::string compression_algorithm;

    unsigned long long run_number = 0;
    unsigned long long run_time_start = 0;

    std::string run_name;
    std::string run_tag;
    std::string run_detector_name;
    std::string run_comments;
    std::string run_commands;

    float run_drift_field_V_cm_bar = 0.0;
    float run_mesh_voltage_V = 0.0;
    float run_detector_pressure_bar = 0.0;

    unsigned long long millisSinceEpochForSpeedCalculation = 0;

    double GetSpeedEventsPerSecond() const;

    bool IsInitialized() const {
        return file != nullptr;
    }

private:
    std::chrono::time_point<std::chrono::system_clock> lastCheckpointTime = std::chrono::system_clock::now();
};

} // namespace mclient_storage

#endif // MCLIENT_STORAGE_H
