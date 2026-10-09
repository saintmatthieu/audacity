/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <fstream>
#include <memory>

#include <QObject>
#include <QStringList>

#include "framework/global/async/asyncable.h"
#include "framework/global/modularity/ioc.h"
#include "framework/testflow/itestflow.h"

class QSocketNotifier;

namespace au::app {
//! NOTE Lets whoever runs a test case step through it from the terminal, without anything
//! on screen that could take focus from the app under test. It writes to the terminal itself, so that
//! the app's output can go to a log file instead. It starts paused, and then
//!  - Enter plays the next step,
//!  - `a` plays all remaining steps,
//!  - `p` pauses again after the step that is running, or about to.
class TestflowStepper : public QObject, public muse::async::Asyncable
{
public:
    TestflowStepper(const muse::modularity::ContextPtr& ctx, std::shared_ptr<muse::testflow::ITestflow> testflow,
                    const muse::io::path_t& scriptPath);

private:
    void readStepNames(const muse::modularity::ContextPtr& ctx, const muse::io::path_t& scriptPath);
    void onStatusChanged(muse::testflow::ITestflow::Status status);
    void onStepStatusChanged(const muse::testflow::StepInfo& step);
    void onCommand();

    void pause();
    void unpause();
    void printInstructions() const;
    void announceNextStep() const;
    QString stepLabel(int index) const;
    std::ostream& out() const;

    std::shared_ptr<muse::testflow::ITestflow> m_testflow;
    QSocketNotifier* m_stdin = nullptr;
    mutable std::unique_ptr<std::ofstream> m_tty;

    QString m_testCaseName;
    QStringList m_stepNames;
    int m_nextStep = 0;
    bool m_started = false;
    bool m_paused = false;
    bool m_playAll = false;
};
}
