import os
import re
import sys
import glob
import subprocess
import argparse
import shutil
import json

SDK_VERSION = '5.0.0'
REPO_URL="https://github.com/massif-maps/MassifMaps"

def makedirs(dir):
  try:
    if not os.path.exists(dir):
      os.makedirs(dir)
  except:
    e = sys.exc_info()[1]
    print("Exception %s while creating directory %s" % (str(e), dir))
    return False
  return True

def copyfile(source, target):
  try:
    shutil.copyfile(source, target)
  except:
    e = sys.exc_info()[1]
    print("Exception %s while copying %s -> %s" % (str(e), source, target))
    return False
  return True

def checksumSHA256(filename):
  import hashlib
  sha256_hash = hashlib.sha256()
  with open(filename, 'rb') as f:
    for block in iter(lambda: f.read(4096), b''):
      sha256_hash.update(block)
  return sha256_hash.hexdigest() 

def makesymlink(dir, source, target):
  currentDir = os.getcwd()
  os.chdir(dir)
  try:
    if os.path.exists(target):
      os.remove(target)
    os.symlink(source, target)
  except:
    e = sys.exc_info()[1]
    print("Exception %s while creating symlink %s -> %s" % (str(e), source, target))
    os.chdir(currentDir)
    return False
  os.chdir(currentDir)
  return True

def execute(cmd, dir, *cmdArgs):
  currentDir = os.getcwd()
  os.chdir(dir)
  cmdLine = [cmd] + list(cmdArgs)
  try:
    code = subprocess.call(cmdLine)
  except:
    e = sys.exc_info()[1]
    print("Exception %s while executing %s (path %s):\n%s" % (str(e), cmd, dir, " ".join(cmdLine)))
    os.chdir(currentDir)
    return False
  os.chdir(currentDir)
  if code != 0:
    print("Error while executing %s (path %s):\n%s" % (cmd, dir, " ".join(cmdLine)))
    return False
  return True

def checkExecutable(cmd, *cmdArgs):
  cmdLine = [cmd] + list(cmdArgs)
  try:
    output = subprocess.check_output(cmdLine, stderr=subprocess.STDOUT)
  except:
    return False
  return True

def cmake(args, dir, cmdArgs):
  return execute(args.cmake, dir, *cmdArgs)

def detectNinja(args):
  # Ninja over Unix Makefiles: better scheduling on the single ~1700 translation unit target,
  # and it writes .ninja_log, the only per-file build timing this repo has.
  ninja = getattr(args, 'ninja', 'auto')
  if ninja == 'none':
    return None
  if ninja != 'auto':
    return ninja if checkExecutable(ninja, '--version') else None
  found = shutil.which('ninja')
  if found:
    return found
  # A machine that never installed ninja itself still has one per Android SDK cmake package.
  sdkPath = getattr(args, 'androidsdkpath', None)
  candidates = glob.glob('%s/cmake/*/bin/ninja' % sdkPath) if sdkPath else []
  if not candidates:
    return None
  versionKey = lambda path: [int(part) if part.isdigit() else 0 for part in path.split('/')[-3].split('.')]
  return sorted(candidates, key=versionKey)[-1]

def detectCCache(args):
  ccache = getattr(args, 'ccache', 'auto')
  if ccache == 'none':
    return None
  if ccache != 'auto':
    return ccache if checkExecutable(ccache, '--version') else None
  return shutil.which('ccache')

def getCCacheMaxSizeGB(ccachePath):
  try:
    output = subprocess.check_output([ccachePath, '--get-config', 'max_size'], stderr=subprocess.STDOUT).decode('utf-8').strip()
  except:
    return None
  match = re.match(r'([0-9.]+)\s*([KMGT]?)i?B?$', output, re.IGNORECASE)
  if not match:
    return None
  scale = { '': 1e-9, 'K': 1e-6, 'M': 1e-3, 'G': 1.0, 'T': 1000.0 }
  return float(match.group(1)) * scale.get(match.group(2).upper(), 1.0)

def resolveBuildTools(args):
  # Resolved once, so the per-ABI builds all report and use the same tools.
  args.ninjapath = detectNinja(args)
  args.ccachepath = detectCCache(args)
  print('Using build tool: %s' % (args.ninjapath or '%s (no ninja found)' % args.make))
  print('Using compiler launcher: %s' % (args.ccachepath or 'none'))
  if args.ccachepath:
    # One ABI writes about 1GB of objects, so on the 5GB default the four ABIs evict each
    # other and every build stays a miss.
    maxSizeGB = getCCacheMaxSizeGB(args.ccachepath)
    if maxSizeGB is not None and maxSizeGB < 20:
      print('Warning: ccache max_size is %.0fGB, too small to keep a full build. Raise it with: %s --max-size 30G' % (maxSizeGB, args.ccachepath))

def getGeneratorOptions(args):
  if args.ninjapath:
    return ['-G', 'Ninja', '-DCMAKE_MAKE_PROGRAM=%s' % args.ninjapath]
  return ['-G', 'Unix Makefiles', "-DCMAKE_MAKE_PROGRAM='%s'" % args.make]

def resetBuildDirOnGeneratorChange(args, buildDir):
  # CMake refuses to reconfigure an existing build tree with a different generator, so an
  # existing Unix Makefiles tree has to go before the first Ninja build.
  cachePath = '%s/CMakeCache.txt' % buildDir
  if not os.path.exists(cachePath):
    return
  generator = 'Ninja' if args.ninjapath else 'Unix Makefiles'
  with open(cachePath, 'r') as f:
    match = re.search(r'^CMAKE_GENERATOR:INTERNAL=(.*)$', f.read(), re.MULTILINE)
  if match and match.group(1).strip() == generator:
    return
  print('Generator changed to %s, clearing build directory %s' % (generator, buildDir))
  shutil.rmtree(buildDir, True)
  makedirs(buildDir)

def getCCacheOptions(args):
  # Every ABI recompiles the same headers, and the four Boost.Spirit grammars (mapnikvt
  # ParserUtils/GeneratorUtils, CartoCSSParser, QueryExpressionParser) are ~15% of a full
  # build on their own while practically never changing.
  if not args.ccachepath:
    return []
  return ['-DCMAKE_C_COMPILER_LAUNCHER=%s' % args.ccachepath, '-DCMAKE_CXX_COMPILER_LAUNCHER=%s' % args.ccachepath]

def getBaseDir():
  baseDir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
  if os.name == 'nt':
    baseDir = baseDir.replace("\\", "/")
  return baseDir

def getVariant(profile):
  if profile == "standard":
    return "core"
  return profile.replace("standard+", "").replace("+", "_")

def getAndroidAarDistName(version, profile):
  return 'carto-mobile-sdk-android-%s-%s.aar' % (version, getVariant(profile))

def getIOSZipDistName(version, profile):
  return 'carto-mobile-sdk-ios-%s-%s.zip' % (version, getVariant(profile))

def getRoutingAndroidAarDistName(version):
  return 'carto-routing-android-%s.aar' % version

def getRoutingIOSZipDistName(version):
  return 'carto-routing-ios-%s.zip' % version

def getBuildDir(target, arch=None):
  if arch is None:
    buildDir = '%s/build/%s' % (getBaseDir(), target)
  else:
    buildDir = '%s/build/%s-%s' % (getBaseDir(), target, arch)
  makedirs(buildDir)
  return buildDir

def getDistDir(target):
  distDir = '%s/dist/%s' % (getBaseDir(), target)
  makedirs(distDir)
  return distDir

def getDefaultProfileId():
  return 'standard'

def getProfile(profileIds):
  includes = set()
  excludes = set()
  defines = set()
  cmakeOptions = set()
  allProfileIds = profileIds.split('+')
  if 'lite' not in allProfileIds:
    allProfileIds.append(getDefaultProfileId())
  for profileId in allProfileIds:
    profile = getProfiles()[profileId]
    includes.update(profile.get('cmake-includes', '').split(';'))
    excludes.update(profile.get('cmake-excludes', '').split(';'))
    defines.update(profile.get('defines', '').split(';'))
    cmakeOptions.update(profile.get('cmake-options', '').split(';'))
  
  for include in includes:
    if (include != ""):
      cmakeOptions.add('INCLUDE_%s:BOOL=ON' % include)

  for exclude in excludes:
    if (exclude != "" and not exclude in includes):
      cmakeOptions.add('INCLUDE_%s:BOOL=OFF' % exclude)

  return { 'defines': ';'.join(list(defines)), 'cmake-options': ';'.join(list(cmakeOptions)) }

def getProfiles():
  profilesFilename = '%s/sdk_profiles.json' % os.path.dirname(os.path.realpath(__file__))
  with open(profilesFilename, 'r') as f:
    profiles = json.loads(f.read())
    return { str(key): val for key, val in profiles.items() if key != 'free' }

def validProfile(profileIds):
  validProfileIds = getProfiles().keys()
  for profileId in profileIds.split('+'):
    if not profileId in validProfileIds:
      raise argparse.ArgumentTypeError('Profile must be one of or a combination of %s' % ', '.join(validProfileIds))
  return profileIds

def getVersion(buildversion, buildnumber):
  try:
    lastCommit = "None"
    gitLog = subprocess.Popen(["git", "describe"], stdout=subprocess.PIPE).communicate()[0]
    lastCommit = gitLog.strip()

    branch = "None"
    gitBranches = subprocess.Popen(["git", "branch"], stdout=subprocess.PIPE).communicate()[0]
    for line in gitBranches.split("\n"):
      match = re.match(r"\\*\s+(.*)", line)
      if match:
        branch = match.group(1)

    return "%s|%s|%s|%s" % (buildversion, buildnumber, branch, lastCommit)
  except:
    return "%s" % (buildversion)

def readLines(fileName):
  with open(fileName, 'r') as f:
    lines_in = f.readlines()
  return lines_in

def readUncommentedLines(fileName):
  with open(fileName, 'r') as f:
    lines_in = f.readlines()
  comment = False
  lines_out = []
  for line in lines_in:
    line_prefix = ""
    while True:
      if comment:
        match = re.search(r'[*][/](.*)$', line)
        if not match:
          line = ""
          break
        line = match.group(1)
        comment = False
      else:
        match = re.search(r'(([^/]|[/][^*])*)[/][*](.*)$', line)
        if not match:
          break
        line_prefix = line_prefix + match.group(1)
        line = match.group(3)
        comment = True
    line = line_prefix + line
    if line != "":
      match = re.search(r'^(.*)[/][/].*$', line)
      if match:
        line = match.group(1)
      lines_out.append(line)
  return lines_out

def readLicense():
  with open('%s/LICENSE' % getBaseDir(), 'r') as f:
    licenseText = f.read()
  return licenseText

def applyTemplate(template, valueMap):
  result = template
  for key, value in valueMap.items():
    result = result.replace("$%s$" % key, value)
  return result.split("\n")
