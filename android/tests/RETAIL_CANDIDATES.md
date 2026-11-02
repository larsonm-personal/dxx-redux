# Known-account candidate qualification

This is a proposed policy, not authorization to run private inputs from this draft

The existing cmake workflow and retail-ci branch restriction remain mandatory. After separately authorized review and integration into cmake, Michael can dispatch that workflow with campaign xl-fast or xl-originals, candidate_pr, and the full candidate_sha copied from his own same-repository PR. Empty candidate inputs preserve existing behavior. Other campaigns are deliberately outside this initial exception

The authenticated GitHub profile identifies larsonm-personal as numeric account ID 265118235. The trusted controller checks that ID for the original dispatcher, current rerun actor, and PR author. It checks run identity, attempt, controller branch, repository ID 1171374860, open PR, same-repository non-fork head, and both live PR and branch SHA. Checks run during selection, planning, and again immediately before private checkout, including partial job reruns. API errors, missing identity fields, head changes and other rerun actors reject access

Supplying the exact SHA is an explicit endorsement of all code at that SHA, including commits by others. This does not claim Git author names or emails identify the authenticated pusher. A PR author alone grants nothing. A later head needs a fresh explicit dispatch; a race after validation still executes only the endorsed immutable SHA

Candidate catalog and setup scripts do not execute before private checkout. Catalog, policy, setup, staging and receipt validation come from the trusted controller. Candidate execution starts after staging and token-scoped checkout, with persist-credentials disabled. The existing immutable asset pin and sanitized public receipt boundary remain in place. The read-only asset token is not passed to candidate commands

This deliberately trusts the endorsed candidate code with the staged assets: it can read and potentially disclose them over the network. It is not a sandbox for malicious code, nor does sanitizing receipts prevent intentional exfiltration. The account must endorse the complete candidate, including build tools and dependencies. A compromised trusted account has the same authority

Activation requires a separately approved merge/integration of this policy into cmake and a subsequent explicitly authorized private qualification dispatch. No environment/security settings changes or new credentials are required. Do not dispatch this draft branch to evade the existing gate. Trusted controller receipt contracts must also support the selected candidate's test output; this policy does not import candidate validation rules or establish a successful XL smoke
