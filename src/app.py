import _mems
x = _mems.read(2.0, 1.0, [-1.0, 4.2, 30.6], [-1.5, 8.0, 63.0],[1.0, 1.5, 0.6])
y = _mems.connect('dev://ttyecu')
print(x)
print(y)
