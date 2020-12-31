#!/usr/bin/env python

from subprocess import call
import toml
import os
import argparse
import pathlib


LIB_NAME = "magic"

deps_list = []

build_dependencies = {}
submodules = {}
found_libs = {}


def create_null_main():
    f = open("src/main.cpp", "w")
    f.write("""
    int main() {

    }
    """)
    f.close()


def create_null_config():
    default_config = {
        "dependencies": {},
        "apps": [{"name": "test", "src": ["src/*.cc"], "deps": []}]
    }
    f = open("%s.toml" % LIB_NAME, "w")
    toml.dump(default_config, f)
    f.close()


def configure_deps_dir(s):
    cmake_file = open("deps/deps.cmake", "w")
    cmake_file.write(
        """cmake_minimum_required(VERSION 3.2)\ninclude(ExternalProject)\n""")

    for k in build_dependencies:
        b = build_dependencies[k]
        b.configure_ext(cmake_file)
        b.write_include(cmake_file)
        cmake_file.write("\n")

    for k in submodules:
        b = submodules[k]
#        b.install_submodule()
        b.write_include(cmake_file)
        cmake_file.write("\n")

    for e in extern_map:
        b = extern_map[e]
        print("write", b)
        b.write_include(cmake_file)
        cmake_file.write("\n")

    includes = load_includes(s)
    for i in includes:
        i.configure_include(cmake_file)
        i.write_include(cmake_file)
    cmake_file.write("\n")
    cmake_file.close()


def configure_deps(s):
    f = open(s)
    deps = (toml.load(f))["dependencies"]

    cmake_file = open("deps/deps.cmake", "w")
    cmake_file.write(
        """cmake_minimum_required(VERSION 3.2)\ninclude(ExternalProject)\n""")
    for k in deps:
        if "gitlink" in deps[k]:
            call(["git", "submodule", "add", deps[k]["gitlink"], "deps/%s" % k])
            content = \
                """
set(%s_DIR "${CMAKE_SOURCE_DIR}/deps/%s")
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/%s)
include_directories(${CMAKE_SOURCE_DIR}/deps/%s/%s)
""" % (k, k, k, k, deps[k]["include_dir"])
            cmake_file.write(content)
        else:
            deps_list.append(k)
            dep = deps[k]
            cmake_file.write(
                """set(%s_INSTALL_DIR ${CMAKE_SOURCE_DIR}/deps/%s)\n""" % (k, k))
            cmake_file.write("""ExternalProject_Add(%s
               URL %s
               CONFIGURE_COMMAND %s --prefix=${%s_INSTALL_DIR}
               BUILD_COMMAND %s
               BUILD_IN_SOURCE 1
               INSTALL_COMMAND %s )\n"""
                             % (k, dep["url"], dep["configure_cmd"], k, dep["build_cmd"], dep["install_cmd"]))

            # add include dir
            cmake_file.write(
                """include_directories(BEFORE ${%s_INSTALL_DIR}/include)""" % k)

    cmake_file.close()
    return deps.keys()


def write_header(f):
    f.write("cmake_minimum_required(VERSION 3.2)\n")
    f.write("project(xxx)\n")
    f.write("ADD_DEFINITIONS(-std=c++17)\n")
    f.write("include(deps/deps.cmake)\n")
    f.write('include_directories("src")\n')
    f.write('include_directories("deps")\n')
    f.write('set(CMAKE_CXX_FLAGS "-O2 -g -mrtm")\n')


def write_srcs(f, app, srcs, cmake_vars=[]):
    f.write('file(GLOB %s_SORUCES "" ' % app)
    for s in srcs:
        f.write(' "%s" ' % s)
    f.write(")\n")
    f.write("add_executable(%s ${%s} " % (app, "%s_SORUCES" % app))
    for v in cmake_vars:
        f.write(v + " ")
    f.write(")\n")


def write_deps(deps, app, f):
    try:
        print("write deps: ", deps, "for ", app)
        if len(deps) > 0:
            f.write("add_dependencies(%s " % app)
            for d in deps:
                f.write("%s " % d)
            f.write(")\n")

    except:
        pass


def write_libs(f, app, libs):
    print("write dep libs:", libs, app)
    temp = []
    for l in libs:
        if l in export_map:
            temp.append(export_map[l].write_lib(f, l))
        else:
            # find library in the OS path
            temp.append(l)
    f.write("target_link_libraries(%s " % app)
    for l in temp:
        f.write(l + " ")
    f.write(")\n")


def write_tests(f):
    content = \
        """
## tests
enable_testing()

add_test(NAME test COMMAND coretest)
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --verbose
                  DEPENDS coretest )\n
"""
    f.write(content)


def configure_main(s):
    f = open(s)
    cmake_file = open("CMakeLists.txt", "w")
    write_header(cmake_file)

    apps = []
    try:
        apps = (toml.load(f))["apps"]
    except Exception as e:
        print("load apps error", str(e))
        pass
    for a in apps:
        name = a['name']
        #libs = a['deps']
        extras = []
        if 'extra' in a:
            extras = a['extra']
        write_srcs(cmake_file, name, a['src'])
#        //write_deps(cmake_file,name,extras)
        try:
            write_deps(a['deps'], name, cmake_file)
        except:
            pass
        write_extra_deps(extras, cmake_file, name)
#        find_libs(cmake_file,name,libs,deps)
#        write_libs(cmake_file,name,extras)
#    cmake_file.write("include(tests/tests.cmake)\n")
#    write_tests(cmake_file)

    cmake_file.close()


def find_libs(f, app, libs, deps):
    f.write("find_library(%s_DEPS NAMES " % app)
    for i in libs:
        f.write(i + " ")
    f.write("HINTS ")
    for d in deps:
        f.write("deps/%s/lib " % d)
    f.write(")\n")


def mkdir_if_not_exsists(dir):
    try:
        os.mkdir(dir)
    except:
        pass


def new():
    mkdir_if_not_exsists("src")
    mkdir_if_not_exsists("tests")
    mkdir_if_not_exsists("deps")

    # create_null_main()
    # create_null_config()

    # if not os.path.exists(".git"):
    # call(["git","init"])


def configure(s="config.toml"):
    load_downloads(s)
    load_externals(s)
    load_installs(s)
    #deps = configure_deps(s)
    configure_deps_dir(s)
    configure_main(s)
    tests_template(s)


export_map = {}
extern_map = {}


class External:
    def __init__(self, name, ext):
        self.name = name
        self.path = ext["path"]
        for i in ext["exports"]:
            export_map[i] = self
        self.lib_path = ""
        if "lib" in ext:
            self.lib_path = ext["lib"]
        self.include_path = self.path
        self.include_path2 = None
        if "include" in ext:
            self.include_path += ("/" + ext["include"])
        if "include2" in ext:
            self.include_path2 = self.path + ("/" + ext["include2"])
        extern_map[name] = self

        self.package = None
        if "package" in ext:
            self.package = ext["package"]

    def write_include(self, f):
        print("write ext: ", self.name, self.include_path)
        content = "include_directories(%s)\n" % self.include_path
        f.write(content)
        if self.include_path2:
            content = "include_directories(%s)\n" % self.include_path2
            f.write(content)
        if self.package:
            content = "find_package(%s REQUIRED PATHS %s)" % (
                self.package, str(pathlib.Path(__file__).parent.absolute())
                + "/" + self.path)
            f.write(content)

    def write_lib(self, f, name):
        #        print("write: ",name)
        if name in found_libs:
            return "${%s_lib}" % name
        found_libs[name] = True
        content = \
            """
find_library(%s_lib NAMES %s PATHS %s PATH_SUFFIXES %s
               NO_DEFAULT_PATH)\n
if(NOT %s_lib)\n
\tset(%s_lib "")\n
endif()\n
""" % (name, name, self.path, self.lib_path, name, name)
        f.write(content)
        return "${%s_lib}" % name


def set_if_not_null(map1, map2, key):
    if key in map1:
        map2[key] = map1[key]


def submodule(name, path, e):
    if "git" in path:
        if not os.path.exists(os.path.join(os.getcwd(), 'deps/%s' % name)):
            call(["git", "submodule", "add", path, "deps/%s" % name, "--force"])
            e.new = True
        return "deps%s" % name
    return None


def call_wrapper(cmds):
    if len(cmds) > 0:
        call(cmds)


class Install(External):
    def __init__(self, name, entries):
        self.new = False

        new_path = None
        try:
            new_path = submodule(name, entries["url"], self)
        except:
            pass
        self.submodule = False
        if new_path:
            self.submodule = True
            entries["url"] = new_path

        self.static = False

        ext = {"path": "./deps/%s" % name,
               "exports": entries["exports"]}

        self.exports = []
        self.exports = entries["exports"]

        set_if_not_null(entries, ext, "lib")
        set_if_not_null(entries, ext, "include")
        set_if_not_null(entries, ext, "include2")

        set_if_not_null(entries, ext, "package")

        External.__init__(self, name, ext)

        self.build = True
        if ("build" in entries):
            self.build = entries["build"]
        self.entries = entries

        # if self.build and (not self.submodule):
        if self.build:
            build_dependencies[name] = self
#        elif self.build and self.submodule:
            #submodules[name] = self

    def install_submodule(self):
        #print("installing/updating: ",self.name)
        # if self.new or self.updated():
        #    os.chdir("./deps/%s" % self.name)
        #    cd_cmd = []
        #    call_wrapper(cd_cmd + self.entries["configure_cmd"].split())
        #    call_wrapper(cd_cmd + self.entries["build_cmd"].split())
        #    call_wrapper(cd_cmd + self.entries["install_cmd"].split())
        #    os.chdir("../../")
        pass

    def updated(self):
        return False

    def configure_ext(self, f):
        entries = self.entries
        # then we init the project
        content = \
            """
set(%s_INSTALL_DIR ${CMAKE_SOURCE_DIR}/deps/%s)
ExternalProject_Add(%s
	       %s %s
               CONFIGURE_COMMAND %s
               BUILD_COMMAND %s
               BUILD_IN_SOURCE 1
"""
        path_name = "URL"
        path = self.entries["url"]

        if self.submodule:
            path_name = "SOURCE_DIR"
            path = "${CMAKE_SOURCE_DIR}/deps/%s" % self.name

        config_cmd = self.entries["configure_cmd"]
        if ("cmake" in config_cmd) or ("mkdir" in config_cmd):
            pass
        else:
            config_cmd += (" " + "--prefix=${%s_INSTALL_DIR}" % self.name)

        content = content % (self.name, self.name, self.name,
                             path_name, path,
                             config_cmd,
                             self.entries["build_cmd"])

        if self.entries["install_cmd"] != "":
            content += "\nINSTALL_COMMAND %s)\n" % self.entries["install_cmd"]
        else:
            content += "\nINSTALL_COMMAND %s)\n" % "cmake -E echo 'Skipping install step.'"
        f.write(content)

        for l in self.exports:
            #            if self.static:
            if not self.submodule:
                f.write("add_library( %s STATIC IMPORTED )\n" % l)
                f.write("""set_target_properties(%s PROPERTIES
                IMPORTED_LOCATION %s/lib/lib%s.a)\n""" % (l, "${%s_INSTALL_DIR}" % self.name, l))
            else:
                pass
        return


class Include_project(External):
    def __init__(self, name, entries):
        ext = {"path": "./deps/%s" % name,
               "exports": []}
        set_if_not_null(entries, ext, "lib")
        set_if_not_null(entries, ext, "include")
        External.__init__(self, name, ext)

        new_path = submodule(name, entries["url"], self)
        if new_path:
            entries["path"] = new_path

    def configure_include(self, f):
        content = \
            """
set(%s_DIR "${CMAKE_SOURCE_DIR}/deps/%s")
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/%s)
"""
        f.write(content % (self.name, self.name, self.name))


gtest_include = Include_project("ggtest", {
                                "include": "googletest/include", "url": "https://github.com/google/googletest.git"})


def load_installs(s):
    try:
        exts = (toml.load(open(s)))["installs"]
        for i in exts:
            print(i)
            a = Install(i, exts[i])
    except:
        print("no install entries")
        pass


def load_externals(s):
    try:
        exts = (toml.load(open(s)))["externals"]
        for i in exts:
            a = External(i, exts[i])
    except:
        pass


def load_downloads(s):
    try:
        downs = (toml.load(open(s)))["downloads"]
        for name in downs:
            i = downs[name]
            if ".git" in i["url"]:
                call(["git", "submodule", "add", i["url"],
                      "deps/%s" % name, "--force"])
            else:
                call("wget", "--directory-prefix=deps", i["url"])
    except:
        pass


def load_includes(s):
    try:
        exts = (toml.load(open(s)))["includes"]
        res = []
        for i in exts:
            res.append(Include_project(i, exts[i]))
        res.append(gtest_include)
        return res
    except:
        print("no include entries")
    return [gtest_include]


def tests_template(s):
    if not os.path.exists("tests"):
        os.mkdir("tests")

    template = \
        """
file(GLOB TSOURCES  "tests/*.cc")
"""
    try:
        f = open("tests/tests.cmake", "w")
        f.write(template)
        srcs = toml.load(open(s))["tests"]["src"]
        write_srcs(f, "coretest", srcs, ["${TSOURCES}"])
        write_extra_deps((toml.load(open(s)))["tests"]["extra"],
                         f, "coretest", ["gtest", "gtest_main"])
        try:
            write_deps((toml.load(open(s)))["tests"]["deps"], "coretest", f)
        except:
            pass
        f.close()
    except:
        pass


def write_extra_deps(extras, f, name, llst=[]):
    global extern_map, export_map
    link_list = []
    link_list += llst
    for e in extras:
        if e in export_map:
            #            print("true find lib here",e)
            link_list.append(export_map[e].write_lib(f, e))
        else:
            #            print("not found lib:",export_map.keys())
            link_list.append(e)
    link_list = list(dict.fromkeys(link_list))

    if len(link_list) == 0:
        return
    f.write("target_link_libraries(%s " % name)
    #print("write for %s: target links:" % name,link_list,extras)
    for i in link_list:
        f.write("%s " % i)
    f.write(")\n")


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest='new')
    parser_new = subparsers.add_parser('new', help='create a null projects')

    parser_new = subparsers.add_parser(
        'config', help='configure the whole projects')
    parser_new.add_argument('-f', '--file', default="config.toml")

    args = parser.parse_args()
    if (args.new == "new"):
        new()
    elif (args.new == 'config'):
        configure(args.file)


if __name__ == "__main__":
    main()
