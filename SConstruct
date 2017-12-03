# Build variables
vars = Variables(None, ARGUMENTS)
vars.Add(PathVariable('prefix', 'Common install prefix', '/usr/local'))
vars.Add(PathVariable('includedir', "Header files' install dir", '${prefix}/include'))
vars.Add(PathVariable('libdir', "Library files' install dir", '${prefix}/lib'))

# Root environment, cloned by SConscripts
root_env = Environment(variables = vars)
root_env['STATIC_AND_SHARED_OBJECTS_ARE_THE_SAME'] = True
root_env.Append(CCFLAGS = ['-g'])

# Generate help text for cli options
Help(vars.GenerateHelpText(root_env))

# SCons is too smart ... need to tell it to fuck off
config_system_API = None
config_system_ABI = 'sysv-amd64' # others: cdecl-x86, microsoft-x64, eabi-arm, aarch64-arm
config_have_c11_thread_local = False

test_c11_thread_local = """
_Thread_local int test = 24;

int main(int argc, char** argv) {
    test = test * 0;
    return test;
}
"""

def check_c11_thread_local(context):
    context.Message('Checking for C11 _Thread_local ... ')
    result = context.TryLink(test_c11_thread_local, '.c')
    context.Result(result)
    return result

libcfib_tests = {'CheckC11ThreadLocal' : check_c11_thread_local}

#if not env.GetOption('clean'):
cnf = Configure(root_env, custom_tests = libcfib_tests)
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
if cnf.CheckC11ThreadLocal():
    config_have_c11_thread_local = True
root_env = cnf.Finish()
#print env['CPPDEFINES']

Export('root_env', 'config_system_API', 'config_system_ABI', 'config_have_c11_thread_local')
built_files = SConscript('src/SConscript', variant_dir='build', duplicate=0)

# Install target
root_env.Install('${libdir}', built_files['lib'])
root_env.Install('${includedir}', root_env.Glob('src/*.h'))
_lib = root_env.Alias('install-lib', '${libdir}')
_include = root_env.Alias('install-include', '${includedir}')
root_env.Alias('install', [_lib, _include])
