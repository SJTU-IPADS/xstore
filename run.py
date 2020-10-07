#!/usr/bin/env python

import toml
import paramiko
import getpass
import argparse
import time

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
    def __init__(self,mac,user=""):
        if user == "": ## use the server user as default
            user = getpass.getuser()
        self.ssh = paramiko.SSHClient()

        self.ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        self.user = user
        self.mac  = mac
        self.sftp = None

    def connect(self,pwd,timeout = 10):
        self.ssh.connect(hostname = self.mac,username = self.user, password = pwd,
                         timeout = timeout, allow_agent=False,look_for_keys=False)

    def connect_with_pkey(self,keyfile_name,timeout = 10):
        self.ssh.connect(hostname = self.mac,username = self.user,key_filename=keyfile_name,timeout = timeout)

    def execute(self,cmd,pty=False,timeout=None,background=False):
        if not background:
            return self.ssh.exec_command(cmd,get_pty=pty,timeout=timeout)
        else:
            print("exe",cmd,"in background")
            transport = self.ssh.get_transport()
            channel = transport.open_session()
            return channel.exec_command(cmd)

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
    def __init__(self,user=getpass.getuser(),pwd="123",hosts = "hosts.xml",curdir = ".",keyfile = ""):
        self.user = user
        self.pwd = pwd
        self.keyfile = keyfile
        self.cached_host = "localhost"

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

server_cmd      = "./fserver -db_type %s --threads %d --id %d"
kill_server_cmd = "pkill fserver"
ccmd = "./%s --threads %d -concurrency %d -workloads %s -server_host %s -id %d"
kcmd = "pkill %s"
OUTPUT_CMD_LOG = " 1>~/log 2>&1 &" ## this will flush the log to a file

root_dir = "/cock/fstore"
base_env  = { "PATH":"/cock/fstore1/:$PATH"
}

cr = Courier2("wxd","123")
cr.envs.set(base_env)

class Cluster:
    def __init__(self,cs):
        f = open(cs,"r")
        handler = toml.load(f)

        self.servers = handler["server"]
        self.db_type = handler["server_config"].get("db_type","ycsb")

        f.close()

    ## start all servers
    def bootstrap(self,num_threads = None,args = ""):
        for i,s in enumerate(self.servers):
            threads = s.get('threads',1)
            if num_threads != None:
                threads = num_threads
            cmd = server_cmd % (self.db_type,threads,s.get("id",i)) \
                + " " + args + " " + " 1>~/log 2>&1 &"
            print(cmd)
            ret,err = cr.execute(cmd,s["host"],False,root_dir,None,output = False,background = True)

    def kill(self):
        for s in self.servers:
            ret,err = cr.execute(kill_server_cmd,
                                 s["host"],True,root_dir,10,output = True)
            time.sleep(1)

cli_lists = []
class Clients:
    def __init__(self,e,cs,nclients):
        f = open(cs,"r")
        handler = toml.load(f)

        self.clients = handler["client"]
        self.clients = self.clients[0:min(len(self.clients),nclients)]

        self.config  = cs
        self.master  = handler["master"]
        self.general = handler["general_config"]

        self.exe = e
        print(self.clients)

        f.close()

    def bootstrap(self,threads,concurrency,workloads,server_hosts,args):
        for i,s in enumerate(self.clients):
            #threads = s.get('thread',self.general.get("thread",threads))
            threads = s.get('thread',threads)
            w = s.get("workloads",workloads)
            cmd = ccmd % (self.exe,threads,concurrency,w,server_hosts,s.get('id',i + 12))
            cmd += (" " + args + " " + OUTPUT_CMD_LOG)
            print(cmd)
            #ret,err = cr.execute(cmd,s["host"],False,"/cock/fstore",timeout = None,output = False)
            cr.execute("rm ~/log",s["host"],True,"")
            ret,err = cr.execute(cmd,s["host"],False,root_dir,timeout = None,output = False,background = True)
            time.sleep(2)
            if not (ret):
                print(err)
#                assert(False)
            cli_lists.append(err)
        pass

    def bootstrap_master(self,epoch):
        cmd = "./master -client_config %s -epoch %d -nclients %d" \
                             % (self.config,epoch,len(self.clients))
        print(cmd)
        ret,err = cr.execute(cmd,self.master["host"],True,root_dir,timeout = None,output = True)
        #cli_lists.append(err)

    def kill(self):
        print("kill all clients")
        for s in self.clients:
            if not self.check_live(s["host"]):
                continue
            print(s,"still has live process, kill it")
            ret,err = cr.execute(kcmd % self.exe,
                                 s["host"],True,root_dir,timeout = None,output = True)
            time.sleep(1)
        cr.execute("pkill master",self.master["host"],True,root_dir,timeout = None,output = True)
        time.sleep(1)

    def check_live(self,host):
        ret,output = cr.execute("ps aux | grep %s" % self.exe,host,False,root_dir,10,output = True,background = False)
        if not ret:
            assert(False)
        if check_keywords(output.split("\n"),[self.exe,cr.user],["grep","<defunct>","[","python3"]) > 0:
            return True
        return False

def main():
    arg_parser = argparse.ArgumentParser(
        description=''' The main test script for fstore.''')

    arg_parser.add_argument(
        '-f', metavar='C', dest='config', default="cs.toml",
        help='Default configuration file')

    arg_parser.add_argument(
        '-t', metavar='T', dest='thread', default=1,type = int,
        help='Number of threads used per client')

    arg_parser.add_argument(
        '-st', metavar='ST', dest='sthread', default=-1,type = int,
        help='Number of threads used per server')

    arg_parser.add_argument(
        '-cc', metavar='CC', dest='concurrency', default=1,type = int,
        help='Number of concurrency per thread.')

    arg_parser.add_argument(
        '-e', metavar='E', dest='epoch', default=10,type = int,
        help='Number of test time.')

    arg_parser.add_argument(
        '-n', metavar='N', dest='nclients', default=1000,type = int,
        help='Number of clients to run.')

    arg_parser.add_argument(
        '-s', metavar='S', dest='server', default="val02",
        help='Server host')

    arg_parser.add_argument(
        '-w', metavar='W', dest='workload', default="null",
        help='Number of client workloads.')

    arg_parser.add_argument(
        '-ca', metavar='args', dest='args', default="",type=str,
        help='user specificed args to clients')

    arg_parser.add_argument(
        '-sa', metavar='sargs', dest='sargs', default="",
        help='user specificed args to servers')

    arg_parser.add_argument(
        '-c', metavar='COMMAND', dest='command', default="s",
        help=' (s) to start the servers, and (c) to kill servers ')

    arg_parser.add_argument(
        '-a',metavar='A',dest='a',default="micro",
        help="client program to run")

    args = arg_parser.parse_args()

    if args.sthread < 0:
        args.sthread = args.thread # if #server_thread is not specificed, use client's
    c = Clients(args.a,args.config,args.nclients)
    cluster = Cluster(args.config)

    if args.command == 's':
        try:
            c.bootstrap(args.thread,args.concurrency,args.workload,args.server,args.args)
            time.sleep(1)
            cluster.bootstrap(args.sthread,args = args.sargs)
            time.sleep(1)
            c.bootstrap_master(args.epoch)
        except Exception as e:
            print("Error happens,",e)
        finally:
            print("finally kill remaining processes")
            cluster.kill()
            c.kill()
            print("kill all done")
    elif args.command == "k":
        cluster.kill()
        c.kill()

if __name__ == "__main__":
    main()
