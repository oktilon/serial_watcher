project(
    'swatcher',
    'c',
    version : '1.0.1',
    meson_version : '>= 0.57.0'
)

# Setup variables
version_arr = meson.project_version().split('.')
prj_name = meson.project_name()

sysPath = get_option('log_folder')
sysHost = get_option('host')
sysUser = get_option('user')
sysPass = get_option('pass')
sysCnd = get_option('cmd')

# Project configuration
conf_data = configuration_data()
conf_data.set('version_major',      version_arr[0])
conf_data.set('version_minor',      version_arr[1])
conf_data.set('version_rev',        version_arr[2])
# conf_data.set('prj_name',           prj_name)
conf_data.set('log_file',   sysPath + 'week')
conf_data.set('host',       sysHost)
conf_data.set('user',       sysUser)
conf_data.set('pass',       sysPass)
conf_data.set('cmd',        sysCmd)


# Dependencies
# deps = [
#     dependency('threads')
# ]

# Includes
inc = include_directories([
    'inc',
])

# Sources
src = [
    'src/sw_uart.c',
    'src/app.c'
]

# Main header file config
configure_file(
    input         : 'tpl/app.h',
    output        : 'app.h',
    configuration : conf_data
)

# Build executable
executable(
    prj_name,
    src,
    include_directories : inc,
    # dependencies        : deps,
    install             : true
)
