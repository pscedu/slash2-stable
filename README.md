$Id$

# Portable File-system Libraries

    See COPYRIGHT for license information.

## Download instructions

Grab the PFL component:

	$ git clone https://github.com/pscedu/pfl
	$ cd pfl

Next, download any additional components/repositories as necessary.
PFL itself is usually just a dependency of other projects.

## Configuration instructions

Similar to the popular GNU autoconf `./configure' system probe
process, the PFL build infrastructure probes the host machine for API
compatibility.
The mechanism in PFL that performs this is called Pickle.

Pickle stores all such information in a file
`mk/gen-localdefs-$HOSTNAME-pickle.mk' from the root of the top
directory.
Pickle uses heuristics to determine when these probe tests may be out of
date and reruns them when necessary.

If for some reason the probes need to be manually rerun, deleting this
file will cause Pickle to regenerate it automatically upon the next
'make' invocation.

## Build infrastructure design

	PFL File	Description
	-------------------------------------------------
	mk/defs.mk	default configuration settings
	mk/main.mk	core building rules
	mk/local.mk	(optional) custom/override settings

Any custom system settings should go in `mk/local.mk'.
Modification of the other files listed above is generally not necessary.

## Installation instructions

After configuration, building of PFL follows a standard make procedure:

	$ make
	$ sudo make install

## Updating instructions

To update PFL and any components also present in the source tree:

	$ make up
