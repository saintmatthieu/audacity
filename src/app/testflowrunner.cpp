/*
 * Audacity: A Digital Audio Editor
 */
#include "testflowrunner.h"

#include <cstdlib>
#include <iostream>
#include <utility>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>

#include "framework/testflow/itestflow.h"

#include "appshell/testflowstartup.h"
#include "effects/effects_base/ieffectsproviderinitializer.h"

#include "log.h"

using namespace au::app;

//! NOTE No signal says the UI is ready: startupCompleted() is set before the
//! startup page even opens, so the first step just has to wait
static constexpr int SETTLE_MS = 3000;

namespace {
//! NOTE Skips teardown, where actions queued by the last steps crash on
//! late-rejected Interactive::openSync promises. Flush your output first.
[[noreturn]] void exitWithoutTeardown(int code)
{
    std::_Exit(code);
}

//! NOTE Lives until the process ends, which it does itself
au::app::TestflowRunner* s_runner = nullptr;
}

void TestflowRunner::prepare(const muse::modularity::ContextPtr& ctx, const AudacityCmdOptions::Testflow& options)
{
    if (!options.testCaseRequested) {
        return;
    }

    if (options.testCaseNameOrFile.isEmpty()) {
        std::cout << "[testflow] FAILED: --test-case was given no test case" << std::endl;
        exitWithoutTeardown(2);
    }

    s_runner = new TestflowRunner(ctx, options);
    s_runner->prepare();
}

void TestflowRunner::runIfRequested()
{
    //! NOTE Taken, so that windows opened by the test case, which go through startup too, do not run it again
    if (TestflowRunner* runner = std::exchange(s_runner, nullptr)) {
        runner->run();
    }
}

TestflowRunner::TestflowRunner(const muse::modularity::ContextPtr& ctx, const AudacityCmdOptions::Testflow& options)
    : m_ctx(ctx), m_options(options)
{
}

void TestflowRunner::prepare()
{
    m_scriptPath = resolveScriptPath();
    if (m_scriptPath.empty()) {
        std::cout << "[testflow] FAILED: script not found: " << m_options.testCaseNameOrFile.toStdString() << std::endl;
        exitWithoutTeardown(2);
    }

    const ScriptOptions scriptOptions = readScriptOptions();

    au::appshell::installTestflowStartupScenario(m_ctx, scriptOptions.allowRecovery);

    auto effectsInitializer = muse::modularity::ioc(m_ctx)->resolve<au::effects::IEffectsProviderInitializer>("app");
    IF_ASSERT_FAILED(effectsInitializer) {
        return;
    }
    effectsInitializer->setStartupPluginValidationPolicy(au::effects::StartupPluginValidationPolicy::Skip);
}

TestflowRunner::ScriptOptions TestflowRunner::readScriptOptions() const
{
    QFile file(m_scriptPath.toQString());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cout << "[testflow] FAILED: cannot read script: " << m_scriptPath.toStdString() << std::endl;
        exitWithoutTeardown(2);
    }

    ScriptOptions options;

    //! NOTE Like Jest's pragmas, only the docblock the script starts with counts
    static const QRegularExpression docblock(R"(^\s*/\*.*?\*/)", QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch header = docblock.match(QString::fromUtf8(file.readAll()));
    if (!header.hasMatch()) {
        return options;
    }

    static const QRegularExpression tag(R"(@testflow\s+(\S+))");
    QRegularExpressionMatchIterator it = tag.globalMatch(header.captured());
    while (it.hasNext()) {
        const QString option = it.next().captured(1);
        if (option == "allow-recovery") {
            options.allowRecovery = true;
        } else {
            //! NOTE Rather than letting a typo make the test case fail for another reason
            std::cout << "[testflow] FAILED: unknown option @testflow " << option.toStdString()
                      << " in script: " << m_scriptPath.toStdString() << std::endl;
            exitWithoutTeardown(2);
        }
    }

    return options;
}

muse::io::path_t TestflowRunner::resolveScriptPath() const
{
    const QString& nameOrFile = m_options.testCaseNameOrFile;
    if (QFileInfo::exists(nameOrFile)) {
        return muse::io::path_t(QFileInfo(nameOrFile).absoluteFilePath());
    }

    for (const muse::io::path_t& dir : configuration()->scriptsDirPaths()) {
        for (const QString& candidate : { nameOrFile, nameOrFile + ".js" }) {
            const QString path = QDir::cleanPath(dir.toQString() + "/" + candidate);
            if (QFileInfo::exists(path)) {
                return muse::io::path_t(path);
            }
        }
    }

    return {};
}

void TestflowRunner::run()
{
    std::cout << "[testflow] running script=" << m_scriptPath.toStdString() << std::endl;

    QTimer::singleShot(SETTLE_MS, qApp, [this]() {
        execAndReport();
    });
}

void TestflowRunner::execAndReport()
{
    auto testflow = muse::modularity::ioc(m_ctx)->resolve<muse::testflow::ITestflow>("app");
    IF_ASSERT_FAILED(testflow) {
        exitWithoutTeardown(2);
    }

    if (!m_options.testCaseSpeed.isEmpty()) {
        testflow->setSpeedMode(muse::testflow::speedModeFromString(m_options.testCaseSpeed));
    }

    muse::testflow::ITestflow::Options opt;
    opt.context = muse::io::path_t(m_options.testCaseContextNameOrFile);
    opt.contextVal = m_options.testCaseContextValue.toStdString();
    opt.func = m_options.testCaseFunc.toStdString();
    opt.funcArgs = m_options.testCaseFuncArgs.toStdString();

    //! NOTE A script that fails to load, or never runs a test case, still ends
    //! up Finished, so count the steps to tell that apart
    testflow->stepStatusChanged().onReceive(this, [this](const muse::testflow::StepInfo& step, const muse::Ret&) {
        if (step.status == muse::testflow::StepStatus::Started) {
            ++m_startedSteps;
        }
    });

    testflow->execScript(m_scriptPath, opt);

    const muse::testflow::ITestflow::Status status = testflow->status();
    const bool ok = status == muse::testflow::ITestflow::Status::Finished && m_startedSteps > 0;

    std::cout << "[testflow] " << (ok ? "PASSED" : "FAILED")
              << " status=" << muse::testflow::ITestflow::statusToString(status).toStdString()
              << " steps=" << m_startedSteps
              << " script=" << m_scriptPath.toStdString()
              << " reports=" << configuration()->reportsPath().toStdString() << std::endl;
    if (m_startedSteps == 0) {
        std::cout << "[testflow] no steps were executed"
                  << " - does the script define main() and run a test case?" << std::endl;
    }

    exitWithoutTeardown(ok ? 0 : 1);
}
