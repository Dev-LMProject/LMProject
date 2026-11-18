"""Real Windows hook integration tests; all state is confined to a temporary copy."""
import base64
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
POWERSHELL = Path(os.environ['SystemRoot']) / 'System32/WindowsPowerShell/v1.0/powershell.exe'

class DesignLoadingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='LM design loading ')
        self.root = Path(self.temp.name)
        shutil.copytree(ROOT / '.codex/hooks', self.root / '.codex/hooks', ignore=shutil.ignore_patterns('__pycache__'))
        shutil.copytree(ROOT / '.agents/skills/planning-with-files', self.root / '.agents/skills/planning-with-files', ignore=shutil.ignore_patterns('__pycache__'))
        for name in ('design-a', 'design-b'):
            plan = self.root / '.planning' / name
            plan.mkdir(parents=True)
            (plan / 'task_plan.md').write_text(f'# {name}\n## Goal\nGOAL-{name}\n### Phase 1: Design\n**Status:** in_progress\n', encoding='utf-8')
            (plan / 'findings.md').write_text('DECISION\n', encoding='utf-8')
            (plan / 'progress.md').write_text('PROGRESS\n', encoding='utf-8')
        self.addCleanup(self.temp.cleanup)

    def run_hook(self, event='UserPromptSubmit', prompt='', plan='', session='qa-a', source='startup', disabled=False):
        command = "$ProgressPreference = 'SilentlyContinue'; & '" + str(self.root / '.codex/hooks/LMInvokePlanning.ps1').replace("'", "''") + "'"
        encoded = base64.b64encode(command.encode('utf-16le')).decode('ascii')
        env = os.environ.copy()
        for key in ('PLAN_ID', 'PWF_PLAN_ROOT', 'PWF_SESSION_ID', 'PLANNING_DISABLED', 'PYTHON_BIN'):
            env.pop(key, None)
        env['PYTHON_BIN'] = sys.executable
        if plan:
            env['PLAN_ID'] = plan
        if disabled:
            env['PLANNING_DISABLED'] = '1'
        payload = dict(hook_event_name=event, prompt=prompt, cwd=str(self.root), session_id=session, source=source, tool_name='exec_command', stop_hook_active=False)
        result = subprocess.run([str(POWERSHELL), '-NoLogo', '-NoProfile', '-NonInteractive', '-EncodedCommand', encoded], input=json.dumps(payload, ensure_ascii=False), encoding='utf-8', capture_output=True, env=env, cwd=self.root, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(result.stderr, result.stderr)
        return result.stdout.strip()

    def test_plain_question_is_silent_unbound(self):
        self.assertEqual(self.run_hook(prompt='오늘 날짜가 뭐야?'), '')

    def test_plain_question_is_silent_bound(self):
        self.assertEqual(self.run_hook(prompt='오타만 고쳐줘', plan='design-a'), '')

    def test_unbound_startup_is_silent(self):
        self.assertEqual(self.run_hook(event='SessionStart'), '')

    def test_design_request_prompts_once_until_bound(self):
        first = self.run_hook(prompt='몬스터 전투 시스템 설계해줘')
        self.assertIn('No task is bound', first)
        self.assertEqual(self.run_hook(prompt='몬스터 전투 시스템 설계해줘'), '')

    def test_same_bound_design_loads_once(self):
        self.assertIn('GOAL-design-a', self.run_hook(prompt='몬스터 전투 시스템 설계해줘', plan='design-a'))
        self.assertEqual(self.run_hook(prompt='동일한 설계를 계속 검토해줘', plan='design-a'), '')
        self.assertEqual(self.run_hook(event='PreToolUse', plan='design-a'), '')

    def test_plan_switch_loads_new_design(self):
        self.run_hook(prompt='전투 설계해줘', plan='design-a')
        out = self.run_hook(prompt='새 퍼즐 설계해줘', plan='design-b')
        self.assertIn('GOAL-design-b', out)
        self.assertNotIn('GOAL-design-a', out)

    def test_explicit_recovery_reloads_same_design(self):
        self.run_hook(prompt='전투 설계해줘', plan='design-a')
        self.assertIn('GOAL-design-a', self.run_hook(prompt='계획 다시 읽어줘', plan='design-a'))

    def test_resume_and_compact_restore_selected_plan(self):
        self.run_hook(prompt='전투 설계해줘', plan='design-a')
        for source in ('resume', 'compact', 'fork'):
            self.assertIn('GOAL-design-a', self.run_hook(event='SessionStart', plan='design-a', source=source))

    def test_ordinary_turn_stops_design_tool_nudges(self):
        self.run_hook(prompt='전투 설계해줘', plan='design-a')
        self.run_hook(prompt='오늘 날씨는 어때?', plan='design-a')
        for event in ('PreToolUse', 'PostToolUse', 'PermissionRequest', 'Stop'):
            self.assertEqual(self.run_hook(event=event, plan='design-a'), '')

    def test_negative_and_quoted_mentions_do_not_activate(self):
        for prompt in ('설계하지 말고 오타만 수정해줘', '설계 말고 코드만 설명해줘', '설계가 뭐야?', '문장 번역: "전투 설계해줘"', '```\n전투 설계해줘\n```', 'do not design, just explain'):
            with self.subTest(prompt=prompt):
                self.assertEqual(self.run_hook(prompt=prompt), '')

    def test_explicit_skill_invocation_is_supported(self):
        self.assertIn('No task is bound', self.run_hook(prompt='$planning-with-files 복잡한 작업 시작'))

    def test_disabled_and_traversal_protection(self):
        self.assertEqual(self.run_hook(prompt='설계해줘', disabled=True), '')
        self.assertIn('Recovery unavailable', self.run_hook(prompt='설계해줘', plan='../design-a'))

    def test_sessions_are_independent(self):
        self.run_hook(prompt='전투 설계해줘', plan='design-a')
        self.assertIn('GOAL-design-a', self.run_hook(prompt='전투 설계해줘', plan='design-a', session='qa-b'))

    def test_new_design_with_old_binding_does_not_reuse_old_plan(self):
        self.run_hook(prompt='전투 설계해줘', plan='design-a')
        out = self.run_hook(prompt='새로운 퍼즐 시스템 설계해줘', plan='design-a')
        self.assertIn('distinct PLAN_ID', out)
        self.assertNotIn('GOAL-design-a', out)
        self.assertEqual(self.run_hook(prompt='새로운 퍼즐 시스템 설계해줘', plan='design-a'), '')
        self.assertIn('GOAL-design-b', self.run_hook(prompt='퍼즐 설계해줘', plan='design-b'))

    def test_post_tool_reminder_is_once_per_design_turn(self):
        self.run_hook(prompt='전투 설계해줘', plan='design-a')
        self.assertIn('next action', self.run_hook(event='PostToolUse', plan='design-a'))
        self.assertEqual(self.run_hook(event='PostToolUse', plan='design-a'), '')
        self.run_hook(prompt='다음 단계', plan='design-a')
        self.assertIn('next action', self.run_hook(event='PostToolUse', plan='design-a'))

    def test_context_records_remain_unchanged(self):
        files = list((self.root / '.planning').rglob('*.md'))
        before = {p: p.read_bytes() for p in files}
        self.run_hook(prompt='전투 설계해줘', plan='design-a')
        self.run_hook(event='SessionStart', plan='design-a', source='compact')
        self.assertEqual(before, {p: p.read_bytes() for p in files})

    def test_explicit_korean_plan_and_resume_requests(self):
        self.assertIn('No task is bound', self.run_hook(prompt='계획을 세워줘'))
        self.run_hook(prompt='전투 설계해줘', plan='design-a')
        self.assertEqual(self.run_hook(prompt='이전 설계 이어서 해줘', plan='design-a'), '')
        self.assertIn('next action', self.run_hook(event='PostToolUse', plan='design-a'))

    def test_changed_plan_gets_short_notice_not_full_reload(self):
        self.run_hook(prompt='전투 설계해줘', plan='design-a')
        path = self.root / '.planning/design-a/findings.md'
        path.write_text('CHANGED DECISION', encoding='utf-8')
        out = self.run_hook(prompt='동일한 설계를 계속 검토해줘', plan='design-a')
        self.assertIn('records changed', out)
        self.assertNotIn('GOAL-design-a', out)
        self.assertEqual(self.run_hook(prompt='동일한 설계를 계속 검토해줘', plan='design-a'), '')

    def test_new_design_without_receipt_does_not_reuse_binding(self):
        out = self.run_hook(prompt='새로운 전투 시스템 설계해줘', plan='design-a')
        self.assertIn('distinct PLAN_ID', out)
        self.assertNotIn('GOAL-design-a', out)

    def test_design_questions_are_not_design_requests(self):
        for prompt in ('새로운 설계가 뭐야?', '다른 설계와 비교만 해줘'):
            self.assertEqual(self.run_hook(prompt=prompt), '')

    def test_unbound_compact_allows_setup_hint_again(self):
        self.run_hook(prompt='전투 설계해줘')
        self.assertEqual(self.run_hook(event='SessionStart', source='compact'), '')
        self.assertIn('No task is bound', self.run_hook(prompt='전투 설계해줘'))

    def test_malformed_receipt_is_recovered_without_selecting_other_plan(self):
        self.run_hook(prompt='전투 설계해줘', plan='design-a')
        receipt = next((self.root / 'Saved/LMContext/Loading').glob('*.json'))
        receipt.write_text('{invalid', encoding='utf-8')
        out = self.run_hook(prompt='전투 설계해줘', plan='design-a')
        self.assertIn('GOAL-design-a', out)
        self.assertNotIn('GOAL-design-b', out)

    def test_oversized_receipt_is_refused(self):
        self.run_hook(prompt='전투 설계해줘', plan='design-a')
        receipt = next((self.root / 'Saved/LMContext/Loading').glob('*.json'))
        receipt.write_text('x' * 4097, encoding='utf-8')
        out = self.run_hook(prompt='전투 설계해줘', plan='design-a')
        self.assertIn('Recovery unavailable', out)
        self.assertNotIn('GOAL-design-a', out)

if __name__ == '__main__':
    unittest.main(verbosity=2)