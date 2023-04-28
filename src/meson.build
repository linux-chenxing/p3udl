libusb_dep = dependency('libusb-1.0')
argtable2_dep = dependency('argtable2')
openssl_dep = dependency('openssl')

src = [
        'main.c',
        'usbms.c',
        'sstarscsi.c',
        'log.c'
       ]

deps = [
	libusb_dep,
	argtable2_dep,
	openssl_dep,
	libdpgc_dep
]

conf_data = configuration_data()
conf_data.set('TAG', 'usbms')
conf_data.set('DEBUG_OPT', 'CONFIG_DEBUG_SSTARSCSI')
conf_data.set('PREFIX', 'usbms')
conf_data.set('FUNC', '(_log_var)->log_cb')

configure_file(input : log_macros_tmpl,
               output : 'usbms_log.h',
               configuration : conf_data)

conf_data = configuration_data()
conf_data.set('TAG', 'sstarscsi')
conf_data.set('DEBUG_OPT', 'CONFIG_DEBUG_SSTARSCSI')
conf_data.set('PREFIX', 'sstarscsi')
conf_data.set('FUNC', '(_log_var)->log_cb')

configure_file(input : log_macros_tmpl,
               output : 'sstarscsi_log.h',
               configuration : conf_data)

conf_data = configuration_data()
conf_data.set('TAG', 'p3udl')
conf_data.set('DEBUG_OPT', 'CONFIG_DEBUG_SSTARSCSI')
conf_data.set('PREFIX', 'p3udl')
conf_data.set('FUNC', '(_log_var)->log_cb')

configure_file(input : log_macros_tmpl,
               output : 'main_log.h',
               configuration : conf_data)

executable('p3udl', src, dependencies: deps, install : true)
