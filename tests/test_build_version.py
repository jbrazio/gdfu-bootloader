from pathlib import Path
import runpy
import subprocess
import tempfile
import unittest


class BuildVersionTests(unittest.TestCase):
    def test_scripts_read_version_file(self):
        source = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'scripts').mkdir()
            for name in ('build-version.py', 'release-notes.py'):
                (root / 'scripts' / name).write_text((source / 'scripts' / name).read_text())
            subprocess.run(['git', 'init', '-q', directory], check=True)
            for version in ('2026.11.1', '2026.12.2'):
                (root / 'VERSION').write_text(version + '\n')
                output = subprocess.check_output(
                    ['python3', str(root / 'scripts/build-version.py'), '--branch', 'master'], text=True)
                self.assertEqual(output.strip(), version + '-dirty')
                (root / 'CHANGELOG.md').write_text(f'## [{version}]\n\nRelease notes.\n')
                subprocess.run(['python3', 'scripts/release-notes.py'], cwd=root, check=True)
                self.assertEqual((root / 'release-notes.md').read_text(), 'Release notes.\n')

    def test_git_versions(self):
        script = Path(__file__).resolve().parents[1] / 'scripts/build-version.py'
        self.assertTrue(script.exists(), 'build-time version helper is missing')
        build_version = runpy.run_path(str(script))['build_version']
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            def git(*args):
                return subprocess.check_output(['git', '-C', directory, *args], text=True).strip()
            git('init', '-q', '-b', 'dev')
            git('config', 'user.email', 'test@example.invalid')
            git('config', 'user.name', 'Test')
            (root / '.gitignore').write_text('ignored/\n')
            (root / 'source').write_text('initial')
            git('add', '.')
            git('commit', '-qm', 'initial')
            sha = git('rev-parse', '--short', 'HEAD')
            stamp = '2026.11.1'
            self.assertEqual(build_version(root, stamp), f'{stamp}-{sha}')
            (root / 'ignored').mkdir()
            (root / 'ignored/output').write_text('generated')
            self.assertEqual(build_version(root, stamp), f'{stamp}-{sha}')
            (root / 'untracked').write_text('new')
            self.assertEqual(build_version(root, stamp), f'{stamp}-{sha}-dirty')
            (root / 'untracked').unlink()
            (root / 'source').write_text('changed')
            self.assertEqual(build_version(root, stamp), f'{stamp}-{sha}-dirty')
            git('add', 'source')
            self.assertEqual(build_version(root, stamp), f'{stamp}-{sha}-dirty')
            git('reset', '--hard', '-q', 'HEAD')
            git('checkout', '-q', 'HEAD', '--detach')
            self.assertEqual(build_version(root, stamp, 'dev'), f'{stamp}-{sha}')
            self.assertEqual(build_version(root, '2026.11.1-rc2', 'dev'), f'2026.11.1-rc2-{sha}')
            self.assertEqual(build_version(root, '2026.11.1-rc2', 'master'), '2026.11.1-rc2')
            self.assertEqual(build_version(root, stamp, 'master'), stamp)
            git('checkout', '-qb', 'master')
            self.assertEqual(build_version(root, stamp), stamp)
            (root / 'source').write_text('dirty production')
            self.assertEqual(build_version(root, stamp), stamp + '-dirty')
            for invalid in ['abc', '2026.11', '2026.11.1+42', '2026.11.1-dirty', '2026.11.1-rc', 'gdfu-2026.11.1']:
                with self.assertRaises(ValueError):
                    build_version(root, invalid)


if __name__ == '__main__':
    unittest.main()
