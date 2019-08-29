import _mems
v = _mems.version()
print(v)

y = _mems.connect("/dev/ttys005")
print(y)

if y['connected']:
   x = _mems.read()
   print(x)

if y['connected']:
   x = _mems.command('7d')
   print(x)