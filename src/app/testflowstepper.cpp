/*
 * Audacity: A Digital Audio Editor
 */
#include "testflowstepper.h"

#include <iostream>
#include <string>

#include <unistd.h>

#include <QSocketNotifier>

#include "framework/testflow/internal/scriptengine.h"

using namespace au::app;
using namespace muse::testflow;

TestflowStepper::TestflowStepper(const muse::modularity::ContextPtr& ctx, std::shared_ptr<ITestflow> testflow,
                                 const muse::io::path_t& scriptPath)
    : m_testflow(std::move(testflow))
{
    readStepNames(ctx, scriptPath);

    m_stdin = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, this);
    connect(m_stdin, &QSocketNotifier::activated, this, [this]() { onCommand(); });

    m_testflow->statusChanged().onReceive(this, [this](const muse::io::path_t&, ITestflow::Status status) {
        onStatusChanged(status);
    });
    m_testflow->stepStatusChanged().onReceive(this, [this](const StepInfo& step, const muse::Ret&) {
        onStepStatusChanged(step);
    });
}

void TestflowStepper::readStepNames(const muse::modularity::ContextPtr& ctx, const muse::io::path_t& scriptPath)
{
    //! NOTE The runner only names a step once it starts: get them all upfront, as the Testflow panel does
    ScriptEngine engine(ctx);
    engine.setScriptPath(scriptPath);
    if (!engine.evaluate()) {
        return;
    }

    const TestCase testCase(engine.globalProperty(TESTCASE_JS_GLOBALNAME));
    if (!testCase.isValid()) {
        return;
    }

    m_testCaseName = testCase.name();
    const Steps steps = testCase.steps();
    for (int i = 0; i < steps.count(); ++i) {
        m_stepNames << steps.step(i).name();
    }
}

void TestflowStepper::onStatusChanged(ITestflow::Status status)
{
    // Pausing as soon as the script runs, the runner does not schedule the first step.
    // Unpausing sets Running again, hence only the first time
    if (status != ITestflow::Status::Running || m_started) {
        return;
    }

    m_started = true;
    out() << m_testCaseName.toStdString() << std::endl;
    printInstructions();
    announceNextStep();
    pause();
}

void TestflowStepper::onStepStatusChanged(const StepInfo& step)
{
    // Not counting Started, which unpausing sends again for the last step
    if (step.status != StepStatus::Finished && step.status != StepStatus::Skipped) {
        return;
    }

    ++m_nextStep;
    // Done after the last step, if the names, hence their count, could be read
    if (!m_stepNames.isEmpty() && m_nextStep >= m_stepNames.size()) {
        return;
    }

    announceNextStep();

    // Pausing now, once the step is done, keeps the runner from scheduling the next one
    if (!m_playAll) {
        pause();
    }
}

void TestflowStepper::onCommand()
{
    std::string command;
    if (!std::getline(std::cin, command)) {
        // E.g. not run from a terminal: rather than waiting forever
        out() << "(no more input, playing all)" << std::endl;
        m_stdin->setEnabled(false);
        m_playAll = true;
        if (m_paused) {
            unpause();
        }
        return;
    }

    if (command.empty()) {
        if (m_playAll) {
            out() << "(playing all, p + Enter to pause)" << std::endl;
        } else if (m_paused) {
            unpause();
        }
    } else if (command == "a") {
        m_playAll = true;
        if (m_paused) {
            unpause();
        }
    } else if (command == "p") {
        if (m_playAll) {
            m_playAll = false;
            out() << "(pausing after this step)" << std::endl;
        }
    } else {
        printInstructions();
    }
}

void TestflowStepper::pause()
{
    m_paused = true;
    m_testflow->pause();
}

void TestflowStepper::unpause()
{
    m_paused = false;
    m_testflow->unpause();
}

void TestflowStepper::printInstructions() const
{
    out() << "Enter: play next · a + Enter: play all · p + Enter: pause" << std::endl;
}

void TestflowStepper::announceNextStep() const
{
    out() << stepLabel(m_nextStep).toStdString() << std::endl;
}

std::ostream& TestflowStepper::out() const
{
    if (!m_tty) {
        m_tty = std::make_unique<std::ofstream>("/dev/tty");
    }
    // E.g. not run from a terminal
    if (!m_tty->is_open()) {
        return std::cout;
    }
    return *m_tty;
}

QString TestflowStepper::stepLabel(int index) const
{
    if (index >= m_stepNames.size()) {
        return QString("%1/?").arg(index + 1);
    }
    return QString("%1/%2: \"%3\"").arg(index + 1).arg(m_stepNames.size()).arg(m_stepNames.at(index));
}
