#pragma once

#include <JuceHeader.h>
#include "ExportSettings.h"
#include "OfflineRenderer.h"
#include "../Canvas/CanvasModel.h"
#include "../Audio/AudioEngine.h"

//==============================================================================
/// Manages a queue of video export jobs.  Runs them sequentially, notifying
/// listeners on progress and completion of each job.
///
/// Usage:
///   BatchExporter batch(canvasModel, audioEngine);
///   batch.addJob(settings1);
///   batch.addJob(settings2);
///   batch.startAll();
class BatchExporter : public OfflineRenderer::Listener
{
public:
    //-- Export job descriptor  ------------------------------------------------
    struct Job
    {
        Export::Settings settings;
        enum class State { Pending, Running, Done, Failed, Cancelled };
        State  state = State::Pending;
        float  progress = 0.0f;
        juce::String resultMessage;
    };

    //-- Listener  -------------------------------------------------------------
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void batchJobProgress(int jobIndex, float progress) {}
        virtual void batchJobFinished(int jobIndex, bool success,
                                      const juce::String& msg) {}
        virtual void batchAllFinished() {}
    };

    BatchExporter(CanvasModel& model, AudioEngine& engine);
    ~BatchExporter();

    /// Add a job to the queue.
    void addJob(const Export::Settings& settings);

    /// Remove all pending jobs.
    void clearJobs();

    /// Number of jobs.
    int getNumJobs() const { return static_cast<int>(jobs_.size()); }
    const Job& getJob(int i) const { return jobs_[static_cast<size_t>(i)]; }

    /// Start processing the queue from the first pending job.
    void startAll();

    /// Cancel the current job and all remaining.
    void cancelAll();

    bool isRunning() const { return currentRenderer_ != nullptr; }

    void addListener(Listener* l)    { listeners_.add(l); }
    void removeListener(Listener* l) { listeners_.remove(l); }

    //-- OfflineRenderer::Listener  --------------------------------------------
    void renderingProgress(float progress, int curFrame, int totalFrames,
                           double eta) override;
    void renderingFinished(bool success, const juce::String& msg) override;

private:
    CanvasModel&  canvasModel_;
    AudioEngine&  audioEngine_;

    std::vector<Job>  jobs_;
    int               currentJobIndex_ = -1;
    std::unique_ptr<OfflineRenderer> currentRenderer_;
    bool              cancelRequested_ = false;

    juce::ListenerList<Listener> listeners_;

    void startNextJob();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BatchExporter)
};
