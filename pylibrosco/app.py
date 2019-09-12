import librosco
import time

v = librosco.version()
print(v)

y = librosco.connect("/dev/ttys026")
print(y)

if y['connected']:
   x = librosco.send('80')
   print(x)

if y['connected']:
   x = librosco.send('7d')
   print(x)

if y['connected']:
   for n in range(0, 100):
      x = librosco.read()
      print(x)
      time.sleep(0.5)

