# What the Makefile's sed recipe does, for machines without make or sed.
#
#     python gen.py header.frag header_frag_src header_fs.inc
#
# Byte-for-byte the same output: a blank line passes through as a blank line,
# because sed's `..*` does not match one.
import io, sys

name, var, out = sys.argv[1], sys.argv[2], sys.argv[3]
src = io.open(name, encoding='utf-8', newline='').read().replace('\r\n', '\n')

if src.endswith('\n'):
    src = src[:-1]

lines = ['const char *' + var + ' =']
for line in src.split('\n'):
    lines.append('"' + line + '\\n"' if line else '')
lines.append(';')

io.open(out, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
print('wrote ' + out)
