project('usbdownloader', 'c',
        default_options : ['c_std=c99'])

cc = meson.get_compiler('c')
cflags = cc.get_supported_arguments([
                                     '-Wall',
                                     '-Werror=incompatible-pointer-types',
                                     '-Werror=implicit-function-declaration',
                                     '-Werror=format-security'
                                    ])
add_project_arguments(cflags, language: 'c')
add_project_arguments('-D_GNU_SOURCE', language: 'c')

libdgpc_proj = subproject('libdgpc')
libdpgc_dep = libdgpc_proj.get_variable('libdgpc_dep')
log_macros_tmpl = libdgpc_proj.get_variable('log_macros_tmpl')

subdir('src')

