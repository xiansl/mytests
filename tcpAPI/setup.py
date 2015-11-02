from distutils.core import setup, Extension

module1 = Extension('acc_monitor',
                    sources = ['acc_monitor.c', 'tcp_transfer.c'],
		    include_dirs = ['.'],
		    libraries = ['pthread','fpga'],
                    library_dirs = ['/usr/lib'] )
setup (name = 'PackageName',
       version = '1.0',
       description = 'This is a fpga server package',
       ext_modules = [module1])

