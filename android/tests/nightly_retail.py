"""Small trusted-entry and receipt boundary for optional base-retail fixtures."""
import os
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'helpers'))
from retail_ci_assets import baseline_identity

REPOSITORY = 'larsonm-personal/dxx-redux'
ASSET_REPOSITORY = 'larsonm-personal/dxx-redux-test-assets'
WORKFLOW = REPOSITORY + '/.github/workflows/nightly-tooling-sweep.yml@refs/heads/cmake'


def trusted_context(env=None):
    env = os.environ if env is None else env
    return (env.get('GITHUB_REPOSITORY') == REPOSITORY
            and env.get('GITHUB_REF') == 'refs/heads/cmake'
            and env.get('GITHUB_WORKFLOW_REF') == WORKFLOW
            and env.get('GITHUB_EVENT_NAME') in ('schedule', 'workflow_dispatch'))


def configured_identity(env=None):
    env = os.environ if env is None else env
    commit = env.get('RETAIL_ASSETS_COMMIT', '')
    if not trusted_context(env) or not commit:
        return None
    if not re.fullmatch('[0-9a-f]{40}', commit):
        raise ValueError('Retail assets require an immutable forty-character commit')
    return {'repository': ASSET_REPOSITORY, 'commit': commit,
            'manifest_sha256': baseline_identity(Path(__file__).resolve().parents[2], include_missions=True)}


def validate_identity(identity):
    if identity is None:
        return
    if (not isinstance(identity, dict) or set(identity) != {'repository', 'commit', 'manifest_sha256'}
            or identity['repository'] != ASSET_REPOSITORY
            or not re.fullmatch('[0-9a-f]{40}', identity.get('commit', ''))
            or identity['manifest_sha256'] != baseline_identity(Path(__file__).resolve().parents[2], include_missions=True)):
        raise ValueError('Unapproved retail asset identity')


def lane_requires_retail(plan, lane):
    return any(entry['lane'] == lane and entry.get('fixture') in ('retail-base', 'retail-missions', 'retail-demos', 'retail-corpus') for entry in plan['tests'])


# Only source-defined messages may cross the retail boundary. Never publish a
# substring of arbitrary exception text or a raw command/output stream.
PUBLIC_REASONS = frozenset((
    '7-Zip prerequisite unavailable',
    'Ambiguous corpus descriptor HOG override',
    'Ambiguous corpus variants have distinct payloads',
    'Corpus HOG override escapes the descriptor directory',
    'Corpus archive cannot be staged safely',
    'Corpus archive entries exceed safe limits or collide',
    'Corpus archive extraction exceeded safe limits',
    'Corpus archive extraction failed',
    'Corpus archive file overlaps a directory',
    'Corpus archive format requires reviewed extraction support',
    'Corpus archive links or encrypted entries are unsupported',
    'Corpus archive listing is ambiguous',
    'Corpus descriptor identity mismatch',
    'Corpus extraction produced a linked or special file',
    'Corpus route stage already exists',
    'Corpus source contains links',
    'Corpus source has case-colliding paths',
    'Corpus source or destination contains links',
    'Descriptor-associated corpus HOG unavailable',
    'Invalid corpus mission descriptor',
    'Selected corpus descriptor unavailable',
    'Unsafe corpus archive path',
    'Unsafe corpus extraction destination or archive',

    'Corpus selected mission identity is ambiguous',
    'Committed level is explicitly missing or unsupported',
    'Archive or CD-source fixture unavailable',
    'Committed route oracle unavailable',
    'Nested mission variant requires a variant-aware regression stage',
    'Reviewed corpus revision is not staged',
    'Metadata projection CLI setup failed',
    'Corpus worker configuration failed',
    'Corpus worker build failed',
    'Missing metadata baseline, generated output or summary',
    'Selected archive missing or duplicated in owner summary',
    'Metadata owner reported failed',
    'Metadata owner did not execute selected archive successfully',
    'Invalid or empty projected mission metadata',
    'Invalid matcen comparison evidence',
    'Generated mission contains absent or empty level geometry',
    'Generated mission metadata contains a non-ok outcome',
    'Baseline mission metadata is not an ok oracle',
    'Full projected metadata differs from committed baseline',
    'Projected metadata semantics match committed baseline',
    'Corpus process failed despite matching output',
    'Suite exceeded its wall-clock timeout',
    'Suite cancelled; child process group terminated',
))
COMMANDS = frozenset(('cmake', 'bash', 'pwsh', 'python', 'other',
    'regenerate_all_mission_metadata_host.ps1', 'regenerate_all_guidebot_simulations.ps1', 'gradlew'))


def public_diagnostics(value):
    clean = {}
    for key in ("elapsed_seconds", "elapsed_wall_seconds", "setup_seconds"):
        number = value.get(key)
        if type(number) in (int, float) and 0 <= number <= 86400:
            clean[key] = round(number, 3)
    if value.get("exception_type") in ("OSError", "FileNotFoundError", "PermissionError", "ValueError", "JSONDecodeError", "ParseError"):
        clean["exception_type"] = value["exception_type"]
    clean["commands"] = []
    for command in value.get("commands", [])[:64]:
        row = {
            key: command[key] for key in ("exit_code", "stdout_bytes", "stderr_bytes") if type(command.get(key)) is int
        }
        row["command"] = command.get("command") if command.get("command") in COMMANDS else "other"
        row["outcome"] = (
            command.get("outcome")
            if command.get("outcome") in ("running", "completed", "timeout", "exception", "cancelled")
            else "exception"
        )
        duration = command.get("duration_seconds")
        if type(duration) in (int, float) and 0 <= duration <= 86400:
            row["duration_seconds"] = round(duration, 3)
        row["phase"] = "setup" if command.get("phase") == "setup" else "tests"
        clean["commands"].append(row)
    from nightly_regression_corpus import DIAGNOSTIC_FIELDS, WORKER_CATEGORIES

    def worker_row(value):
        row = {
            key: value[key]
            for key in ("exit_code", "shutdown_ms", "stdout_bytes", "stderr_bytes")
            if type(value.get(key)) is int
        }
        row.update(
            {
                key: value[key]
                for key in ("timed_out", "stdout_complete", "stderr_complete", "shutdown_missing")
                if type(value.get(key)) is bool
            }
        )
        for stream in ("categories", "stdout_categories", "stderr_categories"):
            row[stream] = [
                category
                for category in value.get(stream, [])[:16]
                if isinstance(category, str) and category in WORKER_CATEGORIES
            ]
        if value.get("worker") in ("d1-metadata", "d2-metadata", "other"):
            row["worker"] = value["worker"]
        return row
    clean['workers'] = [worker_row(row) for row in value.get('workers', [])[:16] if isinstance(row, dict)]
    if isinstance(value.get('owner_failure'), dict):
        clean['owner_failure'] = worker_row(value['owner_failure'])

    clean["mismatches"] = []
    for mismatch in value.get("mismatches", [])[:16]:
        if mismatch.get("kind") not in ("type", "missing-field", "length", "value"):
            continue
        path = mismatch.get("path", [])
        if not isinstance(path, list) or len(path) > 32:
            continue
        clean["mismatches"].append(
            {
                "kind": mismatch["kind"],
                "path": [
                    part
                    if (type(part) is int and 0 <= part <= 1000000)
                    or (isinstance(part, str) and part in DIAGNOSTIC_FIELDS)
                    else "field"
                    for part in path
                ],
            }
        )
    clean["assets"] = {
        key: value["assets"][key]
        for key in ("archive", "baseline", "route_oracle", "retail_stage_verified", "metadata_summary", "metadata_output")
        if type(value.get("assets", {}).get(key)) is bool
    }
    return clean


def public_receipt(receipt):
    """Publish fixed owner/status diagnostics, never arbitrary test strings or files."""
    result = {key: receipt[key] for key in ('schema', 'target_sha', 'catalog_hash', 'batch_id', 'lane')}
    if 'retail_assets' in receipt:
        result['retail_assets'] = receipt['retail_assets']
    result['results'] = []
    run_id = os.environ.get('GITHUB_RUN_ID', 'local')
    run_id = run_id if run_id.isdigit() else 'local'
    for row in receipt['results']:
        status = row['status'] if row.get('status') in ('PASS', 'FAIL', 'TIMEOUT', 'BLOCKED', 'NOT_RUN', 'RUNNING') else 'NOT_RUN'
        phase = row.get('phase') if row.get('phase') in ('preflight', 'setup', 'tests') else 'setup'
        clean = {'id': row['id'], 'status': status, 'phase': phase,
                 'reason': row['reason'] if row.get('reason') in PUBLIC_REASONS else phase + ': ' + status,
                 'run_url': 'https://github.com/' + REPOSITORY + '/actions/runs/' + run_id,
                 'coverage_complete': row.get('coverage_complete') is True,
                 'attempted_tests': row.get('attempted_tests') is True,
                 'retryable': row.get('retryable') is not False,
                 'blocker_kind': row.get('blocker_kind') if row.get('blocker_kind') in (
                     'infrastructure', 'target-capability', 'test-timeout',
                     'metadata-cli-setup', 'corpus-worker-configure', 'corpus-worker-build') else '',
                 'cases': [{'status': case['status']} for case in row.get('cases', []) if case.get('status') in ('PASS', 'FAIL', 'SKIP', 'BLOCKED')]}
        clean.update(public_diagnostics(row))
        clean['comparison_only'] = row.get('comparison_only') is True
        clean['runtime_outcome'] = row.get('runtime_outcome') if row.get('runtime_outcome') in ('ok', 'partial', 'failed', 'timeout', 'unsupported', 'route_mismatch') else ''
        if type(row.get('exit_code')) is int:
            clean['exit_code'] = row['exit_code']
        result['results'].append(clean)
    return result


def demos_available(identity):
    from private_input_demos import ASSETS_COMMIT
    from nightly_regression_catalog import CORPUS_COMMIT
    return bool(identity and identity.get('commit') in (ASSETS_COMMIT, CORPUS_COMMIT))


def corpus_available(identity):
    from nightly_regression_catalog import CORPUS_COMMIT
    return bool(identity and identity.get('commit') == CORPUS_COMMIT)
