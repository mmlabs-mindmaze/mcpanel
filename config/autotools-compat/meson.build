# Defined for compatibility with autotools
# The other such macros defined by autotools are not used and will not be added
prefix = get_option('prefix')
projname = meson.project_name()
config.set_quoted('PACKAGE_NAME', projname)
config.set_quoted('PKGDATADIR', prefix / get_option('datadir') / projname)
