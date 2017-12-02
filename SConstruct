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
env.Append(CCFLAGS = '-g')

if not env.GetOption('clean'):
    cnf = Configure(env, custom_tests = libcfib_tests)
    have_c11_thread_local = False
    have_c11_threads = False
    if cnf.CheckC11ThreadLocal():
        cnf.env.Append(CCFLAGS = ['-D_CFIB_THREAD_LOCAL=C11'])
        have_c11_thread_local = True
        if cnf.CheckHeader('threads.h'):
            cnf.env.Append(LIBS = ['stdthreads'])
            have_c11_threads = True
    if cnf.CheckHeader('unistd.h'):
        cnf.env.Append(CCFLAGS = ['-D_CFIB_SYS=POSIX'])
        if cnf.CheckHeader('pthread.h'):
            if have_c11_thread_local == False:
                cnf.env.Append(CCFLAGS = ['-D_CFIB_THREAD_LOCAL=POSIX'])
            if have_c11_threads == False:
                cnf.env.Append(LIBS = ['pthread'])
        else:
            print "Non-compliant POSIX system: missing pthread.h!"
            Exit(1)
    elif cnf.CheckHeader('windows.h'):
        cnf.env.Append(CCFLAGS = ['-D_CFIB_SYS=WINDOWS'])
        if have_c11_thread_local == False:
            cnf.env.Append(CCFLAGS = ['-D_CFIB_THREAD_LOCAL=WINDOWS'])
    env = cnf.Finish()
# SCons is too smart ... need to tell it to fuck off
env['STATIC_AND_SHARED_OBJECTS_ARE_THE_SAME'] = True
nasm_env = env.Clone();
nasm_env.Append(BUILDERS = {'NASMObject' : Builder(action = 'nasm -f $NASM_FORMAT $NASM_FLAGS -o $TARGET $SOURCE')})
nasm_env['NASM_FORMAT'] = 'elf64'
#nasm_env['NASM_FLAGS'] = '-d _ELF_SHARED=1'
nasm_shared_obj = nasm_env.NASMObject('cfib-sysv-amd64.os', 'cfib-sysv-amd64.asm', NASM_FLAGS='-D _ELF_SHARED')
nasm_static_obj = nasm_env.NASMObject('cfib-sysv-amd64.o', 'cfib-sysv-amd64.asm')
libcfib_shared_sources = ['cfib.c', 'cfib-sysv-amd64.os']
libcfib_static_sources = ['cfib.c', 'cfib-sysv-amd64.o']


#libcfib_a = env.StaticLibrary(target = 'cfib', source = libcfib_sources)
libcfib = env.SharedLibrary(target = 'cfib', source = libcfib_shared_sources)
libcfib = env.StaticLibrary(target = 'cfib', source = libcfib_static_sources)
test_obj = Object('test_cfib')
test_env = env.Clone();
test_env.Program(target = 'test_cfib_static', source = [test_obj, Dir('.').abspath + '/libcfib.a'])
test_env.Program(target = 'test_cfib', source = [test_obj], LIBS = ['cfib'], LIBPATH = ['.'])

