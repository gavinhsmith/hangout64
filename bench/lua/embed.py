"""Embed a text file as a C string: python3 embed.py bench.lua bench_lua > bench_lua.h"""
import sys

path, name = sys.argv[1], sys.argv[2]
text = open(path, encoding='utf-8').read()
print(f'static const char {name}[] =')
for line in text.splitlines(keepends=True):
    print('    "' + line.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"')
print(';')
