project('mcpanel', 'c',
        version : '0.1',
        default_options : [
            'warning_level=1',
        ],
        license : 'LGPL-3.0',
        meson_version: '>= 0.49'
)

# follow semantic versionning (https://semver.org)
# * MAJOR version when you make incompatible API changes,
# * MINOR version when you add functionality in a backwards-compatible manner
# * PATCH version when you make backwards-compatible bug fixes.
major = '0'
minor = '0'
patch = '1'
version = major + '.' + minor + '.' + patch

cc = meson.get_compiler('c')
configuration_inc = include_directories('src')

# additional (optional) warnings
flags = [
        '-Wshadow',
        '-Wmissing-prototypes',
]
add_project_arguments(cc.get_supported_arguments(flags), language : 'c')

# define HAVE_CONFIG_H with compiler command line to include the generated
# config.h file (same as autotools)
add_project_arguments('-DHAVE_CONFIG_H', language : 'c')

config = configuration_data()
subdir('config/api-exports')
subdir('config/autotools-compat')

if get_option('glib-check')
    config.set(G_DISABLE_CAST_CHECKS, 1)
endif

# write config file
build_cfg = 'config.h'  # named as such to match autotools build system
configure_file(output : build_cfg, configuration : config)

mcpanel_headers = files(
        'src/mcpanel.h',
)

mcpanel_sources = files(
        'src/bargraph.c',
        'src/bargraph.h',
        'src/bartab.c',
        'src/binary-scope.c',
        'src/binary-scope.h',
        'src/gtk-led.c',
        'src/gtk-led.h',
        'src/labelized-plot.c',
        'src/labelized-plot.h',
        'src/mcpanel.c',
        'src/mcpanel.h',
        'src/mcp_gui.c',
        'src/mcp_gui.h',
        'src/mcp_shared.h',
        'src/mcp_sighandler.c',
        'src/mcp_sighandler.h',
        'src/misc.c',
        'src/misc.c',
        'src/plot-area.c',
        'src/plot-area.h',
        'src/plotgraph.c',
        'src/plotgraph.h',
        'src/plottk-types.h',
        'src/scope.c',
        'src/scope.h',
        'src/scopetab.c',
        'src/signaltab.c',
        'src/signaltab.h',
        'src/spectrum.c',
        'src/spectrum.h',
        'src/spectrumtab.c',
)

install_headers(mcpanel_headers)

libmath = cc.find_library('m', required : true)
glib2 = dependency('glib-2.0', required : true)
gtk2 = dependency('gtk+-2.0', required : true)
gthread2 = dependency('gthread-2.0', required : true)
rtfilter = cc.find_library('rtfilter', required : true)

mcpanel = shared_library('libmcpanel',
        mcpanel_sources,
        install : true,
        version : version,
        include_directories : configuration_inc,
        dependencies : [libmath, gtk2, gthread2, glib2, rtfilter],
)

pkg = import('pkgconfig')
pkg.generate(mcpanel)

# install data
install_data(
        files('src/led_gray.png',
                'src/led_green.png',
                'src/led_red.png',
                'src/led_blue.png',
                'src/default.ui',
        ),
        install_dir: get_option('datadir') / 'mcpanel'
)

if get_option('tests')
        mmlib = cc.find_library('mmlib', required : true)

        test_thread_panel_sources = files('test/thread_panel.c')
        test_thread_panel = executable('test-thread-panel',
                test_thread_panel_sources,
                include_directories : configuration_inc,
                link_with : mcpanel,
                dependencies : [mmlib, gthread2],
        )
        test('test-thread-panel', test_thread_panel,
                env : ['MCPANEL_DATADIR=' + meson.source_root() + '/src',
                       'XDG_CONFIG_HOME=' + meson.source_root() + '/test',
                ],
        )

        test_signal_panel_sources = files('test/signal_panel.c')
        test_signal_panel = executable('test-signal-panel',
                test_signal_panel_sources,
                include_directories : configuration_inc,
                link_with : mcpanel,
                dependencies : [glib2, libmath],
        )
        test('test-signal-panel', test_signal_panel,
                env : ['MCPANEL_DATADIR=' + meson.source_root() + '/src',
                       'XDG_CONFIG_HOME=' + meson.source_root() + '/test',
                ],
        )
endif
