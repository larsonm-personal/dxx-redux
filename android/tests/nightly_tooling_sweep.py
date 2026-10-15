#!/usr/bin/env python3
"""Exact-revision nightly batches; artifacts contain data, never executable commands."""
import argparse
import base64
import urllib.request
import urllib.error
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time

SCHEMA = 1
LANES = ('host', 'jvm', 'android-single', 'android-dual')
TERMINAL = {'PASS', 'FAIL', 'TIMEOUT'}
COVERED = {'PASS', 'FAIL', 'TIMEOUT'}
SHA = re.compile(r'^[0-9a-f]{40}$')


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def read(path):
    p = Path(path)
    if p.stat().st_size > 8 * 1024 * 1024:
        raise ValueError('JSON exceeds bounded size')
    return json.loads(p.read_text())


def write(path, value):
    p = Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    tmp = p.with_suffix(p.suffix + '.partial')
    tmp.write_text(json.dumps(value, indent=2) + '\n')
    tmp.replace(p)


def policy_entries(policy):
    if policy.get('schema') != SCHEMA or not isinstance(policy.get('suites'), list):
        raise ValueError('Invalid policy')
    entries = {}
    for entry in policy['suites']:
        ident = entry.get('id')
        if not isinstance(ident, str) or not re.fullmatch(r'[A-Za-z0-9_.:#-]{1,160}', ident) or ident in entries:
            raise ValueError('Invalid/duplicate suite ID')
        if entry.get('lane') not in LANES or entry.get('owner') not in ('master', 'host-adapter', 'shareware'):
            raise ValueError('Unknown lane/owner')
        if type(entry.get('estimated_seconds')) is not int or not 1 <= entry['estimated_seconds'] <= 2700:
            raise ValueError('Estimate outside nightly budget')
        expected_args = {'suite', 'game'} if entry['owner'] == 'shareware' else {'suite'}
        if not isinstance(entry.get('args'), dict) or set(entry['args']) != expected_args or not isinstance(entry['args']['suite'], str) or not re.fullmatch(r'[A-Za-z0-9_.:-]{1,160}', entry['args']['suite']):
            raise ValueError('Only typed exact suite selection is accepted')
        if entry['owner'] == 'shareware' and (entry['args']['game'] not in ('d1', 'd2') or entry['lane'] != 'android-single'):
            raise ValueError('Invalid public gameplay lane/game')
        entries[ident] = entry
    return entries


def validate_state(state):
    if state.get('schema') != SCHEMA or not SHA.fullmatch(state.get('target_sha', '')):
        raise ValueError('Invalid ledger schema/revision')
    if not re.fullmatch(r'[0-9a-f]{64}', state.get('catalog_hash', '')) or not isinstance(state.get('records'), dict):
        raise ValueError('Invalid ledger catalog/records')
    if not isinstance(state.get('retryable', {}), dict) or not isinstance(state.get('excluded', {}), dict):
        raise ValueError('Invalid retryable observations')
    for ident, record in state['records'].items():
        if not isinstance(ident, str) or record.get('status') not in TERMINAL or not isinstance(record.get('run_url'), str):
            raise ValueError('Invalid ledger receipt')


def fresh_state(sha, catalog_hash):
    return {'schema': SCHEMA, 'target_sha': sha, 'catalog_hash': catalog_hash, 'records': {}, 'retryable': {}, 'excluded': {}}


def plan_batch(sha, catalog, policy, state=None, budget=2700):
    if not SHA.fullmatch(sha) or type(budget) is not int or not 1 <= budget <= 2700:
        raise ValueError('Invalid revision/budget')
    entries = policy_entries(policy)
    if catalog.get('schema') != SCHEMA or not isinstance(catalog.get('tests'), list):
        raise ValueError('Invalid master discovery catalog')
    names = {row['name'] for row in catalog['tests']}
    catalog_hash = digest({'catalog': catalog, 'policy': policy})
    if state is not None:
        validate_state(state)
    if state is None or (state['target_sha'], state['catalog_hash']) != (sha, catalog_hash):
        state = fresh_state(sha, catalog_hash)
    if not (set(state['records']) | set(state.get('retryable', {})) | set(state.get('excluded', {}))) <= set(entries):
        raise ValueError('Ledger contains unknown suite')
    selected, excluded = [], list(policy.get('exclusions', []))
    admitted = {e['args']['suite'] for e in entries.values() if e['owner'] == 'master'}
    excluded.extend({'id': row['name'], 'reason': 'not yet admitted: public prerequisites/host support require qualification', 'requires': row.get('requires'), 'manual': row.get('manual')} for row in catalog['tests'] if row['name'] not in admitted)
    used = 0
    # Untried work precedes repeatedly blocked setup, so one lane cannot starve the rest.
    ordered = sorted(entries.items(), key=lambda item: (
        state.get('retryable', {}).get(item[0], {}).get('attempts', 0), digest([sha, item[0]])))
    for ident, entry in ordered:
        if entry['owner'] == 'master' and entry['args']['suite'] not in names:
            excluded.append({'id': ident, 'reason': 'not present in pinned master catalog'})
            continue
        if ident in state['records'] or ident in state.get('excluded', {}):
            continue
        if used + entry['estimated_seconds'] <= budget:
            selected.append(entry)
            used += entry['estimated_seconds']
    identity = {'target_sha': sha, 'catalog_hash': catalog_hash, 'tests': selected}
    plan = {'schema': SCHEMA, **identity, 'batch_id': digest(identity), 'budget_seconds': budget,
            'estimated_seconds': used, 'exclusions': excluded,
            'lanes': [lane for lane in LANES if any(e['lane'] == lane for e in selected)]}
    return plan, state


def validate_plan(plan, policy):
    entries = policy_entries(policy)
    if plan.get('schema') != SCHEMA or not SHA.fullmatch(plan.get('target_sha', '')):
        raise ValueError('Invalid plan revision/schema')
    if not re.fullmatch(r'[0-9a-f]{64}', plan.get('catalog_hash', '')):
        raise ValueError('Invalid plan catalog hash')
    selected = plan.get('tests')
    if not isinstance(selected, list) or len(selected) > 1024:
        raise ValueError('Invalid plan selection')
    seen = set()
    for entry in selected:
        ident = entry.get('id')
        if ident in seen or entries.get(ident) != entry:
            raise ValueError('Unapproved or duplicate plan entry')
        seen.add(ident)
    if sum(e['estimated_seconds'] for e in selected) > plan.get('budget_seconds', 0) or not 1 <= plan.get('budget_seconds', 0) <= 2700:
        raise ValueError('Plan exceeds budget')
    identity = {key: plan[key] for key in ('target_sha', 'catalog_hash', 'tests')}
    if digest(identity) != plan.get('batch_id'):
        raise ValueError('Plan batch identity mismatch')


def reconcile(plan, policy, state, receipts):
    validate_plan(plan, policy)
    validate_state(state)
    if any(state[key] != plan[key] for key in ('target_sha', 'catalog_hash')):
        raise ValueError('Ledger/plan revision mismatch')
    allowed = {e['id']: e for e in plan['tests']}
    result = json.loads(json.dumps(state))
    seen = {}
    for receipt in receipts:
        if any(receipt.get(key) != plan[key] for key in ('target_sha', 'catalog_hash', 'batch_id')) or receipt.get('schema') != SCHEMA:
            raise ValueError('Receipt provenance mismatch')
        lane = receipt.get('lane')
        for row in receipt.get('results', []):
            ident = row.get('id')
            if ident not in allowed or allowed[ident]['lane'] != lane:
                raise ValueError('Unplanned receipt ID/lane')
            if ident in seen and seen[ident] != row:
                raise ValueError('Conflicting duplicate receipt')
            seen[ident] = row
            if row.get('status') == 'BLOCKED':
                if not isinstance(row.get('run_url'), str) or not row['run_url'].startswith('https://github.com/'):
                    raise ValueError('Missing infrastructure run provenance')
                if row.get('retryable') is False and row.get('blocker_kind') == 'target-capability':
                    result.setdefault('excluded', {})[ident] = {'reason': str(row.get('reason', ''))[:512], 'run_url': row['run_url']}
                    result.setdefault('retryable', {}).pop(ident, None)
                    continue
                previous = result.setdefault('retryable', {}).get(ident, {})
                result['retryable'][ident] = {'reason': str(row.get('reason', ''))[:512], 'run_url': row['run_url'],
                    'attempts': previous.get('attempts', 0) + int(previous.get('run_url') != row['run_url'])}
                continue
            if row.get('status') not in TERMINAL:
                if row.get('status') not in ('RUNNING', 'NOT_RUN'):
                    raise ValueError('Unknown result status')
                continue
            if not isinstance(row.get('run_url'), str) or not row['run_url'].startswith('https://github.com/'):
                raise ValueError('Missing run provenance')
            compact = {key: row[key] for key in ('status', 'run_url') }
            compact.update(reason=str(row.get('reason', ''))[:512], coverage_complete=row.get('coverage_complete') is True, attempted_tests=row.get('attempted_tests', True), phase=row.get('phase', 'tests'))
            cases = row.get('cases', [])
            compact['case_counts'] = {status: sum(c.get('status') == status for c in cases) for status in ('PASS', 'FAIL', 'SKIP', 'BLOCKED')}
            compact['exclusions'] = [str(x)[:256] for x in row.get('exclusions', [])[:32]]
            prior = result['records'].get(ident)
            if prior and prior != compact:
                raise ValueError('Conflicting completed receipt')
            result['records'][ident] = compact
            result.setdefault('retryable', {}).pop(ident, None)
    return result


def collect(plan, policy, lane, output, blocked_reason=""):
    validate_plan(plan, policy)
    output = Path(output)
    receipt = {key: plan[key] for key in ('schema', 'target_sha', 'catalog_hash', 'batch_id')}
    receipt.update(lane=lane, results=[])
    run_url = os.environ.get('GITHUB_SERVER_URL', 'https://github.com') + '/' + os.environ.get('GITHUB_REPOSITORY', 'local/unpublished') + '/actions/runs/' + os.environ.get('GITHUB_RUN_ID', 'local')
    observations = {}
    journal = output / 'journal.jsonl'
    if journal.exists():
        for line in journal.read_text().splitlines():
            try:
                row = json.loads(line)
            except json.JSONDecodeError:
                continue
            if row.get('commit') != plan['target_sha']:
                raise ValueError('Journal target mismatch')
            observations[row['name']] = row
    for entry in plan['tests']:
        if entry['lane'] != lane:
            continue
        if entry['owner'] == 'master':
            row = observations.get(entry['args']['suite'], {})
            status = row.get('status', 'NOT_RUN')
            if status == 'SKIP':
                status = 'BLOCKED'
        else:
            path = output / (entry['id'].replace(':', '_') + '.json')
            row = read(path) if path.exists() else {}
            status = row.get('status', 'NOT_RUN')
            if row and (row.get('schema') != 1 or row.get('suite') != entry['args']['suite']):
                raise ValueError('Invalid adapter receipt')
        if row.get('blocker_kind') == 'interrupted':
            status = 'NOT_RUN'
        if row.get('attempted_tests') is False and status in ('FAIL', 'TIMEOUT'):
            status = 'BLOCKED'
        if status == 'NOT_RUN' and blocked_reason:
            status = 'BLOCKED'
            row = {'reason': blocked_reason}
        if status not in TERMINAL | {'RUNNING', 'NOT_RUN', 'BLOCKED'}:
            raise ValueError('Invalid collected status')
        receipt['results'].append({'id': entry['id'], 'status': status, 'reason': row.get('reason', '')[:2048], 'run_url': run_url, 'exclusions': row.get('exclusions', []), 'cases': row.get('cases', []), 'coverage_complete': row.get('coverage_complete', False), 'attempted_tests': row.get('attempted_tests', status in TERMINAL), 'phase': row.get('phase', 'tests' if status in TERMINAL else 'setup'), 'retryable': row.get('retryable', status == 'BLOCKED'), 'blocker_kind': row.get('blocker_kind', 'infrastructure' if status == 'BLOCKED' else '')})
    write(output / 'results.json', receipt)
    return receipt


def run_command(command, cwd, log, timeout=None):
    process = subprocess.Popen(command, cwd=cwd, stdout=log, stderr=subprocess.STDOUT,
                               start_new_session=os.name != 'nt')
    try:
        process.wait(timeout=timeout)
        return process
    except BaseException:
        if os.name != 'nt':
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
        else:
            process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            if os.name != 'nt':
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            else:
                process.kill()
            process.wait(timeout=5)
        if os.name != 'nt':
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        raise


def execute(plan, policy, lane, repo, output):
    validate_plan(plan, policy)
    repo, output = Path(repo).resolve(), Path(output).resolve()
    actual = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != plan['target_sha']:
        raise ValueError('Checkout does not match pinned target')
    output.mkdir(parents=True, exist_ok=True)
    selected = [e for e in plan['tests'] if e['lane'] == lane]
    receipt = {key: plan[key] for key in ('schema', 'target_sha', 'catalog_hash', 'batch_id')}
    receipt.update(lane=lane, results=[])
    run_url = os.environ.get('GITHUB_SERVER_URL', 'https://github.com') + '/' + os.environ.get('GITHUB_REPOSITORY', 'local/unpublished') + '/actions/runs/' + os.environ.get('GITHUB_RUN_ID', 'local')
    def save(entry, status, reason='', **detail):
        receipt['results'].append({'id': entry['id'], 'status': status, 'reason': reason[:2048], 'run_url': run_url, **detail})
        write(output / 'execution-progress.json', receipt)
    write(output / 'execution-progress.json', receipt)
    masters = [e for e in selected if e['owner'] == 'master']
    if masters:
        names = output / 'selected.json'
        write(names, [e['args']['suite'] for e in masters])
        journal = output / 'journal.jsonl'
        command = ['pwsh', '-NoProfile', '-File', str(repo / 'android/run_all_tests.ps1'), '-SelectedTestsPath', str(names), '-EvidenceJournalPath', str(journal), '-TestBudgetSeconds', str(sum(e['estimated_seconds'] for e in masters)), '-FullSuite', '-FullExtracts', '-FullRouteCorpus', '-ExtendedGraphics', '-ExtendedMultiplayer', '-ReportDir', str(output / 'master')]
        if lane in ('host', 'jvm'):
            command.append('-HostOnly')
        # Master owns setup/cleanup and per-test timeout; outer hosted cancellation remains pending.
        with (output / 'master.log').open('w') as log:
            process = run_command(command, repo, log)
        write(output / 'master-process.json', {'exit_code': process.returncode})
        observations = {}
        if journal.exists():
            for line in journal.read_text().splitlines():
                try:
                    row = json.loads(line)
                except json.JSONDecodeError:
                    continue  # an interrupted append cannot manufacture completion
                if row.get('commit') != actual:
                    raise ValueError('Journal target mismatch')
                observations[row['name']] = row
        for entry in masters:
            row = observations.get(entry['args']['suite'], {})
            status = row.get('status', 'NOT_RUN')
            if status == 'SKIP':
                status = 'BLOCKED'
            save(entry, status, row.get('reason', ''), exit_code=row.get('exit_code'))
    elapsed = 0.0
    for entry in selected:
        if entry['owner'] != 'master' and elapsed >= sum(e['estimated_seconds'] for e in selected if e['owner'] != 'master'):
            save(entry, 'NOT_RUN', 'test execution budget exhausted')
            continue
        if entry['owner'] == 'shareware':
            destination = output / (entry['id'].replace(':', '_') + '.json')
            command = ['pwsh', '-NoProfile', '-File', str(repo / 'android/helpers/run_shareware_tests.ps1'), '-Filter', entry['args']['suite'], '-Game', entry['args']['game']]
            serial = os.environ.get('DXX_TEST_PRIMARY_SERIAL')
            if serial:
                command += ['-Serial', serial]
            start = time.monotonic()
            with destination.with_suffix('.log').open('w') as log:
                try:
                    process = run_command(command, repo, log, timeout=max(120, entry['estimated_seconds'] * 2))
                    status = 'PASS' if process.returncode == 0 else 'FAIL'
                except subprocess.TimeoutExpired:
                    status = 'TIMEOUT'
            elapsed += time.monotonic() - start
            write(destination, {'schema': 1, 'suite': entry['args']['suite'], 'status': status, 'reason': '', 'elapsed_seconds': time.monotonic() - start, 'coverage_complete': False})
            save(entry, status)
            continue
        if entry['owner'] != 'host-adapter':
            continue
        if elapsed >= sum(e['estimated_seconds'] for e in selected if e['owner'] == 'host-adapter'):
            save(entry, 'NOT_RUN', 'test execution budget exhausted')
            continue
        destination = output / (entry['id'].replace(':', '_') + '.json')
        command = [sys.executable, str(Path(__file__).with_name('run_host_test_suites.py')), '--suite', entry['args']['suite'], '--repo', str(repo), '--build-root', str(output / 'build'), '--output', str(destination)]
        start = time.monotonic()
        with destination.with_suffix('.log').open('w') as log:
            run_command(command, repo, log)
        if destination.exists():
            row = read(destination)
            if row.get('suite') != entry['args']['suite'] or row.get('status') not in ('PASS', 'FAIL', 'BLOCKED'):
                raise ValueError('Invalid adapter result')
            elapsed += max(0, float(row.get('elapsed_seconds', time.monotonic() - start)))
            save(entry, row['status'], row.get('reason', ''), exclusions=row.get('exclusions', []))
        else:
            save(entry, 'NOT_RUN', 'adapter did not produce a terminal receipt')
    return receipt


STATE_BRANCH = 'ci/nightly-tooling-state'


def github_api(path, method='GET', payload=None):
    repository = os.environ['GITHUB_REPOSITORY']
    token = os.environ['GH_TOKEN']
    request = urllib.request.Request('https://api.github.com/repos/' + repository + '/' + path,
        data=json.dumps(payload).encode() if payload is not None else None, method=method,
        headers={'Authorization': 'Bearer ' + token, 'Accept': 'application/vnd.github+json', 'X-GitHub-Api-Version': '2022-11-28'})
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.load(response)


def remote_state():
    try:
        ref = github_api('git/ref/heads/' + STATE_BRANCH)
    except urllib.error.HTTPError as error:
        if error.code == 404:
            return None, None
        raise
    head = ref['object']['sha']
    content = github_api('contents/ledger.json?ref=' + head)
    if content.get('size', 0) > 900000 or content.get('encoding') != 'base64':
        raise ValueError('Remote ledger exceeds compact JSON limit')
    state = json.loads(base64.b64decode(content['content']))
    validate_state(state)
    return head, state


def publish_state(plan, policy, receipts):
    # Dedicated branch only; target and default branches are never mutated.
    for attempt in range(2):
        head, previous = remote_state()
        if previous is None or any(previous[key] != plan[key] for key in ('target_sha', 'catalog_hash')):
            previous = fresh_state(plan['target_sha'], plan['catalog_hash'])
        merged = reconcile(plan, policy, previous, receipts)
        if head and merged == previous:
            return merged
        serialized = json.dumps(merged, indent=2) + '\n'
        if len(serialized.encode()) > 900000:
            raise ValueError('Ledger exceeds bounded permanent summary size')
        blob = github_api('git/blobs', 'POST', {'content': serialized, 'encoding': 'utf-8'})
        tree = github_api('git/trees', 'POST', {'tree': [{'path': 'ledger.json', 'mode': '100644', 'type': 'blob', 'sha': blob['sha']}]})
        commit = github_api('git/commits', 'POST', {'message': 'Record nightly tooling sweep ' + plan['target_sha'], 'tree': tree['sha'], 'parents': [head] if head else []})
        try:
            if head:
                github_api('git/refs/heads/' + STATE_BRANCH, 'PATCH', {'sha': commit['sha'], 'force': False})
            else:
                github_api('git/refs', 'POST', {'ref': 'refs/heads/' + STATE_BRANCH, 'sha': commit['sha']})
            return merged
        except urllib.error.HTTPError as error:
            if attempt or error.code not in (409, 422):
                raise
    raise RuntimeError('State publication conflict')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    p = sub.add_parser('plan')
    for option in ('target-sha', 'catalog', 'policy', 'output', 'state-output'):
        p.add_argument('--' + option, required=True)
    p.add_argument('--state')
    p.add_argument('--validation', action='store_true')
    p = sub.add_parser('execute')
    for option in ('plan', 'policy', 'repo', 'output'):
        p.add_argument('--' + option, required=True)
    p.add_argument('--lane', choices=LANES, required=True)
    p = sub.add_parser('collect')
    for option in ('plan', 'policy', 'output'):
        p.add_argument('--' + option, required=True)
    p.add_argument('--lane', choices=LANES, required=True)
    p.add_argument('--blocked-reason', default='')
    p = sub.add_parser('reconcile')
    for option in ('plan', 'policy', 'state', 'output'):
        p.add_argument('--' + option, required=True)
    p.add_argument('--receipts', nargs='*', default=[])
    p = sub.add_parser('fetch-state')
    p.add_argument('--output', required=True)
    p = sub.add_parser('publish')
    for option in ('plan', 'policy', 'expected-target', 'output'):
        p.add_argument('--' + option, required=True)
    p.add_argument('--receipts', nargs='*', default=[])
    args = parser.parse_args()
    if args.command == 'fetch-state':
        _, state = remote_state()
        if state is not None:
            write(args.output, state)
    elif args.command == 'publish':
        plan = read(args.plan)
        if plan['target_sha'] != args.expected_target:
            raise ValueError('Coordinator target mismatch')
        state = publish_state(plan, read(args.policy), [read(p) for p in args.receipts])
        write(args.output, state)
    elif args.command == 'plan':
        plan, state = plan_batch(args.target_sha, read(args.catalog), read(args.policy), read(args.state) if args.state and Path(args.state).exists() else None)
        if args.validation:
            state = fresh_state(plan['target_sha'], plan['catalog_hash'])
            validation_ids = {'master:test_run_all_tests_catalog', 'master:test_controller_overlay', 'shareware:test_death#d1',
                              'master:test_gradle_unit_tests', 'extractor:full', 'audio:strict-fingerprint', 'engine:ctest-d1'}
            plan['tests'] = [e for e in policy_entries(read(args.policy)).values() if e['id'] in validation_ids]
            if {e['id'] for e in plan['tests']} != validation_ids:
                raise ValueError('Missing representative validation suites')
            plan['estimated_seconds'] = sum(e['estimated_seconds'] for e in plan['tests'])
            plan['lanes'] = [lane for lane in LANES if any(e['lane'] == lane for e in plan['tests'])]
            plan['batch_id'] = digest({key: plan[key] for key in ('target_sha', 'catalog_hash', 'tests')})
        write(args.output, plan)
        write(args.state_output, state)
        print(json.dumps({'lanes': plan['lanes'], 'count': len(plan['tests']), 'batch_id': plan['batch_id']}))
    elif args.command == 'execute':
        signal.signal(signal.SIGTERM, lambda *_: sys.exit(143))
        result = execute(read(args.plan), read(args.policy), args.lane, args.repo, args.output)
        return int(any(r['status'] in ('FAIL', 'TIMEOUT') for r in result['results']))
    elif args.command == 'collect':
        collect(read(args.plan), read(args.policy), args.lane, args.output, args.blocked_reason)
    else:
        plan, policy = read(args.plan), read(args.policy)
        state = reconcile(plan, policy, read(args.state), [read(p) for p in args.receipts])
        write(args.output, state)
        summary = {'target_sha': state['target_sha'], 'terminal_owner_attempts': sum(r['status'] in COVERED for r in state['records'].values()), 'fully_covered_owners': sum(r.get('coverage_complete') is True for r in state['records'].values()), 'passed_leaf_cases': sum(r.get('case_counts', {}).get('PASS', 0) for r in state['records'].values()), 'incomplete_owners': [k for k, r in state['records'].items() if r.get('coverage_complete') is not True], 'failures': [k for k, r in state['records'].items() if r['status'] in ('FAIL', 'TIMEOUT')], 'retryable_infrastructure': state.get('retryable', {}), 'excluded': plan['exclusions'], 'target_capability_exclusions': state.get('excluded', {})}
        pending = set(policy_entries(policy)) - set(state['records']) - set(state.get('excluded', {})) - {e['id'] for e in plan['exclusions']}
        summary['pending_owners'] = len(pending)
        summary['outcome'] = 'incomplete' if pending else ('exhausted_with_failures' if summary['failures'] else ('exhausted_with_exclusions' if summary['incomplete_owners'] or summary['excluded'] or state.get('excluded') else 'complete_pass'))
        print(json.dumps(summary, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
