#include "BatchExporter.h"

//==============================================================================
BatchExporter::BatchExporter(CanvasModel& model, AudioEngine& engine)
    : canvasModel_(model), audioEngine_(engine)
{
}

BatchExporter::~BatchExporter()
{
    cancelAll();
}

//==============================================================================
void BatchExporter::addJob(const Export::Settings& settings)
{
    Job job;
    job.settings = settings;
    jobs_.push_back(std::move(job));
}

void BatchExporter::clearJobs()
{
    if (isRunning()) cancelAll();
    jobs_.clear();
    currentJobIndex_ = -1;
}

//==============================================================================
void BatchExporter::startAll()
{
    if (isRunning()) return;

    cancelRequested_ = false;
    currentJobIndex_ = -1;

    // Reset all jobs to Pending
    for (auto& j : jobs_)
    {
        j.state = Job::State::Pending;
        j.progress = 0.0f;
        j.resultMessage.clear();
    }

    startNextJob();
}

void BatchExporter::cancelAll()
{
    cancelRequested_ = true;

    if (currentRenderer_)
    {
        currentRenderer_->cancel();
        currentRenderer_->stopThread(10000);
        currentRenderer_.reset();
    }

    // Mark remaining as cancelled
    for (auto& j : jobs_)
    {
        if (j.state == Job::State::Pending || j.state == Job::State::Running)
        {
            j.state = Job::State::Cancelled;
            j.resultMessage = "Cancelled";
        }
    }
}

//==============================================================================
void BatchExporter::startNextJob()
{
    ++currentJobIndex_;

    // Find next pending job
    while (currentJobIndex_ < static_cast<int>(jobs_.size()))
    {
        if (jobs_[static_cast<size_t>(currentJobIndex_)].state == Job::State::Pending)
            break;
        ++currentJobIndex_;
    }

    if (currentJobIndex_ >= static_cast<int>(jobs_.size()) || cancelRequested_)
    {
        // All done
        currentRenderer_.reset();
        listeners_.call(&Listener::batchAllFinished);
        return;
    }

    auto& job = jobs_[static_cast<size_t>(currentJobIndex_)];
    job.state = Job::State::Running;

    currentRenderer_ = std::make_unique<OfflineRenderer>(
        job.settings, canvasModel_, audioEngine_);
    currentRenderer_->addListener(this);
    currentRenderer_->startThread();
}

//==============================================================================
void BatchExporter::renderingProgress(float progress, int /*curFrame*/,
                                       int /*totalFrames*/, double /*eta*/)
{
    if (currentJobIndex_ >= 0 && currentJobIndex_ < static_cast<int>(jobs_.size()))
    {
        jobs_[static_cast<size_t>(currentJobIndex_)].progress = progress;
        listeners_.call(&Listener::batchJobProgress, currentJobIndex_, progress);
    }
}

void BatchExporter::renderingFinished(bool success, const juce::String& msg)
{
    if (currentJobIndex_ >= 0 && currentJobIndex_ < static_cast<int>(jobs_.size()))
    {
        auto& job = jobs_[static_cast<size_t>(currentJobIndex_)];
        job.state = success ? Job::State::Done : Job::State::Failed;
        job.progress = success ? 1.0f : job.progress;
        job.resultMessage = msg;

        listeners_.call(&Listener::batchJobFinished, currentJobIndex_, success, msg);
    }

    // Clean up current renderer and start next
    if (currentRenderer_)
    {
        currentRenderer_->removeListener(this);
        currentRenderer_->stopThread(5000);
        currentRenderer_.reset();
    }

    if (!cancelRequested_)
        startNextJob();
    else
        listeners_.call(&Listener::batchAllFinished);
}
