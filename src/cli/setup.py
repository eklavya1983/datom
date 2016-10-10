from setuptools import setup

setup(
    name='datom',
    version='0.1',
    py_modules=['datom'],
    install_requires=[
        'click',
    ],
    entry_points='''
        [console_scripts]
        datom=datom:cli
    ''',
)
