from pathlib import Path
import subprocess

helper = Path(__file__).resolve().parents[1] / 'st-autocomplete'


def complete(mode, visible, prefix):
    result = subprocess.run([helper, mode, '1', str(len(prefix))],
                            input=visible + '\n' + prefix + '\n', text=True,
                            capture_output=True, check=True)
    return result.stdout.splitlines()


assert complete('word-complete', 'alphabet alphabet', 'al') == ['al', 'alphabet']
assert complete('WORD-complete', '/tmp/a-file', '/tmp/a') == ['/tmp/a', '/tmp/a-file']
assert complete('fuzzy-word-complete', 'alphabet', 'abt') == ['abt', 'alphabet']
assert complete('fuzzy-WORD-complete', '/tmp/a-file', '/taf') == ['/taf', '/tmp/a-file']
assert complete('fuzzy-complete', 'hello wide world', 'hww') == ['hww', 'hello wide world']
assert complete('suffix-complete', '/tmp/hello', 'hel') == ['hel', 'hello']
assert complete('surround-complete', '"hello world"', 'hw') == ['hw', 'hello world']
assert complete('word-complete', 'caféine', 'caf') == ['caf', 'caféine']
assert complete('word-complete', 'alphabet', '') == ['']
print('all autocomplete modes, deduplication and unicode: passed')
