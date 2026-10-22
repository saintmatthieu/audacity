/*
 * Audacity: A Digital Audio Editor
 *
 * TC3.1 step 1: open three projects, one per window, and leave them open.
 * Only the first has a track: actions go to the test's window, the first.
 *
 * The runner ends every test case without tearing down the app, so the
 * projects are never closed: for the next step, this run crashed.
 */

var u = require("steps/TestUtils.js");

function newProject() {
    u.command("command://project/new");
}

// The command responds once it has asked for a new window, which only then gets created, with its project
function newProjectInNewWindow() {
    newProject();
    u.sleep(2000);
}

var testCase = {
    name: "TC3.1.1: Open three projects",
    description: "Leaves three new projects open for the next steps to recover",
    steps: [
        u.step("Create first project, with a track", function () {
            newProject();
            u.command("command://trackedit/new-mono-track");
            u.eq(u.trackCount(), 1, "track count");
        }),
        u.step("Create second project", newProjectInNewWindow),
        u.step(
            "Create third project and terminate this run - it will exit like it crashed",
            newProjectInNewWindow,
        ),
    ],
};

function main() {
    api.testflow.setInterval(1000);
    api.testflow.runTestCase(testCase);
}
