import stupid

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

libcfib_tests = {
        'CheckC11ThreadLocal' : check_c11_thread_local
        }

env = Environment()
# SCons is too smart ... need to tell it to fuck off
env['STATIC_AND_SHARED_OBJECTS_ARE_THE_SAME'] = True
env.Append(CCFLAGS = ['-g'])

have_c11_thread_local = False
system_api = None

#if not env.GetOption('clean'):
cnf = Configure(env, custom_tests = libcfib_tests)
if cnf.CheckHeader('unistd.h'):
    cnf.env.Append(CPPDEFINES = '_CFIB_SYSAPI_POSIX')
    system_api = "POSIX"
    if cnf.CheckHeader('pthread.h'):
        if cnf.CheckLib('pthread'):
            cnf.env.Append(LIBS = ['pthread'])
        else:
            print "Non-compliant POSIX system: missing -lpthread!"
            Exit(1)
    else:
        print "Non-compliant POSIX system: missing pthread.h!"
        Exit(1)
elif cnf.CheckHeader('windows.h'):
    cnf.env.Append(CPPDEFINES = '_CFIB_SYSAPI_WINDOWS')
    system_api = "WINDOWS"
else:
    print "Unsupported platform."
    Exit(1)
if cnf.CheckC11ThreadLocal():
    have_c11_thread_local = True
env = cnf.Finish()
print env['CPPDEFINES']

nasm_env = env.Clone();
nasm_env.Append(BUILDERS = {'NASMObject' : Builder(action = 'nasm -f $NASM_FORMAT $NASM_FLAGS -o $TARGET $SOURCE')})
nasm_env['NASM_FORMAT'] = 'elf64'
nasm_shared_obj = nasm_env.NASMObject('cfib-sysv-amd64.os', 'cfib-sysv-amd64.asm', NASM_FLAGS='-D _ELF_SHARED=1')
nasm_static_obj = nasm_env.NASMObject('cfib-sysv-amd64.o', 'cfib-sysv-amd64.asm')

if have_c11_thread_local:
    env_c11 = env.Clone()
    env_c11.Append(CCFLAGS = ['-D_HAVE_C11_THREAD_LOCAL'])
    shared_objects = [env_c11.SharedObject('cfib_C11', 'cfib.c'), nasm_shared_obj]
    static_objects = [env_c11.StaticObject('cfib_C11', 'cfib.c'), nasm_static_obj]
    env_c11.SharedLibrary(target = 'cfib_C11', source = shared_objects)
    env_c11.StaticLibrary(target = 'cfib_C11', source = static_objects)
    test_obj = env_c11.Object('test_cfib_c11', 'test_cfib.c')
    env_c11.Program(target = 'test_cfib_C11_static', source = [test_obj, Dir('.').abspath + '/libcfib_C11.a'])
    env_c11.Program(target = 'test_cfib_C11', source = [test_obj], LIBS = ['cfib_C11'], LIBPATH = ['.'])

shared_objects = [env.SharedObject('cfib', 'cfib.c'), nasm_shared_obj]
static_objects = [env.StaticObject('cfib', 'cfib.c'), nasm_static_obj]
env.SharedLibrary(target = 'cfib', source = shared_objects)
env.StaticLibrary(target = 'cfib', source = static_objects)
test_obj = env.Object('test_cfib.c')
env.Program(target = 'test_cfib_static', source = [test_obj, Dir('.').abspath + '/libcfib.a'])
env.Program(target = 'test_cfib', source = [test_obj], LIBS = ['cfib'], LIBPATH = ['.'])

