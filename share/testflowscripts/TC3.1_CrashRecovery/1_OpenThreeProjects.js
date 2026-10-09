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
    u.run("file-new");
    u.sleep(2000);
}

var testCase = {
    name: "TC3.1.1: Open three projects",
    description: "Leaves three new projects open for the next steps to recover",
    steps: [
        u.step("First project, with a track", function () {
            newProject();
            u.run("new-mono-track");
            u.eq(u.trackCount(), 1, "track count");
        }),
        u.step("Second project", newProject),
        u.step("Third project", newProject),
    ],
};

function main() {
    api.testflow.setInterval(1000);
    api.testflow.runTestCase(testCase);
}
