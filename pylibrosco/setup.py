from setuptools import setup, Extension

setup(
    name = 'librosco', 
    version = '1.0.1',
    description='Python Extension for Rover MEMS ECU library librosco 2.0.0',
    platforms='MacOS X',
    author='Andrew Jackson',
    author_email='andrew.d.jackson@gmail.com',
    url='http://github.com/andrewdjackson/librosco',
    license='MIT',
    ext_package='librosco',
    ext_modules=[Extension('librosco', ['./src/mems.c', './src/protocol.c', './src/setup.c']),],
    classifiers=[
          'Operating System :: MacOS :: MacOS X',
          'Programming Language :: C',
          ],
)