import os

# Build variables
vars = Variables(None, ARGUMENTS)
vars.Add(PathVariable('prefix', 'Common install prefix', '/usr/local'))
vars.Add(PathVariable('includedir', "Header files' install dir", '${prefix}/include'))
vars.Add(PathVariable('libdir', "Library files' install dir", '${prefix}/lib'))

# Root environment, cloned by SConscripts
#root_env = Environment(variables = vars, tools = ['default', 'nasm'])
root_env = Environment(variables = vars)
root_env['ENV']['TERM'] = os.environ['TERM']
root_env.Append(CCFLAGS = ['-Wall', '-Wpedantic', '-g'])
root_env.Append(LINKFLAGS = ['-g'])

# Generate help text for cli options
Help(vars.GenerateHelpText(root_env))

# SCons is too smart ... need to tell it to fuck off
config_system_API = None
config_system_ABI = 'sysv-amd64' # others: cdecl-x86, microsoft-x64, eabi-arm, aarch64-arm
config_have_c11_thread_local = False

def check_c11_thread_local(context):
    context.Message('Checking for C11 _Thread_local ... ')
    result = context.TryLink(open('config_tests/test_c11_thread_local.c').read(), '.c')
    context.Result(result)
    return result

def check_c11_atomics(context):
    context.Message('Checking for C11 _Atomic ... ')
    result = context.TryLink(open('config_tests/test_c11_atomics.c').read(), '.c')
    context.Result(result)
    return result

libcfib_tests = {
    'CheckC11ThreadLocal' : check_c11_thread_local,
    'CheckC11Atomics' : check_c11_atomics
}

#if not env.GetOption('clean'):
cnf = Configure(root_env, custom_tests = libcfib_tests)
if not cnf.CheckC11ThreadLocal():
    print "A compiler that supports C11 _Thread_local is required to build!"
    Exit(1)
else:
    # This config flag is not used anywhere yet, but defined regardless
    config_have_c11_thread_local = True

if cnf.CheckC11Atomics():
    config_have_c11_atomics = True

if cnf.CheckHeader('unistd.h'):
    #cnf.env.Append(CPPDEFINES = '_CFIB_SYSAPI_POSIX')
    config_system_API = "POSIX"
    if cnf.CheckHeader('pthread.h'):
        if not cnf.CheckLib('pthread'):
            print "Non-compliant POSIX system: missing -lpthread!"
            Exit(1)
    else:
        print "Non-compliant POSIX system: missing pthread.h!"
        Exit(1)
elif cnf.CheckHeader('windows.h'):
    #cnf.env.Append(CPPDEFINES = '_CFIB_SYSAPI_WINDOWS')
    config_system_API = "WINDOWS"
else:
    print "Unsupported platform."
    Exit(1)

root_env = cnf.Finish()

Export('root_env', 'config_system_API', 'config_system_ABI', 'config_have_c11_atomics')
built_files = SConscript('src/SConscript', variant_dir='build', duplicate=0)

# Install target
root_env.Install('${libdir}', built_files['lib'])
root_env.Install('${includedir}', root_env.Glob('src/*.h'))
_lib = root_env.Alias('install-lib', '${libdir}')
_include = root_env.Alias('install-include', '${includedir}')
root_env.Alias('install', [_lib, _include])
