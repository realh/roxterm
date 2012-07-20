#  maitch.py - Simple but flexible build system tool
#  Copyright (C) 2012 Tony Houghton <h@realh.co.uk>
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of the GNU Lesser General Public License as published by the
#  Free Software Foundation; either version 3 of the License, or (at your
#  option) any later version.
#
#  This program is distributed in the hope that it will be useful, but WITHOUT
#  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
#  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
#  for more details.
#
#  You should have received a copy of the GNU Lesser General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

import atexit
import fnmatch
import os
import re
import shutil
import subprocess
import sys
import threading
import traceback
from curses import ascii


_mprint_lock = threading.Lock()
_mprint_fp = None

def mprint(*args, **kwargs):
    """ Equivalent to python 3's print but also works in python < 2.6 """
    global _mprint_lock
    with _mprint_lock:
        sep = kwargs.get('sep', ' ')
        end = kwargs.get('end', '\n')
        file = kwargs.get('file', sys.stdout)
        s = sep.join(args) + end
        file.write(s)
        file.flush()
        if _mprint_fp and file != _mprint_fp:
            _mprint_fp.write(s)


_debug = False
def dprint(*args, **kwargs):
    if _debug:
        mprint(*args, **kwargs)
    elif _mprint_fp:
        kwargs['file'] = _mprint_fp
        mprint(*args, **kwargs)


try:
    from lockfile import FileLock
    
    class FLock(FileLock):
        def __init__(self, *args, **kwargs):
            self.maitch_locked = False
            FileLock.__init__(self, *args, **kwargs)
        
        def acquire(self, timeout = None):
            FileLock.acquire(self, timeout)
            self.maitch_locked = True
        
        def release(self):
            if self.maitch_locked:
                try:
                    FileLock.release(self)
                except:
                    mprint("Lock was already released")
                self.maitch_locked = False
            
except:
    _nolock = True
else:
    _nolock = False



# Where to look for sources if a relative path is given and the file
# doesn't exist relative to cwd.
# May be combined with bitwise or.
NOWHERE = 0
SRC = 1
TOP = 2


class MaitchError(Exception):
    pass

class MaitchRuleError(MaitchError):
    pass

class MaitchArgError(MaitchError):
    pass

class MaitchChildError(MaitchError):
    pass

class MaitchDirError(MaitchError):
    pass

class MaitchNotFoundError(MaitchError):
    pass

class MaitchRecursionError(MaitchError):
    pass

class MaitchJobError(MaitchError):
    pass

class MaitchInstallError(MaitchError):
    pass



# subst behaviour if a variable isn't found
NOVAR_FATAL = 0     # Raise exception
NOVAR_BLANK = 1     # Replace with empty string
NOVAR_SKIP = 2      # Leave ${} construct untouched



class Context(object):
    " The fundamental build context. "
    
    def __init__(self, **kwargs):
        """
        All kwargs are added to context's env. They may include:
        
        PACKAGE = package name (compulsory).
        BUILD_DIR = where to output all generated files, defaults to
                "${MSCRIPT_REAL_DIR}/build". ${BUILD_DIR} is used as the cwd for
                all commands and other directories should be specified relative
                to it.
        TOP_DIR = top level of package directory, defaults to ".."
        SRC_DIR = top level of source directory, defaults to ${TOP_DIR}.
                Otherwise it should be specified relative to TOP_DIR to make
                sure dist stage works correctly.
        
        Special env variables. These are expanded in all modes, not just
        configure. They are not saved in the env file.
        
        MSCRIPT_DIR: The directory containing the executable script running
                the build (sys.argv[0]). Symlinks are not followed.
        MSCRIPT_REAL_DIR: MSCRIPT_DIR with symlinks followed.
        
        
        Useful attributes:
        
        env: context's variables
        mode: mode of build, one of "configure", "build", "install" or "dist";
                taken from sys.argv[1]
        build_dir, src_dir, top_dir: Expanded versions of above variables. Bear
                in mind that these are expanded before you have a chance to add
                any vars to env after constructing the ctx:- they may refer to
                each other (non-recursively) but to no other variables.
        
        Notes:
        
        All "nodes" (sources, targets etc) are expressed as strings.
        See process_nodes() for how they are accepted as arguments.
        """
        self.lock = threading.RLock()
        self.tmpfile_index = 0
        
        # Make sure a package name was specified
        self.package_name = kwargs['PACKAGE']
        
        self.created_by_config = {}
        self.installed = []
        self.tar_contents = []
        
        # Check mode
        if len(sys.argv) < 2:
            self.mode = 'help'
        else:
            self.mode = sys.argv[1]
            if not self.mode in \
                    "help configure reconfigure build dist install uninstall " \
                    "clean pristine".split():
                self.mode = 'help'
        if self.mode == 'help':
            ms = os.path.basename(sys.argv[0])
            mprint("Help for maitch:\nUSAGE:")
            for m in ["help", "configure", "reconfigure",
                    "build [TARGETS]", "install", "uninstall",
                    "clean", "pristine", "dist"]:
                mprint("  python %s %s" % (ms, m))
            mprint("""
Further variables may be set after the mode argument in the form VAR=value, or
VAR on its own for True. "--foo-bar" is equivalent to "FOO_BAR". Variables may
refer to each other eg FOO='${BAR}.baz'. Note that maitch does not read
variables directly from the environment. If you want to use, for example,
CFLAGS from the environment, add CFLAGS="$CFLAGS" to the configure command.

Variables can be set from the environment indirectly using MAITCHFLAGS. For
example:

export MAITCHFLAGS="CFLAGS=$CFLAGS;LDFLAGS=$LDFLAGS"

Note that multiple variables are separated by semicolons and you should not use
quoting within the MAITCHFLAGS string.
 
The most pivotal variable is BUILD_DIR which is the working directory and where
built files are saved. It will be created if necessary. If not specified it
defaults to a directory "build" in the same directory as %s
(symbolic links are followed).

TOP_DIR and SRC_DIR are the top level directory of the package, and the
subdirectory containing source files respectively. SRC_DIR is commonly
"${TOP_DIR}/src" or just "${TOP_DIR}". It is recommended that they are specified
as relative paths assuming a working directory of $BUILD_DIR. For example, when
the default BUILD_DIR is used, TOP_DIR='..'

Other predefined variables [default values shown in squarer brackets]:
""" % ms)
            for v in _var_repository:
                if v[3]:
                    alt = "/%s" % var_to_arg(v[0])
                else:
                    alt = ""
                if callable(v[1]):
                    default = 'dynamic'
                else:
                    default = v[1]
                print_wrapped("  %s%s [%s]: %s" %
                        (v[0], alt, default, v[2]),
                        80, 8, 0)
            self.showed_var_header = False
            return
        
        self.env = {}
        
        # From now on reconfigure is the same as configure
        if self.mode == 'reconfigure':
            self.mode = 'configure'
        
        # Process MAITCHFLAGS first because it's lowest priority and
        # subsequent processing will overwrite it
        mf = os.environ.get('MAITCHFLAGS')
        if mf:
            self.__process_args(mf.split(';'))
        
        # Get more env vars from kwargs
        for k, v in kwargs.items():
            self.env[k] = v
        
        self.explicit_rules = {}
        
        self.cli_targets = []
        
        # Process command-line args
        if len(sys.argv) > 2:
            cli_env = self.__process_args(sys.argv[2:])
        else:
            cli_env = {}
        
        # Everything hinges on BUILD_DIR
        self.get_build_dir()
        self.build_dir = os.path.abspath(self.subst(self.env['BUILD_DIR']))
        self.ensure_out_dir(self.build_dir)
        log_file = opj(self.build_dir, ".maitch", "log")
        self.ensure_out_dir_for_file(log_file)
        global _mprint_fp
        _mprint_fp = open(log_file, 'w')
        if self.mode != "dist":
            # This message is to help text editors find the cwd in case errors
            # are reported relative to it
            mprint('make[0]: Entering directory "%s"' % \
                    self.build_dir)
        os.chdir(self.build_dir)
        
        # Get lock on BUILD_DIR
        if not self.env.get('NO_LOCK'):
            global _nolock
            if _nolock:
                mprint("Error: lockfile module unavailable. "
                        "Please install it or, as a last resort, "
                        "use the --no-lock option:\n"
                        "%s %s --no-lock ..." % (sys.argv[0], sys.argv[1]),
                        file = sys.stderr)
                sys.exit(1)
            f = self.get_lock_file_name()
            self.ensure_out_dir_for_file(f)
            self.lock_file = FLock(f)
            self.lock_file.acquire(0)
            atexit.register(lambda x: x.release(), self.lock_file)
        else:
            mprint("Warning: Locking disabled. This is not recommended.")
        
        # If not in configure mode load a previous saved env. 
        if self.mode != 'configure':
            n = self.env_file_name()
            if os.path.exists(n):
                fp = open(n, 'r')
                for l in fp.readlines():
                    k, v = l.split('=', 1)
                    # Don't override command line or constructor
                    if not k in cli_env:
                        v = v.rstrip()
                        if v == 'True':
                            v = True
                        elif v == 'False':
                            v = False
                        elif len(v) \
                                and (ascii.isdigit(v[0]) \
                                    or (v[0] == '-' and len(v) > 1)) \
                                and all([ascii.isdigit(a) for a in v[1:]]):
                            v = int(v)
                        self.env[k] = v
                fp.close()
        
        # Set defaults
        for v in _var_repository:
            if not self.env.get(v[0]):
                if callable(v[1]):
                    d = v[1](v[0])
                else:
                    d = v[1]
                self.env[v[0]] = d
        
        self.top_dir = self.subst(self.env['TOP_DIR'])
        self.src_dir = self.subst(self.env['SRC_DIR'])
        self.check_build_dir()
        self.dest_dir = self.subst(self.env['DESTDIR'])
        
        self.definitions = {}
        
        self.created_by_config[opj(self.build_dir, ".maitch")] = True
        
        # In dist mode everything is relative to TOP_DIR
        if self.mode == "dist":
            self.env['TOP_DIR'] = os.curdir
            bd = os.path.abspath(self.build_dir)
            self.env['BUILD_DIR'] = bd
            td = os.path.abspath(self.top_dir)
            mprint('make[0]: Entering directory "%s"' % td)
            os.chdir(td)
        
        if self.env.get('ENABLE_DEBUG'):
            global _debug
            _debug = True
    

    def __process_args(self, args):
        cli_env = {}
        for a in args:
            if '=' in a:
                k, v = a.split('=', 1)
            elif self.mode != 'build' or a.startswith('--'):
                k = a
                v = True
            elif self.mode == 'build':
                self.cli_targets.append(a)
            if k.startswith('--'):
                k = arg_to_var(k)
            if self.var_is_special(k):
                raise MaitchDirError("%s is a reserved variable" % k)
            self.env[k] = v
            cli_env[k] = v
        return cli_env
    
    
    def define(self, key, value):
        """ Define a variable to be used in the build. At the moment the only
        supported method is to output a C header ${BUILD_DIR}/config.h at the
        end of the configure phase. Definitions are of the form:
        #define key value
        where double quotes are automatically added for string values unless
        already enclosed in single quotes. True and False are converted to 1 and
        0, while a value of None results in:
        #undef key
        being written instead. """
        self.definitions[key] = value
    
    
    def define_from_var(self, key, default = None):
        """ Calls self.define() with env variable's value. """
        self.define(key, self.env.get(key, default))
    
    
    def __arg(self, help_name, help_body, var, antivar, default):
        if self.mode == 'help':
            if not self.showed_var_header:
                mprint("\n%s supports these configure options:\n" %
                        self.package_name)
                self.showed_var_header = True
            print_formatted(help_body, 80, help_name, 20)
        else:
            if self.env.get(antivar):
                self.setenv(var, False)
            elif self.env.get(var) == None:
                if callable(default):
                    default = default(self, var)
                self.setenv(var, default)
    
    
    def arg_enable(self, name, help, var = None, default = False):
        """ Similar to autoconf's AC_ARG_ENABLE. Adds a variable which may
        be enabled/disabled on the configure command line with --enable-name
        or --disable-name. default may be a callable, in which case it will be
        called as default(context, var) if the argument isn't given on the
        command line, and var will be set to its return value. If var is not
        given, it's generated in the form ENABLE_NAME. Its value in the env will
        be 1 or 0. """
        if default == True:
            prefix = "disable"
        else:
            prefix = "enable"
        arg = "--%s-%s" % (prefix, name)
        if not var:
            var = 'ENABLE_' + s_to_var(name)
        self.__arg(arg, help, var, 'DISABLE' + var[6:], default)


    def arg_disable(self, name, help, var = None, default = True):
        """ Can be used as a shortcut for arg_enable with default = True or
        to force help string to be shown as --disable-name when default is
        callable. """
        if not var:
            var = 'ENABLE_' + s_to_var(name)
        self.__arg("--disable-%s" % name, help, var,
                'DISABLE' + var[6:], default)
    
    
    def arg_with(self, name, help, var = None, default = False):
        """ Like arg_enable but uses --with-name=... and --without-name=... """
        if not var:
            var = 'WITH_' + s_to_var(name)
        self.__arg("--with-%s=VALUE" % name, help, var,
                'WITHOUT' + var[4:], default)
    
    
    def save_if_different(self, filename, content):
        " Like global version but runs self.subst() on filename and content. "
        save_if_different(self.subst(filename), self.subst(content))
    
    
    def output_config_h(self):
        sentinel = "%s__CONFIG_H" % self.package_name.upper()
        keys = self.definitions.keys()
        keys.sort()
        s = "/* Auto-generated by maitch.py for %s */\n" \
                "#ifndef %s\n#define %s\n\n" % \
                (self.package_name, sentinel, sentinel)
        for k in keys:
            v = self.definitions[k]
            if v == None:
                s += "#undef %s\n\n" % k
                continue
            elif v == True:
                v = 1
            elif v == False:
                v = 0
            else:
                if isinstance(v, basestring):
                    if (v[0] != "'" or v[-1] != "'"):
                        v = '"' + v + '"'
                    v = self.subst(v)
            s += "#define %s %s\n\n" % (k, v)
        s += "#endif /* %s */\n" % sentinel
        filename = opj(self.build_dir, "config.h")
        self.save_if_different(filename, s)
        self.created_by_config[filename] = True
        
    
    @staticmethod
    def var_is_special(k):
        return k == "MSCRIPT_DIR" or k == "MSCRIPT_REAL_DIR"
        
    
    def check_build_dir(self):
        clash = None
        if self.top_dir == self.build_dir or self.top_dir == ".":
            clash = 'TOP_DIR'
        elif self.src_dir == self.build_dir or self.src_dir == ".":
            clash = 'SRC_DIR'
        if clash:
            mprint("WARNING: BUILD_DIR == %s, unable to clean" % clash,
                    file = sys.stderr)
            return False
        return True
    
    
    def tmpname(self):
        """ Returns the name of a temporary file in ${BUILD_DIR}/.maitch
        which is unique for this context. """
        with self.lock:
            self.tmpfile_index += 1
            i = self.tmpfile_index
        return opj(self.build_dir, ".maitch", "tmp%03d" % i)
        
    
    def get_build_dir(self, kwargs = None):
        """ Sets env's TOP_DIR and SRC_DIR, looking in kwargs or self.env,
        setting defaults if necessary. """
        if not kwargs:
            kwargs = self.env
        self.env['MSCRIPT_DIR'] = os.path.abspath(os.path.dirname(sys.argv[0]))
        self.env['MSCRIPT_REAL_DIR'] = \
                os.path.abspath(os.path.dirname(os.path.realpath(sys.argv[0])))
        bd = kwargs.get('BUILD_DIR')
        if bd:
            bd = os.path.abspath(bd)
        else:
            bd = opj("${MSCRIPT_REAL_DIR}", "build")
        self.env['BUILD_DIR'] = bd
        td = kwargs.get('TOP_DIR')
        if not td:
            td = os.pardir
        self.env['TOP_DIR'] = td
        sd = kwargs.get('SRC_DIR')
        if not sd:
            sd = "${TOP_DIR}"
        self.env['SRC_DIR'] = sd
    

    def env_file_name(self):
        """ Returns the filename of where env is saved, ensuring its 
        directory exists. """
        n = self.make_out_path(".maitch", "env")
        self.ensure_out_dir_for_file(n)
        return n
        
    
    def get_lock_file_name(self):
        if self.env.get("LOCK_TOP"):
            return os.path.abspath(opj(self.subst("${TOP_DIR}"), ".maitchlock"))
        else:
            return os.path.abspath(opj(self.subst("${BUILD_DIR}"),
                    ".maitch", "lock"))
    
    
    def release_lock(self, lock, lockname):
        try:
            lock.release()
        except:
            pass
        try:
            if os.path.isdir(lockname):
                os.unlink(lockname)
            else:
                recursively_remove(lockname, False, [])
        except:
            pass
        
    
    def add_rule(self, rule):
        " Adds an explicit rule. "
        for t in rule.targets:
            self.explicit_rules[self.subst(t)] = rule
        rule.ctx = self
        

    def glob(self, pattern, dir = None, subdir = None, expand = None):
        """ Returns a list of matching filenames relative to dir (default
        ${BUILD_DIR}). subdir, if given, is preserved at the start of each
        filename. expand is whether to use the substituted/expanded version
        of dir in the output. If not given/None it's True in all modes except
        dist. """
        if expand == None:
            expand = self.mode != "dist"
        if dir:
            orig_dir = dir
            dir = self.subst(dir)
        else:
            orig_dir = os.curdir
            dir = os.path.abspath(orig_dir)
        if subdir:
            dir = opj(dir, subdir)
        matches = fnmatch.filter(os.listdir(dir), pattern)
        if expand or subdir:
            if not expand:
                dir = subdir
            for i in range(len(matches)):
                matches[i] = opj(dir, matches[i])
        return matches
    
    
    def glob_src(self, pattern, subdir = None, expand = None):
        " Performs glob on src_dir. "
        return self.glob(pattern, self.src_dir, subdir, expand)
    
    
    def glob_all(self, pattern, subdir = None, expand = None):
        """ Performs glob on both build_dir/src_dir. No pathname component
        is added other than subdir. """
        matches = self.glob(pattern, self.build_dir, subdir, expand)
        for n in self.glob_src(pattern, subdir, expand):
            if not n in matches:
                matches.append(n)
        return matches
        
    
    def run(self):
        " Run whichever stage of the build was specified on CLI. "
        if self.mode == 'configure':
            fp = open(self.env_file_name(), 'w')
            for k, v in self.env.items():
                if not self.var_is_special(k):
                    fp.write("%s=%s\n" % (k, v))
            fp.close()
            if self.definitions:
                self.output_config_h()
            fp = open(opj(self.build_dir, ".maitch", "manifest"), 'w')
            for f in self.created_by_config.keys():
                fp.write("%s\n" % f)
            fp.close()
        elif self.mode == 'build':
            if len(self.cli_targets):
                tgts = self.cli_targets
            else:
                tgts = self.explicit_rules.keys()
            bg = BuildGroup(self, tgts)
            if bg.cancelled:
                sys.exit(1)
        elif self.mode == 'clean':
            filename = opj(self.build_dir, ".maitch", "manifest")
            if os.path.exists(filename):
                fp = open(filename, 'r')
                for f in fp.readlines():
                    self.created_by_config[f.strip()] = True
                fp.close()
            self.clean(False)
        elif self.mode == 'pristine':
            self.clean(False, True)
        elif self.mode == 'dist':
            self.make_tarball()
        elif self.mode == 'uninstall':
            self.uninstall()
    
    
    def make_tarball(self):
        import tarfile
        basedir = self.subst("${PACKAGE}-${VERSION}")
        filename = os.path.abspath(
                self.subst("${BUILD_DIR}/%s.tar.bz2" % basedir))
        mprint("Creating tarball '%s'" % filename)
        tar = tarfile.open(filename, 'w:bz2')
        for f, kwargs in self.tar_contents:
            kwargs = dict(kwargs)
            dest = kwargs.get('arcname')
            if dest:
                dest = os.path.normpath(opj(basedir, dest))
            else:
                dest = os.path.normpath(opj(basedir, f))
            kwargs['arcname'] = dest
            mprint("Adding '%s' to tarball" % dest)
            tar.add(self.subst(f), **kwargs)
        tar.close()
    
    
    def add_dist(self, objects, **kwargs):
        if isinstance(objects, basestring):
            objects = objects.split()
        for o in objects:
            self.tar_contents.append([o, kwargs])
    
    
    def clean(self, fatal, pristine = False):
        if pristine:
            # Release lock prematurely because we're about to delete it!
            self.lock_file.release()
            keep = []
        else:
            keep = self.created_by_config
        if not self.check_build_dir():
            return
        recursively_remove(self.build_dir, fatal, keep)
        recursively_remove(opj(self.build_dir, ".maitch", "deps"), fatal, [])
                
    
    def ensure_out_dir(self, *args):
        """ Esnures the named directory exists. args is a single string or list
        of strings. Single string may be absolute in which case it isn't
        altered. Otherwise a path is made absolute using BUILD_DIR. """
        if isinstance(args, basestring) and os.path.isabs(args):
            path = args
        else:
            path = self.make_out_path(*args)
        with self.lock:
            if not os.path.isdir(path):
                os.makedirs(path)


    def ensure_out_dir_for_file(self, path):
        """ As above but uses a single string only and acts on its
        parent directory. """
        self.ensure_out_dir(os.path.dirname(path))
    
    
    def make_out_path(self, *args):
        """ Creates an absolute filename from BUILD_DIR and args (single
        string or list). """
        if isinstance(args, basestring):
            args = (args)
        args = (self.build_dir,) + args
        return self.subst(opj(*args))
    
    
    def find_prog(self, prog, expand = False):
        " Finds a binary in context's PATH. prog not expanded by default. "
        mprint("Searching for program %s... " % prog, end = '')
        if expand:
            prog = self.subst(prog)
        try:
            p = find_prog(prog, self.env)
            mprint("%s" % p)
        except:
            mprint("not found")
            raise
        return p
    
    
    def find_prog_env(self, prog, var = None, expand = False):
        """ Finds a program and sets an env var to its full path. If the var
        is not specified a capitalised (etc) transform of the program name
        is used. """
        if not var:
            var = s_to_var(prog)
        self.env[var] = self.find_prog(prog, expand)
    
    
    def prog_output(self, prog, use_shell = False):
        """ Runs a program and returns its [stdout, stderr]. prog should be a
        list or single string (former will not be split at spaces). Each
        component will be expanded. cwd is set to BUILD_DIR. """
        if isinstance(prog, basestring):
            prog = prog.split()
        for i in range(len(prog)):
            prog[i] = self.subst(prog[i])
        if not os.path.isabs(prog[0]):
            prog[0] = self.find_prog(prog[0], False)
        proc = subprocess.Popen(prog,
                stdout = subprocess.PIPE, stderr = subprocess.PIPE,
                shell = use_shell, cwd = self.build_dir)
        result = proc.communicate()
        if proc.returncode:
            raise MaitchChildError("%s failed:\n%d:\n%s" % \
                    (' '.join(prog), proc.returncode, result[1].strip()))
        return result
    
    
    def prog_to_var(self, prog, var, use_shell = False):
        " As prog_output(), storing stripped result into self.env[var]. "
        output = self.prog_output(prog, use_shell)[0].strip()
        dprint("prog_to_var: input: %s" % ' '.join(prog))
        dprint("prog_to_var: output: %s" % output)
        self.env[var] = output
    
    
    def pkg_config(self, pkgs, prefix = None, version = None,
            pkg_config = "${PKG_CONFIG}"):
        """ Runs pkg-config (or optionally a similar tool) and sets
        ${prefix}_CFLAGS to the output of --flags and ${prefix}_LIBS
        to the output of --libs. If prefix is None, derive it from pkg
        eg "gobject-2.0 sqlite3" becomes GOBJECT_2_0_SQLITE3.
        If version is given, also check that package's version is at least
        as new (only works on one package at a time). """
        mprint("Checking pkg-config %s..." % pkgs, end = '')
        try:
            if version:
                try:
                    pvs = self.prog_output(
                            [pkg_config, '--modversion', pkgs])[0].strip()
                except:
                    mprint("not found")
                pkg_v = pvs.split('.')
                v = version.split('.')
                new_enough = True
                for n in range(len(v)):
                    if v[n] > pkg_v[n]:
                        new_enough = True
                        break
                    elif v[n] < pkg_v[n]:
                        new_enough = False
                        break
                    if not new_enough:
                        mprint("too old")
                        raise MaitchPkgError("%s has version %s, "
                                "%s needs at least %s" %
                                (pkgs, pvs, self.package_name, version))
            if not prefix:
                prefix = make_var_name(pkg, True)
            pkgs = pkgs.split()
            self.prog_to_var([pkg_config, '--cflags'] + pkgs,
                    prefix + '_CFLAGS')
            self.prog_to_var([pkg_config, '--libs'] + pkgs,
                    prefix + '_LIBS')
        except:
            mprint("error")
        else:
            mprint("ok")
    
    
    def subst(self, s, novar = NOVAR_FATAL, recurse = True, at = False):
        " Runs global subst() using self.env. "
        return subst(self.env, s, novar, recurse, at)
    
    
    def deps_from_cpp(self, sources, cppflags = None):
        """ Runs "${CDEP} sources" and returns its dependencies
        (one file per line). filename should be absolute. If cppflags is
        None, self.env['CPPFLAGS'] is used. """
        if not cppflags:
            cppflags = self.env.get('CPPFLAGS', "")
        sources = process_nodes(sources)
        sources = [self.find_source(s) for s in sources]
        if not sources:
            sources = []
        prog = (self.subst("${CDEP} %s" % cppflags)).split() + sources
        deps = self.prog_output(prog)[0]
        deps = deps.split(':', 1)[1].replace('\\', '').split()
        return deps
    
    
    def find_sys_header(self, header, cflags = None):
        """ Uses deps_from_cpp() to find the full path of a header in the
        include path. Returns None if not found. """
        mprint("Looking for header '%s'... " % header, end = '')
        tmp = self.tmpname() + ".c"
        fp = open(tmp, 'w')
        fp.write("#include <%s>\n" % header)
        fp.close()
        deps = self.deps_from_cpp(tmp, cflags)
        os.unlink(tmp)
        for d in deps:
            if d.endswith(os.sep + header):
                mprint("%s" % d)
                return d
        mprint("not found")
        return None
    
    
    def check_compile(self, code, msg = None, cflags = None, libs = None):
        """ Checks whether the program code can be compiled as C, returns
        True or False. If msg is given prints "Checking msg... ". """
        if msg:
            mprint("Checking %s... " % msg, end = '')
        if not cflags:
            cflags = self.env.get('CFLAGS', "")
        if not libs:
            libs = self.env.get('LIBS', "")
        tmp = self.tmpname()
        fp = open(tmp + ".c", 'w')
        fp.write(code)
        fp.close()
        prog = self.subst("${CC} -o %s %s %s %s" %
                (tmp, tmp + ".c", libs, cflags)).split()
        try:
            self.prog_output(prog)
        except MaitchChildError as e:
            if msg:
                mprint("no")
            dprint("check_compile failed:\n%s" % str(e))
            dprint("code was:\n%s" % code)
            result = False
        else:
            if msg:
                mprint("yes")
            try:
                os.unlink(tmp)
            except OSError:
                pass
            result = True
        os.unlink(tmp + ".c")
        return result


    def check_header(self, header, cflags = None, libs = None):
        """ Uses check_compile to test whether header is available, calling
        self.define('HAVE_HEADER_H', v) where v is 1 or None and HEADER_H is
        derived from header by make_var_name(). Also sets env var with the
        same name to True or False. """
        if self.check_compile("""#include "%s"
int main() { return 0; }
""" % header,
                "for header '%s'" % header, cflags, libs):
            present = 1
        else:
            present = None
        nm = make_var_name('HAVE_' + header, True)
        self.define(nm, present)
        self.setenv(nm, present == 1)
        return present == 1


    def check_func(self, func, cflags = None, libs = None, includes = None):
        """ Like check_header but for a function. includes is an optional
        list of header names. """
        code = ""
        if includes:
            for i in includes:
                code += '#include "%s"\n' % i
        code += """
int main() { %s(); return 0; }
""" % func
        if self.check_compile(code, "for function %s()" % func, cflags, libs):
            present = 1
        else:
            present = None
        nm = make_var_name('HAVE_' + func, True)
        self.define(nm, present)
        self.setenv(nm, present == 1)
        return present == 1
    
    
    def find_source(self, name, where = SRC, fatal = True):
        """ Finds a source file relative to BUILD_DIR (higher priority) or
        SRC_DIR, returning full path or raising exception if not found. Returns
        absolute paths unchanged. If fatal is False, unfound files are
        returned unchanged"""
        name = self.subst(name)
        if os.path.exists(name):
            return name
        if os.path.isabs(name):
            if os.path.exists(name):
                return name
            else:
                self.not_found(name)
        p = opj(self.build_dir, name)
        if os.path.exists(p):
            return p
        if where & SRC:
            p = opj(self.src_dir, name)
            if os.path.exists(p):
                return p
        if where & TOP:
            p = opj(self.top_dir, name)
            if os.path.exists(p):
                return p
        if fatal:
            self.not_found(name)
        else:
            return name
    
    
    @staticmethod
    def not_found(name):
        raise MaitchNotFoundError("Resource '%s' cannot be found" % name)
        
    
    def setenv(self, k, v):
        " Sets an env var. "
        self.env[k] = v
    
    
    def getenv(self, k, default = None):
        " Returns an env var. "
        return self.env.get(k)
    
    
    def get_stamp(self, name, where = NOWHERE):
        """ Returns the mtime for named file. Uses find_source() and subst and
        therefore may raise MaitchNotFoundError or KeyError. """
        n = self.find_source(self.subst(name), where)
        return os.stat(name).st_mtime


    def get_extreme_stamp(self, nodes, comparator, where = NOWHERE):
        """ Like global version, but runs subst() and find_source() on each
        item. Items that don't exist are skipped because some suffix rules
        don't necessarily use all sources or targets (eg gob2 doesn't always
        output a private header). """
        pnodes = []
        for n in nodes:
            try:
                pnodes.append(self.find_source(self.subst(n), where))
            except MaitchNotFoundError:
                pass
        return get_extreme_stamp(pnodes, comparator)
    
    
    def get_oldest(self, nodes, where = NOWHERE):
        """ Like global version, but runs subst() and find_source() on each
        item, so may raise MaitchNotFoundError or KeyError. """
        return self.get_extreme_stamp(nodes, lambda a, b: a < b, where)
    
    
    def get_newest(self, nodes, where = NOWHERE):
        """ Like global version, but runs subst() and find_source() on each
        item, so may raise MaitchNotFoundError or KeyError. """
        return self.get_extreme_stamp(nodes, lambda a, b: a > b, where)
    
    
    def subst_file(self, source, target, at = False):
        """ As global version, using self.env. """
        subst_file(self.env, source, target, at)
        self.created_by_config[self.subst(target)] = True
        
    
    def install(self, directory, sources = None,
            mode = None, libtool = False, other_options = None):
        """ Uses the install program to install files. Default mode is install's
        default mode, which in turn defaults to rwxr-xr-x. sources may be string
        with multiple files separated by spaces, or a list, or None to create
        directory. other_options, which may also be a string or a list, are
        additional options for install.
        Automatically creates target directories.
        If other_options contains -T, directory is the target filename.
        In uninstall mode each installed file/directory is instead added to a
        list which will be deleted in reverse order when run() is called. """
        directory = process_nodes(directory)
        if self.dest_dir:
            directory = [self.dest_dir + os.sep + d for d in directory]
        directory = [os.path.abspath(d) for d in directory]
        if not sources:
            sources = []
        elif isinstance(sources, basestring):
            sources = sources.split()
        if len(directory) > 1 and sources:
            raise MaitchInstallError("Can't install files to multiple " \
                    "directories")
        if len(sources) > 1 and other_options and "-T" in other_options:
            raise MaitchInstallError("Can only install one file where " \
                    "target is a filename instead of a directory")
        if self.mode == 'uninstall':
            if other_options and "-T" in other_options:
                f = [directory[0]]
            elif sources:
                f = [opj(directory[0], os.path.basename(f)) \
                        for f in sources]
            else:
                f = directory
            self.installed.append([f, libtool])
            return
        sources = [self.find_source(s, TOP | SRC, False) for s in sources]
        if libtool:
            cmd = ["${LIBTOOL}", "--mode=install", "${INSTALL}"]
        else:
            cmd = ["${INSTALL}"]
        if isinstance(other_options, basestring):
            cmd += other_options.split()
        elif other_options:
            cmd += list(other_options)
        if mode:
            cmd += ["-m", str(mode)]
        if not sources:
            cmd.append("-d")
        cmd += sources + directory
        if sources:
            if other_options and "-T" in other_options:
                dir = os.path.dirname(directory[0])
            else:
                dir = directory[0]
            if not os.path.isdir(dir):
                dd = self.dest_dir
                self.dest_dir = None
                try:
                    self.install(dir)
                except:
                    raise
                finally:
                    self.dest_dir = dd
        cmd = [self.subst(c) for c in cmd]
        mprint("%s" % ' '.join(cmd))
        if subprocess.call(cmd, cwd = self.build_dir) != 0:
            raise MaitchChildError("install failed")
    
    
    def install_bin(self, sources, directory = "${BINDIR}", mode = None,
            libtool = True, other_options = None):
        self.install(directory, sources, mode, libtool, other_options)
    

    def install_lib(self, sources, directory = "${LIBDIR}", mode = '0644',
            libtool = True, other_options = None):
        self.install(directory, sources, mode, libtool, other_options)
    

    def install_data(self, sources, directory = "${PKGDATADIR}", mode = '0644',
            libtool = False, other_options = None):
        self.install(directory, sources, mode, libtool, other_options)
    

    def install_doc(self, sources, directory = "${DOCDIR}", mode = '0644',
            libtool = False, other_options = None):
        self.install(directory, sources, mode, libtool, other_options)
    
    
    def install_man(self, sources, directory = None, mode = '0644',
            other_options = None):
        if not directory:
            directory = "${MANDIR}"
        sources = process_nodes(sources)
        dirs = {}
        for s in sources:
            if s.endswith('.gz'):
                f = s[:-3]
            else:
                f = s
            sec = f.rsplit('.', 1)[1]
            d = dirs.get(sec)
            if not d:
                d = []
                dirs[sec] = d
            d.append(s)
        for d, s in dirs.items():
            self.install(opj(directory, "man%d" % int(d)), s, '0644')
            
    
    def uninstall(self):
        if not self.installed:
            mprint("Nothing to uninstall")
            return
        self.installed.reverse()
        for fs, libtool in self.installed:
            if libtool:
                cmd = ["${LIBTOOL}", "--mode=uninstall", "rm", "-f"] + fs
                cmd = [self.subst(c) for c in cmd]
                mprint("%s" % ' '.join(cmd))
                subprocess.call(cmd, cwd = self.build_dir)
            else:
                fs = [self.subst(f) for f in fs]
                if os.path.isdir(fs[0]):
                    # If first is directory, then so are the rest
                    rm = os.rmdir
                else:
                    rm = os.unlink
                for f in fs:
                    self.delete(f, rm)

    
    def delete(self, f, rm = None):
        """ Delete a file, which is processed with self.subst(). The default
        for rm is os.unlink, use os.rmdir for directories. """
        if not rm:
            rm = os.unlink
        f = self.subst(f)
        try:
            rm(f)
        except OSError:
            mprint("Failed to delete '%s'" % f)
        else:
            mprint("Removed '%s'" % f)
    
    
    def recursively_remove(self, target, fatal = False, excep = []):
        recursively_remove(self.subst(target), fatal,
                [self.subst(e) for e in excep])
    
    
    def prune_directory(self, root):
        """ Expands variables in root and prefixes ${DESTDIR} before
        calling global version. """
        # Note we can't use opj because root is (usually) absolute
        root = os.path.normpath(self.subst("${DESTDIR}" + os.sep + root))
        mprint("Removing empty directories from '%s'" % root)
        return prune_directory(root)
    


class Rule(object):
    """
    Standard rule.
    """
    def __init__(self, **kwargs):
        """
        Possible arguments (all optional except targets):
        rule: A function or a string containing a command to be called.
                A string should contain "${TGT}" and/or "${SRC}" for
                targets and sources. This will not work if they contain spaces.
                A function is called as func(ctx, env, [targets], [sources])
                where ctx is the Context and targets and soures are expanded.
                Note env is not the same as ctx.env, it will have ${TGT} and
                ${SRC} added.
                An additional variable, ${TGT_DIR}, is set to the directory
                name of the first target. Note ${TGT_DIR} is absolute but
                ${TGT} is not.
                rule may be a heterogeneous list of functions or strings.
        sources: See process_nodes() func for what's valid.
        targets: As above, or a function which takes source(s) as input and
                returns target(s).
        deps: Dependencies other than sources; deps are satisfied before
                sources.
        wdeps: "Weak dependencies":- the targets will not be built until after
                all wdeps are built, but the targets don't necessarily depend
                on the wdeps. This makes sure dynamic dependency tracking
                works properly when some headers are generated by other steps
                eg by making C compiler rules wdep on all files generated by
                gob2.
        dep_func: A function to calculate implicit deps; takes a ctx and rule
                as arguments and returns a list of filenames - absolute or
                relative to BUILD_DIR. This is used only to work out whether the
                target is out of date and needs to be rebuilt; it is not used
                when working out the dependency chain.
        use_shell: Whether to use shell if rule is a string (default False).
        env: A dict containing env vars to override those from Context. Each
                var may refer to its own name in its value to derive values
                from the Context.
        quiet: If True don't print the command about to be executed.
        where: Where to look for sources: SRC, TOP or both (default SRC).
        lock: Some jobs can't be run simultaneously so you can pass in a Lock
                object to prevent that.
        diffpat: If this is given (a list or string) it invokes special
                behaviour. For each target T, if T already exists it's backed
                up to T.old before running the rule. Then T is compared
                line-by-line with T.old. Lines containing a match for a regex
                in diffpat are considered equal. If the files are equal T is
                replaced by T.old and touched.
                This is to prevent VCS conflicts when a build changes a
                POT-Creation-Date but not the content.
        """
        if not 'where' in kwargs:
            kwargs['where'] = SRC
        rule = kwargs.get('rule')
        if callable(rule) or isinstance(rule, basestring):
            self.rules = [rule]
        else:
            self.rules = rule
        self.sources = kwargs.get('sources')
        if self.sources and isinstance(self.sources, basestring):
            self.sources = process_nodes(self.sources)
        self.targets = kwargs['targets']
        if callable(self.targets):
            self.targets = self.targets(self.sources)
        elif isinstance(self.targets, basestring):
            self.targets = process_nodes(self.targets)
        self.deps = kwargs.get('deps')
        if self.deps and isinstance(self.deps, basestring):
            self.deps = process_nodes(self.deps)
        self.wdeps = kwargs.get('wdeps')
        if self.wdeps and isinstance(self.wdeps, basestring):
            self.wdeps = process_nodes(self.wdeps)
        self.dep_func = kwargs.get('dep_func')
        self.use_shell = kwargs.get('use_shell')
        self.env = kwargs.get('env')
        self.quiet = kwargs.get('quiet')
        self.blocking = []
        self.blocked_by = []
        self.completed = False
        self.cached_env = None
        self.cached_targets = None
        self.cached_sources = None
        self.cached_deps = []
        self.where = kwargs['where']
        self.lock = kwargs.get('lock')
        diffpat = kwargs.get('diffpat')
        if isinstance(diffpat, basestring):
            diffpat = [diffpat]
        if diffpat:
            self.diffpat = [re.compile(d) for d in diffpat]
        else:
            self.diffpat = None
    
    
    @staticmethod
    def init_var(kwargs, var):
        """ Call this from subclass' constructor if it uses var (specified in
        lower case). If kwargs includes a key 'var' (lower case) env[VAR_] is
        set to its value. Note the trailing _. This is because a variable
        overridden here would commonly want to refer to itself (as ${VAR}) but
        can't due to recursion. So accompanying variables should refer to one
        set here with the trailing _. If kw var is not present, env[VAR_] is set
        to ${VAR}. See CRule's use of libs and cflags for an example. """
        val = kwargs.get(var)
        if not val:
            val = "${%s}" % var.upper()
        env = kwargs.get('env')
        if not env:
            env = {}
            kwargs['env'] = env
        env[var.upper() + '_'] = val


    def init_cflags(self, kwargs):
        " For rules which use cflags. "
        self.init_var(kwargs, 'cflags')


    def init_libs(self, kwargs):
        " For rules which use libs. "
        self.init_var(kwargs, 'libs')
    
    
    def process_env(self):
        " Merges env with ctx's, with caching. Does not set TGT or SRC. "
        if self.cached_env == None:
            env = dict(self.ctx.env)
            if self.env:
                for k, v in self.env.items():
                    env[k] = v
            self.cached_env = env
        return self.cached_env
        
        
    def process_env_tgt_src(self):
        """ As process_env, but also returns expanded targets and sources
        [env, targets sources] and adds them to env. """
        env = self.process_env()
        if self.cached_targets == None:
            # Expand targets and sources, finding the sources, and prefixing
            # targets with build_dir.
            targets = [subst(env, t) for t in self.targets]
            if self.sources:
                sources = [self.ctx.find_source(subst(env, s), self.where) \
                        for s in self.sources]
            else:
                sources = None
            # Add them to env
            if sources:
                env['SRC'] = ' '.join(sources)
            else:
                env['SRC'] = ''
            if targets:
                env['TGT'] = ' '.join(targets)
                td = os.path.dirname(targets[0])
                if not os.path.isabs(td):
                    td = opj(self.ctx.build_dir, td)
                env['TGT_DIR'] = td
            else:
                env['TGT'] = ''
                env['TGT_DIR'] = ''
            self.cached_targets = targets
            self.cached_sources = sources
        return [self.cached_env, self.cached_targets, self.cached_sources]
    
    
    def run(self):
        " Run a job. "
        if self.is_uptodate():
            return
        env, targets, sources = self.process_env_tgt_src()        
        if self.lock:
            self.lock.acquire()
        if self.diffpat:
            for t in self.targets:
                if os.path.exists(t):
                    shutil.copy(t, t + '.old')
        try:
            for rule in self.rules:
                if callable(rule):
                    if not self.quiet:
                        mprint("Internal function: %s(%s, %s)" %
                                (rule.__name__, str(targets), str(sources)))
                    rule(self.ctx, env, targets, sources)
                else:
                    r = subst(env, rule)
                    if not self.quiet:
                        mprint(r)
                    if self.use_shell:
                        prog = r
                    else:
                        prog = r.split()
                    if call_subprocess(prog,
                            shell = self.use_shell, cwd = self.ctx.build_dir):
                        dprint("%s NZ error code" % r)
                        raise MaitchChildError("Rule '%s' failed" % rule)
                    else:
                        dprint("%s executed successfully" % r)
        except:
            raise
        else:
            if self.diffpat:
                for t in self.targets:
                    same = False
                    s = t + '.old'
                    if os.path.exists(t) and os.path.exists(s):
                        f = open(s, 'r')
                        u = f.readlines()
                        f.close()
                        f = open(t, 'r')
                        v = f.readlines()
                        f.close()
                        if len(u) == len(v):
                            same = True
                            for n in range(len(u)):
                                if u[n] != v[n]:
                                    same = False
                                    for p in self.diffpat:
                                        if p.search(u[n]) and p.search(v[n]):
                                            same = True
                                            break
                                    if not same:
                                        break
                        if same:
                            os.rename(s, t)
                        else:
                            os.unlink(s)
        finally:
            if self.lock:
                self.lock.release()
    
    
    def __repr__(self):
        return "JT:%s" % str(self.targets)
        #return "T:%s S:%s" % (self.targets, self.sources)
    
    
    def list_static_deps(self):
        """ Works out a rule's static dependencies ie deps + sources, but not
        dep_func. Caches them and returns list. """
        if self.deps:
            deps = self.deps
        else:
            deps = []
        if self.sources:
            deps += self.sources
        self.cached_deps = deps
        return deps
        
    
    def is_uptodate(self):
        """ Returns False if any target is older than any source, dep, or result
        of dep_func (implicit/dynamic deps). """
        # If there are no dependencies target(s) must be rebuilt every time
        if not self.cached_deps:
            return False
        # First find whether uptodate wrt static deps.
        # If result is True extra variable newest_dep is available
        uptodate = True
        try:
            oldest_target = self.ctx.get_oldest(self.targets, NOWHERE)
        except MaitchNotFoundError, KeyError:
            oldest_target = None
        if not oldest_target:
            uptodate = False
        if uptodate:
            # All sources and deps should be available at this point so
            # OK to let exception propagate
            newest_dep = self.ctx.get_newest(self.cached_deps, self.where)
            if newest_dep and newest_dep > oldest_target:
                uptodate = False
        else:
            newest_dep = None
        
        if self.dep_func:
            # Work out whether cached dynamic deps need updating
            dyn_deps = None
            deps_name = self.ctx.make_out_path(self.ctx.subst(
                    opj(".maitch", "deps", self.targets[0])))
            if os.path.exists(deps_name):
                deps_stamp = os.stat(deps_name).st_mtime
                if not newest_dep:
                    newest_dep = self.ctx.get_newest(self.cached_deps,
                            self.where)
                if newest_dep and newest_dep > deps_stamp:
                    # sources/static deps are newer than dynamics
                    rebuild_deps = True
                else:
                    # see whether cached deps are older than any file they list
                    dyn_deps = load_deps(deps_name)
                    newest_impl_dep = get_newest(dyn_deps)
                    if newest_impl_dep > newest_dep:
                        newest_dep = newest_impl_dep
                    rebuild_deps = newest_impl_dep > deps_stamp
            else:
                rebuild_deps = True
            if rebuild_deps:
                dyn_deps = self.dep_func(self.ctx, self)
                self.ctx.ensure_out_dir_for_file(deps_name)
                fp = open(deps_name, 'w')
                for d in dyn_deps:
                    fp.write(d + '\n')
                fp.close()
            if not dyn_deps:
                dyn_deps = load_deps(deps_name)
            # Now we have up-to-date implicit dyn_deps, check whether targets
            # are older than them
            if get_newest(dyn_deps) > oldest_target:
                uptodate = False
        
        return uptodate
                
        

class SuffixRule(Rule):
    """ A rule where targets is a function that transforms sources by changing
    their suffix, and optionally prefix. """
    def __init__(self, **kwargs):
        """ args are similar to those for a standard Rule but targets should
        not be specified. deps are complete filenames as with explicit Rules.
        sources and rule are compulsory.
        
        suffix (compulsory): Suffix to be applied to targets. Source suffixes to
                be stripped are inferred by looking for last period. Do not
                include period, it's implied.
        prefix (optional): Added to the start of the targets' leafname(s) -
                after any directory components. Any separator between the prefix
                and leafname (eg '-') should be included at the end of the
                prefix. """
        self.suffix = kwargs['suffix']
        self.prefix = kwargs.get('prefix', '')
        set_default(kwargs, 'targets', self.transform)
        Rule.__init__(self, **kwargs)
    
    
    def transform(self, sources):
        targets = []
        for s in sources:
            s = s.rsplit('.', 1)[0]
            if len(self.prefix):
                p, l = os.path.split(s)
                l = self.prefix + l
                if p:
                    s = os.path.join(p, l)
                else:
                    s = l
            targets.append(s + '.' + self.suffix)
        return targets
    


class TouchRule(Rule):
    """ Use this as a dummy rule rather than use a Rule with no rule parameter.
    The target(s) will be "touched" to update their timestamp. """
    def __init__(self, **kwargs):
        set_default(kwargs, 'rule', self.touch)
        Rule.__init__(self, **kwargs)
    
    
    def touch(self, ctx, env, tgts, srcs):
        for t in tgts:
            fp = open(t, 'w')
            fp.close()
                


class CRuleBase(SuffixRule):
    " Standard rule for compiling C to an object file. "
    def __init__(self, **kwargs):
        self.flagsname = kwargs['flagsname']
        set_default(kwargs, 'rule', "${%s} ${%s_} -c -o ${TGT} ${SRC}" %
                (kwargs['compiler'], self.flagsname))
        set_default(kwargs, 'suffix', "o")
        set_default(kwargs, 'dep_func', self.get_implicit_deps)
        SuffixRule.__init__(self, **kwargs)
     
    
    def get_implicit_deps(self, ctx, rule):
        # rule will be a static rule generated from self with a single source
        env = self.process_env()
        try:
            return ctx.deps_from_cpp(rule.sources, env[self.flagsname + '_'])
        except MaitchNotFoundError:
            return None



class CRule(CRuleBase):
    " Standard rule for compiling C to an object file. "
    def __init__(self, **kwargs):
        self.init_cflags(kwargs)
        set_default(kwargs, 'compiler', "CC");
        set_default(kwargs, 'flagsname', "CFLAGS");
        CRuleBase.__init__(self, **kwargs)



class CxxRule(CRuleBase):
    " Standard rule for compiling C++ to an object file. "
    def __init__(self, **kwargs):
        self.init_var(kwargs, 'cxxflags')
        set_default(kwargs, 'compiler', "CXX");
        set_default(kwargs, 'flagsname', "CXXFLAGS");
        CRuleBase.__init__(self, **kwargs)



class LibtoolCRuleBase(CRuleBase):
    def __init__(self, **kwargs):
        """ Additional kwargs:
            libtool_flags.
            libtool_mode_arg: -shared, -static etc.
        """
        self.init_var(kwargs, 'libtool_flags')
        self.init_var(kwargs, 'libtool_mode_arg')
        set_default(kwargs, 'suffix', "lo")
        set_default(kwargs, 'rule',
                "${LIBTOOL} --mode=compile --tag=%s ${LIBTOOL_FLAGS_} "
                "${%s} ${LIBTOOL_MODE_ARG_} ${%s_} -c -o ${TGT} ${SRC}" %
                (kwargs['tag'], kwargs['compiler'], kwargs['flagsname']))
        CRuleBase.__init__(self, **kwargs)



class LibtoolCRule(LibtoolCRuleBase):
    " libtool version of CRule. "
    def __init__(self, **kwargs):
        """ Additional kwargs:
            libtool_flags.
            libtool_mode_arg: -shared, -static etc.
        """
        self.init_cflags(kwargs)
        set_default(kwargs, 'compiler', "CC");
        set_default(kwargs, 'flagsname', "CFLAGS");
        set_default(kwargs, 'tag', "CC");
        LibtoolCRuleBase.__init__(self, **kwargs)



class LibtoolCxxRule(LibtoolCRuleBase):
    " libtool version of CxxRule. "
    def __init__(self, **kwargs):
        """ Additional kwargs:
            libtool_flags.
            libtool_mode_arg: -shared, -static etc.
        """
        self.init_var(kwargs, 'cxxflags')
        set_default(kwargs, 'compiler', "CXX");
        set_default(kwargs, 'flagsname', "CXXFLAGS");
        set_default(kwargs, 'tag', "CXX");
        LibtoolCRuleBase.__init__(self, **kwargs)



class ShlibCRule(LibtoolCRule):
    " Compiles C into an object file suitable for a shared library. "
    def __init__(self, **kwargs):
        kwargs['libtool_mode_arg'] = "-shared %s" % \
                kwargs.get('libtool_mode_arg', '')
        LibtoolCRule.__init__(self, **kwargs)



class StaticLibCRule(LibtoolCRule):
    " Compiles C into an object file suitable for a static library. "
    def __init__(self, **kwargs):
        kwargs['libtool_mode_arg'] = "-static %s" % \
                kwargs.get('libtool_mode_arg', '')
        LibtoolCRule.__init__(self, **kwargs)



class ShlibCxxRule(LibtoolCxxRule):
    " Compiles C++ into an object file suitable for a shared library. "
    def __init__(self, **kwargs):
        kwargs['libtool_mode_arg'] = "-shared %s" % \
                kwargs.get('libtool_mode_arg', '')
        LibtoolCxxRule.__init__(self, **kwargs)



class StaticLibCxxRule(LibtoolCxxRule):
    " Compiles C++ into an object file suitable for a static library. "
    def __init__(self, **kwargs):
        kwargs['libtool_mode_arg'] = "-static %s" % \
                kwargs.get('libtool_mode_arg', '')
        LibtoolCxxRule.__init__(self, **kwargs)



class ProgramRuleBase(Rule):
    " Standard rule for linking mutiple object files and libs into a program. "
    def __init__(self, **kwargs):
        self.init_libs(kwargs)
        self.init_var(kwargs, 'ldflags')
        set_default(kwargs, 'rule',
                "${%s} ${%s_} ${LIBS_} ${LDFLAGS_} -o ${TGT} ${SRC}" %
                (kwargs['linker'], kwargs['flagsname']))
        Rule.__init__(self, **kwargs)



class CProgramRule(ProgramRuleBase):
    "Standard rule for linking C object files and libs into a program."
    def __init__(self, **kwargs):
        self.init_cflags(kwargs)
        set_default(kwargs, 'linker', "CC");
        set_default(kwargs, 'flagsname', "CFLAGS");
        ProgramRuleBase.__init__(self, **kwargs)



class CxxProgramRule(ProgramRuleBase):
    "Standard rule for linking C++ object files and libs into a program."
    def __init__(self, **kwargs):
        self.init_var(kwargs, 'cxxflags')
        set_default(kwargs, 'linker', "CXX");
        set_default(kwargs, 'flagsname', "CXXFLAGS");
        ProgramRuleBase.__init__(self, **kwargs)



class LibtoolProgramRuleBase(ProgramRuleBase):
    " Use libtool during linking. "
    def __init__(self, **kwargs):
        self.init_var(kwargs, 'libtool_flags')
        self.init_var(kwargs, 'libtool_mode_arg')
        set_default(kwargs, 'rule',
                "${LIBTOOL} --mode=link --tag=%s ${LIBTOOL_FLAGS_} "
                "${%s} ${LIBTOOL_MODE_ARG_} "
                "${%s_} ${LIBS_} ${LDFLAGS_} -o ${TGT} ${SRC}" %
                (kwargs['tag'], kwargs['linker'], kwargs['flagsname']))
        ProgramRuleBase.__init__(self, **kwargs)



class LibtoolCProgramRule(LibtoolProgramRuleBase):
    " Use libtool to link C. "
    def __init__(self, **kwargs):
        self.init_cflags(kwargs)
        set_default(kwargs, 'linker', "CC");
        set_default(kwargs, 'flagsname', "CFLAGS");
        set_default(kwargs, 'tag', "CC");
        LibtoolProgramRuleBase.__init__(self, **kwargs)



class LibtoolCxxProgramRule(LibtoolProgramRuleBase):
    " Use libtool to link C. "
    def __init__(self, **kwargs):
        self.init_var(kwargs, 'cxxflags')
        set_default(kwargs, 'linker', "CXX");
        set_default(kwargs, 'flagsname', "CXXFLAGS");
        set_default(kwargs, 'tag', "CXX");
        LibtoolProgramRuleBase.__init__(self, **kwargs)



class CShlibRule(LibtoolCProgramRule):
    """ Standard rule to create a shared C library with libtool. See the
    libtool manual for an explanation of its interface version system.
    -release is not supported (yet). """
    def __init__(self, **kwargs):
        version = kwargs.get('libtool_version')
        if version:
            version = "-version-info %d:%d:%d" % tuple(version)
        else:
            version = ""
        kwargs['libtool_mode_arg'] = "-shared %s %s" % \
                (kwargs.get('libtool_mode_arg', ''), version)
        LibtoolCProgramRule.__init__(self, **kwargs)



class CxxShlibRule(LibtoolCxxProgramRule):
    """ Standard rule to create a shared C++ library with libtool. See the
    libtool manual for an explanation of its interface version system.
    -release is not supported (yet). """
    def __init__(self, **kwargs):
        version = kwargs.get('libtool_version')
        if version:
            version = "-version-info %d:%d:%d" % tuple(version)
        else:
            version = ""
        kwargs['libtool_mode_arg'] = "-shared %s %s" % \
                (kwargs.get('libtool_mode_arg', ''), version)
        LibtoolCxxProgramRule.__init__(self, **kwargs)



class CStaticLibRule(LibtoolCProgramRule):
    """ Standard rule to create a static C library with libtool. See the
    libtool manual for an explanation of its interface version system.
    -release is not supported (yet). """
    def __init__(self, **kwargs):
        version = kwargs.get('libtool_version')
        if version:
            version = "-version-info %d:%d:%d" % tuple(version)
        else:
            version = ""
        kwargs['libtool_mode_arg'] = "-static %s %s" % \
                (kwargs.get('libtool_mode_arg', ''), version)
        LibtoolCProgramRule.__init__(self, **kwargs)



class CxxStaticLibRule(LibtoolCxxProgramRule):
    """ Standard rule to create a static C++ library with libtool. See the
    libtool manual for an explanation of its interface version system.
    -release is not supported (yet). """
    def __init__(self, **kwargs):
        version = kwargs.get('libtool_version')
        if version:
            version = "-version-info %d:%d:%d" % tuple(version)
        else:
            version = ""
        kwargs['libtool_mode_arg'] = "-static %s %s" % \
                (kwargs.get('libtool_mode_arg', ''), version)
        LibtoolCxxProgramRule.__init__(self, **kwargs)



def mkdir_rule(ctx, env, targets, sources):
    """ A rule function to ensure targets' directories exist. """
    for t in targets:
        ctx.ensure_out_dir_for_file(t)



def PotRules(ctx, **kwargs):
    """ Returns a pair (list) of rules for generating a .pot file from a list of
    source files eg POTFILES.in where filenames are relative to TOP_DIR. Default
    source is "${TOP_DIR}/po/POTFILES.in", default target is
    ${TOP_DIR}/po/${PACKAGE}.pot.
    rule defaults to ${XGETTEXT}, may be overridden - note that source for this
    rule is generated POTFILES, not POTFILES.in.
    XGETTEXT is not set by default. *_XGETTEXT_OPTS is generated on the fly.
    
    Special args:
    copyright_holder
    package
    version
    bugs_addr
    opts_prefix: Goes on front of _XGETTEXT_OPTS added to env.
    
    '^"POT-Creation-Date:' is added to a new or existing diffpat.
    """
    def generate_potfiles(ctx, env, targets, sources):
        potfiles = []
        for s in sources:
            fp = open(subst(env, s), 'r')
            for f in fp.readlines():
                f = f.strip()
                if len(f) and f[0] != '#':
                    potfiles.append(opj(env['TOP_DIR'], f.strip()))
            fp.close()
        t = subst(env, targets[0])
        ctx.ensure_out_dir_for_file(t)
        fp = open(t, 'w')
        for f in potfiles:
            fp.write("%s\n" % f)
        fp.close()
    
    def pot_deps(ctx, rule):
        deps = []
        for s in process_nodes(rule.sources):
            fp = open(ctx.subst(s), 'r')
            for f in fp.readlines():
                deps.append(f.strip())
        return deps
    
    opts_prefix = kwargs.get('opts_prefix', "")
    varname = "%s_XGETTEXT_OPTS" % opts_prefix
    if not 'where' in kwargs:
        kwargs['where'] = SRC | TOP
    try:
        src1 = kwargs['sources']
    except KeyError:
        src1 = ["${TOP_DIR}/po/POTFILES.in"]
    rule1 = Rule(rule = generate_potfiles,
            sources = src1,
            targets = ["${BUILD_DIR}/po/POTFILES"],
            where = kwargs['where'])
    
    kwargs['sources'] = ["${BUILD_DIR}/po/POTFILES"]
    if not 'targets' in kwargs:
        kwargs['targets'] = ["${TOP_DIR}/po/${PACKAGE}.pot"]
    if not 'rule' in kwargs:
        kwargs['rule'] = "${XGETTEXT} ${%s} -f ${SRC} -o ${TGT}" % varname
    if not 'dep_func' in kwargs:
        kwargs['dep_func'] = pot_deps
    xgto = []
    copyrt = kwargs.get('copyright_holder')
    if copyrt:
        xgto.append('--copyright-holder=%s' % copyrt)
    pkg = kwargs.get('package')
    if not pkg:
        pkg = "${PACKAGE}"
    xgto.append('--package-name=${PACKAGE}')
    ver = kwargs.get('version')
    if ver:
        xgto.append('--package-version=%s' % ver)
    ba = kwargs.get('bugs_addr')
    if ba:
        xgto.append('--msgid-bugs-address=%s' % ba)
    for n in range(len(xgto)):
        s = ctx.subst(xgto[n])
        if any([ascii.isspace(c) for c in s]):
            if "'" in s:
                if '"' in s:
                    raise MaitchRuleError(
                            "Unable to quote xgettext option: %s" % s)
                xgto[n] = '"%s"' % s
            else:
                xgto[n] = "'%s'" % s
            kwargs['use_shell'] = True
    ctx.setenv(varname, ' '.join(xgto))
    diffpat = kwargs.get('diffpat')
    if not diffpat:
        diffpat = []
        kwargs['diffpat'] = diffpat
    elif isinstance(diffpat, basestring):
        diffpat = [diffpat]
        kwargs['diffpat'] = diffpat
    if not '^"POT-Creation-Date:' in diffpat:
        diffpat.append('^"POT-Creation-Date:')
    rule2 = Rule(**kwargs)
    
    return [rule1, rule2]



class PoRule(Rule):
    """ Updates a po file from a pot (default src is
    ${TOP_DIR}/po/${PACKAGE}.pot). """
    def __init__(self, *args, **kwargs):
        if not 'sources' in kwargs:
            kwargs['sources'] = "${TOP_DIR}/po/${PACKAGE}.pot"
        if not 'rule' in kwargs:
            kwargs['rule'] = "${MSGMERGE} -q -U ${TGT} ${SRC}"
        diffpat = kwargs.get('diffpat')
        if not diffpat:
            diffpat = []
            kwargs['diffpat'] = diffpat
        elif isinstance(diffpat, basestring):
            diffpat = [diffpat]
            kwargs['diffpat'] = diffpat
        if not '^"POT-Creation-Date:' in diffpat:
            diffpat.append('^"POT-Creation-Date:')
        Rule.__init__(self, *args, **kwargs)



def parse_linguas(ctx, podir = None, linguas = None):
    if not podir:
        podir = opj("${TOP_DIR}", "po")
    if not linguas:
        linguas = opj(podir, "LINGUAS")
    langs = []
    fp = open(ctx.subst(linguas), 'r')
    for l in fp.readlines():
        l = l.strip()
        if not l or l[0] == '#':
            continue
        langs.append(l)
    fp.close()
    return langs



def PoRulesFromLinguas(ctx, *args, **kwargs):
    """ Generates a PoRule for each language listed in linguas. Default linguas
    is podir/LINGUAS. Default podir is ${TOP_DIR}/po. Other args are passed to
    each PoRule. Also generates .mo rules unless nomo is True. """
    podir = kwargs.get('podir')
    if not podir:
        podir = opj("${TOP_DIR}", "po")
    linguas = kwargs.get('linguas')
    if not linguas:
        linguas = opj(podir, "LINGUAS")
    nomo = kwargs.get("nomo")
    langs = parse_linguas(ctx, linguas = linguas)
    rules = []
    for l in langs:
        f = opj(podir, l + ".po")
        if not os.path.isfile(ctx.subst(f)):
            raise MaitchNotFoundError("Linguas file %s includes %s, "
                    "but no corresponding po file found" % (linguas, l))
        kwargs['targets'] = f
        rules.append(PoRule(*args, **kwargs))
        if not nomo:
            rules.append(Rule(rule = "${MSGFMT} -c -o ${TGT} ${SRC}",
                    targets = opj("po", l + ".mo"),
                    sources = f))
    return rules



def StandardTranslationRules(ctx, *args, **kwargs):
    """ Returns all rules necessary to build translation files. kwargs are as
    for PotRules + PoRulesForLinguas """
    return PotRules(ctx, *args, **kwargs) + \
            PoRulesFromLinguas(ctx, *args, **kwargs)
 


def call_subprocess(*args, **kwargs):
    """ Like subprocess.Call() with stdout and stderr logged.
    stdout and stderr in kwargs are overridden. """
    kwargs['stdout'] = subprocess.PIPE
    kwargs['stderr'] = subprocess.PIPE
    sp = subprocess.Popen(*args, **kwargs)
    dprint("%s has pid %d" % (args, sp.pid))
    [out, err] = sp.communicate()
    dprint("pid %d finished, output follows" % sp.pid)
    out = out.strip()
    if out: 
        mprint(out)
    err = err.strip()
    if err: 
        mprint(err, file = sys.stderr)
    return sp.returncode


def report_exception():
    """ Reports the last raised exception. """
    mprint(traceback.format_exc(), file = sys.stderr)
    
    

def print_formatted(body, columns = 80, heading = None, h_columns = 20):
    """ Prints body, wrapped at the specified number of columns. If heading is
    given, h_columns are reserved on the left for it. """
    if heading and len(heading) >= h_columns:
        print_wrapped(heading, columns)
        print_wrapped(body, columns, h_columns)
    elif heading:
        print_wrapped(heading + ' ' * (h_columns - len(heading)) + body,
                columns, h_columns, 0)
    else:
        print_wrapped(body, columns, h_columns)


def print_wrapped(s, columns = 80, indent = 0, first_indent = None,
        file = sys.stdout):
    """ Prints s wrapped to fit in columns, optionally indented,
    with an optional alternative indent for the first line. """
    if first_indent == None:
        first_indent = indent
    i = ' ' * first_indent
    current_indent = first_indent
    while len(s) > columns - current_indent:
        l = s[:columns - current_indent]
        s = s[columns - current_indent:]
        if ascii.isspace(l[-1]):
            l = l[:-1]
            split = []
        else:
            split = l.rsplit(None, 1)
        if len(split) > 1:
            l = split[0]
            s = split[1] + s
        file.write("%s%s\n" % (i, l))
        i = ' ' * indent
        current_indent = indent
    if s:
        file.write("%s%s\n" % (i, s))
    file.flush()



def subst_file(env, source, target, at = False):
    """ Run during configure phase to create a copy of source as target
    with ${} constructs substituted. Uses save_if_different() to avoid
    unnecessary rebuilds. """
    fp = open(subst(env, source), 'r')
    s = fp.read()
    fp.close()
    save_if_different(subst(env, target), subst(env, s, at = at))
    
    
def save_if_different(filename, content):
    """ Saves content to filename, only if file doesn't already contain
    identical content. """
    if os.path.exists(filename):
        fp = open(filename, 'r')
        old = fp.read()
        fp.close()
    else:
        old = None
    if content != old:
        if old == None:
            mprint("Creating '%s'" % filename)
        else:
            mprint("Updating '%s'" % filename)
        fp = open(filename, 'w')
        fp.write(content)
        fp.close()
    else:
        mprint("'%s' is unchanged" % filename)
            


def set_default(d, k, v):
    " Sets d[k] = v only if d doesn't already contain k. "
    if not d.get(k):
        d[k] = v



def opj(*args):
    """ Like os.path.join and also calls os.path.normpath """
    return os.path.normpath(os.path.join(*args))
    
    
    
def prune_directory(root):
    """ Recursively moves all empty directories at root. Returns False if
    remaining files meant one or more directories could not be deleted. """
    if not os.path.exists(root):
        return True
    success = True
    subs = os.listdir(root)
    for n in subs:
        n = opj(root, n)
        if os.path.isdir(n):
            success = prune_directory(n) and success
        elif os.path.exists(n):
            mprint("Unable to remove non-empty directory '%s'" % root,
                    file = sys.stderr)
            success = False
    if success:
        try:
            os.rmdir(root)
        except:
            mprint("Unable to remove directory '%s'" % root, file = sys.stderr)
            success = False
    return success

    

_subst_re = re.compile(r"(\$\{-?([a-zA-Z0-9_]+)\})")
_subst_re_at = re.compile(r"(@-?([a-zA-Z0-9_]+)@)")

def subst(env, s, novar = NOVAR_FATAL, recurse = True, at = False):
    """
    Processes string s, substituting ${var} with values from dict env
    with var as the key. To prevent substitution put a '-' after the
    opening brace. It will be removed. Most variables are expanded
    recursively immediately before use.
    See comment where NOVAR_ constants for description of novar.
    If at is True, substitute @VAR@ instead.
    """
    def ms(match):
        s = match.group(2)
        if s[0] == '-':
            if at:
                return "@%s@" % s[1:]
            else:
                return "${%s}" % s[1:]
        elif novar == NOVAR_FATAL:
            return env[s]
        else:
            if novar == NOVAR_BLANK:
                dr = ''
            else:
                dr = match.group(0)
            return env.get(s, dr)
    if at:
        rex = _subst_re_at
    else:
        rex = _subst_re
    result, n = rex.subn(ms, s)
    if recurse and n:
        return subst(env, result, novar, True, at)
    return result



def process_nodes(nodes):
    """
    nodes may be a string with a single node filename, several names
    separated by spaces, or a list of strings - one per node, each may
    contain spaces. Returns a list of nodes.
    """
    if isinstance(nodes, basestring):
        return nodes.split()
    else:
        return list(nodes)


def recursively_remove(path, fatal, excep):
    """ Deletes path and all its contents, leaving behind exceptions.
    Returns False if path could not be deleted because it contains an
    exception. If something can not be deleted only propagate exception
    if fatal is True. """
    if fatal:
        contents = os.listdir(path)
    else:
        try:
            contents = os.listdir(path)
        except:
            return True
    removable = True
    if contents:
        for f in contents:
            f = opj(path, f)
            if f in excep:
                removable = False
            elif os.path.isdir(f):
                if not recursively_remove(f, fatal, excep):
                    removable = False
            elif fatal:
                os.unlink(f)
            else:
                try:
                    os.unlink(f)
                except OSError:
                    removable = False
    if removable:
        if fatal:
            os.rmdir(path)
        else:
            try:
                os.rmdir(path)
            except OSError:
                removable = False
    return removable
        

def find_prog(name, env = os.environ):
    if os.path.isabs(name):
        return name
    path = env.get('PATH', '/bin:/usr/bin:/usr/local/bin')
    for p in path.split(':'):
        n = opj(p, name)
        if os.path.exists(n):
            return n
    raise MaitchNotFoundError("%s program not found in PATH" % name)


_var_repository = []

def add_var(name, default = "", desc = "", as_arg = False):
    """
    Registers a variable name with a default value which will be used if the
    variable isn't specified on the command-line. If as_arg is True it can
    be specified as a -- option ie --foo-bar=baz is equivalent to the usual
    form FOO_BAR=baz. default may be a function, called as function(name)
    to calculate the default value dynamically.
    """
    global _var_repository
    _var_repository.append([name, default, desc, as_arg])


_prog_var_repository = {}

def add_prog(var, prog):
    global _prog_var_repository
    _prog_var_repository[var] = prog


def find_prog_by_var(var):
    p = _prog_var_repository.get(var)
    if not p:
        p = var_to_s(var)
    return find_prog(p)


def s_to_var(s):
    return s.upper().replace('-', '_').replace('.', '_').replace(' ', '_')

def var_to_s(s):
    return s.lower().replace('_', '-')

def arg_to_var(s):
    return s_to_var(s[2:])

def var_to_arg(s):
    return '--' + var_to_s(s)



def make_var_name(template, upper = False):
    """ Makes an eligible variable name from template by replacing special
    characters with '_' and also prepending a '_' if template has a leading
    digit. ALso optionally converts to upper case. """
    if ascii.isdigit(template[0]):
        s = "_"
    else:
        s = ""
    for c in template:
        if c != '_' and not ascii.isalnum(c):
            s += "_"
        else:
            s += c
    if upper:
        s = s.upper()
    return s
    
    

def change_suffix(files, old, new):
    """ Returns a new list of files with old suffix replaced with new
    (include the '.' in the parameters). files may be a space-separated string
    or a list/tuple of strings. Non-matching suffixes are left unchanged. """
    changed = []
    if isinstance(files, basestring):
        files = files.split()
    l = len(old)
    for f in files:
        if f.endswith(old):
            f = f[:-l] + new
        changed.append(f)
    return changed


def add_prefix(files, prefix):
    """ Returns a new list of files with prefix added to leafname.
    files may be a space-separated string or a list/tuple of strings. """
    changed = []
    if isinstance(files, basestring):
        files = files.split()
    changed = []
    for f in files:
        head, tail = os.path.split(f)
        changed.append(opj(head, prefix + tail))
    return changed


def change_suffix_with_prefix(files, old, new, prefix):
    return add_prefix(change_suffix(files, old, new), prefix)



def get_extreme_stamp(nodes, comparator):
    """ Gets stamp for each item in nodes and returns the highest
    according to comparator. Returns None for an empty list or raises
    MaitchNotFoundError if any member is not found. """
    stamp = None
    for n in nodes:
        if not os.path.exists(n):
            raise MaitchNotFoundError("Can't find '%s' to get timestamp" % n)
        s = os.stat(n).st_mtime
        if stamp == None:
            stamp = s
        else:
            r = comparator(s, stamp)
            if r == True or r > 0:
                stamp = s
    return stamp


def get_oldest(nodes):
    """ See get_extreme_stamp(). """
    return get_extreme_stamp(nodes, lambda a, b: a < b)


def get_newest(nodes):
    """ See get_extreme_stamp(). """
    return get_extreme_stamp(nodes, lambda a, b: a > b)


def load_deps(f):
    """ Loads a list of filenames from file with absolute name f. """
    fp = open(f, 'r')
    deps = []
    while 1:
        l = fp.readline()
        if l:
            deps.append(l.strip())
        else:
            break
    fp.close()
    return deps



class BuildGroup(object):
    " Class to organise the build, arranging tasks in order of dependency. "
    def __init__(self, ctx, targets):
        """ ctx is the context containing info for the build. targets is a
        list of targets. """
        self.ctx = ctx
        
        # cond is used to allow builders to wait until a new task is ready
        # and to access the queue in a thread-safe way
        self.cond = threading.Condition(ctx.lock)
        
        # ready_queue isn't really ordered, jobs are just appended as they
        # become unblocked. This is nice and simple.
        # Additionally all jobs are added to ready_queue before checking
        # up-to-dateness; this is checked just before they would run and
        # up-to-date jobs are skipped and handled as if they were just run.
        self.ready_queue = []
        
        # quick way to find whether a target is due to be, or has been, built
        self.queued = {}
        
        # Jobs are added here until unblocked, then moved to ready_queue
        self.pending_jobs = []
        
        # Add jobs
        for tgt in targets:
            self.add_target(tgt)
        
        # Start builders
        self.cancelled = False
        self.threads = []
        n = int(self.ctx.env.get('PARALLEL', 1))
        if n < 1:
            n = 1
        mprint("Starting %d build thread(s)" % n)
        for n in range(n):
            t = Builder(self)
            self.threads.append(t)
            t.start()
        
        # Wait for threads; doesn't matter what order they finish in
        for t in self.threads:
            dprint("Main loop waiting for %s to finish" % t.describe())
            t.join()
        
    
    def add_target(self, target):
        if not target in self.queued:
            self.add_job(self.ctx.explicit_rules[target])
    
    
    def add_job(self, job):
        if job in self.pending_jobs:
            return
        job.calculating = True
        with self.cond:
            self.pending_jobs.append(job)
        for t in job.targets:
            self.queued[self.ctx.subst(t)] = job
        self.satisfy_deps(job)
        job.calculating = False
        if not len(job.blocked_by):
            self.do_while_locked(self.make_job_ready, job)
    
    
    def do_while_locked(self, func, *args, **kwargs):
        """ Lock cond and run func(*args, **kwargs), handling errors in a
        thread-safe way. Returns result of func. """
        self.cond.acquire()
        try:
            result = func(*args, **kwargs)
        except:
            self.cancel_all_jobs()
            self.cond.release()
            raise
        self.cond.release()
        return result
    
    
    def cancel_all_jobs(self):
        dprint("Cancelling all jobs")
        # Cond.notify_all() sometimes freezes all threads instead of
        # awakening them, so only choice is to exit
        with self.cond:
            if not self.cancelled:
                self.cancelled = True
                self.ready_queue = []
                self.pending_jobs = []
                self.cond.notify_all()
        dprint("Leaving cancel_all_jobs")
        
    
    def make_job_ready(self, job):
        " Moves a job from pending_jobs to ready_queue. "
        with self.cond:
            # We may still get here after all jobs have been cancelled due
            # to an error
            try:
                self.pending_jobs.remove(job)
            except ValueError:
                pass
            else:
                self.ready_queue.append(job)
            self.cond.notify()
    
    
    def satisfy_deps(self, job):
        if job.wdeps:
            deps = job.wdeps
        else:
            deps = []
        sdeps = job.list_static_deps()
        if sdeps:
            deps += sdeps
        if not len(deps):
            return
        for dep in deps:
            dep = self.ctx.subst(dep)
            try:
                # Has a rule to build this dep already been queued?
                rule = self.queued.get(dep)
                if rule:
                    if rule.calculating:
                        raise MaitchRecursionError("%s and %s have circular "
                                "dependencies" % (rule, dep))
                    self.mark_blocking(job, rule)
                    continue
                    
                # Is there an explicit rule for it?
                rule = self.ctx.explicit_rules.get(dep)
                
                if rule:
                    self.mark_blocking(job, rule)
                    self.add_job(rule)
                    continue
                
                # Does file already exist?
                self.ctx.find_source(dep, job.where)
            except:
                mprint("Error trying to satisfy dep '%s' of '%s'" % (dep, job),
                        file = sys.stderr)
                raise
            
    
    def mark_blocking(self, blocked, blocker):
        with self.cond:
            if not blocked in blocker.blocking:
                blocker.blocking.append(blocked)
            if not blocker in blocked.blocked_by:
                blocked.blocked_by.append(blocker)
    
    
    def job_done(self, job):
        for j in job.blocking:
            self.do_while_locked(self.unblock_job, j, job)
        if not len(self.ready_queue) and not len(self.pending_jobs):
            with self.cond:
                self.cond.notify_all()
    
    
    def unblock_job(self, blocked, blocker):
        """ 'blocked' is no longer blocked by 'blocker'. Call while locked.
        blocker is not altered, but if blocked has no remaining blockers it's
        moved to ready_queue. """
        try:
            blocked.blocked_by.remove(blocker)
        except ValueError:
            raise MaitchError(
                    "%s was supposed to be blocked by %s but wasn't" %
                    (blocked, blocker))
        if not len(blocked.blocked_by):
            self.make_job_ready(blocked)



_thread_index = 0

class Builder(threading.Thread):
    """ A Builder starts a new thread which waits for jobs to be added to
    BuildGroup's ready_queue, pops one at a time and runs it. """
    
    def __init__(self, build_group):
        global builder_index
        with build_group.cond:
            global _thread_index
            self.index = _thread_index
            _thread_index += 1
        self.bg = build_group
        threading.Thread.__init__(self)
    
    
    def describe(self):
        return "Builder Thread %d" % self.index
    
    
    def run(self):
        finished = False
        dprint("Starting %s" % self.describe())
        try:
            while not finished:
                job = None
                dprint("%s acquiring cond" % self.describe())
                with self.bg.cond:
                    if not len(self.bg.ready_queue):
                        if len(self.bg.pending_jobs):
                            dprint("%s waiting for jobs" % self.describe())
                            self.bg.cond.wait()
                            dprint("%s awoken" % self.describe())
                    # Check both queues again, because they could have both
                    # changed while we were waiting
                    if not len(self.bg.ready_queue) and \
                            not len(self.bg.pending_jobs):
                        # No jobs left, we're done
                        dprint("%s finished" % self.describe())
                        finished = True
                    elif len(self.bg.ready_queue):
                        job = self.bg.ready_queue.pop()
                dprint("%s released cond" % self.describe())
                if job:
                    dprint("%s running job %s" % (self.describe(), str(job)))
                    job.run()
                    dprint("%s completed job %s" % (self.describe(), str(job)))
                    self.bg.job_done(job)
                    dprint("%s marked job %s done" % \
                            (self.describe(), str(job)))
        except:
            dprint("Exception in %s" % self.describe())
            report_exception()
            self.bg.cancel_all_jobs()
        dprint("%s finished" % self.describe())
            


# Commonly used variables

add_var('PARALLEL', '0', "Number of build operations to run at once", True)
add_var('PREFIX', '/usr/local', "Base directory for installation", True)
add_var('BINDIR', '${PREFIX}/bin', "Installation directory for binaries", True)
add_var('LIBDIR', '${PREFIX}/lib',
        "Installation directory for shared libraries "
        "(you should use multiarch where possible)", True)
add_var('SYSCONFDIR', '/etc',
        "Installation directory for system config files", True)
add_var('MANDIR', '${DATADIR}/man',
        "Installation directory for man pages", True)
add_var('DATADIR', '${PREFIX}/share',
        "Installation directory for data files", True)
add_var('LOCALEDIR', '${DATADIR}/locale',
        "Installation directory for locale data", True)
add_var('PKGDATADIR', '${PREFIX}/share/${PACKAGE}',
        "Installation directory for this package's data files", True)
add_var('DOCDIR', '${PREFIX}/share/doc/${PACKAGE}',
        "Installation directory for this package's documentation", True)
add_var('HTMLDIR', '${DOCDIR}',
        "Installation directory for this package's HTML documentation", True)
add_var('DESTDIR', '',
        "Prepended to prefix at installation (for packaging)", True)
add_var('LOCK_TOP', False, "Lock ${TOP_DIR} instead of ${BUILD_DIR}")
add_var('NO_LOCK', False, "Disable locking (not recommended)")
add_var('ENABLE_DEBUG', False, "Enable the printing of maitch debug messages")
add_var('CC', '${GCC}', "C compiler")
add_var('CXX', 'g++', "C++ compiler")
add_var('GCC', find_prog_by_var, "GNU C compiler")
add_var('CPP', find_prog_by_var, "C preprocessor")
add_var('LIBTOOL', find_prog_by_var, "libtool compiler frontend for libraries")
add_var('CDEP', '${CPP} -M', "C preprocessor with option to print deps")
add_var('CFLAGS', '', "C compiler flags")
add_var('CPPFLAGS', '', "C preprocessor flags")
add_var('LIBS', '', "libraries and linker options")
add_var('LDFLAGS', '', "Extra linker options")
add_var('CXXFLAGS', '${CFLAGS}', "C++ compiler flags")
add_var('LIBTOOL_MODE_ARG', '', "Default libtool mode argument(s)")
add_var('LIBTOOL_FLAGS', '', "Additional libtool flags")
add_var('PKG_CONFIG', find_prog_by_var, "pkg-config")
add_var('INSTALL', find_prog_by_var, "install program")

