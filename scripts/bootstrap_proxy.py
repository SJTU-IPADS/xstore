#!/usr/bin/env python

import toml
import paramiko
import getpass
import argparse
import time
import select
import subprocess

import os

from paramiko import SSHConfig

config = SSHConfig()
with open(os.path.expanduser("/etc/ssh/ssh_config")) as _file:
            config.parse(_file)

class RunPrinter:
    def __init__(self,name,c):
        self.c = c
        self.name = name

    def print_one(self):
        if self.c.recv_ready():
            try:
                res = self.c.recv(4096).decode().splitlines()
                for l in res:
                    print("@%-10s" % self.name,l.strip())
            except:
                pass

        if self.c.recv_stderr_ready():
            res = self.c.recv_stderr(4096).decode().splitlines()
            for l in res:
                print("@%-10s" % self.name,l.strip())

        if self.c.exit_status_ready():
            print("exit ",self.name)
            return False

        return True

def check_keywords(lines,keywords,black_keywords):
    match = []
    for l in lines:
        black = False
        for bk in black_keywords:
            if l.find(bk) >= 0:
                black = True
                break
        if black:
            continue
        flag = True
        for k in keywords:
            flag = flag and (l.find(k) >= 0)
        if flag:
            #print("matched line: ",l)
            match.append(l)
    return len(match)

class Envs:
    def __init__(self,f = ""):
        self.envs = {}
        self.history = []
        try:
            self.load(f)
        except:
            pass

    def set(self,envs):
        self.envs = envs
        self.history += envs.keys()

    def load(self,f):
        self.envs = pickle.load(open(f, "rb"))

    def add(self,name,env):
        self.history.append(name)
        self.envs[name] = env

    def append(self,name,env):
        self.envs[name] = (self.envs[name] + ":" + env)

    def delete(self,name):
        self.history.remove(name)
        del self.envs[name]

    def __str__(self):
        s = ""
        for name in self.history:
            s += ("export %s=%s;" % (name,self.envs[name]))
        return s

    def store(self,f):
        with open(f, 'wb') as handle:
            pickle.dump(self.envs, handle, protocol=pickle.HIGHEST_PROTOCOL)

class ConnectProxy:
    def __init__(self,mac,user="",passp=""):
        if user == "": ## use the server user as default
            user = getpass.getuser()
        self.ssh = paramiko.SSHClient()

        self.ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        self.user = user
        self.mac  = mac
        self.sftp = None
        self.passp = passp

    def connect(self,pwd,passp=None,timeout = 30):
        user_config = config.lookup(self.mac)
        if user_config and 'proxycommand' in user_config:
            print("connect", config)
            #print(user_config)
            #cfg = {'hostname': self.mac, 'username': self.user}
            #cfg['sock'] = paramiko.ProxyCommand(user_config['proxycommand'])
            return self.ssh.connect(hostname = self.mac,username = self.user, password = pwd,
                                    timeout = timeout, allow_agent=False,look_for_keys=False,passphrase=passp,sock=paramiko.ProxyCommand(user_config['proxycommand']),banner_timeout=400)

        else:
            return self.ssh.connect(hostname = self.mac,username = self.user, password = pwd,
                                    timeout = timeout, allow_agent=False,look_for_keys=False,passphrase=passp)

    def connect_with_pkey(self,keyfile_name,timeout = 10):
        print("connect with pkey")
        user_config = ssh_config.lookup(self.mac)
        print(user_config)
        if user_config:
            assert False

        self.ssh.connect(hostname = self.mac,username = self.user,key_filename=keyfile_name,timeout = timeout)

    def execute(self,cmd,pty=False,timeout=None,background=False):
        if not background:
            return self.ssh.exec_command(cmd,get_pty=pty,timeout=timeout)
        else:
            print("exe",cmd,"in background")
            transport = self.ssh.get_transport()
            channel = transport.open_session()
            return channel.exec_command(cmd)

    def execute_w_channel(self,cmd):
        transport = self.ssh.get_transport()
        channel = transport.open_session()
        channel.get_pty()
        channel.exec_command(cmd)
        return channel


    def copy_file(self,f,dst_dir = ""):
        if self.sftp == None:
            self.sftp = paramiko.SFTPClient.from_transport(self.ssh.get_transport())
        self.sftp.put(f, dst_dir + "/" + f)

    def get_file(self,remote_path,f):
        if self.sftp == None:
            self.sftp = paramiko.SFTPClient.from_transport(self.ssh.get_transport())
        self.sftp.get(remote_path + "/" + f,f)

    def close(self):
        if self.sftp != None:
            self.sftp.close()
        self.ssh.close()

    def copy_dir(self, source, target,verbose = False):

        if self.sftp == None:
            self.sftp = paramiko.SFTPClient.from_transport(self.ssh.get_transport())

        if os.path.isfile(source):
            return self.copy_file(source,target)

        try:
            os.listdir(source)
        except:
            print("[%S] failed to put %s" % (self.mac,source))
            return

        self.mkdir(target,ignore_existing = True)

        for item in os.listdir(source):
            if os.path.isfile(os.path.join(source, item)):
                try:
                    self.sftp.put(os.path.join(source, item), '%s/%s' % (target, item))
                    print_v(verbose,"[%s] put %s done" % (self.mac,os.path.join(source, item)))
                except:
                    print("[%s] put %s error" % (self.mac,os.path.join(source, item)))
            else:
                self.mkdir('%s/%s' % (target, item), ignore_existing=True)
                self.copy_dir(os.path.join(source, item), '%s/%s' % (target, item))

    def mkdir(self, path, mode=511, ignore_existing=False):
        try:
            self.sftp.mkdir(path, mode)
        except IOError:
            if ignore_existing:
                pass
            else:
                raise

class Courier2:
    def __init__(self,user=getpass.getuser(),pwd="123",hosts = "hosts.xml",passp="",curdir = ".",keyfile = ""):
        self.user = user
        self.pwd = pwd
        self.keyfile = keyfile
        self.cached_host = "localhost"
        self.passp = passp

        self.curdir = curdir
        self.envs   = Envs()

    def cd(self,dir):
        if os.path.isabs(dir):
            self.curdir = dir
            if self.curdir == "~":
                self.curdir = "."
        else:
            self.curdir += ("/" + dir)

    def get_file(self,host,dst_dir,f,timeout=None):
        p = ConnectProxy(host,self.user)
        try:
            if len(self.keyfile):
                p.connect_with_pkey(self.keyfile,timeout)
            else:
                p.connect(self.pwd,timeout = timeout)
        except Exception as e:
            print("[get_file] connect to %s error: " % host,e)
            p.close()
            return False,None
        try:
            p.get_file(dst_dir,f)
        except Exception as e:
            print("[get_file] get %s @%s error " % (f,host),e)
            p.close()
            return False,None
        if p:
            p.close()

        return True,None

    def copy_file(self,host,f,dst_dir = "~/",timeout = None):
        p = ConnectProxy(host,self.user)
        try:
            if len(self.keyfile):
                p.connect_with_pkey(self.keyfile,timeout)
            else:
                p.connect(self.pwd,timeout = timeout)
        except Exception as e:
            print("[copy_file] connect to %s error: " % host,e)
            p.close()
            return False,None
        try:
            p.copy_file(f,dst_dir)
        except Exception as e:
            print("[copy_file] copy %s error " % host,e)
            p.close()
            return False,None
        if p:
            p.close()

        return True,None

    def execute_w_channel(self,cmd,host,dir,timeout = None):
        p = ConnectProxy(host,self.user)
        try:
            if len(self.keyfile):
                p.connect_with_pkey(self.keyfile,timeout)
            else:
                p.connect(self.pwd,self.passp,timeout = timeout)
        except Exception as e:
            print("[pre execute] connect to %s error: " % host,e)
            p.close()
            return None,e

        try:
            ccmd = ("cd %s" % dir) + ";" + str(self.envs) + cmd
            return p.execute_w_channel(ccmd)
        except:
            return None


    def pre_execute(self,cmd,host,pty=True,dir="",timeout = None,retry_count = 3,background=False):
        if dir == "":
            dir = self.curdir

        p = ConnectProxy(host,self.user)
        try:
            if len(self.keyfile):
                p.connect_with_pkey(self.keyfile,timeout)
            else:
                p.connect(self.pwd,timeout = timeout)
        except Exception as e:
            print("[pre execute] connect to %s error: " % host,e)
            p.close()
            return None,e

        try:
            ccmd = ("cd %s" % dir) + ";" + str(self.envs) + cmd
            if not background:
                _,stdout,_ = p.execute(ccmd,pty,timeout,background = background)
                return p,stdout
            else:
                c = p.execute(ccmd,pty,timeout,background = True)
                return p,c
        except Exception as e:
            print("[pre execute] execute cmd @ %s error: " % host,e)
            p.close()
            if retry_count > 0:
                if timeout:
                    timeout += 2
                return self.pre_execute(cmd,host,pty,dir,timeout,retry_count - 1)

    def execute(self,cmd,host,pty=True,dir="",timeout = None,output = True,background=False):
        ret = [True,""]
        p,stdout = self.pre_execute(cmd,host,pty,dir,timeout,background = background)
        if p and (stdout and output) and (not background):
            try:
                while not stdout.channel.closed:
                    try:
                        for line in iter(lambda: stdout.readline(2048), ""):
                            if pty and (len(line) > 0): ## ignore null lines
                                print((("[%s]: " % host) + line), end="")
                            else:
                                ret[1] += (line + "\n")
                    except UnicodeDecodeError as e:
                        continue
                    except Exception as e:
                        break
            except Exception as e:
                print("[%s] execute with execption:" % host,e)
        if p and (not background):
            p.close()
#            ret[1] = stdout
        else:
            ret[0] = False
            ret[1] = stdout
        return ret[0],ret[1]

#cr.envs.set(base_env)

def main():

    arg_parser = argparse.ArgumentParser(
        description=''' Benchmark script for running the cluster''')
    arg_parser.add_argument(
        '-f', metavar='CONFIG', dest='config', default="run.toml",type=str,
        help='The configuration file to execute commands')
    arg_parser.add_argument('-b','--black', nargs='+', help='hosts to ignore', required=False)
    arg_parser.add_argument('-n','--num', help='num-passes to run', default = 128,type=int)


    args = arg_parser.parse_args()
    num = args.num

    config = toml.load(args.config)

    passes = config.get("pass",[])

    user = config.get("user","wxd")
    pwd  = config.get("pwd","123")
    passp = config.get("passphrase",None)
    global_configs = config.get("global_configs","")


    print(user,pwd,passp)
    print(args.black)

    black_list = {}
    if args.black:
        for e in args.black:
            black_list[e] = True

    cr = Courier2(user,pwd,passp=passp)

    ## first we sync files

    syncs = config.get("sync",[])
    for s in syncs:
        source = s["source"]
        targets = s["targets"]
        for t in targets:
            print("(sync files)","scp -3 %s %s" % (source,t))
            subprocess.call(("scp -3 %s %s" % (source,t)).split())


    printer = []
    runned = 0
    for p in passes:
        if runned >= num:
            break
        runned += 1
        print("(execute cmd @%s" % p["host"],p["cmd"])

        if p["host"] in black_list:
            continue

        if p.get("local","no") == "yes":
            subprocess.run(("cd " + p["path"] + ";" + p["cmd"]).split(" "))
            pass
        else:
            res = cr.execute_w_channel(p["cmd"] + " " + global_configs,
                                       p["host"],
                                       p["path"])
            if p["host"] not in config.get("null",[]):
                        printer.append(RunPrinter(p["host"],res))
            time.sleep(1)

    while len(printer) > 0:
        temp = []
        for p in printer:
            if p.print_one():
                temp.append(p)
        printer = temp

if __name__ == "__main__":
    main()
