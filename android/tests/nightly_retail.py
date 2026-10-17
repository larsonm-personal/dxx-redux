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
                 'reason': phase + ': ' + status,
                 'run_url': 'https://github.com/' + REPOSITORY + '/actions/runs/' + run_id,
                 'coverage_complete': row.get('coverage_complete') is True,
                 'attempted_tests': row.get('attempted_tests') is True,
                 'retryable': row.get('retryable') is not False,
                 'blocker_kind': row.get('blocker_kind') if row.get('blocker_kind') in (
                     'infrastructure', 'target-capability', 'test-timeout',
                     'metadata-cli-setup', 'corpus-worker-configure', 'corpus-worker-build') else '',
                 'cases': [{'status': case['status']} for case in row.get('cases', []) if case.get('status') in ('PASS', 'FAIL', 'SKIP', 'BLOCKED')]}
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
