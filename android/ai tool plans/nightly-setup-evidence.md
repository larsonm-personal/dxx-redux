# Nightly provisioning evidence and aggregate result

Run 42470885273 failed to download the pinned JVM soundfont before any JUnit
cases ran, but the owner journal recorded a terminal test failure. The other
lanes correctly retained provisioning failures as retryable blocks. Reconciliation
returned success while failures remained, which also controlled state publication.

1. Provision the existing checksum-verified soundfont before starting the JVM
   owner. Keep the original Gradle test command, XML requirements, and genuine
   assertion/build-failure behavior unchanged. Do not classify errors by log text.
2. Separate successful reconciliation from the aggregate failure gate. Fail the
   aggregate for retained failures, retryable blocks, or failed/cancelled lanes,
   while allowing intentionally unattempted work to stay pending. Publish valid
   reconciled progress even when the result gate is red.
3. Exercise the workflow snippets with an injected download failure and exercise
   collection, reconciliation, retry selection, assertion failures, aggregate
   outcomes, and state preservation. Run the existing sweep and catalog controls
   and scoped code quality before publication.

No dependency pin, download algorithm, pass criterion, historical receipt, or
published state is rewritten. The existing target/catalog reset handles a future
merged revision, including reselecting the previous head's misclassified JVM
owner. The current historical ledger remains untouched.
