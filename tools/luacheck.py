import sys, re
sys.stdout.reconfigure(encoding='utf-8')
src = open('songs/lib.lua', encoding='utf-8').read()
KEYS = ['local', 'end', 'then', 'function', 'nil', 'true', 'false', 'and', 'or',
        'not', 'in', 'do', 'repeat', 'until', 'return', 'break']
issues = []
for kw in KEYS:
    for m in re.finditer(r'[.:]' + kw + r'(?=[\s=,)\]}])', src):
        issues.append(('dot-field %s line %d' % (kw, src.count('\n', 0, m.start()) + 1)))
    for m in re.finditer(r'\{\s*' + kw + r'\s*=', src):
        issues.append(('ctor-field %s line %d' % (kw, src.count('\n', 0, m.start()) + 1)))
print('keyword issues:', issues if issues else 'NONE')
print('brace balance', src.count('{') - src.count('}'), '| paren balance', src.count('(') - src.count(')'))
print('P functions:', re.findall(r'function\s+(P\.\w+)', src))
print('noRpc refs:', src.count('noRpc'), '| ["local"] refs:', src.count('["local"]'))
