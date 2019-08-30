import librosco

v = librosco.version()
print(v)

y = librosco.connect("/dev/ttys005")
print(y)

if y['connected']:
   x = librosco.read()
   print(x)

if y['connected']:
   x = librosco.command('7d')
   print(x)