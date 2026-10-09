/*
 * Audacity: A Digital Audio Editor
 */
#include "testflowstepper.h"

#include <algorithm>
#include <iostream>
#include <string>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <QSocketNotifier>
#include <QtGlobal>

#include "framework/testflow/internal/scriptengine.h"

using namespace au::app;
using namespace muse::testflow;

namespace {
//! NOTE 0 if unknown
int terminalColumns()
{
    const int fd = ::open("/dev/tty", O_RDONLY);
    if (fd < 0) {
        return 0;
    }
    winsize size {};
    const bool ok = ::ioctl(fd, TIOCGWINSZ, &size) == 0;
    ::close(fd);
    return ok ? size.ws_col : 0;
}
}

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
    printLine(m_testCaseName.toStdString());
    // E.g. set by run_testflow.sh after the first run of a series: anything else typed prints them again
    if (!qEnvironmentVariableIsSet("AU_TESTFLOW_STEPPER_NO_INSTRUCTIONS")) {
        printInstructions();
    }
    announceNextStep();
    pause();
}

void TestflowStepper::onStepStatusChanged(const StepInfo& step)
{
    if (step.status == StepStatus::Error || step.status == StepStatus::Aborted) {
        markStep(m_nextStep, false);
        return;
    }

    // Not counting Started, which unpausing sends again for the last step
    if (step.status != StepStatus::Finished && step.status != StepStatus::Skipped) {
        return;
    }

    markStep(m_nextStep, true);
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
        printLine("(no more input, playing all)");
        m_stdin->setEnabled(false);
        m_playAll = true;
        if (m_paused) {
            unpause();
        }
        return;
    }

    // The terminal echoed the command, Enter included
    ++m_linesBelowLabel;

    if (command.empty()) {
        if (m_playAll) {
            printLine("(playing all, p + Enter to pause)");
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
            printLine("(pausing after this step)");
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

void TestflowStepper::printInstructions()
{
    printLine("Enter: play next · a + Enter: play all · p + Enter: pause");
}

void TestflowStepper::printLine(const std::string& line)
{
    out() << line << std::endl;
    ++m_linesBelowLabel;
}

void TestflowStepper::announceNextStep()
{
    out() << stepLabel(m_nextStep).toStdString();
    // When paused, the Enter typed to play it ends the line
    if (m_playAll) {
        out() << std::endl;
        m_linesBelowLabel = 1;
    } else {
        // Not left in the buffer: the process ends without flushing it
        out() << std::flush;
        m_linesBelowLabel = 0;
    }
}

void TestflowStepper::markStep(int index, bool ok)
{
    if (!isTerminal()) {
        return;
    }

    static const std::string done = "\033[32m✓\033[0m";
    static const std::string failed = "\033[31m✗\033[0m";

    const QString label = stepLabel(index);

    // The terminal wraps a label wider than itself onto more rows
    const int columns = terminalColumns();
    const int labelRows = columns > 0 ? std::max(1, static_cast<int>((label.size() + columns - 1) / columns)) : 1;
    // Rather than spilling a mark that does not fit onto the line below
    if (columns > 0 && label.size() + 2 > labelRows * columns) {
        return;
    }

    // Saves the cursor, goes back up to the first row of the step's label, rewrites it with its mark,
    // and restores the cursor
    const int rowsUp = m_linesBelowLabel + labelRows - 1;
    out() << "\0337";
    if (rowsUp > 0) {
        out() << "\033[" << rowsUp << "A";
    }
    out() << "\r" << label.toStdString() << " " << (ok ? done : failed) << "\033[K\0338" << std::flush;
}

bool TestflowStepper::isTerminal() const
{
    out();
    return m_tty->is_open();
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
