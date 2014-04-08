import logging, re, os, sys
import glob, time, base64
import json

from random import randrange
from os import path
from paramiko import SSHException

from utils.sl2 import SL2Res
from utils.ssh import SSH

from managers import sl2gen

log = logging.getLogger("slash2")

class TSuite(object):
  """SLASH2 File System Test Suite."""

  def __init__(self, conf):
    """Initialization of the TSuite.

    Args:
      conf: configuration dict from configparser."""

    #Test suite directories
    #Relative paths, replaced in init
    self.build_dirs = {
      # "base" populated in init

      #TODO: bring this into the configuration file
      # Or, on a per reosurce basis
      "mp"   : "%base%/mp",

      "datadir": "%base%/data",
      "ctl"  : "%base%/ctl",
      "fs"   : "%base%/fs"
    }

    #Determine where the module is being ran
    self.cwd = path.realpath(__file__).split(os.sep)[:-1]
    self.cwd = os.sep.join(self.cwd)

    self.authbuf_key = None

    self.src_dirs = {
      # "src" populated in init
      "slbase"  : "%src%/slash_nara",
      "tsbase"  : "%slbase%/../tsuite",
      "zpool"   : "%src%/zfs/src/cmd/zpool/zpool",
      "zfs_fuse": "%slbase%/utils/zfs-fuse.sh",
      "sliod"   : "%slbase%/sliod/sliod",
      "slmkjrnl": "%slbase%/slmkjrnl/slmkjrnl",
      "slmctl"  : "%slbase%/slmctl/slmctl",
      "slictl"  : "%slbase%/slictl/slictl",
      "slashd"  : "%slbase%/slashd/slashd",
      "slkeymgt": "%slbase%/slkeymgt/slkeymgt",
      "slmkfs"  : "%slbase%/slmkfs/slmkfs"
    }

    self.tsid = None
    self.rootdir = None

    self.sl2objects = {}
    self.conf = conf

    self.user = os.getenv("USER")

    #TODO: Rename rootdir in src_dir fashion
    self.rootdir = self.conf["tsuite"]["rootdir"]
    self.src_dirs["src"] = self.conf["source"]["srcroot"]

    self.local_setup()
    self.create_remote_setups()

  def all_objects(self):
    """Returns all sl2objects in a list."""
    objects = []
    for res, res_list in self.sl2objects.items():
      objects.extend(res_list)
    return objects

  def check_status(self):
    """Generate general status report for all sl2 objects.

    Returns: {
      "type":[ {"host": ..., "reports": ... } ],
      ...
    }"""
    report = {}

    #Operations based on type
    ops = {
        "all": {
          "load": "cat /proc/loadavg | cut -d' ' -f1,2,3",
          "mem_total": "cat /proc/meminfo | head -n1",
          "mem_free": "sed -n 2,2p /proc/meminfo",
          "uptime": "cat /proc/uptime | head -d' ' -f1",
          "disk_stats": "df -hl"
        },
        "mds": {
          "connections":"{slmctl} -sconnections",
          "iostats":"{slmctl} -siostats"
        },
        "ion": {
          "connections": "{slictl} -sconnections",
          "iostats": "{slictl} -siostats"
        }
    }

    for sl2_restype in self.sl2objects.keys():

      report[sl2_restype] = []

      obj_ops = ops["all"]
      if sl2_obj[sl2_restype] in ops:
        obj_ops = dict(ops["all"].items() + ops[sl2_restype].items())

      for sl2_obj in self.sl2objects[sl2_restype]:
        obj_report = {
          "host": sl2_obj["host"],
          "id": sl2_obj["id"],
          "reports": {}
        }
        user, host = self.user, sl2_obj["host"]
        log.debug("Connecting to {0}@{1}".format(user, host))
        ssh = SSH(user, host, '')
        for op, cmd in obj_ops.items():
          obj_report["reports"][op] = ssh.run(cmd, timeout=2)

        report[sl2_restype].append(obj_report)
        log.debug("Status check completed for {0} [{1}]".format(host, sl2_restype))
        ssh.close()
    return report

  def local_setup(self):
    """Create the local build directories and parse the slash2 config."""

    #Necessary to compute relative paths
    self.build_base_dir()
    log.debug("Base directory: {0}".format(self.build_dirs["base"]))

    self.replace_rel_dirs(self.build_dirs)

    if not self.mk_dirs(self.build_dirs.values()):
      log.fatal("Unable to create some necessary directory!")
      sys.exit(1)
    log.info("Successfully created build directories")
    os.system("chmod -R 777 \"{0}\"".format(self.build_dirs["base"]))

    #Compute relative paths for source dirs
    self.replace_rel_dirs(self.src_dirs)

    #Also check for embedded build paths
    self.replace_rel_dirs(self.src_dirs, self.build_dirs)

    if not self.parse_slash2_conf():
      log.critical("Error parsing slash2 configuration file!")
      sys.exit(1)

    log.info("slash2 configuration parsed successfully.")

    #Show the resources parsed
    objs_disp = [
      "{0}:{1}".format(res, len(res_list))\
          for res, res_list in self.sl2objects.items()
    ]
    log.debug("Found: {0}".format(", ".join(objs_disp)))

  def create_remote_setups(self):
    """Create the necessary build directories on all slash2 objects."""

    for sl2_obj in self.all_objects():
      try:
        ssh = SSH(self.user, sl2_obj["host"], '')
        log.debug("Creating build directories on {0}@{1}".format(sl2_obj["name"], sl2_obj["host"]))
        for d in self.build_dirs.values():
          ssh.make_dirs(d)
          ssh.run("sudo chmod -R 777 \"{0}\"".format(d), quiet=True)
        ssh.close()
      except SSHException:
        log.error("Unable to connect to {0} to create build directories!".format(sl2_obj["host"]))

  def run_tests(self):
    """Uploads and runs each test on each client."""

    test_dir = self.conf["tests"]["runtime_testdir"]
    if len(self.sl2objects["client"]) == 0:
      log.error("No test clients?")
      return
    client_hosts = [client["host"] for client in self.sl2objects["client"]]
    ssh_clients = [SSH(self.user, host) for host in client_hosts]
    remote_modules_path = path.join(self.build_dirs["mp"], "modules")
    map(lambda ssh: ssh.make_dirs(remote_modules_path), ssh_clients)

    tests = []
    for test in os.listdir(test_dir):
      if test.endswith(".py"):
        test_path = path.join(test_dir, test)
        tests.append(test)
        map(lambda ssh: ssh.copy_file(test_path, path.join(remote_modules_path, test)), ssh_clients)
    log.debug("Found tests: {0}".format(",".join(tests)))

    test_handler_path = path.join(self.cwd, "handlers", "test_handle.py")
    remote_test_handler_path = path.join(self.build_dirs["mp"], "test_handle.py")
    map(lambda ssh: ssh.copy_file(test_handler_path, remote_test_handler_path), ssh_clients)

    sock_name = "sl2.{0}.tset".format(self.conf["tests"]["runtime_testname"])

    killed_clients = sum(map(lambda ssh: ssh.kill_screens(sock_name), ssh_clients))
    if killed_clients > 0:
      log.debug("Killed {0} stagnant tset sessions. Please take care of them next time.".format(killed_clients))

    log.debug("Running tests on clients.")

    runtime = self.build_dirs
    runtime_arg = base64.b64encode(json.dumps(runtime))

    map(lambda ssh: ssh.run_screen("python {0} {1}".format(remote_test_handler_path, runtime_arg),
        sock_name, quiet=True), ssh_clients)

    log.debug("Waiting for screen sessions to finish.")
    if not all(map(lambda ssh: ssh.wait_for_screen(sock_name)["finished"], ssh_clients)):
      log.error("Some of the screen sessions running the tset encountered errors! Please check out the clients and rectify the issue.")

    result_path = path.join(self.build_dirs["mp"], "results.json")
    results = map(lambda ssh: (ssh.host, ssh.run("cat "+result_path, quiet=True)), ssh_clients)

    map(lambda ssh: ssh.close(), ssh_clients)

    return results

  def parse_slash2_conf(self):
    """Reads and parses slash2 conf for tokens.
    Writes to the base directory; updates slash2 objects in the tsuite."""

    try:
      with open(self.conf["slash2"]["conf"]) as conf:
        new_conf = "#TSuite Slash2 Conf\n"

        res, site_name = None, None
        in_site = False
        site_id, fsuuid = -1, -1
        client = None

        #Regex config parsing for sl2objects
        reg = {
          "clients" : re.compile(
            "^\s*?#\s*clients\s*=\s*(.+?)\s*;\s*$"
          ),
          "type"   : re.compile(
            "^\s*?type\s*?=\s*?(\S+?)\s*?;\s*$"
          ),
          "id"     : re.compile(
            "^\s*id\s*=\s*(\d+)\s*;\s*$"
          ),
          "zpool"  : re.compile(
            r"^\s*?#\s*?zfspool\s*?=\s*?(\w+?)\s+?(.*?)\s*$"
          ),
          "zpool_path"  : re.compile(
            r"^\s*?#\s*?zfspath\s*?=\s*?(.+?)\s*$"
          ),
          "prefmds": re.compile(
            r"\s*?#\s*?prefmds\s*?=\s*?(\w+?@\w+?)\s*$"
          ),
          "fsuuid": re.compile(
            r"^\s*set\s*fsuuid\s*=\s*\"?(0x[a-fA-F\d]+|\d+)\"?\s*;\s*$"
          ),
          "fsroot" : re.compile(
            "^\s*?fsroot\s*?=\s*?(\S+?)\s*?;\s*$"
          ),
          "nids"    : re.compile(
            "^\s*?nids\s*?=\s*?(.*)$"
          ),
          "new_res": re.compile(
            "^\s*resource\s+(\w+)\s*{\s*$"
          ),
          "fin_res": re.compile(
            "^\s*?}\s*$"
          ),
          "site"   : re.compile(
            "^\s*?site\s*?@(\w+).*?"
          ),
          "site_id": re.compile(
            "^\s*site_id\s*=\s*(0x[a-fA-F\d]+|\d+)\s*;\s*$"
          )
        }

        line = conf.readline()

        while line:
          #Replace keywords and append to new conf

          new_conf += repl(self.build_dirs, line)

          #Iterate through the regexes and return a tuple of
          #(name, [\1, \2, \3, ...]) for successful matches

          matches = [
            (k, reg[k].match(line).groups()) for k in reg\
            if reg[k].match(line)
          ]

          #Should not be possible to have more than one
          assert(len(matches) <= 1)

          #log.debug("%s %s %s\n->%s" % (matches, in_site, res, line))
          if matches:
            (name, groups) = matches[0]

            if in_site:

              if name == "site_id":
                site_id = groups[0]

              elif res:
                if name == "type":
                  res["type"] = groups[0]

                elif name == "id":
                  res["id"] = groups[0]

                elif name == "zpool_path":
                  res["zpool_path"] = groups[0].strip()

                elif name == "zpool":
                  res["zpool_name"] = groups[0]
                  res["zpool_cache"] = path.join(
                    self.build_dirs["base"], "{0}.zcf".format(groups[0])
                  )
                  res["zpool_args"] = groups[1]

                elif name == "prefmds":
                  res["prefmds"] = groups[0]


                elif name == "fsroot":
                  res["fsroot"] = groups[0].strip('"')

                elif name == "nids":
                  #Read subsequent lines and get the first host

                  tmp = groups[0]
                  while line and ";" not in line:
                    tmp += line
                    line = conf.readline()
                  tmp = re.sub(";\s*$", "", tmp)
                  res["host"] = re.split("\s*,\s*", tmp, 1)[0].strip(" ")

                elif name == "fin_res":
                  #Check for errors finalizing object
                  res["site_id"] = site_id
                  res["fsuuid"] = fsuuid

                  if not res.finalize(self.sl2objects):
                    sys.exit(1)
                  res = None
              else:
                if name == "new_res":
                  res =  SL2Res(groups[0], site_name)
            else:
              if name == "clients":
                for client in [g.strip() for g in groups[0].split(",")]:
                  client_res = SL2Res(client, None)
                  client_res["type"] = "client"
                  client_res["host"] = client
                  client_res.finalize(self.sl2objects)
              elif name == "site":
                site_name = groups[0]
                in_site = True
              elif name == "fsuuid":
                fsuuid = groups[0]

          line = conf.readline()

        new_conf_path = path.join(self.build_dirs["base"], "slash.conf")

        try:
          with open(new_conf_path, "w") as new_conf_file:
            new_conf_file.write(new_conf)
            log.debug("Successfully wrote build slash2 conf at {0}"\
                .format(new_conf_path))
            for sl2_obj in self.all_objects():
              try:
                ssh = SSH(self.user, sl2_obj["host"], "")
                log.debug("Copying new config to {0}".format(sl2_obj["host"]))
                ssh.copy_file(new_conf_path, new_conf_path)
                ssh.close()
              except SSHException:
                log.error("Unable to copy config file to {0}!".format(sl2_obj["host"]))
        except IOError, e:
          log.fatal("Unable to write new conf to build directory!")
          log.fatal(new_conf_path)
          log.fatal(e)
          return False
    except IOError, e:
      log.fatal("Unable to read conf file at {0}"\
          .format(self.conf["slash2"]["conf"]))
      log.fatal(e)
      return False

    return True

  def mk_dirs(self, dirs):
    """Creates directories and subdirectories.
    Does not consider file exists as an error.

    Args:
      dirs: list of directory paths.
    Returns:
      False if there was an error.
      True if everything executed."""

    for d in dirs:
      try:
        os.makedirs(d)
      except OSError, e:

        #Error 17 is that the file exists
        #Should be okay as the dir dictionary
        #does not have a guarnteed ordering

        if e.errno != 17:
          log.fatal("Unable to create: {0}".format(d))
          log.fatal(e)
          return False
    return True

  def replace_rel_dirs(self, dirs, lookup = None):
    """Looks up embedded keywords in a dict.

    Args:
      dirs: dict with strings to parse.
      lookup: dict in which keywords are located. If None, looks up in dirs."""

    if lookup is None:
      lookup = dirs

    for k in dirs:
      #Loop and take care of embedded lookups
      replaced = repl(lookup, dirs[k])
      while replaced != dirs[k]:
        dirs[k] = replaced
        replaced = repl(lookup, dirs[k])

  def build_base_dir(self):
    """Generates a valid, non-existing directory for the TSuite."""

    #Assemble a random test directory base
    tsid = randrange(1, 1 << 24)
    random_build = "sltest.{0}".format(tsid)
    base = path.join(self.rootdir, random_build)

    #Call until the directory doesn't exist
    if path.exists(base):
      self.build_base_dir()
    else:
      self.tsid = tsid
      self.build_dirs["base"] = base

def repl(lookup, string):
  """Replaces keywords within a string.

  Args:
    lookup: dict in which to look up keywords.
    string: string with embedded keywords. Ex. %key%.
  Return:
    String containing the replacements."""

  return re.sub("%([\w_]+)%",
    #If it is not in the lookup, leave it alone
    lambda m: lookup[m.group(1)]
      if
        m.group(1) in lookup
      else
        "%{0}%".format(m.group(1)),
    string)

def repl_file(lookup, text):
  """Reads a file and returns an array with keywords replaced.

  Args:
    lookup: dict in which to look up keywords.
    text: file containing strings with embedded keywords. Ex. %key%.
  Return:
    String containing all replacements. Returns none if not able to parse."""

  try:
    with open(text, "r") as f:
     return "".join([repl(lookup, line) for line in f.readlines()])
  except IOError, e:
    return None

def check_subset(necessary, check):
  """Determines missing elements from necessary list.

  Args:
    necessary: list of mandatory objects.
    check: list to be checked.
  Returns:
    List of elements missing from necessary."""

  if not all(n in check for n in necessary):
    #Remove sections that are correctly there
    present = [s for s in check if s in necessary]
    map(necessary.remove, present)
    return necessary
  return []
