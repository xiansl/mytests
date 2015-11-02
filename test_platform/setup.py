from distutils.core import setup, Extension

module1 = Extension('demo',
                    sources = ['demo.c', 'server.c'],
		    include_dirs = ['.'],
		    libraries = ['rdmacm', 'pthread', 'ibverbs','fpga'],
                    library_dirs = ['/usr/lib', './fpga-sim/driver'] )
setup (name = 'PackageName',
       version = '1.0',
       description = 'This is a rpc server package',
       ext_modules = [module1])

