# -*- mode: python ; coding: utf-8 -*-
from pathlib import Path
import os
import sys
import subprocess
import shutil
import errno


#region Build D3D9
def BuildD3D9(filepath, targets):
    buildscript = filepath / 'build.bat'
    success = []
    fail = []
    print(buildscript)

    for target in targets:
        print(target)
        p = subprocess.Popen(str(buildscript)+" "+target, shell=True)
        stdout, stderr = p.communicate()

        if (p.returncode==0):
            print("Build Succeeded")
            success.append(target)
        else:
            print("Build Failed!")
            fail.append(target)

        print(" ")

    print("D3D9:")
    print("--------------")
    if (len(success)>0):
        print("Succeeded: "+str(success))

    if (len(fail)>0):
        print("Failed: "+str(fail))

    print("")

    return {"success":success, "fail":fail}
#endregion

#region Build OpenGL
def BuildOpenGL(filepath,targets):
    buildscript = filepath / 'build.bat'
    success = []
    fail = []
    print(buildscript)

    for target in targets:
        print(target)
        targetcmd = "release "+target
        p = subprocess.Popen(str(buildscript)+" "+targetcmd, shell=True)
        stdout, stderr = p.communicate()

        if (p.returncode==0):
            print("Build Succeeded")
            success.append(target)
        else:
            print("Build Failed!")
            fail.append(target)

        print(" ")

    print("OpenGL:")
    print("--------------")
    if (len(success)>0):
        print("Succeeded: "+str(success))

    if (len(fail)>0):
        print("Failed: "+str(fail))

    print("")

    return {"success":success, "fail":fail}
#endregion

#region Build D3D10
def BuildD3D10(filepath, targets):
    buildscript = filepath / 'build.ps1'
    success = []
    fail = []
    print(buildscript)

    targetcmds={"DeusEx":"Deus Ex Release", "HarryPotter":"Harry Potter Release","Nerf":"Nerf Arena Blast Release","Klingon":"Klingon Release", "Rune_100":"Rune 1.00 Release", "Rune":"Rune Release", "Unreal_224":"Unreal 224 Release", "Unreal_226_Gold":"Unreal Gold Release", "Unreal_227":"Unreal 227 Release", "UnrealTournament":"Unreal Tournament Release", "UnrealTournament_469":"Unreal Tournament 469 Release", "XComEnforcer":"X-COM Enforcer Release"}
    for target in targets:
        print(target)
        targetcmd = targetcmds[target]
        cmd = 'powershell.exe -file '+str(buildscript)+' -Configuration "'+targetcmd+'"'

        p = subprocess.Popen(cmd, shell=True)
        stdout, stderr = p.communicate()

        if (p.returncode==0):
            print("Build Succeeded")
            success.append(target)
        else:
            print("Build Failed!")
            fail.append(target)

        print(" ")

    print("D3D10:")
    print("--------------")
    if (len(success)>0):
        print("Succeeded: "+str(success))

    if (len(fail)>0):
        print("Failed: "+str(fail))

    print("")

    return {"success":success, "fail":fail}
#endregion

#----------------------------------------------------------------------#

#region Clean Directories
def CleanBuildDirectories(dirs):
    print("")
    print("Cleaning Build Directories")
    print("----------------------------------------------")
    for dir in dirs:
        print("Cleaning Directory: "+str(dir))
        try:
            shutil.rmtree(dir)
            print("    Cleaned")
        except OSError as e:
            if (e.errno==errno.ENOENT):
                print("    Already Clean")
            else:
                print("    Failed to clean directory: %s" % (e.strerror))
#endregion

#region Collect Results
def CollectBuildResults(basedir, targets):
    print("")
    print("Collecting build results...")

    d3d9dir = basedir / "D3D9" / "System"
    opengldir = basedir / "OpenGL" / "System"
    d3d10dir = basedir / "D3D10" / "packages"
    renderdirs = [d3d9dir, opengldir, d3d10dir]

    #Make sure the output directory exists (or create it if not)
    distdir = basedir / ".." / "dist"

    create = True
    if (os.path.exists(distdir)):
        if (os.path.isdir(distdir)):
            create=False
            print(str(distdir)+" exists already")
        else:
            print(str(distdir)+" exists, but it's a file???")
            distdir.unlink()

    if create:
        os.mkdir(distdir)
        print("Created "+str(distdir))

    for target in targets:
        targetdir = distdir / target

        #Collect all the renderers into a directory
        for render in renderdirs:
            renderdir = render / target
            print(str(renderdir) + " ---> " + str(targetdir))
            shutil.copytree(renderdir,targetdir, dirs_exist_ok=True)

        #Zip the contents of the target up
        shutil.make_archive(distdir / target,'zip',targetdir)
        print("Packaged "+target+ " into ZIP")

        try:
            shutil.rmtree(targetdir)
            print("Cleaned "+target+" directory")
        except OSError as e:
            if (e.errno==errno.ENOENT):
                print(target+" did not exist?")
            else:
                print("Failed to clean up directory: %s" % (e.strerror))
        print("")

#endregion

#----------------------------------------------------------------------#

#region Run Build

#All valid targets
alltargets=["UnrealTournament","UnrealTournament_469","XComEnforcer","DeusEx","Rune","Rune_100","Unreal_226_Gold","Unreal_224","Unreal_227","Nerf","HarryPotter","Klingon"]

targets = []
if (len(sys.argv)>1):
    for argnum in range(1,len(sys.argv)):
        
        target = sys.argv[argnum]
        if (target not in alltargets):
            print("Target <"+target+"> not in list of valid targets: "+str(alltargets))
        else:
            targets.append(target)
else:
    targets = alltargets

if len(targets)==0:
    print("Didn't find any valid targets!")
    sys.exit()




#The location of this python file
#Should be in the src directory
base = Path(sys.argv[0]).parents[0]



#Clean output directories
builddirs = []
#Delete these for a full clean rebuild
builddirs.append(base / "D3D9" / "Build")
builddirs.append(base / "OpenGL" / "Build")
builddirs.append(base / "D3D10" / "_work")
#Delete these to guarantee the contents are a fresh build
builddirs.append(base / "D3D9" / "System")
builddirs.append(base / "OpenGL" / "System")
builddirs.append(base / "D3D10" / "packages")
#Clean the packaging directory
builddirs.append(base / ".." / "dist")
#Actually clean them
CleanBuildDirectories(builddirs)


#Actually do the builds
d3d9=None
opengl=None
d3d10=None

successes=0
fails = 0

d3d9 = BuildD3D9(base / 'D3D9', targets)
opengl = BuildOpenGL(base / 'OpenGL', targets)
d3d10 = BuildD3D10(base / 'D3D10', targets)

print("")
print("")
print("")
print("Final Build Results:")
print("------------------------")
if (d3d9!=None):
    print("")
    print("D3D9:")
    success = d3d9.get("success",[])
    fail = d3d9.get("fail",[])
    print("Success: "+str(success))
    print("Fail: "+str(fail))
    fails += len(fail)
    successes += len(success)

if (opengl!=None):
    print("")
    print("OpenGL:")
    success = opengl.get("success",[])
    fail = opengl.get("fail",[])
    print("Success: "+str(success))
    print("Fail: "+str(fail))
    fails += len(fail)
    successes += len(success)

if (d3d10!=None):
    print("")
    print("D3D10:")
    success = d3d10.get("success",[])
    fail = d3d10.get("fail",[])
    print("Success: "+str(success))
    print("Fail: "+str(fail))
    fails += len(fail)
    successes += len(success)

#Copy files to packaging folder
CollectBuildResults(base, targets)

print("BuildRelease: "+str(targets))
print("Success: "+str(successes)+"  Fail: "+str(fails))
print("")

if (fails>0):
    sys.exit(1)

sys.exit(0)

#endregion